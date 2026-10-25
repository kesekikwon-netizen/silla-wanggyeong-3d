#pragma once

#include <QColor>
#include <QImage>
#include <QString>
#include <vector>

#include "DemGrid.h"
#include "GeoFeature.h"

namespace strata3d {

// 지형에 입히는 면(보호구역·현상변경허용기준·유존지역·허가구역·유산 면 …). 3D 로 세우지 않고
// 지형 텍스처 위에 반투명으로 칠한다(위성 영상과 같은 DEM 범위·같은 uv, 행 0 = 북).
struct DrapeStyle {
  QColor fill;          // 알파 = 비침 정도. 투명이면 칠하지 않는다
  QColor stroke;        // 경계선
  double strokeM = 2.0; // 경계선 굵기(지상 미터). 텍스처 한 칸보다 가늘면 한 칸으로
  int order = 0;        // 작을수록 먼저(아래) 칠한다
};

// 분류(GeoFeature::category = Strata 레이어 이름)별 모양. 모르는 분류의 면은 가는 먹선만.
DrapeStyle drapeStyleFor(const GeoFeature& f);

// 범례 이름: 구역 6분류는 그대로, 위치도 3종은 하나로, 그 밖의 유산 면은 「지정·등록 유산(면)」
QString drapeLegendName(const GeoFeature& f);
bool zoneLikeCategory(const QString& category);

// 범례 한 줄(드레이프에 실제로 칠한 분류만, 칠한 순서)
struct DrapeLegendRow {
  QString category;
  DrapeStyle style;
  int count = 0;
};

struct DrapeResult {
  QImage image;   // RGBA8888(비사전곱 알파), 행 0 = 북
  QImage detail;  // 가운데 고해상도 창(한 변 절반 detailHalfM). 비어 있으면 없음
  double detailHalfM = 0;
  std::vector<DrapeLegendRow> legend;
  int drawn = 0;  // 칠한 면 수
};

// 면 지물(ring 3점 이상, 구멍 포함)을 DEM 범위 텍스처에 칠한다. 점·건물은 건너뛴다.
// texels: 전체 창 한 변, detailHalfM > 0 이면 같은 가운데의 고해상도 창도 만든다(위성 근접 창과 같은 자리).
DrapeResult rasterizeDrape(const DemGrid& dem, const std::vector<GeoFeature>& features, int texels = 4096,
                           double detailHalfM = 3000.0, int detailTexels = 4096);

}  // namespace strata3d
