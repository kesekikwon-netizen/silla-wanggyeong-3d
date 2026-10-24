#pragma once

#include <QFutureWatcher>
#include <QMainWindow>
#include <memory>

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
  };
  struct LoadResult {
    std::shared_ptr<const strata3d::TerrainLodTree> tree;
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
};
