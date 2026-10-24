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
#include "KhsProvider.h"
#include "LocalVectorProvider.h"
#include "OsmProvider.h"
#include "SatelliteProvider.h"
#include "SceneMesh.h"
#include "TerrainLodTree.h"
#include "Triangulate.h"
#include "VworldKey.h"

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
  // ---- 실제 건물·유적 벡터 ------------------------------------------------------------------
 private:
  FeatureRequest featureRequest(double half) const {
    FeatureRequest fr;
    fr.centerX = cx_;
    fr.centerY = cy_;
    fr.halfExtentM = half;
    fr.cacheDir = cache_.path();
    return fr;
  }
  std::vector<GeoFeature> osm_;
  std::vector<GeoFeature> khs_;

 private slots:
  void osmBuildingsAreRealFootprints() {
    QString err;
    QVERIFY2(loadOsmFeatures(featureRequest(3000), &osm_, &err), qPrintable(err));
    int buildings = 0, heritageAreas = 0, measured = 0, levels = 0;
    for (const GeoFeature& f : osm_) {
      if (f.kind == FeatureKind::Building) ++buildings;
      if (f.kind == FeatureKind::HeritageArea) ++heritageAreas;
      if (f.heightSource == HeightSource::Measured) ++measured;
      if (f.heightSource == HeightSource::FromLevels) ++levels;
      // 모든 외곽점은 요청 창 근처(버퍼 허용) 안에 있어야 한다.
      for (const Pt& p : f.ring) {
        QVERIFY(std::abs(p.x - cx_) < 3500 && std::abs(p.y - cy_) < 3500);
      }
    }
    qInfo() << "OSM 건물" << buildings << "유산 구역" << heritageAreas << "높이 실측" << measured << "층수" << levels;
    QVERIFY2(buildings > 1500, qPrintable(QString::number(buildings)));
    QVERIFY(heritageAreas > 20);
    QVERIFY(measured + levels > 0);  // 태그가 있는 건물은 값이 들어와야 한다
  }

  void osmHeightParsing() {
    double m = 0;
    QVERIFY(parseOsmHeight("9.4", &m) && std::abs(m - 9.4) < 1e-9);
    QVERIFY(parseOsmHeight("12 m", &m) && m == 12);
    QVERIFY(parseOsmHeight("7,5", &m) && std::abs(m - 7.5) < 1e-9);
    QVERIFY(!parseOsmHeight("tall", &m));
    QVERIFY(!parseOsmHeight("0", &m));
  }

  void khsHeritageHasGyeongjuMonuments() {
    QString err;
    QVERIFY2(loadKhsHeritage(featureRequest(6000), &khs_, &err), qPrintable(err));
    qInfo() << "국가유산청 경주 6km 안" << int(khs_.size()) << "건";
    QVERIFY(khs_.size() > 30);
    const GeoFeature* cheom = nullptr;
    for (const GeoFeature& f : khs_)
      if (f.name.contains(QString::fromUtf16(u"첨성대"))) cheom = &f;
    QVERIFY2(cheom, "국가유산청 목록에 첨성대가 없습니다");
    QCOMPARE(cheom->category, QString::fromUtf16(u"국보"));
    // 요청 중심(첨성대 위경도 어림값)에서 300 m 이내여야 한다.
    QVERIFY(std::hypot(cheom->point.x - cx_, cheom->point.y - cy_) < 300);
  }

  void osmAndKhsAgreeOnCheomseongdae() {
    QVERIFY(!osm_.empty() && !khs_.empty());  // 앞 시험에서 채움
    const GeoFeature* khs = nullptr;
    for (const GeoFeature& f : khs_)
      if (f.name.contains(QString::fromUtf16(u"첨성대"))) khs = &f;
    QVERIFY(khs);
    // 서로 다른 출처(OSM 외곽선, 국가유산청 좌표)의 첨성대가 150 m 안에서 일치해야 한다.
    double best = 1e18;
    for (const GeoFeature& f : osm_) {
      if (f.name.contains(QString::fromUtf16(u"첨성대"))) best = std::min(best, std::hypot(f.point.x - khs->point.x, f.point.y - khs->point.y));
    }
    qInfo() << "OSM–국가유산청 첨성대 거리(m)" << best;
    QVERIFY2(best < 150, qPrintable(QString::number(best)));
  }

  void triangulationAreaMatchesFootprints() {
    QVERIFY(!osm_.empty());
    int failures = 0, tested = 0;
    for (const GeoFeature& f : osm_) {
      if (f.kind != FeatureKind::Building) continue;
      std::vector<uint32_t> tris;
      const bool ok = triangulateRing(f.ring, &tris);
      if (!ok) ++failures;
      double sum = 0;
      for (size_t t = 0; t + 2 < tris.size(); t += 3) {
        std::vector<Pt> tri = {f.ring[tris[t]], f.ring[tris[t + 1]], f.ring[tris[t + 2]]};
        sum += std::abs(signedArea(tri));
      }
      const double area = std::abs(signedArea(f.ring));
      ++tested;
      if (ok) QVERIFY2(std::abs(sum - area) <= std::max(0.02 * area, 0.05), qPrintable(f.id));
    }
    qInfo() << "삼각분할 시험" << tested << "동, 실패" << failures;
    QVERIFY(failures * 50 < tested);  // 2% 미만
  }

  void sceneMeshStandsBuildingsOnRealTerrain() {
    QVERIFY(!osm_.empty() && !khs_.empty());
    std::vector<GeoFeature> all = osm_;
    all.insert(all.end(), khs_.begin(), khs_.end());
    const auto mesh = buildSceneMesh(dem_, all);
    qInfo() << "장면: 건물" << mesh->buildings << "(실측" << mesh->heightMeasured << "층수" << mesh->heightFromLevels << "미상"
            << mesh->heightUnknown << ") 유산구역" << mesh->heritageAreas << "점" << mesh->heritagePoints << "건너뜀" << mesh->skipped;
    QVERIFY(mesh->buildings > 1500);
    QVERIFY(mesh->heritagePoints > 30);
    QCOMPARE(mesh->heightMeasured + mesh->heightFromLevels + mesh->heightUnknown, mesh->buildings);
    QVERIFY(mesh->heightUnknown > mesh->heightMeasured);  // 대부분은 높이 태그가 없다 — 숨기지 않고 센다
    // 모든 정점은 지형 최저~최고(+지물 높이) 안에 있다.
    const int stride = SceneMesh::kFloatsPerVertex;
    for (size_t i = 0; i < mesh->vertices.size(); i += stride) {
      const float z = mesh->vertices[i + 2];
      QVERIFY(z > dem_.zMin - 5 && z < dem_.zMax + 200);
    }
    for (uint32_t idx : mesh->indices) QVERIFY(size_t(idx) * stride < mesh->vertices.size());
  }

  void satelliteTilesCoverTheWindow() {
    SatelliteRequest sr;
    sr.centerX = cx_;
    sr.centerY = cy_;
    sr.halfExtentM = 1500;
    sr.texels = 1024;
    sr.cacheDir = cache_.path();
    SatelliteResult res;
    QString err;
    QVERIFY2(loadSatelliteImage(sr, &res, &err), qPrintable(err));
    qInfo() << "위성 출처" << res.sourceName << "대략 타일 단계" << res.zoomLevel;
    QCOMPARE(res.image.width(), 1024);
    QCOMPARE(res.image.height(), 1024);
    // 단색이 아니고, 밝기가 다양하며, 검은 빈칸이 거의 없어야 한다.
    double sum = 0, sum2 = 0;
    int dark = 0;
    const int n = 1024 * 1024;
    for (int y = 0; y < 1024; ++y) {
      const uchar* line = res.image.constScanLine(y);
      for (int x = 0; x < 1024; ++x) {
        const double l = 0.299 * line[x * 3] + 0.587 * line[x * 3 + 1] + 0.114 * line[x * 3 + 2];
        sum += l; sum2 += l * l;
        if (l < 8) ++dark;
      }
    }
    const double mean = sum / n, sd = std::sqrt(std::max(0.0, sum2 / n - mean * mean));
    qInfo() << "위성 밝기 평균" << mean << "표준편차" << sd << "검은 픽셀 비율" << double(dark) / n;
    QVERIFY(sd > 15);
    QVERIFY(double(dark) / n < 0.02);
    QVERIFY(mean > 40 && mean < 220);
  }

  void vworldKeyIsCheckedAgainstRealService() {
    const QString key = loadVworldApiKey();
    if (key.isEmpty()) QSKIP("VWorld 키가 이 PC에 없다");
    QString reason;
    const bool ok = checkVworldApiKey(key, &reason);
    qInfo() << "VWorld 키 확인:" << ok << reason;
    QVERIFY(!reason.contains(key));  // 키 값은 사유에 새지 않는다
    if (!ok) QVERIFY(reason.contains("VWorld"));
  }

  void localStrataShpCacheReadsWithoutWriting() {
    const QStringList paths = strataHeritageShpPaths();
    if (paths.isEmpty()) QSKIP("Strata 주변유적 캐시가 이 PC에 없다");
    std::vector<GeoFeature> out;
    QStringList skipped;
    FeatureRequest fr = featureRequest(30000);
    QVERIFY(loadLocalVectors(fr, paths, &out, &skipped));
    qInfo() << "로컬 SHP 파일" << paths.size() << "건너뜀" << skipped.size() << "창 안 지물" << int(out.size());
    for (const GeoFeature& f : out) QVERIFY(std::abs(f.point.x - cx_) <= 30000);
  }
};

QTEST_GUILESS_MAIN(TestStrata3d)
#include "test_strata_3d.moc"
