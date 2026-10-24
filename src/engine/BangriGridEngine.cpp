#include "BangriGridEngine.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "CrsTransform.h"

namespace strata3d {

double BangriGridEngine::scaleUnitMeters(BangriScaleUnit unit) {
  switch (unit) {
    case BangriScaleUnit::GoguryeoChi:
      return 0.356;  // 고구려척 1척 ≈ 35.5~35.6 cm
    case BangriScaleUnit::TangLargeChi:
      return 0.296;  // 당대척 1척 ≈ 29.6 cm (1보 = 5척 = 1.48 m)
    case BangriScaleUnit::Meter:
    default:
      return 1.0;
  }
}

double BangriGridEngine::metersToScaleUnit(double meters, BangriScaleUnit unit) {
  const double u = scaleUnitMeters(unit);
  return u > 0.0 ? (meters / u) : meters;
}

double BangriGridEngine::scaleUnitToMeters(double scaleValue, BangriScaleUnit unit) {
  return scaleValue * scaleUnitMeters(unit);
}

QString BangriGridEngine::scaleUnitName(BangriScaleUnit unit) {
  switch (unit) {
    case BangriScaleUnit::GoguryeoChi:
      return QStringLiteral("고구려척 (1척 = 35.6 cm)");
    case BangriScaleUnit::TangLargeChi:
      return QStringLiteral("당척 대척 (1척 = 29.6 cm, 1보 = 5척)");
    case BangriScaleUnit::Meter:
    default:
      return QStringLiteral("미터 (1.0 m)");
  }
}

QString BangriGridEngine::subdivisionModeName(BangriSubdivisionMode mode) {
  switch (mode) {
    case BangriSubdivisionMode::None:
      return QStringLiteral("내부 분할 없음 (단일 방)");
    case BangriSubdivisionMode::Cross4:
      return QStringLiteral("十자형 4분할 (방내 소로 1×1)");
    case BangriSubdivisionMode::Well9:
    default:
      return QStringLiteral("井자형 9분할 (방내 소로 2×2 표준)");
  }
}

Pt BangriGridEngine::gridLocalToWorld(const BangriParams& params, double u, double v) {
  const double rad = params.azimuthDeg * M_PI / 180.0;
  const double c = std::cos(rad);
  const double s = std::sin(rad);
  // u: 격자 동쪽(진동에서 시계방향 azimuthDeg 회전), v: 격자 북쪽(진북에서 시계방향 azimuthDeg 회전)
  return {
      params.originEasting + u * c + v * s,
      params.originNorthing - u * s + v * c};
}

Pt BangriGridEngine::worldToGridLocal(const BangriParams& params, double x, double y) {
  const double rad = params.azimuthDeg * M_PI / 180.0;
  const double c = std::cos(rad);
  const double s = std::sin(rad);
  const double dx = x - params.originEasting;
  const double dy = y - params.originNorthing;
  return {
      dx * c - dy * s,
      dx * s + dy * c};
}

namespace {

BangriRoadSegment makeRoadSegment(const BangriParams& params,
                                  const QString& id,
                                  BangriRoadTier tier,
                                  bool isNorthSouth,
                                  int gridIdx,
                                  double u0,
                                  double v0,
                                  double u1,
                                  double v1,
                                  double roadWidthM) {
  BangriRoadSegment seg;
  seg.id = id;
  seg.tier = tier;
  seg.isNorthSouth = isNorthSouth;
  seg.gridIndex = gridIdx;
  seg.a = BangriGridEngine::gridLocalToWorld(params, u0, v0);
  seg.b = BangriGridEngine::gridLocalToWorld(params, u1, v1);
  seg.widthM = roadWidthM;
  seg.ditchWidthM = params.ditchWidthM;
  seg.ditchDepthM = params.ditchDepthM;

  const double ditchOffset = roadWidthM * 0.5 + params.ditchWidthM * 0.5;
  const double wallOffset = roadWidthM * 0.5 + params.ditchWidthM + params.wallBaseWidthM * 0.5;

  if (isNorthSouth) {
    seg.leftDitchA = BangriGridEngine::gridLocalToWorld(params, u0 - ditchOffset, v0);
    seg.leftDitchB = BangriGridEngine::gridLocalToWorld(params, u1 - ditchOffset, v1);
    seg.rightDitchA = BangriGridEngine::gridLocalToWorld(params, u0 + ditchOffset, v0);
    seg.rightDitchB = BangriGridEngine::gridLocalToWorld(params, u1 + ditchOffset, v1);

    seg.leftWallA = BangriGridEngine::gridLocalToWorld(params, u0 - wallOffset, v0);
    seg.leftWallB = BangriGridEngine::gridLocalToWorld(params, u1 - wallOffset, v1);
    seg.rightWallA = BangriGridEngine::gridLocalToWorld(params, u0 + wallOffset, v0);
    seg.rightWallB = BangriGridEngine::gridLocalToWorld(params, u1 + wallOffset, v1);
  } else {
    seg.leftDitchA = BangriGridEngine::gridLocalToWorld(params, u0, v0 + ditchOffset);
    seg.leftDitchB = BangriGridEngine::gridLocalToWorld(params, u1, v1 + ditchOffset);
    seg.rightDitchA = BangriGridEngine::gridLocalToWorld(params, u0, v0 - ditchOffset);
    seg.rightDitchB = BangriGridEngine::gridLocalToWorld(params, u1, v1 - ditchOffset);

    seg.leftWallA = BangriGridEngine::gridLocalToWorld(params, u0, v0 + wallOffset);
    seg.leftWallB = BangriGridEngine::gridLocalToWorld(params, u1, v1 + wallOffset);
    seg.rightWallA = BangriGridEngine::gridLocalToWorld(params, u0, v0 - wallOffset);
    seg.rightWallB = BangriGridEngine::gridLocalToWorld(params, u1, v1 - wallOffset);
  }
  return seg;
}

}  // namespace

BangriGridResult BangriGridEngine::generateGrid(const BangriParams& params) {
  BangriGridResult out;
  out.params = params;

  const int cols = std::clamp(params.extentCols, 1, 32);
  const int rows = std::clamp(params.extentRows, 1, 32);
  const double W = std::max(20.0, params.blockWidthM);
  const double H = std::max(20.0, params.blockHeightM);

  const int cMin = -cols / 2;
  const int cMax = cMin + cols;
  const int rMin = -rows / 2;
  const int rMax = rMin + rows;

  const double uMin = cMin * W;
  const double uMax = cMax * W;
  const double vMin = rMin * H;
  const double vMax = rMax * H;

  // 1. 남북 방향 간선 대로(c == 0) 및 방간 중로(c != 0)
  for (int c = cMin; c <= cMax; ++c) {
    const double u = c * W;
    const bool isMain = (c == 0);
    const BangriRoadTier tier = isMain ? BangriRoadTier::MainAvenue : BangriRoadTier::InterBlockRoad;
    const double rw = isMain ? params.mainRoadWidthM : params.interBlockRoadWidthM;
    BangriRoadSegment seg = makeRoadSegment(
        params,
        QStringLiteral("NS_%1").arg(c),
        tier,
        true,
        c,
        u, vMin,
        u, vMax,
        rw);
    const double len = std::hypot(seg.b.x - seg.a.x, seg.b.y - seg.a.y);
    if (isMain) out.totalMainAvenueLengthM += len;
    else out.totalInterBlockRoadLengthM += len;
    out.roads.push_back(std::move(seg));
  }

  // 2. 동서 방향 간선 대로(r == 0) 및 방간 중로(r != 0)
  for (int r = rMin; r <= rMax; ++r) {
    const double v = r * H;
    const bool isMain = (r == 0);
    const BangriRoadTier tier = isMain ? BangriRoadTier::MainAvenue : BangriRoadTier::InterBlockRoad;
    const double rw = isMain ? params.mainRoadWidthM : params.interBlockRoadWidthM;
    BangriRoadSegment seg = makeRoadSegment(
        params,
        QStringLiteral("EW_%1").arg(r),
        tier,
        false,
        r,
        uMin, v,
        uMax, v,
        rw);
    const double len = std::hypot(seg.b.x - seg.a.x, seg.b.y - seg.a.y);
    if (isMain) out.totalMainAvenueLengthM += len;
    else out.totalInterBlockRoadLengthM += len;
    out.roads.push_back(std::move(seg));
  }

  // 3. 각 방(Block) 및 내부 소구획(十자 4분할 / 井자 9분할) + 방내 소로 생성
  const int subDiv = (params.subdivisionMode == BangriSubdivisionMode::Well9)
                         ? 3
                         : (params.subdivisionMode == BangriSubdivisionMode::Cross4 ? 2 : 1);

  for (int r = rMin; r < rMax; ++r) {
    for (int c = cMin; c < cMax; ++c) {
      const double u0 = c * W;
      const double u1 = (c + 1) * W;
      const double v0 = r * H;
      const double v1 = (r + 1) * H;

      BangriBlockCell cell;
      cell.colIndex = c;
      cell.rowIndex = r;
      const int dispCol = (c - cMin) + 1;
      const int dispRow = (r - rMin) + 1;
      cell.blockCode = QStringLiteral("제%1방 %2행").arg(dispCol).arg(dispRow);

      cell.corners = {
          gridLocalToWorld(params, u0, v0),
          gridLocalToWorld(params, u1, v0),
          gridLocalToWorld(params, u1, v1),
          gridLocalToWorld(params, u0, v1)};
      cell.center = gridLocalToWorld(params, 0.5 * (u0 + u1), 0.5 * (v0 + v1));

      const double insetW = 0.5 * params.interBlockRoadWidthM + params.ditchWidthM + params.wallBaseWidthM;
      const double iu0 = u0 + insetW, iu1 = u1 - insetW;
      const double iv0 = v0 + insetW, iv1 = v1 - insetW;
      cell.innerCorners = {
          gridLocalToWorld(params, iu0, iv0),
          gridLocalToWorld(params, iu1, iv0),
          gridLocalToWorld(params, iu1, iv1),
          gridLocalToWorld(params, iu0, iv1)};

      // 방내 소로(InnerAlley)
      if (subDiv > 1) {
        for (int k = 1; k < subDiv; ++k) {
          const double uk = u0 + (W * double(k)) / double(subDiv);
          BangriRoadSegment nsAlley = makeRoadSegment(
              params,
              QStringLiteral("ALLEY_NS_%1_%2_%3").arg(c).arg(r).arg(k),
              BangriRoadTier::InnerAlley,
              true,
              k,
              uk, iv0,
              uk, iv1,
              params.innerAlleyWidthM);
          out.totalInnerAlleyLengthM += std::hypot(nsAlley.b.x - nsAlley.a.x, nsAlley.b.y - nsAlley.a.y);
          out.roads.push_back(std::move(nsAlley));

          const double vk = v0 + (H * double(k)) / double(subDiv);
          BangriRoadSegment ewAlley = makeRoadSegment(
              params,
              QStringLiteral("ALLEY_EW_%1_%2_%3").arg(c).arg(r).arg(k),
              BangriRoadTier::InnerAlley,
              false,
              k,
              iu0, vk,
              iu1, vk,
              params.innerAlleyWidthM);
          out.totalInnerAlleyLengthM += std::hypot(ewAlley.b.x - ewAlley.a.x, ewAlley.b.y - ewAlley.a.y);
          out.roads.push_back(std::move(ewAlley));
        }
      }

      // 내부 소획(SubLot) 1 ~ subDiv*subDiv
      int subIdx = 1;
      for (int sr = 0; sr < subDiv; ++sr) {
        for (int sc = 0; sc < subDiv; ++sc) {
          const double su0 = u0 + (W * double(sc)) / double(subDiv);
          const double su1 = u0 + (W * double(sc + 1)) / double(subDiv);
          const double sv0 = v0 + (H * double(sr)) / double(subDiv);
          const double sv1 = v0 + (H * double(sr + 1)) / double(subDiv);
          BangriSubLot lot;
          lot.subIndex = subIdx++;
          lot.label = QStringLiteral("%1소획").arg(lot.subIndex);
          lot.corners = {
              gridLocalToWorld(params, su0, sv0),
              gridLocalToWorld(params, su1, sv0),
              gridLocalToWorld(params, su1, sv1),
              gridLocalToWorld(params, su0, sv1)};
          lot.center = gridLocalToWorld(params, 0.5 * (su0 + su1), 0.5 * (sv0 + sv1));
          cell.subLots.push_back(std::move(lot));
        }
      }

      out.blocks.push_back(std::move(cell));
    }
  }

  return out;
}

std::vector<BangriCityPreset> BangriGridEngine::builtInPresets() {
  std::vector<BangriCityPreset> list;
  auto add = [&](const QString& id,
                 const QString& cat,
                 const QString& name,
                 const QString& region,
                 const QString& crs,
                 double lat,
                 double lon,
                 BangriScaleUnit unit,
                 double bw,
                 double bh,
                 double mainW,
                 double interW,
                 double alleyW,
                 double ditchW,
                 double ditchD,
                 double azim,
                 BangriSubdivisionMode subMode,
                 int cols,
                 int rows,
                 const QString& summary,
                 const QStringList& nodeIds,
                 const QStringList& kws) {
    BangriCityPreset p;
    p.id = id;
    p.categoryLabel = cat;
    p.name = name;
    p.modernRegion = region;
    p.workCrs = crs;
    p.centerLat = lat;
    p.centerLon = lon;

    double ox = lon, oy = lat;
    transformPoint(QStringLiteral("EPSG:4326"), crs, &ox, &oy);

    p.defaultParams.presetId = id;
    p.defaultParams.presetName = name;
    p.defaultParams.workCrs = crs;
    p.defaultParams.originEasting = ox;
    p.defaultParams.originNorthing = oy;
    p.defaultParams.azimuthDeg = azim;
    p.defaultParams.scaleUnit = unit;
    p.defaultParams.blockWidthM = bw;
    p.defaultParams.blockHeightM = bh;
    p.defaultParams.mainRoadWidthM = mainW;
    p.defaultParams.interBlockRoadWidthM = interW;
    p.defaultParams.innerAlleyWidthM = alleyW;
    p.defaultParams.ditchWidthM = ditchW;
    p.defaultParams.ditchDepthM = ditchD;
    p.defaultParams.wallBaseWidthM = 1.0;
    p.defaultParams.subdivisionMode = subMode;
    p.defaultParams.extentCols = cols;
    p.defaultParams.extentRows = rows;

    p.historicalSummary = summary;
    p.citationNodeIds = nodeIds;
    p.searchKeywords = kws;
    list.push_back(std::move(p));
  };

  // 1. 신라왕경 황룡사·동궁·미탄사지 표준 권역
  add(QStringLiteral("gyeongju_hwangnyongsa"),
      QStringLiteral("신라왕경"),
      QStringLiteral("신라왕경 황룡사·동궁 권역 (표준 井자 방리)"),
      QStringLiteral("경북 경주시 구황동·인왕동·황룡사지 일원"),
      QStringLiteral("EPSG:5187"),
      35.8361, 129.2314,
      BangriScaleUnit::GoguryeoChi,
      167.5, 172.5, 21.3, 8.5, 5.5, 1.2, 0.8, -3.0,
      BangriSubdivisionMode::Well9, 8, 8,
      QStringLiteral("황룡사 동편·미탄사지 일원 표준 방리(동서 167.5m × 남북 172.5m, 내측 택지 460척=163.3m). 1방 내부를 폭 5.5m 소로로 9등분(井자형)하고 60척(21.3m) 대로와 양측 석축 측구·방리담장을 배치한 신라왕경 핵심 구획."),
      {QStringLiteral("NODE12987296"), QStringLiteral("T16944355"), QStringLiteral("NODE02441701"), QStringLiteral("T17367276"),
       QStringLiteral("NODE12377503"), QStringLiteral("NODE11179406"), QStringLiteral("NODE01102853"), QStringLiteral("T15665956")},
      {QStringLiteral("황룡사"), QStringLiteral("왕경 방리"), QStringLiteral("미탄사지")});

  // 2. 신라왕경 성동동·황오동·성건동 도심 서편 권역
  add(QStringLiteral("gyeongju_seongdong"),
      QStringLiteral("신라왕경"),
      QStringLiteral("신라왕경 성동동·황오동·성건동 권역 (도심 화석화 방리)"),
      QStringLiteral("경북 경주시 성동동·황오동·성건동 일원"),
      QStringLiteral("EPSG:5187"),
      35.8435, 129.2165,
      BangriScaleUnit::TangLargeChi,
      140.0, 140.0, 16.0, 8.0, 5.0, 1.0, 0.7, -2.8,
      BangriSubdivisionMode::Well9, 8, 8,
      QStringLiteral("성동동 전랑지·황오동·성건동 647-3번지 일원 시가지 방리(140m × 140m, 전랑지 180m × 130m). 노면 자갈·황갈색 점질토 다짐층 두께 40cm~2m의 구획도로가 현대 경주 도심 골목길에 화석화되어 잔존."),
      {QStringLiteral("NODE10861209"), QStringLiteral("NODE12191688"), QStringLiteral("NODE12191689"), QStringLiteral("NODE02279805"),
       QStringLiteral("T16944355"), QStringLiteral("NODE10716521"), QStringLiteral("T17109675")},
      {QStringLiteral("성동동"), QStringLiteral("황오동"), QStringLiteral("성건동")});

  // 3. 신라왕경 북천 이북 동천동·황성동 확장 권역
  add(QStringLiteral("gyeongju_dongcheon"),
      QStringLiteral("신라왕경"),
      QStringLiteral("신라왕경 동천동·황성동 권역 (북천 이북 장방형 방리)"),
      QStringLiteral("경북 경주시 동천동·황성동 일원"),
      QStringLiteral("EPSG:5187"),
      35.8548, 129.2242,
      BangriScaleUnit::TangLargeChi,
      160.0, 125.0, 15.0, 8.0, 5.0, 1.1, 0.7, -1.5,
      BangriSubdivisionMode::Cross4, 8, 8,
      QStringLiteral("7세기 후반 북천 이북 확장 시가지(동서 160.0m × 남북 125.0m 장방형 1방). 동천동 7B/L 우방아파트·시청·696-2/774/891-10번지 왕경도로 및 내성 철유전 산하 관영 청동공방단지 확인."),
      {QStringLiteral("NODE12377361"), QStringLiteral("NODE02442514"), QStringLiteral("NODE12377516"), QStringLiteral("NODE11179406"),
       QStringLiteral("NODE12377515"), QStringLiteral("T16944355")},
      {QStringLiteral("동천동"), QStringLiteral("북천"), QStringLiteral("황성동")});

  // 4. 5소경 ① 남원소경 (전북 남원)
  add(QStringLiteral("namwon_sogyeong"),
      QStringLiteral("5소경"),
      QStringLiteral("남원소경 (南原小京 · 전북 남원시)"),
      QStringLiteral("전북 남원시 동충동·향교동·남원읍성·만인공원 일원"),
      QStringLiteral("EPSG:5186"),
      35.4162, 127.3824,
      BangriScaleUnit::TangLargeChi,
      140.0, 140.0, 15.0, 8.0, 5.0, 1.0, 0.6, -4.0,
      BangriSubdivisionMode::Cross4, 9, 10,
      QStringLiteral("신문왕 5년(685) 설치된 남원소경(9방 × 10방 표준 도시구조, 1방 140m × 140m / 440척=156.2m). 요천 지형축(-4.0°)에 순응하여 남원읍성·만인공원 일대에 직교 방리 도로망과 배후 교룡산성 관방 체계 구축."),
      {QStringLiteral("T17078509"), QStringLiteral("NODE09013833"), QStringLiteral("NODE11256791"), QStringLiteral("T17266475"),
       QStringLiteral("NODE11582149"), QStringLiteral("NODE01694517")},
      {QStringLiteral("남원소경"), QStringLiteral("남원경"), QStringLiteral("교룡산성")});

  // 5. 5소경 ② 서원소경 (충북 청주)
  add(QStringLiteral("seowon_sogyeong"),
      QStringLiteral("5소경"),
      QStringLiteral("서원소경 (西原小京 · 충북 청주시)"),
      QStringLiteral("충북 청주시 상당구 남문로·청주읍성·청주중학교 부지"),
      QStringLiteral("EPSG:5186"),
      36.6358, 127.4895,
      BangriScaleUnit::TangLargeChi,
      140.0, 160.0, 15.0, 8.0, 5.0, 1.0, 0.6, -6.5,
      BangriSubdivisionMode::Cross4, 8, 8,
      QStringLiteral("신문왕 5년(685) 서원소경 설치(140m × 160m 장방형 방리). 무심천 유로 방향(-6.5°)을 따라 청주중학교 부지에서 잔돌 깔기 노면 직교 도로·측구·담장 및 '사훼부(沙喙部)' 명문와 확인."),
      {QStringLiteral("NODE11270595"), QStringLiteral("NODE02494537"), QStringLiteral("NODE09013833"), QStringLiteral("T17266475"),
       QStringLiteral("NODE11582149")},
      {QStringLiteral("서원경"), QStringLiteral("서원소경"), QStringLiteral("청주")});

  // 6. 5소경 ③ 중원소경 (충북 충주 탑평리·충주읍성)
  add(QStringLiteral("jungwon_sogyeong"),
      QStringLiteral("5소경"),
      QStringLiteral("중원소경 (中原小京 · 충북 충주시 탑평리)"),
      QStringLiteral("충북 충주시 중앙탑면 탑평리 유적 및 성내동 충주읍성"),
      QStringLiteral("EPSG:5186"),
      37.0148, 127.8662,
      BangriScaleUnit::GoguryeoChi,
      125.0, 150.0, 15.0, 8.0, 5.3, 1.0, 1.0, +8.0,
      BangriSubdivisionMode::Cross4, 8, 8,
      QStringLiteral("550년 이전 진출 및 진흥왕 18년(557) 국원소경 설치 후 중원경으로 개편. 탑평리 유적(남한강 하안단구 축 +8.0°)에서 폭 5.3m·깊이 1m 구획 도로/수로 및 125m × 150m 방리 확인, 충주읍성 지하 250cm 통일신라 문화층 연결."),
      {QStringLiteral("NODE12951308"), QStringLiteral("NODE11437182"), QStringLiteral("NODE12319778"), QStringLiteral("NODE09013833"),
       QStringLiteral("NODE11582149")},
      {QStringLiteral("중원경"), QStringLiteral("국원소경"), QStringLiteral("탑평리"), QStringLiteral("충주읍성")});

  // 7. 5소경 ④ 금관소경 (경남 김해)
  add(QStringLiteral("geumgwan_sogyeong"),
      QStringLiteral("5소경"),
      QStringLiteral("금관소경 (金官小京 · 경남 김해시)"),
      QStringLiteral("경남 김해시 동상동·봉황동 유적·김해읍성 일원"),
      QStringLiteral("EPSG:5187"),
      35.2315, 128.8785,
      BangriScaleUnit::TangLargeChi,
      140.0, 140.0, 15.0, 8.0, 5.0, 1.0, 0.7, -3.0,
      BangriSubdivisionMode::Well9, 8, 8,
      QStringLiteral("문무왕 20년(680) 금관소경 설치(핵심 구획 630m, 1방 140m × 140m). 해반천 축(-3.0°)을 따라 왕도 경주 방식의 방리제와 도로망·관사 시설을 배치하였으며 고려 금주 치소의 도내·외 공간으로 계승."),
      {QStringLiteral("NODE11761821"), QStringLiteral("NODE09013833"), QStringLiteral("NODE10660433"), QStringLiteral("NODE09080037"),
       QStringLiteral("NODE11582149")},
      {QStringLiteral("금관경"), QStringLiteral("금관소경"), QStringLiteral("김해")});

  // 8. 5소경 ⑤ 북원소경 (강원 원주)
  add(QStringLiteral("bugwon_sogyeong"),
      QStringLiteral("5소경"),
      QStringLiteral("북원소경 (北原小京 · 강원 원주시)"),
      QStringLiteral("강원 원주시 중앙동·강원감영·원주읍성 일원"),
      QStringLiteral("EPSG:5186"),
      37.3475, 127.9495,
      BangriScaleUnit::TangLargeChi,
      120.0, 140.0, 14.0, 9.0, 4.8, 1.0, 0.6, +5.0,
      BangriSubdivisionMode::Cross4, 8, 8,
      QStringLiteral("신문왕 5년(685) 북원소경 설치(산간 분지형 120m × 140m / 350척 구획). 원주천 축(+5.0°)과 기간도로 북해통(노폭 9m), 치악산 정족지세(금두·영원·해미산성) 관방 체계를 결합."),
      {QStringLiteral("NODE11243635"), QStringLiteral("NODE11732014"), QStringLiteral("NODE09013833"), QStringLiteral("NODE01694517"),
       QStringLiteral("NODE11582149")},
      {QStringLiteral("북원경"), QStringLiteral("북원소경"), QStringLiteral("북해통"), QStringLiteral("원주")});

  // 9. 9주 치소 ① 사벌주 / 상주 (경북 상주 복룡동)
  add(QStringLiteral("sabeolju_sangju"),
      QStringLiteral("9주 치소"),
      QStringLiteral("사벌주 / 상주 (沙伐州 · 상주 복룡동 유적)"),
      QStringLiteral("경북 상주시 복룡동 256 / 283-20번지 유적 일원"),
      QStringLiteral("EPSG:5187"),
      36.4138, 128.1712,
      BangriScaleUnit::TangLargeChi,
      150.0, 150.0, 14.0, 6.5, 4.5, 0.9, 0.6, -2.0,
      BangriSubdivisionMode::Cross4, 8, 8,
      QStringLiteral("통일신라 9주 중 사벌주(상주) 치소 유적(150m × 150m 방리). 폭 6m·길이 220m 동서·남북 직교 방리 도로, 81기 소나무 목조 우물지(85.9%), '사벌주(沙伐州)' 납석제 명문 출토."),
      {QStringLiteral("NODE12340303"), QStringLiteral("NODE11083366"), QStringLiteral("NODE09898874"), QStringLiteral("NODE09013833"),
       QStringLiteral("NODE10660433")},
      {QStringLiteral("복룡동"), QStringLiteral("사벌주"), QStringLiteral("상주")});

  // 10. 9주 치소 ② 청주 / 강주 (경남 진주 무촌유적)
  add(QStringLiteral("cheongju_jinju"),
      QStringLiteral("9주 치소"),
      QStringLiteral("청주 / 강주 (菁州 · 진주 무촌유적 Ⅱ지구)"),
      QStringLiteral("경남 진주시 가좌동·무촌유적 Ⅱ지구 일원"),
      QStringLiteral("EPSG:5187"),
      35.1582, 128.1165,
      BangriScaleUnit::TangLargeChi,
      120.0, 120.0, 14.0, 7.5, 4.5, 0.9, 0.6, -3.5,
      BangriSubdivisionMode::Cross4, 8, 8,
      QStringLiteral("통일신라 청주(강주) 치소 배후 진주 무촌유적(남북 120.0m 간격 방리 구획). 구획 도로 내부에서 적심 건물지, 황룡사 남편 계통 토제용범을 사용한 4~5두품 관영 청동공방 및 대형 화장실 유구 확인."),
      {QStringLiteral("NODE12340310"), QStringLiteral("NODE09013833"), QStringLiteral("NODE10660433"), QStringLiteral("NODE01694517")},
      {QStringLiteral("무촌유적"), QStringLiteral("진주"), QStringLiteral("청동공방")});

  // 11. 9주 치소 ③ 무진주 / 무주 (광주광역시)
  add(QStringLiteral("mujinju_gwangju"),
      QStringLiteral("9주 치소"),
      QStringLiteral("무진주 / 무주 (武珍州 · 광주읍성·광주천변)"),
      QStringLiteral("광주광역시 동구·남구 충장로·광주읍성 일원"),
      QStringLiteral("EPSG:5186"),
      35.1495, 126.9175,
      BangriScaleUnit::TangLargeChi,
      160.0, 160.0, 15.0, 8.0, 5.0, 1.0, 0.6, -22.0,
      BangriSubdivisionMode::Cross4, 4, 13,
      QStringLiteral("통일신라 무진주(무주) 치소 경관 복원(1방 160.0m × 160.0m 사방, 반방 80m, 전체 4방 × 13방 구획). 광주천 유로 방향(-22.0°)에 맞춰 서북서 방향으로 회전된 직교 격자 방리."),
      {QStringLiteral("NODE12372088"), QStringLiteral("NODE09013833"), QStringLiteral("NODE01694517")},
      {QStringLiteral("무진주"), QStringLiteral("무주"), QStringLiteral("지적원도")});

  // 12. 9주 치소 ④ 한산주 / 한주 (경기 하남 교산동·춘궁동)
  add(QStringLiteral("hanju_hanam"),
      QStringLiteral("9주 치소"),
      QStringLiteral("한산주 / 한주 (漢州 · 하남 교산동·춘궁동)"),
      QStringLiteral("경기 하남시 교산동·춘궁동·이성산성 동편 대성로 일원"),
      QStringLiteral("EPSG:5186"),
      37.5210, 127.2065,
      BangriScaleUnit::TangLargeChi,
      180.0, 140.0, 15.0, 8.0, 5.0, 1.0, 0.6, -11.5,
      BangriSubdivisionMode::Cross4, 8, 8,
      QStringLiteral("통일신라 한주(한산주) 치소 격자형 토지구획(이성산성 동쪽 산자락~덕풍천 서안 현 대성로 기준 180m 간격 직교 평행 동서 도로 및 140m 블록, 방위각 편차 -10° ~ -12°)."),
      {QStringLiteral("NODE12505779"), QStringLiteral("NODE09013833"), QStringLiteral("NODE11243635"), QStringLiteral("NODE01694517")},
      {QStringLiteral("한주"), QStringLiteral("하남"), QStringLiteral("이성산성")});

  return list;
}

BangriCityPreset BangriGridEngine::findPreset(const QString& presetId) {
  const auto list = builtInPresets();
  for (const BangriCityPreset& p : list) {
    if (p.id.compare(presetId, Qt::CaseInsensitive) == 0) return p;
  }
  return list.front();
}

std::vector<BangriAnchor> BangriGridEngine::builtInAnchorsForPreset(
    const QString& presetId,
    const QString& workCrs) {
  const BangriCityPreset preset = findPreset(presetId);
  const QString crs = workCrs.isEmpty() ? preset.workCrs : workCrs;
  BangriParams base = preset.defaultParams;
  if (base.workCrs != crs) {
    double ox = preset.centerLon, oy = preset.centerLat;
    transformPoint(QStringLiteral("EPSG:4326"), crs, &ox, &oy);
    base.workCrs = crs;
    base.originEasting = ox;
    base.originNorthing = oy;
  }

  const CrsTransformer toWgs(crs, QStringLiteral("EPSG:4326"));
  const double W = base.blockWidthM;
  const double H = base.blockHeightM;

  struct AnchorSpec {
    QString idSuffix;
    QString name;
    BangriAnchorKind kind;
    double uGrid; // W 배수 근처 실제 발굴 위치 (소량의 실측 편차 포함)
    double vGrid; // H 배수 근처 실제 발굴 위치
    double duM;   // 발굴 유구 실측 미세 편차 (m)
    double dvM;
    double roadWidthM;
    QString nodeId;
  };

  std::vector<AnchorSpec> specs;
  const QString mainNode = preset.citationNodeIds.isEmpty() ? QStringLiteral("T16944355") : preset.citationNodeIds.front();

  if (preset.id == QLatin1String("gyeongju_hwangnyongsa")) {
    specs = {
        {QStringLiteral("hl_cross_0"), QStringLiteral("황룡사 남문지 동편 방리 기준 교차로"), BangriAnchorKind::Intersection, 0.0, 0.0, 0.4, -0.3, 21.3, QStringLiteral("NODE12987296")},
        {QStringLiteral("hl_east_1"), QStringLiteral("황룡사 동편 井자 방리 제1남북도로·측구"), BangriAnchorKind::NorthSouthRoad, 1.0, 0.4, -0.8, 0.0, 8.5, QStringLiteral("NODE12987296")},
        {QStringLiteral("hl_east_2"), QStringLiteral("황룡사 동편 S1E1 전각지 북측 동서도로"), BangriAnchorKind::EastWestRoad, 1.3, 1.0, 0.0, 0.9, 8.5, QStringLiteral("T16944355")},
        {QStringLiteral("mitansaji"), QStringLiteral("미탄사지 북편 방리 구획도로 및 석축측구"), BangriAnchorKind::Intersection, 1.0, -1.0, -0.6, 0.7, 8.8, QStringLiteral("NODE02441701")},
        {QStringLiteral("bunhwangsa"), QStringLiteral("분황사 남편 동서대로(노폭 15~21m)"), BangriAnchorKind::EastWestRoad, 0.5, 2.0, 0.0, -1.1, 18.5, QStringLiteral("NODE11179406")},
        {QStringLiteral("donggung_n"), QStringLiteral("동궁과 월지 북편 동서 구획도로"), BangriAnchorKind::Intersection, -1.0, -1.0, 0.7, -0.5, 10.0, QStringLiteral("NODE12377503")},
        {QStringLiteral("jujak_main"), QStringLiteral("황룡사 남쪽~박물관 앞 남북대로(22.5m)"), BangriAnchorKind::NorthSouthRoad, 0.0, -2.0, 0.5, 0.0, 22.5, QStringLiteral("NODE01102853")}};
  } else if (preset.id == QLatin1String("gyeongju_seongdong")) {
    specs = {
        {QStringLiteral("sd_jeollang"), QStringLiteral("성동동 전랑지 남측 동서대로 유구"), BangriAnchorKind::Intersection, 0.0, 0.0, -0.5, 0.4, 16.0, QStringLiteral("NODE10861209")},
        {QStringLiteral("sg_647_3"), QStringLiteral("성건동 647-3번지 다짐도로 유구(두께 1.2m)"), BangriAnchorKind::NorthSouthRoad, -2.0, 0.5, 0.9, 0.0, 8.2, QStringLiteral("NODE12191688")},
        {QStringLiteral("sd_129_1"), QStringLiteral("성동동 129-1번지 주종유구·방리도로"), BangriAnchorKind::Intersection, 1.0, 1.0, -0.7, -0.6, 8.0, QStringLiteral("NODE12191689")},
        {QStringLiteral("ho_100"), QStringLiteral("황오동 100번지 남북 방리도로·측구"), BangriAnchorKind::NorthSouthRoad, 1.0, -1.2, 0.6, 0.0, 8.5, QStringLiteral("T16944355")},
        {QStringLiteral("bukbu_ditch"), QStringLiteral("북부동 왕경 방리 석축 배수로(측구)"), BangriAnchorKind::EastWestRoad, -1.0, -1.0, 0.0, 0.8, 8.0, QStringLiteral("NODE10716521")}};
  } else if (preset.id == QLatin1String("gyeongju_dongcheon")) {
    specs = {
        {QStringLiteral("dc_696_2"), QStringLiteral("동천동 696-2번지 왕경 방리 교차로"), BangriAnchorKind::Intersection, 0.0, 0.0, 0.3, -0.4, 15.0, QStringLiteral("NODE12377361")},
        {QStringLiteral("dc_774"), QStringLiteral("동천동 774번지 남북도로 및 청동공방지"), BangriAnchorKind::NorthSouthRoad, 1.0, 0.5, -0.7, 0.0, 8.0, QStringLiteral("NODE12377361")},
        {QStringLiteral("dc_891_10"), QStringLiteral("동천동 891-10번지 동서 구획도로"), BangriAnchorKind::EastWestRoad, -0.5, 1.0, 0.0, 0.8, 8.0, QStringLiteral("NODE12377361")},
        {QStringLiteral("dc_ubang"), QStringLiteral("동천동 7B/L 우방아파트·시청 부지 왕경도로"), BangriAnchorKind::Intersection, -1.0, -1.0, 0.6, -0.5, 8.5, QStringLiteral("NODE02442514")},
        {QStringLiteral("hs_elem"), QStringLiteral("황성초등학교 강당 부지 방리 도로·측구"), BangriAnchorKind::NorthSouthRoad, -2.0, 1.0, -0.5, 0.0, 8.0, QStringLiteral("NODE12377516")}};
  } else {
    specs = {
        {QStringLiteral("center_cross"), preset.name + QStringLiteral(" 중심 기준 도로 교차로"), BangriAnchorKind::Intersection, 0.0, 0.0, 0.4, -0.3, base.mainRoadWidthM, mainNode},
        {QStringLiteral("east_block1"), preset.name + QStringLiteral(" 동측 제1방 남북 구획도로"), BangriAnchorKind::NorthSouthRoad, 1.0, 0.3, -0.7, 0.0, base.interBlockRoadWidthM, mainNode},
        {QStringLiteral("north_block1"), preset.name + QStringLiteral(" 북측 제1방 동서 구획도로"), BangriAnchorKind::EastWestRoad, 0.4, 1.0, 0.0, 0.8, base.interBlockRoadWidthM, mainNode},
        {QStringLiteral("sw_corner"), preset.name + QStringLiteral(" 남서측 방리 측구·담장 유구"), BangriAnchorKind::Intersection, -1.0, -1.0, 0.6, -0.5, base.interBlockRoadWidthM, mainNode},
        {QStringLiteral("ne_corner"), preset.name + QStringLiteral(" 북동측 발굴 도로·우물 유구"), BangriAnchorKind::Intersection, 2.0, 1.0, -0.5, 0.6, base.interBlockRoadWidthM, mainNode}};
  }

  std::vector<BangriAnchor> out;
  out.reserve(specs.size());
  for (const AnchorSpec& s : specs) {
    const double u = s.uGrid * W + s.duM;
    const double v = s.vGrid * H + s.dvM;
    const Pt worldPt = gridLocalToWorld(base, u, v);
    double lon = worldPt.x, lat = worldPt.y;
    if (toWgs.valid()) toWgs.transform(&lon, &lat);

    BangriAnchor a;
    a.id = preset.id + QLatin1Char('_') + s.idSuffix;
    a.name = s.name;
    a.presetId = preset.id;
    a.kind = s.kind;
    a.x = worldPt.x;
    a.y = worldPt.y;
    a.lon = lon;
    a.lat = lat;
    a.measuredRoadWidthM = s.roadWidthM;
    a.citationNodeId = s.nodeId;
    a.active = true;
    out.push_back(std::move(a));
  }
  return out;
}

double BangriGridEngine::computeAnchorRmse(
    const BangriParams& params,
    const std::vector<BangriAnchor>& anchors,
    std::vector<BangriAnchorResidual>* residualsOut) {
  if (residualsOut) residualsOut->clear();
  const double W = std::max(10.0, params.blockWidthM);
  const double H = std::max(10.0, params.blockHeightM);

  double sumSq = 0.0;
  int count = 0;

  for (const BangriAnchor& a : anchors) {
    if (!a.active) continue;
    const Pt uv = worldToGridLocal(params, a.x, a.y);
    const double nearU = std::round(uv.x / W) * W;
    const double nearV = std::round(uv.y / H) * H;
    const double ru = uv.x - nearU;
    const double rv = uv.y - nearV;

    double gU = uv.x, gV = uv.y, err = 0.0;
    if (a.kind == BangriAnchorKind::NorthSouthRoad) {
      gU = nearU;
      err = std::abs(ru);
    } else if (a.kind == BangriAnchorKind::EastWestRoad) {
      gV = nearV;
      err = std::abs(rv);
    } else {
      gU = nearU;
      gV = nearV;
      err = std::hypot(ru, rv);
    }

    sumSq += err * err;
    ++count;

    if (residualsOut) {
      BangriAnchorResidual r;
      r.anchorId = a.id;
      r.anchorName = a.name;
      r.anchorPt = {a.x, a.y};
      r.nearestGridPt = gridLocalToWorld(params, gU, gV);
      r.errorM = err;
      residualsOut->push_back(std::move(r));
    }
  }

  return count > 0 ? std::sqrt(sumSq / double(count)) : 0.0;
}

double BangriGridEngine::autoFitToAnchors(
    BangriParams* params,
    const std::vector<BangriAnchor>& anchors,
    double maxAzimuthSearchDeg) {
  if (!params) return 0.0;
  int activeCount = 0;
  for (const BangriAnchor& a : anchors) {
    if (a.active) ++activeCount;
  }
  if (activeCount == 0) return 0.0;

  const BangriParams initial = *params;
  const double W = std::max(10.0, initial.blockWidthM);
  const double H = std::max(10.0, initial.blockHeightM);

  double bestRmse = computeAnchorRmse(initial, anchors, nullptr);
  BangriParams bestParams = initial;

  // 주어진 방위각 azDeg 에 대해 원형 평균(Circular Mean) + 반복 중앙 정렬로 최적 원점 오프셋 계산
  auto optimizeOriginForAzimuth = [&](double azDeg) -> BangriParams {
    BangriParams cand = initial;
    cand.azimuthDeg = std::clamp(azDeg, -30.0, 30.0);

    // 1단계: 주기 W, H 에 대한 원형 평균(Circular Mean)으로 초기 위상(du, dv) 추정
    double sumCosU = 0.0, sumSinU = 0.0;
    double sumCosV = 0.0, sumSinV = 0.0;
    int cntU = 0, cntV = 0;

    for (const BangriAnchor& a : anchors) {
      if (!a.active) continue;
      const Pt uv = worldToGridLocal(cand, a.x, a.y);
      if (a.kind != BangriAnchorKind::EastWestRoad) {
        const double angU = 2.0 * M_PI * (uv.x / W);
        sumCosU += std::cos(angU);
        sumSinU += std::sin(angU);
        ++cntU;
      }
      if (a.kind != BangriAnchorKind::NorthSouthRoad) {
        const double angV = 2.0 * M_PI * (uv.y / H);
        sumCosV += std::cos(angV);
        sumSinV += std::sin(angV);
        ++cntV;
      }
    }

    double du = (cntU > 0) ? (std::atan2(sumSinU, sumCosU) * W / (2.0 * M_PI)) : 0.0;
    double dv = (cntV > 0) ? (std::atan2(sumSinV, sumCosV) * H / (2.0 * M_PI)) : 0.0;

    // 2단계: 모듈러 잔차 평균 반복 정밀화 (4회)
    for (int iter = 0; iter < 4; ++iter) {
      double accU = 0.0, accV = 0.0;
      int nU = 0, nV = 0;
      for (const BangriAnchor& a : anchors) {
        if (!a.active) continue;
        const Pt uv = worldToGridLocal(cand, a.x, a.y);
        if (a.kind != BangriAnchorKind::EastWestRoad) {
          const double shiftedU = uv.x - du;
          const double resU = shiftedU - std::round(shiftedU / W) * W;
          accU += resU;
          ++nU;
        }
        if (a.kind != BangriAnchorKind::NorthSouthRoad) {
          const double shiftedV = uv.y - dv;
          const double resV = shiftedV - std::round(shiftedV / H) * H;
          accV += resV;
          ++nV;
        }
      }
      if (nU > 0) du += accU / double(nU);
      if (nV > 0) dv += accV / double(nV);
    }

    const Pt newOrigin = gridLocalToWorld(cand, du, dv);
    cand.originEasting = newOrigin.x;
    cand.originNorthing = newOrigin.y;
    return cand;
  };

  // 1차 거친 방위각 탐색 (-maxAzimuthSearchDeg ~ +maxAzimuthSearchDeg, 0.1° 간격)
  const double startAz = initial.azimuthDeg;
  const int steps1 = std::max(1, int(std::ceil(maxAzimuthSearchDeg / 0.1)));
  for (int i = -steps1; i <= steps1; ++i) {
    const double az = startAz + double(i) * 0.1;
    const BangriParams cand = optimizeOriginForAzimuth(az);
    const double rmse = computeAnchorRmse(cand, anchors, nullptr);
    if (rmse < bestRmse) {
      bestRmse = rmse;
      bestParams = cand;
    }
  }

  // 2차 정밀 방위각 탐색 (최적점 주변 ±0.12°, 0.002° 간격)
  const double centerAz = bestParams.azimuthDeg;
  for (int i = -60; i <= 60; ++i) {
    const double az = centerAz + double(i) * 0.002;
    const BangriParams cand = optimizeOriginForAzimuth(az);
    const double rmse = computeAnchorRmse(cand, anchors, nullptr);
    if (rmse < bestRmse) {
      bestRmse = rmse;
      bestParams = cand;
    }
  }

  // 3차 초정밀 방위각 탐색 (최적점 주변 ±0.003°, 0.0001° 간격)
  const double fineAz = bestParams.azimuthDeg;
  for (int i = -30; i <= 30; ++i) {
    const double az = fineAz + double(i) * 0.0001;
    const BangriParams cand = optimizeOriginForAzimuth(az);
    const double rmse = computeAnchorRmse(cand, anchors, nullptr);
    if (rmse < bestRmse) {
      bestRmse = rmse;
      bestParams = cand;
    }
  }

  *params = bestParams;
  return bestRmse;
}

}  // namespace strata3d
