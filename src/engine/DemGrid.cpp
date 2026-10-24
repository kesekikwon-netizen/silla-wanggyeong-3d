#include "DemGrid.h"

#include <algorithm>
#include <cmath>

namespace strata3d {

bool DemGrid::sample(double x, double y, float* zOut) const {
  if (!valid()) return false;
  // 칸 값은 칸 가운데의 표고다. 가운데 기준으로 보간한다.
  const double c = colAt(x) - 0.5;
  const double r = rowAt(y) - 0.5;
  if (c < 0 || r < 0 || c > width - 1 || r > height - 1) return false;
  const int c0 = std::min(int(std::floor(c)), width - 2);
  const int r0 = std::min(int(std::floor(r)), height - 2);
  const double fc = c - c0;
  const double fr = r - r0;
  const double top = at(c0, r0) * (1 - fc) + at(c0 + 1, r0) * fc;
  const double bot = at(c0, r0 + 1) * (1 - fc) + at(c0 + 1, r0 + 1) * fc;
  *zOut = float(top * (1 - fr) + bot * fr);
  return true;
}

}  // namespace strata3d
