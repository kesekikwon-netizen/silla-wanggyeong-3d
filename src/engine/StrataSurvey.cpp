#include "StrataSurvey.h"

#include <gdal_priv.h>
#include <ogrsf_frmts.h>

#include <QFileInfo>
#include <algorithm>
#include <memory>

#include "CrsTransform.h"

namespace strata3d {

namespace {

void addRing(const OGRLineString* ls, const CrsTransformer& tr, bool dropClosing, std::vector<Pt>* out) {
  if (!ls) return;
  int n = ls->getNumPoints();
  if (dropClosing && n > 1 && ls->getX(0) == ls->getX(n - 1) && ls->getY(0) == ls->getY(n - 1)) --n;
  for (int i = 0; i < n; ++i) {
    double x = ls->getX(i), y = ls->getY(i);
    if (tr.transform(&x, &y)) out->push_back({x, y});
  }
}

}  // namespace

bool loadStrataSurvey(const QString& gpkgPath, const QString& workCrsIn, StrataSurvey* out, QString* errorOut) {
  GDALAllRegister();
  if (!QFileInfo::exists(gpkgPath)) {
    if (errorOut) *errorOut = QStringLiteral("조사 파일이 없습니다: %1").arg(gpkgPath);
    return false;
  }
  char* openOpts[] = {const_cast<char*>("IMMUTABLE=YES"), nullptr};  // 읽기 전용, 잠금·-wal 파일을 만들지 않는다
  std::unique_ptr<GDALDataset> ds(GDALDataset::Open(gpkgPath.toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, openOpts, nullptr));
  if (!ds) {
    if (errorOut) *errorOut = QStringLiteral("조사 파일을 열지 못했습니다: %1").arg(gpkgPath);
    return false;
  }
  StrataSurvey s;
  s.path = gpkgPath;
  QString workCrs = workCrsIn;
  if (workCrs == QLatin1String("auto")) {
    workCrs = QStringLiteral("EPSG:5187");
    for (int i = 0; i < ds->GetLayerCount(); ++i) {
      const OGRSpatialReference* srs = ds->GetLayer(i)->GetSpatialRef();
      const char* code = srs ? srs->GetAuthorityCode(nullptr) : nullptr;
      if (code && (QByteArray(code) == "5186" || QByteArray(code) == "5187")) {
        workCrs = QStringLiteral("EPSG:") + QString::fromLatin1(code);
        break;
      }
    }
  }
  s.workCrs = workCrs;
  double minX = 1e18, minY = 1e18, maxX = -1e18, maxY = -1e18;
  auto grow = [&](const Pt& p) { minX = std::min(minX, p.x); maxX = std::max(maxX, p.x); minY = std::min(minY, p.y); maxY = std::max(maxY, p.y); };

  auto readLayer = [&](const char* key, auto&& onGeometry) {
    OGRLayer* layer = ds->GetLayerByName(key);
    if (!layer) return;
    const OGRSpatialReference* srs = layer->GetSpatialRef();
    if (!srs) return;
    char* wkt = nullptr;
    srs->exportToWkt(&wkt);
    const QString layerCrs = QString::fromUtf8(wkt);
    CPLFree(wkt);
    if (s.layerCrs.isEmpty()) s.layerCrs = QString::fromUtf8(srs->GetName() ? srs->GetName() : "");
    const CrsTransformer tr(layerCrs, workCrs);
    if (!tr.valid()) return;
    int count = 0;
    layer->ResetReading();
    while (OGRFeature* raw = layer->GetNextFeature()) {
      const std::unique_ptr<OGRFeature, void (*)(OGRFeature*)> f(raw, [](OGRFeature* x) { OGRFeature::DestroyFeature(x); });
      if (const OGRGeometry* g = f->GetGeometryRef()) {
        onGeometry(g, tr);
        ++count;
      }
    }
    if (count > 0) s.presentLayers << QString::fromLatin1(key);
  };

  auto polygons = [&](std::vector<std::vector<Pt>>* dst) {
    return [&, dst](const OGRGeometry* g, const CrsTransformer& tr) {
      const OGRwkbGeometryType t = wkbFlatten(g->getGeometryType());
      auto addPoly = [&](const OGRPolygon* p) {
        std::vector<Pt> ring;
        addRing(p->getExteriorRing(), tr, true, &ring);
        if (ring.size() >= 3) {
          for (const Pt& q : ring) grow(q);
          dst->push_back(std::move(ring));
        }
      };
      if (t == wkbPolygon) addPoly(g->toPolygon());
      else if (t == wkbMultiPolygon)
        for (const OGRGeometry* part : *g->toMultiPolygon()) addPoly(part->toPolygon());  // 조각마다 따로(Strata 규칙)
      else if (t == wkbLineString) {
        std::vector<Pt> line;
        addRing(g->toLineString(), tr, false, &line);
        for (const Pt& q : line) grow(q);
        if (line.size() >= 2) dst->push_back(std::move(line));
      }
    };
  };
  auto lines = [&](std::vector<std::vector<Pt>>* dst) {
    return [&, dst](const OGRGeometry* g, const CrsTransformer& tr) {
      if (wkbFlatten(g->getGeometryType()) != wkbLineString) return;
      std::vector<Pt> line;
      addRing(g->toLineString(), tr, false, &line);
      for (const Pt& q : line) grow(q);
      if (line.size() >= 2) dst->push_back(std::move(line));
    };
  };
  auto points = [&](std::vector<Pt>* dst) {
    return [&, dst](const OGRGeometry* g, const CrsTransformer& tr) {
      if (wkbFlatten(g->getGeometryType()) != wkbPoint) return;
      double x = g->toPoint()->getX(), y = g->toPoint()->getY();
      if (!tr.transform(&x, &y)) return;
      grow({x, y});
      dst->push_back({x, y});
    };
  };

  readLayer("survey_area", polygons(&s.surveyAreas));
  readLayer("trial_trench", polygons(&s.trenches));
  readLayer("feature_poly", polygons(&s.featurePolys));
  readLayer("feature_line", lines(&s.featureLines));
  readLayer("section_line", lines(&s.sectionLines));
  readLayer("control_points", points(&s.controlPoints));
  readLayer("artifact_point", points(&s.artifactPoints));
  if (s.presentLayers.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("도메인 레이어에 지물이 없습니다(조사구역 등을 먼저 그려 주세요).");
    return false;
  }
  s.center = {(minX + maxX) * 0.5, (minY + maxY) * 0.5};
  s.halfExtentM = std::max(maxX - minX, maxY - minY) * 0.5;
  *out = std::move(s);
  return true;
}

}  // namespace strata3d
