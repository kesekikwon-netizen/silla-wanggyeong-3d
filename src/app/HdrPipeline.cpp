#include "HdrPipeline.h"

#include <QDebug>
#include <algorithm>
#include <cmath>

#include "AgxView.h"
#include "NishitaSky.h"
#include "PostShaders.h"

unsigned HdrPipeline::makeProgram(const char* vsSrc, const char* fsSrc) {
  auto compile = [this](GLenum type, const char* src) -> unsigned {
    const unsigned sh = gl_->glCreateShader(type);
    gl_->glShaderSource(sh, 1, &src, nullptr);
    gl_->glCompileShader(sh);
    int ok = 0;
    gl_->glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
      char log[4096];
      gl_->glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
      qCritical("후처리 셰이더 컴파일 실패: %s", log);
      return 0;
    }
    return sh;
  };
  const unsigned vs = compile(GL_VERTEX_SHADER, vsSrc), fs = compile(GL_FRAGMENT_SHADER, fsSrc);
  if (!vs || !fs) return 0;
  const unsigned prog = gl_->glCreateProgram();
  gl_->glAttachShader(prog, vs);
  gl_->glAttachShader(prog, fs);
  gl_->glLinkProgram(prog);
  gl_->glDeleteShader(vs);
  gl_->glDeleteShader(fs);
  int ok = 0;
  gl_->glGetProgramiv(prog, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[4096];
    gl_->glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
    qCritical("후처리 셰이더 링크 실패: %s", log);
    return 0;
  }
  return prog;
}

bool HdrPipeline::init(QOpenGLFunctions_3_3_Core* gl, const std::vector<float>& lut, int lutSize, QString* errorOut) {
  gl_ = gl;
  skyProg_ = makeProgram(PostShaders::kFullscreenVert, PostShaders::kSkyFrag);
  linProg_ = makeProgram(PostShaders::kFullscreenVert, PostShaders::kLinearizeFrag);
  aoProg_ = makeProgram(PostShaders::kFullscreenVert, PostShaders::kAoFrag);
  blurProg_ = makeProgram(PostShaders::kFullscreenVert, PostShaders::kBlurFrag);
  compProg_ = makeProgram(PostShaders::kFullscreenVert, PostShaders::kCompositeFrag);
  if (!skyProg_ || !linProg_ || !aoProg_ || !blurProg_ || !compProg_) {
    if (errorOut) *errorOut = QStringLiteral("후처리 셰이더를 만들지 못했습니다.");
    return false;
  }
  gl_->glGenVertexArrays(1, &vao_);
  // AgX LUT: 57³ RGB32F. 텍셀 하나가 격자점 하나(texelFetch 로 사면체 보간).
  agxSize_ = lutSize;
  gl_->glGenTextures(1, &lutTex_);
  gl_->glBindTexture(GL_TEXTURE_3D, lutTex_);
  gl_->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  gl_->glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB32F, lutSize, lutSize, lutSize, 0, GL_RGB, GL_FLOAT, lut.data());
  gl_->glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl_->glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gl_->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  ready_ = true;
  return true;
}

void HdrPipeline::release() {
  if (!gl_) return;
  for (unsigned p : {skyProg_, linProg_, aoProg_, blurProg_, compProg_}) if (p) gl_->glDeleteProgram(p);
  unsigned tex[] = {colorTex_, normalTex_, depthTex_, linTex_, aoTex_[0], aoTex_[1], lutTex_};
  gl_->glDeleteTextures(7, tex);
  unsigned fbo[] = {sceneFbo_, linFbo_, aoFbo_[0], aoFbo_[1]};
  gl_->glDeleteFramebuffers(4, fbo);
  if (vao_) gl_->glDeleteVertexArrays(1, &vao_);
  ready_ = false;
}

void HdrPipeline::createTargets(int w, int h) {
  if (w == w_ && h == h_ && sceneFbo_) return;
  w_ = w;
  h_ = h;
  unsigned oldTex[] = {colorTex_, normalTex_, depthTex_, linTex_, aoTex_[0], aoTex_[1]};
  gl_->glDeleteTextures(6, oldTex);
  unsigned oldFbo[] = {sceneFbo_, linFbo_, aoFbo_[0], aoFbo_[1]};
  gl_->glDeleteFramebuffers(4, oldFbo);

  auto tex = [&](unsigned* t, GLint internal, GLenum fmt, GLenum type, bool mips) {
    gl_->glGenTextures(1, t);
    gl_->glBindTexture(GL_TEXTURE_2D, *t);
    gl_->glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, fmt, type, nullptr);
    gl_->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mips ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR);
    gl_->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mips ? GL_NEAREST : GL_LINEAR);
    gl_->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  };
  tex(&colorTex_, GL_RGBA16F, GL_RGBA, GL_FLOAT, false);
  tex(&normalTex_, GL_RGBA16F, GL_RGBA, GL_FLOAT, false);
  tex(&depthTex_, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, false);
  tex(&linTex_, GL_R32F, GL_RED, GL_FLOAT, true);
  tex(&aoTex_[0], GL_R16F, GL_RED, GL_FLOAT, false);
  tex(&aoTex_[1], GL_R16F, GL_RED, GL_FLOAT, false);
  mips_ = 1 + int(std::floor(std::log2(double(std::max(w, h)))));
  gl_->glBindTexture(GL_TEXTURE_2D, linTex_);
  gl_->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mips_ - 1);

  gl_->glGenFramebuffers(1, &sceneFbo_);
  gl_->glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
  gl_->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex_, 0);
  gl_->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normalTex_, 0);
  gl_->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex_, 0);
  gl_->glGenFramebuffers(1, &linFbo_);
  gl_->glBindFramebuffer(GL_FRAMEBUFFER, linFbo_);
  gl_->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, linTex_, 0);
  for (int i = 0; i < 2; ++i) {
    gl_->glGenFramebuffers(1, &aoFbo_[i]);
    gl_->glBindFramebuffer(GL_FRAMEBUFFER, aoFbo_[i]);
    gl_->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, aoTex_[i], 0);
  }
}

void HdrPipeline::beginScene(int w, int h) {
  createTargets(w, h);
  gl_->glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
  const GLenum bufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
  gl_->glDrawBuffers(2, bufs);
  gl_->glViewport(0, 0, w, h);
  gl_->glEnable(GL_DEPTH_TEST);
  gl_->glDepthFunc(GL_LESS);
  gl_->glDepthMask(GL_TRUE);
  gl_->glDisable(GL_BLEND);
  const float zero[4] = {0, 0, 0, 0};
  gl_->glClearBufferfv(GL_COLOR, 0, zero);
  gl_->glClearBufferfv(GL_COLOR, 1, zero);
  const float one = 1.0f;
  gl_->glClearBufferfv(GL_DEPTH, 0, &one);
}

void HdrPipeline::fullscreen() {
  gl_->glBindVertexArray(vao_);
  gl_->glDrawArrays(GL_TRIANGLES, 0, 3);
  gl_->glBindVertexArray(0);
}

void HdrPipeline::drawSky(const QMatrix4x4& viewProj, const Params& p) {
  gl_->glUseProgram(skyProg_);
  const QMatrix4x4 inv = viewProj.inverted();
  gl_->glUniformMatrix4fv(gl_->glGetUniformLocation(skyProg_, "uInvViewProj"), 1, GL_FALSE, inv.constData());
  const QVector3D sd = p.sunDir.normalized();
  gl_->glUniform3f(gl_->glGetUniformLocation(skyProg_, "uSunDir"), sd.x(), sd.y(), sd.z());
  gl_->glUniform1f(gl_->glGetUniformLocation(skyProg_, "uSunIrr"), p.sunIrradiance);
  gl_->glUniform1f(gl_->glGetUniformLocation(skyProg_, "uEyeAlt"), p.eyeAltitudeM);
  gl_->glDepthFunc(GL_LEQUAL);  // 깊이 1(배경)에만
  gl_->glDepthMask(GL_FALSE);
  fullscreen();
  gl_->glDepthMask(GL_TRUE);
  gl_->glDepthFunc(GL_LESS);
}

void HdrPipeline::finish(const QMatrix4x4& proj, const Params& p, unsigned targetFbo, int w, int h) {
  gl_->glDisable(GL_DEPTH_TEST);
  const float projA = proj(2, 2), projB = proj(2, 3);
  // 1) 선형 깊이 + 밉 체인
  gl_->glBindFramebuffer(GL_FRAMEBUFFER, linFbo_);
  gl_->glViewport(0, 0, w, h);
  gl_->glUseProgram(linProg_);
  gl_->glActiveTexture(GL_TEXTURE0);
  gl_->glBindTexture(GL_TEXTURE_2D, depthTex_);
  gl_->glUniform1i(gl_->glGetUniformLocation(linProg_, "uDepth"), 0);
  gl_->glUniform2f(gl_->glGetUniformLocation(linProg_, "uProjAB"), projA, projB);
  fullscreen();
  gl_->glBindTexture(GL_TEXTURE_2D, linTex_);
  gl_->glGenerateMipmap(GL_TEXTURE_2D);

  int aoIndex = 0;
  if (p.ambientOcclusion) {
    // 2) GTAO
    gl_->glBindFramebuffer(GL_FRAMEBUFFER, aoFbo_[0]);
    gl_->glUseProgram(aoProg_);
    gl_->glActiveTexture(GL_TEXTURE0);
    gl_->glBindTexture(GL_TEXTURE_2D, linTex_);
    gl_->glUniform1i(gl_->glGetUniformLocation(aoProg_, "uLinDepth"), 0);
    gl_->glActiveTexture(GL_TEXTURE1);
    gl_->glBindTexture(GL_TEXTURE_2D, normalTex_);
    gl_->glUniform1i(gl_->glGetUniformLocation(aoProg_, "uNormal"), 1);
    gl_->glUniform2f(gl_->glGetUniformLocation(aoProg_, "uProjScale"), proj(0, 0), proj(1, 1));
    gl_->glUniform2f(gl_->glGetUniformLocation(aoProg_, "uResolution"), float(w), float(h));
    gl_->glUniform1f(gl_->glGetUniformLocation(aoProg_, "uRadius"), p.aoRadiusM);
    gl_->glUniform1f(gl_->glGetUniformLocation(aoProg_, "uMaxPixels"), p.aoMaxPixels * float(h) / 1080.0f);
    gl_->glUniform1f(gl_->glGetUniformLocation(aoProg_, "uAlbedoLum"), 0.3f);
    gl_->glUniform1f(gl_->glGetUniformLocation(aoProg_, "uLodFactor"), 0.5f);
    fullscreen();
    // 3) 깊이를 보는 블러
    gl_->glBindFramebuffer(GL_FRAMEBUFFER, aoFbo_[1]);
    gl_->glUseProgram(blurProg_);
    gl_->glActiveTexture(GL_TEXTURE0);
    gl_->glBindTexture(GL_TEXTURE_2D, aoTex_[0]);
    gl_->glUniform1i(gl_->glGetUniformLocation(blurProg_, "uAo"), 0);
    gl_->glActiveTexture(GL_TEXTURE1);
    gl_->glBindTexture(GL_TEXTURE_2D, linTex_);
    gl_->glUniform1i(gl_->glGetUniformLocation(blurProg_, "uLinDepth"), 1);
    gl_->glUniform2f(gl_->glGetUniformLocation(blurProg_, "uTexel"), 1.0f / float(w), 1.0f / float(h));
    fullscreen();
    aoIndex = 1;
  }

  // 4) 합성 + AgX
  gl_->glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
  gl_->glViewport(0, 0, w, h);
  gl_->glUseProgram(compProg_);
  gl_->glActiveTexture(GL_TEXTURE0);
  gl_->glBindTexture(GL_TEXTURE_2D, colorTex_);
  gl_->glUniform1i(gl_->glGetUniformLocation(compProg_, "uHdr"), 0);
  gl_->glActiveTexture(GL_TEXTURE1);
  gl_->glBindTexture(GL_TEXTURE_2D, aoTex_[aoIndex]);
  gl_->glUniform1i(gl_->glGetUniformLocation(compProg_, "uAo"), 1);
  gl_->glActiveTexture(GL_TEXTURE2);
  gl_->glBindTexture(GL_TEXTURE_3D, lutTex_);
  gl_->glUniform1i(gl_->glGetUniformLocation(compProg_, "uAgxLut"), 2);
  gl_->glUniform1i(gl_->glGetUniformLocation(compProg_, "uAgxSize"), agxSize_);
  gl_->glUniform1i(gl_->glGetUniformLocation(compProg_, "uUseAo"), p.ambientOcclusion ? 1 : 0);
  gl_->glUniform1f(gl_->glGetUniformLocation(compProg_, "uExposure"), p.exposure);
  const std::array<float, 9>& m = strata3d::AgxView::rec709ToEGamut();  // 행 우선 → GLSL 열 우선이므로 전치해서 넘긴다
  gl_->glUniformMatrix3fv(gl_->glGetUniformLocation(compProg_, "uToEGamut"), 1, GL_TRUE, m.data());
  fullscreen();
  gl_->glActiveTexture(GL_TEXTURE0);
}
