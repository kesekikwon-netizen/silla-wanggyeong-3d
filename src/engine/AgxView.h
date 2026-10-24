#pragma once

#include <QString>
#include <array>
#include <vector>

namespace strata3d {

// Blender 4.2.3 의 「AgX」 보기 변환(sRGB 표시)을 그대로 따른다. 변환 순서는 Blender 의 config.ocio 가 정한 것이다:
//   장면 선형 Rec.709 → (XYZ D65) → E-Gamut → lg2 할당 [-12.47393, 12.5260688117] → AgX_Base_sRGB.cube(57³, 사면체 보간)
//   → 2.4 제곱 → sRGB OETF.
// LUT(.cube)는 Blender 4.2.3 datafiles/colormanagement/luts 의 파일이다(data/colormanagement, THIRD_PARTY_NOTICES.md).
// 기준값: tests/data/agx_reference.csv — 같은 config.ocio 를 OpenColorIO 2.6.0 으로 계산한 표(scripts/gen_agx_reference.py).
class AgxView {
 public:
  bool loadCube(const QString& path, QString* errorOut = nullptr);
  bool valid() const { return size_ > 1 && lut_.size() == size_t(size_) * size_ * size_ * 3; }
  int size() const { return size_; }
  const std::vector<float>& lut() const { return lut_; }  // R 가 가장 빨리 변함, 3 float 씩

  // 장면 선형 Rec.709 → 화면 sRGB(0..1). 음수·0 은 어둠으로, 큰 값은 LUT 끝에서 잘린다.
  void apply(const float in[3], float out[3]) const;

  // 앱 옆 data/colormanagement/AgX_Base_sRGB.cube (환경 변수 STRATA3D_DATA 가 있으면 그 폴더 우선)
  static QString defaultCubePath();

  // 3×3 행렬(행 우선): 선형 Rec.709 → E-Gamut. config.ocio 의 두 행렬(XYZ D65→Rec.709, E-Gamut→XYZ D65)에서 계산한다.
  static const std::array<float, 9>& rec709ToEGamut();

 private:
  int size_ = 0;
  std::vector<float> lut_;
};

}  // namespace strata3d
