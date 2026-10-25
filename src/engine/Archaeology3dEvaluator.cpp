#include "Archaeology3dEvaluator.h"

#include "DemAnalyzer.h"

#include <ogr_geometry.h>

#include <algorithm>
#include <cmath>
#include <memory>

namespace strata3d {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kSmallCells = 5;   // ±150 m (30 m 칸)
constexpr int kLargeCells = 20;  // ±600 m

QString u(const char16_t* s) { return QString::fromUtf16(s); }

// 현장 용어로: OSM 의 historic= 값을 한국어 분류로. 모르는 값은 원문을 그대로 둔다.
QString fieldCategory(const GeoFeature& f) {
  if (f.source != QLatin1String("OSM")) return f.category;
  const QString h = f.attrs.value(QStringLiteral("historic"));
  if (h == QLatin1String("tomb")) return u(u"고분(OSM)");
  if (h == QLatin1String("archaeological_site")) return u(u"유적지(OSM)");
  if (h == QLatin1String("monument")) return u(u"기념물(OSM)");
  if (h == QLatin1String("memorial")) return u(u"기념비(OSM)");
  if (h == QLatin1String("ruins")) return u(u"폐허(OSM)");
  if (h == QLatin1String("castle") || h == QLatin1String("fort")) return u(u"성곽(OSM)");
  if (h == QLatin1String("wayside_shrine")) return u(u"사당·표석(OSM)");
  if (f.kind == FeatureKind::Building && f.heritage) return u(u"유산 건물(OSM)");
  return f.category;
}

QString fieldName(const GeoFeature& f) {
  if (!f.name.isEmpty()) return f.name;
  for (const char* k : {"name:ko", "alt_name", "old_name", "name:en"}) {
    const QString v = f.attrs.value(QLatin1String(k));
    if (!v.isEmpty()) return v;
  }
  return u(u"(이름 없음)");
}

std::unique_ptr<OGRGeometry> makeGeometry(const Pt& p, const std::vector<Pt>& ring) {
  if (ring.size() >= 3) {
    auto poly = std::make_unique<OGRPolygon>();
    OGRLinearRing lr;
    for (const Pt& q : ring) lr.addPoint(q.x, q.y);
    lr.closeRings();
    poly->addRing(&lr);
    return poly;
  }
  return std::make_unique<OGRPoint>(p.x, p.y);
}

double bearingFromTo(const Pt& a, const Pt& b) {
  double deg = std::atan2(b.x - a.x, b.y - a.y) * 180.0 / kPi;  // 북 0°, 시계 방향
  if (deg < 0) deg += 360.0;
  return deg;
}

// 합 영역표(summed-area table)로 정사각 창 평균을 O(1) 에 구한다.
struct Sat {
  int w = 0, h = 0;
  std::vector<double> s;  // (w+1)*(h+1)
  explicit Sat(const DemGrid& g) : w(g.width), h(g.height), s(size_t(g.width + 1) * (g.height + 1), 0.0) {
    for (int r = 0; r < h; ++r) {
      double row = 0;
      for (int c = 0; c < w; ++c) {
        row += g.at(c, r);
        s[size_t(r + 1) * (w + 1) + (c + 1)] = s[size_t(r) * (w + 1) + (c + 1)] + row;
      }
    }
  }
  double mean(int c, int r, int k) const {
    const int c0 = std::max(0, c - k), c1 = std::min(w - 1, c + k), r0 = std::max(0, r - k), r1 = std::min(h - 1, r + k);
    const auto at = [&](int cc, int rr) { return s[size_t(rr) * (w + 1) + cc]; };
    const double sum = at(c1 + 1, r1 + 1) - at(c0, r1 + 1) - at(c1 + 1, r0) + at(c0, r0);
    return sum / double((c1 - c0 + 1) * (r1 - r0 + 1));
  }
};

void tpiGrid(const DemGrid& g, const Sat& sat, int k, std::vector<float>* out, double* sd) {
  out->resize(size_t(g.width) * g.height);
  double sum = 0, sum2 = 0;
  for (int r = 0; r < g.height; ++r) {
    for (int c = 0; c < g.width; ++c) {
      const float v = float(g.at(c, r) - sat.mean(c, r, k));
      (*out)[size_t(r) * g.width + c] = v;
      sum += v;
      sum2 += double(v) * v;
    }
  }
  const double n = double(out->size());
  *sd = std::sqrt(std::max(1e-9, sum2 / n - (sum / n) * (sum / n)));
}

// Weiss(2001) 경사 위치 분류 10종. zs: 작은 이웃, zl: 큰 이웃(표준화 TPI).
QString weiss(double zs, double zl, double slopeDeg) {
  if (zs <= -1) {
    if (zl <= -1) return u(u"협곡·깊게 파인 하곡");
    if (zl < 1) return u(u"사면 중간의 배수로·얕은 골짜기");
    return u(u"고지대의 배수로·계곡 발원부");
  }
  if (zs < 1) {
    if (zl <= -1) return u(u"U자형 골짜기");
    if (zl < 1) return slopeDeg <= 5.0 ? u(u"평지") : u(u"열린 사면");
    return u(u"상부 사면·대지(메사)");
  }
  if (zl <= -1) return u(u"골짜기 안의 국지 능선·구릉");
  if (zl < 1) return u(u"사면 중간의 능선·평지 속 작은 구릉");
  return u(u"산정·높은 능선");
}

}  // namespace

QString bearingName16(double deg) {
  static const char16_t* names[16] = {u"북", u"북북동", u"북동", u"동북동", u"동", u"동남동", u"남동", u"남남동",
                                      u"남", u"남남서", u"남서", u"서남서", u"서", u"서북서", u"북서", u"북북서"};
  double d = std::fmod(deg, 360.0);
  if (d < 0) d += 360.0;
  return u(names[int(std::floor((d + 11.25) / 22.5)) % 16]);
}

Archaeology3dEvaluator::Archaeology3dEvaluator(const DemGrid& dem, std::vector<GeoFeature> features)
    : dem_(std::make_shared<DemGrid>(dem)), bvh_(TerrainBvh::build(dem)), features_(std::move(features)) {
  std::vector<KdTree3::Entry> entries;
  entries.reserve(features_.size());
  for (size_t i = 0; i < features_.size(); ++i) {
    const GeoFeature& f = features_[i];
    entries.push_back({QVector3D(float(f.point.x - dem.centerX()), float(f.point.y - dem.centerY()), 0), uint32_t(i)});
    for (const Pt& p : f.ring) maxReachM_ = std::max(maxReachM_, std::hypot(p.x - f.point.x, p.y - f.point.y));
  }
  tree_.build(std::move(entries));
  const Sat sat(dem);
  tpiGrid(dem, sat, kSmallCells, &tpiSmall_, &sdSmall_);
  tpiGrid(dem, sat, kLargeCells, &tpiLarge_, &sdLarge_);
  // 경사는 Strata 와 같은 계산을 쓴다(hgis/src/core/DemAnalyzer, 같이 컴파일)
  DemAnalyzer::RasterInfo info;
  info.width = dem.width;
  info.height = dem.height;
  const double gt[6] = {dem.originX, dem.pixelSize, 0, dem.originY, 0, -dem.pixelSize};
  std::copy(gt, gt + 6, info.geotransform);
  DemAnalyzer::slopeDegrees(dem.z, info, 1.0, &slopeDeg_);
}

TerrainMetrics Archaeology3dEvaluator::terrainAt(const Pt& p) const {
  const DemGrid& g = *dem_;
  TerrainMetrics m;
  float z = 0;
  if (!g.sample(p.x, p.y, &z)) return m;
  m.elevationM = z;
  const int c = std::clamp(int(std::floor(g.colAt(p.x))), 1, g.width - 2);
  const int r = std::clamp(int(std::floor(g.rowAt(p.y))), 1, g.height - 2);
  const double ps = g.pixelSize;
  // 3×3: a b c / d e f / g h i (행은 북→남)
  const double a = g.at(c - 1, r - 1), b = g.at(c, r - 1), cc = g.at(c + 1, r - 1);
  const double d = g.at(c - 1, r), f = g.at(c + 1, r);
  const double gg = g.at(c - 1, r + 1), h = g.at(c, r + 1), i = g.at(c + 1, r + 1);
  const double dzE = ((cc + 2 * f + i) - (a + 2 * d + gg)) / (8 * ps);
  const double dzN = ((a + 2 * b + cc) - (gg + 2 * h + i)) / (8 * ps);
  m.slopeDeg = slopeDeg_[size_t(r) * g.width + c];  // Strata DemAnalyzer
  if (m.slopeDeg >= 1.0) {
    m.aspectDeg = std::atan2(-dzE, -dzN) * 180.0 / kPi;
    if (m.aspectDeg < 0) m.aspectDeg += 360.0;
    m.aspectName = bearingName16(m.aspectDeg);
  } else {
    m.aspectName = u(u"평지");
  }
  const size_t idx = size_t(r) * g.width + c;
  m.tpiSmallM = tpiSmall_[idx];
  m.tpiLargeM = tpiLarge_[idx];
  m.zSmall = m.tpiSmallM / sdSmall_;
  m.zLarge = m.tpiLargeM / sdLarge_;
  m.landform = weiss(m.zSmall, m.zLarge, m.slopeDeg);
  return m;
}

std::vector<ProfilePoint> Archaeology3dEvaluator::terrainProfile(const Pt& a, const Pt& b, double stepM) const {
  std::vector<ProfilePoint> out;
  const double len = std::hypot(b.x - a.x, b.y - a.y);
  const int n = std::max(1, int(std::ceil(len / std::max(0.5, stepM))));
  for (int i = 0; i <= n; ++i) {
    const double s = len * i / n;
    const double x = a.x + (b.x - a.x) * i / n, y = a.y + (b.y - a.y) * i / n;
    float z = 0;
    if (!dem_->sample(x, y, &z)) continue;
    out.push_back({s, x, y, z});
  }
  return out;
}

Evaluation Archaeology3dEvaluator::evaluate(const Pt& site, const std::vector<Pt>& sitePolygon, const std::vector<double>& radiiM) const {
  Evaluation ev;
  ev.crs = dem_->crs;
  ev.site = site;
  ev.sitePolygon = sitePolygon;
  ev.terrain = terrainAt(site);
  const auto siteGeom = makeGeometry(site, sitePolygon);
  const QVector3D siteLocal(float(site.x - dem_->centerX()), float(site.y - dem_->centerY()), 0);
  double siteReach = 0;  // 조사지 중심에서 다각형 꼭짓점까지 최대 거리
  for (const Pt& q : sitePolygon) siteReach = std::max(siteReach, std::hypot(q.x - site.x, q.y - site.y));

  for (double radius : radiiM) {
    RadiusResult rr;
    rr.radiusM = radius;
    // 둥근(Round, 사분면당 9분할 = 36각형) 버퍼. 직각 모서리가 √2 배로 튀어나오는 Miter 는 쓰지 않는다.
    const std::unique_ptr<OGRGeometry> buf(siteGeom->Buffer(radius, 9));
    if (buf && wkbFlatten(buf->getGeometryType()) == wkbPolygon) {
      const OGRPolygon* poly = buf->toPolygon();
      rr.bufferAreaM2 = poly->get_Area();
      const OGRLinearRing* ring = poly->getExteriorRing();
      for (int i = 0; i + 1 < ring->getNumPoints(); ++i) {
        rr.bufferRing.push_back({ring->getX(i), ring->getY(i)});
        rr.bufferMaxReachM = std::max(rr.bufferMaxReachM, std::hypot(ring->getX(i) - site.x, ring->getY(i) - site.y));
      }
    }
    const auto candidates = tree_.radiusSearch(siteLocal, float(radius + maxReachM_ + siteReach));
    for (const auto& [idx, dist2d] : candidates) {
      (void)dist2d;
      const GeoFeature& f = features_[idx];
      const auto fg = makeGeometry(f.point, f.ring);
      const double d = siteGeom->Distance(fg.get());
      if (d > radius) continue;
      if (f.kind == FeatureKind::Building) {
        ++rr.buildings;
        continue;
      }
      NearbyItem it;
      it.id = f.id;
      it.name = fieldName(f);
      it.category = fieldCategory(f);
      it.source = f.source;
      it.kind = f.kind;
      it.distanceM = d;
      it.point = f.point;
      it.bearingDeg = bearingFromTo(site, f.point);
      it.bearingName = bearingName16(it.bearingDeg);
      float z = 0;
      if (dem_->sample(f.point.x, f.point.y, &z)) {
        it.elevationM = z;
        it.deltaElevM = z - ev.terrain.elevationM;
      }
      rr.heritage.push_back(std::move(it));
    }
    std::sort(rr.heritage.begin(), rr.heritage.end(), [](const NearbyItem& a, const NearbyItem& b) { return a.distanceM < b.distanceM; });
    ev.radii.push_back(std::move(rr));
  }

  {
    int osmB = 0, osmH = 0, khs = 0, shp = 0;
    for (const GeoFeature& f : features_) {
      if (f.source == QLatin1String("OSM")) (f.kind == FeatureKind::Building ? osmB : osmH)++;
      else if (f.source == QLatin1String("KHS")) ++khs;
      else ++shp;
    }
    ev.notes << u(u"평가에 쓴 자료: OSM 건물 %1동·유산 지물 %2건, 국가유산청 %3건, Strata SHP %4건. 이 목록에 없는 유산·건물은 \"없음\"이 아니라 \"자료에 없음\"이다.").arg(osmB).arg(osmH).arg(khs).arg(shp);
  }
  ev.notes << u(u"거리·높이차는 현재 지형(DEM 30 m) 기준이다.");
  ev.notes << u(u"지형 분류는 DEM 형태(Weiss 2001)만으로 가른 참고값이며 지질·하천 자료는 쓰지 않았다.");
  if (ev.terrain.elevationM == 0 && ev.terrain.slopeDeg == 0) ev.notes << u(u"조사지가 DEM 범위 밖일 수 있다.");

  // 요약: 계산한 값만 서술한다.
  QString s = u(u"고도 %1 m, 경사 %2°, 향 %3, 지형 %4. ")
                  .arg(ev.terrain.elevationM, 0, 'f', 1).arg(ev.terrain.slopeDeg, 0, 'f', 1)
                  .arg(ev.terrain.aspectName, ev.terrain.landform);
  for (const RadiusResult& rr : ev.radii) {
    s += u(u"반경 %1 m 안 지정·등록 유산·유적 %2건, 건물 %3동").arg(rr.radiusM, 0, 'f', 0).arg(int(rr.heritage.size())).arg(rr.buildings);
    if (!rr.heritage.empty()) {
      const NearbyItem& n = rr.heritage.front();
      s += u(u"(가장 가까운 곳: %1 %2 m %3)").arg(n.name).arg(n.distanceM, 0, 'f', 0).arg(n.bearingName);
    }
    s += u(u". ");
  }
  ev.summary = s.trimmed();
  return ev;
}

}  // namespace strata3d
