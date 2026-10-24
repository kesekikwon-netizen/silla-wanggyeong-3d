#include "SheetComposer.h"

#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

// 진북(N)과 방리 구획 축(azimuthDeg) 편차를 동시에 표시하는 정밀 이중 방위표
void drawDualBangriNorthArrow(QPainter& p, const QPointF& c, double radius, double yawDeg, double bangriAzimuthDeg) {
  drawNorthArrow(p, c, radius, yawDeg);
  p.save();
  p.translate(c);
  p.rotate(yawDeg + bangriAzimuthDeg);
  p.setPen(QPen(QColor(215, 135, 15), 2.5, Qt::DashLine));
  p.drawLine(QPointF(0, radius * 0.75), QPointF(0, -radius * 1.15));
  p.drawLine(QPointF(-radius * 0.75, 0), QPointF(radius * 0.75, 0));
  p.restore();
}

// 좌측 하단: 2D 정밀 방리구획·내부 井자 소획·양측 측구·고분·구하도·앵커·시굴트렌치 벡터 평면도 작도
void draw2dPlanimetricBlueprint(QPainter& p, const QRect& box, const BangriSheetData& d) {
  const KaTheme::Tokens& t = KaTheme::tokens();
  p.save();
  p.setClipRect(box);
  p.fillRect(box, QColor(246, 243, 236));

  const auto& params = d.grid.params;
  const double spanU = std::max(300.0, 0.54 * params.extentCols * params.blockWidthM);
  const double spanV = std::max(300.0, 0.54 * params.extentRows * params.blockHeightM);
  const double scale = std::min((box.width() - 48.0) / (2.0 * spanU), (box.height() - 56.0) / (2.0 * spanV));
  const QPointF center(box.center().x(), box.center().y() + 10.0);

  auto worldToPt = [&](const strata3d::Pt& wpt) -> QPointF {
    const double dx = wpt.x - params.originEasting;
    const double dy = wpt.y - params.originNorthing;
    return QPointF(center.x() + dx * scale, center.y() - dy * scale);
  };

  // 1. 구하도·하천 수로망 표시
  for (const auto& ch : d.validation.channels) {
    if (ch.centerline.size() < 2) continue;
    QPainterPath path;
    path.moveTo(worldToPt(ch.centerline.front()));
    for (size_t i = 1; i < ch.centerline.size(); ++i) path.lineTo(worldToPt(ch.centerline[i]));
    p.setPen(QPen(QColor(65, 150, 225, 95), std::max(4.0, ch.halfWidthM * 1.4 * scale), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(path);
    p.setPen(QPen(QColor(30, 110, 195, 190), 1.6, Qt::DashLine));
    p.drawPath(path);
  }

  // 2. 각 방(Block) 및 내부 소획(1~9소획) 격자 배경 및 번호 라벨
  for (const auto& blk : d.grid.blocks) {
    QPolygonF poly;
    for (const auto& c : blk.innerCorners) poly << worldToPt(c);
    p.setPen(QPen(QColor(165, 130, 85, 190), 1.3));
    p.setBrush(QColor(238, 228, 206, 145));
    p.drawPolygon(poly);

    // 내부 소획 점선 및 우상단 대표 샘플 1개 방(col=1, row=1)에만 1~9소획 번호를 깔끔하게 표시
    const bool isSampleSubBlock = (blk.colIndex == 1 && blk.rowIndex == 1);
    p.setFont(uiFont(10, true));
    for (const auto& sub : blk.subLots) {
      QPolygonF spoly;
      for (const auto& sc : sub.corners) spoly << worldToPt(sc);
      p.setPen(QPen(QColor(180, 152, 115, 150), 0.9, Qt::DotLine));
      p.setBrush(Qt::NoBrush);
      p.drawPolygon(spoly);
      if (isSampleSubBlock && scale * params.blockWidthM > 46.0) {
        p.setPen(QColor(95, 70, 40));
        const QPointF scPt = worldToPt(sub.center);
        p.drawText(QRectF(scPt.x() - 18, scPt.y() - 7, 36, 14), Qt::AlignCenter,
                   QStringLiteral("%1칸").arg(sub.subIndex));
      }
    }

    // 방 고유 코드 라벨은 외곽 줄(테두리 방)에만 표시하여 중앙 유적 라벨과 겹치지 않게 한다
    const bool isEdgeBlock = (blk.colIndex == -params.extentCols / 2 ||
                              blk.colIndex == params.extentCols / 2 - 1 ||
                              blk.rowIndex == -params.extentRows / 2 ||
                              blk.rowIndex == params.extentRows / 2 - 1);
    if (isEdgeBlock && scale * params.blockWidthM > 42.0) {
      const QPointF bc = worldToPt(blk.center);
      p.setFont(uiFont(10, true));
      p.setPen(QColor(90, 65, 30, 210));
      p.drawText(QRectF(bc.x() - 42, bc.y() - 9, 84, 18), Qt::AlignCenter, blk.blockCode);
    }
  }

  // 3. 방리 도로(대로·중로·소로) 및 양측 석축 측구 2줄 실선
  for (const auto& rd : d.grid.roads) {
    const QPointF a = worldToPt(rd.a);
    const QPointF b = worldToPt(rd.b);
    if (rd.tier == strata3d::BangriRoadTier::MainAvenue) {
      p.setPen(QPen(QColor(225, 165, 35, 220), std::max(3.5, rd.widthM * scale)));
      p.drawLine(a, b);
    } else if (rd.tier == strata3d::BangriRoadTier::InterBlockRoad) {
      p.setPen(QPen(QColor(205, 150, 55, 200), std::max(2.0, rd.widthM * scale)));
      p.drawLine(a, b);
    } else {
      p.setPen(QPen(QColor(180, 140, 80, 170), std::max(1.2, rd.widthM * scale)));
      p.drawLine(a, b);
    }

    // 양측 측구(배수로) 2줄 청색 실선
    if (rd.tier != strata3d::BangriRoadTier::InnerAlley) {
      p.setPen(QPen(QColor(20, 105, 220, 230), 1.3));
      p.drawLine(worldToPt(rd.leftDitchA), worldToPt(rd.leftDitchB));
      p.drawLine(worldToPt(rd.rightDitchA), worldToPt(rd.rightDitchB));
    }
  }

  // 4. 3D 고분 봉분(능묘) 등고선 원형 표시
  for (const auto& m : d.mounds) {
    const QPointF mc = worldToPt({m.x, m.y});
    const double mr = std::max(4.0, m.radiusM * scale);
    if (!box.adjusted(-30, -30, 30, 30).contains(mc.toPoint())) continue;
    p.setPen(QPen(QColor(55, 115, 40), 1.6));
    p.setBrush(QColor(95, 155, 65, 175));
    p.drawEllipse(mc, mr, mr);
    p.setPen(QPen(QColor(40, 90, 30), 1.0, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(mc, mr * 0.55, mr * 0.55);
  }

  // 5. 발굴 기준점(Anchor) 마커 (겹침 없는 번호 마커) 및 주요 랜드마크 직관적 배지 표시
  auto drawMapPill = [&](const QPointF& pt, const QString& text, const QColor& borderCol, const QColor& textCol) {
    if (!box.adjusted(30, 40, -30, -20).contains(pt.toPoint())) return;
    p.setFont(uiFont(12, true));
    const int tw = p.fontMetrics().horizontalAdvance(text) + 16;
    const QRectF r(pt.x() - tw * 0.5, pt.y() - 12, tw, 24);
    p.setPen(QPen(borderCol, 1.8));
    p.setBrush(QColor(255, 255, 252, 232));
    p.drawRoundedRect(r, 5, 5);
    p.setPen(textCol);
    p.drawText(r, Qt::AlignCenter, text);
  };

  for (size_t i = 0; i < d.anchors.size(); ++i) {
    const auto& a = d.anchors[i];
    if (!a.active) continue;
    const QPointF ap = worldToPt({a.x, a.y});
    p.setPen(QPen(QColor(195, 15, 95), 2.0));
    p.setBrush(QColor(255, 235, 55));
    p.drawEllipse(ap, 6.0, 6.0);
  }

  // 주요 구역 한글 안내 배지 (일반인이 평면도에서도 위치를 즉시 알 수 있도록 표시)
  drawMapPill(worldToPt({params.originEasting, params.originNorthing}),
              params.presetId.startsWith(QLatin1String("gyeongju"))
                  ? QStringLiteral("황룡사 9층목탑 · 바둑판 도시 중심")
                  : (d.preset.name + QStringLiteral(" 중심부")),
              QColor(200, 50, 35), QColor(155, 25, 15));

  if (!d.mounds.empty()) {
    drawMapPill(worldToPt({d.mounds.front().x, d.mounds.front().y}),
                QStringLiteral("대릉원·황남대총 신라 왕릉군"),
                QColor(45, 135, 45), QColor(25, 95, 25));
  }
  if (!d.validation.channels.empty() && !d.validation.channels.front().centerline.empty()) {
    const auto& ch = d.validation.channels.front();
    drawMapPill(worldToPt(ch.centerline[ch.centerline.size() / 2]),
                QStringLiteral("북천(알천) 옛 물길"),
                QColor(25, 110, 200), QColor(15, 75, 150));
  }

  // 6. 추천 직교 시굴 트렌치(T1, T2) 표시
  if (!d.surveyEval.recommendedTrenches.empty()) {
    const auto& tr = d.surveyEval.recommendedTrenches.front();
    const QPointF sPt = worldToPt(tr.startPt);
    const QPointF ePt = worldToPt(tr.endPt);
    p.setPen(QPen(QColor(235, 30, 20), 4.0));
    p.drawLine(sPt, ePt);
    drawMapPill(ePt + QPointF(0, 22),
                QStringLiteral("추천 시굴지점 %1 (도로와 90° 수직)").arg(tr.id),
                QColor(215, 30, 20), QColor(170, 15, 10));
  }

  // 상단 제목 배지
  QRect titleBar(box.left() + 10, box.top() + 8, box.width() - 20, 28);
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(35, 38, 44, 215));
  p.drawRoundedRect(titleBar, 4, 4);
  p.setPen(Qt::white);
  p.setFont(uiFont(14, true));
  p.drawText(titleBar.adjusted(12, 0, -12, 0), Qt::AlignVCenter | Qt::AlignLeft,
             QStringLiteral("2D 정밀 방리구획·井자 내부소획(1~9소획)·양측 석축측구·발굴기준점(Anchor)·직교 시굴트렌치 평면도"));
  p.setFont(uiFont(12, false));
  p.drawText(titleBar.adjusted(12, 0, -12, 0), Qt::AlignVCenter | Qt::AlignRight,
             QStringLiteral("방위각 %1° | 1방 %2m×%3m").arg(params.azimuthDeg, 0, 'f', 1).arg(params.blockWidthM, 0, 'f', 1).arg(params.blockHeightM, 0, 'f', 1));

  p.setPen(QPen(t.borderStrong, 2));
  p.setBrush(Qt::NoBrush);
  p.drawRect(box);
  p.restore();
}

// 우측 상단 패널 1: 방리 도로·양측 석축 측구·방리담장 표준 단면도 (1:100 / 1:200 스케일)
void drawRoadCrossSectionPanel(QPainter& p, const QRect& box, const BangriSheetData& d) {
  const KaTheme::Tokens& t = KaTheme::tokens();
  p.save();
  p.setPen(QPen(t.borderStrong, 2));
  p.setBrush(t.altRow);
  p.drawRoundedRect(box, 6, 6);

  p.setPen(t.ink);
  p.setFont(uiFont(19, true));
  p.drawText(box.adjusted(16, 10, -16, 0), Qt::AlignTop | Qt::AlignLeft,
             QStringLiteral("① 방리 도로 · 양측 석축 측구(側溝) · 방리담장 및 시굴 트렌치(T1) 표준 단면도 (Scale 1:100)"));

  const auto& params = d.grid.params;
  const double roadW = params.interBlockRoadWidthM;
  const double ditchW = params.ditchWidthM;
  const double ditchD = params.ditchDepthM;
  const double wallW = params.wallBaseWidthM;
  const double totalSpanM = roadW + 2.0 * (ditchW + wallW) + 5.0;

  const QRect diag = box.adjusted(24, 46, -24, -18);
  const double cx = diag.center().x();
  const double groundY = diag.top() + diag.height() * 0.54;
  const double pxPerM = (diag.width() * 0.78) / std::max(10.0, totalSpanM);
  const double zScalePx = std::min(pxPerM * 1.35, diag.height() * 0.18);  // 수직 단면 비례

  // 지반 토층 배경
  QRectF soilRect(diag.left(), groundY, diag.width(), diag.bottom() - groundY);
  p.fillRect(soilRect, QColor(218, 198, 168));

  // 도로 다짐층(직경 5~10cm 자갈 + 황갈색 점질토 혼합 다짐층, 두께 0.8m [NODE01102853, NODE12191688])
  const double halfRoadPx = 0.5 * roadW * pxPerM;
  const double bedDepthPx = 0.85 * zScalePx;
  QPolygonF roadBed;
  roadBed << QPointF(cx - halfRoadPx, groundY)
          << QPointF(cx, groundY - 0.18 * zScalePx)  // 중앙 배수 구배(Crown camber)
          << QPointF(cx + halfRoadPx, groundY)
          << QPointF(cx + halfRoadPx * 0.96, groundY + bedDepthPx)
          << QPointF(cx - halfRoadPx * 0.96, groundY + bedDepthPx);
  p.setPen(QPen(QColor(120, 85, 30), 2.0));
  p.setBrush(QColor(215, 165, 65));
  p.drawPolygon(roadBed);

  // 양측 석축 측구(배수로) 단면 (좌·우)
  const double ditchWpx = ditchW * pxPerM;
  const double ditchDpx = ditchD * zScalePx;
  for (int side : {-1, 1}) {
    const double innerX = cx + side * halfRoadPx;
    const double outerX = innerX + side * ditchWpx;
    const double x0 = std::min(innerX, outerX);
    const double x1 = std::max(innerX, outerX);

    // 석축 벽석
    p.setPen(QPen(QColor(70, 75, 82), 1.5));
    p.setBrush(QColor(135, 142, 150));
    p.drawRect(QRectF(x0 - 4, groundY - 2, 5, ditchDpx + 6));
    p.drawRect(QRectF(x1 - 1, groundY - 2, 5, ditchDpx + 6));
    p.drawRect(QRectF(x0 - 4, groundY + ditchDpx, (x1 - x0) + 8, 5));

    // 배수로 내부 수면
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(35, 130, 235, 195));
    p.drawRect(QRectF(x0 + 1, groundY + ditchDpx * 0.25, (x1 - x0) - 2, ditchDpx * 0.75));

    // 외곽 방리담장 기단 및 담장 입면
    const double wallX0 = (side < 0) ? (x0 - 6 - wallW * pxPerM) : (x1 + 6);
    p.setPen(QPen(QColor(95, 55, 30), 1.8));
    p.setBrush(QColor(175, 105, 60));
    p.drawRect(QRectF(wallX0, groundY - 1.35 * zScalePx, wallW * pxPerM, 1.55 * zScalePx));

    // 측구·방리담장 주석 라벨
    p.setFont(uiFont(11, true));
    p.setPen(QColor(20, 75, 150));
    p.drawText(QRectF(x0 - 36, groundY + ditchDpx + 8, (x1 - x0) + 72, 18), Qt::AlignCenter,
               QStringLiteral("석축측구 %1m").arg(ditchW, 0, 'f', 1));
    p.setPen(QColor(95, 50, 25));
    p.drawText(QRectF(wallX0 - 30, groundY - 1.35 * zScalePx - 20, wallW * pxPerM + 60, 18), Qt::AlignCenter,
               QStringLiteral("방리담장(%1m)").arg(wallW, 0, 'f', 1));
  }

  // T1 직교 시굴 트렌치 굴착 범위 표시선
  const double trenchLenM = d.surveyEval.recommendedTrenches.empty() ? 18.0 : d.surveyEval.recommendedTrenches.front().lengthM;
  const double halfTrenchPx = std::min(diag.width() * 0.46, 0.5 * trenchLenM * pxPerM);
  const double arrowY = diag.top() + 26;
  p.setPen(QPen(QColor(220, 30, 20), 2.2, Qt::DashLine));
  p.drawLine(QPointF(cx - halfTrenchPx, arrowY), QPointF(cx + halfTrenchPx, arrowY));
  p.drawLine(QPointF(cx - halfTrenchPx, arrowY - 6), QPointF(cx - halfTrenchPx, groundY + ditchDpx + 10));
  p.drawLine(QPointF(cx + halfTrenchPx, arrowY - 6), QPointF(cx + halfTrenchPx, groundY + ditchDpx + 10));

  p.setFont(uiFont(13, true));
  p.setPen(QColor(200, 25, 15));
  p.drawText(QRectF(cx - halfTrenchPx, arrowY - 22, 2.0 * halfTrenchPx, 20), Qt::AlignCenter,
             QStringLiteral("◀ 추천 직교 시굴 트렌치 T1 (폭 2.0m × 연장 %1m, 도로축과 90.0° 직교) ▶").arg(trenchLenM, 0, 'f', 1));

  // 치수선 및 주석 텍스트
  p.setFont(uiFont(13, true));
  p.setPen(QColor(65, 45, 15));
  p.drawText(QRectF(cx - halfRoadPx, groundY + 6, 2.0 * halfRoadPx, int(bedDepthPx)), Qt::AlignCenter,
             QStringLiteral("방간 중로 %1m (%2척)\n5~10cm 자갈+황갈색 점질토 다짐층(두께 0.4~1.2m)")
                 .arg(roadW, 0, 'f', 1)
                 .arg(strata3d::BangriGridEngine::metersToScaleUnit(roadW, params.scaleUnit), 0, 'f', 1));

  p.setFont(uiFont(12, false));
  p.setPen(t.ink);
  const QString bottomSpec = QStringLiteral(
      "· 간선 대로 폭: %1 m (%2척)  |  방간 중로 폭: %3 m  |  방내 소로 폭: %4 m  |  양측 석축 측구: 폭 %5 m × 깊이 %6 m  |  방리담장 기단 폭: %7 m")
      .arg(params.mainRoadWidthM, 0, 'f', 1)
      .arg(strata3d::BangriGridEngine::metersToScaleUnit(params.mainRoadWidthM, params.scaleUnit), 0, 'f', 1)
      .arg(params.interBlockRoadWidthM, 0, 'f', 1)
      .arg(params.innerAlleyWidthM, 0, 'f', 1)
      .arg(params.ditchWidthM, 0, 'f', 2)
      .arg(params.ditchDepthM, 0, 'f', 2)
      .arg(params.wallBaseWidthM, 0, 'f', 2);
  p.drawText(QRectF(diag.left(), diag.bottom() - 24, diag.width(), 24), Qt::AlignCenter, bottomSpec);

  p.restore();
}

// 우측 중간 패널 2: 방리구획 제원 및 정합성 검증표 (Verification Specification Box)
void drawVerificationSpecPanel(QPainter& p, const QRect& box, const BangriSheetData& d) {
  const KaTheme::Tokens& t = KaTheme::tokens();
  p.save();
  p.setPen(QPen(t.borderStrong, 2));
  p.setBrush(t.altRow);
  p.drawRoundedRect(box, 6, 6);

  p.setPen(t.ink);
  p.setFont(uiFont(19, true));
  p.drawText(box.adjusted(16, 10, -16, 0), Qt::AlignTop | Qt::AlignLeft,
             QStringLiteral("② 방리구획 제원 및 고고학 정합성 검증표 (Verification Specification Box)"));

  const auto& params = d.grid.params;
  const double wChi = strata3d::BangriGridEngine::metersToScaleUnit(params.blockWidthM, params.scaleUnit);
  const double hChi = strata3d::BangriGridEngine::metersToScaleUnit(params.blockHeightM, params.scaleUnit);

  struct SpecRow {
    QString k1, v1, k2, v2;
  };
  const QList<SpecRow> rows = {
      {QStringLiteral("적용 도시·권역"), d.preset.name,
       QStringLiteral("기준 좌표계"), d.baseInfo.crsLabel},
      {QStringLiteral("적용 고대 척도"), strata3d::BangriGridEngine::scaleUnitName(params.scaleUnit),
       QStringLiteral("방위각 편차"), QStringLiteral("%1° (진북 대비 시계방향)").arg(params.azimuthDeg, 0, 'f', 2)},
      {QStringLiteral("1방(坊) 실측 규격"),
       QStringLiteral("동서 %1 m × 남북 %2 m (%3척 × %4척)")
           .arg(params.blockWidthM, 0, 'f', 1).arg(params.blockHeightM, 0, 'f', 1)
           .arg(wChi, 0, 'f', 1).arg(hChi, 0, 'f', 1),
       QStringLiteral("방 내부 소구획"), strata3d::BangriGridEngine::subdivisionModeName(params.subdivisionMode)},
      {QStringLiteral("발굴 기준점 RMSE"),
       QStringLiteral("%1 m (활성 유구 Anchor %2기 자동 정합)").arg(d.anchorRmseM, 0, 'f', 2).arg(int(d.anchors.size())),
       QStringLiteral("현대도로 화석화율"),
       QStringLiteral("%1 % (일치 연장 %2 km / 정렬 필지 %3건)")
           .arg(d.fossilization.fossilizationScorePct, 0, 'f', 1)
           .arg(d.fossilization.matchedLengthM / 1000.0, 0, 'f', 2)
           .arg(d.fossilization.alignedBuildingEdges)},
      {QStringLiteral("지형 실현 가능성"),
       QStringLiteral("평탄 유효 %1% | 고분간섭 %2% | 구하도 %3% | 급경사 %4%")
           .arg(d.validation.validFlatPct, 0, 'f', 1)
           .arg(d.validation.moundInterferencePct, 0, 'f', 1)
           .arg(d.validation.riverChannelPct, 0, 'f', 1)
           .arg(d.validation.steepTerrainPct, 0, 'f', 1),
       QStringLiteral("3D 복원 고분 봉분"),
       QStringLiteral("%1기 융기 (반경 8~42m, 봉분고 +4.0~+23.0m)").arg(int(d.mounds.size()))},
      {QStringLiteral("조사구역 방리 위치"),
       QStringLiteral("%1 [%2] (최근접 도로 %3까지 %4 m)")
           .arg(d.surveyEval.locatedBlockCode, d.surveyEval.locatedSubLotLabel, d.surveyEval.nearestRoadId)
           .arg(d.surveyEval.distToRoadCenterM, 0, 'f', 1),
       QStringLiteral("최적 시굴 트렌치"),
       d.surveyEval.recommendedTrenches.empty()
           ? QStringLiteral("-")
           : QStringLiteral("T1 폭 2.0m × 길이 %1m (도로·측구와 %2° 직교)")
                 .arg(d.surveyEval.recommendedTrenches.front().lengthM, 0, 'f', 1)
                 .arg(d.surveyEval.recommendedTrenches.front().intersectionAngleDeg, 0, 'f', 1)}};

  const QRect tableRect = box.adjusted(16, 44, -16, -52);
  const int rowH = tableRect.height() / std::max(1, int(rows.size()));
  const int w1 = int(tableRect.width() * 0.15);
  const int w2 = int(tableRect.width() * 0.35);
  const int w3 = int(tableRect.width() * 0.15);
  const int w4 = tableRect.width() - w1 - w2 - w3;

  for (int i = 0; i < rows.size(); ++i) {
    const int ry = tableRect.top() + i * rowH;
    const QRect r1(tableRect.left(), ry, w1, rowH);
    const QRect r2(r1.right(), ry, w2, rowH);
    const QRect r3(r2.right(), ry, w3, rowH);
    const QRect r4(r3.right(), ry, w4, rowH);

    p.fillRect(r1, QColor(225, 220, 210));
    p.fillRect(r3, QColor(225, 220, 210));
    p.fillRect(r2, (i % 2 == 0) ? QColor(250, 248, 244) : QColor(242, 239, 232));
    p.fillRect(r4, (i % 2 == 0) ? QColor(250, 248, 244) : QColor(242, 239, 232));

    p.setPen(QPen(t.border, 1));
    for (const QRect& cr : {r1, r2, r3, r4}) p.drawRect(cr);

    p.setPen(t.ink);
    p.setFont(uiFont(13, true));
    p.drawText(r1.adjusted(8, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft, rows[i].k1);
    p.drawText(r3.adjusted(8, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft, rows[i].k2);

    p.setFont(uiFont(13, false));
    p.drawText(r2.adjusted(8, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap, rows[i].v1);
    p.drawText(r4.adjusted(8, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap, rows[i].v2);
  }

  p.setFont(uiFont(13, false));
  p.setPen(t.inkMuted);
  p.drawText(QRect(box.left() + 18, tableRect.bottom() + 6, box.width() - 36, 42),
             Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap,
             QStringLiteral("고증 요약: ") + d.preset.historicalSummary);

  p.restore();
}

// 우측 하단 패널 3: DBpia 학술 문헌 근거 원장 표 (Academic Citation Ledger Table)
void drawDbpiaCitationsPanel(QPainter& p, const QRect& box, const BangriSheetData& d) {
  const KaTheme::Tokens& t = KaTheme::tokens();
  p.save();
  p.setPen(QPen(t.borderStrong, 2));
  p.setBrush(t.altRow);
  p.drawRoundedRect(box, 6, 6);

  p.setPen(t.ink);
  p.setFont(uiFont(19, true));
  p.drawText(box.adjusted(16, 10, -16, 0), Qt::AlignTop | Qt::AlignLeft,
             QStringLiteral("③ DBpia 학술논문 · 발굴조사 보고서 실측 근거 원장 (Academic Citation Ledger · 200편 수록)"));

  const QRect tbl = box.adjusted(16, 44, -16, -14);
  const int maxRows = std::min<int>(7, int(d.citations.size()));
  const int headerH = 30;
  const int rowH = maxRows > 0 ? (tbl.height() - headerH) / maxRows : 36;

  const int cIdW = int(tbl.width() * 0.12);
  const int cAutW = int(tbl.width() * 0.16);
  const int cTitW = int(tbl.width() * 0.42);
  const int cJrnW = tbl.width() - cIdW - cAutW - cTitW;

  // 헤더
  QRect hId(tbl.left(), tbl.top(), cIdW, headerH);
  QRect hAut(hId.right(), tbl.top(), cAutW, headerH);
  QRect hTit(hAut.right(), tbl.top(), cTitW, headerH);
  QRect hJrn(hTit.right(), tbl.top(), cJrnW, headerH);
  p.fillRect(QRect(tbl.left(), tbl.top(), tbl.width(), headerH), QColor(45, 52, 62));
  p.setPen(Qt::white);
  p.setFont(uiFont(13, true));
  p.drawText(hId.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("DBpia ID"));
  p.drawText(hAut.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("저자 (연도)"));
  p.drawText(hTit.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("논문 및 발굴보고서 제목"));
  p.drawText(hJrn.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("학술지 / 본문 핵심 근거"));

  for (int i = 0; i < maxRows; ++i) {
    const auto& c = d.citations[size_t(i)];
    const int ry = tbl.top() + headerH + i * rowH;
    QRect rId(tbl.left(), ry, cIdW, rowH);
    QRect rAut(rId.right(), ry, cAutW, rowH);
    QRect rTit(rAut.right(), ry, cTitW, rowH);
    QRect rJrn(rTit.right(), ry, cJrnW, rowH);

    const QColor bg = (i % 2 == 0) ? QColor(250, 249, 246) : QColor(240, 237, 230);
    p.fillRect(QRect(tbl.left(), ry, tbl.width(), rowH), bg);
    p.setPen(QPen(t.border, 1));
    for (const QRect& cr : {rId, rAut, rTit, rJrn}) p.drawRect(cr);

    p.setFont(uiFont(12, true));
    p.setPen(QColor(25, 85, 165));
    p.drawText(rId.adjusted(6, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, c.nodeId);

    p.setFont(uiFont(12, false));
    p.setPen(t.ink);
    p.drawText(rAut.adjusted(6, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap,
               QStringLiteral("%1 (%2)").arg(c.authors.left(14), c.year));
    p.drawText(rTit.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap,
               QStringLiteral("「%1」").arg(c.title));

    QString note = c.journal;
    if (!c.oneLineSummary.isEmpty()) {
      note += QStringLiteral(" · ") + c.oneLineSummary.left(48);
    } else if (!c.abstractText.isEmpty()) {
      note += QStringLiteral(" · ") + c.abstractText.left(48);
    }
    p.setPen(t.inkMuted);
    p.drawText(rJrn.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap, note);
  }

  p.restore();
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
  QList<Key> keys = {{QColor(26, 115, 242), QStringLiteral("반경 500 m")}, {QColor(140, 64, 217), QStringLiteral("반경 1000 m")},
                     {QColor(217, 31, 31), QStringLiteral("국보·보물 표지")}, {QColor(230, 90, 20), QStringLiteral("사적 표지")}};
  if (info.hasTrench) keys.append({QColor(255, 140, 13), QStringLiteral("시굴 트렌치(노란 기둥: 양 끝)")});
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

QImage composeBangriSheet(const QImage& view, const BangriSheetData& data) {
  const KaTheme::Tokens& t = KaTheme::tokens();
  constexpr int kSheetW = 3400;
  constexpr int kSheetH = 2400;
  constexpr int kMargin = 32;
  constexpr int kHeaderH = 92;
  constexpr int kFooterH = 220;

  QImage sheet(kSheetW, kSheetH, QImage::Format_RGB32);
  sheet.fill(t.surface);

  QPainter p(&sheet);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);

  // 상단 헤더 블록
  p.setPen(t.ink);
  p.setFont(uiFont(34, true));
  const QString mainTitle = data.baseInfo.title.isEmpty()
                                ? QStringLiteral("신라왕경 · 통일신라 9주 5소경 방리구획(坊里區劃) 3D 복원 및 정밀 고고학 도면")
                                : data.baseInfo.title;
  p.drawText(QRect(kMargin, 0, kSheetW - 2 * kMargin, kHeaderH), Qt::AlignVCenter | Qt::AlignLeft, mainTitle);

  p.setFont(uiFont(18, false));
  p.setPen(t.inkMuted);
  p.drawText(QRect(kMargin, 0, kSheetW - 2 * kMargin, kHeaderH), Qt::AlignVCenter | Qt::AlignRight,
             QStringLiteral("%1  |  %2  |  고지형(Bare-Earth) & 3D 고분 융기 (Z×%3)")
                 .arg(data.preset.modernRegion, data.baseInfo.generated)
                 .arg(data.zExaggeration, 0, 'f', 1));

  p.setPen(QPen(t.borderStrong, 3));
  p.drawLine(kMargin, kHeaderH - 6, kSheetW - kMargin, kHeaderH - 6);

  // 본문 영역(좌측 60%: 3D 입체 방리·고분 뷰 + 2D 정밀 평면도 / 우측 40%: 단면도 + 제원검증표 + DBpia 학술원장표)
  const int bodyTop = kHeaderH + 8;
  const int bodyH = kSheetH - bodyTop - kFooterH - kMargin;
  const int leftW = int((kSheetW - 2 * kMargin - 20) * 0.58);
  const int rightW = (kSheetW - 2 * kMargin - 20) - leftW;

  const int view3dH = int(bodyH * 0.62);
  const int plan2dH = bodyH - view3dH - 14;

  const QRect view3dRect(kMargin, bodyTop, leftW, view3dH);
  const QRect plan2dRect(kMargin, view3dRect.bottom() + 14, leftW, plan2dH);

  // 1. 좌측 상단: 3D 고지형 + 3D 고분 봉분 + 방리 격자 뷰
  if (!view.isNull()) {
    const QImage scaled = view.scaled(view3dRect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    p.drawImage(view3dRect.topLeft(), scaled);
  }
  p.setPen(QPen(t.borderStrong, 2));
  p.setBrush(Qt::NoBrush);
  p.drawRect(view3dRect);

  // 3D 뷰 상단 오버레이 배지
  QRect badge3d(view3dRect.left() + 12, view3dRect.top() + 12, view3dRect.width() - 24, 34);
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(24, 28, 34, 200));
  p.drawRoundedRect(badge3d, 5, 5);
  p.setPen(Qt::white);
  p.setFont(uiFont(15, true));
  p.drawText(badge3d.adjusted(14, 0, -14, 0), Qt::AlignVCenter | Qt::AlignLeft,
             QStringLiteral("3D 고지형(Bare-Earth) · 적석목곽분 3D 봉분 융기 · 방리 도로(대로·중로·井자 소로) · 양측 석축측구 · 직교 시굴트렌치(T1·T2) 중첩도"));

  // 2. 좌측 하단: 2D 정밀 방리구획·내부 井자 소획·측구·앵커·시굴트렌치 벡터 평면도
  draw2dPlanimetricBlueprint(p, plan2dRect, data);

  // 3. 우측 3단 패널 (단면도 31% | 제원 검증표 36% | DBpia 학술 문헌 원장 33%)
  const int rightX = view3dRect.right() + 20;
  const int r1H = int(bodyH * 0.30);
  const int r2H = int(bodyH * 0.36);
  const int r3H = bodyH - r1H - r2H - 28;

  const QRect panelCross(rightX, bodyTop, rightW, r1H);
  const QRect panelSpec(rightX, panelCross.bottom() + 14, rightW, r2H);
  const QRect panelCite(rightX, panelSpec.bottom() + 14, rightW, r3H);

  drawRoadCrossSectionPanel(p, panelCross, data);
  drawVerificationSpecPanel(p, panelSpec, data);
  drawDbpiaCitationsPanel(p, panelCite, data);

  // 4. 하단 타이틀 블록 (Title Block: 범례 8종 + 축척 막대 + 이중 방위표 + 도면 메타데이터)
  const QRect footerRect(kMargin, bodyTop + bodyH + 16, kSheetW - 2 * kMargin, kFooterH - 16);
  p.setPen(QPen(t.borderStrong, 2));
  p.setBrush(t.altRow);
  p.drawRoundedRect(footerRect, 6, 6);

  // 범례 8항목
  p.setPen(t.ink);
  p.setFont(uiFont(18, true));
  p.drawText(footerRect.adjusted(18, 10, -18, 0), Qt::AlignTop | Qt::AlignLeft,
             QStringLiteral("④ 범례 (Legend) 및 축척·방위·좌표계 (Title Block)"));

  struct LegendItem {
    QColor col;
    QString label;
  };
  const QList<LegendItem> legendItems = {
      {QColor(240, 188, 46), QStringLiteral("주작대로급 간선 대로 (15~23m, 60척)")},
      {QColor(214, 163, 56), QStringLiteral("방간 중로 (8.0~10.5m, 방 외곽 구획로)")},
      {QColor(184, 143, 71), QStringLiteral("방내 소로 (4.5~6.0m, 井자 9분할 / 十자 4분할)")},
      {QColor(25, 118, 245), QStringLiteral("양측 석축 측구 (도로 양쪽 배수로 폭 0.8~1.5m)")},
      {QColor(13, 235, 224), QStringLiteral("현대 도로·필지 화석화 일치 구간 (Fossilized Grid)")},
      {QColor(95, 155, 65), QStringLiteral("3D 복원 고분 봉분 (대릉원·황남대총·천마총·쪽샘 등)")},
      {QColor(45, 155, 235), QStringLiteral("북천(알천)·남천·발천 및 구하도(옛 물길) 범람원")},
      {QColor(235, 35, 20), QStringLiteral("발굴 기준점(Anchor) 및 추천 90° 직교 시굴트렌치(T1·T2)")}};

  const int legStartX = footerRect.left() + 18;
  const int legStartY = footerRect.top() + 46;
  const int legColW = int(footerRect.width() * 0.165);
  p.setFont(uiFont(14, false));
  for (int i = 0; i < legendItems.size(); ++i) {
    const int col = i / 2;
    const int row = i % 2;
    const int lx = legStartX + col * legColW;
    const int ly = legStartY + row * 42;
    p.setPen(QPen(t.borderStrong, 1));
    p.setBrush(legendItems[i].col);
    p.drawRect(lx, ly, 26, 16);
    p.setPen(t.ink);
    p.drawText(QRect(lx + 34, ly - 4, legColW - 42, 28), Qt::AlignVCenter | Qt::AlignLeft, legendItems[i].label);
  }

  // 우측 축척 막대 + 이중 방위표 + 좌표계/원점 표기
  const int rightMetaX = legStartX + 4 * legColW + 20;
  if (data.baseInfo.visibleWidthM > 0) {
    const double seg = niceScaleLengthM(data.baseInfo.visibleWidthM * 0.20);
    const double px = std::clamp(seg / data.baseInfo.visibleWidthM * view3dRect.width(), 80.0, 260.0);
    const QPointF sbA(rightMetaX, footerRect.top() + 68);
    p.setPen(QPen(t.ink, 4));
    p.drawLine(sbA, sbA + QPointF(px, 0));
    for (double f : {0.0, 0.5, 1.0}) {
      p.drawLine(sbA + QPointF(px * f, 0), sbA + QPointF(px * f, -12));
    }
    p.setFont(uiFont(15, true));
    p.drawText(QPointF(sbA.x(), sbA.y() + 24),
               seg >= 1000 ? QStringLiteral("0 – %1 km (중앙 축척)").arg(seg / 1000.0, 0, 'g', 3)
                           : QStringLiteral("0 – %1 m (중앙 축척)").arg(seg, 0, 'f', 0));
  }

  p.setFont(uiFont(14, false));
  p.setPen(t.ink);
  const QString metaStr = QStringLiteral(
      "좌표계: %1  |  방리 원점: E %2 m, N %3 m\n"
      "DEM: %4  |  학술원장: DBpia 200편 실측 검증")
      .arg(data.baseInfo.crsLabel)
      .arg(data.grid.params.originEasting, 0, 'f', 1)
      .arg(data.grid.params.originNorthing, 0, 'f', 1)
      .arg(data.baseInfo.demName);
  p.drawText(QRect(rightMetaX, footerRect.top() + 104, footerRect.right() - rightMetaX - 150, 72),
             Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, metaStr);

  drawDualBangriNorthArrow(
      p,
      QPointF(footerRect.right() - 78, footerRect.center().y() + 6),
      42.0,
      data.baseInfo.yawDegFromNorth,
      data.grid.params.azimuthDeg);

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
