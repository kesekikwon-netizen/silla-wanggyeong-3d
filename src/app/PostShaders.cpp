#include "PostShaders.h"

namespace PostShaders {

const char* const kFullscreenVert = R"GLSL(
#version 330 core
out vec2 vUv;
void main() {
  // 정점 번호 0,1,2 로 화면을 덮는 삼각형 하나
  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
  vUv = p;
  gl_Position = vec4(p * 2.0 - 1.0, 1.0, 1.0);  // z = w → 깊이 1.0 (배경)
}
)GLSL";

// NishitaSky.cpp 와 같은 상수·식. 상수의 출처는 docs/sky-constants.md.
const char* const kSkyFrag = R"GLSL(
#version 330 core
in vec2 vUv;
uniform mat4 uInvViewProj;
uniform vec3 uEye;       // 지역 좌표의 눈 위치(대기 계산에는 쓰지 않고 방향만 만든다)
uniform vec3 uSunDir;    // 정규화, +Z 위
uniform float uSunIrr;
uniform float uEyeAlt;
layout(location = 0) out vec4 oColor;
layout(location = 1) out vec4 oNormal;

const float R_E = 6360000.0, R_A = 6420000.0, H_R = 8000.0, H_M = 1200.0, G = 0.76, B_M = 2.22e-5;
const vec3 B_R = vec3(5.8e-6, 13.5e-6, 33.1e-6), B_O = vec3(0.650e-6, 1.881e-6, 0.085e-6);
const float PI = 3.14159265358979;

bool hitSphere(vec3 o, vec3 d, float r, out float t0, out float t1) {
  float b = dot(o, d), c = dot(o, o) - r * r, disc = b * b - c;
  if (disc < 0.0) return false;
  float s = sqrt(disc);
  t0 = -b - s; t1 = -b + s;
  return true;
}
float ozone(float h) {
  if (h <= 10000.0 || h >= 40000.0) return 0.0;
  return h < 25000.0 ? (h - 10000.0) / 15000.0 : 1.0 - (h - 25000.0) / 15000.0;
}
vec3 opticalDepthToSun(vec3 p, vec3 sd) {
  float t0, t1;
  if (!hitSphere(p, sd, R_A, t0, t1)) return vec3(0.0);
  float dt = t1 / 12.0, r = 0.0, m = 0.0, o = 0.0;
  for (int i = 0; i < 12; ++i) {
    float h = length(p + sd * ((float(i) + 0.5) * dt)) - R_E;
    r += exp(-h / H_R); m += exp(-h / H_M); o += ozone(h);
  }
  return B_R * (r * dt) + vec3(B_M * 1.11 * m * dt) + B_O * (o * dt);
}
float phaseR(float c) { return 3.0 / (16.0 * PI) * (1.0 + c * c); }
float phaseM(float c) {
  float g2 = G * G;
  return 3.0 / (8.0 * PI) * (1.0 - g2) * (1.0 + c * c) / ((2.0 + g2) * pow(1.0 + g2 - 2.0 * G * c, 1.5));
}
vec3 skyRadiance(vec3 dir, vec3 sd) {
  vec3 o = vec3(0.0, 0.0, R_E + uEyeAlt);
  float t0, t1, e0, e1;
  if (!hitSphere(o, dir, R_A, t0, t1)) return vec3(0.0);
  float len = t1;
  if (hitSphere(o, dir, R_E, e0, e1) && e0 > 0.0) len = e0;
  const int N = 24;
  float dt = len / float(N), mu = dot(dir, sd);
  float pr = phaseR(mu), pm = phaseM(mu);
  vec3 total = vec3(0.0), viewDepth = vec3(0.0);
  for (int i = 0; i < N; ++i) {
    vec3 p = o + dir * ((float(i) + 0.5) * dt);
    float h = length(p) - R_E;
    if (h < 0.0) break;
    float dr = exp(-h / H_R), dm = exp(-h / H_M), doz = ozone(h);
    viewDepth += (B_R * dr + vec3(B_M * 1.11 * dm) + B_O * doz) * dt;
    float s0, s1;
    if (hitSphere(p, sd, R_E, s0, s1) && s0 > 0.0) continue;
    vec3 att = exp(-(viewDepth + opticalDepthToSun(p, sd)));
    total += att * (B_R * (dr * pr) + vec3(B_M * dm * pm)) * dt;
  }
  return total * uSunIrr;
}
void main() {
  vec4 a = uInvViewProj * vec4(vUv * 2.0 - 1.0, -1.0, 1.0);
  vec4 b = uInvViewProj * vec4(vUv * 2.0 - 1.0, 1.0, 1.0);
  vec3 dir = normalize(b.xyz / b.w - a.xyz / a.w);
  dir.z = max(dir.z, 0.002);  // 지평선 아래는 지평선 색으로 (지면은 지형이 가린다)
  oColor = vec4(skyRadiance(normalize(dir), normalize(uSunDir)), 0.0);  // a = 0: 환경 차폐 대상 아님
  oNormal = vec4(0.0);
}
)GLSL";

const char* const kLinearizeFrag = R"GLSL(
#version 330 core
in vec2 vUv;
uniform sampler2D uDepth;
uniform vec2 uProjAB;  // proj(2,2), proj(2,3)
out float oLin;
void main() {
  float d = texture(uDepth, vUv).r;
  float zNdc = d * 2.0 - 1.0;
  oLin = d >= 1.0 ? 1.0e9 : uProjAB.y / (zNdc + uProjAB.x);  // 양수 거리(바깥 하늘은 매우 멀리)
}
)GLSL";

// Blender 4.2.3 eevee_ambient_occlusion_lib.glsl 의 이식(Jimenez 2016 GTAO): 슬라이스 2개 × 4방향의 수평선 탐색
// (ambient_ambient_occlusion_search_horizon) → 안쪽 적분 식 7·8 (ambient_occlusion_eval) → 다중 반사 보정.
// 바뀐 점: HiZ 광선 추적 대신 선형 깊이 밉 체인, 청색 소음 LUT 대신 interleaved gradient noise,
// 월드 공간 대신 뷰 공간, Ng 는 깊이 미분으로 복원.
const char* const kAoFrag = R"GLSL(
#version 330 core
in vec2 vUv;
uniform sampler2D uLinDepth;
uniform sampler2D uNormal;
uniform vec2 uProjScale;   // proj(0,0), proj(1,1)
uniform vec2 uResolution;
uniform float uRadius;     // 미터
uniform float uMaxPixels;  // 탐색 반경 상한(픽셀)
uniform float uAlbedoLum;
uniform float uLodFactor;
out float oVis;
const float PI = 3.14159265358979;
const float SAMPLES = 10.0;

float acosFast(float x) { return acos(clamp(x, -1.0, 1.0)); }
float square(float x) { return x * x; }
float saturate(float x) { return clamp(x, 0.0, 1.0); }
float ign(vec2 p, float k) { return fract(52.9829189 * fract(dot(p + k * 5.588238, vec2(0.06711056, 0.00583715)))); }

vec3 viewPos(vec2 uv, float d) { return vec3((uv * 2.0 - 1.0) / uProjScale * d, -d); }
vec2 toPixel(vec3 vp) { vec2 ndc = vec2(vp.x * uProjScale.x, vp.y * uProjScale.y) / -vp.z; return (ndc * 0.5 + 0.5) * uResolution; }

float searchHorizon(vec3 vI, vec3 vP, float noise, vec2 originPx, vec2 dirPx, float maxTime) {
  float h = -1.0;  // cos(pi)
  maxTime -= 1.0;
  if (maxTime <= 2.0) return acosFast(h);
  float prevTime, time = 0.0;
  for (float iter = 0.0; time < maxTime && iter < SAMPLES; iter++) {
    prevTime = time;
    time = 1.0 + iter + square((iter + noise) / SAMPLES) * maxTime;
    float stride = time - prevTime;
    float lod = (log2(stride) - noise) * uLodFactor;
    vec2 px = originPx + dirPx * time;
    vec2 uv = px / uResolution;
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) break;
    float d = textureLod(uLinDepth, uv, floor(max(lod, 0.0))).r;
    if (d > 1.0e8) continue;  // 하늘은 건너뛴다
    vec3 s = viewPos(uv, d);
    vec3 omega = s - vP;
    float len = length(omega);
    float sh = dot(vI, omega / len);
    float distFac = square(saturate(len / uRadius));  // 구 감쇠
    h = mix(max(h, sh), h, distFac);
  }
  return acosFast(h);
}

void main() {
  float d0 = texture(uLinDepth, vUv).r;
  vec4 nrm = texture(uNormal, vUv);
  if (d0 > 1.0e8 || nrm.a < 0.5) { oVis = 1.0; return; }
  vec3 vP = viewPos(vUv, d0);
  vec3 N = normalize(nrm.xyz * 2.0 - 1.0);
  vec3 Ng = normalize(cross(dFdx(vP), dFdy(vP)));
  if (dot(Ng, N) < 0.0) Ng = -Ng;
  vec3 V = normalize(-vP);
  vec2 noise = vec2(ign(gl_FragCoord.xy, 0.0), ign(gl_FragCoord.xy, 1.0));
  float j = noise.x * (PI * 0.5);
  vec2 dir = vec2(cos(j), sin(j));
  vec2 centerPx = vUv * uResolution;
  vec4 horizons = vec4(PI, -PI, PI, -PI);
  for (int i = 0; i < 2; i++) {
    // 뷰 공간의 광선 한 점을 화면에 투영해 픽셀 단위 방향·길이를 만든다(Blender raytrace_screenspace_ray_create 의 역할)
    vec2 endPx = toPixel(vP + vec3(dir, 0.0) * uRadius);
    vec2 dpx = endPx - centerPx;
    float lenPx = min(length(dpx), uMaxPixels);
    vec2 dnorm = length(dpx) > 1e-4 ? normalize(dpx) : vec2(1.0, 0.0);
    horizons[i * 2 + 0] = searchHorizon(V, vP, noise.y, centerPx, dnorm, lenPx);
    horizons[i * 2 + 1] = -searchHorizon(V, vP, noise.y, centerPx, -dnorm, lenPx);
    dir = vec2(-dir.y, dir.x);
  }
  // ---- ambient_occlusion_eval ----
  j = noise.x * (PI * 0.5);
  dir = vec2(cos(j), sin(j));
  float visibility = 0.0, visibilityError = 0.0;
  for (int i = 0; i < 2; i++) {
    vec3 T = vec3(dir, 0.0);
    vec3 B = normalize(cross(V, T));
    T = normalize(cross(B, V));
    float projNLen = length(N - B * dot(N, B));
    vec3 projN = normalize(N - B * dot(N, B));
    vec3 projNg = normalize(Ng - B * dot(Ng, B));
    vec2 h = (i == 0) ? horizons.xy : horizons.zw;
    float N_sin = dot(projN, T), Ng_sin = dot(projNg, T);
    float N_cos = saturate(dot(projN, V)), Ng_cos = saturate(dot(projNg, V));
    float angleNg = sign(Ng_sin) * acosFast(Ng_cos);
    float angleN = sign(N_sin) * acosFast(N_cos);
    const float maxAngle = PI * 0.5 - 0.05;
    h.x = min(h.x, angleN + maxAngle); h.y = max(h.y, angleN - maxAngle);   // 셰이딩 법선 반구로 자름
    h.x = min(h.x, angleNg + maxAngle); h.y = max(h.y, angleNg - maxAngle);  // 기하 법선 반구로 자름
    float a = dot(-cos(2.0 * h - angleN) + N_cos + 2.0 * h * N_sin, vec2(0.25));  // 안쪽 적분 (식 7)
    visibility += projNLen * a;                                                    // 식 8
    visibilityError += projNLen;
    dir = vec2(-dir.y, dir.x);
  }
  visibility *= 0.5;
  visibilityError *= 0.5;
  // (Blender 는 확산 AO 에서는 visibility_error 보정을 하지 않는다 — 거울 반사 쪽에서만 쓴다)
  visibility = saturate(visibility);
  // 다중 반사 근사(알베도 평균 휘도) — ambient_occlusion_multibounce
  float lum = uAlbedoLum;
  float aa = 2.0404 * lum - 0.3324, bb = -4.7951 * lum + 0.6417, cc = 2.7552 * lum + 0.6903;
  float x = visibility;
  oVis = saturate(max(x, ((x * aa + bb) * x + cc) * x));
}
)GLSL";

const char* const kBlurFrag = R"GLSL(
#version 330 core
in vec2 vUv;
uniform sampler2D uAo;
uniform sampler2D uLinDepth;
uniform vec2 uTexel;
out float oVis;
void main() {
  float d0 = texture(uLinDepth, vUv).r;
  float sum = 0.0, wsum = 0.0;
  for (int y = -2; y <= 2; ++y) {
    for (int x = -2; x <= 2; ++x) {
      vec2 uv = vUv + vec2(x, y) * uTexel;
      float d = texture(uLinDepth, uv).r;
      float w = exp(-abs(d - d0) / max(0.03 * d0, 0.5)) * (1.0 - 0.1 * float(abs(x) + abs(y)));
      sum += texture(uAo, uv).r * w;
      wsum += w;
    }
  }
  oVis = sum / max(wsum, 1e-4);
}
)GLSL";

const char* const kCompositeFrag = R"GLSL(
#version 330 core
in vec2 vUv;
uniform sampler2D uHdr;   // rgb 장면 복사휘도, a = 환경광이 차지하는 비율
uniform sampler2D uAo;
uniform sampler3D uAgxLut;
uniform mat3 uToEGamut;   // 선형 Rec.709 → E-Gamut (행렬은 config.ocio 에서 계산)
uniform float uExposure;
uniform int uAgxSize;
uniform int uUseAo;
out vec4 oColor;

vec3 lutFetch(ivec3 i) { return texelFetch(uAgxLut, i, 0).rgb; }

// OCIO 의 사면체 보간 — engine/AgxView.cpp 와 같은 분기
vec3 tetra(vec3 p) {
  int n = uAgxSize;
  ivec3 i0 = min(ivec3(floor(p)), ivec3(n - 2));
  vec3 f = p - vec3(i0);
  vec3 c000 = lutFetch(i0), c100 = lutFetch(i0 + ivec3(1, 0, 0)), c010 = lutFetch(i0 + ivec3(0, 1, 0)), c001 = lutFetch(i0 + ivec3(0, 0, 1));
  vec3 c110 = lutFetch(i0 + ivec3(1, 1, 0)), c101 = lutFetch(i0 + ivec3(1, 0, 1)), c011 = lutFetch(i0 + ivec3(0, 1, 1)), c111 = lutFetch(i0 + ivec3(1, 1, 1));
  float fx = f.x, fy = f.y, fz = f.z;
  if (fx > fy) {
    if (fy > fz) return c000 + fx * (c100 - c000) + fy * (c110 - c100) + fz * (c111 - c110);
    else if (fx > fz) return c000 + fx * (c100 - c000) + fz * (c101 - c100) + fy * (c111 - c101);
    else return c000 + fz * (c001 - c000) + fx * (c101 - c001) + fy * (c111 - c101);
  } else {
    if (fz > fy) return c000 + fz * (c001 - c000) + fy * (c011 - c001) + fx * (c111 - c011);
    else if (fz > fx) return c000 + fy * (c010 - c000) + fz * (c011 - c010) + fx * (c111 - c011);
    else return c000 + fy * (c010 - c000) + fx * (c110 - c010) + fz * (c111 - c110);
  }
}

vec3 agx(vec3 rec709) {
  vec3 eg = uToEGamut * rec709;
  const float MIN = -12.47393, MAX = 12.5260688117;
  vec3 l = log2(max(eg, vec3(1.0e-30)));
  vec3 p = clamp((l - MIN) / (MAX - MIN), 0.0, 1.0) * float(uAgxSize - 1);
  vec3 v = pow(max(tetra(p), vec3(0.0)), vec3(2.4));
  v = clamp(v, 0.0, 1.0);
  return mix(v * 12.92, 1.055 * pow(v, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), v));
}

void main() {
  vec4 hdr = texture(uHdr, vUv);
  float ao = uUseAo == 1 ? texture(uAo, vUv).r : 1.0;
  vec3 c = hdr.rgb * mix(1.0, ao, hdr.a) * exp2(uExposure);
  oColor = vec4(agx(c), 1.0);
}
)GLSL";

}  // namespace PostShaders
