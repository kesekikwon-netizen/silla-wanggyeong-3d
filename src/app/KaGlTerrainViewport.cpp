#include "KaGlTerrainViewport.h"

#include <QMatrix4x4>
#include <QMouseEvent>
#include <QOpenGLFramebufferObject>
#include <QVector4D>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

#include "AgxView.h"
#include "Frustum.h"
#include "NishitaSky.h"
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
  pipeline_.release();
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

unsigned KaGlTerrainViewport::buildProgram(const char* vsSrc, const char* fsSrc) {
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
  if (!compile(GL_VERTEX_SHADER, vsSrc, &vs) || !compile(GL_FRAGMENT_SHADER, fsSrc, &fs)) return 0;
  const unsigned prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);
  glDeleteShader(vs);
  glDeleteShader(fs);
  int ok = 0;
  glGetProgramiv(prog, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[2048];
    glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
    qCritical("셰이더 링크 실패: %s", log);
    glDeleteProgram(prog);
    return 0;
  }
  return prog;
}

void KaGlTerrainViewport::setScene(std::shared_ptr<const strata3d::SceneMesh> scene) {
  pendingScene_ = std::move(scene);
  sceneChanged_ = true;
  update();
}

void KaGlTerrainViewport::setSatellite(const QImage& rgb, const QImage& detail, double detailHalfM) {
  pendingSat_ = rgb.convertToFormat(QImage::Format_RGB888);
  pendingDetail_ = detail.isNull() ? QImage() : detail.convertToFormat(QImage::Format_RGB888);
  detailHalfM_ = detail.isNull() ? 0.0 : detailHalfM;
  mode_ = Mode::Satellite;
  update();
}

void KaGlTerrainViewport::setSceneVisible(bool visible) {
  sceneVisible_ = visible;
  update();
}

void KaGlTerrainViewport::setOverlay(std::shared_ptr<const strata3d::SceneMesh> overlay) {
  pendingOverlay_ = std::move(overlay);
  overlayChanged_ = true;
  update();
}

void KaGlTerrainViewport::uploadMesh(GlMesh* gm, const std::shared_ptr<const strata3d::SceneMesh>& mesh) {
  if (!gm->vao) {
    glGenVertexArrays(1, &gm->vao);
    glGenBuffers(1, &gm->vbo);
    glGenBuffers(1, &gm->ebo);
  }
  glBindVertexArray(gm->vao);
  glBindBuffer(GL_ARRAY_BUFFER, gm->vbo);
  const size_t vb = mesh ? mesh->vertices.size() * sizeof(float) : 0;
  glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(vb), mesh ? mesh->vertices.data() : nullptr, GL_STATIC_DRAW);
  const GLsizei stride = strata3d::SceneMesh::kFloatsPerVertex * sizeof(float);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(6 * sizeof(float)));
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gm->ebo);
  const size_t ib = mesh ? mesh->indices.size() * sizeof(uint32_t) : 0;
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(ib), mesh ? mesh->indices.data() : nullptr, GL_STATIC_DRAW);
  gm->count = mesh ? int(mesh->indices.size()) : 0;
  glBindVertexArray(0);
}

float KaGlTerrainViewport::visibleWidthAtTargetM(float aspect) const {
  // 수직 시야 45° 투시에서 목표점 거리에서 보이는 지상 폭
  return 2.0f * distance_ * std::tan(qDegreesToRadians(45.0f) * 0.5f) * aspect;
}

// 화면 점 → 시선(역투영). 마지막으로 그린 투영·시점 행렬을 쓴다.
bool KaGlTerrainViewport::pickRay(const QPointF& pos, QVector3D* origin, QVector3D* direction) const {
  const float nx = 2.0f * float(pos.x()) / float(width()) - 1.0f;
  const float ny = 1.0f - 2.0f * float(pos.y()) / float(height());
  bool ok = false;
  const QMatrix4x4 inv = lastViewProj_.inverted(&ok);
  if (!ok) return false;
  const QVector4D p0 = inv * QVector4D(nx, ny, -1.0f, 1.0f), p1 = inv * QVector4D(nx, ny, 1.0f, 1.0f);
  *origin = lastEye_;
  *direction = (p1.toVector3DAffine() - p0.toVector3DAffine()).normalized();
  return true;
}

void KaGlTerrainViewport::mouseDoubleClickEvent(QMouseEvent* e) {
  if (!tree_ || e->button() != Qt::LeftButton || trenchMode_) return;  // 트렌치 모드에서는 더블클릭 평가를 쓰지 않는다
  QVector3D o, d;
  if (pickRay(e->position(), &o, &d)) emit pickRequested(o, d);
}

void KaGlTerrainViewport::setTrenchMode(bool on) {
  trenchMode_ = on;
  setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
}

void KaGlTerrainViewport::mouseReleaseEvent(QMouseEvent* e) {
  if (!trenchMode_ || !tree_ || e->button() != Qt::LeftButton || dragged_) return;
  QVector3D o, d;
  if (pickRay(e->position(), &o, &d)) emit trenchPointRequested(o, d);
}

QImage KaGlTerrainViewport::renderToImage(int w, int h) {
  makeCurrent();
  QOpenGLFramebufferObjectFormat fmt;
  fmt.setAttachment(QOpenGLFramebufferObject::NoAttachment);  // 장면은 HDR 파이프라인의 자체 FBO 에 그리고 마지막 합성만 여기에 쓴다
  fmt.setSamples(0);
  QOpenGLFramebufferObject fbo(w, h, fmt);
  fbo.bind();
  drawFrame(w, h, fbo.handle());
  fbo.release();
  const QImage img = fbo.toImage(false);
  doneCurrent();
  update();
  return img;
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
  program_ = buildProgram(TerrainShaders::kVertex, TerrainShaders::kFragment);
  sceneProgram_ = buildProgram(TerrainShaders::kSceneVertex, TerrainShaders::kSceneFragment);
  if (!program_ || !sceneProgram_) return;
  buildTemplate();
  glEnable(GL_DEPTH_TEST);
  strata3d::AgxView agx;
  QString err;
  if (!agx.loadCube(strata3d::AgxView::defaultCubePath(), &err) || !pipeline_.init(this, agx.lut(), agx.size(), &err)) {
    glInfo_ += QStringLiteral(" | 후처리 초기화 실패: ") + err;
    qCritical("%s", qPrintable(err));
    return;
  }
  // 태양 직사광(투과율 반영)과 하늘 환경광(위쪽 반구 평균) — 하늘과 같은 NishitaSky 식
  const strata3d::NishitaSky sky(params_.eyeAltitudeM, params_.sunIrradiance);
  const QVector3D sd = params_.sunDir.normalized();
  sunColor_ = sky.sunTransmittance(sd) * sunStrength_;
  QVector3D sum(0, 0, 0);
  float wsum = 0;
  for (int i = 0; i < 4; ++i) {
    for (int el : {20, 50, 80}) {
      const float az = float(i) * 1.5707963f, e = float(el) * 0.0174533f;
      const QVector3D d(std::cos(e) * std::cos(az), std::cos(e) * std::sin(az), std::sin(e));
      sum += sky.radiance(d, sd) * std::sin(e);
      wsum += std::sin(e);
    }
  }
  ambient_ = sum / wsum * 3.14159265f * 0.5f;  // 위쪽 반구에서 오는 조도 / π 근사
  if (!sky_ok_) sky_ok_ = true;
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
  // 화면 배율(예: 150%)이 있으면 실제 프레임버퍼는 위젯 크기보다 크다 — 장치 픽셀로 그린다
  const qreal dpr = devicePixelRatioF();
  drawFrame(int(std::lround(width() * dpr)), int(std::lround(height() * dpr)), defaultFramebufferObject());
  ++stats_.frames;
  emit frameRendered();
}

void KaGlTerrainViewport::drawFrame(int w, int h, unsigned targetFbo) {
  if (pendingUpload_) uploadTerrain();
  if (sceneChanged_) {
    sceneChanged_ = false;
    uploadMesh(&sceneMesh_, pendingScene_);
    pendingScene_.reset();
  }
  if (overlayChanged_) {
    overlayChanged_ = false;
    uploadMesh(&overlayMesh_, pendingOverlay_);
    pendingOverlay_.reset();
  }
  if (!pendingSat_.isNull()) {
    if (satTex_) glDeleteTextures(1, &satTex_);
    glGenTextures(1, &satTex_);
    glBindTexture(GL_TEXTURE_2D, satTex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, pendingSat_.width(), pendingSat_.height(), 0, GL_RGB, GL_UNSIGNED_BYTE, pendingSat_.constBits());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, 0x84FE /*GL_TEXTURE_MAX_ANISOTROPY_EXT*/, 8.0f);
    pendingSat_ = QImage();
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  }
  if (!pendingDetail_.isNull()) {
    if (detailTex_) glDeleteTextures(1, &detailTex_);
    glGenTextures(1, &detailTex_);
    glBindTexture(GL_TEXTURE_2D, detailTex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, pendingDetail_.width(), pendingDetail_.height(), 0, GL_RGB, GL_UNSIGNED_BYTE, pendingDetail_.constBits());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, 0x84FE, 8.0f);
    pendingDetail_ = QImage();
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  }
  if (!pipeline_.ready()) return;
  if (!tree_ || !heightTex_) {
    glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
    glViewport(0, 0, w, h);
    glClearColor(0.74f, 0.80f, 0.88f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    return;
  }
  pipeline_.beginScene(w, h);

  const strata3d::DemGrid& g = tree_->grid();
  const float extent = float(std::max(g.width, g.height) * g.pixelSize);
  const QVector3D eye = eyePosition();
  QMatrix4x4 view, proj;
  view.lookAt(eye, target_, QVector3D(0, 0, 1));
  const float aspect = float(w) / float(std::max(1, h));
  proj.perspective(45.0f, aspect, std::max(2.0f, distance_ * 0.01f), distance_ + extent * 3.0f);
  const QMatrix4x4 vp = proj * view;
  lastViewProj_ = vp;
  lastEye_ = eye;
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
  const QVector3D sd = params_.sunDir.normalized();
  const auto rot = view.toGenericMatrix<3, 3>();
  glUniform3f(glGetUniformLocation(program_, "uSunDir"), sd.x(), sd.y(), sd.z());
  glUniformMatrix3fv(glGetUniformLocation(program_, "uViewRot"), 1, GL_FALSE, rot.constData());
  glUniform3f(glGetUniformLocation(program_, "uSunColor"), sunColor_.x(), sunColor_.y(), sunColor_.z());
  glUniform3f(glGetUniformLocation(program_, "uAmbient"), ambient_.x(), ambient_.y(), ambient_.z());
  const Mode mode = (mode_ == Mode::Satellite && !satTex_) ? Mode::Elevation : mode_;
  glUniform1i(glGetUniformLocation(program_, "uMode"), int(mode));
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, detailTex_);
  glUniform1i(glGetUniformLocation(program_, "uSatDetail"), 2);
  const float span = float(g.width * g.pixelSize);
  const float half = detailTex_ ? float(detailHalfM_ / span) : 0.0f;  // uv 에서 절반 폭(창은 DEM 한가운데)
  glUniform4f(glGetUniformLocation(program_, "uDetailRect"), 0.5f - half, 0.5f - half, 0.5f + half, 0.5f + half);
  glUniform1i(glGetUniformLocation(program_, "uHasDetail"), detailTex_ ? 1 : 0);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, satTex_);
  glUniform1i(glGetUniformLocation(program_, "uSat"), 1);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, heightTex_);
  glUniform1i(glGetUniformLocation(program_, "uHeight"), 0);

  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
  glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(instanceData_.size() * sizeof(float)), instanceData_.data(), GL_STREAM_DRAW);
  glDrawElementsInstanced(GL_TRIANGLES, templateIndexCount_, GL_UNSIGNED_INT, nullptr, GLsizei(chunks.size()));
  glBindVertexArray(0);

  glUseProgram(sceneProgram_);
  glUniformMatrix4fv(glGetUniformLocation(sceneProgram_, "uViewProj"), 1, GL_FALSE, vp.constData());
  glUniform3f(glGetUniformLocation(sceneProgram_, "uSunDir"), sd.x(), sd.y(), sd.z());
  glUniformMatrix3fv(glGetUniformLocation(sceneProgram_, "uViewRot"), 1, GL_FALSE, rot.constData());
  glUniform3f(glGetUniformLocation(sceneProgram_, "uSunColor"), sunColor_.x(), sunColor_.y(), sunColor_.z());
  glUniform3f(glGetUniformLocation(sceneProgram_, "uAmbient"), ambient_.x(), ambient_.y(), ambient_.z());
  if (sceneVisible_ && sceneMesh_.count > 0) {
    glBindVertexArray(sceneMesh_.vao);
    glDrawElements(GL_TRIANGLES, sceneMesh_.count, GL_UNSIGNED_INT, nullptr);
  }
  if (overlayMesh_.count > 0) {  // 평가 겹침은 건물 켜고 끔과 상관없이 보인다
    glBindVertexArray(overlayMesh_.vao);
    glDrawElements(GL_TRIANGLES, overlayMesh_.count, GL_UNSIGNED_INT, nullptr);
  }
  glBindVertexArray(0);
  pipeline_.drawSky(vp, params_);
  params_.ambientOcclusion = aoEnabled_;
  pipeline_.finish(proj, params_, targetFbo, w, h);
}

void KaGlTerrainViewport::mousePressEvent(QMouseEvent* e) {
  lastMouse_ = pressPos_ = e->pos();
  dragged_ = false;
}

void KaGlTerrainViewport::mouseMoveEvent(QMouseEvent* e) {
  const QPoint d = e->pos() - lastMouse_;
  lastMouse_ = e->pos();
  if ((e->pos() - pressPos_).manhattanLength() > 5) dragged_ = true;
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
