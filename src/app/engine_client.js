/**
 * @file engine_client.js
 * @description Decoupled App-side RPC Client (`src/app/`) that communicates with the
 * heavy 3D Engine (`src/engine/engine_worker.js` or fallback in-process `Hgis3DEngineCore`).
 * Ensures the UI App never blocks during DEM generation, PBVH raycasting, viewshed, or HGIS sync.
 */

import { Hgis3DEngineCore } from '../engine/engine_core.js';

export class HgisEngineClient {
  constructor(options = {}) {
    this.useWorker = options.useWorker ?? (typeof Worker !== 'undefined');
    this.worker = null;
    this.fallbackEngine = null;
    this.nextRequestId = 1;
    this.pending = new Map();
    this.telemetry = {
      mode: 'INITIALIZING',
      jobsCompleted: 0,
      lastComputeMs: 0,
      totalComputeMs: 0,
    };

    if (this.useWorker && typeof Worker !== 'undefined') {
      try {
        this.worker = new Worker(new URL('../engine/engine_worker.js', import.meta.url), {
          type: 'module',
        });
        this.worker.onmessage = (event) => this._handleWorkerMessage(event.data);
        this.worker.onerror = () => {
          this._activateFallback();
        };
        this.telemetry.mode = 'WEB_WORKER_OFFLOAD';
      } catch (_) {
        this._activateFallback();
      }
    } else {
      this._activateFallback();
    }
  }

  _activateFallback() {
    this.useWorker = false;
    this.worker = null;
    if (!this.fallbackEngine) {
      this.fallbackEngine = new Hgis3DEngineCore();
    }
    this.telemetry.mode = 'DIRECT_ENGINE_CORE';
  }

  _handleWorkerMessage(msg) {
    const { requestId, ...rest } = msg || {};
    const entry = this.pending.get(requestId);
    if (!entry) return;
    this.pending.delete(requestId);

    this.telemetry.jobsCompleted++;
    this.telemetry.lastComputeMs = rest.computeMs ?? 0;
    this.telemetry.totalComputeMs = Number(
      (this.telemetry.totalComputeMs + this.telemetry.lastComputeMs).toFixed(2)
    );

    if (rest.ok === false) {
      entry.reject(new Error(rest.error || 'Engine worker command failed'));
    } else {
      entry.resolve(rest);
    }
  }

  /**
   * Dispatch a command to the decoupled 3D Engine asynchronously.
   * @param {string} type
   * @param {object} [payload]
   */
  async dispatch(type, payload = {}) {
    if (this.worker) {
      const requestId = this.nextRequestId++;
      return new Promise((resolve, reject) => {
        this.pending.set(requestId, { resolve, reject });
        this.worker.postMessage({ requestId, type, payload });
      });
    }

    this._activateFallback();
    const res = this.fallbackEngine.executeCommand(type, payload);
    this.telemetry.jobsCompleted++;
    this.telemetry.lastComputeMs = res.computeMs ?? 0;
    this.telemetry.totalComputeMs = Number(
      (this.telemetry.totalComputeMs + this.telemetry.lastComputeMs).toFixed(2)
    );
    if (res.ok === false) {
      throw new Error(res.error || 'Engine command failed');
    }
    return res;
  }

  terminate() {
    if (this.worker) {
      this.worker.terminate();
      this.worker = null;
    }
  }
}
