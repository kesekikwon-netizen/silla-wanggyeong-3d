#include "KaGlTerrainViewport.h"

#include <QMatrix4x4>
#include <QMouseEvent>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

#include "Frustum.h"
#include "TerrainShaders.h"

namespace {
constexpr int kCells = strata3d::TerrainLodTree::kChunkCells;
// 틀은 가장자리에 한 칸씩 바깥 띠(치마)를 더 갖는다: 서로 다른 단계 조각 사이의 균열을 가린다.
constexpr int kRing = kCells + 3;  // -1 .. kCells+1
constexpr float kSkirtFraction = 0.12f;
}  // namespace

KaGlTerrainViewport::KaGlTerrainViewport(QWidget* parent) : QOpenGLWidget(parent) {
  setMinimumSize(480, 320);
  setFocusPolicy(Qt::StrongFocus);
}

KaGlTerrainViewport::~KaGlTerrainViewport() {
  if (!context()) return;
  makeCurrent();
  if (program_) glDeleteProgram(program_);
  if (heightTex_) glDeleteTextures(1, &heightTex_);
  unsigned bufs[3] = {templateVbo_, templateEbo_, instanceVbo_};
  glDeleteBuffers(3, bufs);
  if (vao_) glDeleteVertexArrays(1, &vao_);
  doneCurrent();
}

void KaGlTerrainViewport::setTerrain(std::shared_ptr<const strata3d::TerrainLodTree> tree) {
  pendingUpload_ = std::move(tree);
  update();
}

void KaGlTerrainViewport::setMode(Mode mode) {
  mode_ = mode;
  update();
}

void KaGlTerrainViewport::setZScale(float zScale) {
  zScale_ = zScale;
  update();
}

bool KaGlTerrainViewport::buildProgram() {
  auto compile = [this](GLenum type, const char* src, unsigned* out) {
    const unsigned sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    int ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
      char log[2048];
      glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
      qCritical("셰이더 컴파일 실패: %s", log);
      return false;
    }
    *out = sh;
    return true;
  };
  unsigned vs = 0, fs = 0;
  if (!compile(GL_VERTEX_SHADER, TerrainShaders::kVertex, &vs) ||
      !compile(GL_FRAGMENT_SHADER, TerrainShaders::kFragment, &fs))
    return false;
  program_ = glCreateProgram();
  glAttachShader(program_, vs);
  glAttachShader(program_, fs);
  glLinkProgram(program_);
  glDeleteShader(vs);
  glDeleteShader(fs);
  int ok = 0;
  glGetProgramiv(program_, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[2048];
    glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
    qCritical("셰이더 링크 실패: %s", log);
    return false;
  }
  return true;
}

void KaGlTerrainViewport::buildTemplate() {
  std::vector<float> verts;
  verts.reserve(size_t(kRing) * kRing * 3);
  for (int j = 0; j < kRing; ++j) {
    for (int i = 0; i < kRing; ++i) {
      const int gi = i - 1, gj = j - 1;  // -1 .. kCells+1
      const bool skirt = gi < 0 || gi > kCells || gj < 0 || gj > kCells;
      verts.push_back(float(std::clamp(gi, 0, kCells)) / kCells);
      verts.push_back(float(std::clamp(gj, 0, kCells)) / kCells);
      verts.push_back(skirt ? 1.0f : 0.0f);
    }
  }
  std::vector<unsigned> idx;
  for (int j = 0; j + 1 < kRing; ++j) {
    for (int i = 0; i + 1 < kRing; ++i) {
      const unsigned a = j * kRing + i, b = a + 1, c = a + kRing, d = c + 1;
      idx.insert(idx.end(), {a, c, b, b, c, d});
    }
  }
  templateIndexCount_ = int(idx.size());
  glGenVertexArrays(1, &vao_);
  glBindVertexArray(vao_);
  glGenBuffers(1, &templateVbo_);
  glBindBuffer(GL_ARRAY_BUFFER, templateVbo_);
  glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(verts.size() * sizeof(float)), verts.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
  glGenBuffers(1, &templateEbo_);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, templateEbo_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(idx.size() * sizeof(unsigned)), idx.data(), GL_STATIC_DRAW);
  glGenBuffers(1, &instanceVbo_);
  glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
  glVertexAttribDivisor(1, 1);
  glBindVertexArray(0);
}

void KaGlTerrainViewport::initializeGL() {
  initializeOpenGLFunctions();
  glInfo_ = QStringLiteral("%1 | %2 | GL %3")
                .arg(QString::fromLatin1(reinterpret_cast<const char*>(glGetString(GL_VENDOR))),
                     QString::fromLatin1(reinterpret_cast<const char*>(glGetString(GL_RENDERER))),
                     QString::fromLatin1(reinterpret_cast<const char*>(glGetString(GL_VERSION))));
  if (!buildProgram()) return;
  buildTemplate();
  glEnable(GL_DEPTH_TEST);
}

void KaGlTerrainViewport::resizeGL(int, int) {}

void KaGlTerrainViewport::uploadTerrain() {
  tree_ = std::move(pendingUpload_);
  pendingUpload_.reset();
  if (heightTex_) glDeleteTextures(1, &heightTex_);
  heightTex_ = 0;
  if (!tree_) return;
  const strata3d::DemGrid& g = tree_->grid();
  glGenTextures(1, &heightTex_);
  glBindTexture(GL_TEXTURE_2D, heightTex_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, g.width, g.height, 0, GL_RED, GL_FLOAT, g.z.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  resetCamera();
  emit terrainUploaded();
}

void KaGlTerrainViewport::setCameraPreset(float yawDeg, float pitchDeg, float distanceM) {
  hasPreset_ = true;
  presetYaw_ = yawDeg;
  presetPitch_ = pitchDeg;
  presetDistance_ = distanceM;
}

void KaGlTerrainViewport::resetCamera() {
  const strata3d::DemGrid& g = tree_->grid();
  const float extent = float(std::max(g.width, g.height) * g.pixelSize);
  float zc = (g.zMin + g.zMax) * 0.5f;
  float zAtCenter = 0;
  if (g.sample(g.xAt((g.width - 1) * 0.5 + 0.5), g.yAt((g.height - 1) * 0.5 + 0.5), &zAtCenter)) zc = zAtCenter;
  target_ = QVector3D(0, 0, zc * zScale_);
  distance_ = extent * 0.7f;
  if (hasPreset_) {  // 미리 정한 시점은 지형 한가운데의 실제 표고를 바라본다
    yawDeg_ = presetYaw_;
    pitchDeg_ = presetPitch_;
    distance_ = presetDistance_;
  }
}

QVector3D KaGlTerrainViewport::eyePosition() const {
  const float yaw = qDegreesToRadians(yawDeg_), pitch = qDegreesToRadians(pitchDeg_);
  // yaw 0 = 북쪽에서 남쪽을 향해 본다(눈은 목표의 남쪽).
  return target_ + distance_ * QVector3D(std::sin(yaw) * std::cos(pitch), -std::cos(yaw) * std::cos(pitch), std::sin(pitch));
}

void KaGlTerrainViewport::paintGL() {
  if (!program_) return;
  if (pendingUpload_) uploadTerrain();
  glClearColor(0.74f, 0.80f, 0.88f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (!tree_ || !heightTex_) return;

  const strata3d::DemGrid& g = tree_->grid();
  const float extent = float(std::max(g.width, g.height) * g.pixelSize);
  const QVector3D eye = eyePosition();
  QMatrix4x4 view, proj;
  view.lookAt(eye, target_, QVector3D(0, 0, 1));
  const float aspect = float(width()) / float(std::max(1, height()));
  proj.perspective(45.0f, aspect, std::max(2.0f, distance_ * 0.01f), distance_ + extent * 3.0f);
  const QMatrix4x4 vp = proj * view;
  const strata3d::Frustum frustum(vp);

  QElapsedTimer t;
  t.start();
  const std::vector<strata3d::ChunkInstance> chunks = tree_->select(eye, &frustum, zScale_);
  stats_.selectMs = t.nsecsElapsed() / 1e6;
  stats_.chunks = int(chunks.size());
  stats_.finestChunks = int(std::count_if(chunks.begin(), chunks.end(), [](const auto& c) { return c.level == 0; }));

  instanceData_.clear();
  instanceData_.reserve(chunks.size() * 4);
  for (const strata3d::ChunkInstance& c : chunks)
    instanceData_.insert(instanceData_.end(), {float(c.col0), float(c.row0), float(c.sizePx), float(c.level)});

  glUseProgram(program_);
  glUniformMatrix4fv(glGetUniformLocation(program_, "uViewProj"), 1, GL_FALSE, vp.constData());
  glUniform2i(glGetUniformLocation(program_, "uGrid"), g.width, g.height);
  glUniform1f(glGetUniformLocation(program_, "uPixel"), float(g.pixelSize));
  glUniform1f(glGetUniformLocation(program_, "uZScale"), zScale_);
  glUniform1f(glGetUniformLocation(program_, "uSkirt"), kSkirtFraction);
  glUniform1f(glGetUniformLocation(program_, "uZMin"), g.zMin);
  glUniform1f(glGetUniformLocation(program_, "uZMax"), g.zMax);
  glUniform3f(glGetUniformLocation(program_, "uSunDir"), -0.55f, 0.55f, 0.62f);  // 북서쪽 위(등고선 판독 관례)
  glUniform1i(glGetUniformLocation(program_, "uMode"), int(mode_));
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, heightTex_);
  glUniform1i(glGetUniformLocation(program_, "uHeight"), 0);

  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
  glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(instanceData_.size() * sizeof(float)), instanceData_.data(), GL_STREAM_DRAW);
  glDrawElementsInstanced(GL_TRIANGLES, templateIndexCount_, GL_UNSIGNED_INT, nullptr, GLsizei(chunks.size()));
  glBindVertexArray(0);
  ++stats_.frames;
  emit frameRendered();
}

void KaGlTerrainViewport::mousePressEvent(QMouseEvent* e) { lastMouse_ = e->pos(); }

void KaGlTerrainViewport::mouseMoveEvent(QMouseEvent* e) {
  const QPoint d = e->pos() - lastMouse_;
  lastMouse_ = e->pos();
  if (e->buttons() & Qt::LeftButton) {
    yawDeg_ += d.x() * 0.4f;
    pitchDeg_ = std::clamp(pitchDeg_ + d.y() * 0.4f, 5.0f, 89.0f);
  } else if (e->buttons() & (Qt::RightButton | Qt::MiddleButton)) {
    const float yaw = qDegreesToRadians(yawDeg_);
    const float k = distance_ * 0.0015f;
    target_ += QVector3D(std::cos(yaw), std::sin(yaw), 0) * (-d.x() * k) + QVector3D(-std::sin(yaw), std::cos(yaw), 0) * (d.y() * k);
  } else {
    return;
  }
  update();
}

void KaGlTerrainViewport::wheelEvent(QWheelEvent* e) {
  distance_ = std::clamp(distance_ * std::pow(0.9f, e->angleDelta().y() / 120.0f), 30.0f, 400000.0f);
  update();
}
