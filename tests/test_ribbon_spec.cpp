#include <QFile>
#include <QRegularExpression>
#include <QtTest>

#include "KaIconsMockup.h"
#include "RibbonSpec.h"

// 리본 단추표 검사(설계도 v3 9장 0단계 완료 기준). 기준은 Strata 소스 그 자체다:
// hgis src/app/MainWindowRibbon.cpp 의 addGroup 순서와 아이콘 id, KaIconsMockupMap.cpp 의 SVG 표.
class TestRibbonSpec : public QObject {
  Q_OBJECT
  QString strata_;

 private slots:
  void initTestCase() {
    QFile f(QStringLiteral(S3D_HGIS_ROOT "/src/app/MainWindowRibbon.cpp"));
    QVERIFY2(f.open(QIODevice::ReadOnly), "hgis MainWindowRibbon.cpp");
    strata_ = QString::fromUtf8(f.readAll());
    QVERIFY(strata_.contains(QStringLiteral("addGroup")));
  }

  // 두 앱에 모두 있는 묶음은 Strata 와 같은 순서다.
  void sharedGroupsKeepStrataOrder() {
    QStringList strataOrder;
    // 따옴표 자리는 「.」로 맞춘다(소스 문자열 안에서 따옴표를 겹쳐 쓰지 않으려고)
    static const QRegularExpression re(QStringLiteral("ribbon->addGroup\\(QStringLiteral\\(.(\\w+).\\)"));
    for (auto it = re.globalMatch(strata_); it.hasNext();) strataOrder << it.next().captured(1);
    QCOMPARE(strataOrder, (QStringList{"survey", "record", "fetch", "basemap", "align", "out", "more"}));
    int last = -1;
    QStringList seen;
    for (const ribbon::Group& g : ribbon::groups()) {
      if (g.strataGroup.isEmpty()) continue;
      const int at = strataOrder.indexOf(g.strataGroup);
      QVERIFY2(at >= 0, qPrintable(g.strataGroup));
      QVERIFY2(at > last, qPrintable(QStringLiteral("묶음 %1 이 Strata 순서와 다릅니다").arg(g.id)));
      last = at;
      seen << g.strataGroup;
    }
    // 「정합」만 두지 않는다(옛 지도·사진 맞추기는 2D 에서, 설계도 v3 3장)
    QCOMPARE(seen, (QStringList{"survey", "record", "fetch", "basemap", "out", "more"}));
  }

  // Strata 에서 가져온 단추는 Strata 와 같은 아이콘 id 이고, Strata 가 addIcon 으로 단 단추는 이름도 같다.
  void strataButtonsKeepStrataIconsAndNames() {
    // 의도한 차이: GeoTIFF 자리는 「이미지」(3D 그림은 지도 좌표에 맞지 않는다). 위성은 Strata 에서 리본 칩이 아니다
    // (VWorld 위성은 시작할 때 자동으로 올라오고, 같은 아이콘은 「더보기」 메뉴에만 있다). 3D 에서는 켜고 끄는 칩으로 둔다.
    const QHash<QString, QString> renamed = {{QStringLiteral("geotiff"), QStringLiteral("이미지")}};
    const QSet<QString> notInStrataRibbon = {QStringLiteral("satellite")};
    static const QRegularExpression addIcon(
        QStringLiteral("addIcon\\(\\s*QStringLiteral\\(.(\\w+).\\)\\s*,\\s*QStringLiteral\\(.([\\w-]+).\\)\\s*,\\s*QStringLiteral\\(.([^)]+).\\)"));
    QHash<QString, QString> strataLabel;
    for (auto it = addIcon.globalMatch(strata_); it.hasNext();) {
      const auto m = it.next();
      strataLabel.insert(m.captured(2), m.captured(3));
    }
    QVERIFY(strataLabel.size() >= 10);
    int checked = 0;
    for (const ribbon::Group& g : ribbon::groups()) {
      if (g.strataGroup.isEmpty()) continue;
      for (const ribbon::Button& b : g.buttons) {
        if (notInStrataRibbon.contains(b.iconId)) continue;
        QVERIFY2(strata_.contains(QStringLiteral("QStringLiteral(\"%1\")").arg(b.iconId)),
                 qPrintable(QStringLiteral("아이콘 id %1 이 Strata 리본에 없습니다").arg(b.iconId)));
        if (strataLabel.contains(b.iconId)) {
          const QString want = renamed.value(b.iconId, strataLabel.value(b.iconId));
          QCOMPARE(b.label, want);
          ++checked;
        }
      }
    }
    QVERIFY(checked >= 10);
  }

  // 모든 단추의 아이콘이 실제 SVG 로 그려진다(Strata 표 또는 3D 전용 표). 빈 그림·「?」 상자가 없다.
  void everyIconHasSvg() {
    QSet<QString> ids;
    for (const ribbon::Group& g : ribbon::groups()) {
      QVERIFY(!g.buttons.isEmpty());
      for (const ribbon::Button& b : g.buttons) {
        QVERIFY2(!ids.contains(b.id), qPrintable(QStringLiteral("단추 id 겹침 %1").arg(b.id)));
        ids.insert(b.id);
        const QString svg = KaIconsMockup::svgPathFor(b.iconId);
        QVERIFY2(!svg.isEmpty(), qPrintable(QStringLiteral("%1(%2) 아이콘 표에 없음").arg(b.id, b.iconId)));
        QFile f(svg);
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(svg));
        QVERIFY2(f.readAll().contains("<svg"), qPrintable(svg));
        QVERIFY(!b.label.isEmpty());
        QVERIFY(!b.tip.isEmpty());
      }
    }
    // Strata 표가 먼저다: Strata id 는 Strata 그림 경로
    QVERIFY(KaIconsMockup::svgPathFor(QStringLiteral("heritage")).startsWith(QStringLiteral(":/ka-hgis/icons/")));
    QVERIFY(KaIconsMockup::svgPathFor(QStringLiteral("view3d")).startsWith(QStringLiteral(":/strata-3d/icons/")));
  }

  // 3D 전용 묶음(3D 보기·3D 분석)이 있고, 화면 단추 순서가 설계도 3장과 같다.
  void layoutMatchesBlueprint() {
    QStringList order;
    for (const ribbon::Group& g : ribbon::groups()) order << g.id;
    QCOMPARE(order, (QStringList{"survey", "record", "fetch", "drape", "view", "analysis", "out", "more"}));
    auto ids = [](const QString& group) {
      QStringList out;
      for (const ribbon::Group& g : ribbon::groups())
        if (g.id == group)
          for (const ribbon::Button& b : g.buttons) out << b.id;
      return out;
    };
    QCOMPARE(ids(QStringLiteral("view")), (QStringList{"view3d", "zscale", "labels", "tiers", "subsurface"}));
    QCOMPARE(ids(QStringLiteral("analysis")), (QStringList{"zones", "relations", "section", "depth", "inventory"}));
    QCOMPARE(ids(QStringLiteral("fetch")), (QStringList{"cadastral", "topo_download", "heritage"}));
    QVERIFY(!ids(QStringLiteral("out")).contains(QStringLiteral("geotiff")));
  }
};

QTEST_GUILESS_MAIN(TestRibbonSpec)
#include "test_ribbon_spec.moc"
