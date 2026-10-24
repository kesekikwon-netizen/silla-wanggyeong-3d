#pragma once

#include <QString>
#include <vector>

#include "GeoFeature.h"

namespace strata3d {

// Strata(ka-hgis) 조사 프로젝트(.gpkg)의 도메인 레이어를 읽은 것. 원본은 읽기 전용(IMMUTABLE)으로만 연다.
// 테이블 이름이 곧 도메인 키다(data/schemas/ka_hgis_layers.yaml): survey_area, feature_poly, feature_line,
// section_line, control_points, artifact_point, trial_trench. 모든 좌표는 작업 좌표계(기본 EPSG:5187)로 옮겨 담는다.
struct StrataSurvey {
  QString path;
  QString workCrs;   // 지물을 담은 작업 좌표계(EPSG:5186 또는 5187)
  QString layerCrs;  // 파일 안의 좌표계 이름(안내용)
  std::vector<std::vector<Pt>> surveyAreas;
  std::vector<std::vector<Pt>> trenches;      // trial_trench (면 또는 선)
  std::vector<std::vector<Pt>> featurePolys;
  std::vector<std::vector<Pt>> featureLines;
  std::vector<std::vector<Pt>> sectionLines;
  std::vector<Pt> controlPoints;
  std::vector<Pt> artifactPoints;
  QStringList presentLayers;  // 실제로 지물이 있는 레이어 키
  Pt center;                  // 모든 지물을 감싸는 상자의 가운데
  double halfExtentM = 0;     // 그 상자의 가장 긴 변 절반

  bool empty() const { return presentLayers.isEmpty(); }
};

// workCrs 가 "auto" 이면 조사 파일의 좌표계를 따른다(Strata 규칙: 5186 또는 5187, 그 밖이면 5187 로).
bool loadStrataSurvey(const QString& gpkgPath, const QString& workCrs, StrataSurvey* out, QString* errorOut = nullptr);

}  // namespace strata3d
