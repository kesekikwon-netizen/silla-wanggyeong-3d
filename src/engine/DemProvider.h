#pragma once

#include <QString>
#include <QStringList>

#include "DemGrid.h"

namespace strata3d {

struct DemRequest {
  double centerX = 0;  // 작업 좌표계 (미터)
  double centerY = 0;
  double halfExtentM = 4000;  // 중심에서 한 변 절반
  double pixelSizeM = 30;     // 결과 격자 한 칸
  QString workCrs = QStringLiteral("EPSG:5187");
  // 비어 있으면 Copernicus GLO-30 COG(/vsicurl/)에서 창이 걸치는 타일을 자동으로 고른다.
  // 로컬 GeoTIFF/VRT 경로나 /vsicurl/ 주소를 직접 줄 수도 있다.
  QStringList sources;
  // 비어 있지 않으면 같은 요청의 결과를 GeoTIFF 로 남겨 두고 다음에는 네트워크 없이 읽는다.
  QString cacheDir;
};

// 창이 걸치는 Copernicus GLO-30 타일 주소들. 창 네 변을 촘촘히 잡아 위경도로 옮겨서 고른다.
QStringList copernicusSourcesFor(const DemRequest& req, QString* errorOut = nullptr);

// 요청한 창만 GDAL(GDALWarp)로 작업 좌표계 미터 격자에 다시 짠다. 원본에 없는 값은 만들지 않는다.
bool loadDemWindow(const DemRequest& req, DemGrid* out, QString* errorOut = nullptr);

}  // namespace strata3d
