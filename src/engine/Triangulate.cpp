#include "Triangulate.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace strata3d {

double signedArea(const std::vector<Pt>& r) {
  double a = 0;
  for (size_t i = 0, n = r.size(); i < n; ++i) {
    const Pt& p = r[i];
    const Pt& q = r[(i + 1) % n];
    a += p.x * q.y - q.x * p.y;
  }
  return a * 0.5;
}

namespace {
double cross(const Pt& o, const Pt& a, const Pt& b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); }

bool insideTriangle(const Pt& p, const Pt& a, const Pt& b, const Pt& c) {
  return cross(a, b, p) >= 0 && cross(b, c, p) >= 0 && cross(c, a, p) >= 0;
}
}  // namespace

bool triangulateRing(const std::vector<Pt>& ring, std::vector<uint32_t>* out) {
  const size_t n = ring.size();
  if (n < 3) return false;
  std::vector<uint32_t> idx(n);
  std::iota(idx.begin(), idx.end(), 0u);
  if (signedArea(ring) < 0) std::reverse(idx.begin(), idx.end());  // 반시계로 맞춤

  size_t guard = 0;
  while (idx.size() > 3 && guard++ < n * n) {
    bool clipped = false;
    for (size_t i = 0; i < idx.size(); ++i) {
      const uint32_t ia = idx[(i + idx.size() - 1) % idx.size()], ib = idx[i], ic = idx[(i + 1) % idx.size()];
      const Pt &a = ring[ia], &b = ring[ib], &c = ring[ic];
      const double cr = cross(a, b, c);
      if (cr <= 1e-9) {  // 볼록하지 않거나 일직선: 이 꼭짓점은 귀가 아님(일직선이면 버린다)
        if (std::abs(cr) <= 1e-9) {
          idx.erase(idx.begin() + long(i));
          clipped = true;
          break;
        }
        continue;
      }
      bool ear = true;
      for (uint32_t k : idx) {
        if (k == ia || k == ib || k == ic) continue;
        const Pt& p = ring[k];
        if ((p.x == a.x && p.y == a.y) || (p.x == b.x && p.y == b.y) || (p.x == c.x && p.y == c.y)) continue;
        if (insideTriangle(p, a, b, c)) {
          ear = false;
          break;
        }
      }
      if (!ear) continue;
      out->insert(out->end(), {ia, ib, ic});
      idx.erase(idx.begin() + long(i));
      clipped = true;
      break;
    }
    if (!clipped) return false;
  }
  if (idx.size() == 3) out->insert(out->end(), {idx[0], idx[1], idx[2]});
  return idx.size() <= 3;
}

}  // namespace strata3d
