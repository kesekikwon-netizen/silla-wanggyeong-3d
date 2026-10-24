#pragma once

#include <QString>
#include <memory>
#include <vector>

#include "DemGrid.h"
#include "GeoFeature.h"
#include "SceneMesh.h"

namespace strata3d {

struct SillaMound {
  QString id;
  QString name;
  double x = 0.0;        // 작업 좌표계 미터 (EPSG:5187 / 5186)
  double y = 0.0;
  double radiusM = 18.0; // 등가 반경 R (m)
  double heightM = 7.0;  // 봉분 융기 높이 H (m, 4.0 ~ 23.0 m)
  bool isTwinMound = false;
};

class SillaMoundReconstructor {
 public:
  // 지물이 고분·능묘(왕릉, 고분, 총, 능, 대릉원, 쪽샘, 봉황대 등)인지 판정한다.
  static bool isTumulusFeature(const GeoFeature& f);

  // 다각형 면적 A 로부터 등가 반경 R = sqrt(A/pi) 및 봉분 높이 H = clamp(0.35*R, 4.0, 23.0)를 계산한다.
  static SillaMound moundFromFeature(const GeoFeature& f);

  // 경주(대릉원·황남대총·천마총·봉황대·쪽샘·노서동·노동동·오릉·서악동·금척리) 및 충주 누암리 실측 고분 카탈로그를 반환한다.
  static std::vector<SillaMound> builtInSillaMounds(const QString& workCrs);

  // 지물 목록(OSM/KHS/SHP)과 내장 실측 고분 카탈로그를 병합(중복 제거)하여 DEM 범위 안의 고분 목록을 만든다.
  static std::vector<SillaMound> collectMounds(const DemGrid& dem,
                                               const std::vector<GeoFeature>& features,
                                               bool includeBuiltInCatalog = true);

  // 임의 좌표 (x, y)에서의 3D 고분 봉분 융기 높이(+m, 지면 위 추가 고도)를 계산한다. 쌍분(표형분) 능선도 자연스럽게 합성된다.
  static float sampleMoundHeight(double x, double y, const std::vector<SillaMound>& mounds);

  // 고분 봉분 3D 반구형(Cosine-Paraboloid Dome) 메쉬만 단독으로 생성한다.
  static std::shared_ptr<SceneMesh> buildMoundsMesh(const DemGrid& dem,
                                                    const std::vector<SillaMound>& mounds);

  // 신라 고지형(Bare-Earth) 모드 + 3D 고분 봉분 융기 통합 SceneMesh 를 생성한다.
  // bareEarthMode 가 true 이면 현대 일반 건물 5,287동은 지면 위 0.25m 의 옅은 윤곽 기단으로만 낮추고
  // 유산 건물·유적 구역과 3D 고분 봉분만 입체로 부각한다.
  static std::shared_ptr<const SceneMesh> buildBareEarthAndMoundScene(
      const DemGrid& dem,
      const std::vector<GeoFeature>& features,
      bool bareEarthMode,
      bool reconstructMounds,
      std::vector<SillaMound>* outMounds = nullptr);
};

}  // namespace strata3d
