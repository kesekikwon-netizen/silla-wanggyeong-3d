/**
 * @file gyeongju_crs.js
 * @description Geodetic & Coordinate Reference System (CRS) engine for Gyeongju 3D Terrain & Archaeology.
 * Implements exact GRS80 Transverse Mercator (EPSG:5187 Korea 2000 East Belt),
 * WGS84 (EPSG:4326), Web Mercator Slippy Tiles (EPSG:3857 z/x/y), and
 * Blender-compatible right-handed local 3D meter coordinates (X: East, Y: Up, Z: South / or Blender Z-Up).
 */

export const GYEONGJU_ORIGIN_WGS84 = Object.freeze({
  name: '경주 신라왕경 중심 (월성·대릉원·첨성대 기준점)',
  lat: 35.8347,
  lon: 129.2190,
  baseElevationM: 44.5,
});

// GRS80 Ellipsoid constants for EPSG:5187 (Korea 2000 / East Belt 2010)
const GRS80_A = 6378137.0;
const GRS80_INV_F = 298.257222101;
const GRS80_F = 1.0 / GRS80_INV_F;
const GRS80_E2 = 2.0 * GRS80_F - GRS80_F * GRS80_F;
const GRS80_EP2 = GRS80_E2 / (1.0 - GRS80_E2);

const EPSG5187_LAT0_RAD = (38.0 * Math.PI) / 180.0;
const EPSG5187_LON0_RAD = (129.0 * Math.PI) / 180.0;
const EPSG5187_K0 = 1.0;
const EPSG5187_FALSE_EASTING = 200000.0;
const EPSG5187_FALSE_NORTHING = 600000.0;

function meridionalArc(latRad) {
  const e2 = GRS80_E2;
  const e4 = e2 * e2;
  const e6 = e4 * e2;
  const A0 = 1.0 - e2 / 4.0 - (3.0 * e4) / 64.0 - (5.0 * e6) / 256.0;
  const A2 = (3.0 / 8.0) * (e2 + e4 / 4.0 + (15.0 * e6) / 128.0);
  const A4 = (15.0 / 256.0) * (e4 + (3.0 * e6) / 4.0);
  const A6 = (35.0 * e6) / 3072.0;
  return (
    GRS80_A *
    (A0 * latRad -
      A2 * Math.sin(2.0 * latRad) +
      A4 * Math.sin(4.0 * latRad) -
      A6 * Math.sin(6.0 * latRad))
  );
}

const M0_5187 = meridionalArc(EPSG5187_LAT0_RAD);

/**
 * Convert WGS84 (lat, lon in degrees) to EPSG:5187 (x: Easting, y: Northing in meters).
 * @param {number} lat - Latitude in decimal degrees
 * @param {number} lon - Longitude in decimal degrees
 * @returns {{x: number, y: number}}
 */
export function wgs84ToEpsg5187(lat, lon) {
  const phi = (lat * Math.PI) / 180.0;
  const lam = (lon * Math.PI) / 180.0;
  const sinPhi = Math.sin(phi);
  const cosPhi = Math.cos(phi);
  const tanPhi = Math.tan(phi);

  const N = GRS80_A / Math.sqrt(1.0 - GRS80_E2 * sinPhi * sinPhi);
  const T = tanPhi * tanPhi;
  const C = GRS80_EP2 * cosPhi * cosPhi;
  const A = (lam - EPSG5187_LON0_RAD) * cosPhi;
  const M = meridionalArc(phi);

  const A2 = A * A;
  const A3 = A2 * A;
  const A4 = A2 * A2;
  const A5 = A4 * A;
  const A6 = A4 * A2;

  const x =
    EPSG5187_FALSE_EASTING +
    EPSG5187_K0 *
      N *
      (A +
        ((1.0 - T + C) * A3) / 6.0 +
        ((5.0 - 18.0 * T + T * T + 72.0 * C - 58.0 * GRS80_EP2) * A5) / 120.0);

  const y =
    EPSG5187_FALSE_NORTHING +
    EPSG5187_K0 *
      (M -
        M0_5187 +
        N *
          tanPhi *
          (A2 / 2.0 +
            ((5.0 - T + 9.0 * C + 4.0 * C * C) * A4) / 24.0 +
            ((61.0 - 58.0 * T + T * T + 600.0 * C - 330.0 * GRS80_EP2) * A6) / 720.0));

  return { x, y };
}

/**
 * Convert EPSG:5187 (x: Easting, y: Northing in meters) back to WGS84 (lat, lon in degrees).
 * @param {number} x - Easting in meters
 * @param {number} y - Northing in meters
 * @returns {{lat: number, lon: number}}
 */
export function epsg5187ToWgs84(x, y) {
  const e2 = GRS80_E2;
  const e1 = (1.0 - Math.sqrt(1.0 - e2)) / (1.0 + Math.sqrt(1.0 - e2));
  const M = M0_5187 + (y - EPSG5187_FALSE_NORTHING) / EPSG5187_K0;

  const e4 = e2 * e2;
  const e6 = e4 * e2;
  const mu = M / (GRS80_A * (1.0 - e2 / 4.0 - (3.0 * e4) / 64.0 - (5.0 * e6) / 256.0));

  const e1_2 = e1 * e1;
  const e1_3 = e1_2 * e1;
  const e1_4 = e1_2 * e1_2;

  const phi1 =
    mu +
    ((3.0 * e1) / 2.0 - (27.0 * e1_3) / 32.0) * Math.sin(2.0 * mu) +
    ((21.0 * e1_2) / 16.0 - (55.0 * e1_4) / 32.0) * Math.sin(4.0 * mu) +
    ((151.0 * e1_3) / 96.0) * Math.sin(6.0 * mu) +
    ((1097.0 * e1_4) / 512.0) * Math.sin(8.0 * mu);

  const sinPhi1 = Math.sin(phi1);
  const cosPhi1 = Math.cos(phi1);
  const tanPhi1 = Math.tan(phi1);

  const N1 = GRS80_A / Math.sqrt(1.0 - e2 * sinPhi1 * sinPhi1);
  const R1 = (GRS80_A * (1.0 - e2)) / Math.pow(1.0 - e2 * sinPhi1 * sinPhi1, 1.5);
  const T1 = tanPhi1 * tanPhi1;
  const C1 = GRS80_EP2 * cosPhi1 * cosPhi1;
  const D = (x - EPSG5187_FALSE_EASTING) / (N1 * EPSG5187_K0);

  const D2 = D * D;
  const D3 = D2 * D;
  const D4 = D2 * D2;
  const D5 = D4 * D;
  const D6 = D4 * D2;

  const phi =
    phi1 -
    ((N1 * tanPhi1) / R1) *
      (D2 / 2.0 -
        ((5.0 + 3.0 * T1 + 10.0 * C1 - 4.0 * C1 * C1 - 9.0 * GRS80_EP2) * D4) / 24.0 +
        ((61.0 + 90.0 * T1 + 298.0 * C1 + 45.0 * T1 * T1 - 252.0 * GRS80_EP2 - 3.0 * C1 * C1) * D6) /
          720.0);

  const lam =
    EPSG5187_LON0_RAD +
    (D -
      ((1.0 + 2.0 * T1 + C1) * D3) / 6.0 +
      ((5.0 - 2.0 * C1 + 28.0 * T1 - 3.0 * C1 * C1 + 8.0 * GRS80_EP2 + 24.0 * T1 * T1) * D5) / 120.0) /
      cosPhi1;

  return {
    lat: (phi * 180.0) / Math.PI,
    lon: (lam * 180.0) / Math.PI,
  };
}

export const GYEONGJU_ORIGIN_5187 = Object.freeze(
  wgs84ToEpsg5187(GYEONGJU_ORIGIN_WGS84.lat, GYEONGJU_ORIGIN_WGS84.lon)
);

/**
 * Convert WGS84 (lat, lon, elevationM) to local 3D world meters centered on Gyeongju Origin.
 * In WebGL / Three.js:
 *   worldX = Easting offset (+East, -West) in meters
 *   worldY = Elevation (+Up) in meters
 *   worldZ = -Northing offset (-North, +South) in meters so right-handed (X, Y, Z) matches North=-Z
 * In Blender Z-Up:
 *   blenderX = worldX (+East), blenderY = -worldZ (+North), blenderZ = worldY (+Up)
 */
export function wgs84ToLocal3D(lat, lon, elevationM = 0.0) {
  const pt = wgs84ToEpsg5187(lat, lon);
  const eastM = pt.x - GYEONGJU_ORIGIN_5187.x;
  const northM = pt.y - GYEONGJU_ORIGIN_5187.y;
  return {
    worldX: eastM,
    worldY: elevationM,
    worldZ: -northM,
    eastM,
    northM,
    epsg5187X: pt.x,
    epsg5187Y: pt.y,
  };
}

/**
 * Convert local 3D world meters (worldX, worldZ) back to WGS84 & EPSG:5187.
 */
export function local3DToWgs84(worldX, worldZ) {
  const epsg5187X = GYEONGJU_ORIGIN_5187.x + worldX;
  const epsg5187Y = GYEONGJU_ORIGIN_5187.y - worldZ;
  const wgs = epsg5187ToWgs84(epsg5187X, epsg5187Y);
  return {
    lat: wgs.lat,
    lon: wgs.lon,
    epsg5187X,
    epsg5187Y,
    eastM: worldX,
    northM: -worldZ,
  };
}

/**
 * Convert WGS84 (lat, lon) to Web Mercator slippy map tile coordinates at zoom `z`.
 */
export function wgs84ToSlippyTile(lat, lon, z) {
  const n = Math.pow(2, z);
  const latRad = (lat * Math.PI) / 180.0;
  const xFloat = ((lon + 180.0) / 360.0) * n;
  const yFloat =
    ((1.0 - Math.log(Math.tan(latRad) + 1.0 / Math.cos(latRad)) / Math.PI) / 2.0) * n;
  const tileX = Math.floor(xFloat);
  const tileY = Math.floor(yFloat);
  return {
    z,
    x: tileX,
    y: tileY,
    u: xFloat - tileX,
    v: yFloat - tileY,
  };
}

/**
 * Compute Euclidean distance (m) and compass bearing (deg & 8-direction Korean label)
 * from origin point (x1, y1) to target point (x2, y2) in EPSG:5187 meters.
 */
export function computeDistanceAndBearing5187(x1, y1, x2, y2) {
  const dx = x2 - x1;
  const dy = y2 - y1;
  const distanceM = Math.hypot(dx, dy);
  let azimuthDeg = (Math.atan2(dx, dy) * 180.0) / Math.PI;
  if (azimuthDeg < 0) azimuthDeg += 360.0;

  const dirs = ['북', '북동', '동', '남동', '남', '남서', '서', '북서'];
  const idx = Math.round(azimuthDeg / 45.0) % 8;
  return {
    distanceM,
    azimuthDeg,
    directionKo: dirs[idx],
  };
}
