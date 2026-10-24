#include "KdTree3.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace strata3d {

void KdTree3::build(std::vector<Entry> pts) {
  nodes_.clear();
  nodes_.reserve(pts.size());
  root_ = pts.empty() ? -1 : buildRange(pts, 0, pts.size());
}

int KdTree3::buildRange(std::vector<Entry>& pts, size_t b, size_t e) {
  if (b >= e) return -1;
  QVector3D lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
  for (size_t i = b; i < e; ++i) {
    const QVector3D& p = pts[i].p;
    lo = QVector3D(std::min(lo.x(), p.x()), std::min(lo.y(), p.y()), std::min(lo.z(), p.z()));
    hi = QVector3D(std::max(hi.x(), p.x()), std::max(hi.y(), p.y()), std::max(hi.z(), p.z()));
  }
  const QVector3D ext = hi - lo;
  const int axis = (ext.x() >= ext.y() && ext.x() >= ext.z()) ? 0 : (ext.y() >= ext.z() ? 1 : 2);
  const size_t mid = b + (e - b) / 2;
  std::nth_element(pts.begin() + long(b), pts.begin() + long(mid), pts.begin() + long(e),
                   [axis](const Entry& a, const Entry& c) { return a.p[axis] < c.p[axis]; });
  const int self = int(nodes_.size());
  nodes_.push_back({pts[mid], -1, -1, axis});
  const int l = buildRange(pts, b, mid);
  const int r = buildRange(pts, mid + 1, e);
  nodes_[size_t(self)].left = l;
  nodes_[size_t(self)].right = r;
  return self;
}

void KdTree3::radiusRec(int n, const QVector3D& c, float r2, std::vector<std::pair<uint32_t, float>>* out) const {
  if (n < 0) return;
  const Node& nd = nodes_[size_t(n)];
  const QVector3D d = nd.e.p - c;
  const float dist2 = QVector3D::dotProduct(d, d);
  if (dist2 <= r2) out->push_back({nd.e.id, std::sqrt(dist2)});
  const float diff = c[nd.axis] - nd.e.p[nd.axis];
  radiusRec(diff <= 0 ? nd.left : nd.right, c, r2, out);
  if (diff * diff <= r2) radiusRec(diff <= 0 ? nd.right : nd.left, c, r2, out);
}

std::vector<std::pair<uint32_t, float>> KdTree3::radiusSearch(const QVector3D& center, float radius) const {
  std::vector<std::pair<uint32_t, float>> out;
  radiusRec(root_, center, radius * radius, &out);
  std::sort(out.begin(), out.end(), [](auto& a, auto& b) { return a.second < b.second; });
  return out;
}

void KdTree3::nearestRec(int n, const QVector3D& p, float* best2, uint32_t* id) const {
  if (n < 0) return;
  const Node& nd = nodes_[size_t(n)];
  const QVector3D d = nd.e.p - p;
  const float dist2 = QVector3D::dotProduct(d, d);
  if (dist2 < *best2) {
    *best2 = dist2;
    *id = nd.e.id;
  }
  const float diff = p[nd.axis] - nd.e.p[nd.axis];
  nearestRec(diff <= 0 ? nd.left : nd.right, p, best2, id);
  if (diff * diff < *best2) nearestRec(diff <= 0 ? nd.right : nd.left, p, best2, id);
}

bool KdTree3::nearest(const QVector3D& p, uint32_t* id, float* dist) const {
  if (root_ < 0) return false;
  float best2 = std::numeric_limits<float>::infinity();
  nearestRec(root_, p, &best2, id);
  if (dist) *dist = std::sqrt(best2);
  return true;
}

}  // namespace strata3d
