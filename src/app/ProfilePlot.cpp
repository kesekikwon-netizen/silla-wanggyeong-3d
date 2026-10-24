#include "ProfilePlot.h"

#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

#include "KaTheme.h"

ProfilePlot::ProfilePlot(QWidget* parent) : QWidget(parent) { setMinimumHeight(150); }

void ProfilePlot::setProfile(std::vector<strata3d::ProfilePoint> profile) {
  profile_ = std::move(profile);
  update();
}

QImage ProfilePlot::render(int width, int height) {
  QImage img(width, height, QImage::Format_ARGB32_Premultiplied);
  QPainter p(&img);
  paintInto(&p, QRect(0, 0, width, height));
  return img;
}

void ProfilePlot::paintEvent(QPaintEvent*) {
  QPainter p(this);
  paintInto(&p, rect());
}

void ProfilePlot::paintInto(QPainter* p, const QRect& area) {
  const KaTheme::Tokens& t = KaTheme::tokens();
  p->setRenderHint(QPainter::Antialiasing);
  p->fillRect(area, t.surface);
  QFont f = p->font();
  f.setPointSizeF(std::max(8.0, f.pointSizeF()));
  p->setFont(f);
  const QFontMetrics fm(f);
  const int left = fm.horizontalAdvance(QStringLiteral("000.0 m")) + 14, right = 14, top = 10, bottom = fm.height() + 12;
  const QRect plot(area.left() + left, area.top() + top, area.width() - left - right, area.height() - top - bottom);
  p->setPen(t.border);
  p->drawRect(plot);
  if (profile_.size() < 2 || plot.width() < 20 || plot.height() < 20) {
    p->setPen(t.inkMuted);
    p->drawText(area, Qt::AlignCenter, QStringLiteral("지형 위 두 점을 눌러 시굴 트렌치를 그립니다"));
    return;
  }
  double zMin = profile_.front().elevationM, zMax = zMin;
  for (const auto& q : profile_) {
    zMin = std::min(zMin, q.elevationM);
    zMax = std::max(zMax, q.elevationM);
  }
  const double pad = std::max(0.5, (zMax - zMin) * 0.1);
  zMin -= pad;
  zMax += pad;
  const double len = profile_.back().distanceM;
  auto px = [&](const strata3d::ProfilePoint& q) {
    return QPointF(plot.left() + q.distanceM / len * plot.width(), plot.bottom() - (q.elevationM - zMin) / (zMax - zMin) * plot.height());
  };
  p->setPen(QPen(t.border, 1, Qt::DotLine));
  const int rows = 4;
  for (int i = 0; i <= rows; ++i) {
    const double z = zMin + (zMax - zMin) * i / rows;
    const int y = plot.bottom() - int(plot.height() * double(i) / rows);
    if (i > 0 && i < rows) p->drawLine(plot.left(), y, plot.right(), y);
    p->setPen(t.inkMuted);
    p->drawText(QRect(area.left() + 2, y - fm.height() / 2, left - 8, fm.height()), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("%1 m").arg(z, 0, 'f', 1));
    p->setPen(QPen(t.border, 1, Qt::DotLine));
  }
  QPainterPath path;
  path.moveTo(px(profile_.front()));
  for (const auto& q : profile_) path.lineTo(px(q));
  QPainterPath fill = path;
  fill.lineTo(plot.right(), plot.bottom());
  fill.lineTo(plot.left(), plot.bottom());
  fill.closeSubpath();
  QColor wash = t.accent;
  wash.setAlpha(60);
  p->fillPath(fill, wash);
  p->setPen(QPen(QColor(255, 140, 13), 2.2));  // 3D 화면의 트렌치 띠와 같은 주황
  p->drawPath(path);
  p->setPen(t.inkMuted);
  p->drawText(QRect(plot.left(), plot.bottom() + 4, plot.width(), fm.height() + 4), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("시점 0 m"));
  p->drawText(QRect(plot.left(), plot.bottom() + 4, plot.width(), fm.height() + 4), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("종점 %1 m").arg(len, 0, 'f', 1));
}
