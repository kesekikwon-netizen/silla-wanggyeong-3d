/**
 * @file server.js
 * @description Zero-dependency HTTP & REST API server for the Gyeongju 3D Terrain & Archaeological Studio.
 * Serves the WebGL2 3D Viewport at http://localhost:4230 and exposes `/api/evaluate` & `/api/sites`.
 */

import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { GyeongjuTerrainQuadtree } from './src/core/gyeongju_dem_terrain.js';
import { GyeongjuArchaeologyEvaluator } from './src/core/archaeology_evaluator.js';
import { getGyeongjuArchaeologySites3D } from './src/core/gyeongju_archaeology_db.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const PORT = Number(process.env.PORT || 4230);

const MIME_TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.md': 'text/markdown; charset=utf-8',
};

let sharedEvaluator = null;
function getEvaluator() {
  if (!sharedEvaluator) {
    const qt = new GyeongjuTerrainQuadtree({ extentM: 8400, maxDepth: 2, chunkGridSegments: 16 }).build();
    sharedEvaluator = new GyeongjuArchaeologyEvaluator(qt.bvh);
  }
  return sharedEvaluator;
}

export function createGyeongjuServer() {
  return http.createServer((req, res) => {
    const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);

    if (url.pathname === '/api/sites') {
      const sites = getGyeongjuArchaeologySites3D();
      res.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8' });
      res.end(JSON.stringify({ count: sites.length, sites }, null, 2));
      return;
    }

    if (url.pathname === '/api/evaluate') {
      const lat = url.searchParams.has('lat') ? Number(url.searchParams.get('lat')) : undefined;
      const lon = url.searchParams.has('lon') ? Number(url.searchParams.get('lon')) : undefined;
      const worldX = url.searchParams.has('worldX') ? Number(url.searchParams.get('worldX')) : undefined;
      const worldZ = url.searchParams.has('worldZ') ? Number(url.searchParams.get('worldZ')) : undefined;
      const name = url.searchParams.get('name') || undefined;

      const report = getEvaluator().evaluateSite({ lat, lon, worldX, worldZ, surveyName: name });
      res.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8' });
      res.end(JSON.stringify(report, null, 2));
      return;
    }

    let filePath =
      url.pathname === '/'
        ? path.join(__dirname, 'public', 'index.html')
        : path.join(__dirname, url.pathname.replace(/^\/+/, ''));

    // Prevent path traversal outside repo root
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
    console.log(`[hgis-3d-engine] Gyeongju 3D Archaeological Studio listening at http://localhost:${PORT}`);
  });
}
