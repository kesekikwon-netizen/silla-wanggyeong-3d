#include "LocalVectorProvider.h"

#include <gdal_priv.h>
#include <ogr_api.h>
#include <ogrsf_frmts.h>

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#include <cmath>
#include <memory>

#include "CrsTransform.h"

namespace strata3d {

QStringList strataHeritageShpPaths() {
  const QString local = qEnvironmentVariable("LOCALAPPDATA");
  if (local.isEmpty()) return {};
  const QString root = local + QString::fromUtf16(u"/ka-hgis/ka-hgis/주변유적/SHP");
  QStringList out;
  QDirIterator it(root, {QStringLiteral("*.shp")}, QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext()) out << it.next();
  return out;
}

namespace {

QString pickNameField(OGRFeatureDefn* defn) {
  // 명칭 계열 필드를 우선, 없으면 첫 문자열 필드.
  QString firstString;
  for (int i = 0; i < defn->GetFieldCount(); ++i) {
    OGRFieldDefn* fd = defn->GetFieldDefn(i);
    if (fd->GetType() != OFTString) continue;
    const QString n = QString::fromUtf8(fd->GetNameRef());
    if (firstString.isEmpty()) firstString = n;
    if (n.contains(QString::fromUtf16(u"명칭")) || n.contains(QString::fromUtf16(u"유산명")) || n.contains(QStringLiteral("NAME"), Qt::CaseInsensitive)) return n;
  }
  return firstString;
}

void addRing(const OGRLinearRing* ring, const CrsTransformer& toWork, GeoFeature* f) {
  if (!ring || ring->getNumPoints() < 4) return;
  for (int i = 0; i + 1 < ring->getNumPoints(); ++i) {  // 닫는 점 제외
    double x = ring->getX(i), y = ring->getY(i);
    if (toWork.transform(&x, &y)) f->ring.push_back({x, y});
  }
}

}  // namespace

bool loadLocalVectors(const FeatureRequest& req, const QStringList& paths, std::vector<GeoFeature>* out,
                      QStringList* skippedOut) {
  GDALAllRegister();
  QSet<QString> seen;
  for (const QString& path : paths) {
    char* openOpts[] = {const_cast<char*>("ENCODING=CP949"), nullptr};
    std::unique_ptr<GDALDataset> ds(GDALDataset::Open(path.toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, openOpts, nullptr));
    if (!ds || ds->GetLayerCount() < 1) {
      if (skippedOut) *skippedOut << path;
      continue;
    }
    OGRLayer* layer = ds->GetLayer(0);
    const OGRSpatialReference* srs = layer->GetSpatialRef();
    if (!srs) {
      if (skippedOut) *skippedOut << path;
      continue;
    }
    char* wkt = nullptr;
    srs->exportToWkt(&wkt);
    const QString layerCrs = QString::fromUtf8(wkt);
    CPLFree(wkt);
    const CrsTransformer toLayer(req.workCrs, layerCrs), toWork(layerCrs, req.workCrs);
    if (!toLayer.valid() || !toWork.valid()) {
      if (skippedOut) *skippedOut << path;
      continue;
    }
    // 창(작업 좌표계 네모)을 레이어 좌표계 경계 상자로
    double lx0 = 1e18, ly0 = 1e18, lx1 = -1e18, ly1 = -1e18;
    for (int i = -1; i <= 1; i += 2) {
      for (int j = -1; j <= 1; j += 2) {
        double x = req.centerX + i * req.halfExtentM, y = req.centerY + j * req.halfExtentM;
        toLayer.transform(&x, &y);
        lx0 = std::min(lx0, x); lx1 = std::max(lx1, x); ly0 = std::min(ly0, y); ly1 = std::max(ly1, y);
      }
    }
    layer->SetSpatialFilterRect(lx0, ly0, lx1, ly1);
    const QString category = QFileInfo(path).completeBaseName();
    const QString nameField = pickNameField(layer->GetLayerDefn());
    layer->ResetReading();
    while (OGRFeature* raw = layer->GetNextFeature()) {
      const std::unique_ptr<OGRFeature, void (*)(OGRFeature*)> feat(raw, [](OGRFeature* f) { OGRFeature::DestroyFeature(f); });
      const OGRGeometry* g = feat->GetGeometryRef();
      if (!g) continue;
      GeoFeature f;
      f.source = QStringLiteral("SHP");
      f.category = category;
      if (!nameField.isEmpty()) f.name = QString::fromUtf8(feat->GetFieldAsString(nameField.toUtf8().constData()));
      const OGRwkbGeometryType t = wkbFlatten(g->getGeometryType());
      if (t == wkbPolygon || t == wkbMultiPolygon) {
        const OGRPolygon* poly = t == wkbPolygon ? g->toPolygon() : g->toMultiPolygon()->getGeometryRef(0)->toPolygon();
        f.kind = FeatureKind::HeritageArea;
        addRing(poly->getExteriorRing(), toWork, &f);
        if (f.ring.size() < 3) continue;
        double sx = 0, sy = 0;
        for (const Pt& p : f.ring) { sx += p.x; sy += p.y; }
        f.point = {sx / double(f.ring.size()), sy / double(f.ring.size())};
      } else if (t == wkbPoint) {
        double x = g->toPoint()->getX(), y = g->toPoint()->getY();
        if (!toWork.transform(&x, &y)) continue;
        f.kind = FeatureKind::HeritagePoint;
        f.point = {x, y};
      } else {
        continue;
      }
      if (std::abs(f.point.x - req.centerX) > req.halfExtentM || std::abs(f.point.y - req.centerY) > req.halfExtentM) continue;
      const QString key = QStringLiteral("%1|%2|%3|%4").arg(category).arg(f.name).arg(f.point.x, 0, 'f', 1).arg(f.point.y, 0, 'f', 1);
      if (seen.contains(key)) continue;
      seen.insert(key);
      f.id = QStringLiteral("shp:%1/%2").arg(category).arg(qint64(feat->GetFID()));
      out->push_back(std::move(f));
    }
  }
  return true;
}

}  // namespace strata3d
