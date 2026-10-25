#include "MainWindow.h"

#include "panels/HeritageCard.h"
#include "panels/RelationPanel.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QSet>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QThreadPool>
#include <QtConcurrent>
#include <qgslayertree.h>
#include <qgsproject.h>
#include <qgsgeometry.h>
#include <qgsmaprendererparalleljob.h>
#include <qgsmapsettings.h>
#include <qgsvectorlayer.h>
#include <ogr_geometry.h>
#include <algorithm>
#include <cmath>

#include "CrsTransform.h"
#include "DemProvider.h"
#include "EvaluationOverlay.h"
#include "HeritageNaming.h"
#include "KaAppBar.h"
#include "KaBeginnerRibbon.h"
#include "KaGlTerrainViewport.h"
#include "KaFeatureCard.h"
#include "KaIcons.h"
#include "KaFeatureFormDialog.h"
#include "KaInspectorPanel.h"
#include "KaNewSurveyDialog.h"
#include "KaUndoGroup.h"
#include "KaRegionLocator.h"
#include "KaStatusBar.h"
#include "KaTheme.h"
#include "KhsProvider.h"
#include "LocalVectorProvider.h"
#include "OsmProvider.h"
#include "ProfilePlot.h"
#include "RibbonSpec.h"
#include "SatelliteProvider.h"
#include "SheetComposer.h"
#include "StrataLayerDock.h"
#include "StrataReceive.h"
#include "StrataProjectLayers.h"
#include "core/EditBufferSummary.h"
#include "core/FeatureRecord.h"
#include "core/KoreaRegionCatalog.h"
#include "core/LayerOps.h"
#include "core/LocationSearch.h"
#include "core/SurveyFileFingerprint.h"
#include "core/SurveyProjectFactory.h"
#include "core/SurveySession.h"
#include "core/SurveyStorage.h"
#include "ProjectSurvey.h"
#include "VworldKey.h"

namespace {
// 3D 의 캐시(DEM·지물·타일). 앱 이름이 Strata 와 같아도(ka-hgis) 예전 Strata 3D 캐시 자리를 그대로 쓴다.
QString s3dCacheDir() {
  QString base = qEnvironmentVariable("LOCALAPPDATA");
  if (base.isEmpty()) base = QDir::homePath() + QStringLiteral("/AppData/Local");
  return QDir(base).filePath(QStringLiteral("strata-3d/strata-3d/cache"));
}

QString crsLabelFor(const QString& crs) {
  if (crs == QLatin1String("EPSG:5186")) return QStringLiteral("EPSG:5186 (KGD2002 중부원점)");
  if (crs == QLatin1String("EPSG:5187")) return QStringLiteral("EPSG:5187 (KGD2002 동부원점)");
  return crs;
}

// 아직 잇지 않은 단추와 그 단계(설계도 v3 9장). 누를 수 없게 두고 이름표에 단계를 적는다.
const QHash<QString, QString>& pendingButtons() {
  static const QHash<QString, QString> k = {
      {QStringLiteral("topo_download"), QStringLiteral("0b단계: Strata 받기 흐름 연결")},
      {QStringLiteral("survey_contour"), QStringLiteral("2단계: 수치지형 등고선 3D 선")},
      {QStringLiteral("paleo"), QStringLiteral("2단계: 고지형도 입히기")},
      {QStringLiteral("old_map"), QStringLiteral("2단계: 옛 지도 입히기")},
      {QStringLiteral("subsurface"), QStringLiteral("2단계: 땅속 보기")},
      {QStringLiteral("depth"), QStringLiteral("2단계: 유구 윗면·바닥 표고")},
      {QStringLiteral("inventory"), QStringLiteral("3단계: 주변유적 현황표 17열")},
      {QStringLiteral("export_convert"), QStringLiteral("검수·제출(5179 묶음)은 Strata 에서 합니다")},
  };
  return k;
}

// 이름표를 다는 유산: 지정·등록 유산만(구역 SHP, 조사·분포 위치도 수백 건, 건물은 뺀다).
bool labelledHeritage(const strata3d::GeoFeature& f) {
  if (f.kind == strata3d::FeatureKind::Building) return false;
  if (strata3d::zoneLayerNames().contains(f.category)) return false;
  for (const char16_t* g : {u"발굴유적위치도", u"지표유적위치도", u"문화유적분포지도"})
    if (f.category == QString::fromUtf16(g)) return false;
  if (f.source == QLatin1String("OSM")) return false;  // OSM 은 이름·위치가 확인된 자료가 아니다
  return !f.name.trimmed().isEmpty();
}

// DEM 읽기·재배열·분트리·BVH 만들기와 건물·유적 받기는 전부 UI 스레드 밖에서 한다.
MainWindow::LoadResult buildScene(MainWindow::Options opt) {
  QElapsedTimer timer;
  timer.start();
  MainWindow::LoadResult out;
  double x = opt.lon, y = opt.lat;
  if (!strata3d::transformPoint(QStringLiteral("EPSG:4326"), opt.workCrs, &x, &y, &out.error)) return out;
  strata3d::DemRequest req;
  req.centerX = x;
  req.centerY = y;
  req.halfExtentM = opt.halfExtentM;
  req.pixelSizeM = opt.pixelSizeM;
  req.workCrs = opt.workCrs;
  req.cacheDir = s3dCacheDir() + QStringLiteral("/dem");
  if (!opt.demPath.isEmpty()) req.sources = {opt.demPath};
  strata3d::DemGrid grid;
  if (!strata3d::loadDemWindow(req, &grid, &out.error)) return out;
  out.tree = strata3d::TerrainLodTree::build(grid);
  if (!out.tree) {
    out.error = QStringLiteral("지형 트리를 만들지 못했습니다.");
    return out;
  }
  auto all = std::make_shared<std::vector<strata3d::GeoFeature>>();
  if (opt.loadFeatures) {
    strata3d::FeatureRequest fr;
    fr.centerX = x;
    fr.centerY = y;
    fr.halfExtentM = std::min(opt.featureRadiusM, opt.halfExtentM);
    fr.workCrs = opt.workCrs;
    fr.cacheDir = s3dCacheDir() + QStringLiteral("/vector");
    QString err;
    // 한 곳이 실패해도 나머지로 계속한다. 실패 사유는 화면에 그대로 알린다.
    if (!strata3d::loadOsmFeatures(fr, all.get(), &err)) out.warnings << err;
    if (!strata3d::loadKhsHeritageNearby(fr, all.get(), &err)) out.warnings << err;
    else if (!err.isEmpty()) out.warnings << QStringLiteral("국가유산청 일부 시도 실패: ") + err;
    QStringList skipped;
    if (opt.heritageFromProject) {
      // 레이어 칸에 연 조사 프로젝트(받은 뒤 저장 전인 레이어 포함)
      if (opt.heritageSources.empty()) out.warnings << QStringLiteral("이 조사에 받아 둔 주변유적 자료가 없습니다(「자료 받기 › 유산」).");
      strata3d::loadLocalVectorSources(fr, opt.heritageSources, all.get(), &skipped);
    } else if (!opt.surveyGpkg.isEmpty()) {
      // Strata 는 조사구역을 그린 뒤 그 둘레(5 km)로 주변유적을 받아 조사 프로젝트에 적는다. 그 목록만 읽는다.
      strata3d::StrataProject proj;
      QStringList missing;
      if (!strata3d::readStrataProject(opt.surveyGpkg, &proj, &err)) {
        out.warnings << err;
      } else {
        const auto sources = strata3d::heritageSourcesOf(proj, &missing);
        if (sources.empty()) out.warnings << QStringLiteral("이 조사에 받아 둔 주변유적 자료가 없습니다(Strata 에서 조사구역을 그리고 주변유적을 받은 뒤 저장).");
        strata3d::loadLocalVectorSources(fr, sources, all.get(), &skipped);
        for (const QString& m : missing) out.warnings << QStringLiteral("주변유적 파일 없음: ") + m;
      }
    } else {
      strata3d::loadLocalVectors(fr, strata3d::strataHeritageShpPaths(), all.get(), &skipped);
    }
    out.mesh = strata3d::buildSceneMesh(grid, *all);
  }
  out.features = all;
  out.evaluator = std::make_shared<strata3d::Archaeology3dEvaluator>(grid, *all);
  out.seconds = timer.elapsed() / 1000.0;
  return out;
}

MainWindow::SatelliteLoad buildSatellite(MainWindow::Options opt, double centerX, double centerY, double halfExtentM) {
  QElapsedTimer timer;
  timer.start();
  MainWindow::SatelliteLoad out;
  strata3d::SatelliteRequest sr;
  sr.centerX = centerX;
  sr.centerY = centerY;
  sr.halfExtentM = halfExtentM;
  sr.workCrs = opt.workCrs;
  sr.texels = opt.satelliteTexels;
  sr.cacheDir = s3dCacheDir() + QStringLiteral("/tiles");
  strata3d::SatelliteResult r;
  if (strata3d::loadSatelliteImage(sr, &r, &out.error)) {
    out.image = std::move(r.image);
    out.source = r.sourceName;
    constexpr double kDetailHalfM = 3000.0;  // 4096 px / 6 km = 1.5 m/px
    if (halfExtentM > kDetailHalfM * 1.5) {
      strata3d::SatelliteRequest dr = sr;
      dr.halfExtentM = kDetailHalfM;
      dr.texels = 4096;
      strata3d::SatelliteResult d;
      QString derr;
      if (strata3d::loadSatelliteImage(dr, &d, &derr)) {
        out.detail = std::move(d.image);
        out.detailHalfM = kDetailHalfM;
      }
    }
  }
  out.seconds = timer.elapsed() / 1000.0;
  return out;
}

QString evaluationHtml(const strata3d::Evaluation& ev) {
  const KaTheme::Tokens& t = KaTheme::tokens();
  QString h = QStringLiteral("<body style='color:%1'>").arg(t.ink.name());
  h += QStringLiteral("<p><b>버퍼</b> 기준점 X %1, Y %2</p>").arg(ev.site.x, 0, 'f', 1).arg(ev.site.y, 0, 'f', 1);
  const auto& tm = ev.terrain;
  h += QStringLiteral("<p>지형(현재 DEM) 고도 %1 m · 경사 %2° · 향 %3</p>").arg(tm.elevationM, 0, 'f', 1).arg(tm.slopeDeg, 0, 'f', 1).arg(tm.aspectName);
  for (const strata3d::RadiusResult& rr : ev.radii) {
    h += QStringLiteral("<p><b>반경 %1 m</b> (넓이 %2 ㎡) — 유산·유적 %3건</p>")
             .arg(rr.radiusM, 0, 'f', 0).arg(rr.bufferAreaM2, 0, 'f', 0).arg(int(rr.heritage.size()));
    h += QStringLiteral("<table cellspacing=0 cellpadding=3 border=0>");
    int n = 0;
    for (const strata3d::NearbyItem& it : rr.heritage) {
      if (++n > 12) break;
      h += QStringLiteral("<tr><td>%1</td><td>%2</td><td align=right>%3 m</td><td>%4</td><td align=right>%5 m</td></tr>")
               .arg(it.name.isEmpty() ? QStringLiteral("(이름 없음)") : strata3d::siteNameOnly(it.name).toHtmlEscaped(), it.category.toHtmlEscaped())
               .arg(it.distanceM, 0, 'f', 0).arg(it.bearingName).arg(it.deltaElevM, 0, 'f', 1);
    }
    h += QStringLiteral("</table>");
    if (int(rr.heritage.size()) > 12) h += QStringLiteral("<p>…그 밖에 %1건</p>").arg(int(rr.heritage.size()) - 12);
  }
  h += QStringLiteral("<p style='color:%1'>").arg(t.inkMuted.name());
  for (const QString& n : ev.notes) h += n.toHtmlEscaped() + QStringLiteral("<br>");
  h += QStringLiteral("</p></body>");
  return h;
}
}  // namespace

MainWindow::MainWindow(const Options& options, QWidget* parent) : QMainWindow(parent), options_(options) {
  setWindowTitle(QStringLiteral("Strata 3D"));
  resize(1600, 960);

  viewport_ = new KaGlTerrainViewport(this);
  setCentralWidget(viewport_);

  buildRibbon();
  buildLayerDock();
  buildInspector();
  buildDocks();
  buildStatusBar();

  connect(viewport_, &KaGlTerrainViewport::frameRendered, this, &MainWindow::updateStatus);
  connect(viewport_, &KaGlTerrainViewport::pickRequested, this, &MainWindow::onPick);
  connect(viewport_, &KaGlTerrainViewport::selectRequested, this, &MainWindow::onSelectRay);
  connect(viewport_, &KaGlTerrainViewport::calloutClicked, this, &MainWindow::selectFeature);
  connect(viewport_, &KaGlTerrainViewport::sketchFinishRequested, this, &MainWindow::finishSketch);
  connect(viewport_, &KaGlTerrainViewport::trenchPointRequested, this, &MainWindow::onTrenchPoint);
  connect(viewport_, &KaGlTerrainViewport::mapDrawPointRequested, this, &MainWindow::onDrawPoint);
  connect(viewport_, &KaGlTerrainViewport::mapHoverRay, this, &MainWindow::onHoverRay);
  connect(viewport_, &KaGlTerrainViewport::mapDrawCancelRequested, this, [this] {
    if (drawStart_) {
      drawStart_.reset();
      previewLine_.reset();
      previewGrid_.reset();
      rebuildDrawOverlay();
    } else {
      setTool(Tool::Select);
    }
  });
  connect(&evalWatcher_, &QFutureWatcher<strata3d::Evaluation>::finished, this, &MainWindow::showEvaluation);
  connect(&drapeWatcher_, &QFutureWatcher<strata3d::DrapeResult>::finished, this, [this] {
    zoneDrape_ = drapeWatcher_.result();
    drapeLegend_ = zoneDrape_.legend;
    viewport_->setZoneLegend(zoneDrape_.legend);
    composeDrape();
  });
  connect(&relWatcher_, &QFutureWatcher<strata3d::RelationTable>::finished, this, [this] {
    const strata3d::RelationTable t = relWatcher_.result();
    lastRelations_ = t;
    relationPanel_->setTable(t, relationSiteLabel_);
    emit relationsFinished(true, QStringLiteral("관계 %1곳").arg(t.rows.size()));
  });
  connect(this, &MainWindow::surveyOpened, this, [this](bool ok, const QString&) {
    if (!ok) return;
    relationDock_->show();
    relationDock_->raise();
    // 조사를 열면 구역 판정·관계 표가 읽히도록 아래 칸을 창 높이의 36 % 로. 사용자가 끌어 바꿀 수 있다.
    resizeDocks({relationDock_}, {int(height() * 0.36)}, Qt::Vertical);
    computeSurveyRelations();
  });

  connect(&watcher_, &QFutureWatcher<LoadResult>::finished, this, [this] {
    lastLoad_ = watcher_.result();
    if (!lastLoad_.tree) {
      statusBar_->showMessage(QStringLiteral("불러오기 실패: ") + lastLoad_.error);
      emit loadFinished(false, lastLoad_.error);
      if (surveyPending_) { surveyPending_ = false; emit surveyOpened(false, lastLoad_.error); }
      emit sceneCompleted(false, lastLoad_.error);
      return;
    }
    viewport_->setTerrain(lastLoad_.tree);
    const strata3d::DemGrid& g = lastLoad_.tree->grid();
    statusBar_->setWorkCrs(g.crs);
    sceneComplete_ = false;
    clearTrench();  // 새 지형이면 이전 단면 좌표는 뜻이 없다
    evalOverlay_.reset();
    selectOverlay_.reset();
    selectedId_.clear();
    lines_.clear();
    grids_.clear();
    drawOverlay_.reset();
    setSchematicModelsShown(showSchematic_);
    applyLayerVisibility();  // 조사 겹침·지물·이름표를 레이어 칸의 체크대로
    if (restoreViewPending_) {
      restoreViewPending_ = false;
      loadViewState(viewStatePath());
    }
    QString msg = QStringLiteral("%1×%2 칸, 표고 %3~%4 m, %5초")
                      .arg(g.width).arg(g.height).arg(g.zMin, 0, 'f', 1).arg(g.zMax, 0, 'f', 1)
                      .arg(lastLoad_.seconds, 0, 'f', 1);
    if (lastLoad_.mesh) {
      const auto& m = *lastLoad_.mesh;
      msg += QStringLiteral(" | 건물 바닥 %1동 · 유산 구역 %2 · 유산 점 %3").arg(m.buildings).arg(m.heritageAreas).arg(m.heritagePoints);
    }
    if (!lastLoad_.warnings.isEmpty()) msg += QStringLiteral(" | 경고: ") + lastLoad_.warnings.join(QStringLiteral("; "));
    summary_ = msg;
    sourceLabel_->setText(QStringLiteral("DEM Copernicus %1 m").arg(g.pixelSize, 0, 'f', 0));
    emit loadFinished(true, msg);
    statusBar_->showMessage(msg, 15000);
    if (options_.loadSatellite) {
      const double cx = g.centerX(), cy = g.centerY(), half = g.width * g.pixelSize * 0.5;
      satWatcher_.setFuture(QtConcurrent::run(buildSatellite, options_, cx, cy, half));
    } else {
      sceneComplete_ = true;
      if (surveyPending_) { surveyPending_ = false; emit surveyOpened(true, QStringLiteral("조사 %1 열림").arg(survey_ ? survey_->path : QString())); }
      emit sceneCompleted(true, summary_);
    }
  });
  connect(&satWatcher_, &QFutureWatcher<SatelliteLoad>::finished, this, [this] {
    const SatelliteLoad s = satWatcher_.result();
    if (s.image.isNull()) {
      summary_ += QStringLiteral(" | 위성 실패: ") + s.error;
      emit satelliteFinished(false, s.error);
      sceneComplete_ = true;
      if (surveyPending_) { surveyPending_ = false; emit surveyOpened(true, QStringLiteral("조사 열림(위성 없이): ") + s.error); }
      emit sceneCompleted(false, s.error);
      return;
    }
    imageryName_ = s.source;
    viewport_->setSatellite(s.image, s.detail, s.detailHalfM);
    setTerrainMode(3);
    if (lastLoad_.tree)
      sourceLabel_->setText(QStringLiteral("DEM Copernicus %1 m · 위성 %2").arg(lastLoad_.tree->grid().pixelSize, 0, 'f', 0).arg(s.source));
    const QString m = QStringLiteral("위성 %1 %2×%3 + 중심 %4×%5, %6초").arg(s.source).arg(s.image.width()).arg(s.image.height()).arg(s.detail.width()).arg(s.detail.height()).arg(s.seconds, 0, 'f', 1);
    summary_ += QStringLiteral(" | ") + m;
    emit satelliteFinished(true, m);
    sceneComplete_ = true;
    if (surveyPending_) { surveyPending_ = false; emit surveyOpened(true, QStringLiteral("조사 %1 열림").arg(survey_ ? survey_->path : QString())); }
    emit sceneCompleted(true, summary_);
  });
}

// ---- 리본 ------------------------------------------------------------------------------------

void MainWindow::buildRibbon() {
  auto* bar = addToolBar(QStringLiteral("주요"));
  bar->setObjectName(QStringLiteral("ribbonBar"));
  bar->setMovable(false);
  bar->setFloatable(false);
  ribbon_ = new KaBeginnerRibbon(bar);
  for (const ribbon::Group& g : ribbon::groups()) {
    ribbon_->addGroup(g.id, g.caption);
    for (const ribbon::Button& b : g.buttons) {
      auto* a = new QAction(KaIcons::icon(b.iconId), b.label, this);
      a->setObjectName(QStringLiteral("s3d_") + b.id);
      a->setToolTip(b.tip);
      a->setCheckable(b.checkable);
      addAction(a);
      actions_.insert(b.id, a);
      QToolButton* btn = ribbon_->addAction(g.id, a);
      if (b.menu && btn) {
        btn->setMenu(menuFor(b.id));
        btn->setPopupMode(QToolButton::InstantPopup);
      }
      const auto pending = pendingButtons().find(b.id);
      if (pending != pendingButtons().end()) {
        a->setEnabled(false);
        a->setToolTip(b.tip + QStringLiteral("\n(아직 없음 — ") + *pending + QStringLiteral(")"));
      }
    }
  }
  ribbon_->applyTabOrder();
  bar->addWidget(ribbon_);
  buildPlaceSearch(bar);  // 리본이 늘어나 단추를 왼쪽에 모으므로 찾기는 오른쪽 끝(Strata 와 같음)
  // 저장은 Strata 처럼 진한 타일로 두드러지게 한다.
  actions_.value(QStringLiteral("save"))->setIcon(KaIcons::strongIcon(QStringLiteral("save")));

  auto act = [this](const char* id) { return actions_.value(QString::fromLatin1(id)); };
  act("open")->setShortcut(QKeySequence::Open);
  act("save")->setShortcut(QKeySequence::Save);
  act("save_as")->setShortcut(QKeySequence::SaveAs);
  act("pdf")->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
  act("print")->setShortcut(QKeySequence::Print);
  connect(act("open"), &QAction::triggered, this, &MainWindow::openSurveyDialog);
  act("new")->setShortcut(QKeySequence::New);
  connect(act("new"), &QAction::triggered, this, &MainWindow::newSurvey);
  // Ctrl+D: Strata 처럼 그리기. 마지막에 그린 레이어(처음에는 조사구역)로 바로 시작한다.
  auto* drawAgain = new QAction(QStringLiteral("그리기"), this);
  drawAgain->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
  connect(drawAgain, &QAction::triggered, this, [this] { startSketch(sketchKey_.isEmpty() ? QStringLiteral("survey_area") : sketchKey_); });
  addAction(drawAgain);
  connect(act("save"), &QAction::triggered, this, [this] {
    const QString path = viewStatePath();
    if (path.isEmpty()) {
      actions_.value(QStringLiteral("save_as"))->trigger();
      return;
    }
    if (!saveSurveyWork()) return;  // 실패 사유는 saveSurveyWork 가 알린다. 편집은 그대로 남는다
    statusBar_->showMessage(saveViewState(path) ? QStringLiteral("저장했습니다: %1 (3D 설정 %2)").arg(QFileInfo(options_.surveyGpkg).fileName(), QFileInfo(path).fileName())
                                                : QStringLiteral("3D 설정을 저장하지 못했습니다: ") + path, 8000);
  });
  connect(act("save_as"), &QAction::triggered, this, [this] {
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("3D 설정 저장"), viewStatePath(),
                                                      QStringLiteral("Strata 3D 설정 (*.s3d.json)"));
    if (path.isEmpty()) return;
    statusBar_->showMessage(saveViewState(path) ? QStringLiteral("3D 설정을 저장했습니다: ") + path
                                                : QStringLiteral("3D 설정을 저장하지 못했습니다: ") + path, 8000);
  });

  // 기록·분석의 지형 위 도구는 한 번에 하나만 켜진다(선택이 기본).
  auto* tools = new QActionGroup(this);
  tools->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
  for (const char* id : {"select", "measure", "trench_grid", "section"}) tools->addAction(act(id));
  act("select")->setChecked(true);
  connect(act("select"), &QAction::triggered, this, [this] { setTool(Tool::Select); });
  // 묶음 안에서 다른 도구를 켜면 앞 도구의 toggled(false) 가 뒤따라 온다: 그 도구가 아직 켜져 있을 때만 선택으로 돌린다.
  auto toolToggle = [this](Tool t) {
    return [this, t](bool on) {
      if (on) setTool(t);
      else if (tool_ == t) setTool(Tool::Select);
    };
  };
  connect(act("measure"), &QAction::toggled, this, toolToggle(Tool::Measure));
  connect(act("trench_grid"), &QAction::toggled, this, toolToggle(Tool::Grid));
  connect(act("section"), &QAction::toggled, this, toolToggle(Tool::Section));
  connect(act("section"), &QAction::toggled, this, [this](bool on) {
    if (on) { profileDock_->show(); profileDock_->raise(); }
  });
  connect(act("buffer"), &QAction::triggered, this, [this] {
    bufferDock_->show();
    bufferDock_->raise();
    if (survey_) {
      evaluateSurvey();
    } else {
      double x = 0, y = 0;
      if (viewport_->currentTargetWorld(&x, &y)) evaluateAt(x, y);
    }
  });

  // 지형 위에 올리기: 위성·지형(점토)은 둘 중 하나, DEM 단추는 표고 색띠·경사.
  auto* drape = new QActionGroup(this);
  drape->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
  drape->addAction(act("satellite"));
  drape->addAction(act("contour"));
  connect(act("satellite"), &QAction::triggered, this, [this](bool on) { setTerrainMode(on ? 3 : 0); });
  connect(act("contour"), &QAction::triggered, this, [this](bool on) { setTerrainMode(on ? 0 : 3); });

  connect(act("zones"), &QAction::triggered, this, [this] {
    relationDock_->show();
    relationDock_->raise();
    computeSurveyRelations();
  });
  connect(act("relations"), &QAction::triggered, this, [this] {
    relationDock_->show();
    relationDock_->raise();
    computeSurveyRelations();
  });
  connect(act("pdf"), &QAction::triggered, this, [this] {
    const QString png = QFileDialog::getSaveFileName(this, QStringLiteral("입체 입지도 저장"), QStringLiteral("입체입지도.png"), QStringLiteral("PNG (*.png)"));
    if (png.isEmpty()) return;
    QString pdf = png;
    pdf.replace(QStringLiteral(".png"), QStringLiteral(".pdf"));
    statusBar_->showMessage(exportSheet(png, pdf) ? QStringLiteral("도면을 저장했습니다: ") + pdf : QStringLiteral("도면을 만들지 못했습니다."), 8000);
  });
  connect(act("print"), &QAction::triggered, this, &MainWindow::printSheet);

  // 자료 받기: Strata 받기 흐름 그대로(StrataReceive), 범위는 조사구역 + 5 km
  receive_ = new StrataReceive(this, [this] { return options_.surveyGpkg; }, this);
  connect(act("heritage"), &QAction::triggered, receive_, &StrataReceive::fetchHeritage);
  connect(act("cadastral"), &QAction::triggered, receive_, &StrataReceive::fetchCadastral);
  // 토양·지질·수계: 이미 받았으면 레이어 칸의 체크를 켜고 끄고(Strata 와 같은 단추 동작), 없으면 받는다
  struct Thematic { const char* id; StrataReceive::Kind kind; const char16_t* layer; };
  for (const Thematic& t : {Thematic{"soil", StrataReceive::Kind::Soil, u"토양도(흙토람)"},
                            Thematic{"geology", StrataReceive::Kind::Geology, u"지질도(KIGAM 1:5만)"},
                            Thematic{"river", StrataReceive::Kind::River, u"수계도(하천망)"}}) {
    const QString layerName = QString::fromUtf16(t.layer);
    const StrataReceive::Kind kind = t.kind;
    QAction* a = act(t.id);
    connect(a, &QAction::triggered, this, [this, a, kind, layerName](bool on) {
      const QList<QgsMapLayer*> found = QgsProject::instance()->mapLayersByName(layerName);
      if (!found.isEmpty()) {
        if (QgsLayerTreeLayer* node = QgsProject::instance()->layerTreeRoot()->findLayer(found.first()))
          node->setItemVisibilityChecked(on);
        return;
      }
      a->setChecked(false);
      receive_->fetchThematic(kind);
    });
  }
  reloadTimer_.setSingleShot(true);
  reloadTimer_.setInterval(1500);  // 유산은 종류마다 하나씩 들어온다: 몰아서 한 번 다시 그린다
  connect(&reloadTimer_, &QTimer::timeout, this, &MainWindow::reloadKeepingView);
  connect(receive_, &StrataReceive::received, this, [this](const QString& what) {
    statusBar_->showMessage(QStringLiteral("받음: %1 — 3D 에 올립니다(「저장」하면 조사에 남습니다).").arg(what), 10000);
    refreshUnsavedChip();
    reloadTimer_.start();
  });
  connect(act("image"), &QAction::triggered, this, &MainWindow::saveImageDialog);
}

QMenu* MainWindow::menuFor(const QString& id) {
  auto* m = new QMenu(this);
  if (id == QLatin1String("dem")) {
    m->addAction(QStringLiteral("표고 색띠"), this, [this] { setTerrainMode(1); });
    m->addAction(QStringLiteral("경사"), this, [this] { setTerrainMode(2); });
    m->addSeparator();
    auto* src = m->addAction(QStringLiteral("원본: Copernicus GLO-30 (30 m)"));
    src->setEnabled(false);
    auto* ngii = m->addAction(QStringLiteral("원본: 국토지리정보원 DEM (0b단계)"));
    ngii->setEnabled(false);
  } else if (id == QLatin1String("draw_poly")) {
    // Strata 그리기 보조 줄의 조사 레이어(같은 레이어·같은 이름). 기준점은 측량값 입력 창이 있어 Strata 에서 넣는다.
    struct Item { const char* key; QString text; };
    for (const Item& it : {Item{"survey_area", QStringLiteral("조사구역 그리기")}, Item{"feature_poly", QStringLiteral("유구 그리기 (면)")},
                           Item{"feature_line", QStringLiteral("유구 선 그리기")}, Item{"section_line", QStringLiteral("단면선 그리기")},
                           Item{"artifact_point", QStringLiteral("유물 위치 표시")}}) {
      const QString key = QString::fromLatin1(it.key);
      m->addAction(it.text, this, [this, key] { startSketch(key); });
    }
    m->addSeparator();
    auto* how = m->addAction(QStringLiteral("지형 위를 눌러 꼭짓점 · 두 번 누르기/Enter 마침 · Backspace 한 점 지우기 · Esc 그만"));
    how->setEnabled(false);
  } else if (id == QLatin1String("view3d")) {
    m->addAction(QStringLiteral("조사구역 · 조감"), this, [this] { focusSurvey(35.0f); });
    m->addAction(QStringLiteral("조사구역 · 바로 위"), this, [this] { focusSurvey(89.0f); });
    m->addAction(QStringLiteral("북쪽을 위로"), this, [this] {
      double x = 0, y = 0;
      if (viewport_->currentTargetWorld(&x, &y)) viewport_->focusOnWorld(x, y, 0.0f, viewport_->pitchDegrees(), viewport_->distanceM());
    });
  } else if (id == QLatin1String("zscale")) {
    auto* g = new QActionGroup(m);
    for (float z : {1.0f, 1.5f, 2.0f, 3.0f}) {
      auto* a = m->addAction(QStringLiteral("높이 ×%1").arg(double(z), 0, 'f', 1));
      a->setCheckable(true);
      a->setChecked(z == 1.0f);
      a->setData(double(z));
      g->addAction(a);
      connect(a, &QAction::triggered, this, [this, z] { setZScale(z); });
    }
  } else if (id == QLatin1String("labels")) {
    auto* on = m->addAction(QStringLiteral("이름표 보이기"));
    on->setObjectName(QStringLiteral("s3d_labels_on"));
    on->setCheckable(true);
    on->setChecked(true);
    connect(on, &QAction::toggled, viewport_, &KaGlTerrainViewport::setShowCalloutLabels);
    m->addSeparator();
    m->addAction(QStringLiteral("크게 (Ctrl+=)"), this, [this] { viewport_->setHudFontScale(viewport_->hudFontScale() + 0.08f); viewport_->setHudBoxScale(viewport_->hudFontScale()); });
    m->addAction(QStringLiteral("작게 (Ctrl+-)"), this, [this] { viewport_->setHudFontScale(viewport_->hudFontScale() - 0.08f); viewport_->setHudBoxScale(viewport_->hudFontScale()); });
    m->addAction(QStringLiteral("기본 크기 (Ctrl+0)"), this, [this] { viewport_->setHudFontScale(0.85f); viewport_->setHudBoxScale(0.85f); });
  } else if (id == QLatin1String("tiers")) {
    auto* a = m->addAction(QStringLiteral("A 실측 — 지형·위성·지정 위치·내 조사"));
    a->setCheckable(true);
    a->setChecked(true);
    a->setEnabled(false);  // 실측 자료는 끌 수 없다(무엇 위에 무엇이 있는지의 바탕)
    auto* b = m->addAction(QStringLiteral("B 근거 — 보고서 치수 모델 (아직 없음)"));
    b->setCheckable(true);
    b->setEnabled(false);
    auto* d = m->addAction(QStringLiteral("D 도식 — 유형 틀 3D 모델 311개 (회색 선화)"));
    d->setObjectName(QStringLiteral("s3d_tier_d"));
    d->setCheckable(true);
    d->setChecked(showSchematic_);
    connect(d, &QAction::toggled, this, &MainWindow::setSchematicModelsShown);
  } else if (id == QLatin1String("more")) {
    auto* screen = m->addMenu(QStringLiteral("화면"));
    auto* ao = screen->addAction(QStringLiteral("그늘(환경 차폐)"));
    ao->setCheckable(true);
    ao->setChecked(viewport_->ambientOcclusion());
    connect(ao, &QAction::toggled, viewport_, &KaGlTerrainViewport::setAmbientOcclusion);
    auto* scene = screen->addAction(QStringLiteral("건물 바닥·유산 표지"));
    scene->setCheckable(true);
    scene->setChecked(true);
    connect(scene, &QAction::toggled, viewport_, &KaGlTerrainViewport::setSceneVisible);
    auto* load = m->addAction(QStringLiteral("불러오기 · 배경 (0b단계)"));
    load->setEnabled(false);
    m->addAction(QStringLiteral("계정과 키"), this, [this] {
      const QString key = strata3d::loadVworldApiKey();
      QString reason;
      const bool ok = !key.isEmpty() && strata3d::checkVworldApiKey(key, &reason);
      // 키 값은 보이지 않는다. 있음·없음과 확인 결과만.
      QMessageBox::information(this, QStringLiteral("계정과 키"),
                               key.isEmpty() ? QStringLiteral("VWorld 키: 없음 (Strata 「기타 › 계정과 키」에서 넣습니다)")
                                             : QStringLiteral("VWorld 키: 있음 · %1").arg(ok ? QStringLiteral("확인됨") : QStringLiteral("확인 실패 — ") + reason));
    });
    m->addAction(QStringLiteral("상태와 정보"), this, [this] {
      QMessageBox::information(this, QStringLiteral("상태와 정보"),
                               (summary_.isEmpty() ? QStringLiteral("장면 없음") : summary_) + QStringLiteral("\n\n") + viewport_->glInfo());
    });
  }
  return m;
}

// ---- 주소·지번 찾기(Strata) ----------------------------------------------------------------

void MainWindow::buildPlaceSearch(QToolBar* bar) {
  appBar_ = new KaAppBar(bar);
  locator_ = new LocationSearch(this);
  auto* region = new KaRegionLocator(appBar_);
  region->setObjectName(QStringLiteral("regionLocator"));
  appBar_->setRegionWidget(region);
  bar->addWidget(appBar_);
  auto search = [this](const QString& q, bool lot) {
    const QString text = q.trimmed();
    if (text.isEmpty()) {
      statusBar_->showMessage(QStringLiteral("주소·지번·지역·상호를 입력하세요"), 4000);
      return;
    }
    statusBar_->showMessage(QStringLiteral("위치 검색 중… %1").arg(text));
    if (lot) locator_->searchParcel(text);
    else locator_->search(text);
  };
  connect(appBar_, &KaAppBar::searchRequested, this, search);
  connect(region, &KaRegionLocator::searchRequested, this, [search](const QString& q) { search(q, false); });
  connect(region, &KaRegionLocator::parcelSearchRequested, this, [search](const QString& q) { search(q, true); });
  connect(region, &KaRegionLocator::regionSelected, this, [this](const QString& sido) {
    const auto bounds = KoreaRegionCatalog::overviewBounds(sido);
    if (!bounds) return;
    locator_->cancel();
    LocationHit hit;
    hit.title = sido;
    hit.west = bounds->west; hit.south = bounds->south;
    hit.east = bounds->east; hit.north = bounds->north;
    hit.lon = (hit.west + hit.east) * 0.5;
    hit.lat = (hit.south + hit.north) * 0.5;
    hit.hasBbox = true;
    goToLocation(hit);
  });
  connect(locator_, &LocationSearch::failed, this, [this](const QString& msg) {
    statusBar_->showMessage(msg, 8000);
    QMessageBox::information(this, QStringLiteral("위치 검색"), msg);
  });
  connect(locator_, &LocationSearch::finished, this, [this](const QVector<LocationHit>& hits) {
    if (hits.isEmpty()) {
      statusBar_->showMessage(QStringLiteral("검색 결과 없음"), 8000);
      return;
    }
    if (hits.size() == 1) {
      goToLocation(hits.first());
      return;
    }
    QStringList labels;
    for (const LocationHit& h : hits) labels << (h.detail.isEmpty() ? h.title : h.title + QStringLiteral("  —  ") + h.detail);
    bool ok = false;
    const QString pick = QInputDialog::getItem(this, QStringLiteral("위치 선택"),
                                               QStringLiteral("검색 결과 %1건 — 이동할 위치를 선택하세요").arg(hits.size()), labels, 0, false, &ok);
    const int idx = labels.indexOf(pick);
    if (ok && idx >= 0) goToLocation(hits.at(idx));
  });
  auto* find = new QAction(QStringLiteral("주소·지번 찾기"), this);
  find->setShortcut(QKeySequence::Find);
  connect(find, &QAction::triggered, appBar_, &KaAppBar::focusSearch);
  addAction(find);
}

void MainWindow::goToLocation(const LocationHit& hit) {
  double lon = hit.lon, lat = hit.lat;
  if (std::abs(lon) <= 90.0 && std::abs(lat) > 90.0) std::swap(lon, lat);  // Strata 와 같은 뒤바뀜 보정
  const QString crs = lastLoad_.tree ? lastLoad_.tree->grid().crs : options_.workCrs;
  double x = lon, y = lat;
  QString err;
  if (!strata3d::transformPoint(QStringLiteral("EPSG:4326"), crs, &x, &y, &err)) {
    statusBar_->showMessage(QStringLiteral("위치를 옮기지 못했습니다: ") + err, 8000);
    return;
  }
  // 범위가 있으면 그 상자가 화면에 들어오는 거리, 없으면 1.2 km
  float dist = 1200.0f;
  if (hit.hasBbox) {
    double wx = hit.west, sy = hit.south, ex = hit.east, ny = hit.north;
    if (strata3d::transformPoint(QStringLiteral("EPSG:4326"), crs, &wx, &sy) && strata3d::transformPoint(QStringLiteral("EPSG:4326"), crs, &ex, &ny))
      dist = float(std::clamp(std::max(ex - wx, ny - sy) * 1.3, 600.0, 200000.0));
  }
  const QString where = hit.detail.isEmpty() ? hit.title : hit.title + QStringLiteral(" · ") + hit.detail;
  float z = 0;
  if (lastLoad_.tree && lastLoad_.tree->grid().sample(x, y, &z)) {
    viewport_->focusOnWorld(x, y, viewport_->yawDegrees(), std::max(30.0f, viewport_->pitchDegrees()), dist);
    statusBar_->showMessage(QStringLiteral("이동: ") + where, 8000);
    return;
  }
  // 지금 지형 창 밖: 그 자리를 가운데로 지형·지물·위성을 다시 받는다(조사는 그대로 둔다).
  options_.lon = lon;
  options_.lat = lat;
  viewport_->focusOnWorld(x, y, viewport_->yawDegrees(), std::max(30.0f, viewport_->pitchDegrees()), dist);
  statusBar_->showMessage(QStringLiteral("지형을 다시 불러옵니다: ") + where);
  loadScene();
}

// ---- 레이어 칸(Strata) -------------------------------------------------------------------------

void MainWindow::buildLayerDock() {
  layerDock_ = new QDockWidget(QStringLiteral("레이어"), this);
  layerDock_->setObjectName(QStringLiteral("layerDock"));
  layerDock_->setTitleBarWidget(new QWidget(layerDock_));  // 제목은 칸 안의 「레이어」(Strata 와 같은 모양)
  layerDock_->setFeatures(QDockWidget::NoDockWidgetFeatures);
  layers_ = new StrataLayerDock(QgsProject::instance(), layerDock_);
  layers_->setMinimumWidth(240);
  layerDock_->setWidget(layers_);
  addDockWidget(Qt::LeftDockWidgetArea, layerDock_);
  connect(layers_, &StrataLayerDock::layersVisibilityChanged, this, &MainWindow::applyLayerVisibility);
}

std::vector<strata3d::GeoFeature> MainWindow::visibleFeatures() const {
  std::vector<strata3d::GeoFeature> out;
  if (!lastLoad_.features) return out;
  const QSet<QString> hidden = layers_ ? layers_->hiddenLayerNames() : QSet<QString>();
  out.reserve(lastLoad_.features->size());
  for (const strata3d::GeoFeature& f : *lastLoad_.features)
    if (!hidden.contains(f.category)) out.push_back(f);
  return out;
}

void MainWindow::applyLayerVisibility() {
  // 리본의 토양·지질·수계 칩은 그 레이어가 켜져 있을 때 눌린 모양(Strata 와 같음)
  for (const auto& [id, name] : {std::pair<const char*, const char16_t*>{"soil", u"토양도(흙토람)"}, {"geology", u"지질도(KIGAM 1:5만)"}, {"river", u"수계도(하천망)"}}) {
    QAction* a = actions_.value(QString::fromLatin1(id));
    if (!a) continue;
    bool on = false;
    for (QgsMapLayer* l : QgsProject::instance()->mapLayersByName(QString::fromUtf16(name)))
      if (QgsLayerTreeLayer* node = QgsProject::instance()->layerTreeRoot()->findLayer(l)) on = on || node->isVisible();
    const QSignalBlocker block(a);
    a->setChecked(on);
  }
  if (!lastLoad_.tree) return;
  const strata3d::DemGrid& g = lastLoad_.tree->grid();
  const QSet<QString> hiddenNames = layers_ ? layers_->hiddenLayerNames() : QSet<QString>();
  if (lastLoad_.features && lastLoad_.mesh) {
    bool anyHidden = false;
    for (const strata3d::GeoFeature& f : *lastLoad_.features)
      if (hiddenNames.contains(f.category)) { anyHidden = true; break; }
    viewport_->setScene(anyHidden ? strata3d::buildSceneMesh(g, visibleFeatures()) : lastLoad_.mesh);
  }
  // 구역·유산 면은 지형 텍스처로(워커). 켠 레이어만.
  if (lastLoad_.tree) {
    auto tree = lastLoad_.tree;
    auto feats = std::make_shared<std::vector<strata3d::GeoFeature>>(visibleFeatures());
    if (survey_) {
      const QSet<QString> keys = layers_ ? layers_->hiddenLayerKeys() : QSet<QString>();
      if (!keys.contains(QStringLiteral("survey_area")))
        for (const auto& ring : survey_->surveyAreas) {
          strata3d::GeoFeature f;
          f.kind = strata3d::FeatureKind::HeritageArea;
          f.category = QStringLiteral("조사구역");
          f.source = QStringLiteral("SURVEY");
          f.ring = ring;
          feats->push_back(std::move(f));
        }
    }
    drapeWatcher_.setFuture(QtConcurrent::run([tree, feats] { return strata3d::rasterizeDrape(tree->grid(), *feats); }));
    rebuildQgisDrape();
  }
  if (survey_) {
    // 조사 레이어(조사구역·트렌치·유구 …)는 ka_hgis/layer_key 로 가른다
    const QSet<QString> keys = layers_ ? layers_->hiddenLayerKeys() : QSet<QString>();
    strata3d::StrataSurvey shown = *survey_;
    if (keys.contains(QStringLiteral("survey_area"))) shown.surveyAreas.clear();
    if (keys.contains(QStringLiteral("trial_trench"))) shown.trenches.clear();
    if (keys.contains(QStringLiteral("feature_poly"))) shown.featurePolys.clear();
    if (keys.contains(QStringLiteral("feature_line"))) shown.featureLines.clear();
    if (keys.contains(QStringLiteral("section_line"))) shown.sectionLines.clear();
    if (keys.contains(QStringLiteral("control_points"))) shown.controlPoints.clear();
    if (keys.contains(QStringLiteral("artifact_point"))) shown.artifactPoints.clear();
    surveyOverlay_ = strata3d::buildSurveyOverlay(g, shown);
    viewport_->setKeepClear(shown.surveyAreas);
  }
  rebuildLabels();
  refreshOverlay();
}

// ---- 속성 칸(Strata)·고르기 ------------------------------------------------------------------

void MainWindow::buildInspector() {
  inspectorDock_ = new QDockWidget(QStringLiteral("속성"), this);
  inspectorDock_->setObjectName(QStringLiteral("inspectorDock"));
  inspectorDock_->setTitleBarWidget(new QWidget(inspectorDock_));  // 제목은 칸 안의 「선택한 …」(Strata 와 같은 모양)
  inspectorDock_->setFeatures(QDockWidget::NoDockWidgetFeatures);
  inspector_ = new KaInspectorPanel(inspectorDock_);
  inspector_->setMinimumWidth(KaInspectorPanel::kMinimumWidth);
  inspector_->setMaximumWidth(KaInspectorPanel::kMaximumWidth);
  card_ = new KaFeatureCard(inspector_->attributeHost());
  card_->hide();  // 하나를 골랐을 때만 보인다(Strata 와 같음)
  inspector_->attributeLayout()->addWidget(card_, 0);
  inspector_->setFeatureCard(card_);
  heritageCard_ = new HeritageCard(inspector_->attributeHost());
  inspector_->attributeLayout()->addWidget(heritageCard_, 0);
  inspectorDock_->setWidget(inspector_);
  addDockWidget(Qt::RightDockWidgetArea, inspectorDock_);
  connect(inspector_, &KaInspectorPanel::collapseRequested, inspectorDock_, &QDockWidget::hide);
  connect(QgsProject::instance(), &QgsProject::layersRemoved, this, [this](const QStringList&) { selectFeature(QString()); });
}

void MainWindow::selectFeature(const QString& featureId) {
  selectedId_ = featureId;
  selectOverlay_.reset();
  const strata3d::GeoFeature* found = nullptr;
  if (!featureId.isEmpty() && lastLoad_.features)
    for (const strata3d::GeoFeature& f : *lastLoad_.features)
      if (f.id == featureId) { found = &f; break; }
  QgsVectorLayer* layer = nullptr;
  QgsFeatureId fid = FID_NULL;
  if (found && found->id.startsWith(QLatin1String("shp:"))) {
    // id = shp:<조사 프로젝트의 레이어 이름>/<OGR FID>[.<여러 조각 번호>]. GPKG·SHP 모두 QGIS 의 fid 와 같다.
    const QString rest = found->id.mid(4);
    const int slash = rest.lastIndexOf(QLatin1Char('/'));
    const QString layerName = rest.left(slash);
    fid = rest.mid(slash + 1).section(QLatin1Char('.'), 0, 0).toLongLong();
    for (QgsMapLayer* ml : QgsProject::instance()->mapLayersByName(layerName))
      if ((layer = qobject_cast<QgsVectorLayer*>(ml))) break;
  }
  const bool reference = layer && LayerOps::isReferenceLayer(layer);
  if (layer && !reference) {
    // 조사 레이어(조사구역·트렌치·유구): Strata 카드 그대로(번호·종류·시대·면적·조사카드)
    heritageCard_->clearRecord();
    card_->setFeature(layer, fid);
    card_->setVisible(card_->hasFeature());
    inspector_->setSelectionCount(card_->hasFeature() ? 1 : 0);
  } else {
    card_->clear();
    card_->hide();
    if (found) showHeritageCard(*found, layer, fid);
    else heritageCard_->clearRecord();
    inspector_->setSelectionCount(found ? 1 : 0);
  }
  if (found) inspectorDock_->show();
  if (found && !(layer && !reference)) {
    // Strata 속성 칸의 제목·안내문은 Strata 카드를 따른다. 유적 카드를 보일 때는 그 두 줄만 3D 쪽 말로 바꾼다
    // (Strata 카드를 비우면 Strata 가 다시 제 말로 돌려 놓는다).
    // Strata 칸이 카드 신호를 받아 한 박자 뒤에 다시 쓰므로 그 뒤에 한 번 더 적용한다.
    auto apply = [this] {
      if (!heritageCard_->isVisibleTo(inspector_)) return;
      if (auto* title = inspector_->findChild<QLabel*>(QStringLiteral("inspectorTitle"))) title->setText(QStringLiteral("선택한 유적"));
      // 같은 이름의 안내문이 스타일 탭에도 있다: 속성 쪽에서만 찾는다
      if (auto* sentence = inspector_->attributeHost()->findChild<QLabel*>(QStringLiteral("inspectorSentence"), Qt::FindDirectChildrenOnly))
        sentence->hide();
    };
    apply();
    QTimer::singleShot(0, this, apply);
  }
  if (found && lastLoad_.tree) {
    // 3D 강조: 면은 경계 띠, 점은 표지 기둥(진한 흙색)
    const strata3d::DemGrid& g = lastLoad_.tree->grid();
    auto m = std::make_shared<strata3d::SceneMesh>();
    const std::array<float, 4> clay{0.71f, 0.34f, 0.23f, 1.0f};
    if (found->ring.size() >= 3) strata3d::appendDrapedRibbon(m.get(), g, found->ring, true, 4.0f, clay, 1.6f);
    strata3d::appendPole(m.get(), g, found->point, 2.5f, 30.0f, clay);
    selectOverlay_ = m;
    const QString name = found->name.isEmpty() ? QStringLiteral("(이름 없음)") : strata3d::siteNameOnly(found->name);
    statusBar_->showMessage(layer ? QStringLiteral("고름: %1 · %2").arg(name, found->category)
                                  : QStringLiteral("고름: %1 · %2 (%3 자료, Strata 조사 레이어 아님)").arg(name, found->category, found->source),
                            8000);
  }
  refreshOverlay();
}

void MainWindow::showHeritageCard(const strata3d::GeoFeature& f, QgsVectorLayer* layer, qint64 fid) {
  HeritageCard::Rows record;
  if (layer) {
    // 원 기록: Strata 가 받아 둔 레이어의 그 기록, 칸 순서 그대로(빈 칸은 뺀다)
    const QgsFeature feat = layer->getFeature(QgsFeatureId(fid));
    const QgsFields fields = feat.fields();
    for (int i = 0; i < fields.count(); ++i) {
      const QString name = fields.at(i).name();
      // 내부 번호(fid·__hgis_*)와 코드 칸(이름 칸이 따로 있음)은 뺀다. 지정번호는 2021-11-19 이후 쓰지 않으므로 보이지 않는다.
      if (name.compare(QLatin1String("fid"), Qt::CaseInsensitive) == 0 || name.startsWith(QLatin1String("__"))) continue;
      if (name == QStringLiteral("지정번호") || (name.endsWith(QStringLiteral("코드")) && name != QStringLiteral("유산코드"))) continue;
      const QVariant v = feat.attribute(i);
      const QString text = v.isNull() ? QString() : v.toString().trimmed();
      if (!text.isEmpty()) record.append({fields.at(i).alias().isEmpty() ? name : fields.at(i).alias(), text});
    }
  } else {
    for (auto it = f.attrs.cbegin(); it != f.attrs.cend(); ++it)
      if (!it.value().trimmed().isEmpty()) record.append({it.key(), it.value()});
    if (!f.category.isEmpty()) record.prepend({QStringLiteral("종목"), f.category});
  }
  HeritageCard::Rows computed;
  const QString shown = strata3d::displayHeritageName(f.name);
  const strata3d::RelationRow* row = nullptr;
  if (lastRelations_)
    for (const auto& r : lastRelations_->rows)
      if (r.item.name == shown && r.item.category == f.category) { row = &r; break; }
  if (row) {
    computed.append({QStringLiteral("거리"), QStringLiteral("%1 m (조사구역 경계에서 최단)").arg(row->item.distanceM, 0, 'f', 1)});
    computed.append({QStringLiteral("방위"), QStringLiteral("%1 %2° (조사구역 중심에서)").arg(row->item.bearingName).arg(row->item.bearingDeg, 0, 'f', 0)});
    computed.append({QStringLiteral("높이차"), QStringLiteral("%1%2 m (현재 지형 30 m 격자)").arg(row->item.deltaElevM >= 0 ? QStringLiteral("+") : QString()).arg(row->item.deltaElevM, 0, 'f', 1)});
    if (!row->period.isEmpty()) computed.append({QStringLiteral("시대"), row->period + QStringLiteral(" (국가유산 공간정보)")});
  } else if (survey_) {
    computed.append({QStringLiteral("거리"), lastRelations_ ? QStringLiteral("조사구역에서 %1 m 밖").arg(lastRelations_->radiusM, 0, 'f', 0)
                                                           : QStringLiteral("「관계 표」를 계산하면 보입니다")});
  }
  const QString src = f.source == QLatin1String("SHP") ? QStringLiteral("Strata 주변유적(국가유산청 GIS 통합인트라넷)")
                      : f.source == QLatin1String("KHS") ? QStringLiteral("국가유산청 Open API")
                                                         : f.source;
  heritageCard_->setRecord(strata3d::siteNameOnly(f.name), f.category, record, computed,
                           QStringLiteral("출처 %1 · 위치 등급 A 실측").arg(src));
}

void MainWindow::onSelectRay(const QVector3D& origin, const QVector3D& direction, double pixelM) {
  strata3d::Pt p;
  if (!groundAt(origin, direction, &p) || !lastLoad_.features) {
    selectFeature(QString());
    return;
  }
  // 점 지물은 화면 12 px 안의 가장 가까운 것, 아니면 누른 점을 품은 면 중 가장 작은 것(구역보다 유적 면이 먼저).
  const QSet<QString> hidden = layers_ ? layers_->hiddenLayerNames() : QSet<QString>();
  const double tol = std::max(8.0, 12.0 * pixelM);
  QString best;
  double bestD = tol;
  for (const strata3d::GeoFeature& f : *lastLoad_.features) {
    if (f.kind != strata3d::FeatureKind::HeritagePoint || hidden.contains(f.category)) continue;
    const double d = std::hypot(f.point.x - p.x, f.point.y - p.y);
    if (d < bestD) { bestD = d; best = f.id; }
  }
  if (best.isEmpty()) {
    OGRPoint at(p.x, p.y);
    double bestArea = 1e300;
    bool bestIsZone = true;
    for (const strata3d::GeoFeature& f : *lastLoad_.features) {
      if (f.kind != strata3d::FeatureKind::HeritageArea || f.ring.size() < 3 || hidden.contains(f.category)) continue;
      double x0 = 1e300, x1 = -1e300, y0 = 1e300, y1 = -1e300;
      for (const auto& q : f.ring) { x0 = std::min(x0, q.x); x1 = std::max(x1, q.x); y0 = std::min(y0, q.y); y1 = std::max(y1, q.y); }
      if (p.x < x0 || p.x > x1 || p.y < y0 || p.y > y1) continue;
      OGRPolygon poly;
      auto ring = [](const std::vector<strata3d::Pt>& r) {
        auto* lr = new OGRLinearRing;
        for (const auto& q : r) lr->addPoint(q.x, q.y);
        lr->closeRings();
        return lr;
      };
      poly.addRingDirectly(ring(f.ring));
      for (const auto& h : f.holes) poly.addRingDirectly(ring(h));
      if (!poly.Contains(&at)) continue;
      const bool zone = strata3d::zoneLayerNames().contains(f.category);
      const double area = poly.get_Area();
      if ((bestIsZone && !zone) || (zone == bestIsZone && area < bestArea)) {
        best = f.id;
        bestArea = area;
        bestIsZone = zone;
      }
    }
  }
  selectFeature(best);
}

// ---- Strata 레이어(토양·지질·수계·지적·수치지형 …)를 Strata 스타일 그대로 지형에 입힌다 ------------------

namespace {
// 3D 가 따로 그리는 것(위성·지형 음영·DEM·2D 배경 지도·조사 레이어·주변유적)은 빼고, 지형에 입힐 참조 지도만
bool drapeFromQgis(QgsMapLayer* l) {
  if (!l || !l->isValid()) return false;
  if (!l->customProperty(QStringLiteral("ka_hgis/heritage_dataset")).toString().isEmpty()) return false;  // 구역 색은 DrapeRaster 가
  if (!LayerOps::layerKeyOf(l).isEmpty()) return false;  // 조사 레이어는 3D 겹침으로
  const QString kind = l->customProperty(QStringLiteral("ka_hgis/reference_kind")).toString();
  static const QStringList skip = {QStringLiteral("vworld_base"), QStringLiteral("satellite"), QStringLiteral("vworld_hybrid"),
                                   QStringLiteral("vworld_contour"), QStringLiteral("osm"), QStringLiteral("carto"),
                                   QStringLiteral("google_road"), QStringLiteral("terrain"), QStringLiteral("dem"),
                                   QStringLiteral("dem_relief"), QStringLiteral("tile_pack"), QStringLiteral("admin_mask")};
  if (skip.contains(kind)) return false;
  // 이름만 있는 옛 배경(태그 전)과 DSM 래스터
  if (l->type() == Qgis::LayerType::Raster && kind.isEmpty()) return false;
  return true;
}
}  // namespace

QList<QgsMapLayer*> MainWindow::qgisDrapeLayers() const {
  QList<QgsMapLayer*> out;
  QgsProject* proj = QgsProject::instance();
  QgsLayerTree* root = proj->layerTreeRoot();
  if (!root || options_.surveyGpkg.isEmpty() || !projectError_.isEmpty()) return out;
  // 위에 있는 레이어가 위에 칠해지게: QgsMapSettings 는 목록의 앞을 위에 그린다
  const QList<QgsMapLayer*> order = root->hasCustomLayerOrder() ? root->customLayerOrder() : root->layerOrder();
  for (QgsMapLayer* l : order) {
    QgsLayerTreeLayer* node = root->findLayer(l);
    if (node && node->isVisible() && drapeFromQgis(l)) out << l;
  }
  return out;
}

void MainWindow::rebuildQgisDrape() {
  if (!lastLoad_.tree) return;
  const QList<QgsMapLayer*> layers = qgisDrapeLayers();
  ++qgisDrapeGeneration_;
  if (layers.isEmpty()) {
    qgisDrape_ = QImage();
    qgisDrapeDetail_ = QImage();
    composeDrape();
    return;
  }
  const strata3d::DemGrid& g = lastLoad_.tree->grid();
  const double spanX = g.width * g.pixelSize, spanY = g.height * g.pixelSize;
  const QgsRectangle full(g.originX, g.originY - spanY, g.originX + spanX, g.originY);
  const double half = zoneDrape_.detailHalfM > 0 ? zoneDrape_.detailHalfM : 3000.0;
  const QgsRectangle detail(g.centerX() - half, g.centerY() - half, g.centerX() + half, g.centerY() + half);
  const quint64 gen = qgisDrapeGeneration_;
  auto render = [this, layers, gen](const QgsRectangle& ext, QSize size, bool isDetail) {
    QgsMapSettings ms;
    ms.setLayers(layers);
    ms.setDestinationCrs(QgsCoordinateReferenceSystem(lastLoad_.tree->grid().crs));
    ms.setTransformContext(QgsProject::instance()->transformContext());
    ms.setExtent(ext);
    ms.setOutputSize(size);
    ms.setBackgroundColor(Qt::transparent);
    ms.setFlag(Qgis::MapSettingsFlag::Antialiasing, true);
    ms.setFlag(Qgis::MapSettingsFlag::DrawLabeling, false);  // 글자는 3D 이름표로(지형에 눕힌 글자는 읽기 어렵다)
    auto* job = new QgsMapRendererParallelJob(ms);
    job->setParent(this);  // 창이 닫힐 때 찾아 멈추려고
    connect(job, &QgsMapRendererJob::finished, this, [this, job, gen, isDetail] {
      const QImage img = job->renderedImage();
      job->deleteLater();
      if (gen != qgisDrapeGeneration_) return;  // 그사이 다시 요청됨
      (isDetail ? qgisDrapeDetail_ : qgisDrape_) = img.convertToFormat(QImage::Format_RGBA8888);
      if (!qgisDrape_.isNull() && (!qgisDrapeDetail_.isNull() || zoneDrape_.detailHalfM <= 0)) composeDrape();
    });
    job->start();
  };
  qgisDrape_ = QImage();
  qgisDrapeDetail_ = QImage();
  render(full, QSize(4096, int(std::lround(4096.0 * spanY / spanX))), false);
  render(detail, QSize(4096, 4096), true);
}

void MainWindow::composeDrape() {
  // 아래: Strata 레이어(QGIS 가 Strata 스타일로 그린 것), 위: 구역·유산 면과 조사구역(DrapeRaster)
  auto over = [](const QImage& under, const QImage& top) {
    if (under.isNull()) return top;
    if (top.isNull()) return under;
    QImage out = under.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    QPainter p(&out);
    p.drawImage(out.rect(), top);
    p.end();
    return out.convertToFormat(QImage::Format_RGBA8888);
  };
  const QImage full = over(qgisDrape_, zoneDrape_.image);
  const QImage detail = over(qgisDrapeDetail_, zoneDrape_.detail);
  viewport_->setDrape(full, detail, detail.isNull() ? 0.0 : (zoneDrape_.detailHalfM > 0 ? zoneDrape_.detailHalfM : 3000.0));
}

// ---- 칸·상태줄 ---------------------------------------------------------------------------------

void MainWindow::buildDocks() {
  setCorner(Qt::BottomLeftCorner, Qt::BottomDockWidgetArea);
  setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);

  relationDock_ = new QDockWidget(QStringLiteral("구역 · 관계 표"), this);
  relationDock_->setObjectName(QStringLiteral("relationDock"));
  relationPanel_ = new RelationPanel(relationDock_);
  relationDock_->setWidget(relationPanel_);
  addDockWidget(Qt::BottomDockWidgetArea, relationDock_);

  profileDock_ = new QDockWidget(QStringLiteral("단면"), this);
  profileDock_->setObjectName(QStringLiteral("profileDock"));
  auto* profileBox = new QWidget(profileDock_);
  auto* profileLayout = new QVBoxLayout(profileBox);
  profileLayout->setContentsMargins(8, 6, 8, 6);
  trenchInfo_ = new QLabel(QStringLiteral("단면 없음. 「단면」을 켜고 지형 위 두 점을 누르세요."), profileBox);
  trenchInfo_->setWordWrap(true);
  plot_ = new ProfilePlot(profileBox);
  profileLayout->addWidget(trenchInfo_);
  profileLayout->addWidget(plot_, 1);
  profileDock_->setWidget(profileBox);
  addDockWidget(Qt::BottomDockWidgetArea, profileDock_);
  tabifyDockWidget(relationDock_, profileDock_);

  bufferDock_ = new QDockWidget(QStringLiteral("버퍼"), this);
  bufferDock_->setObjectName(QStringLiteral("bufferDock"));
  report_ = new QTextBrowser(bufferDock_);
  report_->setHtml(QStringLiteral("<p>「버퍼」를 누르면 조사구역 둘레 500 m · 1000 m 안의 유산을 셉니다. 지형을 더블클릭하면 그 점을 기준으로 셉니다.</p>"));
  bufferDock_->setWidget(report_);
  addDockWidget(Qt::BottomDockWidgetArea, bufferDock_);
  tabifyDockWidget(relationDock_, bufferDock_);

  // 아래 칸은 필요할 때만 열린다(설계도 v3 2장). 처음에는 3D 화면만 보인다.
  relationDock_->hide();
  profileDock_->hide();
  bufferDock_->hide();
}

void MainWindow::buildStatusBar() {
  statusBar_ = new KaStatusBar(this);
  setStatusBar(statusBar_);
  // 3D 에는 한 가지 축척이 없다: 축척 칸 자리에 커서 지점 표고와 높이 배율을 둔다.
  statusBar_->setInstrumentsVisible(true, false);
  statusBar_->setCrsChipsVisible(true);
  statusBar_->setWorkCrs(options_.workCrs);
  statusBar_->setUploadCrs(QStringLiteral("EPSG:5179"));
  if (auto* render = statusBar_->findChild<QToolButton*>(QStringLiteral("renderToggle"))) render->hide();
  elevLabel_ = new QLabel(QStringLiteral("표고 —"), statusBar_);
  elevLabel_->setObjectName(QStringLiteral("s3dElevation"));
  elevLabel_->setMinimumWidth(110);
  elevLabel_->setToolTip(QStringLiteral("커서 지점의 지표 표고(현재 DEM)"));
  statusBar_->insertPermanentWidget(3, elevLabel_);
  zScaleLabel_ = new QLabel(QStringLiteral("높이 ×1.0"), statusBar_);
  zScaleLabel_->setObjectName(QStringLiteral("s3dZScale"));
  statusBar_->insertPermanentWidget(4, zScaleLabel_);
  sourceLabel_ = new QLabel(statusBar_);
  sourceLabel_->setObjectName(QStringLiteral("s3dSources"));
  statusBar_->addPermanentWidget(sourceLabel_);
  connect(statusBar_, &KaStatusBar::submitClicked, this, [this] {
    statusBar_->showMessage(QStringLiteral("검수·제출(5179 묶음)은 Strata 에서 합니다."), 6000);
  });
  connect(statusBar_, &KaStatusBar::crsClicked, this, [this] {
    statusBar_->showMessage(QStringLiteral("작업 좌표계는 조사 파일을 따릅니다(Strata 에서 정함)."), 6000);
  });
}

void MainWindow::updateStatus() {}

// ---- 장면 ------------------------------------------------------------------------------------

void MainWindow::loadScene() {
  options_.heritageFromProject = survey_ && projectError_.isEmpty() && !options_.surveyGpkg.isEmpty();
  options_.heritageSources = options_.heritageFromProject ? heritageSourcesFromProject(QgsProject::instance())
                                                         : std::vector<strata3d::LocalVectorSource>{};
  statusBar_->showMessage(QStringLiteral("실측 DEM 불러오는 중…"));
  watcher_.setFuture(QtConcurrent::run(buildScene, options_));
}

void MainWindow::setTerrainMode(int mode) {
  mode_ = mode;
  viewport_->setMode(KaGlTerrainViewport::Mode(mode));
  if (auto* a = actions_.value(QStringLiteral("satellite"))) a->setChecked(mode == 3);
  if (auto* a = actions_.value(QStringLiteral("contour"))) a->setChecked(mode == 0);
}

void MainWindow::setZScale(float z) {
  zScale_ = z;
  viewport_->setZScale(z);
  zScaleLabel_->setText(QStringLiteral("높이 ×%1").arg(double(z), 0, 'f', 1));
}

void MainWindow::setSchematicModelsShown(bool on) {
  showSchematic_ = on;
  if (!lastLoad_.tree || !on) {
    viewport_->setSchematic(nullptr);
    return;
  }
  viewport_->setSchematic(strata3d::BangriTerrainValidator::buildSchematicModels(lastLoad_.tree->grid()));
}

void MainWindow::rebuildLabels() {
  std::vector<KaGlTerrainViewport::CalloutLabel3D> labels;
  if (lastLoad_.features && lastLoad_.tree) {
    const QSet<QString> hidden = layers_ ? layers_->hiddenLayerNames() : QSet<QString>();
    QSet<QString> seen;
    for (const strata3d::GeoFeature& f : *lastLoad_.features) {
      if (hidden.contains(f.category)) continue;
      if (!labelledHeritage(f)) continue;
      const QString name = strata3d::siteNameOnly(f.name);
      if (name.isEmpty() || seen.contains(name)) continue;  // 같은 유산이 점·면·두 출처로 있으면 한 번만
      seen.insert(name);
      KaGlTerrainViewport::CalloutLabel3D l;
      l.worldX = f.point.x;
      l.worldY = f.point.y;
      l.heightAboveGroundM = 6.0f;
      l.title = name;
      l.badgeColor = KaTheme::tokens().accent;
      l.important = false;
      l.featureId = f.id;
      labels.push_back(std::move(l));
    }
  }
  // 측거 선·시굴격자의 치수(기록한 값)도 지형 위에 단다
  if (lastLoad_.evaluator) {
    for (size_t i = 0; i < lines_.size(); ++i) {
      const auto& ln = lines_[i];
      const double planar = std::hypot(ln.b.x - ln.a.x, ln.b.y - ln.a.y);
      const auto prof = lastLoad_.evaluator->terrainProfile(ln.a, ln.b, std::clamp(planar / 200.0, 0.5, 5.0));
      double surface = 0;
      for (size_t k = 1; k < prof.size(); ++k)
        surface += std::hypot(prof[k].distanceM - prof[k - 1].distanceM, prof[k].elevationM - prof[k - 1].elevationM);
      const double dz = prof.size() >= 2 ? prof.back().elevationM - prof.front().elevationM : 0.0;
      KaGlTerrainViewport::CalloutLabel3D l;
      l.worldX = 0.5 * (ln.a.x + ln.b.x);
      l.worldY = 0.5 * (ln.a.y + ln.b.y);
      l.heightAboveGroundM = 4.0f;
      l.title = QStringLiteral("평면 %1 m · 지표 %2 m · 높이차 %3%4 m")
                    .arg(planar, 0, 'f', 1).arg(surface, 0, 'f', 1).arg(dz >= 0 ? QStringLiteral("+") : QString()).arg(dz, 0, 'f', 1);
      l.badgeColor = KaTheme::tokens().ink;
      l.important = true;
      labels.push_back(std::move(l));
    }
    for (const auto& gr : grids_) {
      KaGlTerrainViewport::CalloutLabel3D l;
      l.worldX = gr.origin.x;
      l.worldY = gr.origin.y;
      l.heightAboveGroundM = 4.0f;
      l.title = QStringLiteral("격자 %1×%2 m · %3×%4칸 · %5°").arg(gr.cellWidthM, 0, 'f', 0).arg(gr.cellHeightM, 0, 'f', 0)
                    .arg(gr.cols).arg(gr.rows).arg(gr.azimuthDeg, 0, 'f', 1);
      l.badgeColor = KaTheme::tokens().ink;
      l.important = true;
      labels.push_back(std::move(l));
    }
  }
  viewport_->setCalloutLabels(std::move(labels));
}

void MainWindow::refreshOverlay() {
  auto overlay = std::make_shared<strata3d::SceneMesh>();
  for (const auto& part : {surveyOverlay_, evalOverlay_, trenchOverlay_, drawOverlay_, selectOverlay_, sketchOverlay_})
    if (part) strata3d::appendMesh(overlay.get(), *part);
  viewport_->setOverlay(overlay);
}

void MainWindow::focusSurvey(float pitchDeg) {
  if (!survey_) {
    double x = 0, y = 0;
    if (viewport_->currentTargetWorld(&x, &y)) viewport_->focusOnWorld(x, y, viewport_->yawDegrees(), pitchDeg, viewport_->distanceM());
    return;
  }
  double x0 = 1e18, x1 = -1e18, y0 = 1e18, y1 = -1e18;
  for (const auto* set : {&survey_->surveyAreas, &survey_->trenches})
    for (const auto& ring : *set)
      for (const auto& p : ring) { x0 = std::min(x0, p.x); x1 = std::max(x1, p.x); y0 = std::min(y0, p.y); y1 = std::max(y1, p.y); }
  const double span = x1 > x0 ? std::max(x1 - x0, y1 - y0) : 300.0;
  viewport_->focusOnWorld(survey_->center.x, survey_->center.y, 0.0f, pitchDeg, float(std::max(800.0, span * 4.0)));
}

// ---- 지형 위 도구 ----------------------------------------------------------------------------

void MainWindow::setTool(Tool tool) {
  tool_ = tool;
  drawStart_.reset();
  previewLine_.reset();
  previewGrid_.reset();
  trenchA_.reset();
  using DM = KaGlTerrainViewport::DrawMode;
  switch (tool) {
    case Tool::Select: viewport_->setDrawMode(DM::None); break;
    case Tool::Measure:
    case Tool::Line: viewport_->setDrawMode(DM::UserLine); break;
    case Tool::Grid: viewport_->setDrawMode(DM::UserGrid); break;
    case Tool::Section: viewport_->setDrawMode(DM::Trench); break;
    case Tool::Sketch: viewport_->setDrawMode(DM::Sketch); break;
  }
  if (tool != Tool::Sketch && (!sketch_.empty() || sketchOverlay_)) {
    sketch_.clear();
    sketchHover_.reset();
    sketchOverlay_.reset();
    refreshOverlay();
  }
  const QString key = tool == Tool::Measure ? QStringLiteral("measure")
                      : tool == Tool::Grid  ? QStringLiteral("trench_grid")
                      : tool == Tool::Section ? QStringLiteral("section")
                                              : QStringLiteral("select");
  if (auto* a = actions_.value(key); a && !a->isChecked()) {
    const QSignalBlocker block(a);
    a->setChecked(true);
  }
  if (tool == Tool::Section && trenchInfo_) trenchInfo_->setText(QStringLiteral("시작점을 지형 위에서 누르세요."));
  if (tool == Tool::Measure) statusBar_->showMessage(QStringLiteral("측거: 지형 위 두 점을 누르세요 (Esc: 그만)"));
  if (tool == Tool::Grid) statusBar_->showMessage(QStringLiteral("시굴격자: 지형 위 두 점(격자의 한 변)을 누르세요 (Esc: 그만)"));
  rebuildDrawOverlay();
}

void MainWindow::setTrenchMode(bool on) {
  setTool(on ? Tool::Section : Tool::Select);
}

bool MainWindow::groundAt(const QVector3D& origin, const QVector3D& direction, strata3d::Pt* at, float* z) const {
  if (!lastLoad_.evaluator) return false;
  strata3d::TerrainBvh::Hit hit;
  if (!lastLoad_.evaluator->terrain().rayCast(origin, direction, 1.0e6f, &hit)) return false;  // 하늘
  const strata3d::DemGrid& g = lastLoad_.evaluator->dem();
  *at = {g.centerX() + hit.point.x(), g.centerY() + hit.point.y()};
  if (z) {
    float zz = 0;
    *z = g.sample(at->x, at->y, &zz) ? zz : hit.point.z() / std::max(zScale_, 0.01f);
  }
  return true;
}

void MainWindow::onPick(const QVector3D& origin, const QVector3D& direction) {
  strata3d::Pt p;
  if (!groundAt(origin, direction, &p)) return;
  bufferDock_->show();
  bufferDock_->raise();
  evaluateAt(p.x, p.y);
}

void MainWindow::onHoverRay(int drawMode, const QVector3D& origin, const QVector3D& direction) {
  strata3d::Pt p;
  float z = 0;
  if (!groundAt(origin, direction, &p, &z)) {
    statusBar_->clearCoordinate();
    elevLabel_->setText(QStringLiteral("표고 —"));
    return;
  }
  statusBar_->setCoordinate(p.x, p.y);
  elevLabel_->setText(QStringLiteral("표고 %1 m").arg(double(z), 0, 'f', 1));
  if (drawMode == int(KaGlTerrainViewport::DrawMode::Sketch)) {
    if (!sketch_.empty()) {
      sketchHover_ = p;
      rebuildSketchOverlay();
    }
    return;
  }
  if (!drawStart_) return;
  const strata3d::Pt a = *drawStart_;
  if (drawMode == int(KaGlTerrainViewport::DrawMode::UserLine)) {
    strata3d::UserDrawnLine3D ln;
    ln.a = a;
    ln.b = p;
    ln.widthM = 1.2f;
    ln.color = {0.08f, 0.08f, 0.07f, 1.0f};
    ln.isPreview = true;
    previewLine_ = ln;
    statusBar_->showMessage(QStringLiteral("측거 평면 %1 m — 누르면 정합니다").arg(std::hypot(p.x - a.x, p.y - a.y), 0, 'f', 1));
  } else if (drawMode == int(KaGlTerrainViewport::DrawMode::UserGrid)) {
    previewGrid_ = strata3d::BangriTerrainValidator::makeGridFromTwoPoints(a, p, 10.0, 10.0, 1, 1, 0.0, true, 0.5f, 1,
                                                                          {0.71f, 0.34f, 0.23f, 1.0f}, true);
  }
  rebuildDrawOverlay();
}

void MainWindow::onDrawPoint(int drawMode, const QVector3D& origin, const QVector3D& direction) {
  strata3d::Pt p;
  if (!groundAt(origin, direction, &p)) return;
  if (drawMode == int(KaGlTerrainViewport::DrawMode::Sketch)) {
    // 두 번 누르기는 누름 두 번 + 두 번 누르기로 온다: 바로 앞 점과 같은 자리(0.3 m 안)는 다시 넣지 않는다
    if (!sketch_.empty() && std::hypot(sketch_.back().x - p.x, sketch_.back().y - p.y) < 0.3) return;
    sketch_.push_back(p);
    if (sketchKey_ == QLatin1String("artifact_point")) {
      finishSketch();
      return;
    }
    statusBar_->showMessage(QStringLiteral("꼭짓점 %1개 — 두 번 누르거나 Enter 로 마칩니다 (Backspace 한 점 지우기, Esc 그만)").arg(sketch_.size()));
    rebuildSketchOverlay();
    return;
  }
  if (!drawStart_) {
    drawStart_ = p;
    rebuildDrawOverlay();
    return;
  }
  const strata3d::Pt a = *drawStart_;
  drawStart_.reset();
  previewLine_.reset();
  previewGrid_.reset();
  if (std::hypot(p.x - a.x, p.y - a.y) < 1.0) return;
  if (drawMode == int(KaGlTerrainViewport::DrawMode::UserLine)) {
    strata3d::UserDrawnLine3D ln;
    ln.a = a;
    ln.b = p;
    ln.widthM = 1.2f;
    ln.color = {0.08f, 0.08f, 0.07f, 1.0f};
    lines_.push_back(ln);
  } else if (drawMode == int(KaGlTerrainViewport::DrawMode::UserGrid)) {
    grids_.push_back(strata3d::BangriTerrainValidator::makeGridFromTwoPoints(a, p, 10.0, 10.0, 1, 1, 0.0, true, 0.5f, 1,
                                                                             {0.71f, 0.34f, 0.23f, 1.0f}, false));
  }
  rebuildDrawOverlay();
  rebuildLabels();
}

void MainWindow::rebuildDrawOverlay() {
  if (!lastLoad_.tree) {
    drawOverlay_.reset();
    return;
  }
  const strata3d::DemGrid& g = lastLoad_.tree->grid();
  std::vector<strata3d::UserDrawnLine3D> allLines = lines_;
  if (previewLine_) allLines.push_back(*previewLine_);
  std::vector<strata3d::UserDrawnGrid3D> allGrids = grids_;
  if (previewGrid_) allGrids.push_back(*previewGrid_);
  auto mesh = strata3d::BangriTerrainValidator::buildUserDrawingsOverlay(g, {}, allLines, allGrids);
  if (drawStart_) strata3d::appendPole(mesh.get(), g, *drawStart_, 1.5f, 12.0f, {0.08f, 0.08f, 0.07f, 1.0f});
  drawOverlay_ = mesh;
  refreshOverlay();
}

void MainWindow::keyPressEvent(QKeyEvent* e) {
  if (tool_ == Tool::Sketch) {
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) { finishSketch(); return; }
    if (e->key() == Qt::Key_Backspace && !sketch_.empty()) {
      sketch_.pop_back();
      rebuildSketchOverlay();
      return;
    }
    if (e->key() == Qt::Key_Escape) { cancelSketch(); return; }
  }
  if (e->key() == Qt::Key_Escape && tool_ != Tool::Select) {
    if (drawStart_ || trenchA_) {
      drawStart_.reset();
      trenchA_.reset();
      previewLine_.reset();
      previewGrid_.reset();
      rebuildDrawOverlay();
    } else {
      setTool(Tool::Select);
    }
    return;
  }
  if (e->key() == Qt::Key_Delete && tool_ == Tool::Select && (!lines_.empty() || !grids_.empty())) {
    lines_.clear();
    grids_.clear();
    rebuildDrawOverlay();
    rebuildLabels();
    statusBar_->showMessage(QStringLiteral("측거 선과 시굴격자를 지웠습니다."), 4000);
    return;
  }
  QMainWindow::keyPressEvent(e);
}

// ---- 조사 레이어 그리기·저장 ----------------------------------------------------------------

namespace {
QString layerTitleFor(const QString& key) {
  // Strata(LayerOps) 와 같은 한국어 이름
  if (key == QLatin1String("survey_area")) return QStringLiteral("조사구역");
  if (key == QLatin1String("feature_poly")) return QStringLiteral("유구 (면)");
  if (key == QLatin1String("feature_line")) return QStringLiteral("유구 (선)");
  if (key == QLatin1String("section_line")) return QStringLiteral("단면선");
  if (key == QLatin1String("artifact_point")) return QStringLiteral("유물");
  return key;
}
}  // namespace

void MainWindow::startSketch(const QString& layerKey) {
  if (options_.surveyGpkg.isEmpty() || !survey_) {
    QMessageBox::information(this, QStringLiteral("그리기"), QStringLiteral("먼저 조사를 열거나 「새 조사」를 만드세요. 그린 도형은 조사 파일의 레이어에 들어갑니다."));
    return;
  }
  if (!lastLoad_.tree) return;
  sketchKey_ = layerKey;
  setTool(Tool::Sketch);
  sketch_.clear();
  sketchHover_.reset();
  statusBar_->showMessage(QStringLiteral("%1: 지형 위를 눌러 꼭짓점을 찍으세요 (두 번 누르기/Enter 마침, Esc 그만)").arg(layerTitleFor(layerKey)));
}

void MainWindow::cancelSketch() {
  sketch_.clear();
  sketchHover_.reset();
  setTool(Tool::Select);
  statusBar_->showMessage(QStringLiteral("그리기를 그만두었습니다."), 4000);
}

void MainWindow::rebuildSketchOverlay() {
  if (!lastLoad_.tree) return;
  const strata3d::DemGrid& g = lastLoad_.tree->grid();
  auto m = std::make_shared<strata3d::SceneMesh>();
  const std::array<float, 4> ink{0.71f, 0.34f, 0.23f, 1.0f};
  std::vector<strata3d::Pt> line = sketch_;
  if (sketchHover_) line.push_back(*sketchHover_);
  const bool area = sketchKey_ == QLatin1String("survey_area") || sketchKey_ == QLatin1String("feature_poly");
  if (line.size() >= 2) strata3d::appendDrapedRibbon(m.get(), g, line, area && line.size() >= 3, 1.2f, ink, 1.0f);
  for (const auto& q : sketch_) strata3d::appendPole(m.get(), g, q, 1.0f, 8.0f, ink);
  sketchOverlay_ = m;
  refreshOverlay();
}

bool MainWindow::addSketchFeature(const QString& key, const std::vector<strata3d::Pt>& pts, QgsFeatureId* fidOut, QString* errorOut) {
  const bool area = key == QLatin1String("survey_area") || key == QLatin1String("feature_poly");
  const bool point = key == QLatin1String("artifact_point");
  QgsProject* proj = QgsProject::instance();
  QString err;
  QgsVectorLayer* layer = LayerOps::ensureDomainLayer(proj, options_.surveyGpkg, key, layerTitleFor(key), &err);
  if (!layer) {
    if (errorOut) *errorOut = QStringLiteral("「%1」 레이어를 준비하지 못했습니다. %2").arg(layerTitleFor(key), err);
    return false;
  }
  LayerOps::applyDomainDrawStyle(layer, key);
  // 작업 좌표계(지형) → 레이어 좌표계. 보통 같다(조사 파일의 좌표계를 따르므로).
  const QString from = lastLoad_.tree ? lastLoad_.tree->grid().crs : options_.workCrs;
  const QgsCoordinateTransform tr(QgsCoordinateReferenceSystem(from), layer->crs(), proj->transformContext());
  QgsPolylineXY xy;
  for (const auto& q : pts) {
    QgsPointXY v(q.x, q.y);
    try {
      if (tr.isValid()) v = tr.transform(v);
    } catch (const QgsCsException&) {
      if (errorOut) *errorOut = QStringLiteral("좌표를 레이어 좌표계로 옮기지 못했습니다.");
      return false;
    }
    xy << v;
  }
  QgsGeometry geom = point ? QgsGeometry::fromPointXY(xy.front())
                     : area ? QgsGeometry::fromPolygonXY({xy + QgsPolylineXY{xy.front()}})
                            : QgsGeometry::fromPolylineXY(xy);
  if (QgsWkbTypes::isMultiType(layer->wkbType())) geom.convertToMultiType();
  QgsFeature feat(layer->fields());
  feat.setGeometry(geom);
  FeatureRecord::stampNew(feat);  // Strata 와 같은 uid·만든 때
  {
    KaUndoGroup drawStep(layer, QStringLiteral("도형 그리기"));
    if (!layer->addFeature(feat)) {
      if (errorOut) *errorOut = QStringLiteral("그린 도형을 「%1」에 넣지 못했습니다. 같은 파일을 연 다른 프로그램을 닫은 뒤 다시 그리세요.").arg(layer->name());
      return false;
    }
  }
  proj->setDirty(true);
  if (fidOut) *fidOut = feat.id();
  sketchLayer_ = layer;
  return true;
}

void MainWindow::finishSketch() {
  if (tool_ != Tool::Sketch) return;
  const QString key = sketchKey_;
  const bool area = key == QLatin1String("survey_area") || key == QLatin1String("feature_poly");
  const bool point = key == QLatin1String("artifact_point");
  const size_t need = point ? 1 : area ? 3 : 2;
  if (sketch_.size() < need) {
    statusBar_->showMessage(QStringLiteral("빈 도형 (면≥3점, 선≥2점) — 이어서 그리세요"), 5000);
    return;
  }
  QgsFeatureId fid = FID_NULL;
  QString err;
  if (!addSketchFeature(key, sketch_, &fid, &err)) {
    QMessageBox::warning(this, QStringLiteral("그리기"), err);
    return;
  }
  QgsVectorLayer* layer = sketchLayer_;
  sketch_.clear();
  sketchHover_.reset();
  sketchOverlay_.reset();
  setTool(Tool::Select);
  // 이름·번호는 도형을 넣은 뒤에 묻는다(Strata 와 같은 창). Esc 로 건너뛰면 속성 칸에서 나중에 적는다.
  if (KaFeatureFormDialog::canOffer(layer)) {
    KaFeatureFormDialog form(layer, this, fid);
    if (form.exec() == QDialog::Accepted) {
      QString formError;
      if (!LayerOps::runEditCommand(layer, QStringLiteral("이름·번호"), [&]() { return form.applyTo(layer, nullptr); }, &formError))
        QMessageBox::warning(this, QStringLiteral("속성"), QStringLiteral("이름·번호를 넣지 못했습니다. 도형은 그대로 있습니다.\n%1").arg(formError));
    }
  }
  afterSketchAdded(layer, fid, key);
}

void MainWindow::afterSketchAdded(QgsVectorLayer* layer, QgsFeatureId fid, const QString& key) {
  heritageCard_->clearRecord();
  card_->setFeature(layer, fid);
  card_->setVisible(card_->hasFeature());
  inspector_->setSelectionCount(1);
  inspectorDock_->show();
  refreshSurveyFromProject();
  refreshUnsavedChip();
  statusBar_->showMessage(QStringLiteral("「%1」에 넣었습니다(아직 저장 안 됨, Ctrl+S).").arg(layer->name()), 8000);
  if (key == QLatin1String("survey_area")) computeSurveyRelations();  // 조사구역이 바뀌면 구역 판정·관계 표도
}

void MainWindow::refreshSurveyFromProject() {
  if (!survey_) return;
  strata3d::StrataSurvey s;
  if (!surveyFromProject(QgsProject::instance(), options_.surveyGpkg, survey_->workCrs, &s)) return;
  if (s.empty()) s.center = survey_->center;  // 아직 도형이 없으면 가운데는 그대로
  survey_ = std::move(s);
  applyLayerVisibility();
}

bool MainWindow::hasUnsavedSurveyWork() const {
  return EditBufferSummary::summarize(QgsProject::instance()).features > 0;
}

void MainWindow::refreshUnsavedChip() {
  const EditBufferSummary::Summary sum = EditBufferSummary::summarize(QgsProject::instance());
  statusBar_->setUnsavedCount(sum.features, false);
  setWindowModified(sum.features > 0);
}

bool MainWindow::saveSurveyWork() {
  QgsProject* proj = QgsProject::instance();
  if (options_.surveyGpkg.isEmpty() || !proj || !projectError_.isEmpty()) return true;  // 레이어 칸에 못 연 조사는 3D 설정만
  if (!hasUnsavedSurveyWork() && !proj->isDirty()) return true;
  const QString path = options_.surveyGpkg;
  // 이 세션이 연 뒤에 다른 곳(Strata 등)에서 저장했으면 알린다. 파일을 잠그지 않는다(Strata 와 같은 규칙).
  if (QString elsewhere; SurveyFileFingerprint::replacedElsewhere(path, &elsewhere)) {
    const auto answer = QMessageBox::warning(this, QStringLiteral("다른 곳에서 저장된 조사 파일"),
                                             elsewhere + QStringLiteral("\n\n그래도 이 파일에 저장할까요? (Strata 에서 같은 조사를 열어 두었다면 먼저 닫으세요)"),
                                             QMessageBox::Save | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Save) return false;
  }
  SurveySession::PersistInput in;
  in.surveyPath = path;
  in.fallbackDirectory = QFileInfo(path).absolutePath();
  in.recoveryDirectory = SurveySession::recoveryDirectoryFor(SurveyStorage::writableSurveyPath(path, in.fallbackDirectory));
  const SurveySession::PersistResult r = SurveySession::persistWork(proj, in);
  if (!r.saved) {
    lastSaveError_ = r.workspace.error.isEmpty() ? QStringLiteral("저장을 마치지 못했습니다. 저장하지 않은 편집은 그대로 있습니다.") : r.workspace.error;
    if (!quietSave_)
      QMessageBox::warning(this, QStringLiteral("저장 실패"), lastSaveError_ + QStringLiteral("\n") + SurveySession::originalStateNote(r.workspace));
    proj->setDirty(true);
    return false;
  }
  lastSaveError_.clear();
  if (!r.surveyPath.isEmpty() && r.surveyPath != options_.surveyGpkg) options_.surveyGpkg = r.surveyPath;
  SurveyFileFingerprint::remember(options_.surveyGpkg);
  refreshUnsavedChip();
  return true;
}

bool MainWindow::createSurveyAt(const QString& dir, const QString& name, const QString& workCrs, QString* errorOut) {
  QString err;
  const QString path = SurveyProjectFactory::createNewSurvey(dir, name, &err, workCrs);
  if (path.isEmpty()) {
    if (errorOut) *errorOut = err;
    return false;
  }
  double cx = 0, cy = 0;
  if (!viewport_->currentTargetWorld(&cx, &cy) && lastLoad_.tree) {
    cx = lastLoad_.tree->grid().centerX();
    cy = lastLoad_.tree->grid().centerY();
  }
  QgsProject* proj = QgsProject::instance();
  proj->clear();
  proj->setCrs(QgsCoordinateReferenceSystem(workCrs));
  proj->setFileName(path);
  projectError_.clear();
  SurveyFileFingerprint::remember(path);
  strata3d::StrataSurvey s;
  s.path = path;
  s.workCrs = workCrs;
  s.center = {cx, cy};
  survey_ = std::move(s);
  surveyOverlay_.reset();
  lastRelations_.reset();
  options_.surveyGpkg = path;
  setWindowTitle(QStringLiteral("%1[*] — Strata 3D").arg(QFileInfo(path).fileName()));
  // 첫 저장: 빈 조사도 프로젝트를 담아 둔다(Strata 가 새 조사를 만든 뒤 저장하는 것과 같이)
  proj->setDirty(true);
  if (!saveSurveyWork()) {
    if (errorOut) *errorOut = lastSaveError_;
    return false;
  }
  if (lastLoad_.tree && lastLoad_.tree->grid().crs != workCrs) {
    // 원점이 바뀌면 지형을 새 좌표계로 다시 받는다(가운데는 그대로)
    double lo = cx, la = cy;
    if (strata3d::transformPoint(lastLoad_.tree->grid().crs, QStringLiteral("EPSG:4326"), &lo, &la)) {
      options_.lon = lo;
      options_.lat = la;
    }
    double nx = lo, ny = la;
    if (strata3d::transformPoint(QStringLiteral("EPSG:4326"), workCrs, &nx, &ny)) survey_->center = {nx, ny};
    options_.workCrs = workCrs;
    loadScene();
  }
  options_.workCrs = workCrs;
  return true;
}

void MainWindow::newSurvey() {
  if (hasUnsavedSurveyWork()) {
    const auto answer = QMessageBox::question(this, QStringLiteral("새 조사"), QStringLiteral("현재 조사에 저장하지 않은 작업이 있습니다. 새 조사를 만들기 전에 저장할까요?"),
                                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Cancel || (answer == QMessageBox::Save && !saveSurveyWork())) return;
  }
  // 지금 화면 가운데의 경도로 원점(5186/5187)을 권한다(Strata 와 같은 창)
  double lon = std::numeric_limits<double>::quiet_NaN();
  double cx = 0, cy = 0;
  if (lastLoad_.tree && viewport_->currentTargetWorld(&cx, &cy)) {
    double la = cy;
    lon = cx;
    if (!strata3d::transformPoint(lastLoad_.tree->grid().crs, QStringLiteral("EPSG:4326"), &lon, &la)) lon = std::numeric_limits<double>::quiet_NaN();
  }
  KaNewSurveyDialog dlg(options_.workCrs, lon, this);
  if (dlg.exec() != QDialog::Accepted) return;
  const QString name = dlg.surveyName();
  if (!KaNewSurveyDialog::nameProblem(name).isEmpty()) return;
  const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("저장 폴더"),
                                                        options_.surveyGpkg.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                                                                                      : QFileInfo(options_.surveyGpkg).absolutePath());
  if (dir.isEmpty()) return;
  QString err;
  if (!createSurveyAt(dir, name, dlg.workCrs(), &err)) {
    QMessageBox::warning(this, QStringLiteral("새 조사"), QStringLiteral("새 조사를 만들지 못했습니다.\n") + err);
    return;
  }
  if (lastLoad_.tree && lastLoad_.tree->grid().crs == dlg.workCrs()) startSketch(QStringLiteral("survey_area"));
  else statusBar_->showMessage(QStringLiteral("새 조사 %1 — 지형을 %2 로 다시 받은 뒤 「그리기 › 조사구역 그리기」로 그리세요.").arg(QFileInfo(options_.surveyGpkg).fileName(), dlg.workCrs()), 10000);
}

void MainWindow::closeEvent(QCloseEvent* e) {
  if (hasUnsavedSurveyWork() && !quietSave_) {
    const auto answer = QMessageBox::question(this, QStringLiteral("닫기"), QStringLiteral("저장하지 않은 도형·기록이 있습니다. 저장할까요?"),
                                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Cancel || (answer == QMessageBox::Save && !saveSurveyWork())) {
      e->ignore();
      return;
    }
  }
  QMainWindow::closeEvent(e);
}

// ---- 단면 ------------------------------------------------------------------------------------

void MainWindow::onTrenchPoint(const QVector3D& origin, const QVector3D& direction) {
  strata3d::Pt p;
  if (!groundAt(origin, direction, &p) || !lastLoad_.evaluator) return;
  const strata3d::DemGrid& g = lastLoad_.evaluator->dem();
  if (!trenchA_) {
    trenchA_ = p;
    trenchProfile_.clear();
    plot_->clear();
    auto m = std::make_shared<strata3d::SceneMesh>();
    strata3d::appendPole(m.get(), g, p, 2.0f, 20.0f, {0.71f, 0.34f, 0.23f, 1});
    trenchOverlay_ = m;
    refreshOverlay();
    trenchInfo_->setText(QStringLiteral("시작점 X %1, Y %2 — 끝점을 누르세요.").arg(p.x, 0, 'f', 1).arg(p.y, 0, 'f', 1));
    return;
  }
  const strata3d::Pt a = *trenchA_;
  trenchA_.reset();
  setTrench(a.x, a.y, p.x, p.y);
}

void MainWindow::clearTrench() {
  trenchA_.reset();
  trenchProfile_.clear();
  trenchOverlay_.reset();
  trenchSummary_.clear();
  if (plot_) plot_->clear();
  if (trenchInfo_) trenchInfo_->setText(QStringLiteral("단면 없음. 「단면」을 켜고 지형 위 두 점을 누르세요."));
  if (viewport_) refreshOverlay();
  emit trenchChanged(QString());
}

void MainWindow::setTrench(double ax, double ay, double bx, double by) {
  if (!lastLoad_.evaluator) return;
  const strata3d::DemGrid& g = lastLoad_.evaluator->dem();
  const double len = std::hypot(bx - ax, by - ay);
  if (len < 1.0) {
    trenchInfo_->setText(QStringLiteral("두 점이 너무 가깝습니다(1 m 미만). 다시 누르세요."));
    return;
  }
  const double step = std::clamp(len / 80.0, 1.0, g.pixelSize * 0.5);
  trenchProfile_ = lastLoad_.evaluator->terrainProfile({ax, ay}, {bx, by}, step);
  if (trenchProfile_.size() < 2) {
    trenchInfo_->setText(QStringLiteral("단면 선이 지형 밖입니다."));
    trenchProfile_.clear();
    return;
  }
  double zMin = trenchProfile_.front().elevationM, zMax = zMin, steepest = 0;
  for (size_t i = 0; i < trenchProfile_.size(); ++i) {
    zMin = std::min(zMin, trenchProfile_[i].elevationM);
    zMax = std::max(zMax, trenchProfile_[i].elevationM);
    if (i > 0) {
      const double dd = trenchProfile_[i].distanceM - trenchProfile_[i - 1].distanceM;
      if (dd > 0) steepest = std::max(steepest, std::abs(trenchProfile_[i].elevationM - trenchProfile_[i - 1].elevationM) / dd);
    }
  }
  double bearing = qRadiansToDegrees(std::atan2(bx - ax, by - ay));  // 북 기준 시계방향
  if (bearing < 0) bearing += 360.0;
  const double mean = (trenchProfile_.back().elevationM - trenchProfile_.front().elevationM) / trenchProfile_.back().distanceM;
  trenchSummary_ = QStringLiteral("단면 길이 %1 m · 방위 %2° · 시점 %3 m → 종점 %4 m(평균 기울기 %5%) · 최고 %6 m · 최저 %7 m · 국지 최대 기울기 %8%")
                       .arg(trenchProfile_.back().distanceM, 0, 'f', 1).arg(bearing, 0, 'f', 0)
                       .arg(trenchProfile_.front().elevationM, 0, 'f', 1).arg(trenchProfile_.back().elevationM, 0, 'f', 1)
                       .arg(mean * 100, 0, 'f', 1).arg(zMax, 0, 'f', 1).arg(zMin, 0, 'f', 1).arg(steepest * 100, 0, 'f', 1);
  trenchInfo_->setText(trenchSummary_ + QStringLiteral("\n현재 지표면(Copernicus GLO-30, %1 m 격자)의 단면이며 층위가 아닙니다. 수 m 규모의 트렌치는 격자 해상도 밖입니다.").arg(g.pixelSize, 0, 'f', 0));
  plot_->setProfile(trenchProfile_);
  auto m = std::make_shared<strata3d::SceneMesh>();
  strata3d::appendMesh(m.get(), *strata3d::buildProfileOverlay(g, trenchProfile_, 4.0f));
  strata3d::appendPole(m.get(), g, {ax, ay}, 2.0f, 20.0f, {0.71f, 0.34f, 0.23f, 1});
  strata3d::appendPole(m.get(), g, {bx, by}, 2.0f, 20.0f, {0.71f, 0.34f, 0.23f, 1});
  trenchOverlay_ = m;
  refreshOverlay();
  profileDock_->show();
  profileDock_->raise();
  emit trenchChanged(trenchSummary_);
}

// ---- 평가·관계 -------------------------------------------------------------------------------

void MainWindow::evaluateAt(double x, double y) {
  if (!lastLoad_.evaluator) {
    emit evaluationFinished(false, QStringLiteral("지형이 아직 없습니다."));
    return;
  }
  report_->setHtml(QStringLiteral("<p>계산 중…</p>"));
  const auto ev = lastLoad_.evaluator;
  evalWatcher_.setFuture(QtConcurrent::run([ev, x, y] { return ev->evaluate({x, y}); }));
}

void MainWindow::showEvaluation() {
  lastEvaluation_ = evalWatcher_.result();
  hasEvaluation_ = true;
  report_->setHtml(evaluationHtml(lastEvaluation_));
  evalOverlay_ = strata3d::buildEvaluationOverlay(lastLoad_.evaluator->dem(), lastEvaluation_);
  refreshOverlay();
  emit evaluationFinished(true, lastEvaluation_.summary);
}

void MainWindow::evaluateSurvey() {
  if (!survey_) {
    emit evaluationFinished(false, QStringLiteral("열린 조사가 없습니다."));
    return;
  }
  if (!lastLoad_.evaluator) {
    emit evaluationFinished(false, QStringLiteral("지형이 아직 없습니다."));
    return;
  }
  const std::vector<strata3d::Pt>* ring = nullptr;
  for (const auto* set : {&survey_->surveyAreas, &survey_->trenches, &survey_->featurePolys})
    if (!set->empty()) { ring = &set->front(); break; }
  strata3d::Pt site = survey_->center;
  std::vector<strata3d::Pt> poly;
  if (ring && ring->size() >= 3) {
    poly = *ring;
    double sx = 0, sy = 0;
    for (const auto& p : poly) { sx += p.x; sy += p.y; }
    site = {sx / double(poly.size()), sy / double(poly.size())};
  }
  report_->setHtml(QStringLiteral("<p>조사구역 버퍼 계산 중…</p>"));
  const auto ev = lastLoad_.evaluator;
  evalWatcher_.setFuture(QtConcurrent::run([ev, site, poly] { return ev->evaluate(site, poly); }));
}

void MainWindow::computeSurveyRelations() {
  if (!survey_ || !lastLoad_.evaluator) {
    relationPanel_->clear(QStringLiteral("조사와 지형이 모두 있어야 관계를 계산합니다. 「열기」로 Strata 조사를 여세요."));
    emit relationsFinished(false, QStringLiteral("조사 또는 지형 없음"));
    return;
  }
  const std::vector<strata3d::Pt>* ring = nullptr;
  for (const auto* set : {&survey_->surveyAreas, &survey_->trenches})
    if (!set->empty()) { ring = &set->front(); break; }
  std::vector<strata3d::Pt> poly = ring ? *ring : std::vector<strata3d::Pt>{};
  strata3d::Pt site = survey_->center;
  if (!poly.empty()) {  // 면이면 꼭짓점 평균, 선(트렌치가 선으로 그려진 조사)이면 선의 가운데
    double sx = 0, sy = 0;
    for (const auto& p : poly) { sx += p.x; sy += p.y; }
    site = {sx / double(poly.size()), sy / double(poly.size())};
  }
  if (poly.size() < 3) poly.clear();  // 두 점 선 트렌치는 가운데 점에서 잰다(면이 아니므로)
  relationSiteLabel_ = QFileInfo(survey_->path).completeBaseName() + (!survey_->surveyAreas.empty() ? QStringLiteral(" 조사구역") : QStringLiteral(" 첫 트렌치"));
  relationPanel_->setBusy(QStringLiteral("관계를 계산하는 중입니다…"));
  const auto ev = lastLoad_.evaluator;
  const QString cacheDir = s3dCacheDir() + QStringLiteral("/vector");
  relWatcher_.setFuture(QtConcurrent::run([ev, site, poly, cacheDir] {
    // 시대: 국보(11)·보물(12)·사적(13) 지정 목록을 합친다(7일 캐시). 못 받으면 시대 열만 비운다.
    strata3d::SpcaList periods;
    for (const char* kd : {"11", "12", "13"}) {
      strata3d::SpcaList one;
      if (strata3d::loadSpca(QString::fromLatin1(kd), cacheDir, &one, nullptr, QStringLiteral("경주"))) {
        periods.records.insert(periods.records.end(), one.records.begin(), one.records.end());
        periods.fetchedAt = one.fetchedAt;
      }
    }
    strata3d::RelationOptions o;
    o.radiusM = 1000;
    if (!periods.records.empty()) o.periods = &periods;
    return strata3d::computeRelations(*ev, site, poly, o);
  }));
}

// ---- 조사 열기·3D 설정 저장 ------------------------------------------------------------------

void MainWindow::openSurveyDialog() {
  const QString start = options_.surveyGpkg.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) : QFileInfo(options_.surveyGpkg).absolutePath();
  const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Strata 조사 열기"), start, QStringLiteral("Strata 조사 (*.gpkg)"));
  if (!path.isEmpty()) openSurvey(path);
}

void MainWindow::openSurvey(const QString& gpkgPath) {
  strata3d::StrataSurvey s;
  QString err;
  if (!strata3d::loadStrataSurvey(gpkgPath, QStringLiteral("auto"), &s, &err)) {
    emit surveyOpened(false, err);
    return;
  }
  double lon = s.center.x, lat = s.center.y;
  if (!strata3d::transformPoint(s.workCrs, QStringLiteral("EPSG:4326"), &lon, &lat, &err)) {
    emit surveyOpened(false, err);
    return;
  }
  survey_ = std::move(s);
  surveyOverlay_.reset();
  // 레이어 칸: Strata 가 조사 파일에 저장한 프로젝트를 Strata 코드 그대로 연다(읽기만, 쓰지 않는다).
  {
    QgsProject* proj = QgsProject::instance();
    proj->clear();
    bool crashed = false;
    projectError_.clear();
    if (!SurveyStorage::readEmbedded(proj, gpkgPath, &crashed, &projectError_, false) && projectError_.isEmpty())
      projectError_ = QStringLiteral("조사 프로젝트를 읽지 못했습니다.");
    if (!projectError_.isEmpty()) {
      statusBar_->showMessage(QStringLiteral("레이어 칸: ") + projectError_, 10000);
    } else {
      SurveyFileFingerprint::remember(gpkgPath);  // 이 세션이 연 파일. 저장 전에 다른 곳에서 바뀌었는지 본다(Strata 와 같음)
      strata3d::StrataSurvey fromProject;
      if (surveyFromProject(proj, gpkgPath, survey_->workCrs, &fromProject) && !fromProject.empty()) survey_ = std::move(fromProject);
    }
  }
  hasEvaluation_ = false;
  options_.workCrs = survey_->workCrs;
  options_.surveyGpkg = gpkgPath;
  setWindowTitle(QStringLiteral("%1[*] — Strata 3D").arg(QFileInfo(gpkgPath).fileName()));
  focusSurvey(50.0f);  // 조사를 열면 조사구역을 화면 가운데에(지형이 올라오면 적용된다)
  restoreViewPending_ = QFileInfo::exists(viewStatePath());
  options_.lon = lon;
  options_.lat = lat;
  surveyPending_ = true;
  loadScene();
}

QString MainWindow::viewStatePath() const {
  // 3D 설정은 조사 파일 옆 파일에 둔다. 조사 .gpkg 는 Strata 가 쓰는 파일이라 건드리지 않는다(설계도 v3 7장).
  if (options_.surveyGpkg.isEmpty()) return QString();
  const QFileInfo fi(options_.surveyGpkg);
  return fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral(".s3d.json");
}

bool MainWindow::saveViewState(const QString& path) {
  double x = 0, y = 0;
  if (!viewport_->currentTargetWorld(&x, &y)) return false;
  QJsonObject cam{{"target_x", x}, {"target_y", y}, {"yaw", double(viewport_->yawDegrees())},
                  {"pitch", double(viewport_->pitchDegrees())}, {"distance", double(viewport_->distanceM())}};
  QJsonObject o{{"format", "strata-3d-view-v1"},
                {"crs", lastLoad_.tree ? lastLoad_.tree->grid().crs : options_.workCrs},
                {"camera", cam},
                {"z_scale", double(zScale_)},
                {"terrain_mode", mode_},
                {"label_scale", double(viewport_->hudFontScale())},
                {"labels", viewport_->showCalloutLabels()},
                {"tier_d", showSchematic_},
                {"saved", QDateTime::currentDateTime().toString(Qt::ISODate)}};
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
  return f.write(QJsonDocument(o).toJson(QJsonDocument::Indented)) > 0;
}

bool MainWindow::loadViewState(const QString& path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) return false;
  const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
  if (o.value(QStringLiteral("format")).toString() != QLatin1String("strata-3d-view-v1")) return false;
  setZScale(float(o.value(QStringLiteral("z_scale")).toDouble(1.0)));
  const double ls = o.value(QStringLiteral("label_scale")).toDouble(0.85);
  viewport_->setHudFontScale(float(ls));
  viewport_->setHudBoxScale(float(ls));
  viewport_->setShowCalloutLabels(o.value(QStringLiteral("labels")).toBool(true));
  setSchematicModelsShown(o.value(QStringLiteral("tier_d")).toBool(false));
  if (o.contains(QStringLiteral("terrain_mode"))) setTerrainMode(o.value(QStringLiteral("terrain_mode")).toInt());
  const QJsonObject cam = o.value(QStringLiteral("camera")).toObject();
  if (!cam.isEmpty())
    viewport_->focusOnWorld(cam.value(QStringLiteral("target_x")).toDouble(), cam.value(QStringLiteral("target_y")).toDouble(),
                            float(cam.value(QStringLiteral("yaw")).toDouble()), float(cam.value(QStringLiteral("pitch")).toDouble()),
                            float(cam.value(QStringLiteral("distance")).toDouble()));
  return true;
}

// ---- 내보내기 --------------------------------------------------------------------------------

bool MainWindow::exportSheet(const QString& pngPath, const QString& pdfPath) {
  if (!viewport_->hasTerrain()) return false;
  constexpr int kSize = 3072;
  const QImage view = viewport_->renderToImage(kSize, kSize);
  if (view.isNull()) return false;
  const strata3d::DemGrid& g = lastLoad_.tree->grid();
  SheetInfo info;
  info.title = survey_ ? QStringLiteral("%1 · 입체 입지도").arg(QFileInfo(survey_->path).completeBaseName()) : QStringLiteral("경주 · 입체 입지도");
  info.visibleWidthM = viewport_->visibleWidthAtTargetM(1.0f);
  info.yawDegFromNorth = std::fmod(360.0 - double(viewport_->yawDegrees()), 360.0);
  info.crsLabel = crsLabelFor(g.crs);
  info.demName = QStringLiteral("Copernicus GLO-30 (%1 m 격자로 재배열)").arg(g.pixelSize, 0, 'f', 0);
  info.imageryName = imageryName_;
  info.zMin = g.zMin;
  info.zMax = g.zMax;
  info.elevationRamp = (mode_ == 1);
  info.generated = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
  info.hasTrench = !trenchSummary_.isEmpty();
  info.lines << QStringLiteral("높이 ×%1 · 시점 고도 %2°").arg(double(zScale_), 0, 'f', 1).arg(double(viewport_->pitchDegrees()), 0, 'f', 0);
  if (!trenchSummary_.isEmpty()) info.lines << trenchSummary_ + QStringLiteral(" (지표면 단면)");
  if (hasEvaluation_) {
    info.lines << lastEvaluation_.summary;
    for (const strata3d::RadiusResult& rr : lastEvaluation_.radii) {
      int n = 0;
      for (const strata3d::NearbyItem& it : rr.heritage) {
        if (++n > 4) break;
        info.lines << QStringLiteral("· %1 m 안 — %2 (%3) %4 m %5").arg(rr.radiusM, 0, 'f', 0).arg(strata3d::siteNameOnly(it.name), it.category).arg(it.distanceM, 0, 'f', 0).arg(it.bearingName);
      }
    }
    info.lines << lastEvaluation_.notes;
  }
  const QImage sheet = composeSheet(view, info);
  bool ok = sheet.save(pngPath);
  if (ok && !pdfPath.isEmpty()) ok = saveSheetPdf(sheet, pdfPath, info.title);
  return ok;
}

void MainWindow::saveImageDialog() {
  if (!viewport_->hasTerrain()) return;
  const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("3D 화면 그림 저장"), QStringLiteral("strata3d.png"), QStringLiteral("PNG (*.png)"));
  if (path.isEmpty()) return;
  const qreal dpr = devicePixelRatioF();
  const QImage img = viewport_->renderToImage(int(viewport_->width() * dpr * 2), int(viewport_->height() * dpr * 2));
  statusBar_->showMessage(!img.isNull() && img.save(path) ? QStringLiteral("그림을 저장했습니다: ") + path : QStringLiteral("그림을 저장하지 못했습니다."), 8000);
}

void MainWindow::printSheet() {
  if (!viewport_->hasTerrain()) return;
  const QString tmp = QDir::temp().filePath(QStringLiteral("strata3d-print.png"));
  if (!exportSheet(tmp)) return;
  const QImage sheet(tmp);
  QPrinter printer(QPrinter::HighResolution);
  printer.setPageOrientation(sheet.width() >= sheet.height() ? QPageLayout::Landscape : QPageLayout::Portrait);
  QPrintDialog dlg(&printer, this);
  if (dlg.exec() != QDialog::Accepted) return;
  QPainter p(&printer);
  const QRect page = p.viewport();
  const QSize sz = sheet.size().scaled(page.size(), Qt::KeepAspectRatio);  // 비율을 지킨다
  p.setViewport(page.x() + (page.width() - sz.width()) / 2, page.y() + (page.height() - sz.height()) / 2, sz.width(), sz.height());
  p.setWindow(sheet.rect());
  p.drawImage(0, 0, sheet);
}

bool MainWindow::selectByName(const QString& name) {
  if (!lastLoad_.features) return false;
  // Strata 조사 레이어의 지물을 먼저(속성 칸에 기록이 열리는 것)
  for (int pass = 0; pass < 2; ++pass)
    for (const strata3d::GeoFeature& f : *lastLoad_.features)
      if ((pass == 1 || f.id.startsWith(QLatin1String("shp:"))) && strata3d::siteNameOnly(f.name) == name) {
        selectFeature(f.id);
        return true;
      }
  return false;
}

bool MainWindow::inspectorShowsRecord() const {
  return (card_ && card_->hasFeature()) || (heritageCard_ && heritageCard_->isVisibleTo(inspector_));
}

void MainWindow::reloadKeepingView() {
  double x = 0, y = 0;
  if (viewport_->currentTargetWorld(&x, &y))
    viewport_->focusOnWorld(x, y, viewport_->yawDegrees(), viewport_->pitchDegrees(), viewport_->distanceM());
  loadScene();
}

MainWindow::~MainWindow() {
  ++qgisDrapeGeneration_;  // 끝난 QGIS 그리기가 더는 창을 건드리지 않게
  for (QgsMapRendererJob* job : findChildren<QgsMapRendererJob*>()) {
    disconnect(job, nullptr, this, nullptr);
    job->cancelWithoutBlocking();
  }
  watcher_.waitForFinished();
  satWatcher_.waitForFinished();
  evalWatcher_.waitForFinished();
  relWatcher_.waitForFinished();
  drapeWatcher_.waitForFinished();
  QThreadPool::globalInstance()->waitForDone();
}
