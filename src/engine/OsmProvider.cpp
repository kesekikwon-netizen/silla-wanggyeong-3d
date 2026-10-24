#include "OsmProvider.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>
#include <algorithm>
#include <cmath>

#include "CrsTransform.h"
#include "HttpFetch.h"

namespace strata3d {

namespace {
constexpr double kMetersPerLevel = 3.0;

QString tag(const QJsonObject& tags, const char* k) { return tags.value(QLatin1String(k)).toString(); }

QStringList endpoints() {
  return {QStringLiteral("https://overpass-api.de/api/interpreter"),
          QStringLiteral("https://overpass.kumi.systems/api/interpreter")};
}
}  // namespace

bool parseOsmHeight(const QString& text, double* meters) {
  static const QRegularExpression re(QStringLiteral("^\\s*([0-9]+(?:[.,][0-9]+)?)\\s*(m|meter|meters)?\\s*$"));
  const auto m = re.match(text);
  if (!m.hasMatch()) return false;
  QString n = m.captured(1);
  n.replace(QLatin1Char(','), QLatin1Char('.'));
  *meters = n.toDouble();
  return *meters > 0;
}

bool parseOverpassJson(const QByteArray& json, const QString& workCrs, std::vector<GeoFeature>* out,
                       QString* errorOut) {
  QJsonParseError pe;
  const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
  if (doc.isNull() || !doc.isObject()) {
    if (errorOut) *errorOut = QStringLiteral("Overpass 응답을 읽지 못했습니다: %1 / %2").arg(pe.errorString(), QString::fromUtf8(json.left(120)));
    return false;
  }
  const CrsTransformer tr(QStringLiteral("EPSG:4326"), workCrs);
  if (!tr.valid()) {
    if (errorOut) *errorOut = QStringLiteral("좌표계를 해석하지 못했습니다: %1").arg(workCrs);
    return false;
  }
  for (const QJsonValue& ev : doc.object().value(QStringLiteral("elements")).toArray()) {
    const QJsonObject e = ev.toObject();
    if (e.value(QStringLiteral("type")).toString() != QLatin1String("way")) continue;
    const QJsonArray geom = e.value(QStringLiteral("geometry")).toArray();
    const QJsonObject tags = e.value(QStringLiteral("tags")).toObject();
    if (geom.size() < 4) continue;  // 닫힌 고리는 점 4개 이상(처음=끝)
    std::vector<Pt> ring;
    ring.reserve(size_t(geom.size()));
    bool good = true;
    for (const QJsonValue& gv : geom) {
      const QJsonObject g = gv.toObject();
      double x = g.value(QStringLiteral("lon")).toDouble(), y = g.value(QStringLiteral("lat")).toDouble();
      if (!tr.transform(&x, &y)) {
        good = false;
        break;
      }
      ring.push_back({x, y});
    }
    if (!good) continue;
    const bool closed = std::abs(ring.front().x - ring.back().x) < 1e-6 && std::abs(ring.front().y - ring.back().y) < 1e-6;
    if (!closed) continue;
    ring.pop_back();

    GeoFeature f;
    f.id = QStringLiteral("osm:way/%1").arg(qint64(e.value(QStringLiteral("id")).toDouble()));
    f.name = tag(tags, "name");
    f.source = QStringLiteral("OSM");
    f.ring = std::move(ring);
    double sx = 0, sy = 0;
    for (const Pt& p : f.ring) { sx += p.x; sy += p.y; }
    f.point = {sx / double(f.ring.size()), sy / double(f.ring.size())};
    const bool isBuilding = !tag(tags, "building").isEmpty();
    const bool isHistoric = !tag(tags, "historic").isEmpty() || !tag(tags, "heritage").isEmpty();
    for (auto it = tags.begin(); it != tags.end(); ++it) f.attrs.insert(it.key(), it.value().toString());
    if (isBuilding) {
      f.kind = FeatureKind::Building;
      f.category = QStringLiteral("building=%1").arg(tag(tags, "building"));
      f.heritage = isHistoric;
      double h = 0;
      if (parseOsmHeight(tag(tags, "height"), &h)) {
        f.heightM = h;
        f.heightSource = HeightSource::Measured;
      } else if (parseOsmHeight(tag(tags, "building:levels"), &h)) {
        f.heightM = h * kMetersPerLevel;
        f.heightSource = HeightSource::FromLevels;
      }
    } else if (isHistoric) {
      f.kind = FeatureKind::HeritageArea;
      f.category = QStringLiteral("historic=%1").arg(tag(tags, "historic"));
    } else {
      continue;
    }
    out->push_back(std::move(f));
  }
  return true;
}

bool loadOsmFeatures(const FeatureRequest& req, std::vector<GeoFeature>* out, QString* errorOut) {
  double s = 1e9, w = 1e9, n = -1e9, e = -1e9;
  const CrsTransformer toWgs(req.workCrs, QStringLiteral("EPSG:4326"));
  if (!toWgs.valid()) {
    if (errorOut) *errorOut = QStringLiteral("좌표계를 해석하지 못했습니다.");
    return false;
  }
  for (int i = -1; i <= 1; i += 2) {
    for (int j = -1; j <= 1; j += 2) {
      double x = req.centerX + i * req.halfExtentM, y = req.centerY + j * req.halfExtentM;
      toWgs.transform(&x, &y);
      w = std::min(w, x); e = std::max(e, x); s = std::min(s, y); n = std::max(n, y);
    }
  }
  const QString bbox = QStringLiteral("%1,%2,%3,%4").arg(s, 0, 'f', 5).arg(w, 0, 'f', 5).arg(n, 0, 'f', 5).arg(e, 0, 'f', 5);

  QByteArray json;
  QString cachePath;
  if (!req.cacheDir.isEmpty()) {
    QDir().mkpath(req.cacheDir);
    cachePath = QDir(req.cacheDir).filePath(QStringLiteral("osm_%1.json").arg(
        QString::fromLatin1(QCryptographicHash::hash(bbox.toUtf8(), QCryptographicHash::Sha1).toHex().left(16))));
    QFile f(cachePath);
    if (f.open(QIODevice::ReadOnly)) json = f.readAll();
  }
  if (json.isEmpty()) {
    const QString q = QStringLiteral("[out:json][timeout:120];(way[\"building\"](%1);way[\"historic\"](%1);way[\"heritage\"](%1););out tags geom;").arg(bbox);
    const QByteArray body = "data=" + QUrl::toPercentEncoding(q);
    QString lastError;
    for (const QString& ep : endpoints()) {
      const HttpResult r = httpPost(ep, body,
          {QStringLiteral("User-Agent: strata-3d/0.1 (archaeology GIS desktop app)"),
           QStringLiteral("Content-Type: application/x-www-form-urlencoded"), QStringLiteral("Accept: application/json")}, 150);
      if (r.ok && r.body.startsWith('{')) { json = r.body; break; }
      lastError = r.ok ? QString::fromUtf8(r.body.left(160)) : r.error;
    }
    if (json.isEmpty()) {
      if (errorOut) *errorOut = QStringLiteral("OSM(Overpass) 건물을 받지 못했습니다: %1").arg(lastError);
      return false;
    }
    if (!cachePath.isEmpty()) {
      QFile f(cachePath);
      if (f.open(QIODevice::WriteOnly)) f.write(json);
    }
  }
  return parseOverpassJson(json, req.workCrs, out, errorOut);
}

}  // namespace strata3d
