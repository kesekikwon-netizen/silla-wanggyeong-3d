/**
 * @file server.js
 * @description Zero-dependency HTTP & Bidirectional HGIS Sharing Server for `hgis-3d-engine`.
 * Serves the KaTheme WebGL2 3D Companion App at http://localhost:4230 and exposes:
 * - `GET  /api/sites`
 * - `GET  /api/evaluate`
 * - `GET  /api/gis-sync/state`         (Lifecycle Planning -> Development -> Evaluation Sync)
 * - `POST /api/gis-sync/state`         (Push survey target from HGIS or 3D App & write shared_gis_bus/)
 * - `GET  /api/gis-sync/dem-scene`     (Export `Terrain3dService::DemScene` for HGIS `KaTerrain3dStudio`)
 * - `POST /api/gis-sync/dem-scene`     (Offload `Terrain3dService::DemScene` mesh build from HGIS)
 * - `GET  /api/gis-sync/geojson`       (Multi-layer GeoJSON in EPSG:5187 / EPSG:4326 for QGIS/HGIS)
 * - `GET  /api/gis-sync/compose-sheet` (`Terrain3dService::composeSheet` specification)
 */

import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { Hgis3DEngineCore } from './src/engine/engine_core.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const PORT = Number(process.env.PORT || 4230);
const SHARED_BUS_DIR = path.join(__dirname, 'shared_gis_bus');

const MIME_TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.geojson': 'application/geo+json; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.md': 'text/markdown; charset=utf-8',
};

let sharedEngine = null;
let latestSyncBundle = null;

function getEngine() {
  if (!sharedEngine) {
    sharedEngine = new Hgis3DEngineCore({
      extentM: 8400,
      maxDepth: 2,
      chunkGridSegments: 16,
      skirtDepthM: 18,
    }).initialize();
  }
  return sharedEngine;
}

function persistSyncBundleToDisk(bundle) {
  fs.mkdirSync(SHARED_BUS_DIR, { recursive: true });
  fs.writeFileSync(
    path.join(SHARED_BUS_DIR, 'hgis_shared_state.json'),
    JSON.stringify(
      {
        lifecycleManifest: bundle.lifecycleManifest,
        report: bundle.report,
        composeSheet: bundle.composeSheet,
      },
      null,
      2
    ),
    'utf8'
  );
  fs.writeFileSync(
    path.join(SHARED_BUS_DIR, 'hgis_archaeo_layers.geojson'),
    JSON.stringify(bundle.geojson, null, 2),
    'utf8'
  );
  fs.writeFileSync(
    path.join(SHARED_BUS_DIR, 'hgis_compose_sheet.json'),
    JSON.stringify(bundle.composeSheet, null, 2),
    'utf8'
  );
}

function getOrUpdateSyncBundle(targetParams = null) {
  if (!latestSyncBundle || targetParams) {
    const engine = getEngine();
    latestSyncBundle = engine.executeCommand(
      'EVALUATE_SITE',
      targetParams || {
        lat: 35.8347,
        lon: 129.2190,
        surveyName: '경주 왕경 중심지구(첨성대·월성·쪽샘) HGIS 공유 조사구역',
      }
    );
    persistSyncBundleToDisk(latestSyncBundle);
  }
  return latestSyncBundle;
}

function readJsonBody(req) {
  return new Promise((resolve, reject) => {
    let body = '';
    req.on('data', (chunk) => {
      body += chunk;
      if (body.length > 32 * 1024 * 1024) {
        reject(new Error('Payload too large'));
      }
    });
    req.on('end', () => {
      try {
        resolve(body ? JSON.parse(body) : {});
      } catch (e) {
        reject(e);
      }
    });
    req.on('error', reject);
  });
}

function sendJson(res, status, data) {
  res.writeHead(status, {
    'Content-Type': 'application/json; charset=utf-8',
    'Access-Control-Allow-Origin': '*',
    'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
    'Access-Control-Allow-Headers': 'Content-Type',
  });
  res.end(JSON.stringify(data, null, 2));
}

export function createGyeongjuServer() {
  return http.createServer(async (req, res) => {
    if (req.method === 'OPTIONS') {
      res.writeHead(204, {
        'Access-Control-Allow-Origin': '*',
        'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
        'Access-Control-Allow-Headers': 'Content-Type',
      });
      res.end();
      return;
    }

    const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);

    try {
      if (url.pathname === '/api/sites') {
        const engine = getEngine();
        sendJson(res, 200, {
          count: engine.sites.length,
          engineStats: engine.stats,
          sites: engine.sites,
        });
        return;
      }

      if (url.pathname === '/api/evaluate') {
        const lat = url.searchParams.has('lat') ? Number(url.searchParams.get('lat')) : undefined;
        const lon = url.searchParams.has('lon') ? Number(url.searchParams.get('lon')) : undefined;
        const worldX = url.searchParams.has('worldX') ? Number(url.searchParams.get('worldX')) : undefined;
        const worldZ = url.searchParams.has('worldZ') ? Number(url.searchParams.get('worldZ')) : undefined;
        const surveyName = url.searchParams.get('name') || undefined;

        const bundle = getOrUpdateSyncBundle({ lat, lon, worldX, worldZ, surveyName });
        sendJson(res, 200, bundle.report);
        return;
      }

      if (url.pathname === '/api/gis-sync/state') {
        if (req.method === 'POST') {
          const payload = await readJsonBody(req);
          const bundle = getOrUpdateSyncBundle(payload);
          sendJson(res, 200, {
            ok: true,
            synchronizedWith: 'shared_gis_bus/hgis_shared_state.json',
            computeMs: bundle.computeMs,
            lifecycleManifest: bundle.lifecycleManifest,
            report: bundle.report,
            composeSheet: bundle.composeSheet,
          });
          return;
        }
        const bundle = getOrUpdateSyncBundle();
        sendJson(res, 200, {
          ok: true,
          synchronizedWith: 'shared_gis_bus/hgis_shared_state.json',
          lifecycleManifest: bundle.lifecycleManifest,
          report: bundle.report,
          composeSheet: bundle.composeSheet,
        });
        return;
      }

      if (url.pathname === '/api/gis-sync/dem-scene') {
        const engine = getEngine();
        if (req.method === 'POST') {
          const payload = await readJsonBody(req);
          const out = engine.executeCommand('IMPORT_HGIS_DEM_SCENE', payload);
          sendJson(res, 200, {
            ok: true,
            computeMs: out.computeMs,
            vertexCount: out.mesh.positions.length / 3,
            indexCount: out.mesh.indices.length,
            zMin: out.mesh.zMin,
            zMax: out.mesh.zMax,
          });
          return;
        }
        const worldX = url.searchParams.has('worldX') ? Number(url.searchParams.get('worldX')) : 0;
        const worldZ = url.searchParams.has('worldZ') ? Number(url.searchParams.get('worldZ')) : 0;
        const out = engine.executeCommand('EXPORT_HGIS_DEM_SCENE', { worldX, worldZ });
        sendJson(res, 200, out.demScene);
        return;
      }

      if (url.pathname === '/api/gis-sync/geojson') {
        const bundle = getOrUpdateSyncBundle();
        sendJson(res, 200, bundle.geojson);
        return;
      }

      if (url.pathname === '/api/gis-sync/compose-sheet') {
        const bundle = getOrUpdateSyncBundle();
        sendJson(res, 200, bundle.composeSheet);
        return;
      }
    } catch (err) {
      sendJson(res, 500, { ok: false, error: err?.message || String(err) });
      return;
    }

    let filePath =
      url.pathname === '/'
        ? path.join(__dirname, 'public', 'index.html')
        : path.join(__dirname, url.pathname.replace(/^\/+/, ''));

    if (!filePath.startsWith(__dirname)) {
      res.writeHead(403);
      res.end('Forbidden');
      return;
    }

    if (!fs.existsSync(filePath) || fs.statSync(filePath).isDirectory()) {
      res.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' });
      res.end('Not Found');
      return;
    }

    const ext = path.extname(filePath).toLowerCase();
    res.writeHead(200, { 'Content-Type': MIME_TYPES[ext] || 'application/octet-stream' });
    fs.createReadStream(filePath).pipe(res);
  });
}

if (process.argv[1] && path.resolve(process.argv[1]) === path.resolve(fileURLToPath(import.meta.url))) {
  const server = createGyeongjuServer();
  server.listen(PORT, () => {
    console.log(`[hgis-3d-engine] KaTheme 3D Companion App & HGIS Sync Server listening at http://localhost:${PORT}`);
  });
}
