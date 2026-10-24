#pragma once

#include <QString>
#include <vector>

namespace strata3d {

// 실측 DEM 한 장. 작업 좌표계(EPSG:5187 등)의 미터 격자이며 행 0이 북쪽이다.
// 값은 GDAL 로 읽은 원본 표고뿐이고, 수식으로 만든 값은 들어오지 않는다.
struct DemGrid {
  int width = 0;
  int height = 0;
  double originX = 0;    // 왼쪽 위 모서리 X (m)
  double originY = 0;    // 왼쪽 위 모서리 Y (m)
  double pixelSize = 0;  // 한 칸 (m)
  std::vector<float> z;  // row-major
  float zMin = 0;
  float zMax = 0;
  int noDataCount = 0;
  QString crs;     // 예: "EPSG:5187"
  QString source;  // 읽어 온 원본 (파일 경로 또는 /vsicurl/ 주소들)

  bool valid() const { return width > 1 && height > 1 && z.size() == size_t(width) * height; }
  float at(int col, int row) const { return z[size_t(row) * width + col]; }
  // 세계 좌표 → 격자 좌표(소수). 칸 가운데가 아니라 칸 모서리 기준이다.
  double colAt(double x) const { return (x - originX) / pixelSize; }
  double rowAt(double y) const { return (originY - y) / pixelSize; }
  double xAt(double col) const { return originX + col * pixelSize; }
  double yAt(double row) const { return originY - row * pixelSize; }
  // 쌍선형 보간 표고. 격자 밖이면 false.
  bool sample(double x, double y, float* zOut) const;
};

}  // namespace strata3d
