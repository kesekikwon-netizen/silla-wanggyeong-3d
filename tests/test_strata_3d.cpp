// strata_3d_engine 시험. 모든 격자는 실제 Copernicus GLO-30 COG(/vsicurl/)에서 GDAL 로 읽는다.
// 네트워크가 막히면 시험은 건너뛰지 않고 실패한다(가짜 값으로 통과시키지 않기 위해).
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QMatrix4x4>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QVector2D>
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <vector>

#include "CrsTransform.h"
#include "DemProvider.h"
#include "Archaeology3dEvaluator.h"
#include "Frustum.h"
#include "Kdop14Bvh.h"
#include "KdTree3.h"
#include "KhsProvider.h"
#include "TerrainBvh.h"
#include "LocalVectorProvider.h"
#include "OsmProvider.h"
#include "SatelliteProvider.h"
#include "SceneMesh.h"
#include "StrataSurvey.h"
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

  void khsNearbyWorksNationwide() {
    // 전국 목록에서 창 안만 고른다: 경주 6 km 창에서 경북만 받은 결과(khs_)와 같은 수·같은 id 여야 하고,
    // 경주와 먼 다른 지역 창에서도 지정유산이 나와야 한다(경북·경주 고정이 아님).
    std::vector<GeoFeature> nat;
    QString err;
    QVERIFY2(loadKhsHeritageNearby(featureRequest(6000), &nat, &err), qPrintable(err));
    QCOMPARE(nat.size(), khs_.size());
    FeatureRequest seoul = featureRequest(3000);
    double sx = 126.9769, sy = 37.5796;  // 경복궁 부근(WGS84)
    QVERIFY(transformPoint("EPSG:4326", "EPSG:5187", &sx, &sy));
    seoul.centerX = sx;
    seoul.centerY = sy;
    std::vector<GeoFeature> far;
    QVERIFY2(loadKhsHeritageNearby(seoul, &far, &err), qPrintable(err));
    qInfo() << "서울 경복궁 3 km 안 국가유산청" << int(far.size()) << "건";
    QVERIFY(far.size() > 10);
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

  // ---- BVH · k-d 트리 · 평가 -----------------------------------------------------------------
  void kdopAxesMatchBlenderTable() {
    const auto& ax = Kdop14::axes();
    const float expect[7][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 1}, {1, -1, 1}, {1, 1, -1}, {1, -1, -1}};
    for (int i = 0; i < 7; ++i)
      for (int j = 0; j < 3; ++j) QCOMPARE(ax[i][j], expect[i][j]);
    // Blender 의 BLI_bvhtree_balance 분기 노드 수 식 (디스어셈블리에서 확인): max(1, (n-3+4)/3)
    QCOMPARE(Kdop14Bvh::expectedBranchNodes(1), 1);
    QCOMPARE(Kdop14Bvh::expectedBranchNodes(100), 33);
  }

  void terrainBvhMatchesBruteForce() {
    const auto bvh = TerrainBvh::build(dem_);
    QVERIFY(bvh);
    qInfo() << "지형 BVH 타일" << bvh->bvh().leafCount() << "노드" << bvh->bvh().nodeCount() << "깊이" << bvh->bvh().depth();
    QRandomGenerator rng(12345);  // 시험 질의를 고르는 데만 쓴다(자료가 아님)
    auto rnd = [&rng](double a, double b) { return a + (b - a) * rng.generateDouble(); };
    const float half = float(dem_.width * dem_.pixelSize * 0.45);
    int hits = 0, misses = 0;
    qint64 bvhNs = 0, bruteNs = 0;
    for (int i = 0; i < 160; ++i) {
      const QVector3D o(float(rnd(-half, half)), float(rnd(-half, half)), dem_.zMax + float(rnd(50.0, 600.0)));
      QVector3D d(float(rnd(-1.0, 1.0)), float(rnd(-1.0, 1.0)), float(rnd(-1.0, -0.02)));
      d.normalize();
      TerrainBvh::Hit a, b;
      QElapsedTimer t;
      t.start();
      const bool ha = bvh->rayCast(o, d, 40000.0f, &a);
      bvhNs += t.nsecsElapsed();
      t.restart();
      const bool hb = bvh->rayCastBruteForce(o, d, 40000.0f, &b);
      bruteNs += t.nsecsElapsed();
      QCOMPARE(ha, hb);
      if (ha) {
        ++hits;
        QVERIFY2(std::abs(a.t - b.t) < 0.01f, qPrintable(QString("%1 vs %2").arg(a.t).arg(b.t)));
      } else {
        ++misses;
      }
    }
    qInfo() << "광선 160개: 맞음" << hits << "빗나감" << misses << "BVH" << bvhNs / 1e6 << "ms, 전수" << bruteNs / 1e6 << "ms";
    QVERIFY(hits > 100);
    QVERIFY(bvhNs * 20 < bruteNs);  // 전수 검사보다 20배 이상 빨라야 의미가 있다
  }

  void kdTreeMatchesBruteForce() {
    QVERIFY(!khs_.empty());
    std::vector<KdTree3::Entry> entries;
    for (size_t i = 0; i < khs_.size(); ++i)
      entries.push_back({QVector3D(float(khs_[i].point.x - cx_), float(khs_[i].point.y - cy_), 0), uint32_t(i)});
    KdTree3 tree;
    tree.build(entries);
    QRandomGenerator rng(777);
    auto rnd = [&rng](double a, double b) { return a + (b - a) * rng.generateDouble(); };
    for (int q = 0; q < 200; ++q) {
      const QVector3D c(float(rnd(-5000.0, 5000.0)), float(rnd(-5000.0, 5000.0)), 0);
      const float r = float(rnd(100.0, 1500.0));
      std::vector<uint32_t> expect;
      float bestD = 1e30f;
      uint32_t bestId = 0;
      for (const auto& e : entries) {
        const float d = (e.p - c).length();
        if (d <= r) expect.push_back(e.id);
        if (d < bestD) { bestD = d; bestId = e.id; }
      }
      auto got = tree.radiusSearch(c, r);
      std::vector<uint32_t> gotIds;
      for (auto& p : got) gotIds.push_back(p.first);
      std::sort(expect.begin(), expect.end());
      std::sort(gotIds.begin(), gotIds.end());
      QCOMPARE(gotIds, expect);
      uint32_t nid = 0;
      float nd = 0;
      QVERIFY(tree.nearest(c, &nid, &nd));
      QVERIFY(std::abs(nd - bestD) < 1e-3f);
      Q_UNUSED(bestId);
    }
  }

  void evaluatorBuffersAreRoundAndDistancesAgree() {
    QVERIFY(!osm_.empty() && !khs_.empty());
    std::vector<GeoFeature> all = osm_;
    all.insert(all.end(), khs_.begin(), khs_.end());
    Archaeology3dEvaluator ev(dem_, all);
    const Pt site{cx_, cy_};
    const Evaluation e = ev.evaluate(site);
    QCOMPARE(int(e.radii.size()), 2);
    for (const RadiusResult& rr : e.radii) {
      const double circle = M_PI * rr.radiusM * rr.radiusM;
      qInfo() << "반경" << rr.radiusM << "버퍼 넓이비" << rr.bufferAreaM2 / circle << "최대 도달" << rr.bufferMaxReachM
              << "유산" << int(rr.heritage.size()) << "건물" << rr.buildings;
      QVERIFY(rr.bufferAreaM2 / circle > 0.99 && rr.bufferAreaM2 / circle <= 1.0);
      QVERIFY(std::abs(rr.bufferMaxReachM - rr.radiusM) < rr.radiusM * 0.002);  // 707 m(Miter)로 튀지 않는다
      double prev = -1;
      for (const NearbyItem& it : rr.heritage) {
        QVERIFY(it.distanceM <= rr.radiusM + 1e-6);
        QVERIFY(it.distanceM >= prev);
        prev = it.distanceM;
      }
    }
    QVERIFY(e.radii[1].heritage.size() >= e.radii[0].heritage.size());
    QVERIFY(e.radii[1].heritage.size() >= 3);
    // 독립 검산: 가장 가까운 국가유산청 점의 거리를 위경도 → 측지 거리(하버사인)로 다시 구해 비교한다.
    const NearbyItem* near = nullptr;
    for (const NearbyItem& it : e.radii[1].heritage)
      if (it.source == "KHS") { near = &it; break; }
    QVERIFY(near);
    double fx = 0, fy = 0;
    for (const GeoFeature& f : khs_) if (f.id == near->id) { fx = f.point.x; fy = f.point.y; }
    double lon1 = cx_, lat1 = cy_, lon2 = fx, lat2 = fy;
    QVERIFY(transformPoint("EPSG:5187", "EPSG:4326", &lon1, &lat1));
    QVERIFY(transformPoint("EPSG:5187", "EPSG:4326", &lon2, &lat2));
    const double R = 6371008.8, rad = M_PI / 180.0;
    const double a = std::pow(std::sin((lat2 - lat1) * rad / 2), 2) + std::cos(lat1 * rad) * std::cos(lat2 * rad) * std::pow(std::sin((lon2 - lon1) * rad / 2), 2);
    const double geodesic = 2 * R * std::asin(std::sqrt(a));
    qInfo() << "가장 가까운 KHS" << near->name << near->distanceM << "m, 측지 거리" << geodesic << "m, 방위" << near->bearingName;
    QVERIFY(std::abs(geodesic - near->distanceM) < 0.003 * near->distanceM + 0.5);
  }

  void evaluatorPolygonBufferHasNoMiterSpike() {
    const double h = 10;
    const std::vector<Pt> square = {{cx_ - h, cy_ - h}, {cx_ + h, cy_ - h}, {cx_ + h, cy_ + h}, {cx_ - h, cy_ + h}};
    Archaeology3dEvaluator ev(dem_, khs_);
    const Evaluation e = ev.evaluate({cx_, cy_}, square, {500.0});
    const RadiusResult& rr = e.radii[0];
    // 정사각형(반폭 10 m)에서 버퍼 꼭짓점까지의 거리는 모든 점에서 500 m 이어야 한다(Miter 면 대각선 모서리가 707 m).
    for (const Pt& p : rr.bufferRing) {
      const double dx = std::max(std::abs(p.x - cx_) - h, 0.0), dy = std::max(std::abs(p.y - cy_) - h, 0.0);
      QVERIFY2(std::abs(std::hypot(dx, dy) - 500.0) < 1.0, qPrintable(QString::number(std::hypot(dx, dy))));
    }
  }

  void evaluatorTerrainMetricsAreSane() {
    Archaeology3dEvaluator ev(dem_, khs_);
    const Evaluation basin = ev.evaluate({cx_, cy_}, {}, {500.0});
    qInfo() << "첨성대 부근: 고도" << basin.terrain.elevationM << "경사" << basin.terrain.slopeDeg << "향" << basin.terrain.aspectName
            << "분류" << basin.terrain.landform << "zS" << basin.terrain.zSmall << "zL" << basin.terrain.zLarge;
    QVERIFY(basin.terrain.slopeDeg < 5.0);
    QVERIFY(!basin.terrain.landform.isEmpty());
    // 창 안 가장 높은 칸은 산정·능선이어야 한다.
    int bc = 0, br = 0;
    for (int r = 0; r < dem_.height; ++r)
      for (int c = 0; c < dem_.width; ++c)
        if (dem_.at(c, r) > dem_.at(bc, br)) { bc = c; br = r; }
    const Evaluation top = ev.evaluate({dem_.xAt(bc + 0.5), dem_.yAt(br + 0.5)}, {}, {500.0});
    qInfo() << "최고점:" << top.terrain.elevationM << "m" << top.terrain.landform << "zL" << top.terrain.zLarge;
    QVERIFY(top.terrain.elevationM > 800);
    QVERIFY(top.terrain.zLarge > 1.0);
    QVERIFY(top.terrain.slopeDeg > basin.terrain.slopeDeg);
  }

  void evaluatorVisibilityAndProfile() {
    QVERIFY(!khs_.empty());
    Archaeology3dEvaluator ev(dem_, khs_);
    const Evaluation e = ev.evaluate({cx_, cy_}, {}, {1000.0});
    int vis = 0;
    for (const VisibilityItem& v : e.visibility) {
      if (v.visible) ++vis;
      else QVERIFY(v.blockedAtM > 0 && v.blockedAtM < v.distanceM);
    }
    qInfo() << "시야 대상" << int(e.visibility.size()) << "곳 중 보이는 곳" << vis;
    QVERIFY(!e.visibility.empty());
    QVERIFY(vis >= 1);  // 평지 안에서는 가까운 사적이 지형에 가리지 않는다
    const auto prof = ev.terrainProfile({cx_, cy_}, {cx_ + 600, cy_ + 300}, 10);
    QVERIFY(prof.size() > 50);
    float z0 = 0;
    QVERIFY(dem_.sample(cx_, cy_, &z0));
    QVERIFY(std::abs(prof.front().elevationM - z0) < 1e-3);
    QVERIFY(std::abs(prof.back().distanceM - std::hypot(600.0, 300.0)) < 1e-6);
  }

  void strataSurveyIsReadOnlyAndTransformed() {
    const QString src = QStringLiteral(S3D_HGIS_ROOT) + QStringLiteral("/samples/demo_survey/demo.gpkg");
    QVERIFY2(QFile::exists(src), qPrintable(src));
    auto sha = [](const QString& p) {
      QFile f(p);
      (void)f.open(QIODevice::ReadOnly);
      return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256);
    };
    const QByteArray before = sha(src);
    StrataSurvey s;
    QString err;
    QVERIFY2(loadStrataSurvey(src, "auto", &s, &err), qPrintable(err));
    QCOMPARE(s.workCrs, QString("EPSG:5186"));  // 표본 파일은 중부원점(5186)이다 — 파일의 좌표계를 따른다
    qInfo() << "조사 레이어" << s.presentLayers << "중심" << s.center.x << s.center.y << "반폭" << s.halfExtentM;
    QVERIFY(s.presentLayers.contains("survey_area"));
    QVERIFY(s.presentLayers.contains("trial_trench"));
    QCOMPARE(int(s.surveyAreas.size()), 1);
    QCOMPARE(int(s.surveyAreas[0].size()), 4);  // 닫는 점은 뺀다
    QVERIFY(std::abs(s.center.x - 200050.0) < 100 && std::abs(s.center.y - 450045.0) < 100);
    // 5187 로 강제하면 같은 지점이 옮겨져야 한다: 경도 2° 차이만큼 X 가 크게 달라진다(변환이 실제로 일어난다).
    StrataSurvey s187;
    QVERIFY2(loadStrataSurvey(src, "EPSG:5187", &s187, &err), qPrintable(err));
    QVERIFY(std::abs(s187.center.x - s.center.x) > 100000);
    double rx = s187.center.x, ry = s187.center.y;
    QVERIFY(transformPoint("EPSG:5187", "EPSG:5186", &rx, &ry));
    QVERIFY(std::abs(rx - s.center.x) < 0.01 && std::abs(ry - s.center.y) < 0.01);
    // 5179 로 옮긴 뒤 되돌린 값과 같아야 한다.
    StrataSurvey s179;
    QVERIFY2(loadStrataSurvey(src, "EPSG:5179", &s179, &err), qPrintable(err));
    double bx = s179.center.x, by = s179.center.y;
    QVERIFY(transformPoint("EPSG:5179", "EPSG:5186", &bx, &by));
    QVERIFY(std::abs(bx - s.center.x) < 0.01 && std::abs(by - s.center.y) < 0.01);
    // 원본은 그대로: 내용 해시 같고 -wal/-shm 도 안 남는다.
    QCOMPARE(sha(src), before);
    QVERIFY(!QFile::exists(src + "-wal") && !QFile::exists(src + "-shm"));
    QVERIFY(!loadStrataSurvey(cache_.path() + "/없는파일.gpkg", "EPSG:5187", &s, &err));
    QVERIFY(!err.isEmpty());
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
