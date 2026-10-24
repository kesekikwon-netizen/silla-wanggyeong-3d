#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include <array>

namespace strata3d {

struct Aabb {
  QVector3D lo;
  QVector3D hi;
};

// 시야 절두체 여섯 평면(Gribb–Hartmann). 상자가 완전히 밖이면 그릴 필요가 없다.
class Frustum {
 public:
  Frustum() = default;
  explicit Frustum(const QMatrix4x4& viewProjection);
  // 상자가 절두체와 겹칠 가능성이 있으면 true (보수적: 모서리 걸침도 true).
  bool intersects(const Aabb& box) const;

 private:
  std::array<QVector4D, 6> planes_{};
};

// 점에서 상자까지의 최단 거리(상자 안이면 0).
float distanceToAabb(const QVector3D& p, const Aabb& box);

}  // namespace strata3d
