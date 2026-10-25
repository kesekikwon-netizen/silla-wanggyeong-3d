#pragma once

#include <QString>
#include <QStringList>
#include <vector>

#include "Archaeology3dEvaluator.h"
#include "EvidenceTier.h"
#include "KhsSpcaProvider.h"

namespace strata3d {

// 내 조사구역(또는 트렌치) 하나와 주변 유산 하나하나의 관계(docs/BLUEPRINT.md 5.5).
// 거리·방위·높이차는 Archaeology3dEvaluator 를 그대로 쓰고, 여기서는 시대와 구역 판정을 붙인다.
// 추정은 넣지 않는다(2026-10-25 사용자 결정, 설계도 v3 10장): 시선(가시권), 방리 칸(추정 격자),
// 지형 분류(30 m 지형으로 가른 참고값)는 표에 없다.
struct RelationOptions {
  double radiusM = 1000.0;
  bool includeUnnamed = false;  // 이름 없는 지물(대개 OSM)을 표에 넣을지
  const SpcaList* periods = nullptr;     // 국가유산 공간정보 지정 목록(시대). 없으면 시대 열은 비운다
};

struct RelationRow {
  NearbyItem item;              // 거리(조사구역 경계 기준)·방위·높이차
  QString period;               // 시대(국가유산 공간정보). 비면 자료 없음
  int pieces = 1;               // 같은 이름·분류의 도형 조각 수(여러 조각 유산은 가장 가까운 조각 한 줄로 합친다)
  EvidenceTier positionTier = EvidenceTier::Measured;  // 위치 자료의 등급
};

// Strata 가 국가유산청 GIS 통합인트라넷에서 받아 둔 구역 SHP(파일 이름 = 분류)와 조사구역의 겹침.
// 보호구역·현상변경허용기준·매장유산유존지역은 반경 가정 대신 실제 폴리곤으로 판정한다(docs/STRATA_REUSE.md).
struct ZoneHit {
  QString layer;          // SHP 파일 이름, 예: "국가지정유산보호구역"
  QString name;           // 구역 이름 또는 그 기준의 유산 이름(없으면 빈 값)
  QString zone;           // 현상변경허용기준의 구역명(예: 「2-4구역」), 없으면 빈 값
  bool overlaps = false;  // 조사구역과 겹치거나 안에 듦
  double distanceM = 0;   // 겹치지 않으면 경계 사이 최단거리
};

// 구역으로 다루는 SHP 분류(관계 표의 유산 행에서는 뺀다)
const QStringList& zoneLayerNames();

struct RelationTable {
  Pt site;
  std::vector<Pt> sitePolygon;
  TerrainMetrics siteTerrain;
  double radiusM = 0;
  std::vector<RelationRow> rows;  // 가까운 순
  int hiddenUnnamed = 0;          // 표에서 뺀 이름 없는 지물 수
  std::vector<ZoneHit> zones;     // 구역 판정(분류 순, 같은 분류 안에서는 겹침 먼저·가까운 순)
  QString crs;
  QStringList notes;              // 해상도·자료 한계(표 아래에 그대로 보인다)
};

RelationTable computeRelations(const Archaeology3dEvaluator& ev, const Pt& site, const std::vector<Pt>& sitePolygon,
                               const RelationOptions& opt = {});

}  // namespace strata3d
