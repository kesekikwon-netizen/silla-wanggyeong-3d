#pragma once

#include <cstdint>
#include <vector>

#include "GeoFeature.h"

namespace strata3d {

// 단순 다각형(구멍 없음)을 귀 자르기로 삼각분할한다. 방향(시계/반시계)은 상관없다.
// 결과는 ring 정점 인덱스 세 개씩. 자기 교차 등으로 끝내지 못하면 false(가능한 만큼은 채운다).
bool triangulateRing(const std::vector<Pt>& ring, std::vector<uint32_t>* trianglesOut);

// 부호 있는 넓이(반시계 +).
double signedArea(const std::vector<Pt>& ring);

}  // namespace strata3d
