#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <vector>

#include "LocalVectorProvider.h"

namespace strata3d {

// Strata 조사 하나에 딸린 자료 목록. Strata 는 조사구역을 그린 뒤 그 둘레로 자료를 받고, 받은 레이어를
// 조사 .gpkg 안의 QGIS 프로젝트(qgis_projects 표)에 적는다. 이것이 조사에 딸린 자료의 유일한 완전한 목록이다
// (hgis SurveyStorage.cpp:346, SurveyBundle.cpp:198-246). 프로젝트가 아직 .gpkg 에 없으면 옆의 .qgz/.qgs 를 읽는다.
struct StrataLayerRef {
  QString name;      // QGIS 레이어 이름(예: 「국가지정유산보호구역」)
  QString provider;  // ogr, gdal, memory …
  QString source;    // 원래 datasource 문자열
  QString path;      // 파일 경로(| 앞부분, 조사 폴더가 옮겨졌으면 고친 경로)
  QString subLayer;  // |layername= 값(없으면 빈 값)
  QMap<QString, QString> props;  // ka_hgis/* 등 사용자 속성
  bool exists = false;           // path 가 이 PC 에 있는지
};

struct StrataProject {
  QString gpkgPath;
  QString projectFrom;  // "gpkg:qgis_projects", "qgz", "qgs"
  QString surveyDir;    // ka_hgis/survey_dir(없으면 .gpkg 폴더)
  std::vector<StrataLayerRef> layers;
};

// 원본은 읽기 전용으로만 연다. 프로젝트를 찾지 못하면 false 와 사유.
bool readStrataProject(const QString& gpkgPath, StrataProject* out, QString* err);

// 이름이 같은 사용자 속성을 가진 레이어(예: key = "ka_hgis/heritage_dataset")
std::vector<const StrataLayerRef*> layersWithProp(const StrataProject& p, const QString& key);

// 이 조사에 받아 둔 주변유적 레이어(ka_hgis/heritage_dataset)를 읽기 목록으로. 분류 = QGIS 레이어 이름,
// 이름 필드 = ka_hgis/label_field. 파일이 없는 레이어는 missingOut 에 적는다.
std::vector<LocalVectorSource> heritageSourcesOf(const StrataProject& p, QStringList* missingOut = nullptr);

}  // namespace strata3d
