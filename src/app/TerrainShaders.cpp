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
out vec4 oColor;

float hAt(ivec2 p) {
  return texelFetch(uHeight, clamp(p, ivec2(0), uGrid - ivec2(1)), 0).r;
}

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

void main() {
  ivec2 p = ivec2(vPix + 0.5);
  float dzdx = (hAt(p + ivec2(1, 0)) - hAt(p - ivec2(1, 0))) / (2.0 * uPixel);
  float dzdy = (hAt(p - ivec2(0, 1)) - hAt(p + ivec2(0, 1))) / (2.0 * uPixel);
  vec3 n = normalize(vec3(-dzdx * uZScale, -dzdy * uZScale, 1.0));
  float lambert = max(dot(n, normalize(uSunDir)), 0.0);
  float light = 0.28 + 0.72 * lambert;
  float slopeDeg = degrees(acos(clamp(normalize(vec3(-dzdx, -dzdy, 1.0)).z, 0.0, 1.0)));
  vec3 base;
  if (uMode == 0) {
    base = vec3(0.78, 0.74, 0.68) * light;
  } else if (uMode == 1) {
    base = ramp((vZ - uZMin) / max(uZMax - uZMin, 1.0)) * light;
  } else {
    base = heat(slopeDeg / 45.0) * (0.55 + 0.45 * lambert);
  }
  oColor = vec4(pow(base, vec3(1.0 / 2.2)), 1.0);
}
)GLSL";

}  // namespace TerrainShaders
