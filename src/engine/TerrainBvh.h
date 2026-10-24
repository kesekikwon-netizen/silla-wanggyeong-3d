#pragma once

#include <QVector3D>
#include <memory>

#include "DemGrid.h"
#include "Kdop14Bvh.h"

namespace strata3d {

// DEM 격자 위의 광선 교차 가속 구조. 8×8 칸 타일마다 14-DOP 를 만들고 타일 위에 4진 트리를 올린다.
// 좌표는 화면과 같은 지역 미터(DEM 중심 원점, +X 동, +Y 북, +Z 위).
// 칸 하나는 삼각형 둘: (c,r)-(c+1,r)-(c,r+1) 과 (c+1,r)-(c+1,r+1)-(c,r+1). 값은 칸 가운데 표고.
class TerrainBvh {
 public:
  static constexpr int kTileCells = 8;

  static std::shared_ptr<const TerrainBvh> build(const DemGrid& grid);

  struct Hit {
    float t = 0;
    QVector3D point;
    int col = 0;
    int row = 0;
  };
  // 방향은 정규화돼 있어야 한다. 가장 가까운 교차를 돌려준다.
  bool rayCast(const QVector3D& origin, const QVector3D& dir, float tMax, Hit* hit) const;
  // 시험·검증용 전수 검사(느림): 모든 삼각형을 직접 검사한다.
  bool rayCastBruteForce(const QVector3D& origin, const QVector3D& dir, float tMax, Hit* hit) const;

  QVector3D toLocal(double X, double Y, float z) const;
  QVector3D vertex(int col, int row) const;  // 칸 가운데 정점(지역 좌표)
  const DemGrid& grid() const { return *grid_; }
  const Kdop14Bvh& bvh() const { return bvh_; }
  int tileCountX() const { return tilesX_; }
  int tileCountY() const { return tilesY_; }

 private:
  bool tileTest(uint32_t tile, const QVector3D& o, const QVector3D& d, float* t, Hit* hit) const;

  std::shared_ptr<DemGrid> grid_;
  Kdop14Bvh bvh_;
  int tilesX_ = 0;
  int tilesY_ = 0;
};

}  // namespace strata3d
