#include "SheetComposer.h"

#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <cmath>

#include "KaTheme.h"
#include "KaThemeFonts.h"

double niceScaleLengthM(double targetM) {
  if (targetM <= 0) return 1;
  const double mag = std::pow(10.0, std::floor(std::log10(targetM)));
  for (double m : {5.0, 2.0, 1.0}) {
    if (m * mag <= targetM) return m * mag;
  }
  return mag;
}

namespace {
QFont uiFont(int px, bool bold = false) {
  QFont f(KaTheme::uiFontFamily());
  f.setFamilies(KaTheme::uiFontStack());
  f.setPixelSize(px);
  f.setBold(bold);
  return f;
}

void drawNorthArrow(QPainter& p, const QPointF& c, double radius, double yawDeg) {
  p.save();
  p.translate(c);
  // 시점 방위가 yaw 면(북 0° 시계 방향) 지도 북은 화면 위쪽에서 시계 방향으로 yaw 만큼 기운다.
  p.rotate(yawDeg);
  QPainterPath arrow;
  arrow.moveTo(0, -radius);
  arrow.lineTo(radius * 0.38, radius * 0.55);
  arrow.lineTo(0, radius * 0.22);
  arrow.lineTo(-radius * 0.38, radius * 0.55);
  arrow.closeSubpath();
  p.setPen(QPen(KaTheme::tokens().ink, 1.5));
  p.setBrush(KaTheme::tokens().ink);
  p.drawPath(arrow);
  p.restore();
  p.setPen(KaTheme::tokens().ink);
  p.setFont(uiFont(int(radius * 0.7), true));
  const double a = yawDeg * M_PI / 180.0;
  const QPointF n = c + QPointF(std::sin(a), -std::cos(a)) * (radius * 1.45);
  p.drawText(QRectF(n.x() - radius, n.y() - radius * 0.6, radius * 2, radius * 1.2), Qt::AlignCenter, QStringLiteral("N"));
}
}  // namespace

QImage composeSheet(const QImage& view, const SheetInfo& info) {
  const KaTheme::Tokens& t = KaTheme::tokens();
  const int w = view.width();
  const int s = std::max(1, w / 1600);            // 해상도에 비례하는 배율
  const int header = 84 * s, footer = 330 * s, margin = 28 * s;
  QImage sheet(w + 2 * margin, header + view.height() + footer + margin, QImage::Format_RGB32);
  sheet.fill(t.surface);
  QPainter p(&sheet);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);

  // 제목 줄
  p.setPen(t.ink);
  p.setFont(uiFont(34 * s, true));
  p.drawText(QRect(margin, 0, sheet.width() - 2 * margin, header), Qt::AlignVCenter | Qt::AlignLeft, info.title);
  p.setFont(uiFont(18 * s));
  p.setPen(t.inkMuted);
  p.drawText(QRect(margin, 0, sheet.width() - 2 * margin, header), Qt::AlignVCenter | Qt::AlignRight, info.generated);
  p.setPen(QPen(t.borderStrong, 2 * s));
  p.drawLine(margin, header - 6 * s, sheet.width() - margin, header - 6 * s);

  // 본 화면
  const QRect viewRect(margin, header, view.width(), view.height());
  p.drawImage(viewRect.topLeft(), view);
  p.setPen(QPen(t.borderStrong, 2 * s));
  p.setBrush(Qt::NoBrush);
  p.drawRect(viewRect);

  // 하단 세 칸: 평가 요약 | 범례 | 축척·방위·좌표계
  const int top = viewRect.bottom() + 18 * s;
  const int colH = footer - 30 * s;
  const int summaryW = int(sheet.width() * 0.50);
  const int legendW = int(sheet.width() * 0.24);
  const QRect summary(margin, top, summaryW - margin, colH);
  const QRect legend(summary.right() + 14 * s, top, legendW - 14 * s, colH);
  const QRect scaleBox(legend.right() + 14 * s, top, sheet.width() - margin - legend.right() - 14 * s, colH);
  for (const QRect& r : {summary, legend, scaleBox}) {
    p.setPen(QPen(t.border, 1.5 * s));
    p.setBrush(t.altRow);
    p.drawRoundedRect(r, 6 * s, 6 * s);
  }

  p.setPen(t.ink);
  p.setFont(uiFont(20 * s, true));
  p.drawText(summary.adjusted(14 * s, 8 * s, -14 * s, 0), Qt::AlignTop | Qt::AlignLeft, QStringLiteral("평가 요약"));
  p.setFont(uiFont(17 * s));
  QRect body = summary.adjusted(14 * s, 40 * s, -14 * s, -8 * s);
  p.drawText(body, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, info.lines.join(QLatin1Char('\n')));

  // 범례
  p.setFont(uiFont(20 * s, true));
  p.drawText(legend.adjusted(14 * s, 8 * s, -14 * s, 0), Qt::AlignTop | Qt::AlignLeft, QStringLiteral("범례·출처"));
  int y = legend.top() + 44 * s;
  p.setFont(uiFont(16 * s));
  if (info.elevationRamp) {
    const QRect bar(legend.left() + 14 * s, y, 26 * s, 120 * s);
    QLinearGradient g(bar.bottomLeft(), bar.topLeft());
    g.setColorAt(0.0f, QColor::fromRgbF(0.16f, 0.38f, 0.22f));
    g.setColorAt(0.25f, QColor::fromRgbF(0.55f, 0.62f, 0.30f));
    g.setColorAt(0.5f, QColor::fromRgbF(0.80f, 0.72f, 0.45f));
    g.setColorAt(0.75f, QColor::fromRgbF(0.62f, 0.48f, 0.36f));
    g.setColorAt(1.0f, QColor::fromRgbF(0.95f, 0.95f, 0.95f));
    p.setPen(QPen(t.borderStrong, 1));
    p.setBrush(g);
    p.drawRect(bar);
    p.setPen(t.ink);
    p.drawText(bar.right() + 8 * s, bar.top() + 14 * s, QStringLiteral("%1 m").arg(info.zMax, 0, 'f', 0));
    p.drawText(bar.right() + 8 * s, bar.bottom(), QStringLiteral("%1 m").arg(info.zMin, 0, 'f', 0));
    y = bar.bottom() + 12 * s;
  } else {
    p.setPen(t.ink);
    p.drawText(legend.left() + 14 * s, y + 14 * s, QStringLiteral("표고 %1 ~ %2 m").arg(info.zMin, 0, 'f', 0).arg(info.zMax, 0, 'f', 0));
    y += 28 * s;
  }
  struct Key { QColor c; QString text; };
  const Key keys[] = {{QColor(26, 115, 242), QStringLiteral("반경 500 m")}, {QColor(140, 64, 217), QStringLiteral("반경 1000 m")},
                      {QColor(217, 31, 31), QStringLiteral("국보·보물 표지")}, {QColor(230, 90, 20), QStringLiteral("사적 표지")}};
  for (const Key& k : keys) {
    p.setBrush(k.c);
    p.setPen(Qt::NoPen);
    p.drawRect(legend.left() + 14 * s, y, 22 * s, 12 * s);
    p.setPen(t.ink);
    p.drawText(legend.left() + 44 * s, y + 12 * s, k.text);
    y += 22 * s;
  }
  p.setPen(t.inkMuted);
  p.setFont(uiFont(14 * s));
  QStringList src;
  src << QStringLiteral("DEM: ") + info.demName;
  if (!info.imageryName.isEmpty()) src << QStringLiteral("영상: ") + info.imageryName;
  src << QStringLiteral("건물: © OpenStreetMap 기여자 / 유산: 국가유산청");
  p.drawText(QRect(legend.left() + 14 * s, y + 4 * s, legend.width() - 28 * s, legend.bottom() - y - 6 * s), Qt::AlignTop | Qt::TextWordWrap, src.join(QLatin1Char('\n')));

  // 축척자·방위표·좌표계
  p.setPen(t.ink);
  p.setFont(uiFont(20 * s, true));
  p.drawText(scaleBox.adjusted(14 * s, 8 * s, -14 * s, 0), Qt::AlignTop | Qt::AlignLeft, QStringLiteral("축척·방위"));
  if (info.visibleWidthM > 0) {
    const double seg = niceScaleLengthM(info.visibleWidthM * 0.22);
    const double px = seg / info.visibleWidthM * view.width();
    const QPointF a(scaleBox.left() + 18 * s, scaleBox.top() + 96 * s);
    p.setPen(QPen(t.ink, 4 * s));
    p.drawLine(a, a + QPointF(px, 0));
    p.setPen(QPen(t.ink, 2 * s));
    for (double f : {0.0, 0.5, 1.0}) p.drawLine(a + QPointF(px * f, 0), a + QPointF(px * f, -12 * s));
    p.setFont(uiFont(16 * s));
    p.drawText(QPointF(a.x(), a.y() + 24 * s), seg >= 1000 ? QStringLiteral("0 – %1 km").arg(seg / 1000.0, 0, 'g', 3) : QStringLiteral("0 – %1 m").arg(seg, 0, 'f', 0));
    p.setPen(t.inkMuted);
    p.setFont(uiFont(13 * s));
    p.drawText(QRectF(a.x(), a.y() + 30 * s, scaleBox.width() - 36 * s, 34 * s), Qt::TextWordWrap, QStringLiteral("투시 화면: 중앙 기준 근사 축척"));
  }
  drawNorthArrow(p, QPointF(scaleBox.right() - 62 * s, scaleBox.top() + 100 * s), 34.0 * s, info.yawDegFromNorth);
  p.setPen(t.ink);
  p.setFont(uiFont(16 * s));
  p.drawText(QRect(scaleBox.left() + 14 * s, scaleBox.bottom() - 100 * s, scaleBox.width() - 28 * s, 92 * s), Qt::AlignBottom | Qt::TextWordWrap,
             QStringLiteral("좌표계: ") + info.crsLabel + QStringLiteral("\n시점 방위 %1°").arg(info.yawDegFromNorth, 0, 'f', 0));
  p.end();
  return sheet;
}

bool saveSheetPdf(const QImage& sheet, const QString& path, const QString& title) {
  QPdfWriter pdf(path);
  pdf.setPageSize(QPageSize(QPageSize::A3));
  pdf.setPageOrientation(QPageLayout::Landscape);
  pdf.setResolution(150);
  pdf.setTitle(title);
  pdf.setCreator(QStringLiteral("strata-3d"));
  QPainter p(&pdf);
  if (!p.isActive()) return false;
  const QRect page(0, 0, pdf.width(), pdf.height());
  QSize sz = sheet.size();
  sz.scale(page.size(), Qt::KeepAspectRatio);
  p.drawImage(QRect(QPoint((page.width() - sz.width()) / 2, (page.height() - sz.height()) / 2), sz), sheet);
  return p.end();
}
