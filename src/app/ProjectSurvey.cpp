#include "ProjectSurvey.h"

#include <qgscoordinatetransform.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsproviderregistry.h>
#include <qgsvectorlayer.h>

#include <algorithm>

#include "core/LayerOps.h"

namespace {

using strata3d::Pt;

std::vector<Pt> toPts(const QgsPolylineXY& line, const QgsCoordinateTransform& tr, bool closed) {
  std::vector<Pt> out;
  out.reserve(size_t(line.size()));
  for (const QgsPointXY& p : line) {
    QgsPointXY q = p;
    try {
      if (tr.isValid()) q = tr.transform(p);
    } catch (const QgsCsException&) {
      continue;
    }
    out.push_back({q.x(), q.y()});
  }
  // 닫힌 고리의 마지막 점(첫 점과 같음)은 엔진 규칙대로 뺀다
  if (closed && out.size() >= 2 && out.front().x == out.back().x && out.front().y == out.back().y) out.pop_back();
  return out;
}

}  // namespace

std::vector<strata3d::LocalVectorSource> heritageSourcesFromProject(QgsProject* project) {
  std::vector<strata3d::LocalVectorSource> out;
  if (!project) return out;
  for (QgsMapLayer* ml : project->mapLayers()) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl || vl->customProperty(QStringLiteral("ka_hgis/heritage_dataset")).toString().isEmpty()) continue;
    const QVariantMap parts = QgsProviderRegistry::instance()->decodeUri(vl->providerType(), vl->source());
    const QString path = parts.value(QStringLiteral("path")).toString();
    if (path.isEmpty()) continue;
    out.push_back({path, parts.value(QStringLiteral("layerName")).toString(), vl->name(),
                   vl->customProperty(QStringLiteral("ka_hgis/label_field")).toString()});
  }
  return out;
}

bool surveyFromProject(QgsProject* project, const QString& gpkgPath, const QString& workCrs, strata3d::StrataSurvey* out) {
  if (!project || !out) return false;
  strata3d::StrataSurvey s;
  s.path = gpkgPath;
  s.workCrs = workCrs;
  const QgsCoordinateReferenceSystem dest(workCrs);
  double minX = 1e18, minY = 1e18, maxX = -1e18, maxY = -1e18;
  auto grow = [&](const std::vector<Pt>& pts) {
    for (const Pt& q : pts) { minX = std::min(minX, q.x); maxX = std::max(maxX, q.x); minY = std::min(minY, q.y); maxY = std::max(maxY, q.y); }
  };
  bool anyLayer = false;
  auto read = [&](const char* key, std::vector<std::vector<Pt>>* rings, std::vector<Pt>* points) {
    QgsVectorLayer* vl = LayerOps::findByLayerKey(project, QString::fromLatin1(key));
    if (!vl || !vl->isValid()) return;
    anyLayer = true;
    if (s.layerCrs.isEmpty()) s.layerCrs = vl->crs().authid();
    const QgsCoordinateTransform tr(vl->crs(), dest, project->transformContext());
    int count = 0;
    QgsFeature f;
    QgsFeatureIterator it = vl->getFeatures();  // 편집 버퍼(저장 안 된 도형)도 함께 읽힌다
    while (it.nextFeature(f)) {
      const QgsGeometry g = f.geometry();
      if (g.isNull() || g.isEmpty()) continue;
      ++count;
      if (points) {
        for (const QgsPointXY& p : g.isMultipart() ? g.asMultiPoint() : QgsMultiPointXY{g.asPoint()}) {
          const auto one = toPts({p}, tr, false);
          if (!one.empty()) { points->push_back(one.front()); grow(one); }
        }
        continue;
      }
      if (g.type() == Qgis::GeometryType::Polygon) {
        const QgsMultiPolygonXY polys = g.isMultipart() ? g.asMultiPolygon() : QgsMultiPolygonXY{g.asPolygon()};
        for (const QgsPolygonXY& poly : polys) {  // 조각마다 따로(Strata 규칙)
          if (poly.isEmpty()) continue;
          auto ring = toPts(poly.front(), tr, true);
          if (ring.size() >= 3) { grow(ring); rings->push_back(std::move(ring)); }
        }
      } else if (g.type() == Qgis::GeometryType::Line) {
        const QgsMultiPolylineXY lines = g.isMultipart() ? g.asMultiPolyline() : QgsMultiPolylineXY{g.asPolyline()};
        for (const QgsPolylineXY& line : lines) {
          auto pts = toPts(line, tr, false);
          if (pts.size() >= 2) { grow(pts); rings->push_back(std::move(pts)); }
        }
      }
    }
    if (count > 0) s.presentLayers << QString::fromLatin1(key);
  };
  read("survey_area", &s.surveyAreas, nullptr);
  read("trial_trench", &s.trenches, nullptr);
  read("feature_poly", &s.featurePolys, nullptr);
  read("feature_line", &s.featureLines, nullptr);
  read("section_line", &s.sectionLines, nullptr);
  read("control_points", nullptr, &s.controlPoints);
  read("artifact_point", nullptr, &s.artifactPoints);
  if (!anyLayer) return false;
  if (maxX >= minX) {
    s.center = {0.5 * (minX + maxX), 0.5 * (minY + maxY)};
    s.halfExtentM = 0.5 * std::max(maxX - minX, maxY - minY);
  }
  *out = std::move(s);
  return true;
}
