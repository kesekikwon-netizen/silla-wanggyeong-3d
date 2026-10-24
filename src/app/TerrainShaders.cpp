#include "TerrainShaders.h"

namespace TerrainShaders {

// aTpl = (u, v, skirt). u,v 는 조각 안 0..1, skirt=1 이면 조각 가장자리 바깥 띠(치마)의 정점.
// aChunk = (col0, row0, sizePx, level), 격자 칸 단위.
const char* const kVertex = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aTpl;
layout(location = 1) in vec4 aChunk;
uniform mat4 uViewProj;
uniform sampler2D uHeight;
uniform ivec2 uGrid;
uniform float uPixel;
uniform float uZScale;
uniform float uSkirt;
out vec2 vPix;
out float vZ;
void main() {
  vec2 pc = clamp(aChunk.xy + aTpl.xy * aChunk.z, vec2(0.0), vec2(uGrid - ivec2(1)));
  float h = texelFetch(uHeight, ivec2(pc + 0.5), 0).r;
  float drop = aTpl.z * aChunk.z * uPixel * uSkirt;
  vec3 world = vec3((pc.x - float(uGrid.x - 1) * 0.5) * uPixel,
                    (float(uGrid.y - 1) * 0.5 - pc.y) * uPixel,
                    h * uZScale - drop);
  vPix = pc;
  vZ = h;
  gl_Position = uViewProj * vec4(world, 1.0);
}
)GLSL";

// mode 0: 점토(조명만, 기복 판독용) / 1: 표고 색띠 + 조명 / 2: 경사도 히트맵
const char* const kFragment = R"GLSL(
#version 330 core
in vec2 vPix;
in float vZ;
uniform sampler2D uHeight;
uniform ivec2 uGrid;
uniform float uPixel;
uniform float uZScale;
uniform float uZMin;
uniform float uZMax;
uniform vec3 uSunDir;
uniform int uMode;
uniform sampler2D uSat;
uniform sampler2D uSatDetail;
uniform vec4 uDetailRect;
uniform int uHasDetail;
uniform mat3 uViewRot;     // 지역 좌표 → 뷰 공간 회전
uniform vec3 uSunColor;    // 태양 직사광(투과율 반영, 선형)
uniform vec3 uAmbient;     // 하늘 환경광(선형)
layout(location = 0) out vec4 oColor;   // rgb 선형 복사휘도, a 환경광 비율(환경 차폐가 곱해질 몫)
layout(location = 1) out vec4 oNormal;  // 뷰 공간 법선(0.5 배 + 0.5), a=1 이면 표면

vec3 ramp(float t) {
  const vec3 c0 = vec3(0.16, 0.38, 0.22);
  const vec3 c1 = vec3(0.55, 0.62, 0.30);
  const vec3 c2 = vec3(0.80, 0.72, 0.45);
  const vec3 c3 = vec3(0.62, 0.48, 0.36);
  const vec3 c4 = vec3(0.95, 0.95, 0.95);
  t = clamp(t, 0.0, 1.0) * 4.0;
  if (t < 1.0) return mix(c0, c1, t);
  if (t < 2.0) return mix(c1, c2, t - 1.0);
  if (t < 3.0) return mix(c2, c3, t - 2.0);
  return mix(c3, c4, t - 3.0);
}

vec3 heat(float t) {
  t = clamp(t, 0.0, 1.0);
  return vec3(smoothstep(0.35, 0.75, t), 1.0 - abs(t - 0.45) * 1.8, 1.0 - smoothstep(0.1, 0.5, t));
}

// 선형 필터로 칸 사이 값을 이어 붙이고(vPix 는 칸 가운데 기준이 아니라 정점 위치), 소벨 3×3 으로
// 법선을 구한다. 칸마다 법선이 뚝뚝 끊겨 생기던 줄무늬를 없애기 위함.
float hLin(vec2 pix) {
  return texture(uHeight, (pix + 0.5) / vec2(uGrid)).r;
}

void main() {
  float dzdx = ((hLin(vPix + vec2(1, -1)) + 2.0 * hLin(vPix + vec2(1, 0)) + hLin(vPix + vec2(1, 1))) -
                (hLin(vPix + vec2(-1, -1)) + 2.0 * hLin(vPix + vec2(-1, 0)) + hLin(vPix + vec2(-1, 1)))) / (8.0 * uPixel);
  float dzdy = ((hLin(vPix + vec2(-1, -1)) + 2.0 * hLin(vPix + vec2(0, -1)) + hLin(vPix + vec2(1, -1))) -
                (hLin(vPix + vec2(-1, 1)) + 2.0 * hLin(vPix + vec2(0, 1)) + hLin(vPix + vec2(1, 1)))) / (8.0 * uPixel);
  vec3 n = normalize(vec3(-dzdx * uZScale, -dzdy * uZScale, 1.0));
  float lambert = max(dot(n, normalize(uSunDir)), 0.0);
  float slopeDeg = degrees(acos(clamp(normalize(vec3(-dzdx, -dzdy, 1.0)).z, 0.0, 1.0)));
  vec3 albedo = vec3(0.5);
  float ambFrac;
  vec3 radiance;
  vec3 direct = uSunColor * lambert;
  vec3 amb = uAmbient * (0.55 + 0.45 * n.z);
  if (uMode == 3) {
    // 위성 실사: 영상에 이미 들어 있는 햇빛·그림자 위에 지형 기복만 살짝 얹는다.
    vec2 uv = (vPix + 0.5) / vec2(uGrid);
    vec3 sat = texture(uSat, uv).rgb;
    if (uHasDetail == 1) {
      vec2 duv = (uv - uDetailRect.xy) / (uDetailRect.zw - uDetailRect.xy);
      float edge = min(min(duv.x, 1.0 - duv.x), min(duv.y, 1.0 - duv.y));  // 가장자리에서 부드럽게 섞는다
      sat = mix(sat, texture(uSatDetail, duv).rgb, smoothstep(0.0, 0.06, edge));
    }
    albedo = pow(sat, vec3(2.2));
    radiance = albedo * clamp(0.85 + 0.55 * (lambert - 0.6), 0.5, 1.2);
    ambFrac = 0.35;
  } else if (uMode == 2) {
    radiance = pow(heat(slopeDeg / 45.0), vec3(2.2)) * (0.55 + 0.45 * lambert);
    ambFrac = 0.0;
  } else {
    albedo = (uMode == 0) ? pow(vec3(0.78, 0.74, 0.68), vec3(2.2)) : pow(ramp((vZ - uZMin) / max(uZMax - uZMin, 1.0)), vec3(2.2));
    radiance = albedo * (direct + amb);
    float dl = dot(direct, vec3(0.333)), al = dot(amb, vec3(0.333));
    ambFrac = al / max(dl + al, 1e-4);
  }
  oColor = vec4(radiance, ambFrac);
  oNormal = vec4(normalize(uViewRot * n) * 0.5 + 0.5, 1.0);
}
)GLSL";

const char* const kSceneVertex = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColor;
uniform mat4 uViewProj;
uniform float uZScale;
out vec3 vNormal;
out vec4 vColor;
void main() {
  float zs = max(uZScale, 0.1);
  vNormal = normalize(vec3(aNormal.xy * zs, aNormal.z));
  vColor = aColor;
  gl_Position = uViewProj * vec4(aPos.xy, aPos.z * zs, 1.0);
}
)GLSL";

const char* const kSceneFragment = R"GLSL(
#version 330 core
in vec3 vNormal;
in vec4 vColor;
uniform vec3 uSunDir;
uniform mat3 uViewRot;
uniform vec3 uSunColor;
uniform vec3 uAmbient;
layout(location = 0) out vec4 oColor;
layout(location = 1) out vec4 oNormal;
void main() {
  vec3 n = normalize(vNormal);
  if (!gl_FrontFacing) n = -n;
  float lambert = max(dot(n, normalize(uSunDir)), 0.0);
  vec3 albedo = pow(vColor.rgb, vec3(2.2));
  vec3 direct = uSunColor * lambert, amb = uAmbient * (0.55 + 0.45 * n.z);
  oColor = vec4(albedo * (direct + amb), dot(amb, vec3(0.333)) / max(dot(direct + amb, vec3(0.333)), 1e-4));
  oNormal = vec4(normalize(uViewRot * n) * 0.5 + 0.5, 1.0);
}
)GLSL";

}  // namespace TerrainShaders
