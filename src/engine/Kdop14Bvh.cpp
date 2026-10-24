#include "Kdop14Bvh.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace strata3d {

namespace {
constexpr float kAxes[Kdop14::kSlabs][3] = {
    {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 1}, {1, -1, 1}, {1, 1, -1}, {1, -1, -1}};
constexpr float kInf = std::numeric_limits<float>::infinity();
inline float dot(const float* a, const QVector3D& p) { return a[0] * p.x() + a[1] * p.y() + a[2] * p.z(); }
}  // namespace

const float (&Kdop14::axes())[Kdop14::kSlabs][3] { return kAxes; }

void Kdop14::reset() {
  for (int i = 0; i < kSlabs; ++i) {
    lo[i] = kInf;
    hi[i] = -kInf;
  }
}

void Kdop14::extend(const QVector3D& p) {
  for (int i = 0; i < kSlabs; ++i) {
    const float v = dot(kAxes[i], p);
    lo[i] = std::min(lo[i], v);
    hi[i] = std::max(hi[i], v);
  }
}

void Kdop14::merge(const Kdop14& o) {
  for (int i = 0; i < kSlabs; ++i) {
    lo[i] = std::min(lo[i], o.lo[i]);
    hi[i] = std::max(hi[i], o.hi[i]);
  }
}

QVector3D Kdop14::centroid() const {
  return QVector3D((lo[0] + hi[0]) * 0.5f, (lo[1] + hi[1]) * 0.5f, (lo[2] + hi[2]) * 0.5f);
}

bool Kdop14::rayInterval(const QVector3D& o, const QVector3D& d, float tMax, float* t0, float* t1) const {
  float tn = 0, tf = tMax;
  for (int i = 0; i < kSlabs; ++i) {
    const float od = dot(kAxes[i], o), dd = dot(kAxes[i], d);
    if (std::abs(dd) < 1e-12f) {
      if (od < lo[i] || od > hi[i]) return false;
      continue;
    }
    float a = (lo[i] - od) / dd, b = (hi[i] - od) / dd;
    if (a > b) std::swap(a, b);
    tn = std::max(tn, a);
    tf = std::min(tf, b);
    if (tn > tf) return false;
  }
  *t0 = tn;
  *t1 = tf;
  return true;
}

float Kdop14::boxDistance(const QVector3D& p) const {
  const float dx = std::max({lo[0] - p.x(), 0.0f, p.x() - hi[0]});
  const float dy = std::max({lo[1] - p.y(), 0.0f, p.y() - hi[1]});
  const float dz = std::max({lo[2] - p.z(), 0.0f, p.z() - hi[2]});
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

int Kdop14Bvh::expectedBranchNodes(int totalLeaves, int treeType) {
  return std::max(1, (totalLeaves - 3 + treeType) / (treeType - 1));
}

void Kdop14Bvh::build(const std::vector<Kdop14>& leafBounds) {
  bounds_ = leafBounds;
  leafCount_ = leafBounds.size();
  nodes_.clear();
  depth_ = 0;
  root_ = -1;
  if (bounds_.empty()) return;
  nodes_.reserve(leafCount_ * 2);
  std::vector<uint32_t> order(leafCount_);
  for (size_t i = 0; i < leafCount_; ++i) order[i] = uint32_t(i);
  root_ = buildRange(order, 0, leafCount_, 1);
}

int Kdop14Bvh::buildRange(std::vector<uint32_t>& order, size_t begin, size_t end, int depth) {
  depth_ = std::max(depth_, depth);
  Node node;
  node.bv.reset();
  for (int i = 0; i < kTreeType; ++i) node.child[i] = -1;
  if (end - begin == 1) {
    node.leaf = int(order[begin]);
    node.bv = bounds_[order[begin]];
    nodes_.push_back(node);
    return int(nodes_.size()) - 1;
  }
  // 중심 상자의 가장 긴 변을 따라 중앙값으로 두 번 나눠 최대 4개 덩어리를 만든다.
  auto splitMedian = [&](size_t b, size_t e, size_t* mid) {
    QVector3D lo(kInf, kInf, kInf), hi(-kInf, -kInf, -kInf);
    for (size_t i = b; i < e; ++i) {
      const QVector3D c = bounds_[order[i]].centroid();
      lo = QVector3D(std::min(lo.x(), c.x()), std::min(lo.y(), c.y()), std::min(lo.z(), c.z()));
      hi = QVector3D(std::max(hi.x(), c.x()), std::max(hi.y(), c.y()), std::max(hi.z(), c.z()));
    }
    const QVector3D ext = hi - lo;
    const int axis = (ext.x() >= ext.y() && ext.x() >= ext.z()) ? 0 : (ext.y() >= ext.z() ? 1 : 2);
    *mid = b + (e - b) / 2;
    std::nth_element(order.begin() + long(b), order.begin() + long(*mid), order.begin() + long(e),
                     [&](uint32_t a, uint32_t c) { return bounds_[a].centroid()[axis] < bounds_[c].centroid()[axis]; });
  };
  std::vector<std::pair<size_t, size_t>> groups = {{begin, end}};
  if (end - begin > 1) {
    std::vector<std::pair<size_t, size_t>> next;
    for (auto [b, e] : groups) {
      size_t m = 0;
      splitMedian(b, e, &m);
      next.push_back({b, m});
      next.push_back({m, e});
    }
    groups = next;
    if (end - begin >= 4) {
      next.clear();
      for (auto [b, e] : groups) {
        if (e - b < 2) { next.push_back({b, e}); continue; }
        size_t m = 0;
        splitMedian(b, e, &m);
        next.push_back({b, m});
        next.push_back({m, e});
      }
      groups = next;
    }
  }
  const int self = int(nodes_.size());
  nodes_.push_back(node);
  for (auto [b, e] : groups) {
    if (b == e) continue;
    const int c = buildRange(order, b, e, depth + 1);
    Node& n = nodes_[size_t(self)];
    n.child[n.numChildren++] = c;
    n.bv.merge(nodes_[size_t(c)].bv);
  }
  return self;
}

bool Kdop14Bvh::rayCast(const QVector3D& origin, const QVector3D& dir, float tMax, const RayTest& test,
                        float* tHit, uint32_t* idHit) const {
  if (root_ < 0) return false;
  bool hit = false;
  float best = tMax;
  std::vector<int> stack;
  stack.push_back(root_);
  while (!stack.empty()) {
    const Node& n = nodes_[size_t(stack.back())];
    stack.pop_back();
    float t0 = 0, t1 = 0;
    if (!n.bv.rayInterval(origin, dir, best, &t0, &t1)) continue;
    if (n.leaf >= 0) {
      float t = best;
      if (test(uint32_t(n.leaf), origin, dir, &t) && t < best) {
        best = t;
        hit = true;
        if (idHit) *idHit = uint32_t(n.leaf);
      }
      continue;
    }
    // 가까운 자식을 나중에 넣어 먼저 꺼낸다.
    std::pair<float, int> order[kTreeType];
    int cnt = 0;
    for (int i = 0; i < n.numChildren; ++i) {
      float c0 = 0, c1 = 0;
      if (nodes_[size_t(n.child[i])].bv.rayInterval(origin, dir, best, &c0, &c1)) order[cnt++] = {c0, n.child[i]};
    }
    std::sort(order, order + cnt, [](auto& a, auto& b) { return a.first > b.first; });
    for (int i = 0; i < cnt; ++i) stack.push_back(order[i].second);
  }
  if (hit && tHit) *tHit = best;
  return hit;
}

bool Kdop14Bvh::findNearest(const QVector3D& p, float maxDist, const NearestTest& test, float* distOut,
                            uint32_t* idOut) const {
  if (root_ < 0) return false;
  bool found = false;
  float best = maxDist;
  std::vector<int> stack;
  stack.push_back(root_);
  while (!stack.empty()) {
    const Node& n = nodes_[size_t(stack.back())];
    stack.pop_back();
    if (n.bv.boxDistance(p) >= best) continue;
    if (n.leaf >= 0) {
      float d = best;
      if (test(uint32_t(n.leaf), p, &d) && d < best) {
        best = d;
        found = true;
        if (idOut) *idOut = uint32_t(n.leaf);
      }
      continue;
    }
    std::pair<float, int> order[kTreeType];
    int cnt = 0;
    for (int i = 0; i < n.numChildren; ++i) order[cnt++] = {nodes_[size_t(n.child[i])].bv.boxDistance(p), n.child[i]};
    std::sort(order, order + cnt, [](auto& a, auto& b) { return a.first > b.first; });
    for (int i = 0; i < cnt; ++i) stack.push_back(order[i].second);
  }
  if (found && distOut) *distOut = best;
  return found;
}

}  // namespace strata3d
