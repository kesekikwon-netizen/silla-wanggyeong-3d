#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include "BangriTerrainValidator.h"
#include "EvidenceTier.h"
#include "HeritageNaming.h"

using namespace strata3d;

// 신뢰 표기 시험: 지정번호를 쓰지 않는다(2021-11-19 시행), 도식 모델은 도식으로 보인다.
class TestHeritageTrust : public QObject {
  Q_OBJECT
 private slots:
  // 3D 이름표는 유적 이름만(사용자 지시 2026-10-25). 지정번호·앞 종목·괄호 설명을 뺀다.
  void labelsShowSiteNameOnly() {
    QCOMPARE(siteNameOnly(QStringLiteral("사적 경주 황룡사지")), QStringLiteral("경주 황룡사지"));
    QCOMPARE(siteNameOnly(QStringLiteral("국보 경주 첨성대")), QStringLiteral("경주 첨성대"));
    QCOMPARE(siteNameOnly(QStringLiteral("황룡사 9층목탑 · 대가람 (사적 제6호)")), QStringLiteral("황룡사 9층목탑 · 대가람"));
    QCOMPARE(siteNameOnly(QStringLiteral("대릉원 황남대총 (높이 23m 남북 쌍분 · 사적 제512호)")), QStringLiteral("대릉원 황남대총"));
    QCOMPARE(siteNameOnly(QStringLiteral("국가민속문화유산 경주 양동마을")), QStringLiteral("경주 양동마을"));
    QCOMPARE(siteNameOnly(QStringLiteral("경주 남산")), QStringLiteral("경주 남산"));
  }

  void stripsDesignationNumbers_data() {
    QTest::addColumn<QString>("raw");
    QTest::addColumn<QString>("expected");
    QTest::newRow("괄호 전체") << QStringLiteral("황룡사 9층목탑 · 대가람 (사적 제6호)") << QStringLiteral("황룡사 9층목탑 · 대가람");
    QTest::newRow("국보") << QStringLiteral("경주 첨성대 (국보 제31호)") << QStringLiteral("경주 첨성대");
    QTest::newRow("다른 말과 섞임") << QStringLiteral("대릉원 황남대총 (높이 23m 남북 쌍분 · 사적 제512호)")
                                  << QStringLiteral("대릉원 황남대총 (높이 23m 남북 쌍분)");
    QTest::newRow("가운뎃점 번호") << QStringLiteral("경주 계림 & 내물왕릉 (사적 제19·188호)") << QStringLiteral("경주 계림 & 내물왕릉");
    QTest::newRow("괄호 밖") << QStringLiteral("성덕대왕신종 (국보 제29호) & 국립경주박물관")
                           << QStringLiteral("성덕대왕신종 & 국립경주박물관");
    QTest::newRow("괄호 안 종목만") << QStringLiteral("분황사 및 구황동 원지 (국보 제30호·사적 제548호)") << QStringLiteral("분황사 및 구황동 원지");
    QTest::newRow("번호 없음") << QStringLiteral("경주 황룡사지") << QStringLiteral("경주 황룡사지");
    QTest::newRow("고분 번호는 둔다") << QStringLiteral("황남동 제98호분") << QStringLiteral("황남동 제98호분");
    QTest::newRow("괄호 안 쉼표 숫자") << QStringLiteral("석탑 (높이 1,245 cm)") << QStringLiteral("석탑 (높이 1,245 cm)");
    QTest::newRow("앞머리 종목") << QStringLiteral("[사적] 경주 탈해왕릉") << QStringLiteral("경주 탈해왕릉");
    QTest::newRow("앞머리 종목 아님") << QStringLiteral("[가칭] 연못") << QStringLiteral("[가칭] 연못");
  }
  void stripsDesignationNumbers() {
    QFETCH(QString, raw);
    QFETCH(QString, expected);
    QCOMPARE(displayHeritageName(raw), expected);
    if (!raw.contains(QStringLiteral("호분"))) QVERIFY(!containsDesignationNumber(displayHeritageName(raw)));
  }

  void kindIsKept() {
    QCOMPARE(designationKind(QStringLiteral("사적 제19·188호")), QStringLiteral("사적"));
    QCOMPARE(designationKind(QStringLiteral("내물왕릉계림월성지대 사적 및 명승")), QStringLiteral("사적 및 명승"));
    QCOMPARE(designationKind(QStringLiteral("국보 제31호")), QStringLiteral("국보"));
    QCOMPARE(designationKind(QStringLiteral("천연기념물")), QStringLiteral("천연기념물"));
    QVERIFY(designationKind(QStringLiteral("대사찰")).isEmpty());
  }

  // 실제 번들 목록(311곳) 전부: 화면 이름에 지정번호가 남지 않는다.
  void wholeManifestIsClean() {
    QFile f(QStringLiteral(S3D_SOURCE_DIR "/data/models/gyeongju_heritage_manifest.json"));
    QVERIFY2(f.open(QIODevice::ReadOnly), "manifest");
    const QJsonArray sites = QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("sites")).toArray();
    QVERIFY(sites.size() >= 300);
    int hadNumber = 0;
    for (const auto& v : sites) {
      const QString raw = v.toObject().value(QStringLiteral("name")).toString();
      if (containsDesignationNumber(raw)) ++hadNumber;
      const QString shown = displayHeritageName(raw);
      QVERIFY2(!containsDesignationNumber(shown), qPrintable(raw + QStringLiteral(" → ") + shown));
      QVERIFY2(!shown.isEmpty(), qPrintable(raw));
    }
    QVERIFY(hadNumber > 0);  // 원자료에는 번호가 있었다(정리가 실제로 일한다)
  }

  // 카탈로그(화면에 쓰는 메타)는 이미 정리된 이름과 도식 등급을 준다.
  void catalogIsCleanAndSchematic() {
    const auto cat = BangriTerrainValidator::gyeongjuHeritageCatalog();
    QVERIFY2(BangriTerrainValidator::hasBlenderHeritageBundle(), "data/models/gyeongju_blender_heritage.s3dmesh");
    QVERIFY2(cat.size() >= 300, qPrintable(QString::number(cat.size())));  // 번들 311곳까지 읽었다
    for (const auto& m : cat) {
      QVERIFY2(!containsDesignationNumber(m.name), qPrintable(m.name));
      QVERIFY2(!containsDesignationNumber(m.category), qPrintable(m.category));
      QCOMPARE(int(m.tier), int(EvidenceTier::Schematic));
      QVERIFY2(!m.subtitle.contains(QStringLiteral("복원")), qPrintable(m.subtitle));
    }
  }

  void schematicLooksGrey() {
    SceneMesh mesh;
    // 붉은 기둥 색 정점 하나(위치 3, 법선 3, 색 4)
    mesh.vertices = {0, 0, 0, 0, 0, 1, 0.78f, 0.21f, 0.15f, 1.0f};
    applyTierStyle(&mesh, 0, 1, defaultTierStyle(EvidenceTier::Schematic));
    const float* c = &mesh.vertices[6];
    const float spread = std::max({c[0], c[1], c[2]}) - std::min({c[0], c[1], c[2]});
    QVERIFY2(spread < 0.08f, qPrintable(QString::number(spread)));
    QCOMPARE(c[3], 1.0f);
    QCOMPARE(mesh.vertices[5], 1.0f);  // 법선은 그대로

    SceneMesh keep = mesh;
    keep.vertices[6] = 0.78f;
    applyTierStyle(&keep, 0, 1, defaultTierStyle(EvidenceTier::Measured));
    QCOMPARE(keep.vertices[6], 0.78f);  // 실측은 색을 바꾸지 않는다
  }

  void tierWords() {
    QCOMPARE(tierLetter(EvidenceTier::Schematic), QChar(u'D'));
    QCOMPARE(tierLabel(EvidenceTier::Documented), QStringLiteral("근거"));  // 「복원」 낱말은 화면에 쓰지 않는다(설계도 v3 11장)
  }
};

QTEST_GUILESS_MAIN(TestHeritageTrust)
#include "test_heritage_trust.moc"
