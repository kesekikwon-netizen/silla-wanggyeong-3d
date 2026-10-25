#include <QDir>
#include <QtTest>

#include "DemProvider.h"
#include "KhsProvider.h"
#include "RelationEngine.h"
#include "StrataSurvey.h"

using namespace strata3d;

// 관계 표 시험: 경주 연습용 조사(합성) 조사구역 ↔ 국가유산청 지정유산.
// 실제 Copernicus DEM·국가유산청 목록·국가유산 공간정보 목록을 받는다(막히면 실패).
// 기대값은 2026-10-25 gdaltransform 으로 국가유산청 점을 EPSG:5187 로 옮겨 조사구역 사각형
// [221343,221443]×[359853.6,359933.6] 까지 따로 계산한 값이다(docs/research/DEVLOG.md M1-4).
class TestRelations : public QObject {
  Q_OBJECT
  DemGrid dem_;
  std::vector<GeoFeature> khs_;
  SpcaList periods_;
  StrataSurvey survey_;
  QString cache_;

  const RelationRow* row(const RelationTable& t, const QString& name) {
    for (const auto& r : t.rows)
      if (r.item.name == name) return &r;
    return nullptr;
  }

 private slots:
  void initTestCase() {
    const QString root = qEnvironmentVariable("OSGEO4W_ROOT", QStringLiteral("C:/OSGeo4W"));
    if (qEnvironmentVariableIsEmpty("PROJ_DATA")) qputenv("PROJ_DATA", QDir(root).filePath("share/proj").toUtf8());
    if (qEnvironmentVariableIsEmpty("GDAL_DATA"))
      qputenv("GDAL_DATA", QDir(root).filePath("apps/gdal-dev/share/gdal").toUtf8());
    cache_ = QDir::temp().filePath(QStringLiteral("strata3d-test-relations"));
    QString err;
    QVERIFY2(loadStrataSurvey(QStringLiteral(S3D_SOURCE_DIR "/data/samples/gyeongju_practice/gyeongju_practice_synthetic.gpkg"),
                              QStringLiteral("EPSG:5187"), &survey_, &err),
             qPrintable(err));
    QCOMPARE(int(survey_.surveyAreas.size()), 1);
    QCOMPARE(int(survey_.trenches.size()), 4);

    DemRequest dr;
    dr.centerX = survey_.center.x;
    dr.centerY = survey_.center.y;
    dr.halfExtentM = 3000;
    dr.pixelSizeM = 30;
    dr.cacheDir = cache_;
    QVERIFY2(loadDemWindow(dr, &dem_, &err), qPrintable(err));

    FeatureRequest fr;
    fr.centerX = survey_.center.x;
    fr.centerY = survey_.center.y;
    fr.halfExtentM = 2900;
    fr.cacheDir = cache_;
    QVERIFY2(loadKhsHeritage(fr, &khs_, &err), qPrintable(err));
    QVERIFY2(loadSpca(QStringLiteral("13"), cache_, &periods_, &err, QStringLiteral("경주")), qPrintable(err));
  }

  void relationsToDesignatedSites() {
    Archaeology3dEvaluator ev(dem_, khs_);
    RelationOptions o;
    o.radiusM = 1000;
    o.periods = &periods_;
    const RelationTable t = computeRelations(ev, survey_.center, survey_.surveyAreas.front(), o);
    QVERIFY(!t.rows.empty());
    for (const auto& r : t.rows)  // 기록용(docs/research/DEVLOG.md 에 옮긴다)
      qInfo().noquote() << QStringLiteral("%1 | %2 | %3 m | %4 %5° | %6 m | %7")
                               .arg(r.item.name, r.item.category).arg(r.item.distanceM, 0, 'f', 1)
                               .arg(r.item.bearingName).arg(r.item.bearingDeg, 0, 'f', 0).arg(r.item.deltaElevM, 0, 'f', 1)
                               .arg(r.period.isEmpty() ? QStringLiteral("자료 없음") : r.period);

    const RelationRow* h = row(t, QStringLiteral("경주 황룡사지"));
    QVERIFY2(h, "경주 황룡사지 행");
    QVERIFY2(std::abs(h->item.distanceM - 492.68) < 1.0, qPrintable(QString::number(h->item.distanceM)));
    QVERIFY2(std::abs(h->item.bearingDeg - 264.6) < 0.5, qPrintable(QString::number(h->item.bearingDeg)));
    QCOMPARE(h->period, QStringLiteral("신라시대"));

    const RelationRow* d = row(t, QStringLiteral("경주 동궁과 월지"));
    QVERIFY2(d, "경주 동궁과 월지 행");
    QVERIFY2(std::abs(d->item.distanceM - 888.38) < 1.0, qPrintable(QString::number(d->item.distanceM)));
    QCOMPARE(d->period, QStringLiteral("통일신라시대"));

    for (size_t i = 1; i < t.rows.size(); ++i) QVERIFY(t.rows[i - 1].item.distanceM <= t.rows[i].item.distanceM);
    QSet<QString> keys;  // 같은 이름·분류는 한 줄(여러 조각 유산은 가장 가까운 조각)
    for (const auto& r : t.rows) {
      const QString k = r.item.name + QLatin1Char('|') + r.item.category;
      QVERIFY2(!keys.contains(k), qPrintable(k));
      keys.insert(k);
      QVERIFY(r.pieces >= 1);
    }
    for (const auto& r : t.rows) QVERIFY(r.item.distanceM <= 1000.0 + 1e-6);
    QVERIFY(t.notes.join(QLatin1Char(' ')).contains(QStringLiteral("옛 지표면이 아닙니다")));
    // 추정 열(방리 칸·지형 분류)은 표에도 주석에도 없다(설계도 v3 10장)
    QVERIFY(!t.notes.join(QLatin1Char(' ')).contains(QStringLiteral("방리")));
    QVERIFY(!t.notes.join(QLatin1Char(' ')).contains(QStringLiteral("지형 단위")));
  }

  void unnamedFeaturesAreHidden() {
    std::vector<GeoFeature> feats = khs_;
    GeoFeature f;  // 이름 없는 OSM 유적 점 하나를 조사구역 옆에 둔다
    f.kind = FeatureKind::HeritagePoint;
    f.id = QStringLiteral("osm:test/unnamed");
    f.category = QStringLiteral("historic=yes");
    f.source = QStringLiteral("OSM");
    f.heritage = true;
    f.point = {survey_.center.x + 120, survey_.center.y};
    feats.push_back(f);
    Archaeology3dEvaluator ev(dem_, feats);
    const RelationTable t = computeRelations(ev, survey_.center, survey_.surveyAreas.front(), {});
    QVERIFY(t.hiddenUnnamed >= 1);
    for (const auto& r : t.rows) QVERIFY2(r.item.name != QStringLiteral("(이름 없음)") && !r.item.name.isEmpty(), qPrintable(r.item.id));
    RelationOptions all;
    all.includeUnnamed = true;
    QVERIFY(computeRelations(ev, survey_.center, survey_.surveyAreas.front(), all).rows.size() > t.rows.size());
  }

  // 경사는 Strata(hgis) DemAnalyzer::slopeDegrees 를 같이 컴파일해 쓴다. 예전 3D 자체 Horn 식과 같은 값인지.
  void slopeComesFromStrataDemAnalyzer() {
    Archaeology3dEvaluator ev(dem_, khs_);
    const DemGrid& g = dem_;
    int checked = 0;
    for (int r = 10; r < g.height - 10; r += 7) {
      for (int c = 10; c < g.width - 10; c += 9) {
        const Pt p{g.xAt(c + 0.5), g.yAt(r + 0.5)};
        const double ps = g.pixelSize;
        const double a = g.at(c - 1, r - 1), b = g.at(c, r - 1), cc = g.at(c + 1, r - 1);
        const double d = g.at(c - 1, r), f = g.at(c + 1, r);
        const double gg = g.at(c - 1, r + 1), h = g.at(c, r + 1), i = g.at(c + 1, r + 1);
        const double dzE = ((cc + 2 * f + i) - (a + 2 * d + gg)) / (8 * ps);
        const double dzN = ((a + 2 * b + cc) - (gg + 2 * h + i)) / (8 * ps);
        const double legacy = std::atan(std::hypot(dzE, dzN)) * 180.0 / 3.14159265358979323846;
        const double got = ev.terrainMetricsAt(p).slopeDeg;
        QVERIFY2(std::abs(got - legacy) < 0.01, qPrintable(QStringLiteral("%1,%2: %3 vs %4").arg(c).arg(r).arg(got).arg(legacy)));
        ++checked;
      }
    }
    QVERIFY(checked > 100);
  }

  void trenchCanBeTheSiteToo() {
    Archaeology3dEvaluator ev(dem_, khs_);
    const auto& tr = survey_.trenches.front();
    Pt c;
    for (const Pt& p : tr) { c.x += p.x / tr.size(); c.y += p.y / tr.size(); }
    const RelationTable t = computeRelations(ev, c, tr, {});
    QVERIFY(!t.rows.empty());
    QVERIFY(t.rows.front().item.distanceM >= 0);
  }
};

QTEST_GUILESS_MAIN(TestRelations)
#include "test_relations.moc"
