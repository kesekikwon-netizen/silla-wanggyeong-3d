#include "TerrainBvh.h"

#include <algorithm>
#include <cmath>

namespace strata3d {

namespace {
// Möller–Trumbore. 맞으면 true 와 t.
bool rayTriangle(const QVector3D& o, const QVector3D& d, const QVector3D& a, const QVector3D& b, const QVector3D& c, float* t) {
  const QVector3D e1 = b - a, e2 = c - a;
  const QVector3D p = QVector3D::crossProduct(d, e2);
  const float det = QVector3D::dotProduct(e1, p);
  if (std::abs(det) < 1e-12f) return false;
  const float inv = 1.0f / det;
  const QVector3D s = o - a;
  const float u = QVector3D::dotProduct(s, p) * inv;
  if (u < 0.0f || u > 1.0f) return false;
  const QVector3D q = QVector3D::crossProduct(s, e1);
  const float v = QVector3D::dotProduct(d, q) * inv;
  if (v < 0.0f || u + v > 1.0f) return false;
  const float tt = QVector3D::dotProduct(e2, q) * inv;
  if (tt < 0.0f) return false;
  *t = tt;
  return true;
}
}  // namespace

QVector3D TerrainBvh::toLocal(double X, double Y, float z) const {
  return QVector3D(float(X - grid_->centerX()), float(Y - grid_->centerY()), z);
}

QVector3D TerrainBvh::vertex(int col, int row) const {
  const DemGrid& g = *grid_;
  const double cx = (g.width - 1) * 0.5, cy = (g.height - 1) * 0.5;
  return QVector3D(float((col - cx) * g.pixelSize), float((cy - row) * g.pixelSize), g.at(col, row));
}

std::shared_ptr<const TerrainBvh> TerrainBvh::build(const DemGrid& grid) {
  if (!grid.valid()) return nullptr;
  auto t = std::shared_ptr<TerrainBvh>(new TerrainBvh());
  t->grid_ = std::make_shared<DemGrid>(grid);
  t->tilesX_ = (grid.width - 1 + kTileCells - 1) / kTileCells;
  t->tilesY_ = (grid.height - 1 + kTileCells - 1) / kTileCells;
  std::vector<Kdop14> bounds(size_t(t->tilesX_) * t->tilesY_);
  const TerrainBvh& self = *t;
#pragma omp parallel for
  for (int ty = 0; ty < t->tilesY_; ++ty) {
    for (int tx = 0; tx < t->tilesX_; ++tx) {
      Kdop14 k;
      k.reset();
      const int c1 = std::min((tx + 1) * kTileCells, grid.width - 1), r1 = std::min((ty + 1) * kTileCells, grid.height - 1);
      for (int r = ty * kTileCells; r <= r1; ++r)
        for (int c = tx * kTileCells; c <= c1; ++c) k.extend(self.vertex(c, r));
      bounds[size_t(ty) * t->tilesX_ + tx] = k;
    }
  }
  t->bvh_.build(bounds);
  return t;
}

bool TerrainBvh::tileTest(uint32_t tile, const QVector3D& o, const QVector3D& d, float* t, Hit* hit) const {
  const int tx = int(tile) % tilesX_, ty = int(tile) / tilesX_;
  const int c1 = std::min((tx + 1) * kTileCells, grid_->width - 1), r1 = std::min((ty + 1) * kTileCells, grid_->height - 1);
  bool any = false;
  for (int r = ty * kTileCells; r < r1; ++r) {
    for (int c = tx * kTileCells; c < c1; ++c) {
      const QVector3D a = vertex(c, r), b = vertex(c + 1, r), cc = vertex(c, r + 1), dd = vertex(c + 1, r + 1);
      float tt = 0;
      if (rayTriangle(o, d, a, b, cc, &tt) && tt < *t) { *t = tt; any = true; if (hit) { hit->col = c; hit->row = r; } }
      if (rayTriangle(o, d, b, dd, cc, &tt) && tt < *t) { *t = tt; any = true; if (hit) { hit->col = c; hit->row = r; } }
    }
  }
  return any;
}

bool TerrainBvh::rayCast(const QVector3D& origin, const QVector3D& dir, float tMax, Hit* hit) const {
  Hit h;
  float t = 0;
  uint32_t id = 0;
  const bool ok = bvh_.rayCast(origin, dir, tMax,
      [&](uint32_t tile, const QVector3D& o, const QVector3D& d, float* tt) { return tileTest(tile, o, d, tt, &h); }, &t, &id);
  if (ok && hit) {
    h.t = t;
    h.point = origin + dir * t;
    *hit = h;
  }
  return ok;
}

bool TerrainBvh::rayCastBruteForce(const QVector3D& origin, const QVector3D& dir, float tMax, Hit* hit) const {
  Hit h;
  float t = tMax;
  bool any = false;
  for (uint32_t tile = 0; tile < uint32_t(tilesX_) * uint32_t(tilesY_); ++tile) any |= tileTest(tile, origin, dir, &t, &h);
  if (any && hit) {
    h.t = t;
    h.point = origin + dir * t;
    *hit = h;
  }
  return any;
}

}  // namespace strata3d
