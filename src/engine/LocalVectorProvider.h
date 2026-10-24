#pragma once

#include <QStringList>
#include <vector>

#include "GeoFeature.h"

namespace strata3d {

// Strata 가 내려받아 둔 주변유적 SHP 캐시: %LOCALAPPDATA%\ka-hgis\ka-hgis\주변유적\SHP\<해시>\payload\*.shp
QStringList strataHeritageShpPaths();

// OGR 로 SHP/GPKG 를 읽어 창 안의 다각형·점을 지물로 돌려준다. 원본은 읽기 전용으로만 연다.
// 창과 겹치지 않는 파일은 조용히 건너뛰고, 열 수 없는 파일은 skippedOut 에 적는다.
// 같은 지물이 캐시의 여러 해시 폴더에 중복돼 있어도 한 번만 담는다(층·좌표·이름 기준).
bool loadLocalVectors(const FeatureRequest& req, const QStringList& paths, std::vector<GeoFeature>* out,
                      QStringList* skippedOut = nullptr);

}  // namespace strata3d
