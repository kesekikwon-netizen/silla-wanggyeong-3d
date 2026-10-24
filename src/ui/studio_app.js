/**
 * @file studio_app.js
 * @description KaTheme 3D Companion App (`src/ui/`) powered by the decoupled
 * Blender 4.2 REA Engine (`HgisEngineClient` -> `src/engine/engine_worker.js`)
 * and synchronized bidirectionally with the main HGIS application (`shared_gis_bus/`).
 */

import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

import { local3DToWgs84, wgs84ToSlippyTile } from '../core/gyeongju_crs.js';
import {
  computeGyeongjuSunPosition,
  NISHITA_SKY_VERTEX_SHADER,
  NISHITA_SKY_FRAGMENT_SHADER,
  EEVEE_TERRAIN_VERTEX_SHADER,
  EEVEE_TERRAIN_FRAGMENT_SHADER,
} from '../core/eevee_sky_shader.js';
import { applyKaThemeToDocument } from '../app/ka_theme.js';
import { HgisEngineClient } from '../app/engine_client.js';

// --- 1. Initialize KaTheme & Decoupled Engine Client ---
const themeState = {
  themeKey: 'default',
  highContrast: false,
  largeText: false,
};
applyKaThemeToDocument(document.documentElement, themeState);

const engineClient = new HgisEngineClient({ useWorker: true });

// --- 2. Setup WebGL2 Renderer, Scene, Camera, OrbitControls ---
const canvas = document.getElementById('viewport-canvas');
const renderer = new THREE.WebGLRenderer({
  canvas,
  antialias: true,
  preserveDrawingBuffer: true, // Required for composeSheet PNG export
  powerPreference: 'high-performance',
});
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.setSize(window.innerWidth, window.innerHeight - 80);

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(
  48,
  window.innerWidth / Math.max(1, window.innerHeight - 80),
  5,
  35000
);
camera.position.set(-680, 560, 1450);

const controls = new OrbitControls(camera, renderer.domElement);
controls.target.set(220, 55, 120);
controls.enableDamping = true;
controls.dampingFactor = 0.06;
controls.maxPolarAngle = Math.PI * 0.485;
controls.minDistance = 40;
controls.maxDistance = 12000;
controls.update();

// --- 3. Nishita Physical Sky Dome (`SKY_nishita_skymodel_precompute_sun`) ---
const sunState = computeGyeongjuSunPosition(14.5, 265);
const skyUniforms = {
  uSunDirection: { value: new THREE.Vector3(...sunState.sunDirection) },
  uTurbidity: { value: 1.15 },
};
const skyGeo = new THREE.SphereGeometry(18000, 32, 20);
const skyMat = new THREE.ShaderMaterial({
  vertexShader: NISHITA_SKY_VERTEX_SHADER,
  fragmentShader: NISHITA_SKY_FRAGMENT_SHADER,
  uniforms: skyUniforms,
  side: THREE.BackSide,
  depthWrite: false,
});
scene.add(new THREE.Mesh(skyGeo, skyMat));

const dirLight = new THREE.DirectionalLight(0xfff4e0, 2.4);
dirLight.position.set(
  sunState.sunDirection[0] * 3000,
  sunState.sunDirection[1] * 3000,
  sunState.sunDirection[2] * 3000
);
scene.add(dirLight);
scene.add(new THREE.HemisphereLight(0x9ec5ff, 0x3d352e, 0.85));

// --- 4. Terrain Material & Scene Groups ---
const terrainUniforms = {
  uSatelliteMap: { value: null },
  uHasSatelliteMap: { value: 0.0 },
  uSunDirection: { value: new THREE.Vector3(...sunState.sunDirection) },
  uShaderMode: { value: 0 },
  uElevationExaggeration: { value: 1.35 },
  uSelectedSiteWorldPos: { value: new THREE.Vector3(0, 44.5, 0) },
  uShowRadiusRings: { value: 1.0 },
};

const terrainMaterial = new THREE.ShaderMaterial({
  vertexShader: EEVEE_TERRAIN_VERTEX_SHADER,
  fragmentShader: EEVEE_TERRAIN_FRAGMENT_SHADER,
  uniforms: terrainUniforms,
});

const terrainGroup = new THREE.Group();
terrainGroup.scale.set(1, 1.35, 1);
scene.add(terrainGroup);

const worldScaleRoot = new THREE.Group();
worldScaleRoot.scale.set(1, 1.35, 1);
scene.add(worldScaleRoot);

const buildingGroup = new THREE.Group();
const sillaReconstructionGroup = new THREE.Group();
const monumentPermanentGroup = new THREE.Group();
const evaluationOverlayGroup = new THREE.Group();

worldScaleRoot.add(buildingGroup);
worldScaleRoot.add(sillaReconstructionGroup);
worldScaleRoot.add(monumentPermanentGroup);
worldScaleRoot.add(evaluationOverlayGroup);

let latestBundle = null;

// --- 5. Bootstrap Scene Asynchronously from Decoupled Engine Worker ---
async function initializeFromEngineWorker() {
  const initData = await engineClient.dispatch('GET_INITIAL_SCENE_BUNDLE');
  document.getElementById('chip-engine-mode').textContent =
    `엔진 분리: ${engineClient.telemetry.mode === 'WEB_WORKER_OFFLOAD' ? 'Web Worker 오프로드' : 'Direct Engine'}`;
  document.getElementById('stat-bvh-nodes').textContent =
    `${initData.engineStats.bvhNodeCount.toLocaleString()} (14-DOP 활성)`;
  document.getElementById('stat-compute-ms').textContent = `${initData.computeMs}`;

  // 5.1 Build Quadtree Terrain Chunks
  for (const chunk of initData.chunks) {
    const geom = new THREE.BufferGeometry();
    geom.setAttribute('position', new THREE.BufferAttribute(new Float32Array(chunk.positions), 3));
    geom.setAttribute('normal', new THREE.BufferAttribute(new Float32Array(chunk.normals), 3));
    geom.setAttribute('uv', new THREE.BufferAttribute(new Float32Array(chunk.uvs), 2));
    geom.setAttribute('aArchaeoAttr', new THREE.BufferAttribute(new Float32Array(chunk.attributes), 4));
    geom.setIndex(new THREE.BufferAttribute(new Uint32Array(chunk.indices), 1));
    geom.computeBoundingSphere();
    terrainGroup.add(new THREE.Mesh(geom, terrainMaterial));
  }

  // 5.2 Build Instanced Contemporary Gyeongju Buildings
  buildInstancedBuildings(initData.buildings);

  // 5.3 Build 3D Silla Monuments & Bangrije Grid
  buildSillaMonumentsAndGrid(initData.sites, initData.bangrijeGrid);

  // 5.4 Populate Preset Buttons
  populatePresetButtons(initData.sites);

  // 5.5 Load Satellite Imagery Tiles in Background
  loadGyeongjuSatelliteOrthoTexture(4200);

  // 5.6 Run Initial Evaluation & Sync with HGIS
  await evaluateAndSyncSite({
    worldX: 180,
    worldZ: -80,
    surveyName: '경주 인왕동(첨성대·쪽샘·월성 사이) HGIS 공유 조사구역',
  });
}

function buildInstancedBuildings(buildings) {
  const boxGeo = new THREE.BoxGeometry(1, 1, 1);
  boxGeo.translate(0, 0.5, 0);
  const bldMat = new THREE.MeshStandardMaterial({ roughness: 0.75, metalness: 0.12 });
  const instMesh = new THREE.InstancedMesh(boxGeo, bldMat, buildings.length);
  const dummy = new THREE.Object3D();
  const color = new THREE.Color();

  buildings.forEach((b, i) => {
    dummy.position.set(b.worldX, b.worldY, b.worldZ);
    dummy.rotation.set(0, b.rotationRad, 0);
    dummy.scale.set(b.widthM, b.heightM, b.depthM);
    dummy.updateMatrix();
    instMesh.setMatrixAt(i, dummy.matrix);
    if (b.isHanok) {
      color.setHSL(0.08, 0.22, 0.28 + (i % 5) * 0.02);
    } else {
      color.setHSL(0.58, 0.08, 0.58 + (i % 7) * 0.03);
    }
    instMesh.setColorAt(i, color);
  });
  instMesh.instanceMatrix.needsUpdate = true;
  if (instMesh.instanceColor) instMesh.instanceColor.needsUpdate = true;
  buildingGroup.add(instMesh);
}

function buildSillaMonumentsAndGrid(sites, bangrijeGrid) {
  const tumulusMat = new THREE.MeshStandardMaterial({ color: 0x5c8a3c, roughness: 0.88, metalness: 0.02 });
  const royalWoodMat = new THREE.MeshStandardMaterial({ color: 0x9e3628, roughness: 0.55, metalness: 0.15 });
  const stoneMat = new THREE.MeshStandardMaterial({ color: 0xb8b2a6, roughness: 0.8 });
  const goldFinialMat = new THREE.MeshStandardMaterial({
    color: 0xfbbf24,
    roughness: 0.25,
    metalness: 0.85,
    emissive: 0x553300,
  });

  for (const s of sites) {
    if (s.archetype3D === 'tumulus_double') {
      const r = s.footprintRadiusM * 0.68;
      const southGeo = new THREE.SphereGeometry(r, 28, 16, 0, Math.PI * 2, 0, Math.PI * 0.5);
      const southMound = new THREE.Mesh(southGeo, tumulusMat);
      southMound.scale.set(1, s.heightM / r, 1);
      southMound.position.set(s.worldX, s.worldY, s.worldZ + 24);
      monumentPermanentGroup.add(southMound);

      const northMound = southMound.clone();
      northMound.position.set(s.worldX, s.worldY, s.worldZ - 24);
      northMound.scale.multiplyScalar(0.94);
      monumentPermanentGroup.add(northMound);
    } else if (s.archetype3D === 'tumulus_single' || s.archetype3D === 'tumulus_cluster') {
      const r = Math.min(44, s.footprintRadiusM);
      const moundGeo = new THREE.SphereGeometry(r, 24, 14, 0, Math.PI * 2, 0, Math.PI * 0.5);
      const mound = new THREE.Mesh(moundGeo, tumulusMat);
      mound.scale.set(1, s.heightM / r, 1);
      mound.position.set(s.worldX, s.worldY, s.worldZ);
      monumentPermanentGroup.add(mound);
    } else if (s.archetype3D === 'observatory_tower') {
      const cylGeo = new THREE.CylinderGeometry(1.55, 2.6, s.heightM, 16);
      const mesh = new THREE.Mesh(cylGeo, stoneMat);
      mesh.position.set(s.worldX, s.worldY + s.heightM * 0.5, s.worldZ);
      monumentPermanentGroup.add(mesh);
    } else if (s.archetype3D === 'pagoda_9story') {
      const pagodaGroup = new THREE.Group();
      pagodaGroup.position.set(s.worldX, s.worldY, s.worldZ);
      let currY = 0;
      const basePlatform = new THREE.Mesh(new THREE.BoxGeometry(36, 3.2, 36), stoneMat);
      basePlatform.position.y = 1.6;
      pagodaGroup.add(basePlatform);
      currY += 3.2;

      for (let tier = 0; tier < 9; tier++) {
        const ratio = 1.0 - tier * 0.068;
        const w = 22.2 * ratio;
        const h = 5.4 * (1.0 - tier * 0.03);
        const body = new THREE.Mesh(new THREE.BoxGeometry(w, h, w), royalWoodMat);
        body.position.y = currY + h * 0.5;
        pagodaGroup.add(body);

        const roof = new THREE.Mesh(new THREE.ConeGeometry(w * 0.88, 2.2, 4), tumulusMat);
        roof.rotation.y = Math.PI * 0.25;
        roof.position.y = currY + h + 0.8;
        pagodaGroup.add(roof);
        currY += h + 1.1;
      }
      const spire = new THREE.Mesh(new THREE.CylinderGeometry(0.35, 0.9, 18.0, 12), goldFinialMat);
      spire.position.y = currY + 9.0;
      pagodaGroup.add(spire);
      sillaReconstructionGroup.add(pagodaGroup);
    } else if (s.archetype3D === 'fortress_palace') {
      const ringGeo = new THREE.TorusGeometry(s.footprintRadiusM * 0.65, 6.5, 10, 48);
      ringGeo.rotateX(Math.PI * 0.5);
      const wall = new THREE.Mesh(ringGeo, stoneMat);
      wall.scale.set(1.35, 1.0, 0.62);
      wall.position.set(s.worldX, s.worldY + 4.5, s.worldZ);
      monumentPermanentGroup.add(wall);
    } else {
      const hall = new THREE.Mesh(new THREE.BoxGeometry(28, s.reconstructedHeightM, 18), royalWoodMat);
      hall.position.set(s.worldX, s.worldY + s.reconstructedHeightM * 0.5, s.worldZ);
      sillaReconstructionGroup.add(hall);
    }
  }

  const mainRoadMat = new THREE.LineBasicMaterial({ color: 0xfbbf24, transparent: true, opacity: 0.65 });
  const subRoadMat = new THREE.LineBasicMaterial({ color: 0xd97706, transparent: true, opacity: 0.32 });

  for (const road of bangrijeGrid.roads) {
    const pts = road.points.map(([x, y, z]) => new THREE.Vector3(x, y, z));
    const lineGeo = new THREE.BufferGeometry().setFromPoints(pts);
    sillaReconstructionGroup.add(
      new THREE.Line(lineGeo, road.isMainRoyalAvenue ? mainRoadMat : subRoadMat)
    );
  }
}

async function loadGyeongjuSatelliteOrthoTexture(halfExtentM = 4200) {
  try {
    const z = 14;
    const nwWgs = local3DToWgs84(-halfExtentM, -halfExtentM);
    const seWgs = local3DToWgs84(halfExtentM, halfExtentM);
    const tNW = wgs84ToSlippyTile(nwWgs.lat, nwWgs.lon, z);
    const tSE = wgs84ToSlippyTile(seWgs.lat, seWgs.lon, z);

    const minX = Math.min(tNW.x, tSE.x);
    const maxX = Math.max(tNW.x, tSE.x);
    const minY = Math.min(tNW.y, tSE.y);
    const maxY = Math.max(tNW.y, tSE.y);

    const cols = maxX - minX + 1;
    const rows = maxY - minY + 1;
    const tileCanvas = document.createElement('canvas');
    tileCanvas.width = cols * 256;
    tileCanvas.height = rows * 256;
    const ctx = tileCanvas.getContext('2d');

    const loadTile = (tx, ty) =>
      new Promise((resolve) => {
        const img = new Image();
        img.crossOrigin = 'anonymous';
        img.onload = () => {
          ctx.drawImage(img, (tx - minX) * 256, (ty - minY) * 256, 256, 256);
          resolve(true);
        };
        img.onerror = () => resolve(false);
        img.src = `https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/${z}/${ty}/${tx}`;
      });

    const jobs = [];
    for (let ty = minY; ty <= maxY; ty++) {
      for (let tx = minX; tx <= maxX; tx++) {
        jobs.push(loadTile(tx, ty));
      }
    }
    const results = await Promise.all(jobs);
    if (results.some(Boolean)) {
      const tex = new THREE.CanvasTexture(tileCanvas);
      tex.wrapS = THREE.ClampToEdgeWrapping;
      tex.wrapT = THREE.ClampToEdgeWrapping;
      tex.needsUpdate = true;
      terrainUniforms.uSatelliteMap.value = tex;
      terrainUniforms.uHasSatelliteMap.value = 1.0;
    }
  } catch (_) {
    // Offline fallback remains active
  }
}

function populatePresetButtons(sites) {
  const presetGrid = document.getElementById('preset-sites-grid');
  presetGrid.innerHTML = '';
  sites.slice(0, 12).forEach((site) => {
    const btn = document.createElement('button');
    btn.className = 'ka-btn preset-btn';
    btn.textContent = site.name.replace('경주 ', '');
    btn.title = `${site.name} (${site.designation})`;
    btn.addEventListener('click', async () => {
      await evaluateAndSyncSite({
        worldX: site.worldX,
        worldZ: site.worldZ,
        surveyName: `${site.name} 정밀 평가구역`,
      });
      const zScale = terrainUniforms.uElevationExaggeration.value;
      controls.target.set(site.worldX, site.worldY * zScale, site.worldZ);
      camera.position.set(site.worldX - 360, site.worldY * zScale + 290, site.worldZ + 520);
      controls.update();
    });
    presetGrid.appendChild(btn);
  });
}

// --- 6. Evaluate Site via Engine Worker & Sync with HGIS ---
async function evaluateAndSyncSite(targetPayload) {
  const bundle = await engineClient.dispatch('EVALUATE_SITE', targetPayload);
  latestBundle = bundle;

  document.getElementById('stat-compute-ms').textContent = `${bundle.computeMs}`;
  document.getElementById('stat-horizon-ao').textContent =
    `${bundle.report.decompiledKernelMetrics?.eeveeHorizonScanAO ?? 0.92}`;

  updateLifecycleStagesUI(bundle.lifecycleManifest);
  updateRightEvaluationUI(bundle.report);
  renderEvaluationOverlays(bundle.report);

  // Push live state to `/api/gis-sync/state` so HGIS (`shared_gis_bus/`) stays synchronized
  try {
    await fetch('/api/gis-sync/state', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(targetPayload),
    });
    document.getElementById('lbl-sync-status').textContent = 'HGIS 동기화됨';
  } catch (_) {
    document.getElementById('lbl-sync-status').textContent = '로컬 워커 활성';
  }
}

function updateLifecycleStagesUI(manifest) {
  const container = document.getElementById('lifecycle-stages-list');
  if (!manifest || !manifest.stages) return;
  container.innerHTML = manifest.stages
    .map(
      (st) => `
    <div class="lifecycle-step">
      <span><b>${st.stageId.slice(0, 2)}</b> · ${st.title}</span>
      <span class="ka-chip ka-chip-ok" style="height:18px; font-size:10px;">${st.status}</span>
    </div>`
    )
    .join('');
}

function updateRightEvaluationUI(report) {
  document.getElementById('eval-site-title').textContent = report.surveyName;
  document.getElementById('eval-coords').textContent =
    `WGS84 ${report.location.lat.toFixed(5)}°N, ${report.location.lon.toFixed(5)}°E | EPSG:5187 (${report.location.epsg5187X.toFixed(0)}, ${report.location.epsg5187Y.toFixed(0)})`;
  document.getElementById('eval-grade').textContent = report.assessmentSummary.recommendedActionGrade;
  document.getElementById('eval-score').textContent = report.assessmentSummary.archaeologicalPotentialScore;
  document.getElementById('eval-risk-chip').textContent = report.assessmentSummary.riskLevel;

  const g = report.geomorphology;
  const b = report.bangrijeEvaluation;
  document.getElementById('eval-geomorph').innerHTML = `
    <div>• <b>지형분류</b>: ${g.landformNameKo}</div>
    <div>• <b>해발 / 경사도 / 사면향 / TPI</b>: <b>${g.elevationM}m</b> / <b>${g.slopeDeg}°</b> / ${g.aspectDeg}° / ${g.tpi}</div>
    <div>• <b>인접 하천</b>: ${g.nearestRiverName} (직선 <b>${g.nearestRiverDistM}m</b>)</div>
    <div>• <b>왕경 방리제(坊里制)</b>: ${b.bangSectorLabel} (고대 도로선 이격 <b>${b.minRoadDistM}m</b>)</div>
  `;
  document.getElementById('eval-synthesis').textContent = report.assessmentSummary.synthesisOpinion;

  const prox = report.proximityAnalysis;
  document.getElementById('eval-nearby-count').textContent =
    `500m: ${prox.within500mCount} / 1km: ${prox.totalWithin1000mCount}`;

  const allNearby = [...prox.sitesWithin500m, ...prox.sites500to1000m];
  const nearbyTbody = document.getElementById('eval-nearby-tbody');
  if (allNearby.length === 0) {
    nearbyTbody.innerHTML = `<tr><td colspan="3" style="color:var(--ka-ink-muted);">반경 1,000m 이내 주요 국가지정 유적 없음 (최인접: ${prox.nearestSiteName} ${prox.nearestSiteDistanceM}m)</td></tr>`;
  } else {
    nearbyTbody.innerHTML = allNearby
      .map(
        (s) => `
      <tr>
        <td><b>${s.name}</b></td>
        <td style="color:${s.distanceM <= 500 ? 'var(--ka-danger)' : 'var(--ka-warn)'}; font-weight:700;">${s.distanceM}m (${s.directionKo})</td>
        <td>${s.zoneBand.split(' ')[0]}</td>
      </tr>`
      )
      .join('');
  }

  const vs = report.viewshedAnalysis;
  document.getElementById('eval-viewshed-count').textContent =
    `${vs.visibleCount} / ${vs.totalLandmarks} 가시`;
  document.getElementById('eval-viewshed-tbody').innerHTML = vs.landmarks
    .map(
      (v) => `
    <tr>
      <td>${v.landmarkName}</td>
      <td style="color:${v.visible ? 'var(--ka-ok)' : 'var(--ka-danger)'}; font-weight:700;">${v.visible ? '● 가시(Visible)' : '× 지형차폐'}</td>
      <td>${v.distanceM}m (${v.elevationAngleDeg}°)</td>
    </tr>`
    )
    .join('');

  document.getElementById('eval-stratigraphy').innerHTML = report.virtualTrenches.predictedStratigraphy
    .map(
      (l) => `
    <div class="strat-box">
      <div class="strat-bar" style="background:${l.colorHex};"></div>
      <div>
        <div style="font-weight:700;">${l.layerCode} · ${l.nameKo} <span style="color:var(--ka-accent-deep);">(GL -${l.depthRangeM[0]}~-${l.depthRangeM[1]}m)</span></div>
        <div style="color:var(--ka-ink-muted);">예상 유구·유물: ${l.expectedFinds}</div>
      </div>
    </div>`
    )
    .join('');
}

function renderEvaluationOverlays(report) {
  while (evaluationOverlayGroup.children.length > 0) {
    const child = evaluationOverlayGroup.children.pop();
    if (child.geometry) child.geometry.dispose();
  }

  const loc = report.location;
  terrainUniforms.uSelectedSiteWorldPos.value.set(loc.worldX, loc.worldY, loc.worldZ);

  const beacon = new THREE.Mesh(
    new THREE.CylinderGeometry(2.5, 2.5, 95, 16),
    new THREE.MeshBasicMaterial({ color: 0xef4444 })
  );
  beacon.position.set(loc.worldX, loc.worldY + 47.5, loc.worldZ);
  evaluationOverlayGroup.add(beacon);

  if (document.getElementById('chk-buffers').checked) {
    const pts500 = report.proximityAnalysis.buffer500m3D.map(([x, y, z]) => new THREE.Vector3(x, y + 1.5, z));
    evaluationOverlayGroup.add(
      new THREE.Line(
        new THREE.BufferGeometry().setFromPoints(pts500),
        new THREE.LineBasicMaterial({ color: 0xef4444 })
      )
    );
    const pts1000 = report.proximityAnalysis.buffer1000m3D.map(([x, y, z]) => new THREE.Vector3(x, y + 1.5, z));
    evaluationOverlayGroup.add(
      new THREE.Line(
        new THREE.BufferGeometry().setFromPoints(pts1000),
        new THREE.LineBasicMaterial({ color: 0xfbbf24 })
      )
    );
  }

  if (document.getElementById('chk-viewshed').checked) {
    for (const vs of report.viewshedAnalysis.landmarks) {
      const p1 = new THREE.Vector3(...vs.observerPoint);
      p1.y += 2.0;
      const p2 = new THREE.Vector3(...vs.targetPoint);
      evaluationOverlayGroup.add(
        new THREE.Line(
          new THREE.BufferGeometry().setFromPoints([p1, p2]),
          new THREE.LineBasicMaterial({
            color: vs.visible ? 0x10b981 : 0xef4444,
            transparent: true,
            opacity: vs.visible ? 0.85 : 0.45,
          })
        )
      );
    }
  }

  const trenchMat = new THREE.MeshStandardMaterial({ color: 0x8c5828, roughness: 0.9 });
  for (const tr of report.virtualTrenches.trenchSpecs) {
    const trMesh = new THREE.Mesh(new THREE.BoxGeometry(tr.lengthM, tr.depthM, tr.widthM), trenchMat);
    trMesh.position.set(tr.worldX, tr.worldY + 0.8, tr.worldZ);
    trMesh.rotation.y = tr.rotationRad;
    evaluationOverlayGroup.add(trMesh);
  }
}

// --- 7. Interactive Double-Click Raycast via Engine Worker (`RAYCAST_TERRAIN`) ---
canvas.addEventListener('dblclick', async (ev) => {
  const rect = canvas.getBoundingClientRect();
  const ndcX = ((ev.clientX - rect.left) / rect.width) * 2 - 1;
  const ndcY = -((ev.clientY - rect.top) / rect.height) * 2 + 1;

  const raycaster = new THREE.Raycaster();
  raycaster.setFromCamera(new THREE.Vector2(ndcX, ndcY), camera);

  const zScale = terrainUniforms.uElevationExaggeration.value;
  const origin = [
    raycaster.ray.origin.x,
    raycaster.ray.origin.y / zScale,
    raycaster.ray.origin.z,
  ];
  const direction = [
    raycaster.ray.direction.x,
    raycaster.ray.direction.y / zScale,
    raycaster.ray.direction.z,
  ];

  const res = await engineClient.dispatch('RAYCAST_TERRAIN', { origin, direction, maxDistance: 30000 });
  document.getElementById('stat-raycast-visits').textContent = `${res.hit.nodeVisits}`;
  if (res.hit.hit && res.hit.point) {
    await evaluateAndSyncSite({
      worldX: res.hit.point[0],
      worldZ: res.hit.point[2],
    });
  }
});

// --- 8. `Terrain3dService::composeSheet` High-Res PNG Export (Matches HGIS `KaTerrain3dStudio::exportSheet`) ---
function exportComposeSheetPng() {
  if (!latestBundle) return;
  renderer.render(scene, camera);

  const spec = latestBundle.composeSheet;
  const report = latestBundle.report;
  const outW = 1600;
  const outH = 1050;
  const sheetCanvas = document.createElement('canvas');
  sheetCanvas.width = outW;
  sheetCanvas.height = outH;
  const ctx = sheetCanvas.getContext('2d');

  // 1. White Plate Background & Border (`KaTheme: surface / plateRule`)
  ctx.fillStyle = '#FFFFFF';
  ctx.fillRect(0, 0, outW, outH);
  ctx.strokeStyle = '#1D2733';
  ctx.lineWidth = 3;
  ctx.strokeRect(16, 16, outW - 32, outH - 32);

  // 2. Header Title Strip
  ctx.fillStyle = '#0B3A63';
  ctx.fillRect(18, 18, outW - 36, 58);
  ctx.fillStyle = '#FFFFFF';
  ctx.font = "bold 22px 'Malgun Gothic', sans-serif";
  ctx.fillText(spec.title, 36, 54);
  ctx.font = "14px 'Malgun Gothic', sans-serif";
  ctx.fillText(`${spec.crsLabel} | 포장 가능성: ${spec.archaeoSummaryBadge.score}점 (${spec.archaeoSummaryBadge.grade})`, outW - 620, 52);

  // 3. 3D Viewport Frame
  const viewX = 32, viewY = 90, viewW = outW - 64, viewH = 780;
  ctx.drawImage(canvas, viewX, viewY, viewW, viewH);
  ctx.strokeStyle = '#B8C4CF';
  ctx.lineWidth = 2;
  ctx.strokeRect(viewX, viewY, viewW, viewH);

  // 4. Footer Legend, Metric Scale Bar (`scaleBarSegmentM`), North Arrow, and Coordinates
  const footY = 885;
  ctx.fillStyle = '#F4F6F8';
  ctx.fillRect(32, footY, outW - 64, 132);
  ctx.strokeStyle = '#DCE3EA';
  ctx.strokeRect(32, footY, outW - 64, 132);

  // Elevation Legend Stops
  ctx.fillStyle = '#1D2733';
  ctx.font = "bold 15px 'Malgun Gothic', sans-serif";
  ctx.fillText('표고 범례 (Elevation DEM)', 52, footY + 30);
  spec.elevationLegend.stops.forEach((st, idx) => {
    const sx = 52 + idx * 175;
    ctx.fillStyle = st.color;
    ctx.fillRect(sx, footY + 44, 24, 18);
    ctx.strokeStyle = '#1D2733';
    ctx.strokeRect(sx, footY + 44, 24, 18);
    ctx.fillStyle = '#1D2733';
    ctx.font = "13px 'Malgun Gothic', sans-serif";
    ctx.fillText(st.label, sx + 30, footY + 58);
  });

  // Metric Scale Bar
  const barX = 52, barY = footY + 95, barPx = 220;
  ctx.fillStyle = '#1D2733';
  ctx.fillRect(barX, barY, barPx * 0.5, 10);
  ctx.strokeRect(barX, barY, barPx, 10);
  ctx.font = "bold 14px 'Malgun Gothic', sans-serif";
  ctx.fillText(`축척 기준: 0`, barX, barY - 6);
  ctx.fillText(`${spec.scaleBarLabel}`, barX + barPx - 35, barY - 6);

  // Summary Metadata Right Side
  ctx.font = "14px 'Malgun Gothic', sans-serif";
  ctx.fillText(
    `중심좌표: EPSG:5187 (${report.location.epsg5187X}m, ${report.location.epsg5187Y}m) / 반경 500m 유적 ${spec.archaeoSummaryBadge.within500mCount}개소 · 1km 유적 ${spec.archaeoSummaryBadge.within1000mCount}개소`,
    520,
    footY + 102
  );

  // North Arrow Compass
  const compX = outW - 95, compY = footY + 66;
  ctx.save();
  ctx.translate(compX, compY);
  ctx.beginPath();
  ctx.moveTo(0, -34);
  ctx.lineTo(14, 20);
  ctx.lineTo(0, 10);
  ctx.lineTo(-14, 20);
  ctx.closePath();
  ctx.fillStyle = '#1F6FB2';
  ctx.fill();
  ctx.fillStyle = '#1D2733';
  ctx.font = "bold 16px 'Malgun Gothic', sans-serif";
  ctx.fillText('N', -6, -40);
  ctx.restore();

  sheetCanvas.toBlob((blob) => {
    if (!blob) return;
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'gyeongju_3d_compose_sheet.png';
    a.click();
    URL.revokeObjectURL(url);
  }, 'image/png');
}

// --- 9. UI Controls & Theme Switching ---
document.getElementById('sel-ka-theme').addEventListener('change', (e) => {
  themeState.themeKey = e.target.value;
  applyKaThemeToDocument(document.documentElement, themeState);
});
document.getElementById('chk-high-contrast').addEventListener('change', (e) => {
  themeState.highContrast = e.target.checked;
  applyKaThemeToDocument(document.documentElement, themeState);
});
document.getElementById('chk-large-text').addEventListener('change', (e) => {
  themeState.largeText = e.target.checked;
  applyKaThemeToDocument(document.documentElement, themeState);
});

document.querySelectorAll('[data-shader]').forEach((btn) => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('[data-shader]').forEach((b) => b.classList.remove('active'));
    btn.classList.add('active');
    terrainUniforms.uShaderMode.value = Number(btn.getAttribute('data-shader'));
  });
});

function updateSunFromSliders() {
  const hour = Number(document.getElementById('rng-sun-hour').value);
  const day = Number(document.getElementById('rng-sun-day').value);
  const hInt = Math.floor(hour);
  const mInt = Math.round((hour - hInt) * 60);
  document.getElementById('lbl-sun-hour').textContent =
    `${String(hInt).padStart(2, '0')}:${String(mInt).padStart(2, '0')}`;
  document.getElementById('lbl-sun-day').textContent = `${day}일차`;

  const sun = computeGyeongjuSunPosition(hour, day);
  skyUniforms.uSunDirection.value.set(...sun.sunDirection);
  terrainUniforms.uSunDirection.value.set(...sun.sunDirection);
  dirLight.position.set(
    sun.sunDirection[0] * 3000,
    sun.sunDirection[1] * 3000,
    sun.sunDirection[2] * 3000
  );
  document.getElementById('hud-sun').textContent =
    `태양 고도: ${sun.altitudeDeg}° / 방위각: ${sun.azimuthDeg}°`;
}
document.getElementById('rng-sun-hour').addEventListener('input', updateSunFromSliders);
document.getElementById('rng-sun-day').addEventListener('input', updateSunFromSliders);
updateSunFromSliders();

document.getElementById('rng-z-scale').addEventListener('input', (e) => {
  const s = Number(e.target.value);
  document.getElementById('lbl-z-scale').textContent = `${s.toFixed(2)}x`;
  terrainUniforms.uElevationExaggeration.value = s;
  terrainGroup.scale.set(1, s, 1);
  worldScaleRoot.scale.set(1, s, 1);
});

document.getElementById('chk-buildings').addEventListener('change', (e) => {
  buildingGroup.visible = e.target.checked;
});
document.getElementById('chk-viewshed').addEventListener('change', () => {
  if (latestBundle) renderEvaluationOverlays(latestBundle.report);
});
document.getElementById('chk-buffers').addEventListener('change', (e) => {
  terrainUniforms.uShowRadiusRings.value = e.target.checked ? 1.0 : 0.0;
  if (latestBundle) renderEvaluationOverlays(latestBundle.report);
});

const btnToggleSilla = document.getElementById('btn-toggle-silla');
btnToggleSilla.addEventListener('click', () => {
  sillaReconstructionGroup.visible = !sillaReconstructionGroup.visible;
  btnToggleSilla.classList.toggle('active', sillaReconstructionGroup.visible);
  btnToggleSilla.textContent = sillaReconstructionGroup.visible
    ? '신라 왕경(8세기 방리제·9층목탑) 복원 ON'
    : '신라 왕경(8세기 방리제·9층목탑) 복원 OFF';
});

function downloadTextFile(filename, content, mime = 'text/plain;charset=utf-8') {
  const blob = new Blob([content], { type: mime });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  a.click();
  URL.revokeObjectURL(url);
}

document.getElementById('btn-sync-hgis').addEventListener('click', async () => {
  if (!latestBundle) return;
  await evaluateAndSyncSite({
    worldX: latestBundle.report.location.worldX,
    worldZ: latestBundle.report.location.worldZ,
    surveyName: latestBundle.report.surveyName,
  });
});

document.getElementById('btn-export-sheet').addEventListener('click', exportComposeSheetPng);

document.getElementById('btn-export-geojson').addEventListener('click', () => {
  if (!latestBundle) return;
  downloadTextFile(
    'hgis_gyeongju_archaeo_layers.geojson',
    JSON.stringify(latestBundle.geojson, null, 2),
    'application/geo+json;charset=utf-8'
  );
});

document.getElementById('btn-export-blender').addEventListener('click', async () => {
  if (!latestBundle) return;
  const out = await engineClient.dispatch('EXPORT_ARTIFACTS', { report: latestBundle.report });
  downloadTextFile('gyeongju_blender42_scene.py', out.blenderPy, 'text/x-python;charset=utf-8');
});

document.getElementById('btn-export-report').addEventListener('click', async () => {
  if (!latestBundle) return;
  const out = await engineClient.dispatch('EXPORT_ARTIFACTS', { report: latestBundle.report });
  downloadTextFile('gyeongju_archaeology_evaluation.md', out.markdownReport, 'text/markdown;charset=utf-8');
});

window.addEventListener('resize', () => {
  const h = Math.max(1, window.innerHeight - 80);
  camera.aspect = window.innerWidth / h;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, h);
});

let frames = 0;
let lastFpsTime = performance.now();
function animate() {
  requestAnimationFrame(animate);
  controls.update();
  renderer.render(scene, camera);

  frames++;
  const now = performance.now();
  if (now - lastFpsTime >= 1000) {
    const fps = Math.round((frames * 1000) / (now - lastFpsTime));
    document.getElementById('hud-fps').textContent = `${fps} FPS (Engine Offload Active)`;
    frames = 0;
    lastFpsTime = now;
  }
}

initializeFromEngineWorker();
animate();
