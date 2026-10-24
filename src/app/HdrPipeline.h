#pragma once

#include <QMatrix4x4>
#include <QOpenGLFunctions_3_3_Core>
#include <QVector3D>
#include <vector>

// 장면을 선형 HDR(RGBA16F) + 뷰 공간 법선으로 그리고, 하늘 → 선형 깊이 → GTAO → 블러 → AgX 합성까지 맡는 후처리.
// 지형·건물 셰이더는 beginScene() 뒤에 MRT(색, 법선)로 그린다.
class HdrPipeline {
 public:
  struct Params {
    QVector3D sunDir{-0.55f, 0.55f, 0.62f};
    float sunIrradiance = 12.0f;
    float eyeAltitudeM = 1.7f;
    float exposure = 0.0f;        // 2의 거듭제곱 단(stop)
    bool ambientOcclusion = true;
    float aoRadiusM = 30.0f;      // 건물(수 m)과 고분(수 m~수십 m) 규모
    float aoMaxPixels = 140.0f;
  };

  bool init(QOpenGLFunctions_3_3_Core* gl, const std::vector<float>& agxLut, int agxSize, QString* errorOut);
  void release();
  bool ready() const { return ready_; }

  void beginScene(int w, int h);  // 장면 FBO 를 묶고 비운다
  // 배경(깊이 1)에만 하늘을 그린다. 장면을 다 그린 뒤 부른다.
  void drawSky(const QMatrix4x4& viewProj, const Params& p);
  // 선형 깊이 → AO → 합성. targetFbo 로 8비트 sRGB 출력.
  void finish(const QMatrix4x4& proj, const Params& p, unsigned targetFbo, int w, int h);

 private:
  unsigned makeProgram(const char* vs, const char* fs);
  void createTargets(int w, int h);
  void fullscreen();

  QOpenGLFunctions_3_3_Core* gl_ = nullptr;
  bool ready_ = false;
  int w_ = 0, h_ = 0;
  unsigned vao_ = 0;
  unsigned sceneFbo_ = 0, colorTex_ = 0, normalTex_ = 0, depthTex_ = 0;
  unsigned linFbo_ = 0, linTex_ = 0;
  unsigned aoFbo_[2] = {0, 0}, aoTex_[2] = {0, 0};
  unsigned lutTex_ = 0;
  int agxSize_ = 0;
  unsigned skyProg_ = 0, linProg_ = 0, aoProg_ = 0, blurProg_ = 0, compProg_ = 0;
  int mips_ = 1;
};
