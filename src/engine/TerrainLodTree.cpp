#include "TerrainLodTree.h"

#include <algorithm>
#include <cmath>

namespace strata3d {

std::shared_ptr<const TerrainLodTree> TerrainLodTree::build(const DemGrid& grid) {
  if (!grid.valid()) return nullptr;
  auto tree = std::shared_ptr<TerrainLodTree>(new TerrainLodTree());
  tree->grid_ = std::make_shared<DemGrid>(grid);
  const int cells = kChunkCells;
  int nx = (grid.width - 1 + cells - 1) / cells;
  int ny = (grid.height - 1 + cells - 1) / cells;

  std::vector<MinMax> leaf(size_t(nx) * ny);
  for (int iy = 0; iy < ny; ++iy) {
    for (int ix = 0; ix < nx; ++ix) {
      const int c1 = std::min(ix * cells + cells, grid.width - 1);
      const int r1 = std::min(iy * cells + cells, grid.height - 1);
      float lo = grid.at(ix * cells, iy * cells), hi = lo;
      for (int r = iy * cells; r <= r1; ++r) {
        for (int c = ix * cells; c <= c1; ++c) {
          const float v = grid.at(c, r);
          lo = std::min(lo, v);
          hi = std::max(hi, v);
        }
      }
      leaf[size_t(iy) * nx + ix] = {lo, hi};
    }
  }
  tree->nodes_.push_back(std::move(leaf));
  tree->nx_.push_back(nx);
  tree->ny_.push_back(ny);

  while (nx > 1 || ny > 1) {
    const int pnx = (nx + 1) / 2, pny = (ny + 1) / 2;
    const std::vector<MinMax>& child = tree->nodes_.back();
    std::vector<MinMax> parent(size_t(pnx) * pny);
    for (int iy = 0; iy < pny; ++iy) {
      for (int ix = 0; ix < pnx; ++ix) {
        bool first = true;
        MinMax m{0, 0};
        for (int dy = 0; dy < 2; ++dy) {
          for (int dx = 0; dx < 2; ++dx) {
            const int cx = ix * 2 + dx, cy = iy * 2 + dy;
            if (cx >= nx || cy >= ny) continue;
            const MinMax c = child[size_t(cy) * nx + cx];
            m = first ? c : MinMax{std::min(m.lo, c.lo), std::max(m.hi, c.hi)};
            first = false;
          }
        }
        parent[size_t(iy) * pnx + ix] = m;
      }
    }
    tree->nodes_.push_back(std::move(parent));
    tree->nx_.push_back(pnx);
    tree->ny_.push_back(pny);
    nx = pnx;
    ny = pny;
  }
  return tree;
}

QVector3D TerrainLodTree::toWorld(double col, double row, float z, float zScale) const {
  const DemGrid& g = *grid_;
  const double cx = (g.width - 1) * 0.5, cy = (g.height - 1) * 0.5;
  return QVector3D(float((col - cx) * g.pixelSize), float((cy - row) * g.pixelSize), z * zScale);
}

Aabb TerrainLodTree::nodeBox(int level, int ix, int iy, float zScale) const {
  const DemGrid& g = *grid_;
  const int size = kChunkCells << level;
  const double c0 = double(ix) * size, r0 = double(iy) * size;
  const double c1 = std::min(c0 + size, double(g.width - 1));
  const double r1 = std::min(r0 + size, double(g.height - 1));
  const MinMax m = nodes_[level][size_t(iy) * nx_[level] + ix];
  const QVector3D a = toWorld(c0, r1, m.lo, zScale);  // 남서쪽 아래
  const QVector3D b = toWorld(c1, r0, m.hi, zScale);  // 북동쪽 위
  return {QVector3D(a.x(), a.y(), std::min(a.z(), b.z())), QVector3D(b.x(), b.y(), std::max(a.z(), b.z()))};
}

void TerrainLodTree::collect(int level, int ix, int iy, const QVector3D& eye, const Frustum* frustum,
                             float zScale, float rangeFactor, std::vector<ChunkInstance>* out) const {
  if (ix >= nx_[level] || iy >= ny_[level]) return;
  const Aabb box = nodeBox(level, ix, iy, zScale);
  if (frustum && !frustum->intersects(box)) return;
  const int size = kChunkCells << level;
  if (level > 0) {
    // 한 단계 더 잘게 쪼갤 거리: 이 노드 한 단계 아래의 거리 구간 안이면 자식으로 내려간다.
    const float range = rangeFactor * float(kChunkCells) * float(grid_->pixelSize) * float(1 << (level - 1));
    if (distanceToAabb(eye, box) < range) {
      for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx) collect(level - 1, ix * 2 + dx, iy * 2 + dy, eye, frustum, zScale, rangeFactor, out);
      return;
    }
  }
  out->push_back({ix * size, iy * size, size, level});
}

std::vector<ChunkInstance> TerrainLodTree::select(const QVector3D& eye, const Frustum* frustum, float zScale,
                                                  float rangeFactor) const {
  std::vector<ChunkInstance> out;
  const int top = levels() - 1;
  collect(top, 0, 0, eye, frustum, zScale, rangeFactor, &out);
  return out;
}

}  // namespace strata3d
