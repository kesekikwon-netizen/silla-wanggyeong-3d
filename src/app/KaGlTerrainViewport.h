#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QMatrix4x4>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLWidget>
#include <QPoint>
#include <QString>
#include <memory>
#include <vector>

#include "DrapeRaster.h"
#include "HdrPipeline.h"
#include "SceneMesh.h"
#include "TerrainLodTree.h"

// OpenGL 3.3 Core 지형 뷰포트. 높이는 R32F 텍스처, 조각은 하나의 33×33 틀을 인스턴싱한다.
// 조각 선택(절두체 컬링·거리 구간)은 엔진(TerrainLodTree)이 CPU 에서 하고 여기서는 그리기만 한다.
class KaGlTerrainViewport : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
  Q_OBJECT
 public:
  enum class Mode { Clay = 0, Elevation = 1, Slope = 2, Satellite = 3 };
  enum class DrawMode { None = 0, Trench = 1, UserLine = 2, UserGrid = 3, Sketch = 4 };  // Sketch: 꼭짓점 여러 개(조사 레이어 그리기)

  struct FrameStats {
    int chunks = 0;
    int finestChunks = 0;
    double selectMs = 0;
    qint64 frames = 0;
  };

  struct CalloutLabel3D {
    double worldX = 0.0;
    double worldY = 0.0;
    float heightAboveGroundM = 12.0f;
    QString category;  // 예: "대사찰", "왕궁", "왕릉", "중심대로"
    QString title;     // 예: "황룡사 9층목탑 (높이 80m)"
    QString subtitle;  // 예: "신라 수도 중심 랜드마크"
    QColor badgeColor = QColor(200, 120, 20);
    bool important = true;
    QString featureId;  // 고를 수 있는 지물이면 그 id(GeoFeature::id). 이름표를 누르면 calloutClicked 로 나간다
  };

  explicit KaGlTerrainViewport(QWidget* parent = nullptr);
  ~KaGlTerrainViewport() override;

  void setTerrain(std::shared_ptr<const strata3d::TerrainLodTree> tree);
  void setScene(std::shared_ptr<const strata3d::SceneMesh> scene);
  void setSceneVisible(bool visible);
  void setAmbientOcclusion(bool on) { aoEnabled_ = on; update(); }
  bool ambientOcclusion() const { return aoEnabled_; }
  // 평가 결과 띠·기둥(버퍼 고리 등). nullptr 이면 지운다.
  void setOverlay(std::shared_ptr<const strata3d::SceneMesh> overlay);
  // 도식(D) 모델. 면을 칠하지 않고 회색 선화로만 그린다(실측 지물과 섞여 보이지 않게). nullptr 이면 지운다.
  void setSchematic(std::shared_ptr<const strata3d::SceneMesh> schematic);
  // 3D 화면 위에 띄울 직관적 안내 이름표(유적·대로·왕릉·방리 블록 설명)
  void setCalloutLabels(std::vector<CalloutLabel3D> labels);
  void setShowCalloutLabels(bool show);
  bool showCalloutLabels() const { return showCalloutLabels_; }
  void setShowTopBanner(bool show) { showTopBanner_ = show; update(); }
  bool showTopBanner() const { return showTopBanner_; }
  void setHudFontScale(float s) { hudFontScale_ = std::clamp(s, 0.45f, 2.40f); update(); }
  float hudFontScale() const { return hudFontScale_; }
  void setHudBoxScale(float s) { hudBoxScale_ = std::clamp(s, 0.45f, 2.40f); update(); }
  float hudBoxScale() const { return hudBoxScale_; }
  // 특정 세계 좌표(EPSG:5187/5186 미터)로 카메라 초점과 각도·거리를 즉시 이동한다.
  void focusOnWorld(double worldX, double worldY, float yawDeg, float pitchDeg, float distanceM);
  // 현재 카메라가 바라보는 목표점의 세계 좌표(EPSG:5187/5186 미터)를 반환한다.
  bool currentTargetWorld(double* worldX, double* worldY) const;
  // 위젯 크기와 상관없이 지정한 크기로 한 장 그린다(FBO). 도면용 고해상도 캡처.
  QImage renderToImage(int width, int height);
  float yawDegrees() const { return yawDeg_; }
  float pitchDegrees() const { return pitchDeg_; }
  float distanceM() const { return distance_; }
  float zScale() const { return zScale_; }
  float visibleWidthAtTargetM(float aspect) const;
  // DEM 격자와 같은 범위의 위성 실사(행 0 = 북). 올리면 위성 보기로 바뀐다.
  // detail 은 같은 중심의 고해상도 창(한 변 절반 detailHalfM 미터). 비어 있으면 쓰지 않는다.
  void setSatellite(const QImage& rgb, const QImage& detail = QImage(), double detailHalfM = 0);
  // 지형에 입히는 구역·유산 면(DrapeRaster). detail 은 같은 가운데의 고해상도 창. 빈 그림이면 지운다.
  void setDrape(const QImage& image, const QImage& detail = QImage(), double detailHalfM = 0);
  // 왼쪽 아래 구역 범례(지형에 입힌 분류와 색). 비우면 범례를 그리지 않는다.
  // 이름표가 덮지 않을 자리(작업 좌표계 고리, 예: 조사구역). 화면에 그린 자리의 상자를 비워 둔다.
  void setKeepClear(std::vector<std::vector<strata3d::Pt>> rings) { keepClear_ = std::move(rings); update(); }
  void setZoneLegend(std::vector<strata3d::DrapeLegendRow> rows) { zoneLegend_ = std::move(rows); update(); }
  bool hasSatellite() const { return satTex_ != 0 || !pendingSat_.isNull(); }
  void setMode(Mode mode);
  void setZScale(float zScale);
  // 지형을 올린 직후의 카메라(도, 도, 미터). 재현 가능한 화면을 얻으려고 쓴다.
  void setCameraPreset(float yawDeg, float pitchDeg, float distanceM);
  // 켜면 끌지 않은 왼쪽 클릭이 trenchPointRequested 로 나간다(카메라 돌리기는 그대로).
  void setTrenchMode(bool on);
  bool trenchMode() const { return drawMode_ == DrawMode::Trench; }
  void setDrawMode(DrawMode mode);
  DrawMode drawMode() const { return drawMode_; }
  const FrameStats& stats() const { return stats_; }
  bool hasTerrain() const { return bool(tree_); }
  QString glInfo() const { return glInfo_; }

 signals:
  // 더블클릭한 화면 점에서 나가는 시선(지역 미터 좌표, 방향은 정규화)
  void pickRequested(const QVector3D& origin, const QVector3D& direction);
  // 그리기 모드가 아닐 때 끌지 않고 한 번 누른 점의 시선(고르기). pixelM = 그 거리에서 화면 1 px 의 지상 길이(m)
  void selectRequested(const QVector3D& origin, const QVector3D& direction, double pixelM);
  // 이름표를 눌렀다(CalloutLabel3D::featureId)
  void calloutClicked(const QString& featureId);
  // 트렌치 그리기 모드에서 끌지 않고 한 번 누른 화면 점의 시선
  void trenchPointRequested(const QVector3D& origin, const QVector3D& direction);
  // 사용자 3D 선 긋기 / 격자 만들기 모드에서 끌지 않고 누른 화면 점의 시선
  void mapDrawPointRequested(int drawMode, const QVector3D& origin, const QVector3D& direction);
  // 마우스를 움직일 때(누르지 않은 채)의 시선. 그리기 모드면 미리보기, 아니어도 상태줄의 좌표·표고에 쓴다
  void mapHoverRay(int drawMode, const QVector3D& origin, const QVector3D& direction);
  // 그리기 중 우클릭 취소 요청
  void mapDrawCancelRequested();
  // Sketch 모드에서 두 번 눌렀다(도형 마치기)
  void sketchFinishRequested();
  void frameRendered();
  void terrainUploaded();
  void hudScaleChanged(float fontScale, float boxScale);

 protected:
  void initializeGL() override;
  void paintGL() override;
  void resizeGL(int w, int h) override;
  void mousePressEvent(QMouseEvent* e) override;
  void mouseMoveEvent(QMouseEvent* e) override;
  void mouseReleaseEvent(QMouseEvent* e) override;
  void wheelEvent(QWheelEvent* e) override;
  void keyPressEvent(QKeyEvent* e) override;  // Ctrl+= / Ctrl+- / Ctrl+0: 이름표 크기
  void mouseDoubleClickEvent(QMouseEvent* e) override;

 private:
  struct GlMesh {
    unsigned vao = 0, vbo = 0, ebo = 0;
    int count = 0;
  };
  void uploadTerrain();
  void uploadMesh(GlMesh* gm, const std::shared_ptr<const strata3d::SceneMesh>& mesh);
  void drawFrame(int w, int h, unsigned targetFbo);
  void paintHudAnnotations(QPainter* p, int w, int h) const;
  void resetCamera();
  bool pickRay(const QPointF& pos, QVector3D* origin, QVector3D* direction) const;
  QVector3D eyePosition() const;
  unsigned buildProgram(const char* vertexSrc, const char* fragmentSrc);
  void buildTemplate();

  std::shared_ptr<const strata3d::TerrainLodTree> tree_;
  std::shared_ptr<const strata3d::TerrainLodTree> pendingUpload_;
  Mode mode_ = Mode::Clay;
  float zScale_ = 1.0f;
  QString glInfo_;
  FrameStats stats_;

  // 카메라: 목표점 주위를 도는 방식
  QVector3D target_;
  float targetBaseZ_ = 45.0f;
  float yawDeg_ = 25.0f;
  float pitchDeg_ = 38.0f;
  float distance_ = 1000.0f;
  QPoint lastMouse_;
  QPoint pressPos_;
  bool dragged_ = false;
  DrawMode drawMode_ = DrawMode::None;
  bool hasPreset_ = false;
  float presetYaw_ = 0, presetPitch_ = 0, presetDistance_ = 0;
  bool hasPendingFocus_ = false;
  double pendingFocusWorldX_ = 0.0;
  double pendingFocusWorldY_ = 0.0;

  // GL 자원
  unsigned program_ = 0;
  unsigned vao_ = 0;
  unsigned templateVbo_ = 0;
  unsigned templateEbo_ = 0;
  unsigned instanceVbo_ = 0;
  unsigned heightTex_ = 0;
  QImage pendingSat_;
  QImage pendingDetail_;
  double detailHalfM_ = 0;
  unsigned satTex_ = 0;
  unsigned detailTex_ = 0;
  QImage pendingDrape_, pendingDrapeDetail_;
  bool drapeChanged_ = false;
  double drapeHalfM_ = 0;
  unsigned drapeTex_ = 0, drapeDetailTex_ = 0;
  std::vector<strata3d::DrapeLegendRow> zoneLegend_;
  std::vector<std::vector<strata3d::Pt>> keepClear_;
  // 건물·유적
  std::shared_ptr<const strata3d::SceneMesh> pendingScene_;
  bool sceneChanged_ = false;
  bool sceneVisible_ = true;
  unsigned sceneProgram_ = 0;
  GlMesh sceneMesh_;
  GlMesh overlayMesh_;
  std::shared_ptr<const strata3d::SceneMesh> pendingOverlay_;
  bool overlayChanged_ = false;
  GlMesh schematicMesh_;
  std::shared_ptr<const strata3d::SceneMesh> pendingSchematic_;
  bool schematicChanged_ = false;
  std::vector<CalloutLabel3D> calloutLabels_;
  mutable std::vector<std::pair<QRect, QString>> placedLabelBoxes_;  // 마지막으로 그린 이름표 자리와 지물 id(누르기 판정)
  bool showCalloutLabels_ = true;
  bool showTopBanner_ = true;
  float hudFontScale_ = 0.85f;
  float hudBoxScale_ = 0.85f;
  HdrPipeline pipeline_;
  HdrPipeline::Params params_;
  QVector3D sunColor_{1, 1, 1};
  QVector3D ambient_{0.2f, 0.2f, 0.2f};
  float sunStrength_ = 1.0f;
  bool aoEnabled_ = true;
  bool sky_ok_ = false;
  QMatrix4x4 lastViewProj_;
  QVector3D lastEye_;
  int templateIndexCount_ = 0;
  std::vector<float> instanceData_;
};
