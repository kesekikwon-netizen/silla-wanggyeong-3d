#pragma once

#include <QString>

namespace strata3d {

// GDAL/PROJ(OSR) 로 점 하나를 옮긴다. 축 순서는 항상 (경도|X, 위도|Y). 직접 투영식을 쓰지 않는다.
// crs 는 "EPSG:5187" 같은 문자열. 실패하면 false 와 사유.
bool transformPoint(const QString& fromCrs, const QString& toCrs, double* x, double* y,
                    QString* errorOut = nullptr);

}  // namespace strata3d
