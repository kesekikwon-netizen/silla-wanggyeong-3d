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

#include "BangriDockPanel.h"
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
  setWindowTitle(QStringLiteral("Strata 3D · 신라왕경·9주5소경 실측 지형 & 방리구획 복원"));
  resize(1540, 900);
  dbpiaLedger_.loadFromFile();

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
  zs->setValue(1.5);
  zs->setPrefix(QStringLiteral("높이 ×"));
  bar->addWidget(zs);
  zScale_ = 1.5f;
  viewport_->setZScale(zScale_);
  connect(zs, &QDoubleSpinBox::valueChanged, this, [this](double v) {
    zScale_ = float(v);
    viewport_->setZScale(zScale_);
  });
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
  auto* bangriSheetBtn = new QPushButton(QStringLiteral("방리구획 5패널 도면 저장…"), bar);
  bar->addWidget(bangriSheetBtn);
  connect(bangriSheetBtn, &QPushButton::clicked, this, [this] {
    const QString png = QFileDialog::getSaveFileName(
        this, QStringLiteral("신라 방리구획 5패널 정밀 도면 저장"),
        QStringLiteral("silla-bangri-sheet.png"), QStringLiteral("PNG (*.png)"));
    if (png.isEmpty()) return;
    exportBangriSheet(png, QString(png).replace(QStringLiteral(".png"), QStringLiteral(".pdf")));
  });
  auto* reload = new QPushButton(QStringLiteral("다시 불러오기"), bar);
  bar->addWidget(reload);
  connect(reload, &QPushButton::clicked, this, &MainWindow::loadScene);
  bar->addSeparator();
  tbLineDrawBtn_ = new QPushButton(QStringLiteral("✏️ 맵에 선 긋기"), bar);
  tbLineDrawBtn_->setCheckable(true);
  tbLineDrawBtn_->setToolTip(QStringLiteral("켜고 3D 지형 위 두 점을 클릭하면 지형과 함께 움직이는 3D 입체 선을 긋습니다."));
  bar->addWidget(tbLineDrawBtn_);
  connect(tbLineDrawBtn_, &QPushButton::toggled, this, [this](bool on) {
    setUserDrawMode(on ? 2 : (viewport_->drawMode() == KaGlTerrainViewport::DrawMode::UserLine ? 0 : int(viewport_->drawMode())));
  });

  tbGridDrawBtn_ = new QPushButton(QStringLiteral("📐 맵에 격자 만들기"), bar);
  tbGridDrawBtn_->setCheckable(true);
  tbGridDrawBtn_->setToolTip(QStringLiteral("켜고 3D 지형 위 두 점을 클릭하면 지형과 함께 움직이는 3D 입체 격자를 만듭니다."));
  bar->addWidget(tbGridDrawBtn_);
  connect(tbGridDrawBtn_, &QPushButton::toggled, this, [this](bool on) {
    setUserDrawMode(on ? 3 : (viewport_->drawMode() == KaGlTerrainViewport::DrawMode::UserGrid ? 0 : int(viewport_->drawMode())));
  });

  auto* tbInstantGridBtn = new QPushButton(QStringLiteral("➕ 현위치 격자"), bar);
  tbInstantGridBtn->setToolTip(QStringLiteral("현재 3D 화면 중심에 즉시 3D 격자를 생성합니다."));
  bar->addWidget(tbInstantGridBtn);
  connect(tbInstantGridBtn, &QPushButton::clicked, this, &MainWindow::addUserGridAtCameraCenter);

  auto* tbClearDrawBtn = new QPushButton(QStringLiteral("선·격자 지우기"), bar);
  bar->addWidget(tbClearDrawBtn);
  connect(tbClearDrawBtn, &QPushButton::clicked, this, &MainWindow::clearUserDrawings);

  bar->addSeparator();
  trenchBtn_ = new QPushButton(QStringLiteral("시굴 트렌치 그리기"), bar);
  trenchBtn_->setCheckable(true);
  trenchBtn_->setToolTip(QStringLiteral("켜고 지형 위 두 점을 차례로 누릅니다(Esc: 취소). 끌면 카메라가 돕니다."));
  bar->addWidget(trenchBtn_);
  connect(trenchBtn_, &QPushButton::toggled, this, &MainWindow::setTrenchMode);
  auto* clearTrenchBtn = new QPushButton(QStringLiteral("트렌치 지우기"), bar);
  bar->addWidget(clearTrenchBtn);
  connect(clearTrenchBtn, &QPushButton::clicked, this, &MainWindow::clearTrench);
  bar->addSeparator();
  auto* tbFontMinus = new QPushButton(QStringLiteral("글씨 -"), bar);
  auto* tbFontPlus = new QPushButton(QStringLiteral("글씨 +"), bar);
  auto* tbBoxMinus = new QPushButton(QStringLiteral("창·이름표 -"), bar);
  auto* tbBoxPlus = new QPushButton(QStringLiteral("창·이름표 +"), bar);
  bar->addWidget(tbFontMinus);
  bar->addWidget(tbFontPlus);
  bar->addWidget(tbBoxMinus);
  bar->addWidget(tbBoxPlus);

  auto* dock = new QDockWidget(QStringLiteral("고고학 3D 평가"), this);
  report_ = new QTextBrowser(dock);
  report_->setMinimumWidth(380);
  report_->setHtml(QStringLiteral("<p>지형을 <b>더블클릭</b>하면 그 지점을 500 m·1000 m 반경으로 평가합니다.</p>"));
  dock->setWidget(report_);
  addDockWidget(Qt::RightDockWidgetArea, dock);

  auto* bangriDock = new QDockWidget(QStringLiteral("신라 방리구획 · 9주5소경 복원"), this);
  bangriPanel_ = new BangriDockPanel(bangriDock);
  bangriDock->setWidget(bangriPanel_);
  addDockWidget(Qt::RightDockWidgetArea, bangriDock);
  tabifyDockWidget(dock, bangriDock);
  bangriDock->raise();

  connect(bangriPanel_, &BangriDockPanel::presetSelected, this, [this](const QString& presetId) {
    selectBangriPreset(presetId, true);
  });
  connect(bangriPanel_, &BangriDockPanel::paramsChanged, this, [this](const strata3d::BangriParams&) {
    recomputeBangriState();
  });
  connect(bangriPanel_, &BangriDockPanel::terrainModeChanged, this, [this](bool, bool, bool) {
    rebuildSceneMeshForTerrainMode();
    recomputeBangriState();
  });
  connect(bangriPanel_, &BangriDockPanel::heritageSiteSelectionChanged, this, [this] {
    rebuildSceneMeshForTerrainMode();
    recomputeBangriState();
  });
  connect(bangriPanel_, &BangriDockPanel::heritageSiteFocusRequested, this, [this](const QString& siteId) {
    if (!viewport_) return;
    const QString activeCrs = lastLoad_.tree ? lastLoad_.tree->grid().crs : options_.workCrs;
    for (const auto& s : strata3d::BangriTerrainValidator::gyeongjuHeritageCatalog()) {
      if (s.id.compare(siteId, Qt::CaseInsensitive) == 0) {
        double wx = s.lon, wy = s.lat;
        if (strata3d::transformPoint(QStringLiteral("EPSG:4326"), activeCrs, &wx, &wy)) {
          viewport_->focusOnWorld(wx, wy, s.focusYawDeg, s.focusPitchDeg, s.focusDistM);
        }
        break;
      }
    }
  });
  connect(bangriPanel_, &BangriDockPanel::userDrawModeRequested, this, &MainWindow::setUserDrawMode);
  connect(bangriPanel_, &BangriDockPanel::addUserGridAtCameraRequested, this, &MainWindow::addUserGridAtCameraCenter);
  connect(bangriPanel_, &BangriDockPanel::undoUserDrawRequested, this, &MainWindow::undoLastUserDrawing);
  connect(bangriPanel_, &BangriDockPanel::clearUserDrawRequested, this, &MainWindow::clearUserDrawings);
  connect(bangriPanel_, &BangriDockPanel::userDrawStyleEdited, this, [this] {
    if (!bangriPanel_) return;
    bool changed = false;
    if (!userGrids_.empty()) {
      auto& g = userGrids_.back();
      g.cellWidthM = bangriPanel_->userGridCellWidthM();
      g.cellHeightM = bangriPanel_->userGridCellHeightM();
      g.cols = bangriPanel_->userGridCols();
      g.rows = bangriPanel_->userGridRows();
      g.azimuthDeg = bangriPanel_->userGridAzimuthDeg();
      g.lineWidthM = bangriPanel_->userDrawLineWidthM();
      g.subDivisions = bangriPanel_->userGridSubdivisions();
      g.color = bangriPanel_->userDrawColor();
      changed = true;
    }
    if (!userLines_.empty()) {
      auto& l = userLines_.back();
      l.widthM = bangriPanel_->userDrawLineWidthM();
      l.color = bangriPanel_->userDrawColor();
      changed = true;
    }
    if (changed) {
      rebuildUserDrawOverlay();
      recomputeBangriState();
    }
  });
  connect(bangriPanel_, &BangriDockPanel::uiAndHudScaleChanged, this, [this, bangriDock](float fontScale, float boxScale) {
    if (viewport_) {
      viewport_->setHudFontScale(fontScale);
      viewport_->setHudBoxScale(boxScale);
    }
    QFont f = font();
    f.setPointSizeF(std::clamp(9.5 * double(fontScale), 7.5, 16.0));
    bangriPanel_->setFont(f);
    if (report_) report_->setFont(f);
    bangriDock->setMinimumWidth(std::clamp(int(360 * boxScale), 260, 680));
    bangriPanel_->updateVerificationSummary(bangriData_);
  });
  connect(viewport_, &KaGlTerrainViewport::hudScaleChanged, this, [this](float fontScale, float boxScale) {
    if (bangriPanel_) bangriPanel_->setHudScales(fontScale, boxScale, false);
  });
  connect(tbFontMinus, &QPushButton::clicked, this, [this] {
    if (bangriPanel_) bangriPanel_->setHudScales(bangriPanel_->hudFontScale() - 0.10f, bangriPanel_->hudBoxScale(), true);
  });
  connect(tbFontPlus, &QPushButton::clicked, this, [this] {
    if (bangriPanel_) bangriPanel_->setHudScales(bangriPanel_->hudFontScale() + 0.10f, bangriPanel_->hudBoxScale(), true);
  });
  connect(tbBoxMinus, &QPushButton::clicked, this, [this] {
    if (bangriPanel_) bangriPanel_->setHudScales(bangriPanel_->hudFontScale(), bangriPanel_->hudBoxScale() - 0.10f, true);
  });
  connect(tbBoxPlus, &QPushButton::clicked, this, [this] {
    if (bangriPanel_) bangriPanel_->setHudScales(bangriPanel_->hudFontScale(), bangriPanel_->hudBoxScale() + 0.10f, true);
  });
  connect(bangriPanel_, &BangriDockPanel::cameraTourRequested, this, [this](int targetKind) {
    if (!viewport_) return;
    const QString activeCrs = lastLoad_.tree ? lastLoad_.tree->grid().crs : options_.workCrs;
    const strata3d::BangriParams p = bangriPanel_->currentParams();
    auto jumpLonLat = [&](double lon, double lat, float yaw, float pitch, float dist) {
      double wx = lon, wy = lat;
      if (strata3d::transformPoint(QStringLiteral("EPSG:4326"), activeCrs, &wx, &wy)) {
        viewport_->focusOnWorld(wx, wy, yaw, pitch, dist);
      }
    };
    using T = BangriDockPanel::CameraTourTarget;
    switch (T(targetKind)) {
      case T::HwangnyongsaAndGrid:
        if (p.presetId.startsWith(QLatin1String("gyeongju")))
          jumpLonLat(129.23313, 35.83750, 14.0f, 32.0f, 820.0f);
        else
          viewport_->focusOnWorld(p.originEasting, p.originNorthing, 16.0f, 34.0f, 1180.0f);
        break;
      case T::PalaceAndWolji:
        jumpLonLat(129.22520, 35.83320, 12.0f, 32.0f, 1050.0f);
        break;
      case T::RoyalTombsDaereung:
        jumpLonLat(129.21160, 35.83900, 20.0f, 30.0f, 920.0f);
        break;
      case T::StreetCloseUp:
        jumpLonLat(129.21830, 35.83180, 10.0f, 31.0f, 860.0f);
        break;
      case T::CapitalOverview:
      default:
        if (p.presetId.startsWith(QLatin1String("gyeongju")))
          jumpLonLat(129.22300, 35.83550, 16.0f, 38.0f, 2450.0f);
        else
          viewport_->focusOnWorld(p.originEasting, p.originNorthing, 18.0f, 42.0f, 2800.0f);
        break;
    }
  });
  connect(bangriPanel_, &BangriDockPanel::autoFitRequested, this, &MainWindow::autoFitBangriAnchors);
  connect(bangriPanel_, &BangriDockPanel::exportBangriSheetRequested, this, [this] {
    const QString png = QFileDialog::getSaveFileName(
        this, QStringLiteral("신라 방리구획 5패널 정밀 도면 저장"),
        QStringLiteral("silla-bangri-sheet.png"), QStringLiteral("PNG (*.png)"));
    if (png.isEmpty()) return;
    exportBangriSheet(png, QString(png).replace(QStringLiteral(".png"), QStringLiteral(".pdf")));
  });
  connect(bangriPanel_, &BangriDockPanel::applyRecommendedTrenchRequested, this, &MainWindow::setTrench);

  if (!options_.bangriPresetId.isEmpty()) {
    selectBangriPreset(options_.bangriPresetId, false);
  }

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
  connect(viewport_, &KaGlTerrainViewport::mapDrawPointRequested, this, &MainWindow::onMapDrawPoint);
  connect(viewport_, &KaGlTerrainViewport::mapHoverRay, this, &MainWindow::onMapHoverRay);
  connect(viewport_, &KaGlTerrainViewport::mapDrawCancelRequested, this, [this] {
    if (userDrawStartPt_) {
      userDrawStartPt_.reset();
      previewLine_.reset();
      previewGrid_.reset();
      rebuildUserDrawOverlay();
      if (bangriPanel_) {
        bangriPanel_->setUserDrawStatusText(QStringLiteral("시작점이 취소되었습니다. 지형 위 새로운 시작점을 클릭하세요."));
      }
    } else {
      setUserDrawMode(0);
    }
  });
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
    const strata3d::DemGrid& g = lastLoad_.tree->grid();
    sceneComplete_ = false;
    clearTrench();  // 새 지형이면 이전 트렌치 좌표는 뜻이 없다
    evalOverlay_.reset();
    if (survey_) surveyOverlay_ = strata3d::buildSurveyOverlay(g, *survey_);
    rebuildSceneMeshForTerrainMode();
    recomputeBangriState();
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
  recomputeBangriState();
  emit evaluationFinished(true, lastEvaluation_.summary);
}

void MainWindow::refreshOverlay() {
  auto overlay = std::make_shared<strata3d::SceneMesh>();
  for (const auto& part : {surveyOverlay_, bangriOverlay_, evalOverlay_, trenchOverlay_, userDrawOverlay_})
    if (part) strata3d::appendMesh(overlay.get(), *part);
  viewport_->setOverlay(overlay);
}

void MainWindow::rebuildSceneMeshForTerrainMode() {
  if (!lastLoad_.tree) return;
  const strata3d::DemGrid& g = lastLoad_.tree->grid();
  const std::vector<strata3d::GeoFeature> emptyFeatures;
  const auto& feats = lastLoad_.features ? *lastLoad_.features : emptyFeatures;
  const bool bareEarth = bangriPanel_ && bangriPanel_->bareEarthMode();
  const bool mounds = bangriPanel_ && bangriPanel_->reconstructMounds();
  const std::vector<QString> enabledSites = bangriPanel_ ? bangriPanel_->enabledHeritageSiteIds() : std::vector<QString>{};
  const std::vector<QString>* siteFilter = bangriPanel_ ? &enabledSites : nullptr;
  if (bareEarth || mounds || siteFilter) {
    std::vector<strata3d::SillaMound> collected;
    auto scene = strata3d::SillaMoundReconstructor::buildBareEarthAndMoundScene(
        g, feats, bareEarth, mounds, &collected, siteFilter);
    bangriData_.mounds = std::move(collected);
    viewport_->setScene(scene);
  } else {
    bangriData_.mounds = strata3d::SillaMoundReconstructor::collectMounds(g, feats, true, siteFilter);
    if (lastLoad_.mesh) viewport_->setScene(lastLoad_.mesh);
  }
}

void MainWindow::selectBangriPreset(const QString& presetId, bool allowSceneReload) {
  const strata3d::BangriCityPreset preset = strata3d::BangriGridEngine::findPreset(presetId);
  if (bangriPanel_) {
    bangriPanel_->setPresetById(preset.id, false);
  }
  options_.bangriPresetId = preset.id;

  // 현재 DEM 범위 밖이거나 아직 지형을 안 불러온 경우 프리셋 좌표로 이동한다.
  bool needReload = false;
  if (!lastLoad_.tree) {
    options_.lon = preset.centerLon;
    options_.lat = preset.centerLat;
    options_.workCrs = preset.workCrs;
  } else {
    const strata3d::DemGrid& g = lastLoad_.tree->grid();
    double px = preset.centerLon, py = preset.centerLat;
    float zDummy = 0.0f;
    if (g.crs != preset.workCrs ||
        !strata3d::transformPoint(QStringLiteral("EPSG:4326"), g.crs, &px, &py) ||
        !g.sample(px, py, &zDummy)) {
      options_.lon = preset.centerLon;
      options_.lat = preset.centerLat;
      options_.workCrs = preset.workCrs;
      needReload = true;
    }
  }

  if (needReload && allowSceneReload) {
    loadScene();
    return;
  }
  recomputeBangriState();
  if (viewport_) {
    const strata3d::BangriParams p = bangriPanel_ ? bangriPanel_->currentParams() : preset.defaultParams;
    viewport_->focusOnWorld(p.originEasting, p.originNorthing, 16.0f, 34.0f, 1350.0f);
  }
}

void MainWindow::recomputeBangriState() {
  if (!bangriPanel_) return;
  const QString presetId = bangriPanel_->currentPresetId();
  strata3d::BangriCityPreset preset = strata3d::BangriGridEngine::findPreset(presetId);
  strata3d::BangriParams params = bangriPanel_->currentParams();

  const QString activeCrs = lastLoad_.tree ? lastLoad_.tree->grid().crs : options_.workCrs;
  if (params.workCrs != activeCrs) {
    double ox = preset.centerLon, oy = preset.centerLat;
    if (strata3d::transformPoint(QStringLiteral("EPSG:4326"), activeCrs, &ox, &oy)) {
      params.originEasting = ox;
      params.originNorthing = oy;
      params.workCrs = activeCrs;
      bangriPanel_->setParams(params, false);
    }
  }

  bangriData_.preset = preset;
  bangriData_.bareEarthMode = bangriPanel_->bareEarthMode();
  bangriData_.reconstructMounds = bangriPanel_->reconstructMounds();
  bangriData_.zExaggeration = zScale_;
  bangriData_.grid = strata3d::BangriGridEngine::generateGrid(params);
  bangriData_.anchors = strata3d::BangriGridEngine::builtInAnchorsForPreset(preset.id, activeCrs);
  bangriData_.anchorRmseM = strata3d::BangriGridEngine::computeAnchorRmse(
      params, bangriData_.anchors, &bangriData_.residuals);
  bangriData_.citations = dbpiaLedger_.matchForPreset(
      preset.citationNodeIds, preset.searchKeywords, 8);

  if (lastLoad_.tree) {
    const strata3d::DemGrid& g = lastLoad_.tree->grid();
    const std::vector<strata3d::GeoFeature> emptyFeatures;
    const auto& feats = lastLoad_.features ? *lastLoad_.features : emptyFeatures;
    const std::vector<QString> enabledSites = bangriPanel_->enabledHeritageSiteIds();
    bangriData_.mounds = strata3d::SillaMoundReconstructor::collectMounds(g, feats, true, &enabledSites);
    bangriData_.fossilization = strata3d::FossilizedRoadAnalyzer::analyze(bangriData_.grid, feats);
    bangriData_.validation = strata3d::BangriTerrainValidator::validate(
        g, bangriData_.grid, bangriData_.mounds);

    strata3d::Pt evalSite{params.originEasting + 0.35 * params.blockWidthM,
                          params.originNorthing + 0.28 * params.blockHeightM};
    std::vector<strata3d::Pt> evalPoly;
    if (hasEvaluation_) {
      evalSite = lastEvaluation_.site;
    } else if (survey_) {
      evalSite = survey_->center;
      if (!survey_->surveyAreas.empty()) evalPoly = survey_->surveyAreas.front();
    }
    bangriData_.surveyEval = strata3d::BangriSurveyEvaluator::evaluateSurvey(
        bangriData_.grid, evalSite, evalPoly);

    if (bangriPanel_->gridOverlayEnabled() || bangriPanel_->architecture3dEnabled()) {
      auto mesh = std::make_shared<strata3d::SceneMesh>();
      auto gridMesh = strata3d::BangriTerrainValidator::buildValidatedGridOverlay(
          g, bangriData_.grid, bangriData_.validation, bangriData_.anchors, bangriData_.residuals,
          bangriData_.mounds, bangriPanel_->architecture3dEnabled(), bangriPanel_->gridOverlayEnabled(),
          &enabledSites);
      if (gridMesh) strata3d::appendMesh(mesh.get(), *gridMesh);
      if (bangriPanel_->gridOverlayEnabled()) {
        auto fossilMesh = strata3d::FossilizedRoadAnalyzer::buildFossilizedOverlay(
            g, bangriData_.fossilization);
        if (fossilMesh) strata3d::appendMesh(mesh.get(), *fossilMesh);
        auto trenchMesh = strata3d::BangriSurveyEvaluator::buildSurveyTrenchesOverlay(
            g, bangriData_.surveyEval);
        if (trenchMesh) strata3d::appendMesh(mesh.get(), *trenchMesh);
      }
      bangriOverlay_ = mesh;
    } else {
      bangriOverlay_.reset();
    }

    rebuildUserDrawOverlay();

    // 3D 화면 위에 경주 실제 유적 위치와 1:1 일치하는 안내 이름표(CalloutLabel3D) 생성 (켜진 유적만 표시)
    std::vector<KaGlTerrainViewport::CalloutLabel3D> labels;
    auto isSiteEnabled = [&](const QString& siteId) {
      return std::find(enabledSites.begin(), enabledSites.end(), siteId) != enabledSites.end();
    };

    if (params.presetId.startsWith(QLatin1String("gyeongju"))) {
      for (const auto& s : strata3d::BangriTerrainValidator::gyeongjuHeritageCatalog()) {
        if (!isSiteEnabled(s.id)) continue;
        double wx = s.lon, wy = s.lat;
        if (strata3d::transformPoint(QStringLiteral("EPSG:4326"), g.crs, &wx, &wy)) {
          KaGlTerrainViewport::CalloutLabel3D l;
          l.worldX = wx;
          l.worldY = wy;
          l.heightAboveGroundM = s.labelHeightM;
          l.category = s.category;
          l.title = s.name;
          l.subtitle = s.subtitle;
          l.badgeColor = QColor::fromRgbF(s.badgeColor[0], s.badgeColor[1], s.badgeColor[2], 1.0f);
          l.important = s.importantLabel;
          labels.push_back(std::move(l));
        }
      }
    } else {
      KaGlTerrainViewport::CalloutLabel3D centerLbl;
      centerLbl.worldX = params.originEasting;
      centerLbl.worldY = params.originNorthing;
      centerLbl.heightAboveGroundM = 24.0f;
      centerLbl.category = preset.categoryLabel;
      centerLbl.title = preset.name + QStringLiteral(" 중심 관아·정전");
      centerLbl.subtitle = preset.modernRegion + QStringLiteral(" · 신라 거점 도시 치소 유적");
      centerLbl.badgeColor = QColor(195, 55, 40);
      centerLbl.important = true;
      labels.push_back(std::move(centerLbl));
    }

    // 사용자가 직접 그은 3D 선과 3D 격자의 치수·방위각 이름표도 3D 공간 위에 함께 띄운다
    for (size_t i = 0; i < userLines_.size(); ++i) {
      const auto& ln = userLines_[i];
      const double lenM = std::hypot(ln.b.x - ln.a.x, ln.b.y - ln.a.y);
      double az = std::atan2(ln.b.x - ln.a.x, ln.b.y - ln.a.y) * 180.0 / 3.14159265358979323846;
      if (az < 0.0) az += 360.0;
      KaGlTerrainViewport::CalloutLabel3D l;
      l.worldX = 0.5 * (ln.a.x + ln.b.x);
      l.worldY = 0.5 * (ln.a.y + ln.b.y);
      l.heightAboveGroundM = 14.0f;
      l.category = QStringLiteral("사용자 3D 선 #%1").arg(i + 1);
      l.title = QStringLiteral("길이 %1 m (고구려척 %2척)").arg(lenM, 0, 'f', 1).arg(lenM / 0.356, 0, 'f', 0);
      l.subtitle = QStringLiteral("방위각 %1° · 굵기 %2m").arg(az, 0, 'f', 1).arg(ln.widthM, 0, 'f', 1);
      l.badgeColor = QColor::fromRgbF(ln.color[0] * 0.78f, ln.color[1] * 0.78f, ln.color[2] * 0.78f, 1.0f);
      l.important = true;
      labels.push_back(std::move(l));
    }

    for (size_t i = 0; i < userGrids_.size(); ++i) {
      const auto& gr = userGrids_[i];
      KaGlTerrainViewport::CalloutLabel3D l;
      l.worldX = gr.origin.x;
      l.worldY = gr.origin.y;
      l.heightAboveGroundM = 18.0f;
      l.category = QStringLiteral("사용자 3D 격자 #%1").arg(i + 1);
      l.title = QStringLiteral("1칸 %1×%2m · %3×%4칸").arg(gr.cellWidthM, 0, 'f', 0).arg(gr.cellHeightM, 0, 'f', 0).arg(gr.cols).arg(gr.rows);
      l.subtitle = QStringLiteral("전체 %1×%2m · 회전각 %3°")
                       .arg(gr.cellWidthM * gr.cols, 0, 'f', 0)
                       .arg(gr.cellHeightM * gr.rows, 0, 'f', 0)
                       .arg(gr.azimuthDeg, 0, 'f', 1);
      l.badgeColor = QColor::fromRgbF(gr.color[0] * 0.75f, gr.color[1] * 0.75f, gr.color[2] * 0.75f, 1.0f);
      l.important = true;
      labels.push_back(std::move(l));
    }

    if (viewport_) {
      viewport_->setShowCalloutLabels(bangriPanel_->calloutLabelsEnabled());
      viewport_->setShowTopBanner(bangriPanel_->topBannerEnabled());
      viewport_->setHudFontScale(bangriPanel_->hudFontScale());
      viewport_->setHudBoxScale(bangriPanel_->hudBoxScale());
      viewport_->setCalloutLabels(std::move(labels));
    }
  } else {
    strata3d::Pt evalSite{params.originEasting + 0.35 * params.blockWidthM,
                          params.originNorthing + 0.28 * params.blockHeightM};
    bangriData_.surveyEval = strata3d::BangriSurveyEvaluator::evaluateSurvey(
        bangriData_.grid, evalSite);
    bangriOverlay_.reset();
  }

  bangriPanel_->updateVerificationSummary(bangriData_);
  if (viewport_) refreshOverlay();
}

void MainWindow::autoFitBangriAnchors() {
  if (!bangriPanel_) return;
  strata3d::BangriParams params = bangriPanel_->currentParams();
  const QString activeCrs = lastLoad_.tree ? lastLoad_.tree->grid().crs : options_.workCrs;
  const auto anchors = strata3d::BangriGridEngine::builtInAnchorsForPreset(
      bangriPanel_->currentPresetId(), activeCrs);
  if (anchors.empty()) return;
  strata3d::BangriGridEngine::autoFitToAnchors(&params, anchors, 6.0);
  bangriPanel_->setParams(params, false);
  recomputeBangriState();
}

void MainWindow::setTrenchMode(bool on) {
  if (on) {
    setUserDrawMode(0);
    viewport_->setTrenchMode(true);
  } else if (viewport_->trenchMode()) {
    viewport_->setTrenchMode(false);
  }
  if (trenchBtn_ && trenchBtn_->isChecked() != on) trenchBtn_->setChecked(on);
  trenchA_.reset();
  if (on) trenchInfo_->setText(QStringLiteral("시작점을 지형 위에서 누르세요."));
}

void MainWindow::setUserDrawMode(int drawMode) {
  userDrawStartPt_.reset();
  previewLine_.reset();
  previewGrid_.reset();
  if (drawMode != 0 && trenchBtn_ && trenchBtn_->isChecked()) {
    trenchBtn_->blockSignals(true);
    trenchBtn_->setChecked(false);
    trenchBtn_->blockSignals(false);
    trenchA_.reset();
  }
  if (tbLineDrawBtn_) {
    tbLineDrawBtn_->blockSignals(true);
    tbLineDrawBtn_->setChecked(drawMode == 2);
    tbLineDrawBtn_->blockSignals(false);
  }
  if (tbGridDrawBtn_) {
    tbGridDrawBtn_->blockSignals(true);
    tbGridDrawBtn_->setChecked(drawMode == 3);
    tbGridDrawBtn_->blockSignals(false);
  }
  if (bangriPanel_) {
    bangriPanel_->syncDrawToolButtons(drawMode);
    if (drawMode == 2) {
      bangriPanel_->setUserDrawStatusText(
          QStringLiteral("✏️ [3D 선 긋기 켜짐] 지형 위 시작점을 클릭하세요. (마우스 드래그로 3D 카메라 회전 가능)"));
    } else if (drawMode == 3) {
      bangriPanel_->setUserDrawStatusText(
          QStringLiteral("📐 [3D 격자 만들기 켜짐] 격자 시작점을 지형 위에서 클릭하세요. (마우스 드래그로 3D 카메라 회전 가능)"));
    } else {
      bangriPanel_->setUserDrawStatusText(
          QStringLiteral("그린 3D 선 %1개 · 3D 격자 %2개 (3D 카메라를 돌려도 지형과 함께 움직입니다).")
              .arg(userLines_.size())
              .arg(userGrids_.size()));
    }
  }
  if (viewport_) {
    viewport_->setDrawMode(KaGlTerrainViewport::DrawMode(drawMode));
  }
  rebuildUserDrawOverlay();
}

void MainWindow::onMapDrawPoint(int drawMode, const QVector3D& origin, const QVector3D& direction) {
  if (!lastLoad_.evaluator) return;
  strata3d::TerrainBvh::Hit hit;
  if (!lastLoad_.evaluator->terrain().rayCast(origin, direction, 1.0e6f, &hit)) return;
  const strata3d::DemGrid& g = lastLoad_.evaluator->dem();
  const strata3d::Pt p{g.centerX() + hit.point.x(), g.centerY() + hit.point.y()};

  if (!userDrawStartPt_) {
    userDrawStartPt_ = p;
    previewLine_.reset();
    previewGrid_.reset();
    if (bangriPanel_) {
      if (drawMode == 2) {
        bangriPanel_->setUserDrawStatusText(
            QStringLiteral("✏️ 시작점 설정됨 (X %1, Y %2) — 마우스를 움직여 끝점을 클릭하세요 (우클릭/Esc: 취소)")
                .arg(p.x, 0, 'f', 1)
                .arg(p.y, 0, 'f', 1));
      } else {
        bangriPanel_->setUserDrawStatusText(
            QStringLiteral("📐 격자 시작점 설정됨 (X %1, Y %2) — 마우스를 움직여 반대쪽 끝점을 클릭하세요")
                .arg(p.x, 0, 'f', 1)
                .arg(p.y, 0, 'f', 1));
      }
    }
    rebuildUserDrawOverlay();
    return;
  }

  const strata3d::Pt a = *userDrawStartPt_;
  userDrawStartPt_.reset();
  previewLine_.reset();
  previewGrid_.reset();

  if (drawMode == 2) {
    addUserLine(a.x, a.y, p.x, p.y);
  } else if (drawMode == 3) {
    if (!bangriPanel_) return;
    strata3d::UserDrawnGrid3D gr = strata3d::BangriTerrainValidator::makeGridFromTwoPoints(
        a, p,
        bangriPanel_->userGridCellWidthM(),
        bangriPanel_->userGridCellHeightM(),
        bangriPanel_->userGridCols(),
        bangriPanel_->userGridRows(),
        bangriPanel_->userGridAzimuthDeg(),
        bangriPanel_->userGridAutoAlign(),
        bangriPanel_->userDrawLineWidthM(),
        bangriPanel_->userGridSubdivisions(),
        bangriPanel_->userDrawColor(),
        false);
    userGrids_.push_back(gr);
    userDrawHistoryIsGrid_.push_back(true);
    if (bangriPanel_) {
      bangriPanel_->setUserDrawStatusText(
          QStringLiteral("✅ 3D 격자 #%1 생성 완료 (%2×%3m, %4×%5칸). 계속 클릭하여 추가하거나 3D 화면을 회전해 보세요!")
              .arg(userGrids_.size())
              .arg(gr.cellWidthM, 0, 'f', 0)
              .arg(gr.cellHeightM, 0, 'f', 0)
              .arg(gr.cols)
              .arg(gr.rows));
    }
    recomputeBangriState();
  }
}

void MainWindow::onMapHoverRay(int drawMode, const QVector3D& origin, const QVector3D& direction) {
  if (!userDrawStartPt_ || !lastLoad_.evaluator || !bangriPanel_) return;
  strata3d::TerrainBvh::Hit hit;
  if (!lastLoad_.evaluator->terrain().rayCast(origin, direction, 1.0e6f, &hit)) return;
  const strata3d::DemGrid& g = lastLoad_.evaluator->dem();
  const strata3d::Pt p{g.centerX() + hit.point.x(), g.centerY() + hit.point.y()};
  const strata3d::Pt a = *userDrawStartPt_;

  if (drawMode == 2) {
    strata3d::UserDrawnLine3D ln;
    ln.a = a;
    ln.b = p;
    ln.widthM = bangriPanel_->userDrawLineWidthM();
    ln.color = bangriPanel_->userDrawColor();
    ln.isPreview = true;
    previewLine_ = ln;
    previewGrid_.reset();
    const double lenM = std::hypot(p.x - a.x, p.y - a.y);
    bangriPanel_->setUserDrawStatusText(
        QStringLiteral("✏️ 미리보기 길이: %1 m (고구려척 %2척) — 클릭하면 3D 선이 확정됩니다.")
            .arg(lenM, 0, 'f', 1)
            .arg(lenM / 0.356, 0, 'f', 0));
  } else if (drawMode == 3) {
    previewGrid_ = strata3d::BangriTerrainValidator::makeGridFromTwoPoints(
        a, p,
        bangriPanel_->userGridCellWidthM(),
        bangriPanel_->userGridCellHeightM(),
        bangriPanel_->userGridCols(),
        bangriPanel_->userGridRows(),
        bangriPanel_->userGridAzimuthDeg(),
        bangriPanel_->userGridAutoAlign(),
        bangriPanel_->userDrawLineWidthM(),
        bangriPanel_->userGridSubdivisions(),
        bangriPanel_->userDrawColor(),
        true);
    previewLine_.reset();
    bangriPanel_->setUserDrawStatusText(
        QStringLiteral("📐 미리보기 격자: %1×%2칸 (전체 %3×%4 m, 각도 %5°) — 클릭하면 3D 격자가 확정됩니다.")
            .arg(previewGrid_->cols)
            .arg(previewGrid_->rows)
            .arg(previewGrid_->cols * previewGrid_->cellWidthM, 0, 'f', 0)
            .arg(previewGrid_->rows * previewGrid_->cellHeightM, 0, 'f', 0)
            .arg(previewGrid_->azimuthDeg, 0, 'f', 1));
  }
  rebuildUserDrawOverlay();
}

void MainWindow::addUserLine(double ax, double ay, double bx, double by) {
  if (std::hypot(bx - ax, by - ay) < 1.0) return;
  strata3d::UserDrawnLine3D ln;
  ln.a = {ax, ay};
  ln.b = {bx, by};
  ln.widthM = bangriPanel_ ? bangriPanel_->userDrawLineWidthM() : 4.0f;
  ln.color = bangriPanel_ ? bangriPanel_->userDrawColor() : std::array<float, 4>{1.0f, 0.84f, 0.16f, 1.0f};
  userLines_.push_back(ln);
  userDrawHistoryIsGrid_.push_back(false);
  if (bangriPanel_) {
    const double lenM = std::hypot(bx - ax, by - ay);
    bangriPanel_->setUserDrawStatusText(
        QStringLiteral("✅ 3D 선 #%1 생성 완료 (길이 %2 m / %3척). 마우스로 3D 지형을 돌려 함께 움직이는지 확인하세요!")
            .arg(userLines_.size())
            .arg(lenM, 0, 'f', 1)
            .arg(lenM / 0.356, 0, 'f', 0));
  }
  recomputeBangriState();
}

void MainWindow::addUserGrid(double centerX, double centerY, double cellW, double cellH, int cols, int rows, double azimuthDeg) {
  strata3d::UserDrawnGrid3D gr;
  gr.origin = {centerX, centerY};
  gr.cellWidthM = cellW;
  gr.cellHeightM = cellH;
  gr.cols = cols;
  gr.rows = rows;
  gr.azimuthDeg = azimuthDeg;
  gr.centerAligned = true;
  gr.lineWidthM = bangriPanel_ ? bangriPanel_->userDrawLineWidthM() : 3.5f;
  gr.subDivisions = bangriPanel_ ? bangriPanel_->userGridSubdivisions() : 1;
  gr.color = bangriPanel_ ? bangriPanel_->userDrawColor() : std::array<float, 4>{0.14f, 0.88f, 1.0f, 1.0f};
  userGrids_.push_back(gr);
  userDrawHistoryIsGrid_.push_back(true);
  if (bangriPanel_) {
    bangriPanel_->setUserDrawStatusText(
        QStringLiteral("✅ 3D 격자 #%1 생성 완료 (%2×%3m · %4×%5칸). 우측 스핀박스로 간격·칸수·각도를 실시간 조절할 수 있습니다!")
            .arg(userGrids_.size())
            .arg(cellW, 0, 'f', 0)
            .arg(cellH, 0, 'f', 0)
            .arg(cols)
            .arg(rows));
  }
  recomputeBangriState();
}

void MainWindow::addUserGridAtCameraCenter() {
  if (!bangriPanel_) return;
  double cx = 0.0, cy = 0.0;
  if (!viewport_ || !viewport_->currentTargetWorld(&cx, &cy)) {
    const auto p = bangriPanel_->currentParams();
    cx = p.originEasting;
    cy = p.originNorthing;
  }
  addUserGrid(cx, cy,
              bangriPanel_->userGridCellWidthM(),
              bangriPanel_->userGridCellHeightM(),
              bangriPanel_->userGridCols(),
              bangriPanel_->userGridRows(),
              bangriPanel_->userGridAzimuthDeg());
}

void MainWindow::undoLastUserDrawing() {
  if (userDrawStartPt_) {
    userDrawStartPt_.reset();
    previewLine_.reset();
    previewGrid_.reset();
    rebuildUserDrawOverlay();
    return;
  }
  if (userDrawHistoryIsGrid_.empty()) return;
  const bool wasGrid = userDrawHistoryIsGrid_.back();
  userDrawHistoryIsGrid_.pop_back();
  if (wasGrid && !userGrids_.empty()) userGrids_.pop_back();
  else if (!wasGrid && !userLines_.empty()) userLines_.pop_back();
  if (bangriPanel_) {
    bangriPanel_->setUserDrawStatusText(
        QStringLiteral("↩ 마지막 3D 그리기 취소됨 (남은 선 %1개 · 격자 %2개)")
            .arg(userLines_.size())
            .arg(userGrids_.size()));
  }
  recomputeBangriState();
}

void MainWindow::clearUserDrawings() {
  userDrawStartPt_.reset();
  previewLine_.reset();
  previewGrid_.reset();
  userLines_.clear();
  userGrids_.clear();
  userDrawHistoryIsGrid_.clear();
  userDrawOverlay_.reset();
  if (bangriPanel_) {
    bangriPanel_->setUserDrawStatusText(QStringLiteral("그린 3D 선과 3D 격자를 모두 지웠습니다."));
  }
  recomputeBangriState();
}

void MainWindow::rebuildUserDrawOverlay() {
  if (!lastLoad_.tree) {
    userDrawOverlay_.reset();
    return;
  }
  const strata3d::DemGrid& g = lastLoad_.tree->grid();
  std::vector<strata3d::UserDrawnLine3D> allLines = userLines_;
  if (previewLine_) allLines.push_back(*previewLine_);
  std::vector<strata3d::UserDrawnGrid3D> allGrids = userGrids_;
  if (previewGrid_) allGrids.push_back(*previewGrid_);

  auto mesh = strata3d::BangriTerrainValidator::buildUserDrawingsOverlay(
      g, bangriData_.mounds, allLines, allGrids);
  if (userDrawStartPt_) {
    const auto col = bangriPanel_ ? bangriPanel_->userDrawColor() : std::array<float, 4>{1.0f, 0.9f, 0.2f, 1.0f};
    strata3d::appendPole(mesh.get(), g, *userDrawStartPt_, 3.2f, 26.0f, col);
  }
  userDrawOverlay_ = mesh;
  if (viewport_) refreshOverlay();
}

void MainWindow::keyPressEvent(QKeyEvent* e) {
  if (e->key() == Qt::Key_Escape) {
    if (viewport_->trenchMode()) {
      trenchA_.reset();
      setTrenchMode(false);
      return;
    }
    if (viewport_->drawMode() != KaGlTerrainViewport::DrawMode::None) {
      if (userDrawStartPt_) {
        userDrawStartPt_.reset();
        previewLine_.reset();
        previewGrid_.reset();
        rebuildUserDrawOverlay();
      } else {
        setUserDrawMode(0);
      }
      return;
    }
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

bool MainWindow::exportBangriSheet(const QString& pngPath, const QString& pdfPath) {
  if (!viewport_->hasTerrain()) return false;
  recomputeBangriState();
  const QImage view = viewport_->renderToImage(2048, 1600);
  if (view.isNull()) return false;
  const strata3d::DemGrid& g = lastLoad_.tree->grid();

  BangriSheetData sheetData = bangriData_;
  sheetData.baseInfo.title = QStringLiteral("%1 · 3D 고지형 및 방리구획 정밀 검증 도면").arg(sheetData.preset.name);
  sheetData.baseInfo.visibleWidthM = viewport_->visibleWidthAtTargetM(1.0f);
  sheetData.baseInfo.yawDegFromNorth = viewport_->yawDegrees();
  sheetData.baseInfo.crsLabel = crsLabelFor(g.crs);
  sheetData.baseInfo.demName = QStringLiteral("Copernicus GLO-30 (%1 m 격자)").arg(g.pixelSize, 0, 'f', 0);
  sheetData.baseInfo.imageryName = imageryName_;
  sheetData.baseInfo.zMin = g.zMin;
  sheetData.baseInfo.zMax = g.zMax;
  sheetData.baseInfo.elevationRamp = (mode_ == 1);
  sheetData.baseInfo.generated = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
  sheetData.baseInfo.hasTrench = !trenchSummary_.isEmpty() || !sheetData.surveyEval.recommendedTrenches.empty();

  const QImage sheet = composeBangriSheet(view, sheetData);
  bool ok = sheet.save(pngPath);
  if (ok && !pdfPath.isEmpty()) ok = saveSheetPdf(sheet, pdfPath, sheetData.baseInfo.title);
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
