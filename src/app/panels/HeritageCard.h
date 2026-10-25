#pragma once

#include <QFrame>
#include <QList>
#include <QPair>
#include <QString>

class QFormLayout;
class QLabel;

// 고른 유적의 카드(설계도 v3 2장 「유적 카드」). Strata 속성 칸 안, Strata 카드와 같은 모양(같은 objectName 이라
// Strata 스타일시트가 그대로 입힌다). 원 기록의 칸은 고치지 않고 그대로 보이고(읽기 전용),
// 3D 가 계산한 값(조사구역까지 거리·방위·높이차)은 따로 「3D 계산」 줄로 구분한다.
class HeritageCard : public QFrame {
  Q_OBJECT
 public:
  using Rows = QList<QPair<QString, QString>>;
  explicit HeritageCard(QWidget* parent = nullptr);

  // title: 유적 이름만(siteNameOnly), kind: 레이어 이름, record: 원 기록의 칸(이름, 값), computed: 3D 계산 줄,
  // footnote: 출처·등급 한 줄
  void setRecord(const QString& title, const QString& kind, const Rows& record, const Rows& computed, const QString& footnote);
  void clearRecord();

 private:
  void fill(QFormLayout* form, const Rows& rows);

  QLabel* name_ = nullptr;
  QLabel* kind_ = nullptr;
  QLabel* recordCaption_ = nullptr;
  QFormLayout* record_ = nullptr;
  QLabel* computedCaption_ = nullptr;
  QFormLayout* computed_ = nullptr;
  QLabel* foot_ = nullptr;
};
