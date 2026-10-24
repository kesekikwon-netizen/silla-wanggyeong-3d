#pragma once

#include <memory>

#include "Archaeology3dEvaluator.h"
#include "SceneMesh.h"
#include "StrataSurvey.h"

namespace strata3d {

// 평가 결과를 화면에 겹칠 띠·기둥: 반경 버퍼 고리(500 m 파랑, 1000 m 보라), 조사지(초록), 시선선은 그리지 않는다.
// 모두 지형에 붙어 깔리고 DEM 밖 부분이 있는 고리는 건너뛴다.
std::shared_ptr<const SceneMesh> buildEvaluationOverlay(const DemGrid& dem, const Evaluation& ev);

// Strata 조사 프로젝트의 도메인 레이어(조사구역 초록, 트렌치 주황, 유구 면 빨강, 유구 선·단면선 청록, 기준점·유물점 기둥)를 지형에 붙여 보여 준다.
std::shared_ptr<const SceneMesh> buildSurveyOverlay(const DemGrid& dem, const StrataSurvey& survey);

// src 의 삼각형을 dst 뒤에 덧붙인다.
void appendMesh(SceneMesh* dst, const SceneMesh& src);

// 시굴 트렌치 같은 선형 조사지의 지형 단면 선을 지형에 붙는 띠로(주황).
std::shared_ptr<const SceneMesh> buildProfileOverlay(const DemGrid& dem, const std::vector<ProfilePoint>& profile);

}  // namespace strata3d
