#pragma once

#include <QJsonObject>

#include "Archaeology3dEvaluator.h"

namespace strata3d {

// 평가 결과를 Strata 로 돌려줄 JSON 으로. 키와 단위는 docs/bridge-protocol.md 가 정본이다.
QJsonObject evaluationToJson(const Evaluation& ev);

}  // namespace strata3d
