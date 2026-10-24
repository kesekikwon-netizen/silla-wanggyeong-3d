#include "KhsProvider.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QXmlStreamReader>
#include <cmath>

#include "CrsTransform.h"
#include "HttpFetch.h"

namespace strata3d {

bool parseKhsList(const QByteArray& xml, const QString& workCrs, const QString& cityContains,
                  std::vector<GeoFeature>* out, QString* errorOut) {
  const CrsTransformer tr(QStringLiteral("EPSG:4326"), workCrs);
  if (!tr.valid()) {
    if (errorOut) *errorOut = QStringLiteral("좌표계를 해석하지 못했습니다: %1").arg(workCrs);
    return false;
  }
  QXmlStreamReader r(xml);
  QMap<QString, QString> cur;
  bool inItem = false;
  int items = 0;
  while (!r.atEnd()) {
    r.readNext();
    if (r.isStartElement()) {
      if (r.name() == QLatin1String("item")) {
        inItem = true;
        cur.clear();
      } else if (inItem) {
        const QString k = r.name().toString();
        cur.insert(k, r.readElementText());
      }
    } else if (r.isEndElement() && r.name() == QLatin1String("item")) {
      inItem = false;
      ++items;
      if (!cityContains.isEmpty() && !cur.value(QStringLiteral("ccsiName")).contains(cityContains)) continue;
      double lon = cur.value(QStringLiteral("longitude")).toDouble(), lat = cur.value(QStringLiteral("latitude")).toDouble();
      if (lon < 100 || lat < 20) continue;  // 좌표 없음
      if (!tr.transform(&lon, &lat)) continue;
      GeoFeature f;
      f.kind = FeatureKind::HeritagePoint;
      f.source = QStringLiteral("KHS");
      f.id = QStringLiteral("khs:%1-%2-%3").arg(cur.value(QStringLiteral("ccbaKdcd")), cur.value(QStringLiteral("ccbaAsno")), cur.value(QStringLiteral("ccbaCtcd")));
      f.name = cur.value(QStringLiteral("ccbaMnm1"));
      f.category = cur.value(QStringLiteral("ccmaName"));
      f.point = {lon, lat};
      f.attrs = cur;
      out->push_back(std::move(f));
    }
  }
  if (r.hasError()) {
    if (errorOut) *errorOut = QStringLiteral("국가유산청 응답(XML)을 읽지 못했습니다: %1").arg(r.errorString());
    return false;
  }
  if (items == 0) {
    if (errorOut) *errorOut = QStringLiteral("국가유산청 응답에 항목이 없습니다.");
    return false;
  }
  return true;
}

bool loadKhsHeritage(const FeatureRequest& req, std::vector<GeoFeature>* out, QString* errorOut,
                     const QString& cityContains, const QString& provinceCode) {
  QByteArray xml;
  QString cachePath;
  if (!req.cacheDir.isEmpty()) {
    QDir().mkpath(req.cacheDir);
    cachePath = QDir(req.cacheDir).filePath(QStringLiteral("khs_list_%1.xml").arg(provinceCode));
    const QFileInfo fi(cachePath);
    if (fi.exists() && fi.lastModified().daysTo(QDateTime::currentDateTime()) < 7) {
      QFile f(cachePath);
      if (f.open(QIODevice::ReadOnly)) xml = f.readAll();
    }
  }
  if (xml.isEmpty()) {
    // 한 도(道) 전체가 3000건 안쪽이라 한 번에 받는다.
    const QString url = QStringLiteral("https://www.khs.go.kr/cha/SearchKindOpenapiList.do?ccbaCtcd=%1&pageUnit=3000&pageIndex=1").arg(provinceCode);
    const HttpResult r = httpGet(url, {QStringLiteral("User-Agent: strata-3d/0.1")}, 90);
    if (!r.ok) {
      if (errorOut) *errorOut = QStringLiteral("국가유산청 목록을 받지 못했습니다: %1").arg(r.error);
      return false;
    }
    xml = r.body;
  }
  std::vector<GeoFeature> all;
  if (!parseKhsList(xml, req.workCrs, cityContains, &all, errorOut)) return false;
  if (!cachePath.isEmpty() && !QFile::exists(cachePath)) {
    QFile f(cachePath);
    if (f.open(QIODevice::WriteOnly)) f.write(xml);
  }
  for (GeoFeature& f : all) {
    if (std::abs(f.point.x - req.centerX) <= req.halfExtentM && std::abs(f.point.y - req.centerY) <= req.halfExtentM)
      out->push_back(std::move(f));
  }
  return true;
}

}  // namespace strata3d
