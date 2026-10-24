#pragma once

// HDR 파이프라인 셰이더(OpenGL 3.3 Core). 하늘은 engine/NishitaSky.cpp 와 같은 식,
// AO 는 Blender 4.2.3 eevee_ambient_occlusion_lib.glsl(docs/rea 에서 추출한 원문)의 이식, 색은 AgX(OCIO 기준표와 대조한 식)다.
namespace PostShaders {
extern const char* const kFullscreenVert;
extern const char* const kSkyFrag;        // 배경(깊이 1)에 Nishita 하늘
extern const char* const kLinearizeFrag;  // 깊이 → 선형 거리(R32F, 밉맵용)
extern const char* const kAoFrag;         // Blender GTAO 이식
extern const char* const kBlurFrag;       // 깊이를 보는 5×5 블러
extern const char* const kCompositeFrag;  // 환경 차폐 + 노출 + AgX
}  // namespace PostShaders
