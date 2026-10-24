#pragma once

#include <QFutureWatcher>
#include <QMainWindow>
#include <memory>

#include "GeoFeature.h"
#include "SceneMesh.h"
#include "TerrainLodTree.h"

class KaGlTerrainViewport;
class QLabel;

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  struct Options {
    QString demPath;  // 비어 있으면 Copernicus GLO-30 COG
    double lon = 129.2194;  // 첨성대 부근 (WGS84)
    double lat = 35.8347;
    double halfExtentM = 12000;
    double pixelSizeM = 30;
    double featureRadiusM = 4000;  // 건물·유적을 받아 오는 반경
    bool loadFeatures = true;
  };
  struct LoadResult {
    std::shared_ptr<const strata3d::TerrainLodTree> tree;
    std::shared_ptr<const strata3d::SceneMesh> mesh;
    std::shared_ptr<const std::vector<strata3d::GeoFeature>> features;
    QStringList warnings;  // 건물·유적을 못 받은 사유 등(지형은 계속 보인다)
    QString error;
    double seconds = 0;
  };

  explicit MainWindow(const Options& options, QWidget* parent = nullptr);
  KaGlTerrainViewport* viewport() const { return viewport_; }
  const LoadResult& lastLoad() const { return lastLoad_; }

 public slots:
  void loadScene();

 signals:
  void loadFinished(bool ok, const QString& message);

 private:
  void updateStatus();

  Options options_;
  KaGlTerrainViewport* viewport_ = nullptr;
  QLabel* status_ = nullptr;
  QFutureWatcher<LoadResult> watcher_;
  LoadResult lastLoad_;
  QString summary_;
};
