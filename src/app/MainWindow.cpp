#include "MainWindow.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QStatusBar>
#include <QToolBar>
#include <QtConcurrent>

#include <QCheckBox>
#include <algorithm>

#include "CrsTransform.h"
#include "DemProvider.h"
#include "KaGlTerrainViewport.h"
#include "KhsProvider.h"
#include "LocalVectorProvider.h"
#include "OsmProvider.h"
#include "SatelliteProvider.h"

namespace {
const QString kWorkCrs = QStringLiteral("EPSG:5187");

// DEM 읽기·재배열·분트리 만들기는 전부 UI 스레드 밖에서 한다.
MainWindow::LoadResult buildScene(MainWindow::Options opt) {
  QElapsedTimer timer;
  timer.start();
  MainWindow::LoadResult out;
  double x = opt.lon, y = opt.lat;
  if (!strata3d::transformPoint(QStringLiteral("EPSG:4326"), kWorkCrs, &x, &y, &out.error)) return out;
  strata3d::DemRequest req;
  req.centerX = x;
  req.centerY = y;
  req.halfExtentM = opt.halfExtentM;
  req.pixelSizeM = opt.pixelSizeM;
  req.workCrs = kWorkCrs;
  req.cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/dem");
  if (!opt.demPath.isEmpty()) req.sources = {opt.demPath};
  strata3d::DemGrid grid;
  if (!strata3d::loadDemWindow(req, &grid, &out.error)) return out;
  out.tree = strata3d::TerrainLodTree::build(grid);
  if (!out.tree) {
    out.error = QStringLiteral("지형 트리를 만들지 못했습니다.");
    return out;
  }
  if (opt.loadFeatures) {
    strata3d::FeatureRequest fr;
    fr.centerX = x;
    fr.centerY = y;
    fr.halfExtentM = std::min(opt.featureRadiusM, opt.halfExtentM);
    fr.workCrs = kWorkCrs;
    fr.cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/vector");
    auto all = std::make_shared<std::vector<strata3d::GeoFeature>>();
    QString err;
    // 한 곳이 실패해도 나머지로 계속한다. 실패 사유는 화면에 그대로 알린다.
    if (!strata3d::loadOsmFeatures(fr, all.get(), &err)) out.warnings << err;
    if (!strata3d::loadKhsHeritage(fr, all.get(), &err)) out.warnings << err;
    QStringList skipped;
    strata3d::loadLocalVectors(fr, strata3d::strataHeritageShpPaths(), all.get(), &skipped);
    out.mesh = strata3d::buildSceneMesh(grid, *all);
    out.features = all;
  }
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
  sr.workCrs = kWorkCrs;
  sr.texels = opt.satelliteTexels;
  sr.cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/tiles");
  strata3d::SatelliteResult r;
  if (strata3d::loadSatelliteImage(sr, &r, &out.error)) {
    out.image = std::move(r.image);
    out.source = r.sourceName;
  }
  out.seconds = timer.elapsed() / 1000.0;
  return out;
}

}  // namespace

MainWindow::MainWindow(const Options& options, QWidget* parent) : QMainWindow(parent), options_(options) {
  setWindowTitle(QStringLiteral("Strata 3D · 경주 실측 지형"));
  resize(1280, 800);
  viewport_ = new KaGlTerrainViewport(this);
  setCentralWidget(viewport_);

  auto* bar = addToolBar(QStringLiteral("보기"));
  bar->setMovable(false);
  auto* mode = new QComboBox(bar);
  mode->addItems({QStringLiteral("점토 (기복 판독)"), QStringLiteral("표고 색띠"), QStringLiteral("경사도"), QStringLiteral("위성 실사")});
  connect(this, &MainWindow::satelliteFinished, mode, [mode](bool ok, const QString&) {
    if (ok) mode->setCurrentIndex(3);
  });
  bar->addWidget(mode);
  auto* zs = new QDoubleSpinBox(bar);
  zs->setRange(1.0, 5.0);
  zs->setSingleStep(0.5);
  zs->setValue(1.0);
  zs->setPrefix(QStringLiteral("높이 ×"));
  bar->addWidget(zs);
  auto* showScene = new QCheckBox(QStringLiteral("건물·유적"), bar);
  showScene->setChecked(true);
  bar->addWidget(showScene);
  connect(showScene, &QCheckBox::toggled, this, [this](bool on) { viewport_->setSceneVisible(on); });
  auto* reload = new QPushButton(QStringLiteral("다시 불러오기"), bar);
  bar->addWidget(reload);
  connect(mode, &QComboBox::currentIndexChanged, this, [this](int i) { viewport_->setMode(KaGlTerrainViewport::Mode(i)); });
  connect(zs, &QDoubleSpinBox::valueChanged, this, [this](double v) { viewport_->setZScale(float(v)); });
  connect(reload, &QPushButton::clicked, this, &MainWindow::loadScene);

  status_ = new QLabel(this);
  statusBar()->addWidget(status_, 1);
  connect(viewport_, &KaGlTerrainViewport::frameRendered, this, &MainWindow::updateStatus);
  connect(&watcher_, &QFutureWatcher<LoadResult>::finished, this, [this] {
    lastLoad_ = watcher_.result();
    if (!lastLoad_.tree) {
      status_->setText(QStringLiteral("불러오기 실패: ") + lastLoad_.error);
      emit loadFinished(false, lastLoad_.error);
      return;
    }
    viewport_->setTerrain(lastLoad_.tree);
    if (lastLoad_.mesh) viewport_->setScene(lastLoad_.mesh);
    const strata3d::DemGrid& g = lastLoad_.tree->grid();
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
    }
  });
  connect(&satWatcher_, &QFutureWatcher<SatelliteLoad>::finished, this, [this] {
    const SatelliteLoad s = satWatcher_.result();
    if (s.image.isNull()) {
      summary_ += QStringLiteral(" | 위성 실패: ") + s.error;
      emit satelliteFinished(false, s.error);
      return;
    }
    viewport_->setSatellite(s.image);
    const QString m = QStringLiteral("위성 %1 %2×%3, %4초").arg(s.source).arg(s.image.width()).arg(s.image.height()).arg(s.seconds, 0, 'f', 1);
    summary_ += QStringLiteral(" | ") + m;
    emit satelliteFinished(true, m);
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
