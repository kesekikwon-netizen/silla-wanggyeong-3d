#pragma once

#include <QVector3D>
#include <cstdint>
#include <functional>
#include <vector>

namespace strata3d {

// 14-DOP: 7쌍의 슬랩. 축은 Blender BLI_kdopbvh 의 bvhtree_kdop_axes 앞 7개
// (x, y, z, (1,1,1), (1,-1,1), (1,1,-1), (1,-1,-1)) — docs/rea 에서 .rdata 0x1430a2de0 표를 확인했다.
// 축은 정규화하지 않는다(Blender 와 같다). 슬랩 거리는 축 길이 배.
struct Kdop14 {
  static constexpr int kSlabs = 7;
  float lo[kSlabs];
  float hi[kSlabs];

  static const float (&axes())[kSlabs][3];
  void reset();
  void extend(const QVector3D& p);
  void merge(const Kdop14& o);
  QVector3D centroid() const;  // x,y,z 슬랩의 가운데
  // 광선(o + t d, 0 ≤ t ≤ tMax)이 닿는 구간. 안 닿으면 false.
  bool rayInterval(const QVector3D& o, const QVector3D& d, float tMax, float* t0, float* t1) const;
  // 점에서 x,y,z 슬랩 상자까지의 거리(하한값으로 쓴다).
  float boxDistance(const QVector3D& p) const;
};

// 여러 갈래(k-ary) 균형 트리. 잎 하나가 외부 원시도형 하나(id)를 가리킨다. 잎은 중앙값 분할로 모은다.
class Kdop14Bvh {
 public:
  static constexpr int kTreeType = 4;  // 노드당 최대 자식 수 (Blender 기본 4진)

  void build(const std::vector<Kdop14>& leafBounds);
  size_t leafCount() const { return leafCount_; }
  size_t nodeCount() const { return nodes_.size(); }
  int depth() const { return depth_; }
  // Blender 의 BLI_bvhtree_balance 가 쓰는 분기 노드 수: max(1, (totleaf - 3 + tree_type) / (tree_type - 1)).
  static int expectedBranchNodes(int totalLeaves, int treeType = kTreeType);

  // test(id, 원점, 방향, &t): 이 원시도형에 광선이 t 보다 가깝게 맞으면 true 와 갱신된 t.
  using RayTest = std::function<bool(uint32_t id, const QVector3D& o, const QVector3D& d, float* t)>;
  bool rayCast(const QVector3D& origin, const QVector3D& dir, float tMax, const RayTest& test, float* tHit,
               uint32_t* idHit) const;

  // test(id, 점, &dist): 이 원시도형까지의 거리가 *dist 보다 가까우면 true 와 갱신된 dist.
  using NearestTest = std::function<bool(uint32_t id, const QVector3D& p, float* dist)>;
  bool findNearest(const QVector3D& p, float maxDist, const NearestTest& test, float* distOut, uint32_t* idOut) const;

 private:
  struct Node {
    Kdop14 bv;
    int child[kTreeType];
    int numChildren = 0;
    int leaf = -1;  // >= 0 이면 잎(원시도형 id)
  };
  int buildRange(std::vector<uint32_t>& order, size_t begin, size_t end, int depth);

  std::vector<Node> nodes_;
  std::vector<Kdop14> bounds_;
  size_t leafCount_ = 0;
  int depth_ = 0;
  int root_ = -1;
};

}  // namespace strata3d
