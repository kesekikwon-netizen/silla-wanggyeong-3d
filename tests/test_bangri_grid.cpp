#include <QtTest>
#include <cmath>

#include "BangriGridEngine.h"
#include "BangriSurveyEvaluator.h"
#include "BangriTerrainValidator.h"
#include "CrsTransform.h"
#include "DbpiaBangriLedger.h"
#include "FossilizedRoadAnalyzer.h"
#include "SillaMoundReconstructor.h"

using namespace strata3d;

class TestBangriGrid : public QObject {
  Q_OBJECT
 private slots:
  void initTestCase() {
    const QString root = qEnvironmentVariable("OSGEO4W_ROOT", QStringLiteral("C:/OSGeo4W"));
    if (qEnvironmentVariableIsEmpty("PROJ_DATA"))
      qputenv("PROJ_DATA", QDir(root).filePath("share/proj").toUtf8());
    if (qEnvironmentVariableIsEmpty("GDAL_DATA"))
      qputenv("GDAL_DATA", QDir(root).filePath("apps/gdal-dev/share/gdal").toUtf8());
  }

  void scaleConversions() {
    QCOMPARE(BangriGridEngine::scaleUnitMeters(BangriScaleUnit::GoguryeoChi), 0.356);
    QCOMPARE(BangriGridEngine::scaleUnitMeters(BangriScaleUnit::TangLargeChi), 0.296);
    QCOMPARE(BangriGridEngine::scaleUnitMeters(BangriScaleUnit::Meter), 1.0);

    // 450 고구려척 = 160.20 m
    const double m450 = BangriGridEngine::scaleUnitToMeters(450.0, BangriScaleUnit::GoguryeoChi);
    QVERIFY(std::abs(m450 - 160.20) < 1e-6);
    QVERIFY(std::abs(BangriGridEngine::metersToScaleUnit(m450, BangriScaleUnit::GoguryeoChi) - 450.0) < 1e-6);

    // 566 당척 ≈ 167.536 m
    const double m566 = BangriGridEngine::scaleUnitToMeters(566.0, BangriScaleUnit::TangLargeChi);
    QVERIFY(std::abs(m566 - 167.536) < 1e-6);
  }

  void hwangnyongsaAndDongcheonSubdivision() {
    // 1. 황룡사 동편 권역: 167.5m × 172.5m, 井자형 9분할
    const BangriCityPreset hwang = BangriGridEngine::findPreset(QStringLiteral("gyeongju_hwangnyongsa"));
    QCOMPARE(hwang.id, QStringLiteral("gyeongju_hwangnyongsa"));
    QVERIFY(std::abs(hwang.defaultParams.blockWidthM - 167.5) < 0.01);
    QVERIFY(std::abs(hwang.defaultParams.blockHeightM - 172.5) < 0.01);
    QCOMPARE(hwang.defaultParams.subdivisionMode, BangriSubdivisionMode::Well9);

    BangriParams p = hwang.defaultParams;
    p.extentCols = 4;
    p.extentRows = 4;
    const BangriGridResult gHwang = BangriGridEngine::generateGrid(p);
    QCOMPARE(int(gHwang.blocks.size()), 16);
    for (const auto& blk : gHwang.blocks) {
      QCOMPARE(int(blk.subLots.size()), 9);
    }
    // 양측 측구 간격 검증: 중심선에서 ±(width + ditchWidth)*0.5 떨어져야 한다
    for (const auto& r : gHwang.roads) {
      const double expectedOffset = (r.widthM + r.ditchWidthM) * 0.5;
      const double leftDist = std::hypot(r.leftDitchA.x - r.a.x, r.leftDitchA.y - r.a.y);
      const double rightDist = std::hypot(r.rightDitchA.x - r.a.x, r.rightDitchA.y - r.a.y);
      QVERIFY2(std::abs(leftDist - expectedOffset) < 1e-4, "Left side ditch offset mismatch");
      QVERIFY2(std::abs(rightDist - expectedOffset) < 1e-4, "Right side ditch offset mismatch");
    }

    // 2. 동천동 권역: 160.0m × 125.0m, 十자형 4분할
    const BangriCityPreset dong = BangriGridEngine::findPreset(QStringLiteral("gyeongju_dongcheon"));
    QVERIFY(std::abs(dong.defaultParams.blockWidthM - 160.0) < 0.01);
    QVERIFY(std::abs(dong.defaultParams.blockHeightM - 125.0) < 0.01);
    QCOMPARE(dong.defaultParams.subdivisionMode, BangriSubdivisionMode::Cross4);
    BangriParams pDong = dong.defaultParams;
    pDong.extentCols = 2;
    pDong.extentRows = 2;
    const BangriGridResult gDong = BangriGridEngine::generateGrid(pDong);
    QCOMPARE(int(gDong.blocks.size()), 4);
    for (const auto& blk : gDong.blocks) {
      QCOMPARE(int(blk.subLots.size()), 4);
    }
  }

  void anchorAutoFitConvergence() {
    BangriCityPreset preset = BangriGridEngine::findPreset(QStringLiteral("gyeongju_hwangnyongsa"));
    const BangriParams trueParams = preset.defaultParams;

    // 정확한 격자 교차점/도로 위에 합성 기준점 5개를 배치
    std::vector<BangriAnchor> syntheticAnchors;
    const std::vector<std::pair<int, int>> gridNodes = {{0, 0}, {1, 0}, {0, 1}, {-1, 2}, {2, -1}};
    for (size_t i = 0; i < gridNodes.size(); ++i) {
      const Pt pt = BangriGridEngine::gridLocalToWorld(
          trueParams,
          gridNodes[i].first * trueParams.blockWidthM,
          gridNodes[i].second * trueParams.blockHeightM);
      BangriAnchor a;
      a.id = QStringLiteral("syn_%1").arg(i);
      a.name = a.id;
      a.kind = BangriAnchorKind::Intersection;
      a.x = pt.x;
      a.y = pt.y;
      a.active = true;
      syntheticAnchors.push_back(a);
    }

    // 초기 파라미터에 인위적 오차 (+14.5m 동서, -11.2m 남북, +1.85° 방위각) 부여
    BangriParams perturbed = trueParams;
    perturbed.originEasting += 14.5;
    perturbed.originNorthing -= 11.2;
    perturbed.azimuthDeg += 1.85;

    const double beforeRmse = BangriGridEngine::computeAnchorRmse(perturbed, syntheticAnchors);
    QVERIFY2(beforeRmse > 8.0, "Perturbed RMSE should be significant before fitting");

    const double afterRmse = BangriGridEngine::autoFitToAnchors(&perturbed, syntheticAnchors, 5.0);
    QVERIFY2(afterRmse < beforeRmse, "autoFitToAnchors must reduce RMSE");
    QVERIFY2(afterRmse < 0.05, qPrintable(QStringLiteral("Expected RMSE < 0.05m, got %1").arg(afterRmse)));
    QVERIFY2(std::abs(perturbed.azimuthDeg - trueParams.azimuthDeg) < 0.05,
             qPrintable(QStringLiteral("Azimuth recovery error too large: %1 vs %2")
                            .arg(perturbed.azimuthDeg)
                            .arg(trueParams.azimuthDeg)));
  }

  void all12PresetsAndDbpiaLedger() {
    const auto presets = BangriGridEngine::builtInPresets();
    QCOMPARE(int(presets.size()), 12);

    DbpiaBangriLedger ledger;
    QString err;
    QVERIFY2(ledger.loadFromFile(QString(), &err), qPrintable(err));
    QCOMPARE(ledger.totalPapers(), 200);

    // 핵심 논문 NODE09013833 (신라 왕경 도시계획의 제문제) 본문 Q&A 탑재 확인
    const DbpiaPaperEntry* pMain = ledger.findByNodeId(QStringLiteral("NODE09013833"));
    QVERIFY(pMain != nullptr);
    QVERIFY(!pMain->title.isEmpty());
    QVERIFY(!pMain->coreAnswer.isEmpty());

    for (const auto& preset : presets) {
      QVERIFY(!preset.id.isEmpty());
      QVERIFY(preset.defaultParams.blockWidthM >= 100.0);
      QVERIFY(preset.defaultParams.blockHeightM >= 100.0);

      const auto anchors = BangriGridEngine::builtInAnchorsForPreset(preset.id, preset.workCrs);
      QVERIFY2(!anchors.empty(), qPrintable(QStringLiteral("Preset %1 has no anchors").arg(preset.id)));

      BangriParams params = preset.defaultParams;
      const double rmse = BangriGridEngine::autoFitToAnchors(&params, anchors, 6.0);
      QVERIFY2(rmse < 15.0, qPrintable(QStringLiteral("Preset %1 RMSE too high: %2").arg(preset.id).arg(rmse)));

      const auto citations = ledger.matchForPreset(preset.citationNodeIds, preset.searchKeywords, 6);
      QVERIFY2(int(citations.size()) >= 3,
               qPrintable(QStringLiteral("Preset %1 matched fewer than 3 papers").arg(preset.id)));
    }
  }

  void sillaMoundReconstruction() {
    const auto catalog = SillaMoundReconstructor::builtInSillaMounds(QStringLiteral("EPSG:5187"));
    QVERIFY(int(catalog.size()) >= 15);

    // 황남대총 남분·북분 중심에서 sampleMoundHeight 가 20m 이상 융기하는지 확인
    bool foundHwangnam = false;
    for (const auto& m : catalog) {
      if (m.id == QLatin1String("mound:hwangnam_s")) {
        foundHwangnam = true;
        const float h = SillaMoundReconstructor::sampleMoundHeight(m.x, m.y, catalog);
        QVERIFY2(h >= 21.0f && h <= 24.0f,
                 qPrintable(QStringLiteral("Hwangnamdaechong height out of range: %1").arg(h)));
      }
    }
    QVERIFY(foundHwangnam);

    // 다각형 유적 지물로부터 봉분 반경 및 높이 계산 확인
    GeoFeature tumulusFeat;
    tumulusFeat.kind = FeatureKind::HeritageArea;
    tumulusFeat.name = QStringLiteral("경주 노동동 고분군 봉황대");
    const double rTrue = 40.0;
    for (int i = 0; i < 24; ++i) {
      const double a = (2.0 * 3.14159265358979323846 * i) / 24.0;
      tumulusFeat.ring.push_back({219500.0 + rTrue * std::cos(a), 360200.0 + rTrue * std::sin(a)});
    }
    QVERIFY(SillaMoundReconstructor::isTumulusFeature(tumulusFeat));
    const SillaMound computed = SillaMoundReconstructor::moundFromFeature(tumulusFeat);
    QVERIFY(std::abs(computed.radiusM - rTrue) < 1.5);
    QVERIFY(computed.heightM >= 12.0 && computed.heightM <= 23.0);
  }

  void fossilizedRoadAndTerrainValidation() {
    const BangriCityPreset preset = BangriGridEngine::findPreset(QStringLiteral("gyeongju_hwangnyongsa"));
    const BangriGridResult grid = BangriGridEngine::generateGrid(preset.defaultParams);
    QVERIFY(!grid.roads.empty());

    // 첫 번째 도로와 거의 일치하는 현대 도로 지물 1개 + 35° 어긋난 도로 1개 생성
    const BangriRoadSegment& refRoad = grid.roads.front();
    GeoFeature alignedRoad;
    alignedRoad.kind = FeatureKind::HeritageArea;
    alignedRoad.name = QStringLiteral("현대 화석화 도로");
    alignedRoad.ring = {{refRoad.a.x + 1.5, refRoad.a.y + 1.0},
                        {refRoad.b.x + 1.5, refRoad.b.y + 1.0}};

    GeoFeature diagonalRoad;
    diagonalRoad.kind = FeatureKind::HeritageArea;
    diagonalRoad.name = QStringLiteral("비정렬 대각선 도로");
    diagonalRoad.ring = {{refRoad.a.x, refRoad.a.y},
                         {refRoad.a.x + 200.0, refRoad.a.y + 140.0}};

    const FossilizationResult fos = FossilizedRoadAnalyzer::analyze(
        grid, {alignedRoad, diagonalRoad}, 2.5, 8.0);
    QVERIFY(!fos.matchedSegments.empty());
    QVERIFY(fos.matchedLengthM > 100.0);
    QVERIFY(fos.fossilizationScorePct > 0.0);
  }

  void orthogonalTrialTrenchDesign() {
    const BangriCityPreset preset = BangriGridEngine::findPreset(QStringLiteral("gyeongju_hwangnyongsa"));
    const BangriGridResult grid = BangriGridEngine::generateGrid(preset.defaultParams);

    const Pt site{preset.defaultParams.originEasting + 45.0,
                  preset.defaultParams.originNorthing + 35.0};
    const BangriSurveyEvaluation eval = BangriSurveyEvaluator::evaluateSurvey(grid, site);

    QVERIFY(!eval.locatedBlockCode.isEmpty());
    QVERIFY(!eval.locatedSubLotLabel.isEmpty());
    QCOMPARE(int(eval.recommendedTrenches.size()), 2);

    for (const auto& tr : eval.recommendedTrenches) {
      QVERIFY2(std::abs(tr.intersectionAngleDeg - 90.0) <= 1.0,
               qPrintable(QStringLiteral("Trench %1 is not orthogonal to Bangri road: %2 deg")
                              .arg(tr.id)
                              .arg(tr.intersectionAngleDeg)));
      QVERIFY(tr.widthM >= 1.5 && tr.widthM <= 3.0);
      QVERIFY(tr.lengthM >= 14.0 && tr.lengthM <= 25.0);
    }
  }
};

QTEST_MAIN(TestBangriGrid)
#include "test_bangri_grid.moc"
