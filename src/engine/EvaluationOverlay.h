#pragma once

#include <memory>

#include "Archaeology3dEvaluator.h"
#include "SceneMesh.h"

namespace strata3d {

// 평가 결과를 화면에 겹칠 띠·기둥: 반경 버퍼 고리(500 m 파랑, 1000 m 보라), 조사지(초록), 시선선은 그리지 않는다.
// 모두 지형에 붙어 깔리고 DEM 밖 부분이 있는 고리는 건너뛴다.
std::shared_ptr<const SceneMesh> buildEvaluationOverlay(const DemGrid& dem, const Evaluation& ev);

// 시굴 트렌치 같은 선형 조사지의 지형 단면 선을 지형에 붙는 띠로(주황).
std::shared_ptr<const SceneMesh> buildProfileOverlay(const DemGrid& dem, const std::vector<ProfilePoint>& profile);

}  // namespace strata3d
