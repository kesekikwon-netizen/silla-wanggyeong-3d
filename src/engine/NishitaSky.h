#pragma once

#include <QVector3D>

namespace strata3d {

// Nishita 1993 단일 산란 하늘. 지구 반경·대기 반경·레일리/Mie 스케일 높이·오존 분포(10–25–40 km 삼각형)·Mie g·위상 함수 상수는
// Blender 4.2.3 blender.exe 의 .rdata 에서 확인한 값이다(docs/sky-constants.md, 주소 0x35bf580~0x35bf5b8).
// 파장별 산란 계수는 Blender 의 스펙트럼 표가 아니라 RGB 근사(레일리: Nishita 논문, 오존 흡수: Hillaire 2020)다.
// GLSL 판(PostShaders.cpp)이 같은 식을 쓰며 시험이 둘을 맞춰 본다.
class NishitaSky {
 public:
  static constexpr float kEarthRadius = 6360000.0f;       // blender.exe 0x35bf5b0
  static constexpr float kAtmosphereRadius = 6420000.0f;  // 0x35bf5b8 = 이것의 제곱
  static constexpr float kRayleighScale = 8000.0f;        // 0x35bf5a4
  static constexpr float kMieScale = 1200.0f;             // 0x35bf5a0
  static constexpr float kMieG = 0.76f;                   // 0x35bf58c/590/594 (3(1-g²), 2g, 1+g²)
  static constexpr float kMieScattering = 2.22e-5f;       // 0x35bf580
  static constexpr float kOzoneLow = 10000.0f, kOzonePeak = 25000.0f, kOzoneHigh = 40000.0f;  // 0x35bf5a8/5ac, 기울기 1/15000 = 0x35bf584

  // 눈 높이(m, 지표 위). 태양 세기는 복사조도 배율.
  explicit NishitaSky(float eyeAltitudeM = 1.7f, float sunIrradiance = 12.0f) : eyeAlt_(eyeAltitudeM), sun_(sunIrradiance) {}

  static float ozoneDensity(float altitudeM);  // 0..1 삼각형
  static float rayleighPhase(float cosTheta);
  static float miePhase(float cosTheta);
  // 해수면 높이에서 이 방향으로 대기를 끝까지 지나온 태양광의 투과율(RGB)
  QVector3D sunTransmittance(const QVector3D& sunDir) const;
  // dir 방향으로 본 하늘 복사휘도(RGB, 선형). 태양 원반은 넣지 않는다.
  QVector3D radiance(const QVector3D& dir, const QVector3D& sunDir) const;

 private:
  float eyeAlt_;
  float sun_;
};

}  // namespace strata3d
