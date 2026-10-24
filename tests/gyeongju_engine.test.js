/**
 * @file gyeongju_engine.test.js
 * @description Automated verification test suite for the Gyeongju 3D Archaeological & Terrain Engine.
 */

import test from 'node:test';
import assert from 'node:assert/strict';

import {
  GYEONGJU_ORIGIN_WGS84,
  wgs84ToEpsg5187,
  epsg5187ToWgs84,
  wgs84ToLocal3D,
  local3DToWgs84,
  wgs84ToSlippyTile,
  computeDistanceAndBearing5187,
} from '../src/core/gyeongju_crs.js';

import {
  PBVHTree,
  KDTree3D,
  generateRoundBufferPolygon,
} from '../src/core/bvh_spatial.js';

import {
  sampleGyeongjuElevation,
  analyzeGyeongjuTerrainPoint,
  GyeongjuTerrainQuadtree,
} from '../src/core/gyeongju_dem_terrain.js';

import {
  getGyeongjuArchaeologySites3D,
  generateSillaBangrijeGrid3D,
  generateGyeongju3DBuildings,
} from '../src/core/gyeongju_archaeology_db.js';

import { GyeongjuArchaeologyEvaluator } from '../src/core/archaeology_evaluator.js';
import { computeGyeongjuSunPosition } from '../src/core/eevee_sky_shader.js';
import {
  generateBlender42PythonScript,
  exportGyeongjuWavefrontObj,
  generateArchaeologicalMarkdownReport,
} from '../src/bridge/blender_exporter.js';

test('1. CRS: EPSG:4326 <-> EPSG:5187 & Local 3D round-trip precision at Gyeongju', () => {
  const { lat, lon } = GYEONGJU_ORIGIN_WGS84;
  const p5187 = wgs84ToEpsg5187(lat, lon);
  // Gyeongju (129.219E, 35.8347N) is ~19.8km East of 129.0E central meridian -> X ~ 219,790m
  assert.ok(p5187.x > 219000 && p5187.x < 221000, `Expected EPSG:5187 X near 219.8km, got ${p5187.x}`);
  assert.ok(p5187.y > 358000 && p5187.y < 362000, `Expected EPSG:5187 Y near 359.8km, got ${p5187.y}`);

  const back = epsg5187ToWgs84(p5187.x, p5187.y);
  assert.ok(Math.abs(back.lat - lat) < 1e-7);
  assert.ok(Math.abs(back.lon - lon) < 1e-7);

  const loc3d = wgs84ToLocal3D(35.8375, 129.2333, 52.0); // Hwangnyongsa
  assert.ok(loc3d.worldX > 1000, 'Hwangnyongsa is East of Cheomseongdae');
  assert.ok(loc3d.worldZ < 0, 'Hwangnyongsa is North (-Z) of Cheomseongdae');

  const backWgs = local3DToWgs84(loc3d.worldX, loc3d.worldZ);
  assert.ok(Math.abs(backWgs.lat - 35.8375) < 1e-6);
  assert.ok(Math.abs(backWgs.lon - 129.2333) < 1e-6);

  const distBearing = computeDistanceAndBearing5187(0, 0, 0, 500);
  assert.equal(distBearing.distanceM, 500);
  assert.equal(distBearing.directionKo, '북');

  const tile = wgs84ToSlippyTile(lat, lon, 14);
  assert.equal(tile.z, 14);
  assert.ok(tile.x > 14000 && tile.y > 6400);
});

test('2. Spatial Acceleration: PBVHTree O(log N) raycast, viewshed occlusion, and KDTree3D 500m/1000m range search', () => {
  const qt = new GyeongjuTerrainQuadtree({
    extentM: 6000,
    maxDepth: 2,
    chunkGridSegments: 12,
  }).build();

  assert.equal(qt.leafChunks.length, 16);
  assert.ok(qt.bvh.nodes.length > 50);

  // Raycast straight down onto Cheomseongdae (0, 0)
  const hit = qt.bvh.raycast([0, 1000, 0], [0, -1, 0], 2000);
  assert.equal(hit.hit, true);
  assert.ok(hit.point[1] > 35 && hit.point[1] < 65, `Expected ground elevation ~44m, got ${hit.point[1]}`);
  // Verify logarithmic node visits (much smaller than total triangles)
  assert.ok(hit.nodeVisits < qt.bvh.triangles.length * 0.15);

  // KDTree3D range search on Gyeongju sites
  const sites = getGyeongjuArchaeologySites3D();
  const kd = new KDTree3D(sites.map((s) => ({ x: s.worldX, y: s.worldZ, z: s.worldY, data: s })));
  const nearCheomseongdae = kd.findRange(0, 0, 45, 1000, true);
  assert.ok(nearCheomseongdae.length >= 5, 'Should find Cheomseongdae, Wolseong, Donggung, Jjoksaem, Daereungwon within 1km');

  // Round buffer polygon (`JoinStyle::Round`)
  const buf500 = generateRoundBufferPolygon([100, 200], 500, 64);
  assert.equal(buf500.length, 65);
  const sampleRadius = Math.hypot(buf500[17][0] - 100, buf500[17][1] - 200);
  assert.ok(Math.abs(sampleRadius - 500.0) < 1e-6);
});

test('3. Calibrated Gyeongju DEM Geomorphology: Basin floor vs. Namsan, Nangsan, Myeonghwalsan', () => {
  const elevBasin = sampleGyeongjuElevation(0, 0); // Cheomseongdae plain
  const elevNangsan = sampleGyeongjuElevation(2110, 1310); // Nangsan (Queen Seondeok Tomb)
  const elevNamsan = sampleGyeongjuElevation(320, 4880); // Namsan Geumobong
  const elevMyeonghwal = sampleGyeongjuElevation(3780, -95); // Myeonghwalsan

  assert.ok(elevBasin >= 40 && elevBasin <= 52, `Basin floor elevation=${elevBasin}`);
  assert.ok(elevNangsan > elevBasin + 40, `Nangsan (${elevNangsan}m) must rise above basin floor`);
  assert.ok(elevNamsan > 400, `Namsan Geumobong (${elevNamsan}m) must exceed 400m`);
  assert.ok(elevMyeonghwal > 200, `Myeonghwalsan (${elevMyeonghwal}m) must exceed 200m`);

  const ptAnalysis = analyzeGyeongjuTerrainPoint(200, -100);
  assert.ok(ptAnalysis.archaeoSuitabilityScore >= 85);
  assert.ok(ptAnalysis.slopeDeg < 6.0);
});

test('4. Gyeongju 3D Archaeological Evaluator & Blender 4.2 Bridge Export', () => {
  const qt = new GyeongjuTerrainQuadtree({ extentM: 8400, maxDepth: 2, chunkGridSegments: 12 }).build();
  const evaluator = new GyeongjuArchaeologyEvaluator(qt.bvh);

  const report = evaluator.evaluateSite({
    lat: 35.8347,
    lon: 129.2190,
    surveyName: '경주 첨성대·인왕동 기준 평가구역',
  });

  assert.ok(report.proximityAnalysis.within500mCount >= 2);
  assert.ok(report.proximityAnalysis.totalWithin1000mCount >= 6);
  assert.equal(report.bangrijeEvaluation.withinCoreCapitalGrid, true);
  assert.equal(report.viewshedAnalysis.landmarks.length, 6);
  assert.equal(report.virtualTrenches.predictedStratigraphy.length, 4);
  assert.ok(report.assessmentSummary.archaeologicalPotentialScore >= 85);

  const sun = computeGyeongjuSunPosition(14.5, 265);
  assert.ok(sun.altitudeDeg > 25 && sun.altitudeDeg < 65);

  const pyScript = generateBlender42PythonScript(report, { gridResolution: 8, extentM: 2000 });
  assert.ok(pyScript.includes("BLENDER_EEVEE_NEXT"));
  assert.ok(pyScript.includes("NISHITA"));
  assert.ok(pyScript.includes("AgX"));
  assert.ok(pyScript.includes("BVHTree.FromPolygons"));

  const mdReport = generateArchaeologicalMarkdownReport(report);
  assert.ok(mdReport.includes('경주 3D 지형·고고학 정밀 입지 및 매장유산 영향평가 보고서'));

  const objText = exportGyeongjuWavefrontObj(qt);
  assert.ok(objText.startsWith('# Wavefront OBJ exported by hgis-3d-engine'));

  const blds = generateGyeongju3DBuildings(200);
  assert.equal(blds.length, 200);
  const grid = generateSillaBangrijeGrid3D();
  assert.ok(grid.roads.length > 20);
});
