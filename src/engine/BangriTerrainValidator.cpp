#include "BangriTerrainValidator.h"

#include <QVector3D>
#include <algorithm>
#include <array>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "CrsTransform.h"
#include "EvaluationOverlay.h"
#include "Triangulate.h"

namespace strata3d {

namespace {

using Color = std::array<float, 4>;

void addVert(SceneMesh* m, const QVector3D& p, const QVector3D& n, const Color& c) {
  m->vertices.insert(m->vertices.end(), {p.x(), p.y(), p.z(), n.x(), n.y(), n.z(), c[0], c[1], c[2], c[3]});
}

void addQuad(SceneMesh* m, const QVector3D& a, const QVector3D& b, const QVector3D& c, const QVector3D& d, const Color& col) {
  QVector3D n = QVector3D::crossProduct(b - a, d - a);
  if (n.lengthSquared() > 1e-8f) n.normalize();
  else n = QVector3D(0, 0, 1);
  const uint32_t i = uint32_t(m->vertices.size() / SceneMesh::kFloatsPerVertex);
  addVert(m, a, n, col);
  addVert(m, b, n, col);
  addVert(m, c, n, col);
  addVert(m, d, n, col);
  m->indices.insert(m->indices.end(), {i, i + 1, i + 2, i, i + 2, i + 3});
}

void addTri(SceneMesh* m, const QVector3D& a, const QVector3D& b, const QVector3D& c, const Color& col) {
  QVector3D n = QVector3D::crossProduct(b - a, c - a);
  if (n.lengthSquared() > 1e-8f) n.normalize();
  else n = QVector3D(0, 0, 1);
  const uint32_t i = uint32_t(m->vertices.size() / SceneMesh::kFloatsPerVertex);
  addVert(m, a, n, col);
  addVert(m, b, n, col);
  addVert(m, c, n, col);
  m->indices.insert(m->indices.end(), {i, i + 1, i + 2});
}

// 회전된 직육면체(기단·초석·벽체·담장) 생성
void addOrientedBox(SceneMesh* m,
                    const DemGrid& dem,
                    double wx,
                    double wy,
                    float zBaseOffset,
                    float heightM,
                    double widthU,
                    double depthV,
                    double azDeg,
                    const Color& col) {
  float gz = 0.0f;
  if (!dem.sample(wx, wy, &gz)) return;
  const double cx = dem.centerX(), cy = dem.centerY();
  const double rad = azDeg * M_PI / 180.0;
  const double cosA = std::cos(rad), sinA = std::sin(rad);
  auto toLocal = [&](double u, double v, float zRel) {
    const double x = wx + u * cosA + v * sinA;
    const double y = wy - u * sinA + v * cosA;
    return QVector3D(float(x - cx), float(y - cy), gz + zBaseOffset + zRel);
  };
  const double hu = 0.5 * widthU, hv = 0.5 * depthV;
  const QVector3D b0 = toLocal(-hu, -hv, 0.0f);
  const QVector3D b1 = toLocal(+hu, -hv, 0.0f);
  const QVector3D b2 = toLocal(+hu, +hv, 0.0f);
  const QVector3D b3 = toLocal(-hu, +hv, 0.0f);
  const QVector3D t0 = toLocal(-hu, -hv, heightM);
  const QVector3D t1 = toLocal(+hu, -hv, heightM);
  const QVector3D t2 = toLocal(+hu, +hv, heightM);
  const QVector3D t3 = toLocal(-hu, +hv, heightM);

  addQuad(m, b0, b1, t1, t0, col);
  addQuad(m, b1, b2, t2, t1, col);
  addQuad(m, b2, b3, t3, t2, col);
  addQuad(m, b3, b0, t0, t3, col);
  addQuad(m, t0, t1, t2, t3, col);
}

// 원기둥/다각기둥 (기둥·석등·당간지주·찰주·수목 등)
void addCylinder(SceneMesh* m,
                 const DemGrid& dem,
                 double wx,
                 double wy,
                 float z0,
                 float z1,
                 double rBottom,
                 double rTop,
                 int segments,
                 const Color& col) {
  float gz = 0.0f;
  if (!dem.sample(wx, wy, &gz)) return;
  const double cx = dem.centerX(), cy = dem.centerY();
  const QVector3D topCenter(float(wx - cx), float(wy - cy), gz + z1);
  for (int i = 0; i < segments; ++i) {
    const double a0 = (2.0 * M_PI * i) / segments;
    const double a1 = (2.0 * M_PI * (i + 1)) / segments;
    const QVector3D b0(float(wx + rBottom * std::cos(a0) - cx), float(wy + rBottom * std::sin(a0) - cy), gz + z0);
    const QVector3D b1(float(wx + rBottom * std::cos(a1) - cx), float(wy + rBottom * std::sin(a1) - cy), gz + z0);
    const QVector3D t0(float(wx + rTop * std::cos(a0) - cx), float(wy + rTop * std::sin(a0) - cy), gz + z1);
    const QVector3D t1(float(wx + rTop * std::cos(a1) - cx), float(wy + rTop * std::sin(a1) - cy), gz + z1);
    addQuad(m, b0, b1, t1, t0, col);
    if (rTop > 0.05) {
      addTri(m, topCenter, t0, t1, col);
    }
  }
}

// 곡선 처마(앙곡·사래 들림)와 용마루·치미(鴟尾)를 갖춘 블렌더급 한국 전통 팔작/우진각 기와지붕 생성기
void addCurvedPaljakRoof(SceneMesh* m,
                         const DemGrid& dem,
                         double wx,
                         double wy,
                         float zEave,
                         float roofH,
                         double eaveWidthU,
                         double eaveDepthV,
                         double azDeg,
                         const Color& roofCol,
                         bool hasGoldenChimi = false) {
  float gz = 0.0f;
  if (!dem.sample(wx, wy, &gz)) return;
  const double cx = dem.centerX(), cy = dem.centerY();
  const double rad = azDeg * M_PI / 180.0;
  const double cosA = std::cos(rad), sinA = std::sin(rad);
  auto toLocal = [&](double u, double v, float zRel) {
    const double x = wx + u * cosA + v * sinA;
    const double y = wy - u * sinA + v * cosA;
    return QVector3D(float(x - cx), float(y - cy), gz + zRel);
  };

  constexpr int kGridU = 8;
  constexpr int kGridV = 6;
  const double halfU = 0.5 * eaveWidthU;
  const double halfV = 0.5 * eaveDepthV;
  const double ridgeHalfU = std::max(0.20 * halfU, halfU - 0.58 * halfV);

  // 표면 높이 및 수평 위치 함수 (s in [-1, 1], t in [-1, 1])
  auto roofPt = [&](double s, double t) {
    const double absT = std::abs(t);
    const double absS = std::abs(s);
    // 정상부(t=0)에서는 용마루 폭(ridgeHalfU)으로 모이고, 처마(t=±1)에서는 안허리곡으로 모서리가 살짝 돌출
    const double cornerFlare = 1.0 + 0.06 * (absS * absS) * (absT * absT);
    const double spanU = (1.0 - absT) * ridgeHalfU + absT * halfU * cornerFlare;
    const double u = s * spanU;
    const double v = t * halfV * (1.0 + 0.05 * (absS * absS) * absT);

    // 오목한 전통 한옥 지붕 곡면 (현수선/포물선 프로파일: (1 - |t|)^1.38) + 처마 끝 앙곡(corner lift)
    const double slopeProfile = std::pow(std::max(0.0, 1.0 - absT), 1.38);
    const double cornerLift = 0.16 * double(roofH) * (absS * absS * absS) * (absT * absT);
    const float z = zEave + float(double(roofH) * slopeProfile + cornerLift);
    return toLocal(u, v, z);
  };

  for (int iu = 0; iu < kGridU; ++iu) {
    const double s0 = -1.0 + 2.0 * double(iu) / kGridU;
    const double s1 = -1.0 + 2.0 * double(iu + 1) / kGridU;
    for (int iv = 0; iv < kGridV; ++iv) {
      const double t0 = -1.0 + 2.0 * double(iv) / kGridV;
      const double t1 = -1.0 + 2.0 * double(iv + 1) / kGridV;
      addQuad(m, roofPt(s0, t0), roofPt(s1, t0), roofPt(s1, t1), roofPt(s0, t1), roofCol);
    }
  }

  // 용마루(석회/백토 마루턱) 및 황금 치미(鴟尾: 신라 왕궁·황룡사 금당 특유의 용마루 양끝 장식)
  const Color ridgeCol{0.34f, 0.37f, 0.42f, 1.0f};
  addOrientedBox(m, dem, wx, wy, zEave + roofH - 0.15f, 0.48f, ridgeHalfU * 2.02, 0.65, azDeg, ridgeCol);
  if (hasGoldenChimi) {
    const Color chimiGold{0.96f, 0.80f, 0.20f, 1.0f};
    for (int side : {-1, 1}) {
      const double uChimi = side * ridgeHalfU * 0.98;
      const double chX = wx + uChimi * cosA;
      const double chY = wy - uChimi * sinA;
      addOrientedBox(m, dem, chX, chY, zEave + roofH + 0.1f, 1.55f, 0.85, 1.15, azDeg, chimiGold);
    }
  }
}

// 실사형 신라 전통 전각/궁궐 건물 (장대석 기단 + 중앙 돌계단 + 붉은 배흘림 기둥열 + 회벽/창호 + 곡선 팔작지붕 + 중층 지원)
void addRealisticSillaHall(SceneMesh* m,
                           const DemGrid& dem,
                           double wx,
                           double wy,
                           double widthU,
                           double depthV,
                           float wallH,
                           float roofH,
                           double azDeg,
                           const Color& pillarCol,
                           const Color& roofCol,
                           bool twoStory = false,
                           bool goldenChimi = false) {
  float gz = 0.0f;
  if (!dem.sample(wx, wy, &gz)) return;
  const double rad = azDeg * M_PI / 180.0;
  const double cosA = std::cos(rad), sinA = std::sin(rad);

  // 1. 화강암 장대석 2단 기단 + 남쪽 중앙 답도(돌계단)
  const float baseH = std::clamp(wallH * 0.22f, 0.9f, 2.2f);
  const Color graniteBase{0.82f, 0.80f, 0.76f, 1.0f};
  const Color graniteTrim{0.72f, 0.70f, 0.66f, 1.0f};
  addOrientedBox(m, dem, wx, wy, 0.0f, baseH * 0.55f, widthU * 1.14, depthV * 1.14, azDeg, graniteTrim);
  addOrientedBox(m, dem, wx, wy, baseH * 0.55f, baseH * 0.45f, widthU * 1.07, depthV * 1.07, azDeg, graniteBase);

  // 남측 전면 돌계단
  const double stairV = -0.5 * depthV * 1.16;
  const double sx = wx + stairV * sinA;
  const double sy = wy + stairV * cosA;
  addOrientedBox(m, dem, sx, sy, 0.0f, baseH * 0.75f, std::min(6.0, widthU * 0.22), 2.4, azDeg, graniteBase);

  // 2. 내진 회백색/황토 회벽 코어 + 외진 주홍빛 배흘림 기둥열(칸수 계산)
  const Color innerWallCol{0.86f, 0.81f, 0.72f, 1.0f};
  const float story1H = twoStory ? wallH * 0.56f : wallH;
  addOrientedBox(m, dem, wx, wy, baseH, story1H, widthU * 0.90, depthV * 0.90, azDeg, innerWallCol);
  // 창방·평방·공포대 붉은 띠
  addOrientedBox(m, dem, wx, wy, baseH + story1H * 0.78f, story1H * 0.22f, widthU * 1.01, depthV * 1.01, azDeg, pillarCol);

  const int baysU = std::clamp(int(std::round(widthU / 4.8)), 3, 9);
  const int baysV = std::clamp(int(std::round(depthV / 4.8)), 2, 5);
  const double colRadius = std::clamp(widthU * 0.014, 0.28, 0.55);
  for (int iu = 0; iu <= baysU; ++iu) {
    for (int iv = 0; iv <= baysV; ++iv) {
      if (iu != 0 && iu != baysU && iv != 0 && iv != baysV) continue;  // 외곽 기둥열
      const double u = (-0.5 + double(iu) / baysU) * widthU * 0.98;
      const double v = (-0.5 + double(iv) / baysV) * depthV * 0.98;
      const double px = wx + u * cosA + v * sinA;
      const double py = wy - u * sinA + v * cosA;
      addCylinder(m, dem, px, py, baseH, baseH + story1H, colRadius, colRadius * 0.88, 8, pillarCol);
    }
  }

  // 3. 1층 곡선 처마 지붕 (중층 건물인 경우 1층 부연 처마 + 2층 탑신·팔작지붕)
  if (twoStory) {
    addCurvedPaljakRoof(m, dem, wx, wy, baseH + story1H, roofH * 0.52f, widthU * 1.25, depthV * 1.25, azDeg, roofCol, false);
    const float z2 = baseH + story1H + roofH * 0.36f;
    const float story2H = wallH * 0.48f;
    addOrientedBox(m, dem, wx, wy, z2, story2H, widthU * 0.74, depthV * 0.74, azDeg, pillarCol);
    addCurvedPaljakRoof(m, dem, wx, wy, z2 + story2H, roofH * 0.82f, widthU * 1.06, depthV * 1.06, azDeg, roofCol, goldenChimi);
  } else {
    addCurvedPaljakRoof(m, dem, wx, wy, baseH + story1H, roofH, widthU * 1.26, depthV * 1.26, azDeg, roofCol, goldenChimi);
  }
}

// 황룡사 9층목탑 (실제 심초석 위치 35.837180, 129.233130 · 높이 80m · 9층 곡선 기와처마 + 붉은 기둥 탑신 + 황금 상륜부)
void addHwangnyongsaNineStoryPagoda(SceneMesh* m,
                                    const DemGrid& dem,
                                    double wx,
                                    double wy,
                                    double azDeg) {
  float gz = 0.0f;
  if (!dem.sample(wx, wy, &gz)) return;

  const Color graniteCol{0.83f, 0.81f, 0.76f, 1.0f};
  const Color graniteDark{0.70f, 0.68f, 0.64f, 1.0f};
  const Color woodRedCol{0.78f, 0.21f, 0.15f, 1.0f};
  const Color roofTileCol{0.20f, 0.23f, 0.28f, 1.0f};
  const Color goldSpireCol{0.98f, 0.82f, 0.18f, 1.0f};

  // 화강암 상·하 이중 기단 (36m × 36m) 및 사방 계단
  addOrientedBox(m, dem, wx, wy, 0.0f, 2.0f, 36.0, 36.0, azDeg, graniteDark);
  addOrientedBox(m, dem, wx, wy, 2.0f, 1.8f, 30.5, 30.5, azDeg, graniteCol);
  addOrientedBox(m, dem, wx, wy, 0.0f, 3.4f, 7.5, 39.0, azDeg, graniteCol);
  addOrientedBox(m, dem, wx, wy, 0.0f, 3.4f, 39.0, 7.5, azDeg, graniteCol);

  float curZ = 3.8f;
  for (int story = 0; story < 9; ++story) {
    const double t = double(story) / 8.0;
    const double coreW = 22.4 * (1.0 - 0.52 * t);
    const double eaveW = coreW * 1.42;
    const float storyWallH = float(4.3 - 1.45 * t);
    const float storyRoofH = float(2.5 - 0.75 * t);

    // 층별 붉은 목조 탑신 + 모서리 기둥 강조
    addOrientedBox(m, dem, wx, wy, curZ, storyWallH, coreW, coreW, azDeg, woodRedCol);
    curZ += storyWallH;

    // 층별 곡선 처마 기와지붕 (앙곡이 살아있는 4면 우진각 옥개)
    addCurvedPaljakRoof(m, dem, wx, wy, curZ - 0.3f, storyRoofH, eaveW, eaveW, azDeg, roofTileCol, false);
    curZ += storyRoofH * 0.86f;
  }

  // 상륜부: 노반·복발·앙화·보륜 9단·찰주 (황금빛 총고 80m)
  addOrientedBox(m, dem, wx, wy, curZ, 2.2f, 5.4, 5.4, azDeg, goldSpireCol);
  addCylinder(m, dem, wx, wy, curZ + 2.2f, curZ + 4.5f, 2.4, 1.6, 12, goldSpireCol);
  for (int ring = 0; ring < 7; ++ring) {
    const float rz = curZ + 4.8f + ring * 1.35f;
    const double rr = 1.95 * (1.0 - 0.08 * ring);
    addCylinder(m, dem, wx, wy, rz, rz + 0.45f, rr, rr, 12, goldSpireCol);
  }
  addCylinder(m, dem, wx, wy, curZ + 2.2f, 80.0f, 0.55, 0.18, 10, goldSpireCol);
}

// 국보 제31호 경주 첨성대 (실제 위치 35.834702, 129.218984 · 정방형 기단 + 27단 곡선 병 모양 원통 석축 + 남측 창문 + 2단 정(井)자 정상석)
void addRealisticCheomseongdae(SceneMesh* m, const DemGrid& dem, double wx, double wy) {
  float gz = 0.0f;
  if (!dem.sample(wx, wy, &gz)) return;
  const Color stoneLight{0.82f, 0.79f, 0.72f, 1.0f};
  const Color stoneMid{0.75f, 0.72f, 0.65f, 1.0f};
  const Color darkWindow{0.18f, 0.19f, 0.22f, 1.0f};

  // 1. 기단부 (5.35m × 5.35m, 시인성을 위해 1.35배 스케일 보정)
  constexpr double kScale = 1.45;
  addOrientedBox(m, dem, wx, wy, 0.0f, float(1.25 * kScale), 5.4 * kScale, 5.4 * kScale, 0.0, stoneMid);

  // 2. 27단 유선형 원통부 (하이퍼볼릭 오목 곡선 프로파일)
  constexpr int kTiers = 12;
  const float zStart = float(1.25 * kScale);
  const float bodyH = float(6.85 * kScale);
  for (int i = 0; i < kTiers; ++i) {
    const double u0 = double(i) / kTiers;
    const double u1 = double(i + 1) / kTiers;
    auto radiusAt = [&](double u) {
      // 하단 반경 2.58m -> 상단 반경 1.45m (중간 허리가 우아하게 오목한 곡선)
      const double curve = std::pow(1.0 - u, 1.65);
      return (1.45 + (2.58 - 1.45) * curve) * kScale;
    };
    const float z0 = zStart + float(u0 * bodyH);
    const float z1 = zStart + float(u1 * bodyH);
    addCylinder(m, dem, wx, wy, z0, z1, radiusAt(u0), radiusAt(u1), 20, (i % 2 == 0) ? stoneLight : stoneMid);
  }

  // 3. 남쪽 중앙 창문 (13~15단 높이)
  addOrientedBox(m, dem, wx, wy - 1.95 * kScale, zStart + bodyH * 0.46f, float(1.15 * kScale),
                 1.15 * kScale, 0.8 * kScale, 0.0, darkWindow);
  addOrientedBox(m, dem, wx, wy - 2.02 * kScale, zStart + bodyH * 0.43f, float(1.35 * kScale),
                 1.45 * kScale, 0.35 * kScale, 0.0, stoneLight);

  // 4. 정상부 2단 정(井)자형 석재 (우물 정자 모양 교차 장대석)
  const float zTop = zStart + bodyH;
  const double topSpan = 3.2 * kScale;
  const double beamThick = 0.55 * kScale;
  for (int side : {-1, 1}) {
    addOrientedBox(m, dem, wx + side * 1.15 * kScale, wy, zTop, float(1.05 * kScale), beamThick, topSpan, 0.0, stoneLight);
    addOrientedBox(m, dem, wx, wy + side * 1.15 * kScale, zTop, float(1.05 * kScale), topSpan, beamThick, 0.0, stoneLight);
  }
}

// 국보 제30호 분황사 모전석탑 (실제 위치 35.840566, 129.233618 · 화강암 기단 + 네 모서리 돌사자 + 안산암 벽돌 3층 옥개)
void addBunhwangsaBrickPagoda(SceneMesh* m, const DemGrid& dem, double wx, double wy) {
  float gz = 0.0f;
  if (!dem.sample(wx, wy, &gz)) return;
  const Color graniteBase{0.80f, 0.78f, 0.73f, 1.0f};
  const Color lionCol{0.68f, 0.65f, 0.60f, 1.0f};
  const Color brickDark{0.34f, 0.36f, 0.39f, 1.0f};
  const Color brickStep{0.27f, 0.29f, 0.32f, 1.0f};

  // 화강암 단층 기단 (13m × 13m) + 네 귀퉁이 돌사자상
  addOrientedBox(m, dem, wx, wy, 0.0f, 1.8f, 13.5, 13.5, 0.0, graniteBase);
  for (int sx : {-1, 1}) {
    for (int sy : {-1, 1}) {
      addOrientedBox(m, dem, wx + sx * 5.6, wy + sy * 5.6, 1.8f, 1.5f, 1.6, 1.6, 0.0, lionCol);
    }
  }

  // 1~3층 안산암 모전 벽돌 탑신 및 층급받침·지붕
  float curZ = 1.8f;
  const double widths[3] = {9.8, 7.0, 4.8};
  const float heights[3] = {3.8f, 2.4f, 1.8f};
  for (int st = 0; st < 3; ++st) {
    addOrientedBox(m, dem, wx, wy, curZ, heights[st], widths[st], widths[st], 0.0, brickDark);
    if (st == 0) {
      // 1층 사방 화강암 감실 문비 및 인왕상 테두리
      addOrientedBox(m, dem, wx, wy, curZ + 0.3f, 2.6f, widths[0] * 1.04, 2.2, 0.0, graniteBase);
      addOrientedBox(m, dem, wx, wy, curZ + 0.3f, 2.6f, 2.2, widths[0] * 1.04, 0.0, graniteBase);
    }
    curZ += heights[st];
    // 벽돌 층급 옥개석
    for (int step = 0; step < 3; ++step) {
      const double sw = widths[st] * (1.20 - 0.10 * step);
      addOrientedBox(m, dem, wx, wy, curZ + step * 0.45f, 0.45f, sw, sw, 0.0, brickStep);
    }
    curZ += 1.35f;
  }
}

// 신라 전형 3층 화강암 석탑 (황복사지 삼층석탑 등)
void addSillaThreeStoryStonePagoda(SceneMesh* m, const DemGrid& dem, double wx, double wy) {
  float gz = 0.0f;
  if (!dem.sample(wx, wy, &gz)) return;
  const Color stoneLight{0.84f, 0.82f, 0.78f, 1.0f};
  const Color stoneRoof{0.74f, 0.72f, 0.68f, 1.0f};

  addOrientedBox(m, dem, wx, wy, 0.0f, 1.4f, 8.5, 8.5, 0.0, stoneRoof);
  addOrientedBox(m, dem, wx, wy, 1.4f, 1.6f, 6.2, 6.2, 0.0, stoneLight);
  float z = 3.0f;
  for (int i = 0; i < 3; ++i) {
    const double w = 3.8 * (1.0 - 0.22 * i);
    const float h = 2.2f - 0.35f * i;
    addOrientedBox(m, dem, wx, wy, z, h, w, w, 0.0, stoneLight);
    z += h;
    addOrientedBox(m, dem, wx, wy, z, 0.85f, w * 1.42, w * 1.42, 0.0, stoneRoof);
    z += 0.85f;
  }
  addCylinder(m, dem, wx, wy, z, z + 2.2f, 0.45, 0.15, 8, stoneLight);
}

// 사적 제457호 경주 월정교 (실제 위성 위치 35.829250, 129.218140 · 주형 석조 교각 4기 + 단청 회랑 교량 + 남·북 중층 문루 2동)
void addRealisticWoljeonggyo(SceneMesh* m, const DemGrid& dem, double wx, double wy, double azDeg) {
  float gz = 0.0f;
  if (!dem.sample(wx, wy, &gz)) return;
  const double rad = azDeg * M_PI / 180.0;
  const double cosA = std::cos(rad), sinA = std::sin(rad);

  const Color granitePier{0.78f, 0.76f, 0.72f, 1.0f};
  const Color royalRed{0.78f, 0.21f, 0.16f, 1.0f};
  const Color roofCol{0.19f, 0.22f, 0.27f, 1.0f};

  // 1. 남천 강물 위 주형(배 모양) 화강암 교각 4기 + 양안 교대
  for (double vOff : {-22.0, -7.5, +7.5, +22.0}) {
    const double px = wx + vOff * sinA;
    const double py = wy + vOff * cosA;
    addOrientedBox(m, dem, px, py, -1.0f, 5.2f, 11.8, 3.4, azDeg, granitePier);
  }

  // 2. 교량 목조 상판 및 단청 회랑 본체 (길이 61m × 폭 9.5m)
  addOrientedBox(m, dem, wx, wy, 3.8f, 1.4f, 10.5, 62.0, azDeg, royalRed);
  addRealisticSillaHall(m, dem, wx, wy, 9.2, 56.0, 5.2f, 3.8f, azDeg, royalRed, roofCol, false, false);

  // 3. 남·북 양단 중층 문루(2층 누각) 2동 (용마루가 동서 방향으로 놓인 중층 팔작지붕)
  for (double vGate : {-31.5, +31.5}) {
    const double gx = wx + vGate * sinA;
    const double gy = wy + vGate * cosA;
    addRealisticSillaHall(m, dem, gx, gy, 21.5, 13.5, 9.8f, 6.4f, azDeg, royalRed, roofCol, true, true);
  }
}

// 다각형 수면(월지 연못·월성 해자·구황동 원지) 및 호안 석축 생성
void addPolygonWaterPond(SceneMesh* m,
                         const DemGrid& dem,
                         const std::vector<std::pair<double, double>>& latLons,
                         const CrsTransformer& tr,
                         float liftM,
                         const Color& waterCol,
                         const Color& bankCol) {
  std::vector<Pt> ring;
  ring.reserve(latLons.size());
  for (const auto& ll : latLons) {
    double x = ll.second, y = ll.first;
    if (tr.transform(&x, &y)) ring.push_back({x, y});
  }
  if (ring.size() < 3) return;
  if (signedArea(ring) < 0) std::reverse(ring.begin(), ring.end());

  std::vector<uint32_t> tris;
  if (!triangulateRing(ring, &tris) || tris.empty()) return;

  const double cx = dem.centerX(), cy = dem.centerY();
  const uint32_t baseIdx = uint32_t(m->vertices.size() / SceneMesh::kFloatsPerVertex);
  for (const Pt& pt : ring) {
    float gz = 0.0f;
    if (!dem.sample(pt.x, pt.y, &gz)) return;
    addVert(m, QVector3D(float(pt.x - cx), float(pt.y - cy), gz + liftM), QVector3D(0, 0, 1), waterCol);
  }
  for (size_t i = 0; i + 2 < tris.size(); i += 3) {
    m->indices.insert(m->indices.end(), {baseIdx + tris[i], baseIdx + tris[i + 1], baseIdx + tris[i + 2]});
  }

  // 호안 석축 테두리
  appendDrapedRibbon(m, dem, ring, true, 2.6f, bankCol, liftM + 0.35f);
}

// 두 점 사이를 잇는 회랑/성벽 세그먼트
void addCorridorOrRampartSegment(SceneMesh* m,
                                 const DemGrid& dem,
                                 const Pt& a,
                                 const Pt& b,
                                 float wallH,
                                 double thicknessM,
                                 const Color& wallCol,
                                 const Color& roofCol) {
  const double len = std::hypot(b.x - a.x, b.y - a.y);
  if (len < 3.0) return;
  const double mx = 0.5 * (a.x + b.x);
  const double my = 0.5 * (a.y + b.y);
  const double azDeg = std::atan2(b.x - a.x, b.y - a.y) * 180.0 / M_PI;
  addOrientedBox(m, dem, mx, my, 0.0f, wallH, thicknessM, len, azDeg, wallCol);
  addOrientedBox(m, dem, mx, my, wallH, 1.1f, thicknessM * 1.38, len + 0.8, azDeg, roofCol);
}

double pointToSegmentDist(const Pt& m, const Pt& p, const Pt& q) {
  const double vx = q.x - p.x;
  const double vy = q.y - p.y;
  const double lenSq = vx * vx + vy * vy;
  if (lenSq < 1e-9) return std::hypot(m.x - p.x, m.y - p.y);
  const double t = std::clamp(((m.x - p.x) * vx + (m.y - p.y) * vy) / lenSq, 0.0, 1.0);
  return std::hypot(m.x - (p.x + t * vx), m.y - (p.y + t * vy));
}

bool isInsidePaleoChannel(const Pt& p, const std::vector<PaleoChannelCorridor>& channels) {
  for (const PaleoChannelCorridor& ch : channels) {
    for (size_t i = 0; i + 1 < ch.centerline.size(); ++i) {
      if (pointToSegmentDist(p, ch.centerline[i], ch.centerline[i + 1]) <= ch.halfWidthM) {
        return true;
      }
    }
  }
  return false;
}

double sampleSlopeDeg(const DemGrid& dem, double x, double y) {
  const double step = std::max(10.0, dem.pixelSize);
  float zE = 0, zW = 0, zN = 0, zS = 0;
  if (!dem.sample(x + step, y, &zE) || !dem.sample(x - step, y, &zW) ||
      !dem.sample(x, y + step, &zN) || !dem.sample(x, y - step, &zS)) {
    return 0.0;
  }
  const double dzdx = double(zE - zW) / (2.0 * step);
  const double dzdy = double(zN - zS) / (2.0 * step);
  return std::atan(std::hypot(dzdx, dzdy)) * 180.0 / M_PI;
}

}  // namespace

std::vector<PaleoChannelCorridor> BangriTerrainValidator::builtInPaleoChannels(const QString& workCrs) {
  std::vector<PaleoChannelCorridor> out;
  const CrsTransformer tr(QStringLiteral("EPSG:4326"), workCrs);
  if (!tr.valid()) return out;

  auto addChannel = [&](const char* id, const char16_t* name, double halfW,
                        std::initializer_list<std::pair<double, double>> latLons) {
    PaleoChannelCorridor c;
    c.id = QString::fromLatin1(id);
    c.name = QString::fromUtf16(name);
    c.halfWidthM = halfW;
    for (const auto& ll : latLons) {
      double x = ll.second, y = ll.first;
      if (tr.transform(&x, &y)) c.centerline.push_back({x, y});
    }
    if (c.centerline.size() >= 2) out.push_back(std::move(c));
  };

  addChannel("ch:bukcheon_main", u"북천(알천) 본류 및 제방 구간", 55.0,
             {{35.8505, 129.2480}, {35.8492, 129.2340}, {35.8508, 129.2200}, {35.8535, 129.2060}});

  addChannel("ch:bukcheon_paleo", u"북천 남안 구하도(분황사~성동동 옛 물길)", 32.0,
             {{35.8465, 129.2390}, {35.8438, 129.2285}, {35.8430, 129.2185}, {35.8442, 129.2080}});

  addChannel("ch:namcheon_woljeong", u"남천(문천) 및 월정교·춘양교 수로 구간", 45.0,
             {{35.8268, 129.2380}, {35.8288, 129.2265}, {35.8292, 129.2175}, {35.8280, 129.2050}});

  addChannel("ch:balcheon", u"발천(撥川) 궁성·동궁 배수 수로망", 18.0,
             {{35.8355, 129.2285}, {35.8332, 129.2235}, {35.8305, 129.2195}, {35.8290, 129.2175}});

  return out;
}

BangriValidationResult BangriTerrainValidator::validate(
    const DemGrid& dem,
    const BangriGridResult& grid,
    const std::vector<SillaMound>& mounds) {
  BangriValidationResult out;
  if (!dem.valid()) return out;

  out.channels = builtInPaleoChannels(dem.crs);
  constexpr double kStepM = 18.0;

  for (const BangriRoadSegment& r : grid.roads) {
    const double totalSegLen = std::hypot(r.b.x - r.a.x, r.b.y - r.a.y);
    if (totalSegLen < 1.0) continue;
    const int nSteps = std::max(1, int(std::ceil(totalSegLen / kStepM)));

    for (int i = 0; i < nSteps; ++i) {
      const double t0 = double(i) / double(nSteps);
      const double t1 = double(i + 1) / double(nSteps);
      const double tm = 0.5 * (t0 + t1);

      const Pt a{r.a.x + (r.b.x - r.a.x) * t0, r.a.y + (r.b.y - r.a.y) * t0};
      const Pt b{r.a.x + (r.b.x - r.a.x) * t1, r.a.y + (r.b.y - r.a.y) * t1};
      const Pt mid{r.a.x + (r.b.x - r.a.x) * tm, r.a.y + (r.b.y - r.a.y) * tm};

      float zMid = 0.0f;
      if (!dem.sample(mid.x, mid.y, &zMid)) continue;

      const double chunkLen = std::hypot(b.x - a.x, b.y - a.y);
      const double slopeDeg = sampleSlopeDeg(dem, mid.x, mid.y);
      const double moundH = SillaMoundReconstructor::sampleMoundHeight(mid.x, mid.y, mounds);
      const bool inRiver = isInsidePaleoChannel(mid, out.channels);

      BangriRoadFeasibility feas = BangriRoadFeasibility::ValidFlat;
      if (moundH > 0.35) {
        feas = BangriRoadFeasibility::MoundInterference;
        out.moundInterferenceLengthM += chunkLen;
      } else if (slopeDeg >= 8.0) {
        feas = BangriRoadFeasibility::SteepTerrain;
        out.steepTerrainLengthM += chunkLen;
      } else if (inRiver) {
        feas = BangriRoadFeasibility::RiverOrPaleoChannel;
        out.riverChannelLengthM += chunkLen;
      } else {
        feas = BangriRoadFeasibility::ValidFlat;
        out.validFlatLengthM += chunkLen;
      }
      out.totalLengthM += chunkLen;

      ValidatedRoadChunk c;
      c.a = a;
      c.b = b;
      c.tier = r.tier;
      c.feasibility = feas;
      c.widthM = r.widthM;
      c.ditchWidthM = r.ditchWidthM;
      c.slopeDeg = slopeDeg;
      c.moundHeightM = moundH;
      c.leftDitchA = {r.leftDitchA.x + (r.leftDitchB.x - r.leftDitchA.x) * t0,
                      r.leftDitchA.y + (r.leftDitchB.y - r.leftDitchA.y) * t0};
      c.leftDitchB = {r.leftDitchA.x + (r.leftDitchB.x - r.leftDitchA.x) * t1,
                      r.leftDitchA.y + (r.leftDitchB.y - r.leftDitchA.y) * t1};
      c.rightDitchA = {r.rightDitchA.x + (r.rightDitchB.x - r.rightDitchA.x) * t0,
                       r.rightDitchA.y + (r.rightDitchB.y - r.rightDitchA.y) * t0};
      c.rightDitchB = {r.rightDitchA.x + (r.rightDitchB.x - r.rightDitchA.x) * t1,
                       r.rightDitchA.y + (r.rightDitchB.y - r.rightDitchA.y) * t1};
      out.chunks.push_back(std::move(c));
    }
  }

  if (out.totalLengthM > 0.0) {
    out.validFlatPct = (out.validFlatLengthM / out.totalLengthM) * 100.0;
    out.moundInterferencePct = (out.moundInterferenceLengthM / out.totalLengthM) * 100.0;
    out.riverChannelPct = (out.riverChannelLengthM / out.totalLengthM) * 100.0;
    out.steepTerrainPct = (out.steepTerrainLengthM / out.totalLengthM) * 100.0;
  }
  return out;
}

std::shared_ptr<SceneMesh> BangriTerrainValidator::buildValidatedGridOverlay(
    const DemGrid& dem,
    const BangriGridResult& grid,
    const BangriValidationResult& validation,
    const std::vector<BangriAnchor>& anchors,
    const std::vector<BangriAnchorResidual>& residuals,
    const std::vector<SillaMound>& /*mounds*/,
    bool include3dCityArchitecture,
    bool includeRoadGrid) {
  auto mesh = std::make_shared<SceneMesh>();
  if (!dem.valid()) return mesh;

  // 1. 사용자가 "가상 방리 구획선 표시"를 켠 경우에만 간선 도로를 표시하며, 내부 소획 구획선(InnerAlley)과 가상 가옥은 절대 그리지 않는다.
  if (includeRoadGrid) {
    const Color mainRoadCol{0.88f, 0.72f, 0.28f, 1.0f};
    const Color interRoadCol{0.80f, 0.64f, 0.30f, 1.0f};
    for (const ValidatedRoadChunk& c : validation.chunks) {
      if (c.tier == BangriRoadTier::InnerAlley) continue;  // 내부 구획선은 전부 제거
      if (c.feasibility != BangriRoadFeasibility::ValidFlat) continue;
      const auto& col = (c.tier == BangriRoadTier::MainAvenue) ? mainRoadCol : interRoadCol;
      appendDrapedRibbon(mesh.get(), dem, {c.a, c.b}, false, float(c.widthM * 0.7), col, 1.2f);
    }
    const Color anchorPoleCol{0.96f, 0.16f, 0.58f, 1.0f};
    for (const BangriAnchor& a : anchors) {
      if (a.active) appendPole(mesh.get(), dem, {a.x, a.y}, 3.5f, 28.0f, anchorPoleCol);
    }
    (void)residuals;
  }

  // 2. 경주 실제 유적들의 위성 실사 1:1 정밀 좌표 및 실제 형태 3D 모델링
  if (include3dCityArchitecture) {
    const Color royalPillarCol{0.78f, 0.21f, 0.15f, 1.0f};   // 궁궐·대사찰 주홍 단청 기둥
    const Color hanokWoodCol{0.58f, 0.36f, 0.20f, 1.0f};     // 향교·고택 전통 목조 기둥
    const Color palaceRoofCol{0.19f, 0.22f, 0.27f, 1.0f};    // 신라 전통 수키와·암키와 진회색 지붕
    const Color waterPondCol{0.09f, 0.42f, 0.52f, 1.0f};     // 월지·해자·원지 깊은 옥빛 수면
    const Color stoneBankCol{0.75f, 0.73f, 0.68f, 1.0f};     // 호안 화강암 장대석 석축
    const Color islandGreenCol{0.24f, 0.52f, 0.22f, 1.0f};   // 삼신산 인공섬 및 계림 숲 녹지

    const CrsTransformer tr(QStringLiteral("EPSG:4326"), dem.crs);
    auto toWorld = [&](double lat, double lon, double* wx, double* wy) {
      *wx = lon;
      *wy = lat;
      if (!tr.valid() || !tr.transform(wx, wy)) return false;
      float gz = 0.0f;
      return dem.sample(*wx, *wy, &gz);
    };

    if (grid.params.presetId.startsWith(QLatin1String("gyeongju"))) {
      double wx = 0.0, wy = 0.0;

      // =========================================================================
      // [유적 1] 경주 황룡사지 (사적 제6호 · 위성 실사 실제 유구 좌표계, 방위각 -1.5°)
      // =========================================================================
      const double hwangAz = -1.5;
      // (1-1) 황룡사 9층목탑 (실제 심초석 중심: 35.837180, 129.233130)
      if (toWorld(35.837180, 129.233130, &wx, &wy)) {
        addHwangnyongsaNineStoryPagoda(mesh.get(), dem, wx, wy, hwangAz);
      }
      // (1-2) 황룡사 중금당 (실제 기단 중심: 35.837550, 129.233110 · 장육존상 봉안 중층 대금당)
      if (toWorld(35.837550, 129.233110, &wx, &wy)) {
        addRealisticSillaHall(mesh.get(), dem, wx, wy, 51.0, 27.0, 12.2f, 8.2f, hwangAz, royalPillarCol, palaceRoofCol, true, true);
      }
      // (1-3) 황룡사 서금당 (35.837480, 129.232580) & 동금당 (35.837500, 129.233640)
      if (toWorld(35.837480, 129.232580, &wx, &wy)) {
        addRealisticSillaHall(mesh.get(), dem, wx, wy, 26.5, 19.5, 8.5f, 5.8f, hwangAz, royalPillarCol, palaceRoofCol, false, true);
      }
      if (toWorld(35.837500, 129.233640, &wx, &wy)) {
        addRealisticSillaHall(mesh.get(), dem, wx, wy, 26.5, 19.5, 8.5f, 5.8f, hwangAz, royalPillarCol, palaceRoofCol, false, true);
      }
      // (1-4) 황룡사 강당지 (35.837950, 129.233090)
      if (toWorld(35.837950, 129.233090, &wx, &wy)) {
        addRealisticSillaHall(mesh.get(), dem, wx, wy, 55.0, 21.0, 9.2f, 6.2f, hwangAz, royalPillarCol, palaceRoofCol, false, true);
      }
      // (1-5) 황룡사 종루 (35.837030, 129.232780) & 경루 (35.837040, 129.233480)
      if (toWorld(35.837030, 129.232780, &wx, &wy)) {
        addRealisticSillaHall(mesh.get(), dem, wx, wy, 13.5, 13.5, 7.6f, 5.0f, hwangAz, royalPillarCol, palaceRoofCol, true, false);
      }
      if (toWorld(35.837040, 129.233480, &wx, &wy)) {
        addRealisticSillaHall(mesh.get(), dem, wx, wy, 13.5, 13.5, 7.6f, 5.0f, hwangAz, royalPillarCol, palaceRoofCol, true, false);
      }
      // (1-6) 황룡사 중문 (35.836770, 129.233150) & 남문 (35.836520, 129.233160)
      if (toWorld(35.836770, 129.233150, &wx, &wy)) {
        addRealisticSillaHall(mesh.get(), dem, wx, wy, 31.0, 14.5, 8.8f, 5.8f, hwangAz, royalPillarCol, palaceRoofCol, true, true);
      }
      if (toWorld(35.836520, 129.233160, &wx, &wy)) {
        addRealisticSillaHall(mesh.get(), dem, wx, wy, 26.0, 12.5, 7.2f, 4.8f, hwangAz, royalPillarCol, palaceRoofCol, false, true);
      }
      // (1-7) 황룡사 실제 사역 외곽 회랑 (위성 유구 경계: 35.83675~35.83795, 129.23212~129.23412)
      {
        Pt cSW{}, cSE{}, cNE{}, cNW{};
        if (toWorld(35.836770, 129.232120, &cSW.x, &cSW.y) &&
            toWorld(35.836770, 129.234140, &cSE.x, &cSE.y) &&
            toWorld(35.837950, 129.234100, &cNE.x, &cNE.y) &&
            toWorld(35.837950, 129.232090, &cNW.x, &cNW.y)) {
          addCorridorOrRampartSegment(mesh.get(), dem, cSW, cSE, 4.2f, 4.6, royalPillarCol, palaceRoofCol);
          addCorridorOrRampartSegment(mesh.get(), dem, cSE, cNE, 4.2f, 4.6, royalPillarCol, palaceRoofCol);
          addCorridorOrRampartSegment(mesh.get(), dem, cNE, cNW, 4.2f, 4.6, royalPillarCol, palaceRoofCol);
          addCorridorOrRampartSegment(mesh.get(), dem, cNW, cSW, 4.2f, 4.6, royalPillarCol, palaceRoofCol);
        }
      }

      // =========================================================================
      // [유적 2] 분황사 모전석탑 (국보 제30호 · 35.840566, 129.233618) & 보광전 & 구황동 원지
      // =========================================================================
      if (toWorld(35.840566, 129.233618, &wx, &wy)) {
        addBunhwangsaBrickPagoda(mesh.get(), dem, wx, wy);
      }
      if (toWorld(35.840890, 129.233640, &wx, &wy)) {
        addRealisticSillaHall(mesh.get(), dem, wx, wy, 19.0, 12.0, 6.8f, 4.6f, 0.0, royalPillarCol, palaceRoofCol, false, false);
      }
      // 구황동 원지 유적 (사적 제548호 · 실제 연못 및 2개 인공섬: 35.84055, 129.23518)
      addPolygonWaterPond(mesh.get(), dem,
                          {{35.84032, 129.23492}, {35.84030, 129.23542}, {35.84078, 129.23545},
                           {35.84085, 129.23512}, {35.84062, 129.23488}},
                          tr, 0.5f, waterPondCol, stoneBankCol);
      if (toWorld(35.840540, 129.235180, &wx, &wy)) {
        addCylinder(mesh.get(), dem, wx, wy, 0.4f, 2.2f, 11.0, 8.0, 12, islandGreenCol);
      }

      // =========================================================================
      // [유적 3] 동궁과 월지 (안압지 · 사적 제18호 · 실제 곡선 호안 + 삼신산 3섬 + 제1·2·3건물지)
      // =========================================================================
      addPolygonWaterPond(mesh.get(), dem,
                          {{35.83404, 129.22682}, {35.83403, 129.22752}, {35.83442, 129.22778},
                           {35.83502, 129.22775}, {35.83552, 129.22755}, {35.83568, 129.22685},
                           {35.83562, 129.22605}, {35.83505, 129.22603}, {35.83482, 129.22606},
                           {35.83480, 129.22658}, {35.83422, 129.22676}},
                          tr, 0.5f, waterPondCol, stoneBankCol);
      // 월지 연못 내 3개 인공섬: 대도(남동)·중도(북동)·소도(북서)
      if (toWorld(35.834380, 129.227320, &wx, &wy)) {
        addCylinder(mesh.get(), dem, wx, wy, 0.4f, 3.8f, 21.0, 14.0, 16, islandGreenCol);
      }
      if (toWorld(35.835060, 129.227280, &wx, &wy)) {
        addCylinder(mesh.get(), dem, wx, wy, 0.4f, 3.4f, 16.0, 10.5, 14, islandGreenCol);
      }
      if (toWorld(35.835430, 129.226380, &wx, &wy)) {
        addCylinder(mesh.get(), dem, wx, wy, 0.4f, 2.8f, 11.0, 7.5, 12, islandGreenCol);
      }
      // 동궁과 월지 서안 실제 복원 3대 누각 (OSM 477417217, 477417218, 477417219 정밀 좌표)
      if (toWorld(35.834149, 129.226798, &wx, &wy)) {  // 제1건물(임해전 남측 누각)
        addRealisticSillaHall(mesh.get(), dem, wx, wy, 16.5, 18.5, 7.8f, 5.4f, -2.0, royalPillarCol, palaceRoofCol, false, true);
      }
      if (toWorld(35.834788, 129.226537, &wx, &wy)) {  // 제2건물(임해전 중앙 대누각)
        addRealisticSillaHall(mesh.get(), dem, wx, wy, 23.5, 21.0, 9.2f, 6.4f, -2.0, royalPillarCol, palaceRoofCol, false, true);
      }
      if (toWorld(35.835087, 129.225999, &wx, &wy)) {  // 제3건물(임해전 북서 누각)
        addRealisticSillaHall(mesh.get(), dem, wx, wy, 13.0, 14.5, 7.0f, 4.8f, -2.0, royalPillarCol, palaceRoofCol, false, true);
      }
      // 동궁 서편 왕궁 전각군 기단·건물지 (35.83445, 129.22632)
      if (toWorld(35.834450, 129.226320, &wx, &wy)) {
        addRealisticSillaHall(mesh.get(), dem, wx, wy, 36.0, 18.0, 8.8f, 6.0f, -2.0, royalPillarCol, palaceRoofCol, false, true);
      }

      // =========================================================================
      // [유적 4] 경주 월성 (반월성 · 사적 제16호 · 실제 초승달 모양 성곽 + 북편 해자 + 왕궁 정전 + 석빙고)
      // =========================================================================
      {
        // OSM 42437237 / A·B·C·D지구에서 추출한 실제 반월(초승달) 성벽 외곽선
        static constexpr std::pair<double, double> kWolseongCrescent[] = {
            {35.83149, 129.21842}, {35.83086, 129.21847}, {35.83009, 129.21806},
            {35.82959, 129.21840}, {35.82930, 129.21905}, {35.82907, 129.22049},
            {35.82972, 129.22127}, {35.83099, 129.22188}, {35.83157, 129.22320},
            {35.83178, 129.22480}, {35.83042, 129.22666}, {35.83071, 129.22776},
            {35.83158, 129.22862}, {35.83190, 129.22808}, {35.83255, 129.22601},
            {35.83317, 129.22500}, {35.83300, 129.22429}, {35.83281, 129.22308},
            {35.83246, 129.22206}, {35.83193, 129.22093}, {35.83153, 129.22061},
            {35.83131, 129.21914}, {35.83149, 129.21842}};
        std::vector<Pt> wallPts;
        for (const auto& ll : kWolseongCrescent) {
          double x = 0, y = 0;
          if (toWorld(ll.first, ll.second, &x, &y)) wallPts.push_back({x, y});
        }
        const Color rampartEarth{0.58f, 0.52f, 0.42f, 1.0f};
        const Color rampartParapet{0.72f, 0.70f, 0.65f, 1.0f};
        for (size_t i = 0; i + 1 < wallPts.size(); ++i) {
          addCorridorOrRampartSegment(mesh.get(), dem, wallPts[i], wallPts[i + 1], 5.8f, 9.5, rampartEarth, rampartParapet);
        }

        // 월성 북편 해자 (위성 실사에서 선명히 보이는 계단식 연못 해자 4구간)
        addPolygonWaterPond(mesh.get(), dem,
                            {{35.83235, 129.22065}, {35.83275, 129.22155}, {35.83302, 129.22138}, {35.83260, 129.22048}},
                            tr, 0.4f, waterPondCol, stoneBankCol);
        addPolygonWaterPond(mesh.get(), dem,
                            {{35.83282, 129.22170}, {35.83318, 129.22268}, {35.83348, 129.22252}, {35.83312, 129.22155}},
                            tr, 0.4f, waterPondCol, stoneBankCol);
        addPolygonWaterPond(mesh.get(), dem,
                            {{35.83322, 129.22282}, {35.83342, 129.22382}, {35.83372, 129.22375}, {35.83352, 129.22274}},
                            tr, 0.4f, waterPondCol, stoneBankCol);

        // 월성 내부 왕궁 정전군 (B지구 중심 정전 35.83183, 129.22208 & C지구 발굴 정전 35.83245, 129.22415)
        if (toWorld(35.831830, 129.222080, &wx, &wy)) {
          addRealisticSillaHall(mesh.get(), dem, wx, wy, 48.0, 24.0, 11.5f, 7.8f, -14.0, royalPillarCol, palaceRoofCol, true, true);
        }
        if (toWorld(35.832450, 129.224150, &wx, &wy)) {
          addRealisticSillaHall(mesh.get(), dem, wx, wy, 38.0, 19.0, 9.2f, 6.2f, -10.0, royalPillarCol, palaceRoofCol, false, true);
        }
        // 보물 제66호 경주 석빙고 (실제 위치: 35.833140, 129.223950 · 아치형 화강암 홍예와 잔디 봉토)
        if (toWorld(35.833140, 129.223950, &wx, &wy)) {
          addOrientedBox(mesh.get(), dem, wx, wy, 0.0f, 2.8f, 9.5, 19.8, 5.0, stoneBankCol);
          addOrientedBox(mesh.get(), dem, wx, wy, 2.8f, 2.2f, 7.8, 18.2, 5.0, islandGreenCol);
        }
      }

      // =========================================================================
      // [유적 5] 국보 제31호 경주 첨성대 (실제 위치: 35.834702, 129.218984)
      // =========================================================================
      if (toWorld(35.834702, 129.218984, &wx, &wy)) {
        addRealisticCheomseongdae(mesh.get(), dem, wx, wy);
      }

      // =========================================================================
      // [유적 6] 사적 제457호 경주 월정교 (실제 위성 위치: 35.829250, 129.218140, 방위각 +1.0°)
      // =========================================================================
      if (toWorld(35.829250, 129.218140, &wx, &wy)) {
        addRealisticWoljeonggyo(mesh.get(), dem, wx, wy, 1.0);
      }

      // =========================================================================
      // [유적 7] 경주 계림 (사적 제19호) · 경주향교 (보물 대성전·명륜당) · 교동 최부자댁
      // =========================================================================
      // 계림 김알지 탄강 유허비각 (35.832350, 129.217920)
      if (toWorld(35.832350, 129.217920, &wx, &wy)) {
        addRealisticSillaHall(mesh.get(), dem, wx, wy, 8.5, 8.5, 5.2f, 3.8f, 15.0, royalPillarCol, palaceRoofCol, false, false);
      }
      // 경주향교 대성전 (보물 제1727호 · 35.831253, 129.216928) & 명륜당 (보물 제2148호 · 35.831579, 129.216794)
      if (toWorld(35.831253, 129.216928, &wx, &wy)) {
        addRealisticSillaHall(mesh.get(), dem, wx, wy, 22.0, 14.0, 7.2f, 5.0f, 16.0, royalPillarCol, palaceRoofCol, false, false);
      }
      if (toWorld(35.831579, 129.216794, &wx, &wy)) {
        addRealisticSillaHall(mesh.get(), dem, wx, wy, 23.0, 13.0, 6.8f, 4.6f, 16.0, hanokWoodCol, palaceRoofCol, false, false);
      }
      // 경주 교동 최부자댁 안채·사랑채 (국가민속문화유산 · 35.830880, 129.216210)
      if (toWorld(35.830880, 129.216210, &wx, &wy)) {
        addRealisticSillaHall(mesh.get(), dem, wx, wy, 24.0, 14.0, 5.5f, 4.0f, 15.0, hanokWoodCol, palaceRoofCol, false, false);
      }

      // =========================================================================
      // [유적 8] 국립경주박물관 & 국보 제29호 성덕대왕신종(에밀레종) 종각 (35.829891, 129.227277)
      // =========================================================================
      if (toWorld(35.829891, 129.227277, &wx, &wy)) {
        addRealisticSillaHall(mesh.get(), dem, wx, wy, 15.5, 14.6, 8.2f, 5.8f, 0.0, royalPillarCol, palaceRoofCol, false, false);
      }
      if (toWorld(35.829490, 129.227820, &wx, &wy)) {  // 신라역사관
        addRealisticSillaHall(mesh.get(), dem, wx, wy, 56.0, 42.0, 11.0f, 6.5f, 0.0, stoneBankCol, palaceRoofCol, false, false);
      }
      if (toWorld(35.828720, 129.228560, &wx, &wy)) {  // 신라미술관
        addRealisticSillaHall(mesh.get(), dem, wx, wy, 40.0, 52.0, 10.5f, 6.2f, 0.0, stoneBankCol, palaceRoofCol, false, false);
      }

      // =========================================================================
      // [유적 9] 국보 제37호 황복사지 삼층석탑 (35.832190, 129.242390) & 경주 오릉 숭덕전 & 경주읍성 향일문
      // =========================================================================
      if (toWorld(35.832190, 129.242390, &wx, &wy)) {
        addSillaThreeStoryStonePagoda(mesh.get(), dem, wx, wy);
      }
      if (toWorld(35.822150, 129.209570, &wx, &wy)) {  // 오릉 숭덕전(박혁거세 시조묘)
        addRealisticSillaHall(mesh.get(), dem, wx, wy, 20.0, 12.5, 6.5f, 4.5f, 0.0, royalPillarCol, palaceRoofCol, false, false);
      }
      if (toWorld(35.847195, 129.213564, &wx, &wy)) {  // 경주읍성 동문(향일문) 및 성벽
        addOrientedBox(mesh.get(), dem, wx, wy, 0.0f, 6.2f, 16.0, 190.0, 8.0, stoneBankCol);
        addRealisticSillaHall(mesh.get(), dem, wx, wy, 14.0, 24.0, 9.5f, 6.2f, 8.0, royalPillarCol, palaceRoofCol, true, true);
      }
    } else {
      // 9주 5소경 지방 도독부/소경 치소 중심 관아 정전 (내부 가상 일반 가옥은 그리지 않고 치소 정전만 복원)
      const double azDeg = grid.params.azimuthDeg;
      addRealisticSillaHall(mesh.get(), dem, grid.params.originEasting, grid.params.originNorthing + 12.0,
                            44.0, 22.0, 10.5f, 7.0f, azDeg, royalPillarCol, palaceRoofCol, true, true);
    }
  }

  return mesh;
}

}  // namespace strata3d
