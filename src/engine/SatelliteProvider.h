#pragma once

#include <QImage>
#include <QString>

namespace strata3d {

enum class SatelliteSource { Auto, Google, Vworld };

struct SatelliteRequest {
  double centerX = 0;  // DEM 창과 같은 값을 준다(텍스처가 DEM 격자에 딱 맞도록)
  double centerY = 0;
  double halfExtentM = 12000;
  QString workCrs = QStringLiteral("EPSG:5187");
  int texels = 4096;  // 결과 한 변 픽셀 수
  SatelliteSource source = SatelliteSource::Auto;  // Auto: 유효한 VWorld 키가 있으면 VWorld, 아니면 Google
  QString cacheDir;   // GDAL WMS 타일 캐시(디스크)
  QString vworldKey;  // Auto/Vworld 에서만 쓰며, 이 값은 로그·오류문에 쓰지 않는다
};

struct SatelliteResult {
  QImage image;       // RGB, 행 0 = 북쪽. DEM 격자와 같은 범위
  QString sourceName; // "Google Satellite XYZ" | "VWorld Satellite WMTS"
  int zoomLevel = 0;  // 대략의 타일 단계(안내용)
};

// 실제 위성 타일(Google XYZ 또는 VWorld WMTS)을 GDAL(WMS/TMS 드라이버)로 받아
// 작업 좌표계 창으로 GDALWarp 한다. 타일이 비면 실패로 돌려준다.
bool loadSatelliteImage(const SatelliteRequest& req, SatelliteResult* out, QString* errorOut = nullptr);

}  // namespace strata3d
