#include "VworldKey.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUrl>
#include <QStandardPaths>

#include "HttpFetch.h"

namespace strata3d {

namespace {
QString readKeyFromIni(const QString& path) {
  if (path.isEmpty() || !QFile::exists(path)) return {};
  QSettings ini(path, QSettings::IniFormat);
  for (const char* k : {"VWorld/ApiKey", "vworld/apiKey", "apiKey"}) {
    const QString v = ini.value(QLatin1String(k)).toString().trimmed();
    if (!v.isEmpty()) return v;
  }
  return {};
}
}  // namespace

QString loadVworldApiKey() {
  const QString local = qEnvironmentVariable("LOCALAPPDATA");
  if (!local.isEmpty()) {
    for (const QString& p : {local + QStringLiteral("/ka-hgis/ka-hgis/ka-hgis-vworld.ini"),
                             local + QStringLiteral("/ka-hgis/ka-hgis-vworld.ini")}) {
      const QString k = readKeyFromIni(p);
      if (!k.isEmpty()) return k;
    }
  }
  {
    QSettings native(QStringLiteral("ka-hgis"), QStringLiteral("ka-hgis"));
    for (const char* k : {"VWorld/ApiKey", "vworld/apiKey"}) {
      const QString v = native.value(QLatin1String(k)).toString().trimmed();
      if (!v.isEmpty()) return v;
    }
  }
  const QString env = qEnvironmentVariable("VWORLD_API_KEY").trimmed();
  if (!env.isEmpty()) return env;
  const QString app = QCoreApplication::applicationDirPath();
  for (const QString& p : {app + QStringLiteral("/../config/secrets.ini"), app + QStringLiteral("/../../config/secrets.ini"),
                           app + QStringLiteral("/config/secrets.ini"), QDir::current().filePath(QStringLiteral("config/secrets.ini"))}) {
    const QString k = readKeyFromIni(p);
    if (!k.isEmpty()) return k;
  }
  return {};
}

bool checkVworldApiKey(const QString& key, QString* reasonOut) {
  if (key.isEmpty()) {
    if (reasonOut) *reasonOut = QStringLiteral("VWorld 키가 없습니다.");
    return false;
  }
  const QString url = QStringLiteral(
      "https://api.vworld.kr/req/data?service=data&version=2.0&request=GetFeature&format=json&size=1&page=1"
      "&geometry=false&attribute=true&crs=EPSG:4326&data=LT_C_ADSIGG_INFO&attrFilter=full_nm:like:%1&key=%2")
      .arg(QString::fromUtf8(QUrl::toPercentEncoding(QString::fromUtf16(u"경주"))), key);
  // VWorld 는 키를 Referer 머리글로 인증한다. 없으면 같은 키도 INCORRECT_KEY(hgis BasemapOps.cpp:1196, 2026-09-06 실측).
  const HttpResult r = httpGet(url, {QStringLiteral("Referer: https://localhost")}, 30);
  if (!r.ok) {
    if (reasonOut) *reasonOut = QStringLiteral("VWorld 서버에 연결하지 못했습니다: %1").arg(r.error.left(100));
    return false;
  }
  const QJsonObject resp = QJsonDocument::fromJson(r.body).object().value(QStringLiteral("response")).toObject();
  const QString status = resp.value(QStringLiteral("status")).toString();
  if (status == QLatin1String("OK") || status == QLatin1String("NOT_FOUND")) return true;
  if (reasonOut) {
    const QJsonObject err = resp.value(QStringLiteral("error")).toObject();
    *reasonOut = QStringLiteral("VWorld 키를 쓸 수 없습니다(%1): %2")
                     .arg(err.value(QStringLiteral("code")).toString(), err.value(QStringLiteral("text")).toString());
  }
  return false;
}

}  // namespace strata3d
