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
  std::vector<LocalVectorSource> sources;
  for (const QString& p : paths) sources.push_back({p, QString(), QFileInfo(p).completeBaseName(), QString()});
  return loadLocalVectorSources(req, sources, out, skippedOut);
}

bool loadLocalVectorSources(const FeatureRequest& req, const std::vector<LocalVectorSource>& sources,
                            std::vector<GeoFeature>* out, QStringList* skippedOut) {
  GDALAllRegister();
  QSet<QString> seen;
  for (const LocalVectorSource& src : sources) {
    const QString& path = src.path;
    // 국가유산청 SHP 는 CP949. GPKG(Strata 가 저장한 heritage.gpkg 등)는 UTF-8 이라 열기 옵션을 주지 않는다.
    const bool shp = path.endsWith(QStringLiteral(".shp"), Qt::CaseInsensitive);
    char* shpOpts[] = {const_cast<char*>("ENCODING=CP949"), nullptr};
    std::unique_ptr<GDALDataset> ds(GDALDataset::Open(path.toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr,
                                                      shp ? shpOpts : nullptr, nullptr));
    if (!ds || ds->GetLayerCount() < 1) {
      if (skippedOut) *skippedOut << path;
      continue;
    }
    OGRLayer* layer = src.subLayer.isEmpty() ? ds->GetLayer(0) : ds->GetLayerByName(src.subLayer.toUtf8().constData());
    if (!layer) {
      if (skippedOut) *skippedOut << path + QStringLiteral("|") + src.subLayer;
      continue;
    }
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
    const QString category = src.category.isEmpty() ? QFileInfo(path).completeBaseName() : src.category;
    QString nameField = src.nameField;
    if (nameField.isEmpty() || layer->GetLayerDefn()->GetFieldIndex(nameField.toUtf8().constData()) < 0)
      nameField = pickNameField(layer->GetLayerDefn());
    const int zoneField = layer->GetLayerDefn()->GetFieldIndex(QString::fromUtf16(u"구역명").toUtf8().constData());
    const int heritageField = zoneField >= 0 ? layer->GetLayerDefn()->GetFieldIndex(QString::fromUtf16(u"국가유산명").toUtf8().constData()) : -1;
    layer->ResetReading();
    while (OGRFeature* raw = layer->GetNextFeature()) {
      const std::unique_ptr<OGRFeature, void (*)(OGRFeature*)> feat(raw, [](OGRFeature* f) { OGRFeature::DestroyFeature(f); });
      const OGRGeometry* g = feat->GetGeometryRef();
      if (!g) continue;
      GeoFeature base;
      base.source = QStringLiteral("SHP");
      base.category = category;
      if (!nameField.isEmpty()) base.name = QString::fromUtf8(feat->GetFieldAsString(nameField.toUtf8().constData()));
      // 현상변경허용기준은 같은 구역 도형이 유산마다 따로 등록된다(경주 조사에서 확인: 도형 4개 = 유산 91건 × 구역).
      // 유산 이름과 구역명을 함께 가져야 서로 다른 기준을 하나로 합치지 않는다.
      if (zoneField >= 0) base.attrs.insert(QStringLiteral("구역명"), QString::fromUtf8(feat->GetFieldAsString(zoneField)));
      if (heritageField >= 0 && nameField != QString::fromUtf16(u"국가유산명")) {
        const QString h = QString::fromUtf8(feat->GetFieldAsString(heritageField));
        if (!h.isEmpty()) base.name = h;
      }
      const OGRwkbGeometryType t = wkbFlatten(g->getGeometryType());
      // 다중 다각형(보호구역이 여러 조각인 경우 등)은 조각마다 지물 하나로 담는다. 첫 조각만 쓰면 나머지 조각과의 겹침을 놓친다.
      std::vector<GeoFeature> parts;
      if (t == wkbPolygon || t == wkbMultiPolygon) {
        const int n = t == wkbPolygon ? 1 : g->toMultiPolygon()->getNumGeometries();
        for (int k = 0; k < n; ++k) {
          const OGRPolygon* poly = t == wkbPolygon ? g->toPolygon() : g->toMultiPolygon()->getGeometryRef(k)->toPolygon();
          GeoFeature f = base;
          f.kind = FeatureKind::HeritageArea;
          addRing(poly->getExteriorRing(), toWork, &f);
          if (f.ring.size() < 3) continue;
          for (int h = 0; h < poly->getNumInteriorRings(); ++h) {
            GeoFeature hole;
            addRing(poly->getInteriorRing(h), toWork, &hole);
            if (hole.ring.size() >= 3) f.holes.push_back(std::move(hole.ring));
          }
          double sx = 0, sy = 0;
          for (const Pt& p : f.ring) { sx += p.x; sy += p.y; }
          f.point = {sx / double(f.ring.size()), sy / double(f.ring.size())};
          f.id = n > 1 ? QStringLiteral("shp:%1/%2.%3").arg(category).arg(qint64(feat->GetFID())).arg(k)
                       : QStringLiteral("shp:%1/%2").arg(category).arg(qint64(feat->GetFID()));
          parts.push_back(std::move(f));
        }
      } else if (t == wkbPoint) {
        double x = g->toPoint()->getX(), y = g->toPoint()->getY();
        if (!toWork.transform(&x, &y)) continue;
        GeoFeature f = base;
        f.kind = FeatureKind::HeritagePoint;
        f.point = {x, y};
        f.id = QStringLiteral("shp:%1/%2").arg(category).arg(qint64(feat->GetFID()));
        parts.push_back(std::move(f));
      }
      for (GeoFeature& f : parts) {
        // 다각형은 외곽 상자가 창과 겹치면 담는다(중심이 창 밖인 큰 보호구역·현상변경허용기준을 놓치지 않게)
        double x0 = f.point.x, x1 = f.point.x, y0 = f.point.y, y1 = f.point.y;
        for (const Pt& q : f.ring) { x0 = std::min(x0, q.x); x1 = std::max(x1, q.x); y0 = std::min(y0, q.y); y1 = std::max(y1, q.y); }
        if (x1 < req.centerX - req.halfExtentM || x0 > req.centerX + req.halfExtentM || y1 < req.centerY - req.halfExtentM ||
            y0 > req.centerY + req.halfExtentM)
          continue;
        const QString key = QStringLiteral("%1|%2|%3|%4|%5").arg(category, f.name, f.attrs.value(QStringLiteral("구역명"))).arg(f.point.x, 0, 'f', 1).arg(f.point.y, 0, 'f', 1);
        if (seen.contains(key)) continue;
        seen.insert(key);
        out->push_back(std::move(f));
      }
    }
  }
  return true;
}

}  // namespace strata3d
