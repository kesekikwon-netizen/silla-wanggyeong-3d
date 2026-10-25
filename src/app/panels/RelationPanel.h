#pragma once

#include <QWidget>

#include "RelationEngine.h"

class QComboBox;
class QLabel;
class QTextBrowser;
class QTableWidget;

// 관계 표(docs/BLUEPRINT.md 5.5): 내 조사구역 ↔ 주변 유산. 값은 RelationEngine 이 계산하고 여기서는 보여 주기만 한다.
class RelationPanel : public QWidget {
  Q_OBJECT
 public:
  explicit RelationPanel(QWidget* parent = nullptr);

  void setBusy(const QString& message);
  void setTable(const strata3d::RelationTable& table, const QString& siteLabel);
  void clear(const QString& message);
  int rowCount() const;

 signals:
  // 줄을 고르면 그 유산 이름(카메라 이동·강조에 쓴다)
  void heritageChosen(const QString& name);

 private:
  void fillRows();  // 고른 분류의 줄만 표에 채운다

  strata3d::RelationTable last_;  // 마지막 결과(분류를 바꿀 때 다시 계산하지 않는다)
  QComboBox* filter_ = nullptr;
  QLabel* title_ = nullptr;
  QTextBrowser* zones_ = nullptr;  // 보호구역·현상변경허용기준·유존지역·조사 허가구역 판정
  QTableWidget* table_ = nullptr;
  QTextBrowser* notes_ = nullptr;
};
