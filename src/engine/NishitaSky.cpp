#include "NishitaSky.h"

#include <algorithm>
#include <cmath>

namespace strata3d {

namespace {
constexpr float kPi = 3.14159265358979f;
const QVector3D kRayleigh(5.8e-6f, 13.5e-6f, 33.1e-6f);           // 1/m, Nishita 1993
const QVector3D kOzoneAbsorb(0.650e-6f, 1.881e-6f, 0.085e-6f);     // 1/m, Hillaire 2020
constexpr float kMieExtinctionRatio = 1.11f;

// 광선 o + t d 와 중심이 원점인 반지름 r 구의 (가까운, 먼) 교점. 안 만나면 false.
bool sphere(const QVector3D& o, const QVector3D& d, float r, float* t0, float* t1) {
  const float b = QVector3D::dotProduct(o, d);
  const float c = QVector3D::dotProduct(o, o) - r * r;
  const float disc = b * b - c;
  if (disc < 0) return false;
  const float s = std::sqrt(disc);
  *t0 = -b - s;
  *t1 = -b + s;
  return true;
}

struct Density {
  float r, m, o;
};
Density density(float altitude) {
  return {std::exp(-altitude / NishitaSky::kRayleighScale), std::exp(-altitude / NishitaSky::kMieScale),
          NishitaSky::ozoneDensity(altitude)};
}

QVector3D expNeg(const QVector3D& v) { return QVector3D(std::exp(-v.x()), std::exp(-v.y()), std::exp(-v.z())); }
QVector3D mulc(const QVector3D& a, const QVector3D& b) { return QVector3D(a.x() * b.x(), a.y() * b.y(), a.z() * b.z()); }
}  // namespace

float NishitaSky::ozoneDensity(float h) {
  if (h <= kOzoneLow || h >= kOzoneHigh) return 0.0f;
  return h < kOzonePeak ? (h - kOzoneLow) / (kOzonePeak - kOzoneLow) : 1.0f - (h - kOzonePeak) / (kOzoneHigh - kOzonePeak);
}

float NishitaSky::rayleighPhase(float c) { return 3.0f / (16.0f * kPi) * (1.0f + c * c); }

float NishitaSky::miePhase(float c) {
  const float g = kMieG, g2 = g * g;
  return 3.0f / (8.0f * kPi) * (1.0f - g2) * (1.0f + c * c) / ((2.0f + g2) * std::pow(1.0f + g2 - 2.0f * g * c, 1.5f));
}

namespace {
// 점 p 에서 sunDir 로 대기 끝까지의 광학 깊이(RGB)
QVector3D opticalDepthToSun(const QVector3D& p, const QVector3D& sunDir) {
  float t0, t1;
  if (!sphere(p, sunDir, NishitaSky::kAtmosphereRadius, &t0, &t1)) return QVector3D(0, 0, 0);
  const float len = t1;  // p 는 대기 안이라 t0 < 0
  constexpr int kSteps = 12;
  const float dt = len / kSteps;
  QVector3D sum(0, 0, 0);
  float r = 0, m = 0, o = 0;
  for (int i = 0; i < kSteps; ++i) {
    const QVector3D q = p + sunDir * ((i + 0.5f) * dt);
    const Density d = density(q.length() - NishitaSky::kEarthRadius);
    r += d.r; m += d.m; o += d.o;
  }
  Q_UNUSED(sum);
  return kRayleigh * (r * dt) + QVector3D(1, 1, 1) * (NishitaSky::kMieScattering * kMieExtinctionRatio * m * dt) + kOzoneAbsorb * (o * dt);
}
}  // namespace

QVector3D NishitaSky::sunTransmittance(const QVector3D& sunDir) const {
  return expNeg(opticalDepthToSun(QVector3D(0, 0, kEarthRadius + eyeAlt_), sunDir.normalized()));
}

QVector3D NishitaSky::radiance(const QVector3D& dirIn, const QVector3D& sunDirIn) const {
  const QVector3D dir = dirIn.normalized(), sunDir = sunDirIn.normalized();
  const QVector3D origin(0, 0, kEarthRadius + eyeAlt_);
  float t0, t1;
  if (!sphere(origin, dir, kAtmosphereRadius, &t0, &t1)) return QVector3D(0, 0, 0);
  float e0, e1;
  float len = t1;
  if (sphere(origin, dir, kEarthRadius, &e0, &e1) && e0 > 0) len = e0;  // 지면에 막힘
  constexpr int kSteps = 24;
  const float dt = len / kSteps;
  const float mu = QVector3D::dotProduct(dir, sunDir);
  const float pr = rayleighPhase(mu), pm = miePhase(mu);
  QVector3D total(0, 0, 0);
  QVector3D viewDepth(0, 0, 0);
  for (int i = 0; i < kSteps; ++i) {
    const QVector3D p = origin + dir * ((i + 0.5f) * dt);
    const float alt = p.length() - kEarthRadius;
    if (alt < 0) break;
    const Density d = density(alt);
    viewDepth += (kRayleigh * d.r + QVector3D(1, 1, 1) * (kMieScattering * kMieExtinctionRatio * d.m) + kOzoneAbsorb * d.o) * dt;
    // 지면 그림자(태양이 지평선 아래)
    float s0, s1;
    if (sphere(p, sunDir, kEarthRadius, &s0, &s1) && s0 > 0) continue;
    const QVector3D att = expNeg(viewDepth + opticalDepthToSun(p, sunDir));
    const QVector3D scatter = kRayleigh * (d.r * pr) + QVector3D(1, 1, 1) * (kMieScattering * d.m * pm);
    total += mulc(att, scatter) * dt;
  }
  return total * sun_;
}

}  // namespace strata3d
