#include <QDir>
#include <QtTest>

#include "CrossCheck.h"
#include "KhsSpcaProvider.h"

using namespace strata3d;

// 출처 대조 시험. 실제 국가유산 공간정보서비스(spca.do)를 받는다(막히면 실패).
// 기대 거리는 2026-10-25 GDAL gdaltransform 으로 따로 계산한 값이다(docs/research/METHODS.md 3장).
class TestCrossCheck : public QObject {
  Q_OBJECT
  SpcaList sites_;

 private slots:
  void initTestCase() {
    const QString root = qEnvironmentVariable("OSGEO4W_ROOT", QStringLiteral("C:/OSGeo4W"));
    if (qEnvironmentVariableIsEmpty("PROJ_DATA")) qputenv("PROJ_DATA", QDir(root).filePath("share/proj").toUtf8());
    if (qEnvironmentVariableIsEmpty("GDAL_DATA"))
      qputenv("GDAL_DATA", QDir(root).filePath("apps/gdal-dev/share/gdal").toUtf8());
    QString err;
    QVERIFY2(loadSpca(QStringLiteral("13"), QString(), &sites_, &err, QStringLiteral("경주")), qPrintable(err));
  }

  void parsesOffline() {
    const QByteArray xml =
        "<?xml version=\"1.0\"?><ns2:response xmlns:ns2=\"http://gis-heritage.go.kr/\"><spca><ccbaMnm>경주 월성</ccbaMnm>"
        "<ccceName>신라시대</ccceName><ccmaName>사적</ccmaName><cnX>1155659.28</cnX><cnY>1760936.56</cnY>"
        "<vlocName>경상북도 경주시</vlocName></spca></ns2:response>";
    std::vector<SpcaRecord> r;
    QVERIFY(parseSpca(xml, &r));
    QCOMPARE(int(r.size()), 1);
    QCOMPARE(r[0].period, QStringLiteral("신라시대"));
    QCOMPARE(r[0].x5179, 1155659.28);
  }

  void gyeongjuHistoricSitesHavePeriods() {
    QVERIFY2(sites_.records.size() >= 60, qPrintable(QString::number(sites_.records.size())));  // 2026-10-25: 74건
    const SpcaRecord* d = findSpcaByName(sites_, QStringLiteral("경주 동궁과 월지"));
    QVERIFY(d);
    QCOMPARE(d->period, QStringLiteral("통일신라시대"));
    const SpcaRecord* h = findSpcaByName(sites_, QStringLiteral("경주황룡사지"));  // 공백 무시
    QVERIFY(h);
    QCOMPARE(h->period, QStringLiteral("신라시대"));
    QVERIFY(sites_.fetchedAt.isValid());
  }

  // 국가유산청 점(경도·위도)과 공간정보 중심점(EPSG:5179) 대조
  void donggungAgreesWithinNearThreshold() {
    const SpcaRecord* d = findSpcaByName(sites_, QStringLiteral("경주 동궁과 월지"));
    QVERIFY(d);
    const PositionCheck c = comparePositions(QStringLiteral("국가유산청"), 129.226486463902, 35.83471758976, QStringLiteral("EPSG:4326"),
                                             QStringLiteral("국가유산 공간정보"), d->x5179, d->y5179, QStringLiteral("EPSG:5179"));
    QVERIFY2(c.error.isEmpty(), qPrintable(c.error));
    QVERIFY2(std::abs(c.distanceM - 11.98) < 0.5, qPrintable(QString::number(c.distanceM)));
    QCOMPARE(int(c.level), int(Agreement::Near));
  }

  void hwangnyongsaNeedsChecking() {
    const SpcaRecord* h = findSpcaByName(sites_, QStringLiteral("경주 황룡사지"));
    QVERIFY(h);
    const PositionCheck c = comparePositions(QStringLiteral("국가유산청"), 129.2307733, 35.83572849, QStringLiteral("EPSG:4326"),
                                             QStringLiteral("국가유산 공간정보"), h->x5179, h->y5179, QStringLiteral("EPSG:5179"));
    QVERIFY2(std::abs(c.distanceM - 278.1) < 1.0, qPrintable(QString::number(c.distanceM)));
    QCOMPARE(int(c.level), int(Agreement::Mismatch));
    QCOMPARE(agreementLabel(c.level), QStringLiteral("확인 필요"));
  }

  void thresholdsAreUserSettings() {
    CrossCheckThresholds loose;
    loose.matchM = 15.0;
    const PositionCheck c = comparePositions(QStringLiteral("a"), 1155935.65, 1761177.14, QStringLiteral("EPSG:5179"), QStringLiteral("b"),
                                             1155942.94, 1761186.65, QStringLiteral("EPSG:5179"), loose);
    QCOMPARE(int(c.level), int(Agreement::Match));
  }
};

QTEST_GUILESS_MAIN(TestCrossCheck)
#include "test_cross_check.moc"
