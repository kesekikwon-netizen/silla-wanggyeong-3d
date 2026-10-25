#pragma once

#include <array>
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
  // 건물은 바닥 외곽선(지형에 붙는 띠)만 그린다. 높이는 OSM 태그일 뿐 측량이 아니고, 태그가 없으면
  // 추정 높이로 세워야 하므로 세우지 않는다(설계도 v3 10장). false 면 예전처럼 세운다(시험용).
  bool footprintOnly = true;
  float footprintWidthM = 1.2f;
  // 유산·구역 면은 지형 텍스처에 칠한다(DrapeRaster). true 면 띠로 세우지 않고 세기만 한다.
  bool drapeAreas = true;
  float unknownHeightM = 6.0f;  // footprintOnly=false 일 때 높이 태그가 없는 건물의 추정 높이(실측 아님)
  float markerHeightM = 45.0f;  // 점 지물 표지 기둥 높이
  float markerWidthM = 5.0f;
  float ribbonWidthM = 3.0f;
};

// 경계(닫힌 고리 또는 열린 선)를 지형에 붙는 띠로 덧붙인다. DEM 밖 점이 있으면 false.
bool appendDrapedRibbon(SceneMesh* mesh, const DemGrid& dem, const std::vector<Pt>& line, bool closed, float widthM,
                        const std::array<float, 4>& color, float liftM = 1.0f);
// 땅에서 솟은 사각 기둥 표지. DEM 밖이면 false.
bool appendPole(SceneMesh* mesh, const DemGrid& dem, const Pt& at, float widthM, float heightM, const std::array<float, 4>& color);

// 표고는 DEM 에서 읽는다(범위 밖이면 그 지물은 건너뜀). 좌표는 DEM 중심 기준 지역 미터.
std::shared_ptr<const SceneMesh> buildSceneMesh(const DemGrid& dem, const std::vector<GeoFeature>& features,
                                                const SceneStyle& style = {});

}  // namespace strata3d
