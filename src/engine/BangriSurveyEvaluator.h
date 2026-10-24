#pragma once

#include <QString>
#include <array>
#include <memory>
#include <vector>

#include "BangriGridEngine.h"
#include "DemGrid.h"
#include "SceneMesh.h"

namespace strata3d {

struct RecommendedTrialTrench {
  QString id;              // 예: "T1", "T2"
  QString targetRoadId;    // 교차 대상 방리 도로 ID
  BangriRoadTier roadTier = BangriRoadTier::InterBlockRoad;
  Pt startPt;              // 트렌치 중심선 시점 (workCrs m)
  Pt endPt;                // 트렌치 중심선 종점 (workCrs m)
  double widthM = 2.0;     // 트렌치 폭 (기본 2.0 m)
  double lengthM = 18.0;   // 트렌치 길이 (15.0 ~ 22.0 m)
  double azimuthDeg = 0.0; // 트렌치 방향 방위각
  double intersectionAngleDeg = 90.0; // 방리 도로축과의 교차 각도 (90.0° 직교)
  std::array<Pt, 4> corners;          // 2.0m × lengthM 사각형 폴리곤
  QString purposeNote;                // 예: "방간 중로(8.5m) + 양측 석축 측구(1.2m×2) + 방리담장 기단 동시 확인"
};

struct BangriSurveyEvaluation {
  Pt siteCenter;
  std::vector<Pt> surveyPolygon;
  QString locatedBlockCode;   // 예: "제5방 5행"
  QString locatedSubLotLabel; // 예: "5소획"
  QString nearestRoadId;
  BangriRoadTier nearestRoadTier = BangriRoadTier::InterBlockRoad;
  double distToRoadCenterM = 0.0;
  double distToNearestDitchM = 0.0;
  double distToNearestWallM = 0.0;
  double estimatedRoadAreaInSurveyM2 = 0.0;
  std::vector<RecommendedTrialTrench> recommendedTrenches;
  QString summaryText;
};

class BangriSurveyEvaluator {
 public:
  // 두 방향 벡터(a0->a1, b0->b1) 사이의 교차 예각/직각(0 ~ 90°)을 도(°) 단위로 계산한다.
  static double angleBetweenSegmentsDeg(const Pt& a0, const Pt& a1, const Pt& b0, const Pt& b1);

  // 조사구역 중심점(또는 다각형)이 속한 방리 위치(몇 방 몇 행 · 몇 소획)를 판정하고,
  // 방리 도로·측구·방리담장 기단을 한 번의 단면으로 확인하는 90° 직교 시굴 트렌치(T1, T2)를 자동 설계한다.
  static BangriSurveyEvaluation evaluateSurvey(const BangriGridResult& grid,
                                               const Pt& siteCenter,
                                               const std::vector<Pt>& surveyPolygon = {});

  // 조사구역 경계 및 추천 직교 시굴 트렌치(T1, T2)를 3D SceneMesh 로 생성한다.
  static std::shared_ptr<SceneMesh> buildSurveyTrenchesOverlay(
      const DemGrid& dem,
      const BangriSurveyEvaluation& eval);
};

}  // namespace strata3d
