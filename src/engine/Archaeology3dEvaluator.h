#pragma once

#include <QString>
#include <QStringList>
#include <memory>
#include <vector>

#include "DemGrid.h"
#include "GeoFeature.h"
#include "KdTree3.h"
#include "TerrainBvh.h"

namespace strata3d {

struct NearbyItem {
  QString id, name, category, source;
  FeatureKind kind = FeatureKind::HeritagePoint;
  double distanceM = 0;   // 조사지(점·다각형)에서 지물까지 최단거리. 겹치면 0
  double bearingDeg = 0;  // 조사지 중심 → 지물 대표점, 북 0° 시계 방향
  QString bearingName;    // 16방위
  double elevationM = 0;  // 지물 대표점의 DEM 표고
  double deltaElevM = 0;  // 조사지 대비
};

struct TerrainMetrics {
  double elevationM = 0;
  double slopeDeg = 0;     // Horn 3×3, DEM 칸 크기 기준
  double aspectDeg = -1;   // 내리막 방향, 북 0° 시계 방향. 평지면 -1
  QString aspectName;
  double tpiSmallM = 0;    // 주변 ±150 m 상자 평균 대비 높이
  double tpiLargeM = 0;    // 주변 ±600 m
  double zSmall = 0;       // 표준화(전체 창의 표준편차로 나눔)
  double zLarge = 0;
  QString landform;        // Weiss(2001) 10분류. DEM 형태만으로 가른 참고 분류
};

struct RadiusResult {
  double radiusM = 0;
  double bufferAreaM2 = 0;
  double bufferMaxReachM = 0;  // 조사지에서 버퍼 꼭짓점까지 최대 거리(둥근 버퍼면 반경과 거의 같다)
  std::vector<Pt> bufferRing;
  std::vector<NearbyItem> heritage;  // 가까운 순
  int buildings = 0;
};

struct VisibilityItem {
  QString name, category;
  double distanceM = 0;
  bool visible = false;
  double blockedAtM = 0;  // 가려졌다면 시선이 지형에 닿은 거리
};

struct Evaluation {
  Pt site;
  std::vector<Pt> sitePolygon;
  TerrainMetrics terrain;
  std::vector<RadiusResult> radii;
  std::vector<VisibilityItem> visibility;
  QStringList notes;  // 계산에 쓰지 못한 것, 가정 등
  QString summary;    // 한국어 요약(계산값만 서술)
};

struct ProfilePoint {
  double distanceM = 0;
  double x = 0, y = 0;
  double elevationM = 0;
};

// 조사지 하나(점 또는 다각형)를 실측 DEM·실제 지물로 평가한다. 층서·유물 같은 DEM 밖의 사실은 만들지 않는다.
class Archaeology3dEvaluator {
 public:
  Archaeology3dEvaluator(const DemGrid& dem, std::vector<GeoFeature> features);

  Evaluation evaluate(const Pt& site, const std::vector<Pt>& sitePolygon = {},
                      const std::vector<double>& radiiM = {500.0, 1000.0}) const;
  // 두 점 사이 지형 단면(시굴 트렌치의 지표면 윤곽). step 간격 표본.
  std::vector<ProfilePoint> terrainProfile(const Pt& a, const Pt& b, double stepM) const;
  // 눈(지상 높이 포함)에서 목표까지 시선이 지형에 가리는지. 가리면 닿은 거리.
  bool lineOfSight(const Pt& from, double fromAboveGround, const Pt& to, double toAboveGround, double* blockedAtM) const;

  const TerrainBvh& terrain() const { return *bvh_; }
  const DemGrid& dem() const { return *dem_; }
  const std::vector<GeoFeature>& features() const { return features_; }

 private:
  TerrainMetrics terrainAt(const Pt& p) const;

  std::shared_ptr<DemGrid> dem_;
  std::shared_ptr<const TerrainBvh> bvh_;
  std::vector<GeoFeature> features_;
  KdTree3 tree_;
  double maxReachM_ = 0;
  std::vector<float> tpiSmall_, tpiLarge_;
  double sdSmall_ = 1, sdLarge_ = 1;
};

// 북 0° 시계 방향 각도 → "북", "북북동", … 16방위
QString bearingName16(double deg);

}  // namespace strata3d
