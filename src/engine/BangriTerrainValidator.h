#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "BangriGridEngine.h"
#include "DemGrid.h"
#include "SceneMesh.h"
#include "SillaMoundReconstructor.h"

namespace strata3d {

struct GyeongjuHeritageSiteMeta {
  QString id;           // 예: "hwangnyongsa", "cheomseongdae", "daereungwon_hwangnam"
  QString name;         // 예: "황룡사 9층목탑 · 대가람 (사적 제6호)"
  QString category;     // 예: "사적 제6호", "국보 제31호"
  QString subtitle;     // 예: "중금당·동서금당·강당·남문·회랑 실제 유구 위치"
  double lat = 0.0;     // WGS84 위도
  double lon = 0.0;     // WGS84 경도
  float focusYawDeg = 14.0f;
  float focusPitchDeg = 32.0f;
  float focusDistM = 750.0f;
  float labelHeightM = 18.0f;
  std::array<float, 4> badgeColor = {0.80f, 0.25f, 0.15f, 1.0f};
  bool importantLabel = true;
  uint32_t blenderVertexCount = 0;
  uint32_t blenderTriangleCount = 0;
};

struct UserDrawnLine3D {
  Pt a;
  Pt b;
  float widthM = 4.0f;
  std::array<float, 4> color = {1.0f, 0.84f, 0.18f, 1.0f};
  QString label;
  bool isPreview = false;
};

struct UserDrawnGrid3D {
  Pt origin;               // 격자 중심 또는 시작 모서리 (centerAligned 에 따라 결정)
  double cellWidthM = 140.0;
  double cellHeightM = 140.0;
  int cols = 6;
  int rows = 6;
  double azimuthDeg = 0.0;
  float lineWidthM = 3.0f;
  int subDivisions = 1;    // 1: 외곽 격자만, 2: 十자 4분할 보조격자, 3: 井자 9분할 보조격자
  std::array<float, 4> color = {0.18f, 0.86f, 1.0f, 1.0f};
  bool centerAligned = true;
  QString label;
  bool isPreview = false;
};

enum class BangriRoadFeasibility {
  ValidFlat = 0,           // 경사 < 8°, 고분·하천 간섭 없음 (실제 방리 도로·측구 개설 구간)
  MoundInterference = 1,   // 3D 고분 봉분(대릉원·쪽샘 등) 간섭으로 우회·단절되는 구간
  RiverOrPaleoChannel = 2, // 북천·남천·발천 및 구하도 범람원(교량·제방 필요 구간)
  SteepTerrain = 3         // 경사 >= 8~10° 산지(낭산·남산·명활산 등) 절단 구간
};

struct ValidatedRoadChunk {
  Pt a;
  Pt b;
  BangriRoadTier tier = BangriRoadTier::InterBlockRoad;
  BangriRoadFeasibility feasibility = BangriRoadFeasibility::ValidFlat;
  double widthM = 8.5;
  double ditchWidthM = 1.2;
  double slopeDeg = 0.0;
  double moundHeightM = 0.0;
  Pt leftDitchA, leftDitchB;
  Pt rightDitchA, rightDitchB;
};

struct PaleoChannelCorridor {
  QString id;
  QString name;
  std::vector<Pt> centerline;
  double halfWidthM = 35.0;
};

struct BangriValidationResult {
  std::vector<ValidatedRoadChunk> chunks;
  std::vector<PaleoChannelCorridor> channels;
  double totalLengthM = 0.0;
  double validFlatLengthM = 0.0;
  double moundInterferenceLengthM = 0.0;
  double riverChannelLengthM = 0.0;
  double steepTerrainLengthM = 0.0;

  double validFlatPct = 0.0;
  double moundInterferencePct = 0.0;
  double riverChannelPct = 0.0;
  double steepTerrainPct = 0.0;
};

class BangriTerrainValidator {
 public:
  // 블렌더(Blender 4.2.3)로 생성된 경주 20대 핵심 유적 카탈로그 및 3D 모델 메타데이터를 반환한다.
  static std::vector<GyeongjuHeritageSiteMeta> gyeongjuHeritageCatalog();

  // 블렌더 3D 유적 메쉬 번들(gyeongju_blender_heritage.s3dmesh) 파일 존재 여부를 확인한다.
  static bool hasBlenderHeritageBundle(QString* resolvedPath = nullptr);

  // 경주 북천(알천)·남천·발천 및 북천 구하도(옛 물길) 수로망 중심선과 폭 카탈로그를 반환한다.
  static std::vector<PaleoChannelCorridor> builtInPaleoChannels(const QString& workCrs);

  // DEM 표고/경사도, 3D 고분 봉분, 하천·구하도 수로망을 종합하여 방리 격자 도로의 실현 가능성을 4단계로 검증한다.
  static BangriValidationResult validate(const DemGrid& dem,
                                         const BangriGridResult& grid,
                                         const std::vector<SillaMound>& mounds);

  // 검증된 방리 격자(선택적) 및 블렌더로 제작된 경주 20대 핵심 유적의 실제 위치·실제 형태 3D 모델을 생성한다.
  // enabledHeritageIds 가 nullptr 이 아니면 해당 목록에 포함된 유적만 선택적으로 렌더링한다.
  static std::shared_ptr<SceneMesh> buildValidatedGridOverlay(
      const DemGrid& dem,
      const BangriGridResult& grid,
      const BangriValidationResult& validation,
      const std::vector<BangriAnchor>& anchors,
      const std::vector<BangriAnchorResidual>& residuals,
      const std::vector<SillaMound>& mounds = {},
      bool include3dCityArchitecture = true,
      bool includeRoadGrid = false,
      const std::vector<QString>* enabledHeritageIds = nullptr);

  // 사용자가 지형 위에서 클릭한 두 점(a, b)으로부터 3D 사용자 정의 격자 제원을 계산한다.
  static UserDrawnGrid3D makeGridFromTwoPoints(
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
      bool isPreview = false);

  // 사용자가 3D 맵 위에 직접 그은 선(UserDrawnLine3D)들과 격자(UserDrawnGrid3D)들을
  // 실제 지형 표고(DEM + 3D 고분 봉분)에 밀착시켜 3D 카메라와 100% 일체로 움직이는 입체 메쉬로 생성한다.
  static std::shared_ptr<SceneMesh> buildUserDrawingsOverlay(
      const DemGrid& dem,
      const std::vector<SillaMound>& mounds,
      const std::vector<UserDrawnLine3D>& lines,
      const std::vector<UserDrawnGrid3D>& grids);
};

}  // namespace strata3d
