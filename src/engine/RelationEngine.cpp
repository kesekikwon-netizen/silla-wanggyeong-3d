#include "RelationEngine.h"

#include <algorithm>
#include <cmath>

#include <ogr_geometry.h>

#include "HeritageNaming.h"

namespace strata3d {

const QStringList& zoneLayerNames() {
  static const QStringList k = {QStringLiteral("국가지정유산보호구역"), QStringLiteral("시도지정유산보호구역"),
                                QStringLiteral("현상변경허용기준"),     QStringLiteral("매장유산유존지역"),
                                QStringLiteral("발굴사업허가구역"),     QStringLiteral("지표사업허가구역")};
  return k;
}

namespace {

std::unique_ptr<OGRGeometry> geom(const Pt& p, const std::vector<Pt>& ring, const std::vector<std::vector<Pt>>& holes = {}) {
  if (ring.size() >= 3) {
    auto poly = std::make_unique<OGRPolygon>();
    auto add = [&poly](const std::vector<Pt>& r) {
      OGRLinearRing lr;
      for (const Pt& q : r) lr.addPoint(q.x, q.y);
      lr.closeRings();
      poly->addRing(&lr);
    };
    add(ring);
    for (const auto& h : holes) add(h);  // 구멍 안의 조사구역은 구역 「밖」이다
    return poly;
  }
  return std::make_unique<OGRPoint>(p.x, p.y);
}

}  // namespace

RelationTable computeRelations(const Archaeology3dEvaluator& ev, const Pt& site, const std::vector<Pt>& sitePolygon,
                               const RelationOptions& opt) {
  RelationTable t;
  t.site = site;
  t.sitePolygon = sitePolygon;
  t.radiusM = opt.radiusM;
  t.crs = ev.dem().crs;
  t.siteTerrain = ev.terrainMetricsAt(site);

  const Evaluation e = ev.evaluate(site, sitePolygon, {opt.radiusM});
  if (e.radii.empty()) return t;

  // 구역 판정
  const auto siteGeom = geom(site, sitePolygon);
  for (const GeoFeature& f : ev.features()) {
    if (f.ring.size() < 3 || !zoneLayerNames().contains(f.category)) continue;
    const auto zg = geom(f.point, f.ring, f.holes);
    ZoneHit z;
    z.layer = f.category;
    z.name = f.name;
    z.zone = f.attrs.value(QStringLiteral("구역명"));
    z.overlaps = siteGeom->Intersects(zg.get());
    z.distanceM = z.overlaps ? 0.0 : siteGeom->Distance(zg.get());
    if (z.overlaps || z.distanceM <= opt.radiusM) t.zones.push_back(z);
  }
  std::sort(t.zones.begin(), t.zones.end(), [](const ZoneHit& a, const ZoneHit& b) {
    const int la = zoneLayerNames().indexOf(a.layer), lb = zoneLayerNames().indexOf(b.layer);
    if (la != lb) return la < lb;
    if (a.overlaps != b.overlaps) return a.overlaps;
    return a.distanceM < b.distanceM;
  });

  for (const NearbyItem& it : e.radii.front().heritage) {
    if (zoneLayerNames().contains(it.category)) continue;  // 구역은 위에서 따로
    // 평가 엔진은 이름이 없으면 「(이름 없음)」을 채워 준다
    const bool unnamed = it.name.trimmed().isEmpty() || it.name == QStringLiteral("(이름 없음)");
    if (unnamed && !opt.includeUnnamed) {
      ++t.hiddenUnnamed;
      continue;
    }
    const QString shown = displayHeritageName(it.name);
    // 여러 조각으로 된 유산(대릉원 일원 등)은 가까운 순이므로 첫 조각이 최단거리다. 나머지는 조각 수로만 센다.
    // 분류가 다르면(국가유산청 점 ↔ Strata 면, 지정 종목 차이) 출처가 다른 자료라 따로 둔다.
    auto same = std::find_if(t.rows.begin(), t.rows.end(), [&](const RelationRow& q) {
      return q.item.name == shown && q.item.category == it.category;
    });
    if (same != t.rows.end()) {
      ++same->pieces;
      continue;
    }
    RelationRow r;
    r.item = it;
    r.item.name = shown;
    if (opt.periods) {
      if (const SpcaRecord* rec = findSpcaByName(*opt.periods, r.item.name)) r.period = rec->period;
    }
    t.rows.push_back(std::move(r));
  }

  const double px = ev.dem().pixelSize;
  t.notes << QStringLiteral("거리는 조사구역 경계에서 유산까지 최단거리, 방위는 조사구역 중심에서 본 방향입니다. 여러 조각인 유산은 가장 가까운 조각으로 한 줄입니다.");
  t.notes << QStringLiteral("높이차는 현재 지형(%1 m 격자 DEM) 기준이며 옛 지표면이 아닙니다.").arg(px, 0, 'f', 0);
  if (opt.periods) t.notes << QStringLiteral("시대는 국가유산 공간정보서비스 지정 목록(%1 받음)에서 이름이 같은 항목입니다. 비어 있으면 자료 없음입니다.")
                                  .arg(opt.periods->fetchedAt.toString(QStringLiteral("yyyy-MM-dd")));
  if (!t.zones.empty())
    t.notes << QStringLiteral("구역 판정은 Strata 가 국가유산청 GIS 통합인트라넷에서 받아 둔 SHP(보호구역·현상변경허용기준·매장유산유존지역·조사 허가구역)의 실제 경계로 계산했습니다.");
  if (t.hiddenUnnamed > 0) t.notes << QStringLiteral("이름 없는 지물 %1곳은 표에서 뺐습니다.").arg(t.hiddenUnnamed);
  return t;
}

}  // namespace strata3d
