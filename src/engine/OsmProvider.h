#pragma once

#include <vector>

#include "GeoFeature.h"

namespace strata3d {

// OpenStreetMap(Overpass) 의 실제 건물 외곽선과 historic/heritage 지물. 키가 필요 없다.
// 높이는 태그(height, building:levels)가 있을 때만 값이 있고, 없으면 HeightSource::Unknown 이다.
bool loadOsmFeatures(const FeatureRequest& req, std::vector<GeoFeature>* out, QString* errorOut = nullptr);

// 시험용: Overpass JSON 한 덩어리를 지물로 바꾼다(네트워크 없음).
bool parseOverpassJson(const QByteArray& json, const QString& workCrs, std::vector<GeoFeature>* out,
                       QString* errorOut = nullptr);

// "9.4", "9 m", "9,5" → 미터. 읽을 수 없으면 false.
bool parseOsmHeight(const QString& text, double* meters);

}  // namespace strata3d
