/**
 * @file engine_worker.js
 * @description Browser Web Worker entrypoint for the decoupled 3D Engine (`src/engine/`).
 * Keeps 100% of DEM generation, BVH raycasting, viewshed, and HGIS sync serialization off the UI thread.
 */

import { Hgis3DEngineCore } from './engine_core.js';

const engine = new Hgis3DEngineCore();

self.onmessage = (event) => {
  const { requestId, type, payload } = event.data || {};
  try {
    const result = engine.executeCommand(type, payload);
    self.postMessage({ requestId, ...result });
  } catch (err) {
    self.postMessage({
      requestId,
      ok: false,
      error: err?.message || String(err),
    });
  }
};
