#include <QDir>
#include <QFileInfo>
#include <QtTest>

#include "DemProvider.h"
#include "RelationEngine.h"
#include "StrataProjectLayers.h"
#include "StrataSurvey.h"

using namespace strata3d;

// 실제 Strata 조사(사용자가 2026-10-25 Strata 에서 그리고 저장한 경주 조사)로 시험한다.
//   S3D_STRATA_SURVEY 가 있으면 그 .gpkg, 없으면 %USERPROFILE%\Documents\경주시\경주시.gpkg. 없으면 실패한다.
// 구역 판정 기대값은 엔진과 따로 2026-10-25 GDAL 명령줄로 계산했다:
//   ogr2ogr -explodecollections -t_srs EPSG:5187 <가져온자료\<분류>\heritage.gpkg>
//   ogrinfo -dialect SQLite "SELECT COUNT(*), SUM(ST_Intersects(geom,조사구역)), MIN(바깥 ST_Distance), SUM(ST_Distance<=1000) …"
// (docs/research/DEVLOG.md M1-6). 조사가 다시 저장돼 자료가 바뀌면 기대값도 다시 계산해야 한다.
class TestStrataProject : public QObject {
  Q_OBJECT
  QString gpkg_;
  StrataProject proj_;
  StrataSurvey survey_;

  struct Oracle {
    const char16_t* layer;
    int overlapping;   // 겹치는 조각 수
    double minOutside; // 겹치지 않는 조각 중 가장 가까운 거리(m)
    int within1000;    // 1000 m 안 조각 수(겹침 포함)
  };

 private slots:
  void initTestCase() {
    const QString root = qEnvironmentVariable("OSGEO4W_ROOT", QStringLiteral("C:/OSGeo4W"));
    if (qEnvironmentVariableIsEmpty("PROJ_DATA")) qputenv("PROJ_DATA", QDir(root).filePath("share/proj").toUtf8());
    if (qEnvironmentVariableIsEmpty("GDAL_DATA")) qputenv("GDAL_DATA", QDir(root).filePath("apps/gdal-dev/share/gdal").toUtf8());
    gpkg_ = qEnvironmentVariable("S3D_STRATA_SURVEY",
                                 QDir::home().filePath(QString::fromUtf16(u"Documents/경주시/경주시.gpkg")));
    QVERIFY2(QFileInfo::exists(gpkg_), qPrintable(QStringLiteral("Strata 조사 파일 없음: %1 (Strata 에서 조사구역을 그리고 저장)").arg(gpkg_)));
    QString err;
    QVERIFY2(readStrataProject(gpkg_, &proj_, &err), qPrintable(err));
    QVERIFY2(loadStrataSurvey(gpkg_, QStringLiteral("auto"), &survey_, &err), qPrintable(err));
  }

  void projectListsSurveyHeritageLayers() {
    QCOMPARE(proj_.projectFrom, QStringLiteral("gpkg:qgis_projects"));
    QCOMPARE(QDir::cleanPath(proj_.surveyDir), QDir::cleanPath(QFileInfo(gpkg_).absolutePath()));
    QStringList missing;
    const auto src = heritageSourcesOf(proj_, &missing);
    QVERIFY2(missing.isEmpty(), qPrintable(missing.join(QLatin1Char('\n'))));
    QCOMPARE(int(src.size()), 12);  // 저장된 조사의 「가져온자료」 12종
    QStringList cats;
    for (const auto& s : src) {
      cats << s.category;
      QVERIFY2(s.path.contains(QString::fromUtf16(u"가져온자료")), qPrintable(s.path));
      QCOMPARE(s.subLayer, QStringLiteral("heritage"));
    }
    for (const QString& z : zoneLayerNames()) QVERIFY2(cats.contains(z), qPrintable(z));
  }

  void zonesMatchIndependentGdalOracle() {
    QCOMPARE(int(survey_.surveyAreas.size()), 1);
    const auto& area = survey_.surveyAreas.front();
    Pt c;
    for (const Pt& p : area) { c.x += p.x / double(area.size()); c.y += p.y / double(area.size()); }

    FeatureRequest fr;
    fr.centerX = c.x;
    fr.centerY = c.y;
    fr.halfExtentM = 4000;
    fr.workCrs = survey_.workCrs;
    std::vector<GeoFeature> feats;
    QStringList skipped;
    QVERIFY(loadLocalVectorSources(fr, heritageSourcesOf(proj_), &feats, &skipped));
    QVERIFY2(skipped.isEmpty(), qPrintable(skipped.join(QLatin1Char('\n'))));

    DemGrid dem;
    DemRequest dr;
    dr.centerX = c.x;
    dr.centerY = c.y;
    dr.halfExtentM = 3000;
    dr.pixelSizeM = 30;
    dr.workCrs = survey_.workCrs;
    dr.cacheDir = QDir::temp().filePath(QStringLiteral("strata3d-test-project"));
    QString err;
    QVERIFY2(loadDemWindow(dr, &dem, &err), qPrintable(err));
    Archaeology3dEvaluator ev(dem, feats);
    RelationOptions o;
    o.radiusM = 1000;
    const RelationTable t = computeRelations(ev, c, area, o);

    const Oracle oracle[] = {
        {u"국가지정유산보호구역", 0, 324.299, 5}, {u"시도지정유산보호구역", 0, 23.708, 5}, {u"현상변경허용기준", 182, 24.177, 1670},
        // 발굴사업허가구역 512 기록 중 2건은 사업명·유산코드·도형이 같은 중복 등록이라 하나로 센다(GDAL COUNT(DISTINCT 사업명||도형) = 510)
        {u"매장유산유존지역", 2, 12.651, 144},     {u"발굴사업허가구역", 4, 6.311, 510},  {u"지표사업허가구역", 1, 6.023, 48},
    };
    for (const Oracle& want : oracle) {
      const QString layer = QString::fromUtf16(want.layer);
      int overlapping = 0, within = 0;
      double minOut = 1e18;
      for (const ZoneHit& z : t.zones) {
        if (z.layer != layer) continue;
        ++within;
        if (z.overlaps) ++overlapping;
        else minOut = std::min(minOut, z.distanceM);
      }
      qInfo().noquote() << QStringLiteral("%1: 겹침 %2 / 가장 가까운 바깥 %3 m / 1000 m 안 %4").arg(layer).arg(overlapping).arg(minOut, 0, 'f', 3).arg(within);
      QVERIFY2(overlapping == want.overlapping, qPrintable(QStringLiteral("%1 겹침 %2 ≠ %3").arg(layer).arg(overlapping).arg(want.overlapping)));
      QVERIFY2(std::abs(minOut - want.minOutside) < 0.01, qPrintable(QStringLiteral("%1 최단 %2 ≠ %3").arg(layer).arg(minOut).arg(want.minOutside)));
      QVERIFY2(within == want.within1000, qPrintable(QStringLiteral("%1 1000 m 안 %2 ≠ %3").arg(layer).arg(within).arg(want.within1000)));
    }
    // 현상변경허용기준: 겹치는 기준의 유산 91건, 구역 1구역·2-4구역·2-5구역·3구역(GDAL: COUNT(DISTINCT 국가유산명), GROUP BY 구역명)
    QStringList heritage, zones;
    for (const ZoneHit& z : t.zones) {
      if (z.layer != QString::fromUtf16(u"현상변경허용기준") || !z.overlaps) continue;
      if (!heritage.contains(z.name)) heritage << z.name;
      if (!zones.contains(z.zone)) zones << z.zone;
    }
    zones.sort();
    QCOMPARE(int(heritage.size()), 91);
    QCOMPARE(zones.join(QLatin1Char(',')), QString::fromUtf16(u"1구역,2-4구역,2-5구역,3구역"));
  }
};

QTEST_GUILESS_MAIN(TestStrataProject)
#include "test_strata_project.moc"
