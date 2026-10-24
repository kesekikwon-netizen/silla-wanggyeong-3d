#include "MainWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTextBrowser>
#include <QToolBar>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <algorithm>
#include <cmath>

#include "CrsTransform.h"
#include "DemProvider.h"
#include "EvaluationOverlay.h"
#include "KaGlTerrainViewport.h"
#include "KaTheme.h"
#include "KhsProvider.h"
#include "LocalVectorProvider.h"
#include "OsmProvider.h"
#include "ProfilePlot.h"
#include "SatelliteProvider.h"
#include "SheetComposer.h"

namespace {
QString crsLabelFor(const QString& crs) {
  if (crs == QLatin1String("EPSG:5186")) return QStringLiteral("EPSG:5186 (KGD2002 중부원점)");
  if (crs == QLatin1String("EPSG:5187")) return QStringLiteral("EPSG:5187 (KGD2002 동부원점)");
  return crs;
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
  req.cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/dem");
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
    fr.cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/vector");
    QString err;
    // 한 곳이 실패해도 나머지로 계속한다. 실패 사유는 화면에 그대로 알린다.
    if (!strata3d::loadOsmFeatures(fr, all.get(), &err)) out.warnings << err;
    if (!strata3d::loadKhsHeritageNearby(fr, all.get(), &err)) out.warnings << err;
    else if (!err.isEmpty()) out.warnings << QStringLiteral("국가유산청 일부 시도 실패: ") + err;
    QStringList skipped;
    strata3d::loadLocalVectors(fr, strata3d::strataHeritageShpPaths(), all.get(), &skipped);
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
  sr.cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/tiles");
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

QString html(const QColor& c) { return c.name(); }

QString evaluationHtml(const strata3d::Evaluation& ev) {
  const KaTheme::Tokens& t = KaTheme::tokens();
  QString h = QStringLiteral("<body style='color:%1'>").arg(html(t.ink));
  h += QStringLiteral("<h3>고고학 3D 평가</h3><p>조사지 EPSG:5187 X %1, Y %2</p>").arg(ev.site.x, 0, 'f', 1).arg(ev.site.y, 0, 'f', 1);
  const auto& tm = ev.terrain;
  h += QStringLiteral("<p><b>지형</b> 고도 %1 m · 경사 %2° · 향 %3<br>TPI 소 %4 m(%5σ) · 대 %6 m(%7σ)<br>분류(참고): %8</p>")
           .arg(tm.elevationM, 0, 'f', 1).arg(tm.slopeDeg, 0, 'f', 1).arg(tm.aspectName)
           .arg(tm.tpiSmallM, 0, 'f', 1).arg(tm.zSmall, 0, 'f', 2).arg(tm.tpiLargeM, 0, 'f', 1).arg(tm.zLarge, 0, 'f', 2).arg(tm.landform);
  for (const strata3d::RadiusResult& rr : ev.radii) {
    h += QStringLiteral("<p><b>반경 %1 m</b> (둥근 버퍼, 넓이 %2 ㎡) — 지정·등록 유산·유적 %3건, 건물 %4동</p>")
             .arg(rr.radiusM, 0, 'f', 0).arg(rr.bufferAreaM2, 0, 'f', 0).arg(int(rr.heritage.size())).arg(rr.buildings);
    h += QStringLiteral("<table cellspacing=0 cellpadding=3 border=0>");
    int n = 0;
    for (const strata3d::NearbyItem& it : rr.heritage) {
      if (++n > 12) break;
      h += QStringLiteral("<tr><td>%1</td><td>%2</td><td align=right>%3 m</td><td>%4</td><td align=right>%5 m</td></tr>")
               .arg(it.name.isEmpty() ? QStringLiteral("(이름 없음)") : it.name.toHtmlEscaped(), it.category.toHtmlEscaped())
               .arg(it.distanceM, 0, 'f', 0).arg(it.bearingName).arg(it.deltaElevM, 0, 'f', 1);
    }
    h += QStringLiteral("</table>");
    if (int(rr.heritage.size()) > 12) h += QStringLiteral("<p>…그 밖에 %1건</p>").arg(int(rr.heritage.size()) - 12);
  }
  if (!ev.visibility.empty()) {
    h += QStringLiteral("<p><b>시선(지형만)</b></p><table cellspacing=0 cellpadding=3>");
    for (const strata3d::VisibilityItem& v : ev.visibility)
      h += QStringLiteral("<tr><td>%1</td><td align=right>%2 m</td><td>%3</td></tr>").arg(v.name.toHtmlEscaped()).arg(v.distanceM, 0, 'f', 0)
               .arg(v.visible ? QStringLiteral("보임") : QStringLiteral("지형에 가림(%1 m)").arg(v.blockedAtM, 0, 'f', 0));
    h += QStringLiteral("</table>");
  }
  h += QStringLiteral("<p style='color:%1'>").arg(html(t.inkMuted));
  for (const QString& n : ev.notes) h += n.toHtmlEscaped() + QStringLiteral("<br>");
  h += QStringLiteral("</p></body>");
  return h;
}
}  // namespace

MainWindow::MainWindow(const Options& options, QWidget* parent) : QMainWindow(parent), options_(options) {
  setWindowTitle(QStringLiteral("Strata 3D · 경주 실측 지형"));
  resize(1440, 860);
  viewport_ = new KaGlTerrainViewport(this);
  setCentralWidget(viewport_);

  auto* bar = addToolBar(QStringLiteral("보기"));
  bar->setMovable(false);
  auto* mode = new QComboBox(bar);
  mode->addItems({QStringLiteral("점토 (기복 판독)"), QStringLiteral("표고 색띠"), QStringLiteral("경사도"), QStringLiteral("위성 실사")});
  bar->addWidget(mode);
  connect(mode, &QComboBox::currentIndexChanged, this, [this](int i) {
    mode_ = i;
    viewport_->setMode(KaGlTerrainViewport::Mode(i));
  });
  connect(this, &MainWindow::satelliteFinished, mode, [mode](bool ok, const QString&) {
    if (ok) mode->setCurrentIndex(3);
  });
  auto* zs = new QDoubleSpinBox(bar);
  zs->setRange(1.0, 5.0);
  zs->setSingleStep(0.5);
  zs->setValue(1.0);
  zs->setPrefix(QStringLiteral("높이 ×"));
  bar->addWidget(zs);
  connect(zs, &QDoubleSpinBox::valueChanged, this, [this](double v) { viewport_->setZScale(float(v)); });
  auto* showScene = new QCheckBox(QStringLiteral("건물·유적"), bar);
  showScene->setChecked(true);
  bar->addWidget(showScene);
  connect(showScene, &QCheckBox::toggled, this, [this](bool on) { viewport_->setSceneVisible(on); });
  auto* sheet = new QPushButton(QStringLiteral("도면 저장…"), bar);
  bar->addWidget(sheet);
  connect(sheet, &QPushButton::clicked, this, [this] {
    const QString png = QFileDialog::getSaveFileName(this, QStringLiteral("입체지형 도면 저장"), QStringLiteral("strata3d-도면.png"), QStringLiteral("PNG (*.png)"));
    if (png.isEmpty()) return;
    exportSheet(png, QString(png).replace(QStringLiteral(".png"), QStringLiteral(".pdf")));
  });
  auto* reload = new QPushButton(QStringLiteral("다시 불러오기"), bar);
  bar->addWidget(reload);
  connect(reload, &QPushButton::clicked, this, &MainWindow::loadScene);
  bar->addSeparator();
  trenchBtn_ = new QPushButton(QStringLiteral("시굴 트렌치 그리기"), bar);
  trenchBtn_->setCheckable(true);
  trenchBtn_->setToolTip(QStringLiteral("켜고 지형 위 두 점을 차례로 누릅니다(Esc: 취소). 끌면 카메라가 돕니다."));
  bar->addWidget(trenchBtn_);
  connect(trenchBtn_, &QPushButton::toggled, this, &MainWindow::setTrenchMode);
  auto* clearTrenchBtn = new QPushButton(QStringLiteral("트렌치 지우기"), bar);
  bar->addWidget(clearTrenchBtn);
  connect(clearTrenchBtn, &QPushButton::clicked, this, &MainWindow::clearTrench);

  auto* dock = new QDockWidget(QStringLiteral("고고학 3D 평가"), this);
  report_ = new QTextBrowser(dock);
  report_->setMinimumWidth(380);
  report_->setHtml(QStringLiteral("<p>지형을 <b>더블클릭</b>하면 그 지점을 500 m·1000 m 반경으로 평가합니다.</p>"));
  dock->setWidget(report_);
  addDockWidget(Qt::RightDockWidgetArea, dock);

  auto* profileDock = new QDockWidget(QStringLiteral("시굴 트렌치 단면"), this);
  auto* profileBox = new QWidget(profileDock);
  auto* profileLayout = new QVBoxLayout(profileBox);
  profileLayout->setContentsMargins(6, 6, 6, 6);
  trenchInfo_ = new QLabel(QStringLiteral("트렌치 없음. 「시굴 트렌치 그리기」를 켜고 지형 위 두 점을 누르세요."), profileBox);
  trenchInfo_->setWordWrap(true);
  plot_ = new ProfilePlot(profileBox);
  profileLayout->addWidget(trenchInfo_);
  profileLayout->addWidget(plot_, 1);
  profileDock->setWidget(profileBox);
  addDockWidget(Qt::BottomDockWidgetArea, profileDock);

  status_ = new QLabel(this);
  statusBar()->addWidget(status_, 1);
  connect(viewport_, &KaGlTerrainViewport::frameRendered, this, &MainWindow::updateStatus);
  connect(viewport_, &KaGlTerrainViewport::pickRequested, this, &MainWindow::onPick);
  connect(viewport_, &KaGlTerrainViewport::trenchPointRequested, this, &MainWindow::onTrenchPoint);
  connect(&evalWatcher_, &QFutureWatcher<strata3d::Evaluation>::finished, this, &MainWindow::showEvaluation);

  connect(&watcher_, &QFutureWatcher<LoadResult>::finished, this, [this] {
    lastLoad_ = watcher_.result();
    if (!lastLoad_.tree) {
      status_->setText(QStringLiteral("불러오기 실패: ") + lastLoad_.error);
      emit loadFinished(false, lastLoad_.error);
      if (surveyPending_) { surveyPending_ = false; emit surveyOpened(false, lastLoad_.error); }
      emit sceneCompleted(false, lastLoad_.error);
      return;
    }
    viewport_->setTerrain(lastLoad_.tree);
    if (lastLoad_.mesh) viewport_->setScene(lastLoad_.mesh);
    const strata3d::DemGrid& g = lastLoad_.tree->grid();
    sceneComplete_ = false;
    clearTrench();  // 새 지형이면 이전 트렌치 좌표는 뜻이 없다
    evalOverlay_.reset();
    if (survey_) surveyOverlay_ = strata3d::buildSurveyOverlay(g, *survey_);
    refreshOverlay();
    QString msg = QStringLiteral("%1×%2 칸, 표고 %3~%4 m, %5초")
                      .arg(g.width).arg(g.height).arg(g.zMin, 0, 'f', 1).arg(g.zMax, 0, 'f', 1)
                      .arg(lastLoad_.seconds, 0, 'f', 1);
    if (lastLoad_.mesh) {
      const auto& m = *lastLoad_.mesh;
      msg += QStringLiteral(" | 건물 %1동(높이 실측 %2·층수 %3·미상 %4) 유산구역 %5 지정유산 %6")
                 .arg(m.buildings).arg(m.heightMeasured).arg(m.heightFromLevels).arg(m.heightUnknown)
                 .arg(m.heritageAreas).arg(m.heritagePoints);
    }
    if (!lastLoad_.warnings.isEmpty()) msg += QStringLiteral(" | 경고: ") + lastLoad_.warnings.join(QStringLiteral("; "));
    summary_ = msg;
    emit loadFinished(true, msg);
    status_->setText(msg);
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
    const QString m = QStringLiteral("위성 %1 %2×%3 + 중심 %4×%5, %6초").arg(s.source).arg(s.image.width()).arg(s.image.height()).arg(s.detail.width()).arg(s.detail.height()).arg(s.seconds, 0, 'f', 1);
    summary_ += QStringLiteral(" | ") + m;
    emit satelliteFinished(true, m);
    sceneComplete_ = true;
    if (surveyPending_) { surveyPending_ = false; emit surveyOpened(true, QStringLiteral("조사 %1 열림").arg(survey_ ? survey_->path : QString())); }
    emit sceneCompleted(true, summary_);
  });
}

void MainWindow::loadScene() {
  status_->setText(QStringLiteral("실측 DEM 불러오는 중…"));
  watcher_.setFuture(QtConcurrent::run(buildScene, options_));
}

void MainWindow::updateStatus() {
  if (!viewport_->hasTerrain()) return;
  const auto& s = viewport_->stats();
  const strata3d::DemGrid& g = lastLoad_.tree->grid();
  status_->setText(QStringLiteral("%1 · %2 · %3×%4 @ %5 m · 조각 %6개(가장 잘게 %7) · 선택 %8 ms · %9")
                       .arg(summary_, g.crs, g.source.contains(QStringLiteral("copernicus")) ? QStringLiteral("Copernicus GLO-30") : g.source)
                       .arg(g.width).arg(g.height).arg(g.pixelSize)
                       .arg(s.chunks).arg(s.finestChunks).arg(s.selectMs, 0, 'f', 2)
                       .arg(viewport_->glInfo()));
}

void MainWindow::onPick(const QVector3D& origin, const QVector3D& direction) {
  if (!lastLoad_.evaluator) return;
  strata3d::TerrainBvh::Hit hit;
  if (!lastLoad_.evaluator->terrain().rayCast(origin, direction, 1.0e6f, &hit)) return;  // 하늘을 눌렀다
  const strata3d::DemGrid& g = lastLoad_.evaluator->dem();
  evaluateAt(g.centerX() + hit.point.x(), g.centerY() + hit.point.y());
}

void MainWindow::evaluateAt(double x, double y) {
  if (!lastLoad_.evaluator) {
    emit evaluationFinished(false, QStringLiteral("지형이 아직 없습니다."));
    return;
  }
  report_->setHtml(QStringLiteral("<p>평가 계산 중…</p>"));
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

void MainWindow::refreshOverlay() {
  auto overlay = std::make_shared<strata3d::SceneMesh>();
  for (const auto& part : {surveyOverlay_, evalOverlay_, trenchOverlay_})
    if (part) strata3d::appendMesh(overlay.get(), *part);
  viewport_->setOverlay(overlay);
}

void MainWindow::setTrenchMode(bool on) {
  viewport_->setTrenchMode(on);
  if (trenchBtn_ && trenchBtn_->isChecked() != on) trenchBtn_->setChecked(on);
  trenchA_.reset();
  if (on) trenchInfo_->setText(QStringLiteral("시작점을 지형 위에서 누르세요."));
}

void MainWindow::keyPressEvent(QKeyEvent* e) {
  if (e->key() == Qt::Key_Escape && viewport_->trenchMode()) {
    trenchA_.reset();
    setTrenchMode(false);
    return;
  }
  QMainWindow::keyPressEvent(e);
}

void MainWindow::onTrenchPoint(const QVector3D& origin, const QVector3D& direction) {
  if (!lastLoad_.evaluator) return;
  strata3d::TerrainBvh::Hit hit;
  if (!lastLoad_.evaluator->terrain().rayCast(origin, direction, 1.0e6f, &hit)) return;  // 하늘을 눌렀다
  const strata3d::DemGrid& g = lastLoad_.evaluator->dem();
  const strata3d::Pt p{g.centerX() + hit.point.x(), g.centerY() + hit.point.y()};
  if (!trenchA_) {
    trenchA_ = p;
    trenchProfile_.clear();
    plot_->clear();
    auto m = std::make_shared<strata3d::SceneMesh>();
    strata3d::appendPole(m.get(), g, p, 3.0f, 40.0f, {1.0f, 0.95f, 0.1f, 1});
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
  if (trenchInfo_) trenchInfo_->setText(QStringLiteral("트렌치 없음. 「시굴 트렌치 그리기」를 켜고 지형 위 두 점을 누르세요."));
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
    trenchInfo_->setText(QStringLiteral("트렌치가 지형 밖입니다."));
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
  trenchSummary_ = QStringLiteral("시굴 트렌치 길이 %1 m · 방위 %2° · 시점 %3 m → 종점 %4 m(평균 기울기 %5%) · 최고 %6 m · 최저 %7 m · 국지 최대 기울기 %8%")
                       .arg(trenchProfile_.back().distanceM, 0, 'f', 1).arg(bearing, 0, 'f', 0)
                       .arg(trenchProfile_.front().elevationM, 0, 'f', 1).arg(trenchProfile_.back().elevationM, 0, 'f', 1)
                       .arg(mean * 100, 0, 'f', 1).arg(zMax, 0, 'f', 1).arg(zMin, 0, 'f', 1).arg(steepest * 100, 0, 'f', 1);
  trenchInfo_->setText(trenchSummary_ + QStringLiteral("\n지표면(Copernicus GLO-30, %1 m 격자) 단면이며 층서가 아닙니다. 수 m 규모의 트렌치는 격자 해상도 밖입니다.").arg(g.pixelSize, 0, 'f', 0));
  plot_->setProfile(trenchProfile_);
  auto m = std::make_shared<strata3d::SceneMesh>();
  strata3d::appendMesh(m.get(), *strata3d::buildProfileOverlay(g, trenchProfile_, 10.0f));
  strata3d::appendPole(m.get(), g, {ax, ay}, 3.0f, 40.0f, {1.0f, 0.95f, 0.1f, 1});
  strata3d::appendPole(m.get(), g, {bx, by}, 3.0f, 40.0f, {1.0f, 0.95f, 0.1f, 1});
  trenchOverlay_ = m;
  refreshOverlay();
  emit trenchChanged(trenchSummary_);
}

bool MainWindow::exportSheet(const QString& pngPath, const QString& pdfPath) {
  if (!viewport_->hasTerrain()) return false;
  constexpr int kSize = 3072;
  const QImage view = viewport_->renderToImage(kSize, kSize);
  if (view.isNull()) return false;
  const strata3d::DemGrid& g = lastLoad_.tree->grid();
  SheetInfo info;
  info.title = QStringLiteral("경주 입체지형 · 고고학 3D 평가");
  info.visibleWidthM = viewport_->visibleWidthAtTargetM(1.0f);
  info.yawDegFromNorth = viewport_->yawDegrees();
  info.crsLabel = crsLabelFor(g.crs);
  info.demName = QStringLiteral("Copernicus GLO-30 (%1 m 격자로 재배열)").arg(g.pixelSize, 0, 'f', 0);
  info.imageryName = imageryName_;
  info.zMin = g.zMin;
  info.zMax = g.zMax;
  info.elevationRamp = (mode_ == 1);
  info.generated = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
  info.hasTrench = !trenchSummary_.isEmpty();
  if (!trenchSummary_.isEmpty()) info.lines << trenchSummary_ + QStringLiteral(" (지표면 단면)");
  if (hasEvaluation_) {
    info.lines << lastEvaluation_.summary;
    for (const strata3d::RadiusResult& rr : lastEvaluation_.radii) {
      int n = 0;
      for (const strata3d::NearbyItem& it : rr.heritage) {
        if (++n > 4) break;
        info.lines << QStringLiteral("· %1 m 안 — %2 (%3) %4 m %5").arg(rr.radiusM, 0, 'f', 0).arg(it.name, it.category).arg(it.distanceM, 0, 'f', 0).arg(it.bearingName);
      }
    }
    info.lines << lastEvaluation_.notes;
  } else {
    info.lines << QStringLiteral("평가한 조사지가 없습니다(지형을 더블클릭하면 평가가 도면에 들어갑니다).");
  }
  const QImage sheet = composeSheet(view, info);
  bool ok = sheet.save(pngPath);
  if (ok && !pdfPath.isEmpty()) ok = saveSheetPdf(sheet, pdfPath, info.title);
  return ok;
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
  hasEvaluation_ = false;
  options_.workCrs = survey_->workCrs;
  options_.lon = lon;
  options_.lat = lat;
  surveyPending_ = true;
  loadScene();
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
  report_->setHtml(QStringLiteral("<p>조사지 평가 계산 중…</p>"));
  const auto ev = lastLoad_.evaluator;
  evalWatcher_.setFuture(QtConcurrent::run([ev, site, poly] { return ev->evaluate(site, poly); }));
}

