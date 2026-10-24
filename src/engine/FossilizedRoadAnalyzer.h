#pragma once

#include <memory>
#include <vector>

#include "BangriGridEngine.h"
#include "DemGrid.h"
#include "GeoFeature.h"
#include "SceneMesh.h"

namespace strata3d {

struct FossilizedSegment {
  Pt a;
  Pt b;
  double lengthM = 0.0;
  double angleErrorDeg = 0.0;      // 방리축 대비 각도 편차 (0 ~ 2.5°)
  double distToGridRoadM = 0.0;    // 가장 가까운 방리 도로 중심선까지의 수직 거리 (0 ~ 8.0 m)
  QString matchedGridRoadId;
};

struct FossilizationResult {
  std::vector<FossilizedSegment> matchedSegments;
  double matchedLengthM = 0.0;
  double totalEvaluatedGridLengthM = 0.0;
  double fossilizationScorePct = 0.0; // 화석화 일치율 (0 ~ 100 %)
  int alignedBuildingEdges = 0;
};

class FossilizedRoadAnalyzer {
 public:
  // 각도(도, 진북 기준 시계방향) 두 값의 mod 90° 직교 격자 축 편차(0 ~ 45°)를 반환한다.
  static double orthogonalAngleDiffDeg(double azDeg1, double azDeg2);

  // 현대 OSM 지물(도로 선형 및 필지/건물 전면 경계선)과 방리 격자 도로축을 비교하여
  // 각도 오차 <= maxAngleDiffDeg (기본 2.5°), 격자 도로와의 거리 <= maxDistM (기본 8.0 m)인
  // 화석화(Fossilized Urban Grid) 구간과 일치율(%)을 산출한다.
  static FossilizationResult analyze(const BangriGridResult& grid,
                                     const std::vector<GeoFeature>& features,
                                     double maxAngleDiffDeg = 2.5,
                                     double maxDistM = 8.0);

  // 화석화 일치 구간을 밝은 청록색(Cyan) 하이라이트 3D 리본으로 생성한다.
  static std::shared_ptr<SceneMesh> buildFossilizedOverlay(
      const DemGrid& dem,
      const FossilizationResult& result);
};

}  // namespace strata3d
