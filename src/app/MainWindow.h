#pragma once

#include <QFutureWatcher>
#include <QHash>
#include <QImage>
#include <QMainWindow>
#include <QPointer>
#include <QTimer>
#include <qgsfeatureid.h>
#include <memory>
#include <optional>

#include "Archaeology3dEvaluator.h"
#include "BangriTerrainValidator.h"
#include "DrapeRaster.h"
#include "GeoFeature.h"
#include "KhsSpcaProvider.h"
#include "LocalVectorProvider.h"
#include "RelationEngine.h"
#include "SceneMesh.h"
#include "StrataSurvey.h"
#include "TerrainLodTree.h"

class KaBeginnerRibbon;
class KaGlTerrainViewport;
class KaStatusBar;
class ProfilePlot;
class RelationPanel;
class StrataLayerDock;
class StrataReceive;
class KaFeatureCard;
class KaInspectorPanel;
class HeritageCard;
class KaAppBar;
class LocationSearch;
class QgsVectorLayer;
struct LocationHit;
class QAction;
class QDockWidget;
class QLabel;
class QMenu;
class QTextBrowser;

// Strata 3D 창. 위는 Strata 리본(KaBeginnerRibbon, 단추 표는 RibbonSpec), 가운데는 3D 화면 하나,
// 아래는 필요할 때 여는 분석 칸(구역·관계 표 / 단면 / 버퍼), 맨 아래는 Strata 상태줄(KaStatusBar).
// 화면에 올리는 것은 측정·기록된 자료뿐이다(추정 금지, 설계도 v3 4장).
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
    // 열린 조사 프로젝트(QgsProject)의 주변유적 레이어. heritageFromProject 면 파일 대신 이것을 읽는다
    bool heritageFromProject = false;
    std::vector<strata3d::LocalVectorSource> heritageSources;
    QString surveyGpkg;      // 연 Strata 조사. 있으면 그 조사에 받아 둔 주변유적 레이어만 읽는다(캐시 전체를 뒤지지 않는다)
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
  KaBeginnerRibbon* ribbon() const { return ribbon_; }
  // 리본 단추의 동작(RibbonSpec 의 id). 없으면 nullptr.
  QAction* ribbonAction(const QString& id) const { return actions_.value(id); }
  const LoadResult& lastLoad() const { return lastLoad_; }
  const strata3d::Evaluation* lastEvaluation() const { return hasEvaluation_ ? &lastEvaluation_ : nullptr; }
  const strata3d::StrataSurvey* survey() const { return survey_ ? &*survey_ : nullptr; }
  bool sceneComplete() const { return sceneComplete_; }
  ProfilePlot* profilePlot() const { return plot_; }
  // 그려 둔 단면 선의 지표 단면(점 두 개를 찍기 전에는 비어 있다)
  const std::vector<strata3d::ProfilePoint>& trenchProfile() const { return trenchProfile_; }
  const QString& imageryName() const { return imageryName_; }
  // 도식(D) 모델을 켰는지(기본 끔, 설계도 v3 10장)
  bool schematicModelsShown() const { return showSchematic_; }
  const QString& selectedFeatureId() const { return selectedId_; }
  StrataReceive* receiver() const { return receive_; }
  // 지형에 입힌 Strata 레이어 수(마지막으로 그린 것)
  int qgisDrapeLayerCount() const { return int(qgisDrapeLayers().size()); }
  bool qgisDrapeReady() const { return !qgisDrape_.isNull(); }
  KaFeatureCard* featureCard() const { return card_; }
  // 속성 칸에 기록이 열려 있는지(Strata 카드 또는 유적 카드)
  bool inspectorShowsRecord() const;

 public slots:
  void loadScene();
  // 작업 좌표계 점을 500/1000 m 로 평가한다. 계산은 워커에서, 결과는 「버퍼」 칸과 3D 겹침으로.
  void evaluateAt(double x, double y);
  // Strata 조사 프로젝트(.gpkg)를 읽어(읽기 전용) 그 자리로 장면을 다시 불러오고 조사 레이어를 3D 로 겹친다.
  void openSurvey(const QString& gpkgPath);
  // 열린 조사의 조사구역(없으면 첫 트렌치·유구)을 평가한다(리본 「버퍼」).
  void evaluateSurvey();
  // 열린 조사의 조사구역(없으면 첫 트렌치)과 주변 유산의 관계 표를 워커에서 계산해 아래 칸에 올린다.
  void computeSurveyRelations();
  // 입체 입지도(PNG, pdfPath 가 비어 있지 않으면 PDF 도)를 저장한다.
  bool exportSheet(const QString& pngPath, const QString& pdfPath = QString());
  // 단면 선: 지형에서 두 점(작업 좌표계)을 이어 지표 단면과 3D 띠를 만든다. 화면에서 찍는 것과 같은 길이다.
  void setTrench(double ax, double ay, double bx, double by);
  void clearTrench();
  void setTrenchMode(bool on);
  // 조사 파일을 새로 만들고(Strata SurveyProjectFactory) 3D 에서 연다. 빈 조사도 한 번 저장한다.
  bool createSurveyAt(const QString& dir, const QString& name, const QString& workCrs, QString* errorOut = nullptr);
  // 조사 레이어(survey_area · feature_poly · feature_line · section_line · artifact_point)에 도형 하나를 넣는다
  // (편집 버퍼, 저장은 saveSurveyWork). pts 는 작업 좌표계.
  bool addSketchFeature(const QString& layerKey, const std::vector<strata3d::Pt>& pts, QgsFeatureId* fidOut = nullptr, QString* errorOut = nullptr);
  // Strata SurveySession::persistWork 로 저장(도형·기록·프로젝트). 바꾼 것이 없으면 true.
  bool saveSurveyWork();
  bool hasUnsavedSurveyWork() const;
  const QString& lastSaveError() const { return lastSaveError_; }
  // 확인 창 없이(스모크) 저장·닫기
  void setQuietSave(bool on) { quietSave_ = on; }
  // 유산 이름(이름표와 같은 이름)으로 골라 속성 칸에 연다. 없으면 false.
  bool selectByName(const QString& name);
  // 도식(D) 모델 311개를 회색 선화로 켜고 끈다.
  void setSchematicModelsShown(bool on);

 signals:
  void loadFinished(bool ok, const QString& message);
  void satelliteFinished(bool ok, const QString& message);
  void evaluationFinished(bool ok, const QString& message);
  void trenchChanged(const QString& summary);
  void surveyOpened(bool ok, const QString& message);
  void relationsFinished(bool ok, const QString& message);
  // 지형·건물·유적과 (켜져 있으면) 위성영상까지 모두 올라왔다.
  void sceneCompleted(bool ok, const QString& message);

 private:
  enum class Tool { Select, Measure, Line, Grid, Section, Sketch };

  void buildRibbon();
  void buildDocks();
  void buildStatusBar();
  void buildLayerDock();
  void buildInspector();
  void buildPlaceSearch(class QToolBar* bar);
  // 주소·지번·지역 찾기 결과로 3D 카메라를 옮긴다. 지금 지형 밖이면 그 자리로 지형을 다시 불러온다.
  void goToLocation(const LocationHit& hit);
  // 3D 에서 고른 지물을 Strata 속성 칸에 보이고 3D 에서 강조한다(id 가 비면 고른 것 없음).
  void selectFeature(const QString& featureId);
  void showHeritageCard(const strata3d::GeoFeature& f, class QgsVectorLayer* layer, qint64 fid);
  void onSelectRay(const QVector3D& origin, const QVector3D& direction, double pixelM);
  // 레이어 칸에서 끈 레이어를 3D 에서도 뺀다(지물·조사 겹침·이름표).
  void applyLayerVisibility();
  std::vector<strata3d::GeoFeature> visibleFeatures() const;
  // 지형에 입힐 Strata 레이어(켠 것, 위가 앞). 위성·음영·DEM·배경 지도·조사·주변유적은 뺀다
  QList<class QgsMapLayer*> qgisDrapeLayers() const;
  // QGIS 로 그 레이어들을 DEM 범위(+ 가운데 고해상도 창)에 그려 지형에 입힌다(비동기)
  void rebuildQgisDrape();
  // QGIS 그림(아래) + 구역·유산 면(위)을 합쳐 뷰포트에 올린다
  void composeDrape();
  QMenu* menuFor(const QString& id);
  void setTool(Tool tool);
  void updateStatus();
  void onPick(const QVector3D& origin, const QVector3D& direction);
  void showEvaluation();
  void onTrenchPoint(const QVector3D& origin, const QVector3D& direction);
  void onDrawPoint(int drawMode, const QVector3D& origin, const QVector3D& direction);
  void onHoverRay(int drawMode, const QVector3D& origin, const QVector3D& direction);
  bool groundAt(const QVector3D& origin, const QVector3D& direction, strata3d::Pt* at, float* z = nullptr) const;
  void rebuildDrawOverlay();
  void rebuildLabels();
  void refreshOverlay();  // 조사 + 평가 + 단면 + 측거·격자 겹침을 한 메시로 합쳐 올린다
  void setZScale(float z);
  void setTerrainMode(int mode);
  void focusSurvey(float pitchDeg);
  bool saveViewState(const QString& path);
  bool loadViewState(const QString& path);
  QString viewStatePath() const;
  void openSurveyDialog();
  void saveImageDialog();
  void printSheet();
  void keyPressEvent(QKeyEvent* e) override;
  void closeEvent(QCloseEvent* e) override;
  // ---- 조사 레이어 그리기·저장(Strata 와 같은 레이어·저장 방식) ----
  void startSketch(const QString& layerKey);
  void finishSketch();
  void cancelSketch();
  void rebuildSketchOverlay();
  // 조사 프로젝트의 조사 레이어(저장 안 된 편집 포함)로 3D 조사 겹침을 다시 만든다
  void refreshSurveyFromProject();
  void refreshUnsavedChip();
  void afterSketchAdded(QgsVectorLayer* layer, QgsFeatureId fid, const QString& key);
  void newSurvey();
  // 받은 자료를 올리려 지형·지물을 다시 불러온다. 카메라는 지금 자리 그대로.
  void reloadKeepingView();

  Options options_;
  KaBeginnerRibbon* ribbon_ = nullptr;
  QHash<QString, QAction*> actions_;
  KaStatusBar* statusBar_ = nullptr;
  QLabel* elevLabel_ = nullptr;
  QLabel* zScaleLabel_ = nullptr;
  QLabel* sourceLabel_ = nullptr;
  KaAppBar* appBar_ = nullptr;
  StrataReceive* receive_ = nullptr;
  QTimer reloadTimer_;
  LocationSearch* locator_ = nullptr;
  QDockWidget* layerDock_ = nullptr;
  QDockWidget* inspectorDock_ = nullptr;
  KaInspectorPanel* inspector_ = nullptr;
  KaFeatureCard* card_ = nullptr;
  HeritageCard* heritageCard_ = nullptr;
  std::optional<strata3d::RelationTable> lastRelations_;
  QString selectedId_;
  std::shared_ptr<const strata3d::SceneMesh> selectOverlay_;
  StrataLayerDock* layers_ = nullptr;
  QDockWidget* relationDock_ = nullptr;
  QDockWidget* profileDock_ = nullptr;
  QDockWidget* bufferDock_ = nullptr;
  ProfilePlot* plot_ = nullptr;
  QLabel* trenchInfo_ = nullptr;
  QTextBrowser* report_ = nullptr;
  Tool tool_ = Tool::Select;
  std::optional<strata3d::Pt> trenchA_;  // 첫 점만 찍은 상태
  std::vector<strata3d::ProfilePoint> trenchProfile_;
  QString trenchSummary_;
  // 측거(선)·시굴격자: 지형 위 두 점
  std::optional<strata3d::Pt> drawStart_;
  std::optional<strata3d::UserDrawnLine3D> previewLine_;
  std::optional<strata3d::UserDrawnGrid3D> previewGrid_;
  std::vector<strata3d::UserDrawnLine3D> lines_;
  std::vector<strata3d::UserDrawnGrid3D> grids_;
  std::shared_ptr<const strata3d::SceneMesh> evalOverlay_, trenchOverlay_, drawOverlay_;
  KaGlTerrainViewport* viewport_ = nullptr;
  float zScale_ = 1.0f;
  bool showSchematic_ = false;
  QFutureWatcher<LoadResult> watcher_;
  QFutureWatcher<SatelliteLoad> satWatcher_;
  QFutureWatcher<strata3d::Evaluation> evalWatcher_;
  QFutureWatcher<strata3d::RelationTable> relWatcher_;
  QFutureWatcher<strata3d::DrapeResult> drapeWatcher_;
  std::vector<strata3d::DrapeLegendRow> drapeLegend_;
  strata3d::DrapeResult zoneDrape_;
  QImage qgisDrape_, qgisDrapeDetail_;
  quint64 qgisDrapeGeneration_ = 0;
  RelationPanel* relationPanel_ = nullptr;
  QString relationSiteLabel_;
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
  bool restoreViewPending_ = false;
  QString projectError_;
  QString sketchKey_;                       // 그리는 조사 레이어(survey_area · feature_poly …)
  std::vector<strata3d::Pt> sketch_;        // 찍은 꼭짓점(작업 좌표계)
  std::optional<strata3d::Pt> sketchHover_;
  std::shared_ptr<const strata3d::SceneMesh> sketchOverlay_;
  QPointer<QgsVectorLayer> sketchLayer_;  // 마지막으로 도형을 넣은 레이어
  QString lastSaveError_;
  bool quietSave_ = false;  // Strata 조사 프로젝트를 레이어 칸에 열지 못한 사유(3D 는 GDAL 로 계속 읽는다)
};
