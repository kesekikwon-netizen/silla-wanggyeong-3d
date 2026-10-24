#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "DemGrid.h"
#include "GeoFeature.h"

namespace strata3d {

// 건물·유적을 한 덩어리 삼각형 목록으로 만든 것. 정점 10 float = 위치 3, 법선 3, 색 4.
struct SceneMesh {
  static constexpr int kFloatsPerVertex = 10;
  std::vector<float> vertices;
  std::vector<uint32_t> indices;
  int buildings = 0;
  int heightMeasured = 0;   // 태그 height
  int heightFromLevels = 0; // building:levels × 3 m
  int heightUnknown = 0;    // 추정 높이로 세운 것(색을 달리 칠한다)
  int heritageAreas = 0;
  int heritagePoints = 0;
  int skipped = 0;          // DEM 밖이거나 도형이 깨져 못 세운 것
  int triangulationFailures = 0;
};

struct SceneStyle {
  float unknownHeightM = 6.0f;  // 높이 태그가 없는 건물을 세울 때의 추정 높이(실측 아님)
  float markerHeightM = 45.0f;  // 점 지물 표지 기둥 높이
  float markerWidthM = 5.0f;
  float ribbonWidthM = 3.0f;
};

// 표고는 DEM 에서 읽는다(범위 밖이면 그 지물은 건너뜀). 좌표는 DEM 중심 기준 지역 미터.
std::shared_ptr<const SceneMesh> buildSceneMesh(const DemGrid& dem, const std::vector<GeoFeature>& features,
                                                const SceneStyle& style = {});

}  // namespace strata3d
