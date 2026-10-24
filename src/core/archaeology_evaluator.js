/**
 * @file archaeology_evaluator.js
 * @description 3D Archaeological Evaluation Engine for Gyeongju (경주).
 * Combines Blender REA-derived KDTree3D + PBVHTree raycasting with Gyeongju geomorphology,
 * Silla Bangrije (방리제) street grid proximity, 500m/1,000m Round buffer analysis,
 * 3D Viewshed (가시권) intervisibility, and Virtual Excavation Trench stratigraphy modeling.
 */

import { computeDistanceAndBearing5187, local3DToWgs84, wgs84ToLocal3D } from './gyeongju_crs.js';
import { KDTree3D, generateRoundBufferPolygon } from './bvh_spatial.js';
import { analyzeGyeongjuTerrainPoint, sampleGyeongjuElevation } from './gyeongju_dem_terrain.js';
import { getGyeongjuArchaeologySites3D, generateSillaBangrijeGrid3D } from './gyeongju_archaeology_db.js';

export class GyeongjuArchaeologyEvaluator {
  /**
   * @param {import('./bvh_spatial.js').PBVHTree} [terrainBvh]
   */
  constructor(terrainBvh = null) {
    this.sites = getGyeongjuArchaeologySites3D();
    this.bangrije = generateSillaBangrijeGrid3D();
    this.terrainBvh = terrainBvh;

    // Build 3D KD-Tree in EPSG:5187 meter coordinates for exact 500m / 1,000m range queries
    this.siteKdTree = new KDTree3D(
      this.sites.map((s) => ({
        x: s.epsg5187X,
        y: s.epsg5187Y,
        z: s.groundElevationM,
        data: s,
      }))
    );
  }

  setTerrainBVH(terrainBvh) {
    this.terrainBvh = terrainBvh;
  }

  /**
   * Evaluate a target point `(worldX, worldZ)` or WGS84 `{lat, lon}` or survey polygon.
   * @param {object} target
   * @param {number} [target.worldX]
   * @param {number} [target.worldZ]
   * @param {number} [target.lat]
   * @param {number} [target.lon]
   * @param {string} [target.surveyName]
   * @param {number} [target.surveyAreaSqm=1200]
   */
  evaluateSite(target = {}) {
    let worldX = target.worldX;
    let worldZ = target.worldZ;
    if (typeof worldX !== 'number' || typeof worldZ !== 'number') {
      const loc = wgs84ToLocal3D(target.lat ?? 35.8347, target.lon ?? 129.2190, 0.0);
      worldX = loc.worldX;
      worldZ = loc.worldZ;
    }

    const terrain = analyzeGyeongjuTerrainPoint(worldX, worldZ);
    const surveyName =
      target.surveyName ||
      `경주 조사구역 (${terrain.lat.toFixed(5)}°N, ${terrain.lon.toFixed(5)}°E)`;
    const surveyAreaSqm = target.surveyAreaSqm ?? 1200;

    // 1. 500m and 1,000m Round Buffer Rings (in local 3D worldX, worldZ with terrain draping)
    const ring500m2D = generateRoundBufferPolygon([worldX, worldZ], 500.0, 64);
    const ring1000m2D = generateRoundBufferPolygon([worldX, worldZ], 1000.0, 64);

    const buffer500m3D = ring500m2D.map(([x, z]) => [x, sampleGyeongjuElevation(x, z) + 1.2, z]);
    const buffer1000m3D = ring1000m2D.map(([x, z]) => [x, sampleGyeongjuElevation(x, z) + 1.2, z]);

    // 2. KD-Tree Range Search for nearby archaeological sites within 500m and 1,000m (and up to 2,000m context)
    const hits1000 = this.siteKdTree.findRange(
      terrain.epsg5187X,
      terrain.epsg5187Y,
      terrain.elevationM,
      1000.0,
      true
    );

    const nearbySites = hits1000.map((hit) => {
      const site = hit.data;
      const rel = computeDistanceAndBearing5187(
        terrain.epsg5187X,
        terrain.epsg5187Y,
        site.epsg5187X,
        site.epsg5187Y
      );
      return {
        id: site.id,
        name: site.name,
        category: site.category,
        designation: site.designation,
        period: site.period,
        distanceM: Math.round(rel.distanceM * 10) / 10,
        azimuthDeg: Math.round(rel.azimuthDeg * 10) / 10,
        directionKo: rel.directionKo,
        zoneBand: rel.distanceM <= 500.0 ? '500m 이내 (직접 영향권)' : '500m~1,000m (간접 영향권)',
        siteElevationM: Math.round(site.groundElevationM * 10) / 10,
        elevationDiffM: Math.round((site.groundElevationM - terrain.elevationM) * 10) / 10,
        buriedFeatureDepthM: site.buriedFeatureDepthM,
        worldX: site.worldX,
        worldY: site.worldY,
        worldZ: site.worldZ,
        summary: site.summary,
      };
    });

    const sitesWithin500m = nearbySites.filter((s) => s.distanceM <= 500.0);
    const sites500to1000m = nearbySites.filter((s) => s.distanceM > 500.0 && s.distanceM <= 1000.0);
    const nearestOverallHit = this.siteKdTree.findNearest(
      terrain.epsg5187X,
      terrain.epsg5187Y,
      terrain.elevationM,
      true
    );

    // 3. Silla Bangrije (신라 왕경 방리제) Ancient Street Grid Alignment Evaluation
    const bangrijeEval = this._evaluateBangrijeAlignment(worldX, worldZ);

    // 4. 3D Viewshed / Intervisibility Analysis using PBVHTree
    const viewshedTargets = [
      { id: 'VS-WOLSEONG', name: '월성(왕궁 정전)', worldX: 405, worldZ: 515, targetHeightOffsetM: 14.0 },
      { id: 'VS-HWANGNYONGSA', name: '황룡사 9층목탑(80m 상륜부)', worldX: 1295, worldZ: -310, targetHeightOffsetM: 80.0 },
      { id: 'VS-CHEOMSEONGDAE', name: '첨성대 관측단', worldX: 0, worldZ: 0, targetHeightOffsetM: 9.17 },
      { id: 'VS-NANGSAN', name: '낭산 선덕여왕릉(도리천)', worldX: 2110, worldZ: 1310, targetHeightOffsetM: 7.0 },
      { id: 'VS-NAMSAN', name: '남산 금오봉(468m)', worldX: 320, worldZ: 4880, targetHeightOffsetM: 4.0 },
      { id: 'VS-MYEONGHWAL', name: '명활산성 서문루', worldX: 3780, worldZ: -95, targetHeightOffsetM: 10.0 },
    ];

    const observerPoint = [worldX, terrain.elevationM + 1.7, worldZ];
    const viewshedResults = viewshedTargets.map((vt) => {
      const targetGround = sampleGyeongjuElevation(vt.worldX, vt.worldZ);
      const targetPoint = [vt.worldX, targetGround + vt.targetHeightOffsetM, vt.worldZ];
      let los = {
        visible: true,
        distanceM: Math.hypot(vt.worldX - worldX, targetPoint[1] - observerPoint[1], vt.worldZ - worldZ),
        obstructionPoint: null,
      };
      if (this.terrainBvh) {
        los = this.terrainBvh.hasLineOfSight(observerPoint, targetPoint);
      }
      return {
        id: vt.id,
        landmarkName: vt.name,
        visible: los.visible,
        distanceM: Math.round(los.distanceM),
        observerPoint,
        targetPoint,
        obstructionPoint: los.obstructionPoint,
        elevationAngleDeg:
          Math.round(
            (Math.atan2(
              targetPoint[1] - observerPoint[1],
              Math.hypot(vt.worldX - worldX, vt.worldZ - worldZ)
            ) *
              180.0) /
              Math.PI *
              100
          ) / 100,
      };
    });

    // 5. Virtual 3D Excavation Trenches & Predicted Stratigraphy Profile
    const virtualTrenches = this._generateVirtualTrenches(worldX, worldZ, terrain, sitesWithin500m, bangrijeEval);

    // 6. Comprehensive Archaeological Potential Score & Action Grade
    const scoreBreakdown = this._computeComprehensiveScore(
      terrain,
      sitesWithin500m,
      sites500to1000m,
      bangrijeEval,
      nearestOverallHit
    );

    return {
      timestamp: new Date().toISOString(),
      surveyName,
      surveyAreaSqm,
      location: {
        lat: Number(terrain.lat.toFixed(6)),
        lon: Number(terrain.lon.toFixed(6)),
        epsg5187X: Number(terrain.epsg5187X.toFixed(2)),
        epsg5187Y: Number(terrain.epsg5187Y.toFixed(2)),
        worldX: Number(worldX.toFixed(2)),
        worldY: Number(terrain.elevationM.toFixed(2)),
        worldZ: Number(worldZ.toFixed(2)),
      },
      geomorphology: {
        elevationM: Number(terrain.elevationM.toFixed(2)),
        slopeDeg: Number(terrain.slopeDeg.toFixed(2)),
        aspectDeg: Number(terrain.aspectDeg.toFixed(1)),
        tpi: Number(terrain.tpi.toFixed(2)),
        landformCode: terrain.landformCode,
        landformNameKo: terrain.landformNameKo,
        nearestRiverName: terrain.nearestRiverName,
        nearestRiverDistM: Math.round(terrain.nearestRiverDistM),
      },
      proximityAnalysis: {
        within500mCount: sitesWithin500m.length,
        within500to1000mCount: sites500to1000m.length,
        totalWithin1000mCount: nearbySites.length,
        nearestSiteName: nearestOverallHit ? nearestOverallHit.data.name : null,
        nearestSiteDistanceM: nearestOverallHit ? Math.round(nearestOverallHit.distance * 10) / 10 : null,
        sitesWithin500m,
        sites500to1000m,
        buffer500m3D,
        buffer1000m3D,
      },
      bangrijeEvaluation: bangrijeEval,
      viewshedAnalysis: {
        visibleCount: viewshedResults.filter((v) => v.visible).length,
        totalLandmarks: viewshedResults.length,
        landmarks: viewshedResults,
      },
      virtualTrenches,
      assessmentSummary: scoreBreakdown,
    };
  }

  _evaluateBangrijeAlignment(worldX, worldZ) {
    const angleRad = (this.bangrije.azimuthDeg * Math.PI) / 180.0;
    const cosA = Math.cos(angleRad);
    const sinA = Math.sin(angleRad);
    const dx = worldX - this.bangrije.originWorldX;
    const dz = worldZ - this.bangrije.originWorldZ;

    // Local coordinates in the rotated Silla Bangrije grid
    const u = dx * cosA + dz * sinA;
    const v = -dx * sinA + dz * cosA;
    const pitch = this.bangrije.blockPitchM;

    const withinCoreCapitalGrid = Math.abs(u) <= 9 * pitch && Math.abs(v) <= 10 * pitch;
    const distToNearestNSRoadM = Math.abs(u - Math.round(u / pitch) * pitch);
    const distToNearestEWRoadM = Math.abs(v - Math.round(v / pitch) * pitch);
    const minRoadDistM = Math.min(distToNearestNSRoadM, distToNearestEWRoadM);

    const bangCol = Math.floor(u / pitch);
    const bangRow = Math.floor(-v / pitch);

    return {
      withinCoreCapitalGrid,
      bangSectorLabel: withinCoreCapitalGrid
        ? `신라 왕경 중심 방리(坊里) 구역 [동서 ${bangCol >= 0 ? '+' : ''}${bangCol}방 · 남북 ${bangRow >= 0 ? '+' : ''}${bangRow}방]`
        : '왕경 외곽 방리 확장권 / 교외 구릉대',
      distToNearestNSRoadM: Math.round(distToNearestNSRoadM * 10) / 10,
      distToNearestEWRoadM: Math.round(distToNearestEWRoadM * 10) / 10,
      minRoadDistM: Math.round(minRoadDistM * 10) / 10,
      ancientRoadEncounterExpected: withinCoreCapitalGrid && minRoadDistM <= 18.0,
    };
  }

  _generateVirtualTrenches(worldX, worldZ, terrain, sitesWithin500m, bangrijeEval) {
    // Standard Korean archaeological trial trenches (폭 2m × 길이 20m, T-1 동서향 / T-2 남북향)
    const baseElev = terrain.elevationM;
    const expectedFeatureDepthTopM = terrain.landformCode === 'FLOODPLAIN_CHANNEL' ? 1.2 : 0.45;
    const expectedFeatureDepthBottomM =
      sitesWithin500m.length > 0 ? 3.4 : bangrijeEval.withinCoreCapitalGrid ? 2.6 : 1.9;

    const layers = [
      {
        layerCode: 'I층',
        nameKo: '현 표토 및 근현대 성토·경작층',
        depthRangeM: [0.0, Number(expectedFeatureDepthTopM.toFixed(2))],
        colorHex: '#6b5b4b',
        expectedFinds: '근현대 자기편, 교란 쇄석',
      },
      {
        layerCode: 'II층',
        nameKo: '통일신라 왕경 도시·생활 문화층 (암갈색 사질점토)',
        depthRangeM: [
          Number(expectedFeatureDepthTopM.toFixed(2)),
          Number((expectedFeatureDepthTopM + 0.95).toFixed(2)),
        ],
        colorHex: '#8c6239',
        expectedFinds: bangrijeEval.ancientRoadEncounterExpected
          ? '통일신라 방리제 도로 측구(배수로), 수레바퀴 흔적, 인화문토기, 평기와·수막새'
          : '통일신라 건물지 적심·초석, 석조 우물, 담장열, 인화문토기·녹유토기편',
      },
      {
        layerCode: 'III층',
        nameKo: '삼국시대(신라 마립간~중고기) 문화층 (흑갈색 점질토)',
        depthRangeM: [
          Number((expectedFeatureDepthTopM + 0.95).toFixed(2)),
          Number(expectedFeatureDepthBottomM.toFixed(2)),
        ],
        colorHex: '#593e25',
        expectedFinds:
          sitesWithin500m.some((s) => s.category === 'ROYAL_TUMULUS')
            ? '적석목곽분·석곽묘 주구(周溝), 호석 열, 고배·장경호·철제 무기편'
            : '신라 5~6세기 수혈주거지, 굴립주 건물지 주혈, 단경호·고배편',
      },
      {
        layerCode: 'IV층',
        nameKo: '플라이스토세 하안단구 역층 / 풍화암반 생토층 (무문물층)',
        depthRangeM: [
          Number(expectedFeatureDepthBottomM.toFixed(2)),
          Number((expectedFeatureDepthBottomM + 1.2).toFixed(2)),
        ],
        colorHex: '#b59b7c',
        expectedFinds: '유구·유물 없음 (기반 생토층 확인 후 시굴 종료 기준면)',
      },
    ];

    return {
      trenchSpecs: [
        {
          trenchId: 'T-1 (동서 기준 트렌치)',
          widthM: 4.0,
          lengthM: 24.0,
          depthM: Number((expectedFeatureDepthBottomM + 0.6).toFixed(2)),
          worldX: worldX - 10.0,
          worldY: baseElev,
          worldZ: worldZ,
          rotationRad: (4.5 * Math.PI) / 180.0,
        },
        {
          trenchId: 'T-2 (남북 직교 트렌치)',
          widthM: 4.0,
          lengthM: 24.0,
          depthM: Number((expectedFeatureDepthBottomM + 0.6).toFixed(2)),
          worldX: worldX + 10.0,
          worldY: baseElev,
          worldZ: worldZ,
          rotationRad: ((4.5 + 90.0) * Math.PI) / 180.0,
        },
      ],
      predictedStratigraphy: layers,
    };
  }

  _computeComprehensiveScore(terrain, sites500m, sites500to1000m, bangrijeEval, nearestHit) {
    let score = terrain.archaeoSuitabilityScore * 0.45;
    if (sites500m.length > 0) {
      score += Math.min(35, 18 + sites500m.length * 5);
    } else if (sites500to1000m.length > 0) {
      score += Math.min(20, 8 + sites500to1000m.length * 3);
    }
    if (bangrijeEval.withinCoreCapitalGrid) {
      score += 14;
      if (bangrijeEval.ancientRoadEncounterExpected) score += 6;
    }
    const finalScore = Math.min(99, Math.max(15, Math.round(score)));

    let grade = '표본조사 또는 입회조사 검토';
    let riskLevel = 'MEDIUM';
    if (finalScore >= 85 || (nearestHit && nearestHit.distance <= 250.0)) {
      grade = '정밀 발굴조사 및 시굴조사 필수 (신라 왕경 핵심 유구 밀집권)';
      riskLevel = 'CRITICAL';
    } else if (finalScore >= 70 || sites500m.length > 0) {
      grade = '사전 시굴조사 권고 (매장유산 포장 가능성 높음)';
      riskLevel = 'HIGH';
    }

    const synthesisOpinion = [
      `조사구역은 해발 ${terrain.elevationM.toFixed(1)}m, 평균 경사도 ${terrain.slopeDeg.toFixed(1)}°의 [${terrain.landformNameKo}]에 위치하며, ${terrain.nearestRiverName}으로부터 약 ${Math.round(terrain.nearestRiverDistM)}m 이격되어 있다.`,
      `반경 500m 이내에 국가지정·세계유산급 유적 ${sites500m.length}개소, 500m~1,000m 이내에 ${sites500to1000m.length}개소가 분포하며, 최인접 유적은 [${nearestHit ? nearestHit.data.name : '없음'}](이격거리 ${nearestHit ? Math.round(nearestHit.distance) : 0}m)이다.`,
      bangrijeEval.withinCoreCapitalGrid
        ? `${bangrijeEval.bangSectorLabel} 내부에 해당하며, 고대 방리 도로 추정선과 최소 ${bangrijeEval.minRoadDistM}m 거리로 인접하여 통일신라 도시 유구(건물지·도로·배수로·우물) 및 하층 4~6세기 고분/생활유구 중첩 가능성이 매우 높다.`
        : `왕경 중심 조방 구역 외곽의 구릉·선상지 사면에 해당하여 고분군 및 사지·생산유구 분포 가능성을 고려한 시굴 트렌치 확인이 필요하다.`,
    ].join(' ');

    return {
      archaeologicalPotentialScore: finalScore,
      riskLevel,
      recommendedActionGrade: grade,
      synthesisOpinion,
    };
  }
}
