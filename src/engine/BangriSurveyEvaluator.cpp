#include "BangriSurveyEvaluator.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace strata3d {

double BangriSurveyEvaluator::angleBetweenSegmentsDeg(
    const Pt& a0, const Pt& a1, const Pt& b0, const Pt& b1) {
  const double ax = a1.x - a0.x, ay = a1.y - a0.y;
  const double bx = b1.x - b0.x, by = b1.y - b0.y;
  const double la = std::hypot(ax, ay);
  const double lb = std::hypot(bx, by);
  if (la < 1e-9 || lb < 1e-9) return 0.0;
  const double cosVal = std::clamp(std::abs(ax * bx + ay * by) / (la * lb), 0.0, 1.0);
  return std::acos(cosVal) * 180.0 / M_PI;
}

namespace {

Pt closestPointOnSegment(const Pt& m, const Pt& p, const Pt& q, double* distOut = nullptr) {
  const double vx = q.x - p.x;
  const double vy = q.y - p.y;
  const double lenSq = vx * vx + vy * vy;
  if (lenSq < 1e-9) {
    if (distOut) *distOut = std::hypot(m.x - p.x, m.y - p.y);
    return p;
  }
  const double t = std::clamp(((m.x - p.x) * vx + (m.y - p.y) * vy) / lenSq, 0.0, 1.0);
  const Pt proj{p.x + t * vx, p.y + t * vy};
  if (distOut) *distOut = std::hypot(m.x - proj.x, m.y - proj.y);
  return proj;
}

RecommendedTrialTrench makeOrthogonalTrench(const QString& id,
                                            const BangriRoadSegment& road,
                                            const BangriParams& params,
                                            const Pt& targetPt) {
  RecommendedTrialTrench tr;
  tr.id = id;
  tr.targetRoadId = road.id;
  tr.roadTier = road.tier;
  tr.widthM = 2.0;

  // 도로 폭 + 양측 측구(×2) + 양측 방리담장 기단(×2) + 여유폭(4.5m)을 모두 관통하도록 15~20m 범위로 설정
  const double totalCorridor = road.widthM + 2.0 * (road.ditchWidthM + params.wallBaseWidthM) + 5.0;
  tr.lengthM = std::clamp(totalCorridor, 15.0, 20.0);

  const Pt onRoad = closestPointOnSegment(targetPt, road.a, road.b);
  double dx = road.b.x - road.a.x;
  double dy = road.b.y - road.a.y;
  const double roadLen = std::hypot(dx, dy);
  if (roadLen > 1e-6) {
    dx /= roadLen;
    dy /= roadLen;
  } else {
    dx = 1.0;
    dy = 0.0;
  }

  // 도로 방향 벡터 (dx, dy)에 엄밀히 90° 직교하는 법선 벡터 (nx, ny) = (-dy, dx)
  const double nx = -dy;
  const double ny = dx;

  const double halfL = 0.5 * tr.lengthM;
  const double halfW = 0.5 * tr.widthM;

  tr.startPt = {onRoad.x - halfL * nx, onRoad.y - halfL * ny};
  tr.endPt = {onRoad.x + halfL * nx, onRoad.y + halfL * ny};

  tr.azimuthDeg = std::atan2(tr.endPt.x - tr.startPt.x, tr.endPt.y - tr.startPt.y) * 180.0 / M_PI;
  if (tr.azimuthDeg < 0.0) tr.azimuthDeg += 360.0;

  tr.intersectionAngleDeg = BangriSurveyEvaluator::angleBetweenSegmentsDeg(
      tr.startPt, tr.endPt, road.a, road.b);

  tr.corners = {
      Pt{tr.startPt.x - halfW * dx, tr.startPt.y - halfW * dy},
      Pt{tr.endPt.x - halfW * dx, tr.endPt.y - halfW * dy},
      Pt{tr.endPt.x + halfW * dx, tr.endPt.y + halfW * dy},
      Pt{tr.startPt.x + halfW * dx, tr.startPt.y + halfW * dy}};

  const QString tierLabel = (road.tier == BangriRoadTier::MainAvenue)
                                ? QStringLiteral("간선 대로")
                                : (road.tier == BangriRoadTier::InterBlockRoad
                                       ? QStringLiteral("방간 중로")
                                       : QStringLiteral("방내 井자 소로"));
  tr.purposeNote = QStringLiteral("%1(%2m) 노면 다짐층 + 양측 석축 측구(%3m×2) + 방리담장 기단 90° 직교 단면 확인")
                       .arg(tierLabel)
                       .arg(road.widthM, 0, 'f', 1)
                       .arg(road.ditchWidthM, 0, 'f', 1);
  return tr;
}

}  // namespace

BangriSurveyEvaluation BangriSurveyEvaluator::evaluateSurvey(
    const BangriGridResult& grid,
    const Pt& siteCenter,
    const std::vector<Pt>& surveyPolygon) {
  BangriSurveyEvaluation out;
  out.siteCenter = siteCenter;
  if (surveyPolygon.size() >= 3) {
    out.surveyPolygon = surveyPolygon;
  } else {
    // 기본 50m × 50m 시굴 대상 조사구역 생성
    constexpr double h = 25.0;
    out.surveyPolygon = {
        {siteCenter.x - h, siteCenter.y - h},
        {siteCenter.x + h, siteCenter.y - h},
        {siteCenter.x + h, siteCenter.y + h},
        {siteCenter.x - h, siteCenter.y + h}};
  }

  const BangriParams& params = grid.params;
  const Pt localUV = BangriGridEngine::worldToGridLocal(params, siteCenter.x, siteCenter.y);
  const double W = std::max(20.0, params.blockWidthM);
  const double H = std::max(20.0, params.blockHeightM);

  const int cIdx = int(std::floor(localUV.x / W));
  const int rIdx = int(std::floor(localUV.y / H));
  const int cMin = -std::clamp(params.extentCols, 1, 32) / 2;
  const int rMin = -std::clamp(params.extentRows, 1, 32) / 2;
  const int dispCol = (cIdx - cMin) + 1;
  const int dispRow = (rIdx - rMin) + 1;
  out.locatedBlockCode = QStringLiteral("제%1방 %2행").arg(dispCol).arg(dispRow);

  const int subDiv = (params.subdivisionMode == BangriSubdivisionMode::Well9)
                         ? 3
                         : (params.subdivisionMode == BangriSubdivisionMode::Cross4 ? 2 : 1);
  const double fracU = std::clamp((localUV.x - cIdx * W) / W, 0.0, 0.9999);
  const double fracV = std::clamp((localUV.y - rIdx * H) / H, 0.0, 0.9999);
  const int subC = int(fracU * subDiv);
  const int subR = int(fracV * subDiv);
  const int subNo = subR * subDiv + subC + 1;
  out.locatedSubLotLabel = QStringLiteral("%1소획").arg(subNo);

  // 가장 가까운 남북 도로 및 동서 도로 탐색
  const BangriRoadSegment* bestRoad = nullptr;
  const BangriRoadSegment* bestOrthoRoad = nullptr;
  double bestDist = 1e18;
  double bestOrthoDist = 1e18;

  for (const BangriRoadSegment& r : grid.roads) {
    double d = 0.0;
    closestPointOnSegment(siteCenter, r.a, r.b, &d);
    if (d < bestDist) {
      bestDist = d;
      bestRoad = &r;
    }
  }

  if (bestRoad) {
    for (const BangriRoadSegment& r : grid.roads) {
      if (r.isNorthSouth == bestRoad->isNorthSouth) continue;
      double d = 0.0;
      closestPointOnSegment(siteCenter, r.a, r.b, &d);
      if (d < bestOrthoDist) {
        bestOrthoDist = d;
        bestOrthoRoad = &r;
      }
    }

    out.nearestRoadId = bestRoad->id;
    out.nearestRoadTier = bestRoad->tier;
    out.distToRoadCenterM = bestDist;

    double dLeftDitch = 0.0, dRightDitch = 0.0;
    closestPointOnSegment(siteCenter, bestRoad->leftDitchA, bestRoad->leftDitchB, &dLeftDitch);
    closestPointOnSegment(siteCenter, bestRoad->rightDitchA, bestRoad->rightDitchB, &dRightDitch);
    out.distToNearestDitchM = std::min(dLeftDitch, dRightDitch);

    double dLeftWall = 0.0, dRightWall = 0.0;
    closestPointOnSegment(siteCenter, bestRoad->leftWallA, bestRoad->leftWallB, &dLeftWall);
    closestPointOnSegment(siteCenter, bestRoad->rightWallA, bestRoad->rightWallB, &dRightWall);
    out.distToNearestWallM = std::min(dLeftWall, dRightWall);

    const double corridorW = bestRoad->widthM + 2.0 * (bestRoad->ditchWidthM + params.wallBaseWidthM);
    out.estimatedRoadAreaInSurveyM2 = std::max(12.0, corridorW * 42.0 * std::exp(-bestDist / 60.0));

    // T1: 최단 거리 방리 도로 및 양측 측구 직교(90°) 트렌치
    out.recommendedTrenches.push_back(makeOrthogonalTrench(QStringLiteral("T1"), *bestRoad, params, siteCenter));

    // T2: 교차하는 직교 방향 도로 및 측구 확인용 트렌치
    if (bestOrthoRoad) {
      out.recommendedTrenches.push_back(makeOrthogonalTrench(QStringLiteral("T2"), *bestOrthoRoad, params, siteCenter));
    }
  }

  out.summaryText = QStringLiteral("조사구역 위치: %1 [%2] · 최근접 방리도로(%3) 중심선까지 %4 m, 측구까지 %5 m · 추천 직교 시굴트렌치 %6기(T1 폭 2.0m×길이 %7m, 교차각 %8°)")
                        .arg(out.locatedBlockCode, out.locatedSubLotLabel, out.nearestRoadId)
                        .arg(out.distToRoadCenterM, 0, 'f', 1)
                        .arg(out.distToNearestDitchM, 0, 'f', 1)
                        .arg(int(out.recommendedTrenches.size()))
                        .arg(out.recommendedTrenches.empty() ? 0.0 : out.recommendedTrenches.front().lengthM, 0, 'f', 1)
                        .arg(out.recommendedTrenches.empty() ? 90.0 : out.recommendedTrenches.front().intersectionAngleDeg, 0, 'f', 1);

  return out;
}

std::shared_ptr<SceneMesh> BangriSurveyEvaluator::buildSurveyTrenchesOverlay(
    const DemGrid& dem,
    const BangriSurveyEvaluation& eval) {
  auto mesh = std::make_shared<SceneMesh>();
  if (!dem.valid()) return mesh;

  // 조사구역 외곽 녹색 띠
  if (eval.surveyPolygon.size() >= 3) {
    appendDrapedRibbon(mesh.get(), dem, eval.surveyPolygon, true, 3.2f, {0.15f, 0.88f, 0.38f, 1.0f}, 3.6f);
  }

  // 추천 시굴 트렌치(T1, T2) 밝은 마젠타-골드 직교 트렌치 박스 및 표지 기둥
  const std::array<float, 4> trenchRibbonCol{1.00f, 0.25f, 0.12f, 1.0f};
  const std::array<float, 4> trenchPoleCol{1.00f, 0.92f, 0.15f, 1.0f};
  for (const RecommendedTrialTrench& tr : eval.recommendedTrenches) {
    appendDrapedRibbon(mesh.get(), dem, {tr.startPt, tr.endPt}, false, float(tr.widthM * 1.4), trenchRibbonCol, 4.4f);
    std::vector<Pt> box(tr.corners.begin(), tr.corners.end());
    appendDrapedRibbon(mesh.get(), dem, box, true, 1.2f, trenchPoleCol, 4.7f);
    appendPole(mesh.get(), dem, tr.startPt, 2.2f, 26.0f, trenchPoleCol);
    appendPole(mesh.get(), dem, tr.endPt, 2.2f, 26.0f, trenchPoleCol);
  }

  return mesh;
}

}  // namespace strata3d
