/**
 * @file engine_core.js
 * @description Decoupled Heavy 3D & Spatial Engine (`src/engine/`).
 * Strictly separated from the UI App (`src/app/`) so all heavy DEM tessellation,
 * decompiled Blender `BLI_kdopbvh` / `BKE_pbvh` / `SKY_nishita` / `EEVEE` GTAO compute,
 * archaeological viewshed raytracing, and `hgis` `Terrain3dService::DemScene` bridging
 * execute off the UI main thread.
 */

import { GyeongjuTerrainQuadtree, sampleGyeongjuElevation } from '../core/gyeongju_dem_terrain.js';
import { GyeongjuArchaeologyEvaluator } from '../core/archaeology_evaluator.js';
import {
  getGyeongjuArchaeologySites3D,
  generateSillaBangrijeGrid3D,
  generateGyeongju3DBuildings,
} from '../core/gyeongju_archaeology_db.js';
import {
  compute14DopHull,
  evaluateNishitaSunIrradiance,
  computeEeveeHorizonScanAO,
  triangulatePolygon2D,
} from './decompiled_blender_kernels.js';
import {
  exportToHgisDemScene,
  buildMeshFromHgisDemScene,
  buildComposeSheetSpec,
  exportEvaluationToHgisGeoJson,
  buildLifecycleSyncManifest,
} from '../bridge/hgis_share_protocol.js';
import {
  generateBlender42PythonScript,
  exportGyeongjuWavefrontObj,
  generateArchaeologicalMarkdownReport,
} from '../bridge/blender_exporter.js';

export class Hgis3DEngineCore {
  constructor(options = {}) {
    this.options = {
      extentM: options.extentM ?? 8400,
      maxDepth: options.maxDepth ?? 3,
      chunkGridSegments: options.chunkGridSegments ?? 20,
      skirtDepthM: options.skirtDepthM ?? 18,
    };
    this.terrainQuadtree = null;
    this.evaluator = null;
    this.sites = [];
    this.bangrijeGrid = null;
    this.buildings = [];
    this.stats = {
      initializedAt: null,
      buildTimeMs: 0,
      bvhNodeCount: 0,
      triangleCount: 0,
      kdop14RootHull: null,
      jobsProcessed: 0,
    };
  }

  initialize() {
    const t0 = performance.now();
    this.terrainQuadtree = new GyeongjuTerrainQuadtree(this.options).build();
    this.evaluator = new GyeongjuArchaeologyEvaluator(this.terrainQuadtree.bvh);
    this.sites = getGyeongjuArchaeologySites3D();
    this.bangrijeGrid = generateSillaBangrijeGrid3D();
    this.buildings = generateGyeongju3DBuildings(950);

    // Compute decompiled 14-DOP bounding hull (`BLI_kdopbvh: bvhtree_kdop_axes[13][3]`) for root scene
    const sampleCorners = this.sites.map((s) => [s.worldX, s.worldY, s.worldZ]);
    const kdop14 = compute14DopHull(sampleCorners);

    this.stats = {
      initializedAt: new Date().toISOString(),
      buildTimeMs: Number((performance.now() - t0).toFixed(2)),
      bvhNodeCount: this.terrainQuadtree.bvh.nodes.length,
      triangleCount: this.terrainQuadtree.bvh.triangles.length,
      kdop14RootHull: Array.from(kdop14).map((v) => Number(v.toFixed(2))),
      jobsProcessed: 0,
    };
    return this;
  }

  ensureInitialized() {
    if (!this.terrainQuadtree) {
      this.initialize();
    }
  }

  /**
   * Dispatch an engine command (`type`, `payload`) and return structured output + timing.
   */
  executeCommand(type, payload = {}) {
    this.ensureInitialized();
    const t0 = performance.now();
    this.stats.jobsProcessed++;

    switch (type) {
      case 'GET_INITIAL_SCENE_BUNDLE': {
        return {
          ok: true,
          computeMs: Number((performance.now() - t0).toFixed(2)),
          engineStats: this.stats,
          sites: this.sites,
          bangrijeGrid: this.bangrijeGrid,
          buildings: this.buildings,
          chunks: this.terrainQuadtree.leafChunks.map((c) => ({
            chunkId: c.chunkId,
            bounds: c.bounds,
            positions: c.positions,
            normals: c.normals,
            uvs: c.uvs,
            attributes: c.attributes,
            indices: c.indices,
          })),
        };
      }

      case 'EVALUATE_SITE': {
        const report = this.evaluator.evaluateSite(payload);
        const worldX = report.location.worldX;
        const worldZ = report.location.worldZ;
        const horizonAO = computeEeveeHorizonScanAO(sampleGyeongjuElevation, worldX, worldZ, 45.0, 4);
        const composeSheet = buildComposeSheetSpec(report, payload.cameraState);
        const geojson = exportEvaluationToHgisGeoJson(report);
        const lifecycleManifest = buildLifecycleSyncManifest(report);

        return {
          ok: true,
          computeMs: Number((performance.now() - t0).toFixed(2)),
          report: {
            ...report,
            decompiledKernelMetrics: {
              eeveeHorizonScanAO: horizonAO,
              bvhTotalNodes: this.stats.bvhNodeCount,
              kdop14Active: true,
            },
          },
          composeSheet,
          geojson,
          lifecycleManifest,
        };
      }

      case 'RAYCAST_TERRAIN': {
        const { origin, direction, maxDistance } = payload;
        const hit = this.terrainQuadtree.bvh.raycast(origin, direction, maxDistance ?? 30000);
        return {
          ok: true,
          computeMs: Number((performance.now() - t0).toFixed(2)),
          hit,
        };
      }

      case 'EXPORT_HGIS_DEM_SCENE': {
        const demScene = exportToHgisDemScene(payload);
        return {
          ok: true,
          computeMs: Number((performance.now() - t0).toFixed(2)),
          demScene,
        };
      }

      case 'IMPORT_HGIS_DEM_SCENE': {
        const mesh = buildMeshFromHgisDemScene(payload.demScene, payload.zExaggeration ?? 1.35);
        return {
          ok: true,
          computeMs: Number((performance.now() - t0).toFixed(2)),
          mesh,
        };
      }

      case 'COMPUTE_NISHITA_IRRADIANCE': {
        const res = evaluateNishitaSunIrradiance(
          payload.sunElevationRad ?? 0.72,
          payload.altitudeM ?? 45.0,
          payload.airDensity ?? 1.0,
          payload.dustDensity ?? 1.1,
          payload.ozoneDensity ?? 1.0
        );
        return {
          ok: true,
          computeMs: Number((performance.now() - t0).toFixed(2)),
          irradiance: res,
        };
      }

      case 'TRIANGULATE_SURVEY_POLYGON': {
        const indices = triangulatePolygon2D(payload.polygonRing || []);
        return {
          ok: true,
          computeMs: Number((performance.now() - t0).toFixed(2)),
          indices,
        };
      }

      case 'EXPORT_ARTIFACTS': {
        const report = payload.report || this.evaluator.evaluateSite(payload);
        return {
          ok: true,
          computeMs: Number((performance.now() - t0).toFixed(2)),
          blenderPy: generateBlender42PythonScript(report),
          markdownReport: generateArchaeologicalMarkdownReport(report),
          geojson: exportEvaluationToHgisGeoJson(report),
          objPreview: payload.includeObj ? exportGyeongjuWavefrontObj(this.terrainQuadtree) : null,
        };
      }

      default:
        return {
          ok: false,
          error: `Unknown engine command: ${type}`,
        };
    }
  }
}
