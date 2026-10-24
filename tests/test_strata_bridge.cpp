// 실제 strata-3d.exe 를 띄워 Strata 연동 파이프로 전 주기를 한 번 돌린다:
// 조사 열기 → 평가 → 지형 단면 → 도면 저장 → 상태 파일 → 종료.
// 조사 파일은 hgis 의 시험용 표본(samples/demo_survey/demo.gpkg)의 복사본이다. 표본의 도형은 시험용이지만
// 그 자리의 DEM·건물·유적은 실제 자료다. 창이 뜨는 시험이다(GPU 필요).
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

#include "StrataBridgeClient.h"

using namespace strata3d_bridge;

class TestStrataBridge : public QObject {
  Q_OBJECT
  QProcess proc_;
  QString pipe_;
  QTemporaryDir tmp_;

  QJsonObject call(const QJsonObject& req, int timeoutMs = 120000) {
    QString err;
    const QJsonObject r = request(pipe_, req, timeoutMs, &err);
    if (r.isEmpty()) qWarning() << "요청 실패:" << req.value("cmd").toString() << err;
    return r;
  }

 private slots:
  void initTestCase() {
    QVERIFY(tmp_.isValid());
    pipe_ = QStringLiteral("strata-3d-test-%1").arg(QCoreApplication::applicationPid());
    proc_.setProcessChannelMode(QProcess::ForwardedChannels);
    proc_.start(QStringLiteral(S3D_APP_EXE), {QStringLiteral("--bridge-pipe"), pipe_, QStringLiteral("--no-satellite"), QStringLiteral("--no-initial-scene")});
    QVERIFY2(proc_.waitForStarted(20000), "strata-3d.exe 를 시작하지 못했습니다");
    QElapsedTimer t;
    t.start();
    bool up = false;
    while (t.elapsed() < 90000 && !up) {
      QString err;
      up = request(pipe_, {{"cmd", "hello"}}, 2000, &err).value("ok").toBool();
      if (!up) QTest::qWait(500);
    }
    QVERIFY2(up, "연동 파이프가 열리지 않았습니다");
  }

  void helloListsCommands() {
    const QJsonObject r = call({{"cmd", "hello"}, {"id", 7}});
    QVERIFY(r.value("ok").toBool());
    QCOMPARE(r.value("id").toInt(), 7);
    QCOMPARE(r.value("protocol").toInt(), kProtocolVersion);
    QVERIFY(r.value("commands").toArray().contains("renderSheet"));
    QVERIFY(!call({{"cmd", "nonsense"}}).value("ok").toBool());
  }

  void openEvaluateProfileSheetQuit() {
    const QString src = QStringLiteral(S3D_HGIS_ROOT) + QStringLiteral("/samples/demo_survey/demo.gpkg");
    QVERIFY2(QFile::exists(src), qPrintable(src));
    const QString gpkg = tmp_.filePath("survey.gpkg");
    QVERIFY(QFile::copy(src, gpkg));

    // 1) 조사 열기: 그 자리로 DEM·건물·유적을 다시 불러온다.
    const QJsonObject open = call({{"cmd", "openSurvey"}, {"gpkg", gpkg}}, 300000);
    QVERIFY2(open.value("ok").toBool(), qPrintable(open.value("error").toString()));
    const QJsonObject sv = open.value("survey").toObject();
    qInfo() << "조사 열림:" << sv.value("layers").toArray() << "중심" << sv.value("center").toObject();
    QVERIFY(sv.value("layers").toArray().contains("survey_area"));
    QVERIFY(sv.value("layers").toArray().contains("trial_trench"));
    QVERIFY(std::abs(sv.value("center").toObject().value("x").toDouble() - 200050.0) < 100 && open.value("survey").toObject().contains("layers"));
    QVERIFY(open.value("scene_complete").toBool());
    QVERIFY(open.contains("warnings"));  // 자료를 못 받았다면 여기에 사유가 있어야 한다
    qInfo() << "경고" << open.value("warnings").toArray() << "받은 지물" << open.value("feature_count").toInt();

    // 2) 조사구역 평가
    const QJsonObject ev = call({{"cmd", "evaluate"}, {"target", "survey"}}, 180000);
    QVERIFY2(ev.value("ok").toBool(), qPrintable(ev.value("error").toString()));
    const QJsonObject e = ev.value("evaluation").toObject();
    qInfo() << "평가 요약:" << e.value("summary").toString();
    const double z = e.value("terrain").toObject().value("elevation_m").toDouble();
    QVERIFY(z > 20 && z < 1500);
    QCOMPARE(e.value("radii").toArray().size(), 2);
    QCOMPARE(e.value("radii").toArray().at(0).toObject().value("radius_m").toDouble(), 500.0);
    const double reach = e.value("radii").toArray().at(0).toObject().value("buffer_max_reach_m").toDouble();
    QVERIFY(reach >= 500.0 && reach < 500.0 + 100.0);  // 조사구역(120×100 m) 둘레 + 500 m, 대각선으로 튀지 않음
    QVERIFY(!e.value("summary").toString().isEmpty());
    bool coverageNote = false;
    for (const QJsonValue& n : e.value("notes").toArray()) coverageNote |= n.toString().contains(QString::fromUtf16(u"평가에 쓴 자료"));
    QVERIFY(coverageNote);
    QCOMPARE(e.value("crs").toString(), QString("EPSG:5186"));  // 조사 파일의 좌표계를 따른다

    // 3) 지형 단면(지표면)
    const QJsonObject pf = call({{"cmd", "profile"}, {"a", QJsonObject{{"x", 200060.0}, {"y", 450000.0}}}, {"b", QJsonObject{{"x", 200080.0}, {"y", 450002.0}}}, {"step", 2}});
    if (!pf.value("ok").toBool()) qWarning() << "profile 응답:" << QJsonDocument(pf).toJson(QJsonDocument::Compact);
    QVERIFY(pf.value("ok").toBool());
    QVERIFY(pf.value("profile").toArray().size() >= 5);

    // 4) 도면 저장
    const QString png = tmp_.filePath("sheet/도면.png"), pdf = tmp_.filePath("sheet/도면.pdf");
    const QJsonObject sh = call({{"cmd", "renderSheet"}, {"png", png}, {"pdf", pdf}}, 300000);
    QVERIFY2(sh.value("ok").toBool(), qPrintable(sh.value("error").toString()));
    QVERIFY(QFileInfo(png).size() > 200000);
    QVERIFY(QFileInfo(pdf).size() > 20000);

    // 5) 상태 파일: 앱이 꺼져도 마지막 결과 경로를 읽을 수 있다.
    QFile sf(stateFilePath());
    QVERIFY(sf.open(QIODevice::ReadOnly));
    const QJsonObject st = QJsonDocument::fromJson(sf.readAll()).object();
    QVERIFY(st.value("last_sheet").toObject().value("png").toString().endsWith("도면.png"));
    QVERIFY(!st.value("last_evaluation").toObject().value("summary").toString().isEmpty());

    // 6) 종료
    QVERIFY(call({{"cmd", "quit"}}).value("ok").toBool());
    QVERIFY2(proc_.waitForFinished(30000), "quit 뒤에도 프로세스가 끝나지 않았습니다");
  }

  void cleanupTestCase() {
    if (proc_.state() != QProcess::NotRunning) {
      proc_.kill();
      proc_.waitForFinished(5000);
    }
  }
};

QTEST_GUILESS_MAIN(TestStrataBridge)
#include "test_strata_bridge.moc"
