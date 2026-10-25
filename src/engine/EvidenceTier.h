#pragma once

#include <QString>
#include <array>
#include <cstddef>

#include "SceneMesh.h"

namespace strata3d {

// 화면에 올리는 모든 지물의 근거 등급(docs/BLUEPRINT.md 4장).
//  A 실측      — DEM, 위성, 국가유산청 좌표·구역, 내 조사 파일, 실측 높이 건물
//  B 근거      — 발굴보고서 치수(쪽수 확인됨)로 세운 것
//  C 추정      — 학설에 따른 방리·도로·봉분 복원
//  D 도식      — 유형 틀로 만든 모델(경주 유적 311개 대부분), 높이 미상 건물
enum class EvidenceTier { Measured = 0, Documented = 1, Inferred = 2, Schematic = 3 };

QChar tierLetter(EvidenceTier t);      // A B C D
QString tierLabel(EvidenceTier t);     // 실측 근거 추정 도식 (C 는 화면에 올리지 않는다)

// 등급별 칠하기. 블렌딩 없이 그리는 지금 파이프라인에서 D·C 를 한눈에 가르는 방법은
// 채도를 빼고(desaturate) 종이색 쪽으로 바래게(fade) 하는 것이다.
struct TierStyle {
  float desaturate = 0.0f;  // 0 = 원래 색, 1 = 완전 회색
  float fade = 0.0f;        // 0 = 그대로, 1 = paper 색
  std::array<float, 3> paper = {0.82f, 0.81f, 0.78f};
};

TierStyle defaultTierStyle(EvidenceTier t);

// mesh 의 정점 [firstVertex, endVertex) 색을 style 대로 바꾼다. 위치·법선·알파는 그대로.
void applyTierStyle(SceneMesh* mesh, std::size_t firstVertex, std::size_t endVertex, const TierStyle& style);

}  // namespace strata3d
