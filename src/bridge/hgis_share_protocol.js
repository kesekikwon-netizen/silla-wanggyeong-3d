/**
 * @file hgis_share_protocol.js
 * @description Bidirectional Data, Planning, DEM Scene (`Terrain3dService::DemScene`),
 * GeoJSON (`EPSG:5187` / `EPSG:4326`), and 3D Sheet (`composeSheet`) Sharing Protocol
 * between the main HGIS application (`C:\Users\권을\Documents\hgis`) and `hgis-3d-engine`.
 */

import {
  GYEONGJU_ORIGIN_5187,
  local3DToWgs84,
  wgs84ToLocal3D,
  epsg5187ToWgs84,
} from '../core/gyeongju_crs.js';
import { sampleGyeongjuElevation } from '../core/gyeongju_dem_terrain.js';

export const EPSG5187_WKT =
  'PROJCS["Korea 2000 / East Belt 2010",GEOGCS["Korea 2000",DATUM["Geocentric_datum_of_Korea",SPHEROID["GRS 1980",6378137,298.257222101]],PRIMEM["Greenwich",0],UNIT["Degree",0.0174532925199433]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",38],PARAMETER["central_meridian",129],PARAMETER["scale_factor",1],PARAMETER["false_easting",200000],PARAMETER["false_northing",600000],UNIT["Meter",1]]';

/**
 * Exact port of `Terrain3dService::scaleBarSegmentM(double visibleWidthM)` from `hgis/src/core/Terrain3dService.h`.
 * Picks a clean 1-2-5 metric scale bar length (~1/5 of visible ground width).
 */
export function scaleBarSegmentM(visibleWidthM) {
  const target = Math.max(10.0, visibleWidthM * 0.22);
  const exp = Math.floor(Math.log10(target));
  const base = Math.pow(10, exp);
  const ratio = target / base;
  if (ratio >= 5.0) return 5.0 * base;
  if (ratio >= 2.0) return 2.0 * base;
  return 1.0 * base;
}

/**
 * Exact port of `Terrain3dService::northAzimuthDeg(const double geotransform[6])`.
 */
export function northAzimuthDeg(geotransform = [0, 1, 0, 0, 0, -1]) {
  const upX = -geotransform[2];
  const upY = -geotransform[5];
  const rad = Math.atan2(upY, upX);
  return ((rad * 180.0) / Math.PI - 90.0 + 360.0) % 360.0;
}

/**
 * Export a Gyeongju region window into a `Terrain3dService::DemScene`-compatible JSON payload
 * that `hgis` (`KaTerrain3dStudio`) and `hgis-3d-engine` both read natively.
 */
export function exportToHgisDemScene(options = {}) {
  const centerXWorld = options.worldX ?? 0.0;
  const centerZWorld = options.worldZ ?? 0.0;
  const width = options.width ?? 65;
  const height = options.height ?? 65;
  const groundWidthM = options.groundWidthM ?? 2000.0;
  const groundHeightM = options.groundHeightM ?? 2000.0;

  const center5187X = GYEONGJU_ORIGIN_5187.x + centerXWorld;
  const center5187Y = GYEONGJU_ORIGIN_5187.y - centerZWorld;
  const xMin = center5187X - groundWidthM * 0.5;
  const yMax = center5187Y + groundHeightM * 0.5;
  const pixelW = groundWidthM / Math.max(1, width - 1);
  const pixelH = -groundHeightM / Math.max(1, height - 1);

  const z = new Array(width * height);
  let zMin = Infinity;
  let zMax = -Infinity;

  for (let row = 0; row < height; row++) {
    const wz = centerZWorld - groundHeightM * 0.5 + (row / Math.max(1, height - 1)) * groundHeightM;
    for (let col = 0; col < width; col++) {
      const wx = centerXWorld - groundWidthM * 0.5 + (col / Math.max(1, width - 1)) * groundWidthM;
      const elev = Number(sampleGyeongjuElevation(wx, wz).toFixed(2));
      z[row * width + col] = elev;
      if (elev < zMin) zMin = elev;
      if (elev > zMax) zMax = elev;
    }
  }

  return {
    schema: 'hgis.Terrain3dService.DemScene.v1',
    width,
    height,
    geotransform: [
      Number(xMin.toFixed(2)),
      Number(pixelW.toFixed(4)),
      0.0,
      Number(yMax.toFixed(2)),
      0.0,
      Number(pixelH.toFixed(4)),
    ],
    z,
    zMin: Number(zMin.toFixed(2)),
    zMax: Number(zMax.toFixed(2)),
    groundWidthM,
    groundHeightM,
    centerX: Number(center5187X.toFixed(2)),
    centerY: Number(center5187Y.toFixed(2)),
    projectionWkt: EPSG5187_WKT,
    crsAuthId: 'EPSG:5187',
  };
}

/**
 * Import a `Terrain3dService::DemScene` payload sent from `hgis` (`KaTerrain3dStudio`)
 * and build a 3D mesh buffer (`positions`, `normals`, `uvs`, `indices`) offloaded in the Engine.
 */
export function buildMeshFromHgisDemScene(demScene, zExaggeration = 1.35) {
  const { width, height, groundWidthM, groundHeightM, z, zMin, zMax } = demScene;
  const vCount = width * height;
  const positions = new Float32Array(vCount * 3);
  const normals = new Float32Array(vCount * 3);
  const uvs = new Float32Array(vCount * 2);

  const dx = groundWidthM / Math.max(1, width - 1);
  const dz = groundHeightM / Math.max(1, height - 1);

  for (let r = 0; r < height; r++) {
    const vz = -groundHeightM * 0.5 + r * dz;
    for (let c = 0; c < width; c++) {
      const vx = -groundWidthM * 0.5 + c * dx;
      const idx = r * width + c;
      const vy = (z[idx] ?? zMin) * zExaggeration;

      positions[idx * 3 + 0] = vx;
      positions[idx * 3 + 1] = vy;
      positions[idx * 3 + 2] = vz;

      uvs[idx * 2 + 0] = c / Math.max(1, width - 1);
      uvs[idx * 2 + 1] = 1.0 - r / Math.max(1, height - 1);
    }
  }

  // Compute central-difference normals
  for (let r = 0; r < height; r++) {
    for (let c = 0; c < width; c++) {
      const idx = r * width + c;
      const cL = Math.max(0, c - 1);
      const cR = Math.min(width - 1, c + 1);
      const rU = Math.max(0, r - 1);
      const rD = Math.min(height - 1, r + 1);

      const zL = positions[(r * width + cL) * 3 + 1];
      const zR = positions[(r * width + cR) * 3 + 1];
      const zU = positions[(rU * width + c) * 3 + 1];
      const zD = positions[(rD * width + c) * 3 + 1];

      const nx = -(zR - zL) / (2.0 * dx);
      const ny = 1.0;
      const nz = -(zD - zU) / (2.0 * dz);
      const len = Math.hypot(nx, ny, nz) || 1.0;

      normals[idx * 3 + 0] = nx / len;
      normals[idx * 3 + 1] = ny / len;
      normals[idx * 3 + 2] = nz / len;
    }
  }

  const indices = new Uint32Array((width - 1) * (height - 1) * 6);
  let ptr = 0;
  for (let r = 0; r < height - 1; r++) {
    for (let c = 0; c < width - 1; c++) {
      const i00 = r * width + c;
      const i10 = i00 + 1;
      const i01 = (r + 1) * width + c;
      const i11 = i01 + 1;
      indices[ptr++] = i00;
      indices[ptr++] = i01;
      indices[ptr++] = i10;
      indices[ptr++] = i10;
      indices[ptr++] = i01;
      indices[ptr++] = i11;
    }
  }

  return {
    width,
    height,
    groundWidthM,
    groundHeightM,
    zMin,
    zMax,
    positions,
    normals,
    uvs,
    indices,
  };
}

/**
 * Build the `Terrain3dService::composeSheet` specification (`입체지형 도면: 범례·축척자·방위·좌표계`)
 * so both the 3D companion app and `hgis` render a standardized archaeological submission plate.
 */
export function buildComposeSheetSpec(evaluationReport, cameraState = {}) {
  const visibleWidthM = cameraState.visibleWidthM ?? 1600.0;
  const yawDegFromNorth = cameraState.yawDegFromNorth ?? 18.0;
  const scaleSegmentM = scaleBarSegmentM(visibleWidthM);
  const zMin = 34.0;
  const zMax = 494.0;

  return {
    schema: 'hgis.Terrain3dService.ComposeSheet.v1',
    title: `${evaluationReport.surveyName} 3D 입체지형 및 매장유산 영향검토 도면`,
    crsLabel: 'EPSG:5187 (Korea 2000 / East Belt 2010)',
    demName: '경주 왕경 정밀 수치표고모형(DEM) + 위성정사영상 중첩',
    visibleWidthM: Math.round(visibleWidthM),
    scaleBarSegmentM: scaleSegmentM,
    scaleBarLabel: `${scaleSegmentM >= 1000 ? `${scaleSegmentM / 1000}km` : `${scaleSegmentM}m`}`,
    yawDegFromNorth: Number(yawDegFromNorth.toFixed(1)),
    elevationLegend: {
      zMinM: zMin,
      zMaxM: zMax,
      stops: [
        { elevM: 35, color: '#24526b', label: '35m (형산강·남천 하도)' },
        { elevM: 48, color: '#4d7c43', label: '48m (왕경 하안단구·선상지)' },
        { elevM: 110, color: '#8f7d46', label: '110m (낭산·월성·구릉대)' },
        { elevM: 260, color: '#7a583b', label: '260m (명활산·선도산 중턱)' },
        { elevM: 494, color: '#d6d1c7', label: '494m (남산 고위봉·금오봉)' },
      ],
    },
    archaeoSummaryBadge: {
      score: evaluationReport.assessmentSummary.archaeologicalPotentialScore,
      grade: evaluationReport.assessmentSummary.recommendedActionGrade,
      within500mCount: evaluationReport.proximityAnalysis.within500mCount,
      within1000mCount: evaluationReport.proximityAnalysis.totalWithin1000mCount,
    },
  };
}

/**
 * Convert an archaeological evaluation report into a multi-layer GeoJSON FeatureCollection
 * (`EPSG:4326` with `EPSG:5187` properties) for immediate drag-and-drop or live sync into `hgis` (QGIS).
 */
export function exportEvaluationToHgisGeoJson(report) {
  const features = [];

  // 1. Survey Center Point
  features.push({
    type: 'Feature',
    properties: {
      layer: '조사구역_중심점',
      name: report.surveyName,
      epsg5187X: report.location.epsg5187X,
      epsg5187Y: report.location.epsg5187Y,
      elevationM: report.geomorphology.elevationM,
      slopeDeg: report.geomorphology.slopeDeg,
      landform: report.geomorphology.landformNameKo,
      score: report.assessmentSummary.archaeologicalPotentialScore,
      grade: report.assessmentSummary.recommendedActionGrade,
    },
    geometry: {
      type: 'Point',
      coordinates: [report.location.lon, report.location.lat, report.location.worldY],
    },
  });

  // 2. 500m & 1,000m Round Buffer Polygons (`JoinStyle::Round`)
  const toWgsRing = (ring3D) =>
    ring3D.map(([wx, wy, wz]) => {
      const wgs = local3DToWgs84(wx, wz);
      return [Number(wgs.lon.toFixed(6)), Number(wgs.lat.toFixed(6)), Number(wy.toFixed(2))];
    });

  features.push({
    type: 'Feature',
    properties: {
      layer: '문화유산_영향검토_버퍼',
      radiusM: 500,
      joinStyle: 'Round',
      siteCount: report.proximityAnalysis.within500mCount,
    },
    geometry: {
      type: 'Polygon',
      coordinates: [toWgsRing(report.proximityAnalysis.buffer500m3D)],
    },
  });

  features.push({
    type: 'Feature',
    properties: {
      layer: '문화유산_영향검토_버퍼',
      radiusM: 1000,
      joinStyle: 'Round',
      siteCount: report.proximityAnalysis.totalWithin1000mCount,
    },
    geometry: {
      type: 'Polygon',
      coordinates: [toWgsRing(report.proximityAnalysis.buffer1000m3D)],
    },
  });

  // 3. Nearby Archaeological Sites within 1,000m
  for (const s of [
    ...report.proximityAnalysis.sitesWithin500m,
    ...report.proximityAnalysis.sites500to1000m,
  ]) {
    const wgs = local3DToWgs84(s.worldX, s.worldZ);
    features.push({
      type: 'Feature',
      properties: {
        layer: '인접_고고학유적',
        id: s.id,
        name: s.name,
        designation: s.designation,
        period: s.period,
        distanceM: s.distanceM,
        azimuthDeg: s.azimuthDeg,
        directionKo: s.directionKo,
        zoneBand: s.zoneBand,
      },
      geometry: {
        type: 'Point',
        coordinates: [Number(wgs.lon.toFixed(6)), Number(wgs.lat.toFixed(6)), s.siteElevationM],
      },
    });
  }

  // 4. 3D Viewshed Sightlines to 6 Royal Capital Landmarks
  for (const vs of report.viewshedAnalysis.landmarks) {
    const wgs1 = local3DToWgs84(vs.observerPoint[0], vs.observerPoint[2]);
    const wgs2 = local3DToWgs84(vs.targetPoint[0], vs.targetPoint[2]);
    features.push({
      type: 'Feature',
      properties: {
        layer: '왕경거점_3D가시권_시선축',
        landmarkName: vs.landmarkName,
        visible: vs.visible,
        distanceM: vs.distanceM,
        elevationAngleDeg: vs.elevationAngleDeg,
      },
      geometry: {
        type: 'LineString',
        coordinates: [
          [Number(wgs1.lon.toFixed(6)), Number(wgs1.lat.toFixed(6)), vs.observerPoint[1]],
          [Number(wgs2.lon.toFixed(6)), Number(wgs2.lat.toFixed(6)), vs.targetPoint[1]],
        ],
      },
    });
  }

  return {
    type: 'FeatureCollection',
    name: 'HGIS_3D_Gyeongju_Archaeology_Sync',
    crs: {
      type: 'name',
      properties: { name: 'urn:ogc:def:crs:OGC:1.3:CRS84' },
    },
    hgisMetadata: {
      companionEngine: 'hgis-3d-engine',
      nativeWorkCrs: 'EPSG:5187',
      synchronizedAt: report.timestamp,
      surveyName: report.surveyName,
    },
    features,
  };
}

/**
 * Full-lifecycle shared state manifest between `hgis` and `hgis-3d-engine`
 * covering Planning -> REA Decompilation -> Engine Offload -> Archaeological Assessment -> Sheet Export.
 */
export function buildLifecycleSyncManifest(report) {
  return {
    protocolVersion: '2.0.0',
    sharedBetween: ['hgis (gogohak1 C++/Qt6/QGIS)', 'hgis-3d-engine (Standalone 3D Engine & Studio)'],
    updatedAt: new Date().toISOString(),
    stages: [
      {
        stageId: '01_PLANNING',
        title: '조사구역 설정 및 좌표계(EPSG:5187) 동기화',
        status: 'SYNCED',
        surveyName: report.surveyName,
        epsg5187Center: [report.location.epsg5187X, report.location.epsg5187Y],
        wgs84Center: [report.location.lat, report.location.lon],
      },
      {
        stageId: '02_REA_DECOMPILATION',
        title: 'Blender 4.2.3 바이너리 디컴파일 및 웹 GIS 번들 REA 역공학',
        status: 'COMPLETED',
        blenderSymbolsExtracted: 468,
        eeveeShadersExtracted: 175,
        decompiledKernels: [
          'BLI_bvhtree_balance (0x14061e870)',
          'BLI_bvhtree_find_nearest_ex (0x14061eab0)',
          'bvhtree_kdop_axes_13 (0x1430a2de0)',
          'SKY_nishita_skymodel_precompute_sun (0x14217e500)',
          'datatoc_eevee_ambient_occlusion_lib_glsl (0x143d84dc0)',
        ],
      },
      {
        stageId: '03_ENGINE_OFFLOAD',
        title: '앱/엔진 분리 비동기 워커 오프로드 (UI 무렉 보장)',
        status: 'ACTIVE',
        architecture: 'Decoupled Engine Worker (src/engine/) <-> KaTheme UI App (src/app/)',
      },
      {
        stageId: '04_ARCHAEO_EVALUATION',
        title: '경주 왕경 500m/1km 버퍼·방리제·가시권·층위 정밀 평가',
        status: 'SYNCED',
        score: report.assessmentSummary.archaeologicalPotentialScore,
        recommendedGrade: report.assessmentSummary.recommendedActionGrade,
        within500mCount: report.proximityAnalysis.within500mCount,
        within1000mCount: report.proximityAnalysis.totalWithin1000mCount,
      },
      {
        stageId: '05_GIS_SHEET_EXPORT',
        title: 'HGIS 입체지형 도면(composeSheet) 및 GeoJSON/Blender 공유',
        status: 'READY',
        sharedEndpoints: [
          '/api/gis-sync/state',
          '/api/gis-sync/dem-scene',
          '/api/gis-sync/geojson',
          '/api/gis-sync/compose-sheet',
        ],
      },
    ],
  };
}
