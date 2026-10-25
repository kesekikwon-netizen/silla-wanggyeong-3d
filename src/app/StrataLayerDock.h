#pragma once

#include <QFrame>
#include <QSet>
#include <QString>

class KaLayerInformationModel;
class KaLayerInformationView;
class QgsProject;
class QLabel;
class QToolButton;

// Strata 의 왼쪽 「레이어」 칸을 Strata 부품 그대로 세운다(hgis MainWindow.cpp 의 레이어 카드와 같은 짜임):
// 제목줄(「레이어」 + 전체 끄기/켜기), KaLayerListChrome(찾기·편집 중 표시), KaLayerInformationView +
// KaLayerInformationModel(구역 조사 데이터 · 지적도 · 주변·참조 · 배경 지도), 아래 KaLayerInformationPanel.
// 레이어는 QgsProject::instance() 에 Strata 가 저장한 조사 프로젝트를 그대로 연 것이다.
// 체크를 바꾸면 layersVisibilityChanged 로 알린다: 3D 창이 그 레이어의 3D 표시를 켜고 끈다.
class StrataLayerDock : public QFrame {
  Q_OBJECT
 public:
  explicit StrataLayerDock(QgsProject* project, QWidget* parent = nullptr);

  KaLayerInformationView* view() const { return view_; }
  // 꺼진 레이어의 이름(조사 프로젝트의 레이어 이름 = 3D 지물의 분류 이름)
  QSet<QString> hiddenLayerNames() const;
  // 꺼진 레이어의 ka_hgis/layer_key(survey_area · trial_trench …)
  QSet<QString> hiddenLayerKeys() const;
  int layerCount() const;

 signals:
  void layersVisibilityChanged();
  // 레이어 칸에서 고른 레이어(이름). 비면 고른 것 없음
  void currentLayerChanged(const QString& name);

 private:
  void toggleAll();
  void refreshChrome();

  QgsProject* project_ = nullptr;
  KaLayerInformationModel* model_ = nullptr;
  KaLayerInformationView* view_ = nullptr;
  QToolButton* checkAll_ = nullptr;
  QLabel* empty_ = nullptr;
};
