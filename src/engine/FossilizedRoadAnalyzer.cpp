#include "FossilizedRoadAnalyzer.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace strata3d {

double FossilizedRoadAnalyzer::orthogonalAngleDiffDeg(double azDeg1, double azDeg2) {
  double d = std::fmod(azDeg1 - azDeg2, 90.0);
  if (d < 0.0) d += 90.0;
  if (d > 45.0) d = 90.0 - d;
  return std::abs(d);
}

namespace {

// 선분 PQ 와 점 M 사이의 최소 거리
double pointToSegmentDist(const Pt& m, const Pt& p, const Pt& q) {
  const double vx = q.x - p.x;
  const double vy = q.y - p.y;
  const double lenSq = vx * vx + vy * vy;
  if (lenSq < 1e-9) return std::hypot(m.x - p.x, m.y - p.y);
  const double t = std::clamp(((m.x - p.x) * vx + (m.y - p.y) * vy) / lenSq, 0.0, 1.0);
  const double projX = p.x + t * vx;
  const double projY = p.y + t * vy;
  return std::hypot(m.x - projX, m.y - projY);
}

}  // namespace

FossilizationResult FossilizedRoadAnalyzer::analyze(
    const BangriGridResult& grid,
    const std::vector<GeoFeature>& features,
    double maxAngleDiffDeg,
    double maxDistM) {
  FossilizationResult out;
  if (grid.roads.empty()) return out;

  const BangriParams& params = grid.params;
  const double gridSpanU = 0.55 * params.extentCols * params.blockWidthM + 100.0;
  const double gridSpanV = 0.55 * params.extentRows * params.blockHeightM + 100.0;

  // 방리 격자 간선·중로·소로 중 총 기준 연장 계산
  for (const BangriRoadSegment& r : grid.roads) {
    const double len = std::hypot(r.b.x - r.a.x, r.b.y - r.a.y);
    if (r.tier != BangriRoadTier::InnerAlley) {
      out.totalEvaluatedGridLengthM += len;
    } else {
      out.totalEvaluatedGridLengthM += 0.35 * len;
    }
  }
  if (out.totalEvaluatedGridLengthM < 1.0) out.totalEvaluatedGridLengthM = 1.0;

  // OSM 건물·유적 다각형 외곽 변(필지·도로 경계선)에서 6m 이상 직선 세그먼트 추출
  for (const GeoFeature& f : features) {
    if (f.ring.size() < 2) continue;
    const Pt refPt = (f.point.x == 0.0 && f.point.y == 0.0) ? f.ring.front() : f.point;
    const Pt localCenter = BangriGridEngine::worldToGridLocal(params, refPt.x, refPt.y);
    if (std::abs(localCenter.x) > gridSpanU || std::abs(localCenter.y) > gridSpanV) continue;

    const size_t n = f.ring.size();
    for (size_t i = 0; i < n; ++i) {
      const Pt& p0 = f.ring[i];
      const Pt& p1 = f.ring[(i + 1) % n];
      const double segLen = std::hypot(p1.x - p0.x, p1.y - p0.y);
      if (segLen < 6.0) continue;

      // 진북 기준 시계방향 방위각(도)
      const double segAzDeg = std::atan2(p1.x - p0.x, p1.y - p0.y) * 180.0 / M_PI;
      const double angDiff = orthogonalAngleDiffDeg(segAzDeg, params.azimuthDeg);
      if (angDiff > maxAngleDiffDeg) continue;

      ++out.alignedBuildingEdges;

      const Pt mid{0.5 * (p0.x + p1.x), 0.5 * (p0.y + p1.y)};
      double bestDist = 1e9;
      QString bestRoadId;

      for (const BangriRoadSegment& r : grid.roads) {
        // 도로 외곽(양측 측구·담장선 포함) 또는 중심선과의 거리 평가
        const double dCenter = pointToSegmentDist(mid, r.a, r.b);
        const double halfRoadCorridor = 0.5 * r.widthM + r.ditchWidthM + params.wallBaseWidthM;
        const double effectiveDist = std::max(0.0, dCenter - halfRoadCorridor);
        if (effectiveDist < bestDist) {
          bestDist = effectiveDist;
          bestRoadId = r.id;
        }
      }

      if (bestDist <= maxDistM) {
        FossilizedSegment fs;
        fs.a = p0;
        fs.b = p1;
        fs.lengthM = segLen;
        fs.angleErrorDeg = angDiff;
        fs.distToGridRoadM = bestDist;
        fs.matchedGridRoadId = bestRoadId;
        out.matchedLengthM += segLen;
        out.matchedSegments.push_back(std::move(fs));
      }
    }
  }

  // 화석화 점수(%): 매칭된 필지·도로변 총 연장과 격자 기준 연장의 비율 (최대 98.5% 캡)
  const double rawRatio = (out.matchedLengthM / (out.totalEvaluatedGridLengthM * 0.45)) * 100.0;
  out.fossilizationScorePct = std::clamp(rawRatio, 0.0, 98.5);
  return out;
}

std::shared_ptr<SceneMesh> FossilizedRoadAnalyzer::buildFossilizedOverlay(
    const DemGrid& dem,
    const FossilizationResult& result) {
  auto mesh = std::make_shared<SceneMesh>();
  if (!dem.valid()) return mesh;

  const std::array<float, 4> cyanHighlight{0.05f, 0.92f, 0.88f, 1.0f};
  for (const FossilizedSegment& seg : result.matchedSegments) {
    appendDrapedRibbon(mesh.get(), dem, {seg.a, seg.b}, false, 3.2f, cyanHighlight, 4.2f);
  }
  return mesh;
}

}  // namespace strata3d
