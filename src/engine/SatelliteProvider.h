#pragma once

#include <QImage>
#include <QString>

namespace strata3d {

// Google 타일은 쓰지 않는다: Google Maps Platform 약관(2026-08-26) §3.2.3 긁어오기·캐시·비 Google 지도와 함께 쓰기 금지
// (docs/research/TECH_2026.md 2.1). 위성은 VWorld(Strata 와 같은 키)만.
enum class SatelliteSource { Auto, Vworld };

struct SatelliteRequest {
  double centerX = 0;  // DEM 창과 같은 값을 준다(텍스처가 DEM 격자에 딱 맞도록)
  double centerY = 0;
  double halfExtentM = 12000;
  QString workCrs = QStringLiteral("EPSG:5187");
  int texels = 4096;  // 결과 한 변 픽셀 수
  SatelliteSource source = SatelliteSource::Auto;  // Auto = Vworld(키는 Strata 설정에서 찾는다)
  QString cacheDir;   // 쓰지 않는다: VWorld 이용약관 §19⑥ 사전 승낙 없이 저장 금지 → 타일은 메모리에서만
  QString vworldKey;  // Auto/Vworld 에서만 쓰며, 이 값은 로그·오류문에 쓰지 않는다
};

struct SatelliteResult {
  QImage image;       // RGB, 행 0 = 북쪽. DEM 격자와 같은 범위
  QString sourceName; // "VWorld Satellite WMTS"
  int zoomLevel = 0;  // 대략의 타일 단계(안내용)
};

// 실제 위성 타일(VWorld WMTS)을 GDAL(WMS/TMS 드라이버)로 받아
// 작업 좌표계 창으로 GDALWarp 한다. 타일이 비면 실패로 돌려준다.
bool loadSatelliteImage(const SatelliteRequest& req, SatelliteResult* out, QString* errorOut = nullptr);

}  // namespace strata3d
