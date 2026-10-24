#pragma once

#include <QString>
#include <QStringList>
#include <array>
#include <vector>

#include "GeoFeature.h"

namespace strata3d {

enum class BangriScaleUnit {
  GoguryeoChi = 0,  // 고구려척 (1척 = 0.356 m, 왕경 Ⅰ기·황룡사 창건기)
  TangLargeChi = 1, // 당대척 (1척 = 0.296 m, 1보 = 5척 = 1.48 m, 왕경 Ⅱ·Ⅲ기 및 9주5소경)
  Meter = 2         // 미터 (1.0 m)
};

enum class BangriSubdivisionMode {
  None = 0,   // 내부 분할 없음
  Cross4 = 1, // 十자형 4분할 (동서 1줄 · 남북 1줄 소로)
  Well9 = 2   // 井자형 9분할 (동서 2줄 · 남북 2줄 소로, 황룡사 동편 표준)
};

enum class BangriRoadTier {
  MainAvenue = 0,     // 주작대로·중심 간선 대로 (폭 15~23 m)
  InterBlockRoad = 1, // 방간 중로 (폭 8.0~10.5 m)
  InnerAlley = 2      // 방내 소로 (폭 4.5~6.0 m, 井/十자 분할 도로)
};

enum class BangriAnchorKind {
  Intersection = 0,   // 동서·남북 도로 교차로 유구
  NorthSouthRoad = 1, // 남북 방향 도로 중심/측구 유구
  EastWestRoad = 2    // 동서 방향 도로 중심/측구 유구
};

struct BangriParams {
  QString presetId = QStringLiteral("gyeongju_hwangnyongsa");
  QString presetName = QStringLiteral("신라왕경 황룡사·동궁 권역");
  QString workCrs = QStringLiteral("EPSG:5187");

  double originEasting = 0.0;   // 기준 원점 X (m, workCrs)
  double originNorthing = 0.0;  // 기준 원점 Y (m, workCrs)
  double azimuthDeg = -3.0;     // 진북(도면 북) 대비 시계방향 방위각 편차 (-30° ~ +30°)

  BangriScaleUnit scaleUnit = BangriScaleUnit::TangLargeChi;
  double blockWidthM = 167.5;        // 1방 동서 폭 (m)
  double blockHeightM = 172.5;       // 1방 남북 길이 (m)
  double mainRoadWidthM = 20.0;      // 간선 대로 폭 (m)
  double interBlockRoadWidthM = 8.5; // 방간 중로 폭 (m)
  double innerAlleyWidthM = 5.5;     // 방내 소로 폭 (m)
  double ditchWidthM = 1.2;          // 양측 측구(배수로) 폭 (m)
  double ditchDepthM = 0.7;          // 양측 측구 깊이 (m)
  double wallBaseWidthM = 1.0;       // 방리담장 기단 폭 (m)

  BangriSubdivisionMode subdivisionMode = BangriSubdivisionMode::Well9;
  int extentCols = 8; // 동서 생성 방 수 (예: 8방)
  int extentRows = 8; // 남북 생성 방 수 (예: 8방)
};

struct BangriAnchor {
  QString id;
  QString name;
  QString presetId;
  BangriAnchorKind kind = BangriAnchorKind::Intersection;
  double lon = 0.0; // WGS84 경도
  double lat = 0.0; // WGS84 위도
  double x = 0.0;   // workCrs 미터 좌표 X
  double y = 0.0;   // workCrs 미터 좌표 Y
  double measuredRoadWidthM = 8.5;
  QString citationNodeId;
  bool active = true;
};

struct BangriAnchorResidual {
  QString anchorId;
  QString anchorName;
  Pt anchorPt;
  Pt nearestGridPt;
  double errorM = 0.0;
};

struct BangriCityPreset {
  QString id;
  QString categoryLabel; // "신라왕경" | "5소경" | "9주 치소"
  QString name;
  QString modernRegion;
  QString workCrs;
  double centerLon = 129.2314;
  double centerLat = 35.8361;
  BangriParams defaultParams;
  QString historicalSummary;
  QStringList citationNodeIds;
  QStringList searchKeywords;
};

struct BangriRoadSegment {
  QString id;
  BangriRoadTier tier = BangriRoadTier::InterBlockRoad;
  bool isNorthSouth = false; // true: 남북도로, false: 동서도로
  int gridIndex = 0;         // 격자 인덱스 (0 이면 중심 대로)
  Pt a;                      // 도로 중심선 시점 (workCrs m)
  Pt b;                      // 도로 중심선 종점 (workCrs m)
  double widthM = 8.5;
  double ditchWidthM = 1.2;
  double ditchDepthM = 0.7;
  // 양측 측구(배수로) 중심선
  Pt leftDitchA, leftDitchB;
  Pt rightDitchA, rightDitchB;
  // 양측 방리담장 기단선
  Pt leftWallA, leftWallB;
  Pt rightWallA, rightWallB;
};

struct BangriSubLot {
  int subIndex = 1; // 1 ~ 9 (또는 1 ~ 4)
  QString label;    // 예: "5소획"
  std::array<Pt, 4> corners;
  Pt center;
};

struct BangriBlockCell {
  int colIndex = 0; // 동서 방 번호 (-cols/2 .. +cols/2 - 1)
  int rowIndex = 0; // 남북 방 번호 (-rows/2 .. +rows/2 - 1)
  QString blockCode; // 예: "제3방 2리"
  std::array<Pt, 4> corners; // 도로 중심선 기준 1방 사각형
  std::array<Pt, 4> innerCorners; // 도로·측구·담장 제외한 순수 택지 외곽
  Pt center;
  std::vector<BangriSubLot> subLots;
};

struct BangriGridResult {
  BangriParams params;
  std::vector<BangriRoadSegment> roads;
  std::vector<BangriBlockCell> blocks;
  double totalMainAvenueLengthM = 0.0;
  double totalInterBlockRoadLengthM = 0.0;
  double totalInnerAlleyLengthM = 0.0;
};

class BangriGridEngine {
 public:
  // 척도 단위 1척(또는 1m)의 미터 값 반환
  static double scaleUnitMeters(BangriScaleUnit unit);
  static double metersToScaleUnit(double meters, BangriScaleUnit unit);
  static double scaleUnitToMeters(double scaleValue, BangriScaleUnit unit);
  static QString scaleUnitName(BangriScaleUnit unit);
  static QString subdivisionModeName(BangriSubdivisionMode mode);

  // 지역 격자 좌표 (u: 동서 m, v: 남북 m) <-> 작업 좌표계 (x, y) 변환
  static Pt gridLocalToWorld(const BangriParams& params, double u, double v);
  static Pt worldToGridLocal(const BangriParams& params, double x, double y);

  // 12개 신라왕경·9주5소경 프리셋 목록
  static std::vector<BangriCityPreset> builtInPresets();
  static BangriCityPreset findPreset(const QString& presetId);

  // 프리셋별 실제 발굴 도로·교차로 기준점(Anchor) 목록
  static std::vector<BangriAnchor> builtInAnchorsForPreset(const QString& presetId,
                                                           const QString& workCrs = QString());

  // 파라메트릭 방리 격자(대로·중로·방내 소로·양측 측구·담장·블록·소획) 생성
  static BangriGridResult generateGrid(const BangriParams& params);

  // 발굴 기준점(Anchor)들과 현재 방리 격자 사이의 수직 오차 및 RMSE(m) 계산
  static double computeAnchorRmse(const BangriParams& params,
                                  const std::vector<BangriAnchor>& anchors,
                                  std::vector<BangriAnchorResidual>* residualsOut = nullptr);

  // 최소제곱/모듈러 원형 평균 + 황금분할 방위각 탐색으로 발굴 기준점 RMSE 를 최소화하도록
  // originEasting, originNorthing, azimuthDeg 를 자동 정합한다. 정합 후 RMSE(m)를 반환한다.
  static double autoFitToAnchors(BangriParams* params,
                                 const std::vector<BangriAnchor>& anchors,
                                 double maxAzimuthSearchDeg = 6.0);
};

}  // namespace strata3d
