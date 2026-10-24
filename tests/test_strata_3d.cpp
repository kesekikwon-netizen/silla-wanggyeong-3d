// strata_3d_engine 시험. 모든 격자는 실제 Copernicus GLO-30 COG(/vsicurl/)에서 GDAL 로 읽는다.
// 네트워크가 막히면 시험은 건너뛰지 않고 실패한다(가짜 값으로 통과시키지 않기 위해).
#include <QMatrix4x4>
#include <QTemporaryDir>
#include <QVector2D>
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <vector>

#include "CrsTransform.h"
#include "DemProvider.h"
#include "Frustum.h"
#include "TerrainLodTree.h"

using namespace strata3d;

namespace {
// 첨성대 부근. 위경도(WGS84)에서 작업 좌표계로 옮긴 값이며 격자 값은 전부 원본 DEM 이다.
constexpr double kCheomseongdaeLon = 129.2194;
constexpr double kCheomseongdaeLat = 35.8347;
}  // namespace

class TestStrata3d : public QObject {
  Q_OBJECT
  QTemporaryDir cache_;
  double cx_ = 0, cy_ = 0;
  DemGrid dem_;

 private slots:
  void initTestCase() {
    QVERIFY(cache_.isValid());
    cx_ = kCheomseongdaeLon;
    cy_ = kCheomseongdaeLat;
    QString err;
    QVERIFY2(transformPoint("EPSG:4326", "EPSG:5187", &cx_, &cy_, &err), qPrintable(err));
    DemRequest req;
    req.centerX = cx_;
    req.centerY = cy_;
    req.halfExtentM = 12000;
    req.pixelSizeM = 30;
    req.cacheDir = cache_.path();
    QVERIFY2(loadDemWindow(req, &dem_, &err), qPrintable(err));
  }

  void crsMatchesEpsg5187Definition() {
    // EPSG:5187 은 (129°E, 38°N)이 (200000, 600000)이다.
    double x = 129.0, y = 38.0;
    QString err;
    QVERIFY2(transformPoint("EPSG:4326", "EPSG:5187", &x, &y, &err), qPrintable(err));
    QVERIFY(std::abs(x - 200000.0) < 0.01);
    QVERIFY(std::abs(y - 600000.0) < 0.01);
    // 왕복 변환
    double lon = 129.2194, lat = 35.8347;
    double px = lon, py = lat;
    QVERIFY(transformPoint("EPSG:4326", "EPSG:5187", &px, &py));
    QVERIFY(transformPoint("EPSG:5187", "EPSG:4326", &px, &py));
    QVERIFY(std::abs(px - lon) < 1e-8 && std::abs(py - lat) < 1e-8);
  }

  void demIsRealCopernicusWindow() {
    QVERIFY(dem_.valid());
    QCOMPARE(dem_.crs, QString("EPSG:5187"));
    QVERIFY2(dem_.source.contains("copernicus-dem-30m"), qPrintable(dem_.source));
    QCOMPARE(dem_.width, 800);
    QCOMPARE(dem_.height, 800);
    QCOMPARE(dem_.noDataCount, 0);
    qInfo() << "DEM zMin" << dem_.zMin << "zMax" << dem_.zMax << "source" << dem_.source;
    // 경주 분지(첨성대 반경 12 km): 평지는 수십 m, 주변 산(남산·토함산 자락)은 수백 m.
    QVERIFY(dem_.zMin > 0 && dem_.zMin < 60);
    QVERIFY(dem_.zMax > 300 && dem_.zMax < 1000);
    float zc = 0;
    QVERIFY(dem_.sample(cx_, cy_, &zc));
    qInfo() << "첨성대 부근 표고" << zc;
    QVERIFY(zc > 20 && zc < 80);
  }

  void cacheRoundTripKeepsGrid() {
    DemRequest req;
    req.centerX = cx_;
    req.centerY = cy_;
    req.halfExtentM = 12000;
    req.pixelSizeM = 30;
    req.cacheDir = cache_.path();
    DemGrid again;
    QString err;
    QVERIFY2(loadDemWindow(req, &again, &err), qPrintable(err));  // 두 번째는 캐시 GeoTIFF
    QCOMPARE(again.width, dem_.width);
    QCOMPARE(again.z, dem_.z);
    QCOMPARE(again.originX, dem_.originX);
  }

  void lodTreeCoversGridExactlyOnce() {
    const auto tree = TerrainLodTree::build(dem_);
    QVERIFY(tree);
    QVERIFY(tree->levels() > 2);
    const QVector3D eye = tree->toWorld(100, 100, dem_.zMax + 200, 1.0f);
    const auto chunks = tree->select(eye, nullptr, 1.0f);
    std::vector<int> cover(size_t(dem_.width - 1) * (dem_.height - 1), 0);
    for (const ChunkInstance& c : chunks) {
      for (int r = c.row0; r < std::min(c.row0 + c.sizePx, dem_.height - 1); ++r)
        for (int col = c.col0; col < std::min(c.col0 + c.sizePx, dem_.width - 1); ++col)
          ++cover[size_t(r) * (dem_.width - 1) + col];
    }
    for (int v : cover) QCOMPARE(v, 1);  // 틈 없음, 겹침 없음
  }

  void farChunksUseCoarserLevels() {
    const auto tree = TerrainLodTree::build(dem_);
    const QVector3D eye = tree->toWorld(40, 40, dem_.zMax + 100, 1.0f);
    const auto chunks = tree->select(eye, nullptr, 1.0f);
    int nearLevel = 99, farLevel = -1;
    for (const ChunkInstance& c : chunks) {
      const QVector3D mid = tree->toWorld(c.col0 + c.sizePx * 0.5, c.row0 + c.sizePx * 0.5, 0, 1.0f);
      const float d = QVector2D(mid.x() - eye.x(), mid.y() - eye.y()).length();
      if (d < 1500) nearLevel = std::min(nearLevel, c.level);
      if (d > 14000) farLevel = std::max(farLevel, c.level);
    }
    QCOMPARE(nearLevel, 0);
    QVERIFY2(farLevel >= 2, qPrintable(QString::number(farLevel)));
    // 모든 조각을 가장 잘게 그리는 것보다 훨씬 적어야 한다.
    const int leafCount = tree->nodesAt(0);
    QVERIFY2(int(chunks.size()) < leafCount, qPrintable(QString("%1 vs %2").arg(chunks.size()).arg(leafCount)));
  }

  void frustumCullingDropsHiddenChunks() {
    const auto tree = TerrainLodTree::build(dem_);
    const QVector3D eye = tree->toWorld(400, 780, dem_.zMax + 300, 1.0f);  // 남쪽 가장자리 위
    QMatrix4x4 view, proj;
    view.lookAt(eye, tree->toWorld(400, 400, 100, 1.0f), QVector3D(0, 0, 1));
    proj.perspective(45.0f, 1.6f, 10.0f, 60000.0f);
    const Frustum frustum(proj * view);
    const auto all = tree->select(eye, nullptr, 1.0f);
    const auto seen = tree->select(eye, &frustum, 1.0f);
    QVERIFY(!seen.empty());
    QVERIFY2(seen.size() < all.size(), qPrintable(QString("%1 vs %2").arg(seen.size()).arg(all.size())));
    // 뒤쪽(남쪽 바깥)을 보면 거의 아무것도 안 보인다.
    QMatrix4x4 away;
    away.lookAt(eye, eye + QVector3D(0, -1000, 0), QVector3D(0, 0, 1));
    const Frustum awayFrustum(proj * away);
    QVERIFY(tree->select(eye, &awayFrustum, 1.0f).empty());
  }
};

QTEST_GUILESS_MAIN(TestStrata3d)
#include "test_strata_3d.moc"
