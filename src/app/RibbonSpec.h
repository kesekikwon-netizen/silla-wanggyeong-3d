#pragma once

#include <QList>
#include <QString>

// Strata 3D 위 리본의 단추 표(설계도 v3 3장). 창은 이 표대로 리본을 짓고, 시험은 이 표를 Strata
// MainWindowRibbon.cpp 와 맞대어 본다(묶음 순서·아이콘 이름). 위젯 코드는 없다.
namespace ribbon {

struct Button {
  QString id;      // 창이 동작을 잇는 이름(QAction objectName = "s3d_" + id)
  QString iconId;  // KaIcons::icon(id). Strata 에서 가져온 단추는 Strata 와 같은 id
  QString label;   // 칩 아래 이름
  QString tip;
  bool checkable = false;
  bool menu = false;  // 누르면 고르기 목록이 열린다(시점·높이·이름표·등급·기타)
};

struct Group {
  QString id;
  QString caption;
  // Strata 에 같은 묶음이 있으면 그 묶음 id(survey·record·fetch·basemap·out·more). 3D 전용이면 비어 있다.
  QString strataGroup;
  QList<Button> buttons;
};

// 리본 왼쪽부터의 묶음 순서. 두 앱에 모두 있는 묶음은 Strata 와 같은 순서다.
const QList<Group>& groups();
const Button* find(const QString& buttonId);

}  // namespace ribbon
