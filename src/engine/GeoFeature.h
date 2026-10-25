#pragma once

#include <QMap>
#include <QString>
#include <vector>

namespace strata3d {

struct Pt {
  double x = 0;  // 작업 좌표계 미터
  double y = 0;
};

enum class FeatureKind { Building, HeritageArea, HeritagePoint };

// 높이가 어디서 왔는지. Unknown 이면 추정치로 세우고 화면에서 구분해 보여야 한다.
enum class HeightSource { Unknown, FromLevels, Measured };

struct GeoFeature {
  FeatureKind kind = FeatureKind::Building;
  QString id;          // 예: "osm:way/123", "khs:11-0000310000000"
  QString name;
  QString category;    // 예: "국보", "사적", "building=house"
  QString source;      // "OSM" | "KHS" | "SHP"
  std::vector<Pt> ring;  // 외곽 고리(닫힘 점 제외). 점 지물은 비어 있다.
  std::vector<std::vector<Pt>> holes;  // 다각형 구멍(보호구역 등은 구멍이 있다; 구역 안/밖 판정에 쓴다)
  Pt point;              // 점 지물 위치 / 다각형 대표점
  double heightM = 0;
  HeightSource heightSource = HeightSource::Unknown;
  bool heritage = false;  // 건물이지만 유산 태그가 있음
  QMap<QString, QString> attrs;
};

// 같은 창(작업 좌표계 중심 ± 반경)을 모든 제공자가 쓴다.
struct FeatureRequest {
  double centerX = 0;
  double centerY = 0;
  double halfExtentM = 4000;
  QString workCrs = QStringLiteral("EPSG:5187");
  QString cacheDir;  // 비어 있으면 캐시하지 않는다
};

}  // namespace strata3d
