#pragma once

#include <QVector3D>
#include <memory>
#include <vector>

#include "DemGrid.h"
#include "Frustum.h"

namespace strata3d {

// 격자 칸 단위로 쓴 지형 조각. level L 조각은 같은 33×33 틀을 (chunkCells<<L) 칸에 펼쳐 그린다.
struct ChunkInstance {
  int col0 = 0;
  int row0 = 0;
  int sizePx = 0;  // 조각이 덮는 칸 수(변)
  int level = 0;
};

// DEM 위에 얹은 4분트리(CDLOD 방식 거리 구간 선택). 노드마다 표고 최소·최대를 들고 있어
// 절두체 컬링과 거리 계산에 쓴다. UI/GL 코드 없음 — 워커 스레드에서 만든다.
class TerrainLodTree {
 public:
  static constexpr int kChunkCells = 32;

  static std::shared_ptr<const TerrainLodTree> build(const DemGrid& grid);

  int levels() const { return int(nodes_.size()); }
  int nodesAt(int level) const { return nx_[level] * ny_[level]; }

  // 세계 좌표(미터, DEM 가운데가 원점, +X 동, +Y 북, +Z 위). 표고는 zScale 배.
  QVector3D toWorld(double col, double row, float z, float zScale) const;
  Aabb nodeBox(int level, int ix, int iy, float zScale) const;

  // 눈 위치에서 거리 구간으로 조각을 고른다. frustum 이 null 이면 시야 컬링을 하지 않는다.
  std::vector<ChunkInstance> select(const QVector3D& eye, const Frustum* frustum, float zScale,
                                    float rangeFactor = 2.5f) const;

  const DemGrid& grid() const { return *grid_; }

 private:
  struct MinMax {
    float lo;
    float hi;
  };
  void collect(int level, int ix, int iy, const QVector3D& eye, const Frustum* frustum, float zScale,
               float rangeFactor, std::vector<ChunkInstance>* out) const;

  std::shared_ptr<DemGrid> grid_;
  std::vector<std::vector<MinMax>> nodes_;  // [level][iy * nx + ix]
  std::vector<int> nx_;
  std::vector<int> ny_;
};

}  // namespace strata3d
