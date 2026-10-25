#include "DrapeRaster.h"

#include <QHash>
#include <QSet>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <map>

namespace strata3d {

DrapeStyle drapeStyleFor(const GeoFeature& f) {
  // 조용한 도판 색(설계도 v3 토큰): 보호구역 = 짙은 붉은 흙, 현상변경 = 놋쇠, 유존지역 = 황토,
  // 허가구역 = 풀색·쪽색, 지정 유산 면 = 먹. 면은 옅게, 경계는 또렷하게(위성 영상이 비쳐야 한다).
  const QString& c = f.category;
  if (f.source == QLatin1String("SURVEY")) {  // 내 조사구역: 가장 위, 가장 굵게(Strata 조사구역 색 계열)
    QColor fill(0xE0, 0x7B, 0x39);
    fill.setAlpha(46);
    return DrapeStyle{fill, QColor(0xD9, 0x5F, 0x1E), 4.0, 90};
  }
  auto st = [](QColor fill, int fillA, QColor stroke, double w, int order) {
    fill.setAlpha(fillA);
    return DrapeStyle{fill, stroke, w, order};
  };
  if (c == QStringLiteral("국가지정유산보호구역")) return st(QColor(0xA3, 0x3B, 0x3B), 64, QColor(0xA3, 0x3B, 0x3B), 2.5, 30);
  if (c == QStringLiteral("시도지정유산보호구역")) return st(QColor(0xB5, 0x57, 0x3A), 64, QColor(0xB5, 0x57, 0x3A), 2.5, 31);
  if (c == QStringLiteral("현상변경허용기준")) return st(QColor(0x9A, 0x7B, 0x38), 26, QColor(0x9A, 0x7B, 0x38), 1.5, 10);
  if (c == QStringLiteral("매장유산유존지역")) return st(QColor(0xC9, 0x9A, 0x2E), 56, QColor(0x7A, 0x5A, 0x00), 2.0, 20);
  if (c == QStringLiteral("발굴사업허가구역")) return st(QColor(0x3F, 0x6B, 0x31), 34, QColor(0x3F, 0x6B, 0x31), 1.5, 40);
  if (c == QStringLiteral("지표사업허가구역")) return st(QColor(0x2F, 0x5D, 0x8A), 30, QColor(0x2F, 0x5D, 0x8A), 1.5, 41);
  for (const char16_t* g : {u"발굴유적위치도", u"지표유적위치도", u"문화유적분포지도"})
    if (c == QString::fromUtf16(g)) return st(QColor(0x5E, 0x5D, 0x59), 0, QColor(0x5E, 0x5D, 0x59), 1.0, 50);
  // 지정·등록 유산의 면(국가지정유산·시도지정유산 …)과 그 밖
  return st(QColor(0x14, 0x14, 0x13), 22, QColor(0x14, 0x14, 0x13), 2.0, 60);
}

QString drapeLegendName(const GeoFeature& f) {
  if (f.source == QLatin1String("SURVEY") || zoneLikeCategory(f.category)) return f.category;
  for (const char16_t* g : {u"발굴유적위치도", u"지표유적위치도", u"문화유적분포지도"})
    if (f.category == QString::fromUtf16(g)) return QStringLiteral("발굴·지표·분포 위치도");
  return QStringLiteral("지정·등록 유산(면)");
}

bool zoneLikeCategory(const QString& c) {
  for (const char16_t* z : {u"국가지정유산보호구역", u"시도지정유산보호구역", u"현상변경허용기준", u"매장유산유존지역", u"발굴사업허가구역", u"지표사업허가구역"})
    if (c == QString::fromUtf16(z)) return true;
  return false;
}

namespace {

QPainterPath pathOf(const GeoFeature& f, const DemGrid& dem, double x0, double y0, double mPerPx) {
  QPainterPath path;
  path.setFillRule(Qt::OddEvenFill);  // 구멍은 칠하지 않는다
  auto add = [&](const std::vector<Pt>& ring) {
    if (ring.size() < 3) return;
    QPolygonF poly;
    poly.reserve(qsizetype(ring.size()) + 1);
    for (const Pt& p : ring) poly << QPointF((p.x - x0) / mPerPx, (y0 - p.y) / mPerPx);
    poly << poly.front();
    path.addPolygon(poly);
  };
  add(f.ring);
  for (const auto& h : f.holes) add(h);
  Q_UNUSED(dem);
  return path;
}

QImage paintWindow(const DemGrid& dem, const std::vector<std::pair<const GeoFeature*, DrapeStyle>>& shapes,
                   double x0, double y0, double spanM, int texels, int* drawn) {
  QImage img(texels, texels, QImage::Format_RGBA8888_Premultiplied);
  img.fill(Qt::transparent);
  QPainter p(&img);
  p.setRenderHint(QPainter::Antialiasing, true);
  const double mPerPx = spanM / texels;
  const QRectF view(0, 0, texels, texels);
  int n = 0;
  for (const auto& [f, s] : shapes) {
    double minX = 1e300, maxX = -1e300, minY = 1e300, maxY = -1e300;
    for (const Pt& q : f->ring) { minX = std::min(minX, q.x); maxX = std::max(maxX, q.x); minY = std::min(minY, q.y); maxY = std::max(maxY, q.y); }
    const QRectF box((minX - x0) / mPerPx, (y0 - maxY) / mPerPx, (maxX - minX) / mPerPx, (maxY - minY) / mPerPx);
    if (!box.adjusted(-4, -4, 4, 4).intersects(view)) continue;
    const QPainterPath path = pathOf(*f, dem, x0, y0, mPerPx);
    if (s.fill.alpha() > 0) p.fillPath(path, s.fill);
    if (s.stroke.alpha() > 0) {
      // 넓은 창(한 칸 수 m)에서도 경계가 읽히게 최소 2 칸
      QPen pen(s.stroke, std::max(2.0, s.strokeM / mPerPx));
      pen.setJoinStyle(Qt::RoundJoin);
      p.strokePath(path, pen);
    }
    ++n;
  }
  p.end();
  if (drawn) *drawn = std::max(*drawn, n);
  return img.convertToFormat(QImage::Format_RGBA8888);
}

}  // namespace

DrapeResult rasterizeDrape(const DemGrid& dem, const std::vector<GeoFeature>& features, int texels, double detailHalfM, int detailTexels) {
  DrapeResult out;
  if (!dem.valid() || texels < 16) return out;
  std::vector<std::pair<const GeoFeature*, DrapeStyle>> shapes;
  std::map<QString, DrapeLegendRow> legend;
  QSet<QString> seen;
  for (const GeoFeature& f : features) {
    if (f.kind != FeatureKind::HeritageArea || f.ring.size() < 3) continue;
    if (f.source == QLatin1String("OSM")) continue;  // OSM 면은 Strata·국가유산청 자료가 아니다(입히지 않는다)
    // 같은 도형이 유산마다 따로 등록된 분류(현상변경허용기준: 도형 4개에 기록 182건)는 한 번만 칠한다.
    // 겹쳐 칠하면 반투명이 쌓여 그 구역만 진해 보인다.
    const Pt& a = f.ring.front();
    const Pt& m = f.ring[f.ring.size() / 2];
    const QString key = QStringLiteral("%1|%2|%3|%4|%5|%6").arg(f.category).arg(f.ring.size()).arg(a.x, 0, 'f', 2).arg(a.y, 0, 'f', 2).arg(m.x, 0, 'f', 2).arg(m.y, 0, 'f', 2);
    if (seen.contains(key)) continue;
    seen.insert(key);
    const DrapeStyle s = drapeStyleFor(f);
    shapes.push_back({&f, s});
    const QString name = drapeLegendName(f);
    auto& row = legend[name];
    row.category = name;
    row.style = s;
    ++row.count;
  }
  // 넓은 구역을 먼저(아래), 좁은 유산 면을 나중에(위) 칠한다
  std::stable_sort(shapes.begin(), shapes.end(), [](const auto& a, const auto& b) { return a.second.order < b.second.order; });
  const double spanX = dem.width * dem.pixelSize;
  out.image = paintWindow(dem, shapes, dem.originX, dem.originY, spanX, texels, &out.drawn);
  if (detailHalfM > 0 && detailHalfM * 2 < spanX * 0.75) {
    out.detail = paintWindow(dem, shapes, dem.centerX() - detailHalfM, dem.centerY() + detailHalfM, detailHalfM * 2, detailTexels, nullptr);
    out.detailHalfM = detailHalfM;
  }
  for (auto& [k, row] : legend) out.legend.push_back(row);
  std::sort(out.legend.begin(), out.legend.end(), [](const DrapeLegendRow& a, const DrapeLegendRow& b) { return a.style.order < b.style.order; });
  return out;
}

}  // namespace strata3d
