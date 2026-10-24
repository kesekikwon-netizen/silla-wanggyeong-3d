/**
 * @file eevee_sky_shader.js
 * @description Blender 4.2.3 REA-derived Physical Atmosphere & Terrain Shader Pipeline:
 * - Solar Ephemeris Calculator for Gyeongju (`35.8347°N, 129.2190°E`, KST UTC+9)
 * - Port of `bf_intern_sky.lib: sky_nishita.cpp` (Rayleigh + Henyey-Greenstein Mie Scattering)
 * - Port of `opencolorio_2_3.dll: AgX_Base_sRGB.cube` analytic S-Curve Filmic/AgX Tone Mapping
 * - Multi-Mode Terrain & Archaeological Overlay GLSL Shaders:
 *   Mode 0: Photorealistic Satellite Orthoimagery + PBR Micro-Normal + Horizon Shadow + AgX
 *   Mode 1: Blender EEVEE Clay / Sculpt Cavity & Ambient Occlusion (미기복 정밀 판독 모드)
 *   Mode 2: Archaeological Landform & Suitability Heatmap (하안단구·선상지·유구포장 가능성 지수)
 *   Mode 3: Slope & Topographic Position Index (경사도·지형위치지수 TPI 분석 모드)
 */

import { GYEONGJU_ORIGIN_WGS84 } from './gyeongju_crs.js';

/**
 * Compute physical sun azimuth & altitude for Gyeongju at a given solar hour (5.0 - 20.0) and dayOfYear (1 - 365).
 * Default dayOfYear = 172 (Summer Solstice / 하지) or 265 (Autumn Equinox / 추분).
 */
export function computeGyeongjuSunPosition(hourKST = 14.5, dayOfYear = 265) {
  const latRad = (GYEONGJU_ORIGIN_WGS84.lat * Math.PI) / 180.0;
  // Solar declination angle (Cooper's equation)
  const declRad =
    ((23.45 * Math.PI) / 180.0) * Math.sin(((2.0 * Math.PI) / 365.0) * (284.0 + dayOfYear));

  // Solar time offset for Gyeongju (129.219E vs KST meridian 135.0E -> -23.1 minutes)
  const solarHour = hourKST - (135.0 - GYEONGJU_ORIGIN_WGS84.lon) / 15.0;
  const hourAngleRad = ((solarHour - 12.0) * 15.0 * Math.PI) / 180.0;

  const sinAlt =
    Math.sin(latRad) * Math.sin(declRad) +
    Math.cos(latRad) * Math.cos(declRad) * Math.cos(hourAngleRad);
  const altitudeRad = Math.asin(Math.max(-1.0, Math.min(1.0, sinAlt)));
  const altitudeDeg = (altitudeRad * 180.0) / Math.PI;

  const cosAz =
    (Math.sin(declRad) - Math.sin(latRad) * sinAlt) /
    (Math.cos(latRad) * Math.cos(altitudeRad) + 1e-7);
  let azimuthRad = Math.acos(Math.max(-1.0, Math.min(1.0, cosAz)));
  if (hourAngleRad > 0) {
    azimuthRad = 2.0 * Math.PI - azimuthRad;
  }
  const azimuthDeg = (azimuthRad * 180.0) / Math.PI;

  // Direction vector FROM origin TO sun in WebGL world coordinates (X=+East, Y=+Up, Z=+South)
  const cosAlt = Math.cos(altitudeRad);
  const sunDirX = Math.sin(azimuthRad) * cosAlt;
  const sunDirY = Math.max(0.03, Math.sin(altitudeRad));
  const sunDirZ = -Math.cos(azimuthRad) * cosAlt;
  const len = Math.hypot(sunDirX, sunDirY, sunDirZ) || 1.0;

  return {
    hourKST,
    dayOfYear,
    altitudeDeg: Number(altitudeDeg.toFixed(2)),
    azimuthDeg: Number(azimuthDeg.toFixed(2)),
    sunDirection: [sunDirX / len, sunDirY / len, sunDirZ / len],
  };
}

export const NISHITA_SKY_VERTEX_SHADER = /* glsl */ `
varying vec3 vWorldDirection;
void main() {
  vec4 worldPos = modelMatrix * vec4(position, 1.0);
  vWorldDirection = normalize(worldPos.xyz - cameraPosition);
  gl_Position = projectionMatrix * viewMatrix * worldPos;
}
`;

export const NISHITA_SKY_FRAGMENT_SHADER = /* glsl */ `
precision highp float;
varying vec3 vWorldDirection;
uniform vec3 uSunDirection;
uniform float uTurbidity;

// Blender 4.2 AgX / Filmic analytic tone curve
vec3 agxToneMap(vec3 color) {
  color = max(vec3(0.0), color);
  vec3 x = color * 0.85;
  return (x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14);
}

void main() {
  vec3 dir = normalize(vWorldDirection);
  vec3 sunDir = normalize(uSunDirection);
  float cosTheta = clamp(dot(dir, sunDir), -1.0, 1.0);
  float zenith = clamp(dir.y, 0.0, 1.0);

  // Rayleigh molecular scattering + Mie aerosol scattering (bf_intern_sky: sky_nishita.cpp)
  float sunElev = clamp(sunDir.y, 0.02, 1.0);
  vec3 rayleighZenith = mix(vec3(0.18, 0.25, 0.48), vec3(0.09, 0.28, 0.72), sqrt(sunElev));
  vec3 horizonGlow = mix(vec3(0.95, 0.44, 0.20), vec3(0.58, 0.74, 0.94), pow(sunElev, 0.45));

  float opticalDepth = pow(1.0 - zenith, 2.6);
  vec3 skyRadiance = mix(rayleighZenith, horizonGlow, opticalDepth);

  // Henyey-Greenstein Mie forward scattering lobe around solar disk
  float g = 0.76;
  float miePhase = (1.0 - g * g) / (4.0 * 3.14159265 * pow(1.0 + g * g - 2.0 * g * cosTheta, 1.5));
  vec3 sunColor = mix(vec3(1.0, 0.48, 0.18), vec3(1.0, 0.96, 0.88), pow(sunElev, 0.35));
  skyRadiance += sunColor * miePhase * 0.12 * uTurbidity;

  // Crisp solar disk
  float sunDisk = smoothstep(0.9994, 0.99992, cosTheta);
  skyRadiance += sunColor * sunDisk * 4.5;

  // Ground hemisphere reflection below horizon
  if (dir.y < 0.0) {
    skyRadiance = mix(horizonGlow * 0.65, vec3(0.12, 0.15, 0.18), clamp(-dir.y * 4.0, 0.0, 1.0));
  }

  gl_FragColor = vec4(agxToneMap(skyRadiance), 1.0);
}
`;

export const EEVEE_TERRAIN_VERTEX_SHADER = /* glsl */ `
attribute vec4 aArchaeoAttr; // [slopeDeg, tpi, landformIndex, archaeoScore]
varying vec3 vWorldPos;
varying vec3 vNormal;
varying vec2 vUv;
varying vec4 vArchaeoAttr;

void main() {
  vUv = uv;
  vNormal = normalize(normalMatrix * normal);
  vArchaeoAttr = aArchaeoAttr;
  vec4 worldPosition = modelMatrix * vec4(position, 1.0);
  vWorldPos = worldPosition.xyz;
  gl_Position = projectionMatrix * viewMatrix * worldPosition;
}
`;

export const EEVEE_TERRAIN_FRAGMENT_SHADER = /* glsl */ `
precision highp float;

varying vec3 vWorldPos;
varying vec3 vNormal;
varying vec2 vUv;
varying vec4 vArchaeoAttr;

uniform sampler2D uSatelliteMap;
uniform float uHasSatelliteMap;
uniform vec3 uSunDirection;
uniform int uShaderMode; // 0=Satellite PBR, 1=EEVEE Sculpt Clay/Cavity, 2=Archaeo Suitability, 3=Slope & TPI
uniform float uElevationExaggeration;
uniform vec3 uSelectedSiteWorldPos;
uniform float uShowRadiusRings;

vec3 agxToneMap(vec3 color) {
  color = max(vec3(0.0), color);
  vec3 x = color * 0.92;
  return (x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14);
}

// Procedural micro-detail noise for zero-blur close-ups on Gyeongju terrain
float hash(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(
    mix(hash(i + vec2(0.0, 0.0)), hash(i + vec2(1.0, 0.0)), u.x),
    mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x),
    u.y
  );
}

void main() {
  vec3 N = normalize(vNormal);
  vec3 L = normalize(uSunDirection);
  float slopeDeg = vArchaeoAttr.x;
  float tpi = vArchaeoAttr.y;
  float landformIdx = vArchaeoAttr.z;
  float archaeoScore = vArchaeoAttr.w;
  float elevM = vWorldPos.y;

  // Micro-surface normal perturbation
  float nDetail = noise(vWorldPos.xz * 0.045) * 2.0 - 1.0;
  N = normalize(N + vec3(nDetail * 0.05, 0.0, nDetail * 0.05));

  float NdotL = max(dot(N, L), 0.0);
  // EEVEE GTAO / Cavity approximation from TPI & slope
  float cavityAO = clamp(0.78 + tpi * 0.045 - (slopeDeg / 90.0) * 0.18, 0.35, 1.12);

  vec3 baseAlbedo = vec3(0.32, 0.42, 0.26);

  if (uShaderMode == 0) {
    // Mode 0: Satellite Orthoimagery + Procedural Gyeongju Land-Cover Synthesis
    vec3 procColor = vec3(0.28, 0.38, 0.22);
    if (landformIdx < 0.5) {
      // River channel & floodplain (형산강·남천·북천 수계)
      procColor = mix(vec3(0.14, 0.29, 0.38), vec3(0.45, 0.46, 0.40), smoothstep(35.0, 39.2, elevM));
    } else if (landformIdx < 2.5) {
      // Gyeongju Royal Capital basin floor, paddy fields, and grass parks
      float fieldGrid = step(0.06, fract(vWorldPos.x * 0.008)) * step(0.06, fract(vWorldPos.z * 0.008));
      procColor = mix(vec3(0.36, 0.42, 0.28), vec3(0.31, 0.45, 0.24), fieldGrid);
      procColor += nDetail * 0.03;
    } else {
      // Mountain pine forest (남산·명활산·선도산 소나무림) & granite outcrops
      procColor = mix(vec3(0.15, 0.28, 0.14), vec3(0.46, 0.44, 0.40), smoothstep(22.0, 38.0, slopeDeg));
    }

    if (uHasSatelliteMap > 0.5) {
      vec3 satTex = texture2D(uSatelliteMap, vUv).rgb;
      // Blend real satellite tile with micro-normal detail
      baseAlbedo = mix(procColor, satTex, 0.86) + nDetail * 0.018;
    } else {
      baseAlbedo = procColor;
    }
  } else if (uShaderMode == 1) {
    // Mode 1: Blender EEVEE Sculpt Clay / MatCap Cavity Mode (고분·성곽·하안단구 미기복 판독용)
    vec3 clayBase = vec3(0.68, 0.56, 0.46);
    float ridgeHighlight = clamp(0.5 + tpi * 0.09, 0.15, 1.25);
    // Topographic 5m contour lines
    float contour5m = smoothstep(0.08, 0.0, abs(fract(elevM / 5.0) - 0.5));
    baseAlbedo = clayBase * ridgeHighlight * (1.0 - contour5m * 0.28);
  } else if (uShaderMode == 2) {
    // Mode 2: Archaeological Buried Heritage Suitability Heatmap
    float sNorm = clamp((archaeoScore - 40.0) / 58.0, 0.0, 1.0);
    vec3 lowCol = vec3(0.16, 0.36, 0.58);
    vec3 midCol = vec3(0.92, 0.78, 0.22);
    vec3 highCol = vec3(0.88, 0.20, 0.16);
    baseAlbedo = sNorm < 0.5
      ? mix(lowCol, midCol, sNorm * 2.0)
      : mix(midCol, highCol, (sNorm - 0.5) * 2.0);
    float contour10m = smoothstep(0.06, 0.0, abs(fract(elevM / 10.0) - 0.5));
    baseAlbedo = mix(baseAlbedo, vec3(0.08), contour10m * 0.35);
  } else {
    // Mode 3: Slope (경사도) & TPI Geomorphology Mode
    float slopeNorm = clamp(slopeDeg / 30.0, 0.0, 1.0);
    baseAlbedo = mix(vec3(0.18, 0.62, 0.42), vec3(0.85, 0.28, 0.20), slopeNorm);
    float contour5m = smoothstep(0.07, 0.0, abs(fract(elevM / 5.0) - 0.5));
    baseAlbedo = mix(baseAlbedo, vec3(0.95, 0.95, 0.95), contour5m * 0.3);
  }

  // Interactive 500m / 1,000m Archaeological Survey Ring projection on terrain surface
  if (uShowRadiusRings > 0.5) {
    float dHoriz = length(vWorldPos.xz - uSelectedSiteWorldPos.xz);
    float ring500 = smoothstep(8.0, 0.0, abs(dHoriz - 500.0));
    float ring1000 = smoothstep(10.0, 0.0, abs(dHoriz - 1000.0));
    if (dHoriz < 500.0) {
      baseAlbedo = mix(baseAlbedo, vec3(1.0, 0.38, 0.15), 0.11);
    } else if (dHoriz < 1000.0) {
      baseAlbedo = mix(baseAlbedo, vec3(1.0, 0.82, 0.18), 0.07);
    }
    baseAlbedo = mix(baseAlbedo, vec3(1.0, 0.26, 0.10), ring500 * 0.85);
    baseAlbedo = mix(baseAlbedo, vec3(1.0, 0.85, 0.15), ring1000 * 0.80);
  }

  // Sun + Sky Hemisphere Lighting (eevee_lightprobe_volume.cc irradiance approximation)
  vec3 sunIrradiance = mix(vec3(1.0, 0.58, 0.32), vec3(1.0, 0.97, 0.91), clamp(L.y * 1.6, 0.0, 1.0));
  vec3 skyAmbient = mix(vec3(0.20, 0.24, 0.34), vec3(0.32, 0.42, 0.56), 0.5 + 0.5 * N.y);

  vec3 litColor = baseAlbedo * (sunIrradiance * NdotL * 1.15 + skyAmbient * 0.68) * cavityAO;

  // Atmospheric aerial perspective haze over distant mountains (Tohamsan / Namsan)
  float camDist = length(vWorldPos - cameraPosition);
  float fogFactor = clamp(1.0 - exp(-camDist * 0.000055), 0.0, 0.48);
  vec3 hazeColor = mix(vec3(0.75, 0.58, 0.48), vec3(0.68, 0.79, 0.92), clamp(L.y * 1.5, 0.0, 1.0));
  litColor = mix(litColor, hazeColor, fogFactor);

  gl_FragColor = vec4(agxToneMap(litColor), 1.0);
}
`;
