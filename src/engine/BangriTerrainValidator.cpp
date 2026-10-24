#include "BangriTerrainValidator.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QVector3D>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "CrsTransform.h"
#include "EvaluationOverlay.h"
#include "Triangulate.h"

namespace strata3d {

namespace {

using Color = std::array<float, 4>;

struct BlenderSiteBinaryMesh {
  QString id;
  QString name;
  QString category;
  QString subtitle;
  double lat = 0.0;
  double lon = 0.0;
  float labelH = 18.0f;
  float camYaw = 14.0f;
  float camPitch = 32.0f;
  float camDist = 700.0f;
  std::array<uint8_t, 4> badgeRgba = {200, 60, 40, 255};
  uint32_t vCount = 0;
  uint32_t iCount = 0;
  std::vector<float> vertices;   // vCount * 10 (localDxM, localDyM, localZ, nx, ny, nz, r, g, b, a)
  std::vector<uint32_t> indices; // iCount
};

QString resolveBlenderHeritageBundlePath() {
  QStringList candidates;
  if (QCoreApplication::instance()) {
    const QString appDir = QCoreApplication::applicationDirPath();
    candidates << (appDir + QStringLiteral("/data/models/gyeongju_blender_heritage.s3dmesh"))
               << (appDir + QStringLiteral("/../data/models/gyeongju_blender_heritage.s3dmesh"))
               << (appDir + QStringLiteral("/../../data/models/gyeongju_blender_heritage.s3dmesh"));
  }
#ifdef S3D_SOURCE_DIR
  candidates << (QStringLiteral(S3D_SOURCE_DIR) + QStringLiteral("/data/models/gyeongju_blender_heritage.s3dmesh"));
#endif
  candidates << QStringLiteral("data/models/gyeongju_blender_heritage.s3dmesh")
             << QStringLiteral("../data/models/gyeongju_blender_heritage.s3dmesh");

  for (const QString& c : candidates) {
    QFileInfo fi(c);
    if (fi.exists() && fi.isFile() && fi.size() > 64) {
      return fi.absoluteFilePath();
    }
  }
  return QString();
}

const std::vector<BlenderSiteBinaryMesh>& cachedBlenderHeritageMeshes() {
  static std::vector<BlenderSiteBinaryMesh> s_cache;
  static bool s_loaded = false;
  if (s_loaded) return s_cache;
  s_loaded = true;

  const QString path = resolveBlenderHeritageBundlePath();
  if (path.isEmpty()) return s_cache;

  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) return s_cache;
  const QByteArray raw = f.readAll();
  if (raw.size() < 12) return s_cache;

  const char* ptr = raw.constData();
  const char* end = ptr + raw.size();
  if (std::memcmp(ptr, "S3DMESH1", 8) != 0) return s_cache;
  ptr += 8;

  auto readU32 = [&](uint32_t* out) {
    if (end - ptr < 4) return false;
    std::memcpy(out, ptr, 4);
    ptr += 4;
    return true;
  };
  auto readF32 = [&](float* out) {
    if (end - ptr < 4) return false;
    std::memcpy(out, ptr, 4);
    ptr += 4;
    return true;
  };
  auto readF64 = [&](double* out) {
    if (end - ptr < 8) return false;
    std::memcpy(out, ptr, 8);
    ptr += 8;
    return true;
  };
  auto readStr = [&](QString* out) {
    uint32_t len = 0;
    if (!readU32(&len) || uint32_t(end - ptr) < len) return false;
    *out = QString::fromUtf8(ptr, int(len));
    ptr += len;
    return true;
  };

  uint32_t numSites = 0;
  if (!readU32(&numSites) || numSites == 0 || numSites > 4096) return s_cache;

  s_cache.reserve(numSites);
  for (uint32_t i = 0; i < numSites; ++i) {
    BlenderSiteBinaryMesh sm;
    if (!readStr(&sm.id) || !readStr(&sm.name) || !readStr(&sm.category) || !readStr(&sm.subtitle) ||
        !readF64(&sm.lat) || !readF64(&sm.lon) ||
        !readF32(&sm.labelH) || !readF32(&sm.camYaw) || !readF32(&sm.camPitch) || !readF32(&sm.camDist)) {
      break;
    }
    if (end - ptr < 4) break;
    std::memcpy(sm.badgeRgba.data(), ptr, 4);
    ptr += 4;

    uint32_t floatCount = 0;
    if (!readU32(&floatCount) || !readU32(&sm.iCount) || floatCount % 10 != 0) {
      break;
    }
    sm.vCount = floatCount / 10;

    const size_t vBytes = size_t(floatCount) * sizeof(float);
    const size_t iBytes = size_t(sm.iCount) * sizeof(uint32_t);
    if (size_t(end - ptr) < vBytes + iBytes) break;

    sm.vertices.resize(size_t(floatCount));
    std::memcpy(sm.vertices.data(), ptr, vBytes);
    ptr += vBytes;

    sm.indices.resize(size_t(sm.iCount));
    std::memcpy(sm.indices.data(), ptr, iBytes);
    ptr += iBytes;

    s_cache.push_back(std::move(sm));
  }
  return s_cache;
}

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
[[maybe_unused]] void addHwangnyongsaNineStoryPagoda(SceneMesh* m,
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
[[maybe_unused]] void addRealisticCheomseongdae(SceneMesh* m, const DemGrid& dem, double wx, double wy) {
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
[[maybe_unused]] void addBunhwangsaBrickPagoda(SceneMesh* m, const DemGrid& dem, double wx, double wy) {
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
[[maybe_unused]] void addSillaThreeStoryStonePagoda(SceneMesh* m, const DemGrid& dem, double wx, double wy) {
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
[[maybe_unused]] void addRealisticWoljeonggyo(SceneMesh* m, const DemGrid& dem, double wx, double wy, double azDeg) {
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
[[maybe_unused]] void addCorridorOrRampartSegment(SceneMesh* m,
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

bool BangriTerrainValidator::hasBlenderHeritageBundle(QString* resolvedPath) {
  const QString p = resolveBlenderHeritageBundlePath();
  if (resolvedPath) *resolvedPath = p;
  return !p.isEmpty() && !cachedBlenderHeritageMeshes().empty();
}

std::vector<GyeongjuHeritageSiteMeta> BangriTerrainValidator::gyeongjuHeritageCatalog() {
  struct StaticSiteSpec {
    const char* id;
    const char16_t* name;
    const char16_t* category;
    const char16_t* subtitle;
    double lat;
    double lon;
    float focusYaw;
    float focusPitch;
    float focusDist;
    float labelH;
    std::array<float, 4> badgeCol;
    bool imp;
  };
  static constexpr StaticSiteSpec kSpecs[] = {
      {"hwangnyongsa", u"황룡사 9층목탑 · 대가람 (사적 제6호)", u"사적 제6호",
       u"80m 9층목탑·중금당·동서금당·강당·종루·경루·중문·남문·회랑",
       35.837180, 129.233130, 14.0f, 32.0f, 760.0f, 82.0f, {0.80f, 0.20f, 0.14f, 1.0f}, true},
      {"bunhwangsa", u"분황사 모전석탑 · 보광전 (국보 제30호)", u"국보 제30호",
       u"신라 최고(最古) 안산암 벽돌 모전석탑·돌사자·보광전·화쟁국사비",
       35.840566, 129.233618, 12.0f, 30.0f, 420.0f, 15.0f, {0.43f, 0.45f, 0.49f, 1.0f}, true},
      {"guhwangdong_wonji", u"경주 구황동 원지 유적 (사적 제548호)", u"사적 제548호",
       u"신라 왕경 조경 연못·호안 석축과 대섬·소섬 정자",
       35.840550, 129.235180, 15.0f, 32.0f, 380.0f, 9.0f, {0.10f, 0.51f, 0.69f, 1.0f}, false},
      {"donggung_wolji", u"동궁과 월지 · 안압지 (사적 제18호)", u"사적 제18호",
       u"삼신산 3개 섬(대도·중도·소도) 연못과 서안 임해전 제1·2·3 누각",
       35.834788, 129.226537, 12.0f, 32.0f, 620.0f, 18.0f, {0.10f, 0.45f, 0.82f, 1.0f}, true},
      {"wolseong", u"경주 월성 · 반월성 왕궁 (사적 제16호)", u"사적 제16호",
       u"초승달 모양 왕궁 토석 성곽·북편 해자 4기·왕궁 정전군",
       35.831830, 129.222080, 12.0f, 34.0f, 920.0f, 22.0f, {0.69f, 0.18f, 0.16f, 1.0f}, true},
      {"seokbinggo", u"경주 석빙고 (보물 제66호)", u"보물 제66호",
       u"월성 북측 성루 위 화강암 홍예 아치 얼음창고",
       35.833140, 129.223950, 10.0f, 28.0f, 300.0f, 10.0f, {0.47f, 0.51f, 0.55f, 1.0f}, false},
      {"cheomseongdae", u"경주 첨성대 (국보 제31호)", u"국보 제31호",
       u"27단 유선형 원통 석축과 정(井)자 정상석을 갖춘 천문대",
       35.834702, 129.218984, 10.0f, 26.0f, 320.0f, 16.0f, {0.61f, 0.43f, 0.18f, 1.0f}, true},
      {"woljeonggyo", u"경주 월정교 · 춘양교지 (사적 제457호)", u"사적 제457호",
       u"남천 위 주형 석조 교각 4기와 단청 회랑·남북 중층 문루 2동",
       35.829250, 129.218140, 14.0f, 28.0f, 450.0f, 18.0f, {0.67f, 0.25f, 0.14f, 1.0f}, true},
      {"gyerim_naemul", u"경주 계림 & 내물왕릉 (사적 제19·188호)", u"사적 제19·188호",
       u"김알지 탄강 신화 숲·비각과 내물마립간 왕릉",
       35.832566, 129.217202, 12.0f, 30.0f, 420.0f, 12.0f, {0.18f, 0.57f, 0.25f, 1.0f}, false},
      {"gyeongju_hyanggyo", u"경주향교 (대성전·명륜당) & 최부자댁", u"보물·민속유산",
       u"신라 국학(國學) 터의 경주향교 대성전·명륜당 및 교동 최부자댁",
       35.831253, 129.216928, 14.0f, 30.0f, 420.0f, 12.0f, {0.55f, 0.33f, 0.16f, 1.0f}, false},
      {"national_museum_emille", u"성덕대왕신종 (국보 제29호) & 국립경주박물관", u"국보 제29호",
       u"에밀레종 종각 및 국립경주박물관 신라역사관·신라미술관",
       35.829891, 129.227277, 15.0f, 32.0f, 580.0f, 16.0f, {0.65f, 0.45f, 0.12f, 1.0f}, false},
      {"daereungwon_hwangnam", u"대릉원 황남대총 (높이 23m 남북 쌍분 · 사적 제512호)", u"사적 제512호",
       u"신라 최대 크기 표형분(남북 쌍분, 120m×80m) 왕릉",
       35.838967, 129.211929, 20.0f, 30.0f, 580.0f, 26.0f, {0.18f, 0.59f, 0.24f, 1.0f}, true},
      {"cheonmachong_michu", u"천마총 · 미추왕릉 · 대릉원 고분군", u"사적 제512호",
       u"금관·천마도가 출토된 천마총과 김씨 시조왕 미추왕릉·숭혜전",
       35.838574, 129.210495, 18.0f, 30.0f, 650.0f, 16.0f, {0.18f, 0.59f, 0.24f, 1.0f}, true},
      {"bonghwangdae_geumgwanchong", u"봉황대 (높이 22m) · 금관총 · 서봉총", u"사적 제512호",
       u"단일 최대 고분 봉황대와 신라 금관 최초 출토 금관총·서봉총·식리총",
       35.841289, 129.210629, 18.0f, 30.0f, 560.0f, 25.0f, {0.18f, 0.59f, 0.24f, 1.0f}, true},
      {"jjoksaem", u"쪽샘지구 신라 고분군 (44호·마갑총 · 발굴관)", u"사적 제512호",
       u"황오동·인왕동 쪽샘 적석목곽분 유적 및 44호분 발굴전시관",
       35.840346, 129.215712, 16.0f, 32.0f, 620.0f, 15.0f, {0.22f, 0.62f, 0.28f, 1.0f}, false},
      {"oreung", u"경주 오릉 (五陵) & 숭덕전 · 알영정 (사적 제172호)", u"사적 제172호",
       u"박혁거세 거서간과 초기 4왕·왕비의 5기 능 및 숭덕전·알영정",
       35.822158, 129.208669, 15.0f, 32.0f, 620.0f, 15.0f, {0.18f, 0.59f, 0.24f, 1.0f}, true},
      {"poseokjeong_najeong", u"경주 나정 · 양산재 & 포석정지 (사적 제1·245호)", u"사적 제1·245호",
       u"박혁거세 탄강지 나정·6부 촌장 사당 양산재와 전복 모양 포석정 수로",
       35.815730, 129.212720, 16.0f, 34.0f, 780.0f, 14.0f, {0.55f, 0.42f, 0.22f, 1.0f}, false},
      {"hwangboksa_nangsan", u"황복사지 3층석탑 · 선덕여왕릉 · 사천왕사지", u"국보 제37호·사적",
       u"낭산 왕실 성지의 황복사지 3층석탑·선덕여왕릉·사천왕사지 쌍탑",
       35.832190, 129.242390, 14.0f, 32.0f, 820.0f, 15.0f, {0.51f, 0.49f, 0.45f, 1.0f}, false},
      {"muyeol_kimyushin", u"태종무열왕릉 (국보 무열왕릉비) & 김유신장군묘", u"사적 제20·21호",
       u"삼국통일 주역 태종무열왕릉(귀부·이수)과 송화산 김유신장군묘",
       35.825270, 129.187390, 18.0f, 32.0f, 820.0f, 16.0f, {0.20f, 0.55f, 0.30f, 1.0f}, false},
      {"gyeongju_eupseong", u"경주읍성 (향일문·성곽) & 성동동 전랑지 · 재매정", u"사적 제96·88호",
       u"경주읍성 성벽·향일문 문루와 성동동 전랑지 관아·김유신 본가 재매정",
       35.847195, 129.213564, 14.0f, 30.0f, 620.0f, 16.0f, {0.58f, 0.38f, 0.22f, 1.0f}, false},
  };

  const auto& blenderSites = cachedBlenderHeritageMeshes();
  std::vector<GyeongjuHeritageSiteMeta> out;
  out.reserve(std::max<size_t>(std::size(kSpecs), blenderSites.size()));
  for (const StaticSiteSpec& s : kSpecs) {
    GyeongjuHeritageSiteMeta m;
    m.id = QString::fromLatin1(s.id);
    m.name = QString::fromUtf16(s.name);
    m.category = QString::fromUtf16(s.category);
    m.subtitle = QString::fromUtf16(s.subtitle);
    m.lat = s.lat;
    m.lon = s.lon;
    m.focusYawDeg = s.focusYaw;
    m.focusPitchDeg = s.focusPitch;
    m.focusDistM = s.focusDist;
    m.labelHeightM = s.labelH;
    m.badgeColor = s.badgeCol;
    m.importantLabel = s.imp;
    for (const BlenderSiteBinaryMesh& bm : blenderSites) {
      if (bm.id == m.id) {
        m.blenderVertexCount = bm.vCount;
        m.blenderTriangleCount = bm.iCount / 3;
        break;
      }
    }
    out.push_back(std::move(m));
  }

  // 블렌더 번들(gyeongju_blender_heritage.s3dmesh)에 포함된 경주시 전체 국가유산청(KHS) 지정·등록 유산 전수 추가
  for (const BlenderSiteBinaryMesh& bm : blenderSites) {
    bool already = false;
    for (const auto& ex : out) {
      if (ex.id == bm.id) {
        already = true;
        break;
      }
    }
    if (already) continue;
    GyeongjuHeritageSiteMeta m;
    m.id = bm.id;
    m.name = bm.name;
    m.category = bm.category;
    m.subtitle = bm.subtitle;
    m.lat = bm.lat;
    m.lon = bm.lon;
    m.focusYawDeg = bm.camYaw;
    m.focusPitchDeg = bm.camPitch;
    m.focusDistM = bm.camDist;
    m.labelHeightM = bm.labelH;
    m.badgeColor = {bm.badgeRgba[0] / 255.0f,
                    bm.badgeRgba[1] / 255.0f,
                    bm.badgeRgba[2] / 255.0f,
                    bm.badgeRgba[3] / 255.0f};
    m.importantLabel = false;
    m.blenderVertexCount = bm.vCount;
    m.blenderTriangleCount = bm.iCount / 3;
    out.push_back(std::move(m));
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
    bool includeRoadGrid,
    const std::vector<QString>* enabledHeritageIds) {
  auto mesh = std::make_shared<SceneMesh>();
  if (!dem.valid()) return mesh;

  auto isSiteEnabled = [&](const QString& siteId) {
    if (!enabledHeritageIds) return true;
    return std::find(enabledHeritageIds->begin(), enabledHeritageIds->end(), siteId) != enabledHeritageIds->end();
  };

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

  // 2. 블렌더(Blender 4.2.3)로 생성된 경주 20대 핵심 유적 3D 메쉬 로드 및 실제 지형(DEM) 위 1:1 안착
  if (include3dCityArchitecture) {
    const Color royalPillarCol{0.78f, 0.21f, 0.15f, 1.0f};
    const Color palaceRoofCol{0.19f, 0.22f, 0.27f, 1.0f};
    const Color waterPondCol{0.09f, 0.42f, 0.52f, 1.0f};
    const Color stoneBankCol{0.75f, 0.73f, 0.68f, 1.0f};

    const CrsTransformer tr(QStringLiteral("EPSG:4326"), dem.crs);

    if (grid.params.presetId.startsWith(QLatin1String("gyeongju"))) {
      const auto& blenderSites = cachedBlenderHeritageMeshes();
      const double cx = dem.centerX(), cy = dem.centerY();

      if (!blenderSites.empty()) {
        for (const BlenderSiteBinaryMesh& bm : blenderSites) {
          if (!isSiteEnabled(bm.id)) continue;
          if (bm.vCount == 0 || bm.iCount == 0) continue;

          // 유적 기준점(WGS84 -> dem.crs) 변환 및 지형 표고 샘플링 (외곽 경주 유적도 경계 표고로 모두 3D 배치)
          double anchorX = bm.lon, anchorY = bm.lat;
          if (!tr.valid() || !tr.transform(&anchorX, &anchorY)) {
            continue;
          }
          float anchorGz = 0.5f * (dem.zMin + dem.zMax);
          if (!dem.sample(anchorX, anchorY, &anchorGz)) {
            const double clampedX = std::clamp(anchorX, dem.originX, dem.originX + (dem.width - 1) * dem.pixelSize);
            const double clampedY = std::clamp(anchorY, dem.originY - (dem.height - 1) * dem.pixelSize, dem.originY);
            dem.sample(clampedX, clampedY, &anchorGz);
          }

          const uint32_t baseVertIdx = uint32_t(mesh->vertices.size() / SceneMesh::kFloatsPerVertex);
          mesh->vertices.reserve(mesh->vertices.size() + bm.vertices.size());

          for (uint32_t iv = 0; iv < bm.vCount; ++iv) {
            const float* v = &bm.vertices[size_t(iv) * 10];
            const double wx = anchorX + double(v[0]);
            const double wy = anchorY + double(v[1]);
            float gz = anchorGz;
            dem.sample(wx, wy, &gz);
            const float localZ = v[2];
            mesh->vertices.insert(mesh->vertices.end(),
                                  {float(wx - cx), float(wy - cy), gz + localZ,
                                   v[3], v[4], v[5],
                                   v[6], v[7], v[8], v[9]});
          }

          mesh->indices.reserve(mesh->indices.size() + bm.indices.size());
          for (uint32_t idx : bm.indices) {
            mesh->indices.push_back(baseVertIdx + idx);
          }
          ++mesh->heritagePoints;
        }
      }

      // 수면 다각형(구황동 원지·동궁과 월지·월성 북편 해자)은 삼각분할 수면도 함께 입혀 선명도를 높인다
      if (isSiteEnabled(QStringLiteral("guhwangdong_wonji"))) {
        addPolygonWaterPond(mesh.get(), dem,
                            {{35.84032, 129.23492}, {35.84030, 129.23542}, {35.84078, 129.23545},
                             {35.84085, 129.23512}, {35.84062, 129.23488}},
                            tr, 0.52f, waterPondCol, stoneBankCol);
      }
      if (isSiteEnabled(QStringLiteral("donggung_wolji"))) {
        addPolygonWaterPond(mesh.get(), dem,
                            {{35.83404, 129.22682}, {35.83403, 129.22752}, {35.83442, 129.22778},
                             {35.83502, 129.22775}, {35.83552, 129.22755}, {35.83568, 129.22685},
                             {35.83562, 129.22605}, {35.83505, 129.22603}, {35.83482, 129.22606},
                             {35.83480, 129.22658}, {35.83422, 129.22676}},
                            tr, 0.52f, waterPondCol, stoneBankCol);
      }
      if (isSiteEnabled(QStringLiteral("wolseong"))) {
        addPolygonWaterPond(mesh.get(), dem,
                            {{35.83235, 129.22065}, {35.83275, 129.22155}, {35.83302, 129.22138}, {35.83260, 129.22048}},
                            tr, 0.45f, waterPondCol, stoneBankCol);
        addPolygonWaterPond(mesh.get(), dem,
                            {{35.83282, 129.22170}, {35.83318, 129.22268}, {35.83348, 129.22252}, {35.83312, 129.22155}},
                            tr, 0.45f, waterPondCol, stoneBankCol);
        addPolygonWaterPond(mesh.get(), dem,
                            {{35.83322, 129.22282}, {35.83342, 129.22382}, {35.83372, 129.22375}, {35.83352, 129.22274}},
                            tr, 0.45f, waterPondCol, stoneBankCol);
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

UserDrawnGrid3D BangriTerrainValidator::makeGridFromTwoPoints(
    const Pt& a,
    const Pt& b,
    double cellWidthM,
    double cellHeightM,
    int minCols,
    int minRows,
    double defaultAzimuthDeg,
    bool alignToClickLine,
    float lineWidthM,
    int subDivisions,
    const std::array<float, 4>& color,
    bool isPreview) {
  UserDrawnGrid3D g;
  g.cellWidthM = std::clamp(cellWidthM, 5.0, 2000.0);
  g.cellHeightM = std::clamp(cellHeightM, 5.0, 2000.0);
  g.lineWidthM = std::clamp(lineWidthM, 0.5f, 40.0f);
  g.subDivisions = std::clamp(subDivisions, 1, 3);
  g.color = color;
  g.isPreview = isPreview;

  const double dx = b.x - a.x;
  const double dy = b.y - a.y;
  const double dist = std::hypot(dx, dy);

  if (dist < 4.0) {
    g.origin = a;
    g.centerAligned = true;
    g.cols = std::clamp(minCols, 1, 50);
    g.rows = std::clamp(minRows, 1, 50);
    g.azimuthDeg = defaultAzimuthDeg;
    return g;
  }

  if (alignToClickLine) {
    // 두 점 A -> B 방향을 격자 주축으로 자동 정렬하고, A에서 B까지 격자를 생성
    const double azDeg = std::atan2(dx, dy) * 180.0 / M_PI;
    g.azimuthDeg = azDeg;
    g.origin = {0.5 * (a.x + b.x), 0.5 * (a.y + b.y)};
    g.centerAligned = true;
    g.rows = std::clamp(int(std::round(dist / g.cellHeightM)), 1, 50);
    g.cols = std::clamp(minCols, 1, 50);
  } else {
    // 지정된 방위각(defaultAzimuthDeg) 기준으로 A 와 B 가 이루는 직사각형 영역을 채우는 격자 생성
    g.azimuthDeg = defaultAzimuthDeg;
    const double rad = defaultAzimuthDeg * M_PI / 180.0;
    const double cosA = std::cos(rad), sinA = std::sin(rad);
    const double spanU = std::abs(dx * cosA - dy * sinA);
    const double spanV = std::abs(dx * sinA + dy * cosA);
    g.origin = {0.5 * (a.x + b.x), 0.5 * (a.y + b.y)};
    g.centerAligned = true;
    g.cols = (spanU >= g.cellWidthM * 0.75)
                 ? std::clamp(int(std::round(spanU / g.cellWidthM)), 1, 50)
                 : std::clamp(minCols, 1, 50);
    g.rows = (spanV >= g.cellHeightM * 0.75)
                 ? std::clamp(int(std::round(spanV / g.cellHeightM)), 1, 50)
                 : std::clamp(minRows, 1, 50);
  }
  return g;
}

std::shared_ptr<SceneMesh> BangriTerrainValidator::buildUserDrawingsOverlay(
    const DemGrid& dem,
    const std::vector<SillaMound>& mounds,
    const std::vector<UserDrawnLine3D>& lines,
    const std::vector<UserDrawnGrid3D>& grids) {
  auto mesh = std::make_shared<SceneMesh>();
  if (!dem.valid()) return mesh;

  const double cx = dem.centerX(), cy = dem.centerY();

  // 지형 표고 + 3D 고분 봉분 높이를 함께 샘플링하여 선과 격자가 지형·왕릉 위를 입체적으로 타고 넘어가게 한다
  auto sampleSurfaceZ = [&](double wx, double wy, float* outZ) {
    float gz = 0.0f;
    if (!dem.sample(wx, wy, &gz)) return false;
    const float mh = SillaMoundReconstructor::sampleMoundHeight(wx, wy, mounds);
    *outZ = gz + mh;
    return true;
  };

  // 두 점 p0 -> p1 사이를 지형 굴곡을 따라 입체 사각 리본(상면 + 좌우 측면)으로 그리는 헬퍼
  auto addVolumetricTerrainLine = [&](const Pt& p0,
                                      const Pt& p1,
                                      float widthM,
                                      float baseLiftM,
                                      float ribbonHeightM,
                                      const Color& col) {
    const double totalLen = std::hypot(p1.x - p0.x, p1.y - p0.y);
    if (totalLen < 0.5) return;
    const double ux = (p1.x - p0.x) / totalLen;
    const double uy = (p1.y - p0.y) / totalLen;
    const double nx = -uy * (0.5 * double(widthM));
    const double ny =  ux * (0.5 * double(widthM));

    const int steps = std::clamp(int(std::ceil(totalLen / 8.0)), 1, 600);
    const Color sideCol{col[0] * 0.80f, col[1] * 0.80f, col[2] * 0.80f, col[3]};

    for (int i = 0; i < steps; ++i) {
      const double t0 = double(i) / double(steps);
      const double t1 = double(i + 1) / double(steps);
      const double x0 = p0.x + (p1.x - p0.x) * t0;
      const double y0 = p0.y + (p1.y - p0.y) * t0;
      const double x1 = p0.x + (p1.x - p0.x) * t1;
      const double y1 = p0.y + (p1.y - p0.y) * t1;

      float z0 = 0.0f, z1 = 0.0f;
      if (!sampleSurfaceZ(x0, y0, &z0) || !sampleSurfaceZ(x1, y1, &z1)) continue;

      const QVector3D tL0(float(x0 + nx - cx), float(y0 + ny - cy), z0 + baseLiftM + ribbonHeightM);
      const QVector3D tR0(float(x0 - nx - cx), float(y0 - ny - cy), z0 + baseLiftM + ribbonHeightM);
      const QVector3D tR1(float(x1 - nx - cx), float(y1 - ny - cy), z1 + baseLiftM + ribbonHeightM);
      const QVector3D tL1(float(x1 + nx - cx), float(y1 + ny - cy), z1 + baseLiftM + ribbonHeightM);

      const QVector3D bL0(float(x0 + nx - cx), float(y0 + ny - cy), z0 + baseLiftM);
      const QVector3D bR0(float(x0 - nx - cx), float(y0 - ny - cy), z0 + baseLiftM);
      const QVector3D bR1(float(x1 - nx - cx), float(y1 - ny - cy), z1 + baseLiftM);
      const QVector3D bL1(float(x1 + nx - cx), float(y1 + ny - cy), z1 + baseLiftM);

      addQuad(mesh.get(), tL0, tR0, tR1, tL1, col);
      addQuad(mesh.get(), bL0, bL1, tL1, tL0, sideCol);
      addQuad(mesh.get(), bR1, bR0, tR0, tR1, sideCol);
    }
  };

  // 1. 사용자가 맵에 그은 3D 선(UserDrawnLine3D) 렌더링
  for (const UserDrawnLine3D& ln : lines) {
    const float w = std::clamp(ln.widthM, 0.5f, 40.0f);
    const float h = ln.isPreview ? 2.2f : 2.6f;
    addVolumetricTerrainLine(ln.a, ln.b, w, 0.35f, h, ln.color);

    // 양 끝점 입체 기둥 마커
    appendPole(mesh.get(), dem, ln.a, std::max(2.2f, w * 0.75f), 22.0f, ln.color);
    appendPole(mesh.get(), dem, ln.b, std::max(2.2f, w * 0.75f), 22.0f, ln.color);

    // 100m 간격 눈금 표시
    const double len = std::hypot(ln.b.x - ln.a.x, ln.b.y - ln.a.y);
    if (len >= 100.0) {
      const double ux = (ln.b.x - ln.a.x) / len;
      const double uy = (ln.b.y - ln.a.y) / len;
      const double px = -uy, py = ux;
      const double tickHalf = std::max(6.0, double(w) * 1.8);
      for (double d = 100.0; d < len - 15.0; d += 100.0) {
        const double mx = ln.a.x + ux * d;
        const double my = ln.a.y + uy * d;
        addVolumetricTerrainLine({mx - px * tickHalf, my - py * tickHalf},
                                 {mx + px * tickHalf, my + py * tickHalf},
                                 std::max(1.2f, w * 0.55f), 0.45f, h + 0.5f, ln.color);
      }
    }
  }

  // 2. 사용자가 맵에 생성한 3D 격자(UserDrawnGrid3D) 렌더링
  for (const UserDrawnGrid3D& gr : grids) {
    const int cols = std::clamp(gr.cols, 1, 60);
    const int rows = std::clamp(gr.rows, 1, 60);
    const double cw = std::clamp(gr.cellWidthM, 5.0, 2000.0);
    const double ch = std::clamp(gr.cellHeightM, 5.0, 2000.0);
    const double rad = gr.azimuthDeg * M_PI / 180.0;
    const double cosA = std::cos(rad), sinA = std::sin(rad);

    const double uMin = gr.centerAligned ? (-0.5 * cols * cw) : 0.0;
    const double uMax = uMin + cols * cw;
    const double vMin = gr.centerAligned ? (-0.5 * rows * ch) : 0.0;
    const double vMax = vMin + rows * ch;

    auto toWorldPt = [&](double u, double v) -> Pt {
      return {gr.origin.x + u * cosA + v * sinA,
              gr.origin.y - u * sinA + v * cosA};
    };

    const float mainW = std::clamp(gr.lineWidthM, 0.5f, 35.0f);
    const float borderW = mainW * 1.35f;
    const float mainH = gr.isPreview ? 2.0f : 2.4f;

    // 세로(남북) 격자선 (cols + 1 개)
    for (int ic = 0; ic <= cols; ++ic) {
      const double u = uMin + ic * cw;
      const bool isOuter = (ic == 0 || ic == cols);
      addVolumetricTerrainLine(toWorldPt(u, vMin), toWorldPt(u, vMax),
                               isOuter ? borderW : mainW, 0.35f,
                               isOuter ? (mainH + 0.6f) : mainH, gr.color);
    }

    // 가로(동서) 격자선 (rows + 1 개)
    for (int ir = 0; ir <= rows; ++ir) {
      const double v = vMin + ir * ch;
      const bool isOuter = (ir == 0 || ir == rows);
      addVolumetricTerrainLine(toWorldPt(uMin, v), toWorldPt(uMax, v),
                               isOuter ? borderW : mainW, 0.35f,
                               isOuter ? (mainH + 0.6f) : mainH, gr.color);
    }

    // 보조 분할 격자선 (2분할 十자 또는 3분할 井자)
    if (gr.subDivisions >= 2) {
      const Color subCol{gr.color[0] * 0.88f, gr.color[1] * 0.88f, gr.color[2] * 0.88f, gr.color[3]};
      const float subW = std::max(0.8f, mainW * 0.48f);
      for (int ic = 0; ic < cols; ++ic) {
        for (int s = 1; s < gr.subDivisions; ++s) {
          const double u = uMin + (ic + double(s) / gr.subDivisions) * cw;
          addVolumetricTerrainLine(toWorldPt(u, vMin), toWorldPt(u, vMax), subW, 0.25f, mainH * 0.68f, subCol);
        }
      }
      for (int ir = 0; ir < rows; ++ir) {
        for (int s = 1; s < gr.subDivisions; ++s) {
          const double v = vMin + (ir + double(s) / gr.subDivisions) * ch;
          addVolumetricTerrainLine(toWorldPt(uMin, v), toWorldPt(uMax, v), subW, 0.25f, mainH * 0.68f, subCol);
        }
      }
    }

    // 네 모서리 및 중심점 3D 기둥 마커
    const float poleW = std::max(2.4f, borderW * 0.8f);
    appendPole(mesh.get(), dem, toWorldPt(uMin, vMin), poleW, 20.0f, gr.color);
    appendPole(mesh.get(), dem, toWorldPt(uMax, vMin), poleW, 20.0f, gr.color);
    appendPole(mesh.get(), dem, toWorldPt(uMax, vMax), poleW, 20.0f, gr.color);
    appendPole(mesh.get(), dem, toWorldPt(uMin, vMax), poleW, 20.0f, gr.color);
    appendPole(mesh.get(), dem, toWorldPt(0.5 * (uMin + uMax), 0.5 * (vMin + vMax)),
               poleW * 1.15f, 26.0f, {1.0f, 0.30f, 0.25f, 1.0f});
  }

  return mesh;
}

}  // namespace strata3d
