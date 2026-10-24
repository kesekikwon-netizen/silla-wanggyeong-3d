#include "AgxView.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <algorithm>
#include <array>
#include <cmath>

namespace strata3d {

namespace {

constexpr double kLg2Min = -12.47393;
constexpr double kLg2Max = 12.5260688117;

using Mat3 = std::array<double, 9>;

Mat3 inverse(const Mat3& m) {
  const double a = m[0], b = m[1], c = m[2], d = m[3], e = m[4], f = m[5], g = m[6], h = m[7], i = m[8];
  const double det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
  const double s = 1.0 / det;
  return {(e * i - f * h) * s, (c * h - b * i) * s, (b * f - c * e) * s,
          (f * g - d * i) * s, (a * i - c * g) * s, (c * d - a * f) * s,
          (d * h - e * g) * s, (b * g - a * h) * s, (a * e - b * d) * s};
}

Mat3 mul(const Mat3& x, const Mat3& y) {
  Mat3 r{};
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
      for (int k = 0; k < 3; ++k) r[size_t(i * 3 + j)] += x[size_t(i * 3 + k)] * y[size_t(k * 3 + j)];
  return r;
}

// config.ocio 의 「Linear Rec.709」: XYZ D65 → Rec.709
const Mat3 kXyzD65ToRec709 = {3.2404373564920070, -1.5371305409000549, -0.4985288240756933,
                              -0.9692674982687597, 1.8760136862977035, 0.0415560804587997,
                              0.0556434463874256, -0.2040259700872272, 1.0572254813610864};
// config.ocio 의 「Linear FilmLight E-Gamut」: E-Gamut → XYZ D65 (from_scene_reference 는 이것의 역행렬)
const Mat3 kEGamutToXyzD65 = {0.7053968501, 0.1640413283, 0.08101774865,
                              0.2801307241, 0.8202066415, -0.1003373656,
                              -0.1037815116, -0.07290725703, 1.265746519};

float srgbOetf(float c) {
  c = std::clamp(c, 0.0f, 1.0f);
  return c <= 0.0031308f ? 12.92f * c : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

}  // namespace

const std::array<float, 9>& AgxView::rec709ToEGamut() {
  static const std::array<float, 9> m = [] {
    // Rec.709 → XYZ D65 → E-Gamut. (XYZ E ↔ D65 는 두 변환에서 같은 행렬이라 서로 상쇄된다.)
    const Mat3 t = mul(inverse(kEGamutToXyzD65), inverse(kXyzD65ToRec709));
    std::array<float, 9> f{};
    for (size_t i = 0; i < 9; ++i) f[i] = float(t[i]);
    return f;
  }();
  return m;
}

QString AgxView::defaultCubePath() {
  const QString rel = QStringLiteral("data/colormanagement/AgX_Base_sRGB.cube");
  const QString env = qEnvironmentVariable("STRATA3D_DATA");
  if (!env.isEmpty() && QFileInfo::exists(env + QStringLiteral("/colormanagement/AgX_Base_sRGB.cube")))
    return env + QStringLiteral("/colormanagement/AgX_Base_sRGB.cube");
  const QString app = QCoreApplication::applicationDirPath();
  for (const QString& base : {app, app + QStringLiteral("/.."), app + QStringLiteral("/../.."), app + QStringLiteral("/../../..")}) {
    if (QFileInfo::exists(base + QLatin1Char('/') + rel)) return QFileInfo(base + QLatin1Char('/') + rel).absoluteFilePath();
  }
  return rel;
}

bool AgxView::loadCube(const QString& path, QString* errorOut) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (errorOut) *errorOut = QStringLiteral("AgX LUT 를 열지 못했습니다: %1").arg(path);
    return false;
  }
  QTextStream in(&f);
  int n = 0;
  std::vector<float> data;
  while (!in.atEnd()) {
    const QString line = in.readLine().trimmed();
    if (line.isEmpty() || line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1String("TITLE"))) continue;
    if (line.startsWith(QLatin1String("LUT_3D_SIZE"))) {
      n = line.mid(11).trimmed().toInt();
      if (n > 1) data.reserve(size_t(n) * n * n * 3);
      continue;
    }
    if (line.startsWith(QLatin1String("DOMAIN_"))) continue;  // 0..1 이어야 한다(아래에서 확인)
    const QStringList p = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (p.size() != 3) continue;
    data.push_back(p[0].toFloat());
    data.push_back(p[1].toFloat());
    data.push_back(p[2].toFloat());
  }
  if (n < 2 || data.size() != size_t(n) * n * n * 3) {
    if (errorOut) *errorOut = QStringLiteral("AgX LUT 크기가 맞지 않습니다(%1³ 이어야 하는데 값 %2개).").arg(n).arg(int(data.size() / 3));
    return false;
  }
  size_ = n;
  lut_ = std::move(data);
  return true;
}

void AgxView::apply(const float in[3], float out[3]) const {
  const std::array<float, 9>& m = rec709ToEGamut();
  float eg[3];
  for (int i = 0; i < 3; ++i) eg[i] = m[i * 3] * in[0] + m[i * 3 + 1] * in[1] + m[i * 3 + 2] * in[2];
  // lg2 할당: (log2(x) - min) / (max - min). 0 이하는 최소로.
  float p[3];
  const float N1 = float(size_ - 1);
  for (int i = 0; i < 3; ++i) {
    const double l = eg[i] > 0.0f ? std::log2(double(eg[i])) : kLg2Min;
    p[i] = float(std::clamp((l - kLg2Min) / (kLg2Max - kLg2Min), 0.0, 1.0)) * N1;
  }
  int i0[3];
  float f[3];
  for (int i = 0; i < 3; ++i) {
    i0[i] = std::min(int(std::floor(p[i])), size_ - 2);
    f[i] = p[i] - float(i0[i]);
  }
  auto c = [&](int dr, int dg, int db, int ch) {
    const size_t idx = size_t(i0[0] + dr) + size_t(size_) * (size_t(i0[1] + dg) + size_t(size_) * size_t(i0[2] + db));
    return lut_[idx * 3 + size_t(ch)];
  };
  const float fx = f[0], fy = f[1], fz = f[2];
  for (int ch = 0; ch < 3; ++ch) {
    const float c000 = c(0, 0, 0, ch), c100 = c(1, 0, 0, ch), c010 = c(0, 1, 0, ch), c001 = c(0, 0, 1, ch);
    const float c110 = c(1, 1, 0, ch), c101 = c(1, 0, 1, ch), c011 = c(0, 1, 1, ch), c111 = c(1, 1, 1, ch);
    float v;  // 사면체 보간
    if (fx > fy) {
      if (fy > fz) v = c000 + fx * (c100 - c000) + fy * (c110 - c100) + fz * (c111 - c110);
      else if (fx > fz) v = c000 + fx * (c100 - c000) + fz * (c101 - c100) + fy * (c111 - c101);
      else v = c000 + fz * (c001 - c000) + fx * (c101 - c001) + fy * (c111 - c101);
    } else {
      if (fz > fy) v = c000 + fz * (c001 - c000) + fy * (c011 - c001) + fx * (c111 - c011);
      else if (fz > fx) v = c000 + fy * (c010 - c000) + fz * (c011 - c010) + fx * (c111 - c011);
      else v = c000 + fy * (c010 - c000) + fx * (c110 - c010) + fz * (c111 - c110);
    }
    // 2.4 제곱(표시 선형) 뒤 sRGB OETF
    out[ch] = srgbOetf(std::pow(std::max(v, 0.0f), 2.4f));
  }
}

}  // namespace strata3d
