#pragma once

#include <QWidget>
#include <vector>

#include "Archaeology3dEvaluator.h"

// 시굴 트렌치 지표면 단면(거리-표고). KaTheme 토큰으로 칠하고 위젯 크기를 따른다.
class ProfilePlot : public QWidget {
  Q_OBJECT
 public:
  explicit ProfilePlot(QWidget* parent = nullptr);
  void setProfile(std::vector<strata3d::ProfilePoint> profile);
  void clear() { setProfile({}); }
  const std::vector<strata3d::ProfilePoint>& profile() const { return profile_; }
  // 단면을 PNG 로 그린다(도면·시험용).
  QImage render(int width, int height);

 protected:
  void paintEvent(QPaintEvent*) override;

 private:
  void paintInto(QPainter* p, const QRect& area);
  std::vector<strata3d::ProfilePoint> profile_;
};
