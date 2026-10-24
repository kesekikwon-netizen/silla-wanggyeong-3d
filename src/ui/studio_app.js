/**
 * @file studio_app.js
 * @description WebGL2 Interactive 3D Gyeongju Archaeological & Satellite Terrain Studio.
 * Implements Blender 4.2 REA-derived PBVH + KDTree3D + Quadtree CDLOD + Nishita Sky + AgX Pipeline.
 */

import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

import { local3DToWgs84, wgs84ToSlippyTile } from '../core/gyeongju_crs.js';
import { GyeongjuTerrainQuadtree, sampleGyeongjuElevation } from '../core/gyeongju_dem_terrain.js';
import {
  getGyeongjuArchaeologySites3D,
  generateSillaBangrijeGrid3D,
  generateGyeongju3DBuildings,
} from '../core/gyeongju_archaeology_db.js';
import { GyeongjuArchaeologyEvaluator } from '../core/archaeology_evaluator.js';
import {
  computeGyeongjuSunPosition,
  NISHITA_SKY_VERTEX_SHADER,
  NISHITA_SKY_FRAGMENT_SHADER,
  EEVEE_TERRAIN_VERTEX_SHADER,
  EEVEE_TERRAIN_FRAGMENT_SHADER,
} from '../core/eevee_sky_shader.js';
import {
  generateBlender42PythonScript,
  generateArchaeologicalMarkdownReport,
} from '../bridge/blender_exporter.js';

// --- 1. Initialize Core Gyeongju Quadtree DEM, PBVH, and Archaeology Evaluator ---
const terrainQuadtree = new GyeongjuTerrainQuadtree({
  extentM: 8400,
  maxDepth: 3,
  chunkGridSegments: 20,
  skirtDepthM: 18,
}).build();

const evaluator = new GyeongjuArchaeologyEvaluator(terrainQuadtree.bvh);
const gyeongjuSites = getGyeongjuArchaeologySites3D();
const bangrijeGrid = generateSillaBangrijeGrid3D();
const modernBuildings = generateGyeongju3DBuildings(950);

document.getElementById('stat-bvh-nodes').textContent = terrainQuadtree.bvh.nodes.length.toLocaleString();

// --- 2. Setup WebGL2 Renderer, Scene, Camera, OrbitControls ---
const canvas = document.getElementById('viewport-canvas');
const renderer = new THREE.WebGLRenderer({
  canvas,
  antialias: true,
  powerPreference: 'high-performance',
});
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.setSize(window.innerWidth, window.innerHeight);

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(48, window.innerWidth / window.innerHeight, 5, 35000);
camera.position.set(-680, 560, 1450);

const controls = new OrbitControls(camera, renderer.domElement);
controls.target.set(220, 55, 120);
controls.enableDamping = true;
controls.dampingFactor = 0.06;
controls.maxPolarAngle = Math.PI * 0.485;
controls.minDistance = 40;
controls.maxDistance = 12000;
controls.update();

// --- 3. Nishita Physical Sky Dome (`bf_intern_sky: sky_nishita.cpp`) ---
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

// Directional Sun + Hemisphere Fill Light for 3D Monuments & Instanced Buildings
const dirLight = new THREE.DirectionalLight(0xfff4e0, 2.4);
dirLight.position.set(
  sunState.sunDirection[0] * 3000,
  sunState.sunDirection[1] * 3000,
  sunState.sunDirection[2] * 3000
);
scene.add(dirLight);
const hemiLight = new THREE.HemisphereLight(0x9ec5ff, 0x3d352e, 0.85);
scene.add(hemiLight);

// --- 4. Terrain Material & Asynchronous Satellite Tile Draping (Esri World Imagery) ---
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

for (const chunk of terrainQuadtree.leafChunks) {
  const geom = new THREE.BufferGeometry();
  geom.setAttribute('position', new THREE.BufferAttribute(chunk.positions, 3));
  geom.setAttribute('normal', new THREE.BufferAttribute(chunk.normals, 3));
  geom.setAttribute('uv', new THREE.BufferAttribute(chunk.uvs, 2));
  geom.setAttribute('aArchaeoAttr', new THREE.BufferAttribute(chunk.attributes, 4));
  geom.setIndex(new THREE.BufferAttribute(chunk.indices, 1));
  geom.computeBoundingSphere();

  const mesh = new THREE.Mesh(geom, terrainMaterial);
  mesh.name = `Terrain_${chunk.chunkId}`;
  terrainGroup.add(mesh);
}

// Stitch real satellite imagery tiles (Esri World Imagery z=14) over the 8.4km x 8.4km Gyeongju Basin
async function loadGyeongjuSatelliteOrthoTexture() {
  try {
    const z = 14;
    const nwWgs = local3DToWgs84(-terrainQuadtree.halfExtentM, -terrainQuadtree.halfExtentM);
    const seWgs = local3DToWgs84(terrainQuadtree.halfExtentM, terrainQuadtree.halfExtentM);
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
    // Procedural Gyeongju land-cover shader remains active if offline
  }
}
loadGyeongjuSatelliteOrthoTexture();

// --- 5. Build GPU-Instanced 3D Contemporary Gyeongju Buildings (`draw_instance_data.cc` pattern) ---
const worldScaleRoot = new THREE.Group();
worldScaleRoot.scale.set(1, 1.35, 1);
scene.add(worldScaleRoot);

const buildingGroup = new THREE.Group();
worldScaleRoot.add(buildingGroup);

function buildInstancedGyeongjuBuildings() {
  const boxGeo = new THREE.BoxGeometry(1, 1, 1);
  boxGeo.translate(0, 0.5, 0);
  const bldMat = new THREE.MeshStandardMaterial({
    roughness: 0.75,
    metalness: 0.12,
  });

  const instMesh = new THREE.InstancedMesh(boxGeo, bldMat, modernBuildings.length);
  const dummy = new THREE.Object3D();
  const color = new THREE.Color();

  modernBuildings.forEach((b, i) => {
    dummy.position.set(b.worldX, b.worldY, b.worldZ);
    dummy.rotation.set(0, b.rotationRad, 0);
    dummy.scale.set(b.widthM, b.heightM, b.depthM);
    dummy.updateMatrix();
    instMesh.setMatrixAt(i, dummy.matrix);

    if (b.isHanok) {
      // Dark traditional tiled roof & warm timber tone (`황리단길·교촌 한옥지구`)
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
buildInstancedGyeongjuBuildings();

// --- 6. Build 3D Silla Royal Monuments, Tumuli, and Bangrije Ancient Street Grid ---
const sillaReconstructionGroup = new THREE.Group();
worldScaleRoot.add(sillaReconstructionGroup);

const monumentPermanentGroup = new THREE.Group();
worldScaleRoot.add(monumentPermanentGroup);

function buildSillaHeritage3D() {
  const tumulusMat = new THREE.MeshStandardMaterial({
    color: 0x5c8a3c,
    roughness: 0.88,
    metalness: 0.02,
  });
  const royalWoodMat = new THREE.MeshStandardMaterial({
    color: 0x9e3628,
    roughness: 0.55,
    metalness: 0.15,
  });
  const stoneMat = new THREE.MeshStandardMaterial({
    color: 0xb8b2a6,
    roughness: 0.8,
  });
  const goldFinialMat = new THREE.MeshStandardMaterial({
    color: 0xfbbf24,
    roughness: 0.25,
    metalness: 0.85,
    emissive: 0x553300,
  });

  for (const s of gyeongjuSites) {
    if (s.archetype3D === 'tumulus_double') {
      // Hwangnamdaechong Twin Mound (황남대총 남북 쌍분)
      const southGeo = new THREE.SphereGeometry(s.footprintRadiusM * 0.68, 28, 16, 0, Math.PI * 2, 0, Math.PI * 0.5);
      const southMound = new THREE.Mesh(southGeo, tumulusMat);
      southMound.scale.set(1, s.heightM / (s.footprintRadiusM * 0.68), 1);
      southMound.position.set(s.worldX, s.worldY, s.worldZ + 24);
      monumentPermanentGroup.add(southMound);

      const northMound = southMound.clone();
      northMound.position.set(s.worldX, s.worldY, s.worldZ - 24);
      northMound.scale.multiplyScalar(0.94);
      monumentPermanentGroup.add(northMound);
    } else if (s.archetype3D === 'tumulus_single' || s.archetype3D === 'tumulus_cluster') {
      const moundGeo = new THREE.SphereGeometry(
        Math.min(44, s.footprintRadiusM),
        24,
        14,
        0,
        Math.PI * 2,
        0,
        Math.PI * 0.5
      );
      const mound = new THREE.Mesh(moundGeo, tumulusMat);
      const r = Math.min(44, s.footprintRadiusM);
      mound.scale.set(1, s.heightM / r, 1);
      mound.position.set(s.worldX, s.worldY, s.worldZ);
      monumentPermanentGroup.add(mound);
    } else if (s.archetype3D === 'observatory_tower') {
      // Cheomseongdae (첨성대)
      const cylGeo = new THREE.CylinderGeometry(1.55, 2.6, s.heightM, 16);
      const mesh = new THREE.Mesh(cylGeo, stoneMat);
      mesh.position.set(s.worldX, s.worldY + s.heightM * 0.5, s.worldZ);
      monumentPermanentGroup.add(mesh);
    } else if (s.archetype3D === 'pagoda_9story') {
      // Hwangnyongsa 9-Story Wooden Pagoda (황룡사 9층목탑 80m 입체 복원)
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
      // 18m Bronze/Gold Spire (찰주·상륜부, 합계 약 80m)
      const spire = new THREE.Mesh(new THREE.CylinderGeometry(0.35, 0.9, 18.0, 12), goldFinialMat);
      spire.position.y = currY + 9.0;
      pagodaGroup.add(spire);

      sillaReconstructionGroup.add(pagodaGroup);
    } else if (s.archetype3D === 'fortress_palace') {
      // Wolseong / Myeonghwalsanseong rampart ring
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

  // Silla Bangrije Ancient Gridded Streets (`신라 왕경 방리제 도로망`)
  const mainRoadMat = new THREE.LineBasicMaterial({ color: 0xfbbf24, transparent: true, opacity: 0.65 });
  const subRoadMat = new THREE.LineBasicMaterial({ color: 0xd97706, transparent: true, opacity: 0.32 });

  for (const road of bangrijeGrid.roads) {
    const pts = road.points.map(([x, y, z]) => new THREE.Vector3(x, y, z));
    const lineGeo = new THREE.BufferGeometry().setFromPoints(pts);
    const line = new THREE.Line(lineGeo, road.isMainRoyalAvenue ? mainRoadMat : subRoadMat);
    sillaReconstructionGroup.add(line);
  }
}
buildSillaHeritage3D();

// --- 7. Dynamic Archaeological Evaluation Overlay Group (500m/1000m Rings, Viewshed Rays, Trenches) ---
const evaluationOverlayGroup = new THREE.Group();
worldScaleRoot.add(evaluationOverlayGroup);

let latestEvaluationReport = null;

function renderEvaluationOverlays(report) {
  while (evaluationOverlayGroup.children.length > 0) {
    const child = evaluationOverlayGroup.children.pop();
    if (child.geometry) child.geometry.dispose();
  }

  const loc = report.location;
  terrainUniforms.uSelectedSiteWorldPos.value.set(loc.worldX, loc.worldY, loc.worldZ);

  // 1. Survey Center Pin Beacon
  const beaconGeo = new THREE.CylinderGeometry(2.5, 2.5, 95, 16);
  const beaconMat = new THREE.MeshBasicMaterial({ color: 0xef4444 });
  const beacon = new THREE.Mesh(beaconGeo, beaconMat);
  beacon.position.set(loc.worldX, loc.worldY + 47.5, loc.worldZ);
  evaluationOverlayGroup.add(beacon);

  // 2. 500m and 1,000m Round Buffer Rings (`JoinStyle::Round`)
  if (document.getElementById('chk-buffers').checked) {
    const pts500 = report.proximityAnalysis.buffer500m3D.map(([x, y, z]) => new THREE.Vector3(x, y + 1.5, z));
    const line500 = new THREE.Line(
      new THREE.BufferGeometry().setFromPoints(pts500),
      new THREE.LineBasicMaterial({ color: 0xef4444 })
    );
    evaluationOverlayGroup.add(line500);

    const pts1000 = report.proximityAnalysis.buffer1000m3D.map(([x, y, z]) => new THREE.Vector3(x, y + 1.5, z));
    const line1000 = new THREE.Line(
      new THREE.BufferGeometry().setFromPoints(pts1000),
      new THREE.LineBasicMaterial({ color: 0xfbbf24 })
    );
    evaluationOverlayGroup.add(line1000);
  }

  // 3. 3D Viewshed Intervisibility Rays (`PBVHTree.hasLineOfSight`)
  if (document.getElementById('chk-viewshed').checked) {
    for (const vs of report.viewshedAnalysis.landmarks) {
      const p1 = new THREE.Vector3(...vs.observerPoint);
      p1.y += 2.0;
      const p2 = new THREE.Vector3(...vs.targetPoint);
      const rayGeo = new THREE.BufferGeometry().setFromPoints([p1, p2]);
      const rayMat = new THREE.LineBasicMaterial({
        color: vs.visible ? 0x10b981 : 0xef4444,
        transparent: true,
        opacity: vs.visible ? 0.85 : 0.45,
      });
      evaluationOverlayGroup.add(new THREE.Line(rayGeo, rayMat));
    }
  }

  // 4. 3D Virtual Excavation Trenches (`T-1`, `T-2`)
  const trenchMat = new THREE.MeshStandardMaterial({ color: 0x8c5828, roughness: 0.9 });
  for (const tr of report.virtualTrenches.trenchSpecs) {
    const trGeo = new THREE.BoxGeometry(tr.lengthM, tr.depthM, tr.widthM);
    const trMesh = new THREE.Mesh(trGeo, trenchMat);
    trMesh.position.set(tr.worldX, tr.worldY + 0.8, tr.worldZ);
    trMesh.rotation.y = tr.rotationRad;
    evaluationOverlayGroup.add(trMesh);
  }
}

function updateRightEvaluationPanel(report) {
  latestEvaluationReport = report;
  document.getElementById('eval-site-title').textContent = report.surveyName;
  document.getElementById('eval-coords').textContent =
    `WGS84 ${report.location.lat.toFixed(5)}°N, ${report.location.lon.toFixed(5)}°E | EPSG:5187 (${report.location.epsg5187X.toFixed(0)}, ${report.location.epsg5187Y.toFixed(0)})`;
  document.getElementById('eval-grade').textContent = report.assessmentSummary.recommendedActionGrade;
  document.getElementById('eval-score').textContent = report.assessmentSummary.archaeologicalPotentialScore;

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
    `500m 내 ${prox.within500mCount}개소 / 1km 내 ${prox.totalWithin1000mCount}개소`;

  const allNearby = [...prox.sitesWithin500m, ...prox.sites500to1000m];
  const nearbyTbody = document.getElementById('eval-nearby-tbody');
  if (allNearby.length === 0) {
    nearbyTbody.innerHTML = `<tr><td colspan="3" style="color:var(--text-muted);">반경 1,000m 이내 주요 국가지정 유적 없음 (최인접: ${prox.nearestSiteName} ${prox.nearestSiteDistanceM}m)</td></tr>`;
  } else {
    nearbyTbody.innerHTML = allNearby
      .map(
        (s) => `
      <tr>
        <td><b>${s.name}</b></td>
        <td style="color:${s.distanceM <= 500 ? '#fca5a5' : '#fcd34d'}; font-weight:600;">${s.distanceM}m (${s.directionKo})</td>
        <td>${s.zoneBand.split(' ')[0]}</td>
      </tr>`
      )
      .join('');
  }

  const vs = report.viewshedAnalysis;
  document.getElementById('eval-viewshed-count').textContent =
    `${vs.visibleCount} / ${vs.totalLandmarks} 거점 가시`;
  document.getElementById('eval-viewshed-tbody').innerHTML = vs.landmarks
    .map(
      (v) => `
    <tr>
      <td>${v.landmarkName}</td>
      <td style="color:${v.visible ? '#34d399' : '#f87171'}; font-weight:600;">${v.visible ? '● 가시(Visible)' : '× 지형차폐'}</td>
      <td>${v.distanceM}m (${v.elevationAngleDeg}°)</td>
    </tr>`
    )
    .join('');

  document.getElementById('eval-stratigraphy').innerHTML = report.virtualTrenches.predictedStratigraphy
    .map(
      (l) => `
    <div class="strat-layer">
      <div class="strat-swatch" style="background:${l.colorHex};"></div>
      <div>
        <div style="font-weight:700;">${l.layerCode} · ${l.nameKo} <span style="color:#fcd34d;">(GL -${l.depthRangeM[0]}~-${l.depthRangeM[1]}m)</span></div>
        <div style="color:var(--text-muted); font-size:11px;">예상 유구·유물: ${l.expectedFinds}</div>
      </div>
    </div>`
    )
    .join('');

  renderEvaluationOverlays(report);
}

function runEvaluationAtWorldXZ(worldX, worldZ, customTitle = null) {
  const report = evaluator.evaluateSite({
    worldX,
    worldZ,
    surveyName: customTitle,
  });
  updateRightEvaluationPanel(report);
}

// --- 8. Populate Preset Gyeongju Archaeological Sites in Left Panel ---
const presetGrid = document.getElementById('preset-sites-grid');
gyeongjuSites.slice(0, 12).forEach((site) => {
  const btn = document.createElement('button');
  btn.className = 'btn preset-btn';
  btn.textContent = site.name.replace('경주 ', '');
  btn.title = `${site.name} (${site.designation})`;
  btn.addEventListener('click', () => {
    runEvaluationAtWorldXZ(site.worldX, site.worldZ, `${site.name} 정밀 평가구역`);
    const zScale = terrainUniforms.uElevationExaggeration.value;
    controls.target.set(site.worldX, site.worldY * zScale, site.worldZ);
    camera.position.set(site.worldX - 360, site.worldY * zScale + 290, site.worldZ + 520);
    controls.update();
  });
  presetGrid.appendChild(btn);
});

// Run initial evaluation at Cheomseongdae / Wolseong / Jjoksaem royal capital junction
runEvaluationAtWorldXZ(180, -80, '경주 인왕동(첨성대·쪽샘·월성 사이) 시굴조사구역');

// --- 9. Interactive Double-Click PBVHTree Raycasting on 3D Terrain ---
canvas.addEventListener('dblclick', (ev) => {
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
  const dir = [
    raycaster.ray.direction.x,
    raycaster.ray.direction.y / zScale,
    raycaster.ray.direction.z,
  ];

  const hit = terrainQuadtree.bvh.raycast(origin, dir, 30000);
  document.getElementById('stat-raycast-visits').textContent = `${hit.nodeVisits}`;
  if (hit.hit && hit.point) {
    runEvaluationAtWorldXZ(hit.point[0], hit.point[2]);
  }
});

// --- 10. UI Event Listeners (Shader Modes, Nishita Sun Sliders, Silla Reconstruction, Exports) ---
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
  if (latestEvaluationReport) renderEvaluationOverlays(latestEvaluationReport);
});
document.getElementById('chk-buffers').addEventListener('change', (e) => {
  terrainUniforms.uShowRadiusRings.value = e.target.checked ? 1.0 : 0.0;
  if (latestEvaluationReport) renderEvaluationOverlays(latestEvaluationReport);
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

document.getElementById('btn-export-blender').addEventListener('click', () => {
  const py = generateBlender42PythonScript(latestEvaluationReport);
  downloadTextFile('gyeongju_blender42_scene.py', py, 'text/x-python;charset=utf-8');
});

document.getElementById('btn-export-report').addEventListener('click', () => {
  const md = generateArchaeologicalMarkdownReport(latestEvaluationReport);
  downloadTextFile('gyeongju_archaeology_evaluation.md', md, 'text/markdown;charset=utf-8');
});

window.addEventListener('resize', () => {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
});

// --- 11. Zero-Lag Render Loop ---
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
    document.getElementById('hud-fps').textContent = `${fps} FPS (GPU Instanced + PBVH Active)`;
    frames = 0;
    lastFpsTime = now;
  }
}
animate();
