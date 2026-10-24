#pragma once

#include <QVector3D>
#include <cstdint>
#include <utility>
#include <vector>

namespace strata3d {

// 정적 3차원 k-d 트리(BLI_kdtree_3d 와 같은 용도: 점 모음에서 반경 탐색·최근접 점).
// 점 하나 = (위치, id). 만든 뒤에는 바꾸지 않는다.
class KdTree3 {
 public:
  struct Entry {
    QVector3D p;
    uint32_t id;
  };
  void build(std::vector<Entry> points);
  size_t size() const { return nodes_.size(); }

  // 중심에서 반경 이하인 점들(id, 거리), 가까운 순.
  std::vector<std::pair<uint32_t, float>> radiusSearch(const QVector3D& center, float radius) const;
  // 가장 가까운 점. 없으면 false.
  bool nearest(const QVector3D& p, uint32_t* id, float* dist) const;

 private:
  struct Node {
    Entry e;
    int left = -1;
    int right = -1;
    int axis = 0;
  };
  int buildRange(std::vector<Entry>& pts, size_t b, size_t e);
  void radiusRec(int n, const QVector3D& c, float r2, std::vector<std::pair<uint32_t, float>>* out) const;
  void nearestRec(int n, const QVector3D& p, float* best2, uint32_t* id) const;

  std::vector<Node> nodes_;
  int root_ = -1;
};

}  // namespace strata3d
