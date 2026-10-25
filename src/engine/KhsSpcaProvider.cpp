#include "KhsSpcaProvider.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>

#include "HttpFetch.h"

namespace strata3d {
namespace {

QString squeeze(QString s) {
  s.remove(QLatin1Char(' '));
  return s;
}

}  // namespace

bool parseSpca(const QByteArray& xml, std::vector<SpcaRecord>* out, QString* errorOut) {
  QXmlStreamReader r(xml);
  SpcaRecord cur;
  bool inItem = false;
  while (!r.atEnd()) {
    r.readNext();
    if (r.isStartElement()) {
      const QString tag = r.name().toString();  // readElementText 뒤에도 쓰므로 복사
      if (tag == QLatin1String("spca")) {
        cur = SpcaRecord();
        inItem = true;
      } else if (inItem) {
        const QString text = r.readElementText().trimmed();
        if (tag == QLatin1String("ccbaMnm")) cur.name = text;
        else if (tag == QLatin1String("ccmaName")) cur.kind = text;
        else if (tag == QLatin1String("ccceName")) cur.period = text;
        else if (tag == QLatin1String("ctgrname")) cur.classification = text;
        else if (tag == QLatin1String("ccbaAsdt")) cur.designatedOn = text;
        else if (tag == QLatin1String("vlocName")) cur.location = text;
        else if (tag == QLatin1String("cnX")) cur.x5179 = text.toDouble();
        else if (tag == QLatin1String("cnY")) cur.y5179 = text.toDouble();
      }
    } else if (r.isEndElement() && r.name() == QLatin1String("spca")) {
      if (!cur.name.isEmpty()) out->push_back(cur);
      inItem = false;
    }
  }
  if (r.hasError()) {
    if (errorOut) *errorOut = QStringLiteral("국가유산 공간정보 목록을 읽지 못했습니다: %1").arg(r.errorString());
    return false;
  }
  return true;
}

bool loadSpca(const QString& kindCode, const QString& cacheDir, SpcaList* out, QString* errorOut,
              const QString& locationContains) {
  out->kindCode = kindCode;
  out->sourceUrl = QStringLiteral("https://gis-heritage.go.kr/openapi/xmlService/spca.do?ccbaKdcd=%1").arg(kindCode);
  QString cachePath;
  QFileInfo cacheInfo;
  if (!cacheDir.isEmpty()) {
    QDir().mkpath(cacheDir);
    cachePath = QDir(cacheDir).filePath(QStringLiteral("spca_%1.xml").arg(kindCode));
    cacheInfo = QFileInfo(cachePath);
  }
  auto readCache = [&](std::vector<SpcaRecord>* recs) {
    QFile f(cachePath);
    if (cachePath.isEmpty() || !f.open(QIODevice::ReadOnly)) return false;
    recs->clear();
    if (!parseSpca(f.readAll(), recs) || recs->empty()) return false;
    out->fetchedAt = cacheInfo.lastModified();
    return true;
  };
  std::vector<SpcaRecord> all;
  // 1) 7일 안의 캐시가 읽히면 그대로 쓴다
  bool have = cacheInfo.exists() && cacheInfo.lastModified().daysTo(QDateTime::currentDateTime()) < 7 && readCache(&all);
  // 2) 아니면 받는다. 받으면 캐시를 늘 새로 쓴다
  if (!have) {
    const HttpResult r = httpGet(out->sourceUrl, {QStringLiteral("User-Agent: strata-3d/0.1")}, 30);
    QString perr;
    all.clear();
    if (r.ok && !r.body.isEmpty() && parseSpca(r.body, &all, &perr) && !all.empty()) {
      have = true;
      out->fetchedAt = QDateTime::currentDateTime();
      if (!cachePath.isEmpty()) {
        QFile f(cachePath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(r.body);
      }
    } else if (readCache(&all)) {
      have = true;  // 3) 네트워크가 안 되면 오래된 캐시라도 쓴다(받은 날짜가 그대로 보인다)
    } else {
      if (errorOut) *errorOut = QStringLiteral("국가유산 공간정보 목록을 받지 못했습니다(종목 %1): %2").arg(kindCode, perr.isEmpty() ? r.error : perr);
      return false;
    }
  }
  for (SpcaRecord& rec : all) {
    if (locationContains.isEmpty() || rec.location.contains(locationContains)) out->records.push_back(std::move(rec));
  }
  return true;
}

const SpcaRecord* findSpcaByName(const SpcaList& list, const QString& name) {
  const QString key = squeeze(name);
  for (const SpcaRecord& r : list.records) {
    if (squeeze(r.name) == key) return &r;
  }
  return nullptr;
}

}  // namespace strata3d
