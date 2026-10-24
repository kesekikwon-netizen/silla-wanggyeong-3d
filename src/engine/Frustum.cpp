#include "Frustum.h"

#include <QVector4D>
#include <algorithm>
#include <cmath>

namespace strata3d {

Frustum::Frustum(const QMatrix4x4& m) {
  const QVector4D r0 = m.row(0), r1 = m.row(1), r2 = m.row(2), r3 = m.row(3);
  planes_[0] = r3 + r0;  // left
  planes_[1] = r3 - r0;  // right
  planes_[2] = r3 + r1;  // bottom
  planes_[3] = r3 - r1;  // top
  planes_[4] = r3 + r2;  // near
  planes_[5] = r3 - r2;  // far
  for (QVector4D& p : planes_) {
    const float len = QVector3D(p.x(), p.y(), p.z()).length();
    if (len > 0) p /= len;
  }
}

bool Frustum::intersects(const Aabb& box) const {
  for (const QVector4D& p : planes_) {
    // 평면 법선 방향으로 가장 앞선 모서리가 뒤쪽이면 상자 전체가 밖이다.
    const QVector3D v(p.x() >= 0 ? box.hi.x() : box.lo.x(), p.y() >= 0 ? box.hi.y() : box.lo.y(),
                      p.z() >= 0 ? box.hi.z() : box.lo.z());
    if (p.x() * v.x() + p.y() * v.y() + p.z() * v.z() + p.w() < 0) return false;
  }
  return true;
}

float distanceToAabb(const QVector3D& p, const Aabb& box) {
  const float dx = std::max({box.lo.x() - p.x(), 0.0f, p.x() - box.hi.x()});
  const float dy = std::max({box.lo.y() - p.y(), 0.0f, p.y() - box.hi.y()});
  const float dz = std::max({box.lo.z() - p.z(), 0.0f, p.z() - box.hi.z()});
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

}  // namespace strata3d
