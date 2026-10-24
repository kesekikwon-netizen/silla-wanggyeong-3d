#pragma once

#include <QFutureWatcher>
#include <QImage>
#include <QMainWindow>
#include <memory>
#include <optional>

#include "Archaeology3dEvaluator.h"
#include "BangriGridEngine.h"
#include "DbpiaBangriLedger.h"
#include "GeoFeature.h"
#include "SceneMesh.h"
#include "SheetComposer.h"
#include "StrataSurvey.h"
#include "TerrainLodTree.h"

class BangriDockPanel;
class KaGlTerrainViewport;
class ProfilePlot;
class QLabel;
class QPushButton;
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
    QString workCrs = QStringLiteral("EPSG:5187");  // 조사를 열면 조사 파일의 좌표계(5186/5187)를 따른다
    QString bangriPresetId;  // 비어 있지 않으면 시작 시 해당 방리 프리셋 좌표·제원을 적용한다
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
  const strata3d::StrataSurvey* survey() const { return survey_ ? &*survey_ : nullptr; }
  bool sceneComplete() const { return sceneComplete_; }
  ProfilePlot* profilePlot() const { return plot_; }
  // 그려 둔 시굴 트렌치의 단면(점 두 개를 찍기 전에는 비어 있다)
  const std::vector<strata3d::ProfilePoint>& trenchProfile() const { return trenchProfile_; }
  const QString& imageryName() const { return imageryName_; }
  BangriDockPanel* bangriPanel() const { return bangriPanel_; }
  const BangriSheetData& bangriData() const { return bangriData_; }

 public slots:
  void loadScene();
  // 작업 좌표계(EPSG:5187) 점을 500/1000 m 로 평가한다. 계산은 워커에서, 결과는 패널과 3D 겹침으로.
  void evaluateAt(double x, double y);
  // Strata 조사 프로젝트(.gpkg)를 읽어(읽기 전용) 그 자리로 장면을 다시 불러오고 도메인 레이어를 3D 로 겹친다.
  void openSurvey(const QString& gpkgPath);
  // 열린 조사의 조사구역(없으면 첫 트렌치·유구)을 평가한다.
  void evaluateSurvey();
  // 3072×3072 입체지형 도면(PNG, pdfPath 가 비어 있지 않으면 PDF 도)을 저장한다.
  bool exportSheet(const QString& pngPath, const QString& pdfPath = QString());
  // 3400×2400 5패널 신라왕경·9주5소경 방리구획 정밀 고고학 도면(PNG + 선택적 A3 PDF)을 저장한다.
  bool exportBangriSheet(const QString& pngPath, const QString& pdfPath = QString());
  // 신라왕경·9주5소경 프리셋을 선택하고 필요하면 해당 지역 DEM 을 불러온다.
  void selectBangriPreset(const QString& presetId, bool allowSceneReload = true);
  // 현재 DEM·지물·파라메트릭 방리 제원으로 3D 고지형·봉분·방리 격자·화석화·직교 트렌치를 다시 계산한다.
  void recomputeBangriState();
  // 발굴 기준점(Anchor)들에 대해 원점·방위각 RMSE 최소제곱 자동 정합을 수행한다.
  void autoFitBangriAnchors();
  // 시굴 트렌치: 지형에서 두 점(작업 좌표계)을 이어 단면과 3D 띠를 만든다. 화면에서 찍는 것과 같은 길이다.
  void setTrench(double ax, double ay, double bx, double by);
  void clearTrench();
  void setTrenchMode(bool on);
  // 사용자 3D 맵 선 긋기 & 3D 격자 만들기 제어
  void setUserDrawMode(int drawMode);
  void addUserLine(double ax, double ay, double bx, double by);
  void addUserGrid(double centerX, double centerY, double cellW, double cellH, int cols, int rows, double azimuthDeg = 0.0);
  void addUserGridAtCameraCenter();
  void undoLastUserDrawing();
  void clearUserDrawings();

 signals:
  void loadFinished(bool ok, const QString& message);
  void satelliteFinished(bool ok, const QString& message);
  void evaluationFinished(bool ok, const QString& message);
  void trenchChanged(const QString& summary);
  void surveyOpened(bool ok, const QString& message);
  // 지형·건물·유적과 (켜져 있으면) 위성영상까지 모두 올라왔다.
  void sceneCompleted(bool ok, const QString& message);

 private:
  void updateStatus();
  void onPick(const QVector3D& origin, const QVector3D& direction);
  void showEvaluation();
  void onTrenchPoint(const QVector3D& origin, const QVector3D& direction);
  void onMapDrawPoint(int drawMode, const QVector3D& origin, const QVector3D& direction);
  void onMapHoverRay(int drawMode, const QVector3D& origin, const QVector3D& direction);
  void rebuildUserDrawOverlay();
  void refreshOverlay();  // 조사 + 평가 + 트렌치 + 방리구획 + 사용자 3D 선/격자 겹침을 한 메시로 합쳐 올린다
  void rebuildSceneMeshForTerrainMode();
  void keyPressEvent(QKeyEvent* e) override;

  Options options_;
  ProfilePlot* plot_ = nullptr;
  QLabel* trenchInfo_ = nullptr;
  QPushButton* trenchBtn_ = nullptr;
  QPushButton* tbLineDrawBtn_ = nullptr;
  QPushButton* tbGridDrawBtn_ = nullptr;
  std::optional<strata3d::Pt> trenchA_;  // 첫 점만 찍은 상태
  std::vector<strata3d::ProfilePoint> trenchProfile_;
  QString trenchSummary_;
  std::optional<strata3d::Pt> userDrawStartPt_;
  std::optional<strata3d::UserDrawnLine3D> previewLine_;
  std::optional<strata3d::UserDrawnGrid3D> previewGrid_;
  std::vector<strata3d::UserDrawnLine3D> userLines_;
  std::vector<strata3d::UserDrawnGrid3D> userGrids_;
  std::vector<bool> userDrawHistoryIsGrid_;
  std::shared_ptr<const strata3d::SceneMesh> evalOverlay_, trenchOverlay_, bangriOverlay_, userDrawOverlay_;
  KaGlTerrainViewport* viewport_ = nullptr;
  QLabel* status_ = nullptr;
  QTextBrowser* report_ = nullptr;
  BangriDockPanel* bangriPanel_ = nullptr;
  strata3d::DbpiaBangriLedger dbpiaLedger_;
  BangriSheetData bangriData_;
  float zScale_ = 1.0f;
  QFutureWatcher<LoadResult> watcher_;
  QFutureWatcher<SatelliteLoad> satWatcher_;
  QFutureWatcher<strata3d::Evaluation> evalWatcher_;
  LoadResult lastLoad_;
  strata3d::Evaluation lastEvaluation_;
  bool hasEvaluation_ = false;
  QString summary_;
  QString imageryName_;
  int mode_ = 0;
  std::optional<strata3d::StrataSurvey> survey_;
  std::shared_ptr<const strata3d::SceneMesh> surveyOverlay_;
  bool sceneComplete_ = false;
  bool surveyPending_ = false;
};
