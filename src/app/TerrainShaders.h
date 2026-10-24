#pragma once

// OpenGL 3.3 Core GLSL. 지형 높이는 R32F 텍스처에서 정점 셰이더가 직접 읽는다(CDLOD 방식).
namespace TerrainShaders {
extern const char* const kVertex;
extern const char* const kFragment;
}  // namespace TerrainShaders
