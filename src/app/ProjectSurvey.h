#pragma once

#include <QString>

#include <vector>

#include "LocalVectorProvider.h"
#include "StrataSurvey.h"

class QgsProject;

// QgsProject(Strata 가 저장한 조사 프로젝트를 연 것)의 조사 레이어 7종을 3D 가 쓰는 StrataSurvey 로 옮긴다.
// 파일이 아니라 레이어에서 읽으므로 아직 저장하지 않은 편집(3D 에서 그린 조사구역 등)도 들어간다.
// 좌표는 workCrs 로 옮긴다(QgsCoordinateTransform). 레이어가 하나도 없으면 false.
bool surveyFromProject(QgsProject* project, const QString& gpkgPath, const QString& workCrs, strata3d::StrataSurvey* out);

// 조사 프로젝트에서 주변유적 레이어(ka_hgis/heritage_dataset)의 파일·하위 레이어·이름 칸. 방금 받아 아직
// 저장하지 않은 레이어도 들어간다(3D 는 파일의 프로젝트 기록이 아니라 열린 프로젝트를 따른다).
std::vector<strata3d::LocalVectorSource> heritageSourcesFromProject(QgsProject* project);
