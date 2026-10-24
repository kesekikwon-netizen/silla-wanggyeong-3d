#pragma once

#include <memory>
#include <vector>

#include "BangriGridEngine.h"
#include "DemGrid.h"
#include "SceneMesh.h"
#include "SillaMoundReconstructor.h"

namespace strata3d {

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
  // 경주 북천(알천)·남천·발천 및 북천 구하도(옛 물길) 수로망 중심선과 폭 카탈로그를 반환한다.
  static std::vector<PaleoChannelCorridor> builtInPaleoChannels(const QString& workCrs);

  // DEM 표고/경사도, 3D 고분 봉분, 하천·구하도 수로망을 종합하여 방리 격자 도로의 실현 가능성을 4단계로 검증한다.
  static BangriValidationResult validate(const DemGrid& dem,
                                         const BangriGridResult& grid,
                                         const std::vector<SillaMound>& mounds);

  // 검증된 방리 격자(선택적) 및 경주 핵심 유적(황룡사, 분황사, 동궁과 월지, 월성, 첨성대, 월정교, 계림, 향교, 박물관 등)의
  // 실제 위치·실제 형태 3D 모델을 생성한다.
  static std::shared_ptr<SceneMesh> buildValidatedGridOverlay(
      const DemGrid& dem,
      const BangriGridResult& grid,
      const BangriValidationResult& validation,
      const std::vector<BangriAnchor>& anchors,
      const std::vector<BangriAnchorResidual>& residuals,
      const std::vector<SillaMound>& mounds = {},
      bool include3dCityArchitecture = true,
      bool includeRoadGrid = false);
};

}  // namespace strata3d
