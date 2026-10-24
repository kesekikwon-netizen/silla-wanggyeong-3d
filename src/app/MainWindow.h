#pragma once

#include <QFutureWatcher>
#include <QImage>
#include <QMainWindow>
#include <memory>

#include "Archaeology3dEvaluator.h"
#include "GeoFeature.h"
#include "SceneMesh.h"
#include "TerrainLodTree.h"

class KaGlTerrainViewport;
class QLabel;
class QTextBrowser;

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
    bool loadSatellite = true;
    int satelliteTexels = 4096;
  };
  struct SatelliteLoad {
    QImage image;
    QImage detail;           // 중심부 고해상도 창(전체 영상이 흐릿한 근접 시점용)
    double detailHalfM = 0;
    QString source;
    QString error;
    double seconds = 0;
  };
  struct LoadResult {
    std::shared_ptr<const strata3d::TerrainLodTree> tree;
    std::shared_ptr<const strata3d::SceneMesh> mesh;
    std::shared_ptr<const std::vector<strata3d::GeoFeature>> features;
    std::shared_ptr<const strata3d::Archaeology3dEvaluator> evaluator;
    QStringList warnings;  // 건물·유적을 못 받은 사유 등(지형은 계속 보인다)
    QString error;
    double seconds = 0;
  };

  explicit MainWindow(const Options& options, QWidget* parent = nullptr);
  KaGlTerrainViewport* viewport() const { return viewport_; }
  const LoadResult& lastLoad() const { return lastLoad_; }
  const strata3d::Evaluation* lastEvaluation() const { return hasEvaluation_ ? &lastEvaluation_ : nullptr; }

 public slots:
  void loadScene();
  // 작업 좌표계(EPSG:5187) 점을 500/1000 m 로 평가한다. 계산은 워커에서, 결과는 패널과 3D 겹침으로.
  void evaluateAt(double x, double y);
  // 3072×3072 입체지형 도면(PNG, pdfPath 가 비어 있지 않으면 PDF 도)을 저장한다.
  bool exportSheet(const QString& pngPath, const QString& pdfPath = QString());

 signals:
  void loadFinished(bool ok, const QString& message);
  void satelliteFinished(bool ok, const QString& message);
  void evaluationFinished(bool ok, const QString& message);

 private:
  void updateStatus();
  void onPick(const QVector3D& origin, const QVector3D& direction);
  void showEvaluation();

  Options options_;
  KaGlTerrainViewport* viewport_ = nullptr;
  QLabel* status_ = nullptr;
  QTextBrowser* report_ = nullptr;
  QFutureWatcher<LoadResult> watcher_;
  QFutureWatcher<SatelliteLoad> satWatcher_;
  QFutureWatcher<strata3d::Evaluation> evalWatcher_;
  LoadResult lastLoad_;
  strata3d::Evaluation lastEvaluation_;
  bool hasEvaluation_ = false;
  QString summary_;
  QString imageryName_;
  int mode_ = 0;
};
