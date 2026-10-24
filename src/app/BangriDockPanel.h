#pragma once

#include <QWidget>
#include <vector>

#include "BangriGridEngine.h"
#include "SheetComposer.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;
class QTextBrowser;

class BangriDockPanel : public QWidget {
  Q_OBJECT
 public:
  enum class CameraTourTarget {
    HwangnyongsaAndGrid = 0,  // 황룡사 9층목탑 · 분황사 모전석탑 · 구황동 원지
    PalaceAndWolji = 1,       // 동궁과 월지 · 월성(왕궁·해자·석빙고)
    RoyalTombsDaereung = 2,   // 대릉원(황남대총·천마총·미추왕릉) · 봉황대 · 금관총
    StreetCloseUp = 3,        // 첨성대 · 계림 · 내물왕릉 · 월정교 · 경주향교
    CapitalOverview = 4       // 경주 신라 왕경 유적 전체 조망
  };

  explicit BangriDockPanel(QWidget* parent = nullptr);

  bool gridOverlayEnabled() const;
  bool architecture3dEnabled() const;
  bool calloutLabelsEnabled() const;
  bool topBannerEnabled() const;
  bool bareEarthMode() const;
  bool reconstructMounds() const;
  float hudFontScale() const;
  float hudBoxScale() const;
  void setHudScales(float fontScale, float boxScale, bool emitSignal = false);
  QString currentPresetId() const;
  strata3d::BangriParams currentParams() const;

  void setPresetById(const QString& presetId, bool emitSignals = true);
  void setParams(const strata3d::BangriParams& params, bool emitSignals = false);
  void setBareEarthAndMounds(bool bareEarth, bool mounds, bool emitSignals = false);
  void updateVerificationSummary(const BangriSheetData& data);

 signals:
  void presetSelected(const QString& presetId);
  void paramsChanged(const strata3d::BangriParams& params);
  void terrainModeChanged(bool gridEnabled, bool bareEarth, bool mounds);
  void cameraTourRequested(int targetKind);
  void uiAndHudScaleChanged(float fontScale, float boxScale);
  void autoFitRequested();
  void exportBangriSheetRequested();
  void applyRecommendedTrenchRequested(double ax, double ay, double bx, double by);

 private:
  void syncChiLabels();
  void populateFromParams(const strata3d::BangriParams& p);
  strata3d::BangriParams collectParams() const;

  std::vector<strata3d::BangriCityPreset> presets_;
  strata3d::BangriParams baseParams_;
  bool updatingUi_ = false;

  QCheckBox* arch3dCheck_ = nullptr;
  QCheckBox* labelsCheck_ = nullptr;
  QCheckBox* topBannerCheck_ = nullptr;
  QCheckBox* gridEnabledCheck_ = nullptr;
  QCheckBox* bareEarthCheck_ = nullptr;
  QCheckBox* moundCheck_ = nullptr;
  QSlider* fontSlider_ = nullptr;
  QSlider* boxSlider_ = nullptr;
  QLabel* fontPctLabel_ = nullptr;
  QLabel* boxPctLabel_ = nullptr;
  QComboBox* presetCombo_ = nullptr;
  QComboBox* scaleCombo_ = nullptr;
  QDoubleSpinBox* blockWidthSpin_ = nullptr;
  QDoubleSpinBox* blockHeightSpin_ = nullptr;
  QLabel* blockChiLabel_ = nullptr;
  QDoubleSpinBox* azimuthSpin_ = nullptr;
  QComboBox* subdivCombo_ = nullptr;
  QDoubleSpinBox* mainRoadSpin_ = nullptr;
  QDoubleSpinBox* interRoadSpin_ = nullptr;
  QDoubleSpinBox* alleySpin_ = nullptr;
  QDoubleSpinBox* ditchWidthSpin_ = nullptr;
  QSpinBox* colsSpin_ = nullptr;
  QSpinBox* rowsSpin_ = nullptr;
  QPushButton* autoFitBtn_ = nullptr;
  QPushButton* applyTrenchBtn_ = nullptr;
  QPushButton* exportBtn_ = nullptr;
  QTextBrowser* summaryBrowser_ = nullptr;

  strata3d::Pt recTrenchA_{0.0, 0.0};
  strata3d::Pt recTrenchB_{0.0, 0.0};
  bool hasRecTrench_ = false;
};
