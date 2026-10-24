#pragma once

#include <QElapsedTimer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLWidget>
#include <QPoint>
#include <memory>
#include <vector>

#include "TerrainLodTree.h"

// OpenGL 3.3 Core 지형 뷰포트. 높이는 R32F 텍스처, 조각은 하나의 33×33 틀을 인스턴싱한다.
// 조각 선택(절두체 컬링·거리 구간)은 엔진(TerrainLodTree)이 CPU 에서 하고 여기서는 그리기만 한다.
class KaGlTerrainViewport : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
  Q_OBJECT
 public:
  enum class Mode { Clay = 0, Elevation = 1, Slope = 2 };

  struct FrameStats {
    int chunks = 0;
    int finestChunks = 0;
    double selectMs = 0;
    qint64 frames = 0;
  };

  explicit KaGlTerrainViewport(QWidget* parent = nullptr);
  ~KaGlTerrainViewport() override;

  void setTerrain(std::shared_ptr<const strata3d::TerrainLodTree> tree);
  void setMode(Mode mode);
  void setZScale(float zScale);
  const FrameStats& stats() const { return stats_; }
  bool hasTerrain() const { return bool(tree_); }
  QString glInfo() const { return glInfo_; }

 signals:
  void frameRendered();
  void terrainUploaded();

 protected:
  void initializeGL() override;
  void paintGL() override;
  void resizeGL(int w, int h) override;
  void mousePressEvent(QMouseEvent* e) override;
  void mouseMoveEvent(QMouseEvent* e) override;
  void wheelEvent(QWheelEvent* e) override;

 private:
  void uploadTerrain();
  void resetCamera();
  QVector3D eyePosition() const;
  bool buildProgram();
  void buildTemplate();

  std::shared_ptr<const strata3d::TerrainLodTree> tree_;
  std::shared_ptr<const strata3d::TerrainLodTree> pendingUpload_;
  Mode mode_ = Mode::Clay;
  float zScale_ = 1.0f;
  QString glInfo_;
  FrameStats stats_;

  // 카메라: 목표점 주위를 도는 방식
  QVector3D target_;
  float yawDeg_ = 25.0f;
  float pitchDeg_ = 38.0f;
  float distance_ = 1000.0f;
  QPoint lastMouse_;

  // GL 자원
  unsigned program_ = 0;
  unsigned vao_ = 0;
  unsigned templateVbo_ = 0;
  unsigned templateEbo_ = 0;
  unsigned instanceVbo_ = 0;
  unsigned heightTex_ = 0;
  int templateIndexCount_ = 0;
  std::vector<float> instanceData_;
};
