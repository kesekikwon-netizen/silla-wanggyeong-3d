#include "EvaluationJson.h"

#include <QJsonArray>

namespace strata3d {

namespace {
QJsonObject pt(const Pt& p) { return {{"x", p.x}, {"y", p.y}}; }
}  // namespace

QJsonObject evaluationToJson(const Evaluation& ev) {
  QJsonObject o;
  o["crs"] = ev.crs;
  o["site"] = pt(ev.site);
  const TerrainMetrics& t = ev.terrain;
  o["terrain"] = QJsonObject{{"elevation_m", t.elevationM}, {"slope_deg", t.slopeDeg}, {"aspect_deg", t.aspectDeg},
                             {"aspect_name", t.aspectName}, {"tpi_small_m", t.tpiSmallM}, {"tpi_large_m", t.tpiLargeM},
                             {"z_small", t.zSmall}, {"z_large", t.zLarge}, {"landform", t.landform}};
  QJsonArray radii;
  for (const RadiusResult& r : ev.radii) {
    QJsonArray items;
    for (const NearbyItem& n : r.heritage)
      items.append(QJsonObject{{"id", n.id}, {"name", n.name}, {"category", n.category}, {"source", n.source},
                               {"distance_m", n.distanceM}, {"bearing_deg", n.bearingDeg}, {"bearing_name", n.bearingName},
                               {"elevation_m", n.elevationM}, {"delta_elevation_m", n.deltaElevM}});
    radii.append(QJsonObject{{"radius_m", r.radiusM}, {"buffer_area_m2", r.bufferAreaM2}, {"buffer_max_reach_m", r.bufferMaxReachM},
                             {"buildings", r.buildings}, {"heritage", items}});
  }
  o["radii"] = radii;
  QJsonArray vis;
  for (const VisibilityItem& v : ev.visibility)
    vis.append(QJsonObject{{"name", v.name}, {"category", v.category}, {"distance_m", v.distanceM}, {"visible", v.visible}, {"blocked_at_m", v.blockedAtM}});
  o["visibility"] = vis;
  QJsonArray notes;
  for (const QString& n : ev.notes) notes.append(n);
  o["notes"] = notes;
  o["summary"] = ev.summary;
  return o;
}

}  // namespace strata3d
