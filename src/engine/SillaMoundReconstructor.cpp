#include "SillaMoundReconstructor.h"

#include <QVector3D>
#include <algorithm>
#include <array>
#include <cmath>

#include "CrsTransform.h"
#include "EvaluationOverlay.h"
#include "Triangulate.h"

namespace strata3d {

namespace {

using Color = std::array<float, 4>;

void appendVertex(SceneMesh* m, const QVector3D& p, const QVector3D& n, const Color& c) {
  m->vertices.insert(m->vertices.end(), {p.x(), p.y(), p.z(), n.x(), n.y(), n.z(), c[0], c[1], c[2], c[3]});
}

void appendTri(SceneMesh* m, uint32_t a, uint32_t b, uint32_t c) {
  m->indices.insert(m->indices.end(), {a, b, c});
}

double polygonAreaAbs(const std::vector<Pt>& ring) {
  if (ring.size() < 3) return 0.0;
  return std::abs(signedArea(ring));
}

}  // namespace

bool SillaMoundReconstructor::isTumulusFeature(const GeoFeature& f) {
  const QString hist = f.attrs.value(QStringLiteral("historic")).toLower();
  const QString arch = f.attrs.value(QStringLiteral("archaeological_site")).toLower();
  const QString tomb = f.attrs.value(QStringLiteral("tomb")).toLower();
  if (hist == QLatin1String("tomb") || hist == QLatin1String("tumulus") ||
      arch == QLatin1String("tumulus") || arch == QLatin1String("megalith") ||
      tomb == QLatin1String("tumulus") || tomb == QLatin1String("royal_tomb")) {
    return true;
  }

  const QString combined = f.name + QLatin1Char(' ') + f.category + QLatin1Char(' ') +
                           f.attrs.value(QStringLiteral("name")) + QLatin1Char(' ') +
                           f.attrs.value(QStringLiteral("ccbaMnm1"));

  static const QStringList kKeywords = {
      QStringLiteral("왕릉"),     QStringLiteral("고분"),   QStringLiteral("대릉원"),
      QStringLiteral("쪽샘"),     QStringLiteral("봉황대"), QStringLiteral("황남대총"),
      QStringLiteral("천마총"),   QStringLiteral("금관총"), QStringLiteral("서봉총"),
      QStringLiteral("호우총"),   QStringLiteral("식리총"), QStringLiteral("금령총"),
      QStringLiteral("은령총"),   QStringLiteral("검총"),   QStringLiteral("마총"),
      QStringLiteral("오릉"),     QStringLiteral("금척리"), QStringLiteral("서악동"),
      QStringLiteral("누암리"),   QStringLiteral("노서리"), QStringLiteral("노동리"),
      QStringLiteral("황오리")};

  for (const QString& kw : kKeywords) {
    if (combined.contains(kw, Qt::CaseInsensitive)) return true;
  }
  return false;
}

SillaMound SillaMoundReconstructor::moundFromFeature(const GeoFeature& f) {
  SillaMound m;
  m.id = f.id;
  m.name = f.name.isEmpty() ? f.category : f.name;
  m.x = f.point.x;
  m.y = f.point.y;

  double r = 16.0;
  if (f.ring.size() >= 3) {
    if (m.x == 0.0 && m.y == 0.0) {
      double sx = 0.0, sy = 0.0;
      for (const Pt& pt : f.ring) {
        sx += pt.x;
        sy += pt.y;
      }
      m.x = sx / double(f.ring.size());
      m.y = sy / double(f.ring.size());
    }
    const double area = polygonAreaAbs(f.ring);
    if (area > 10.0) {
      r = std::sqrt(area / M_PI);
      // 공원 전체 담장(대릉원 등 R > 65m)인 경우 개별 대표 봉분 반경으로 제한한다.
      if (r > 65.0) r = 32.0;
    }
  }
  m.radiusM = std::clamp(r, 8.0, 52.0);
  m.heightM = std::clamp(0.35 * m.radiusM, 4.0, 23.0);
  return m;
}

std::vector<SillaMound> SillaMoundReconstructor::builtInSillaMounds(const QString& workCrs) {
  struct RawMound {
    const char* id;
    const char16_t* name;
    double lat;
    double lon;
    double radiusM;
    double heightM;
    bool twin;
  };
  // 위성 실사 및 OSM·국가유산청 정밀 폴리곤 중심점과 1:1 일치하는 왕릉·고분 좌표 및 봉분 규격
  static constexpr RawMound kCatalog[] = {
      {"mound:hwangnam_n", u"황남대총 북분(98호 북분)", 35.839240, 129.211929, 41.0, 23.0, true},
      {"mound:hwangnam_s", u"황남대총 남분(98호 남분)", 35.838680, 129.211929, 40.5, 22.2, true},
      {"mound:cheonmachong", u"천마총(황남동 155호분)", 35.838574, 129.210495, 26.8, 12.7, false},
      {"mound:michu", u"경주 미추왕릉(대릉원 175호)", 35.837067, 129.213390, 28.0, 12.4, false},
      {"mound:geomchong", u"검총(대릉원 고분)", 35.837456, 129.211932, 16.5, 8.2, false},
      {"mound:daereung_90", u"대릉원 북편 대형분(90호)", 35.839964, 129.213112, 28.0, 11.8, false},
      {"mound:daereung_100", u"대릉원 중앙 대형분", 35.838237, 129.212099, 27.5, 11.5, false},
      {"mound:daereung_110", u"대릉원 서편 대형분", 35.837890, 129.211085, 24.8, 10.5, false},
      {"mound:daereung_s", u"대릉원 남편 대형분", 35.836969, 129.212806, 24.9, 10.6, false},
      {"mound:bonghwangdae", u"봉황대(노동동 125호분)", 35.841289, 129.210629, 39.2, 22.0, false},
      {"mound:singnichong", u"식리총·금령총(노동동 고분군)", 35.840772, 129.210419, 16.0, 7.8, false},
      {"mound:geumgwanchong", u"금관총(노서동 128호분)", 35.841140, 129.209825, 21.0, 9.6, false},
      {"mound:seobongchong", u"서봉총·호우총(노서동 130호)", 35.841454, 129.208288, 32.0, 13.5, true},
      {"mound:noseo_138", u"노서동 138호 대형분", 35.840638, 129.208299, 27.6, 11.8, false},
      {"mound:naemul", u"경주 내물왕릉(교동)", 35.832566, 129.217202, 13.5, 6.8, false},
      {"mound:inwang_w", u"인왕동 서편 대형고분", 35.832720, 129.214949, 34.0, 13.2, false},
      {"mound:jjoksaem_44", u"쪽샘 44호분(황오동 고분군)", 35.840650, 129.214863, 25.0, 10.2, false},
      {"mound:jjoksaem_l17", u"쪽샘 L17호·마갑총 일원", 35.839259, 129.214734, 16.5, 7.0, false},
      {"mound:jjoksaem_b", u"쪽샘 B지구 적석목곽분", 35.840346, 129.215712, 15.0, 6.5, false},
      {"mound:hwangnam_s1", u"황남동 남단 고분군 1호", 35.832260, 129.210405, 19.5, 8.6, false},
      {"mound:hwangnam_s2", u"황남동 남단 고분군 2호", 35.831170, 129.210613, 21.0, 9.2, false},
      {"mound:oreung_main", u"경주 오릉(五陵) 제1릉(박혁거세왕릉)", 35.822450, 129.208250, 24.0, 10.8, false},
      {"mound:oreung_2", u"경주 오릉 제2릉(알영부인릉)", 35.822100, 129.208650, 19.0, 8.5, false},
      {"mound:oreung_3", u"경주 오릉 제3릉(남해차차웅릉)", 35.821820, 129.208180, 18.0, 8.0, false},
      {"mound:oreung_4", u"경주 오릉 제4릉(유리이사금릉)", 35.822650, 129.208780, 17.5, 7.8, false},
      {"mound:oreung_5", u"경주 오릉 제5릉(파사이사금릉)", 35.821600, 129.208720, 16.5, 7.2, false},
      {"mound:muyeol", u"경주 태종무열왕릉(서악동)", 35.825270, 129.187390, 26.5, 12.2, false},
      {"mound:kimyushin", u"경주 김유신장군묘(송화산)", 35.845140, 129.191470, 18.5, 8.5, false},
      {"mound:seondeok", u"경주 선덕여왕릉(낭산)", 35.823480, 129.242390, 23.0, 10.2, false},
      {"mound:nuamri", u"충주 누암리 고분군(국원소경)", 37.018500, 127.854200, 16.0, 6.0, false},
  };

  std::vector<SillaMound> out;
  const CrsTransformer tr(QStringLiteral("EPSG:4326"), workCrs);
  if (!tr.valid()) return out;

  for (const RawMound& rm : kCatalog) {
    double x = rm.lon, y = rm.lat;
    if (!tr.transform(&x, &y)) continue;
    SillaMound m;
    m.id = QString::fromLatin1(rm.id);
    m.name = QString::fromUtf16(rm.name);
    m.x = x;
    m.y = y;
    m.radiusM = rm.radiusM;
    m.heightM = rm.heightM;
    m.isTwinMound = rm.twin;
    out.push_back(std::move(m));
  }
  return out;
}

std::vector<SillaMound> SillaMoundReconstructor::collectMounds(
    const DemGrid& dem,
    const std::vector<GeoFeature>& features,
    bool includeBuiltInCatalog) {
  std::vector<SillaMound> out;
  auto insideDem = [&](double x, double y) {
    float z = 0;
    return dem.sample(x, y, &z);
  };

  if (includeBuiltInCatalog && !dem.crs.isEmpty()) {
    for (SillaMound& m : builtInSillaMounds(dem.crs)) {
      if (insideDem(m.x, m.y)) out.push_back(std::move(m));
    }
  }

  for (const GeoFeature& f : features) {
    if (!isTumulusFeature(f)) continue;
    if (!insideDem(f.point.x, f.point.y)) continue;
    SillaMound cand = moundFromFeature(f);
    bool duplicate = false;
    for (const SillaMound& ex : out) {
      if (std::hypot(ex.x - cand.x, ex.y - cand.y) < std::max(25.0, 0.6 * (ex.radiusM + cand.radiusM))) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate) out.push_back(std::move(cand));
  }
  return out;
}

float SillaMoundReconstructor::sampleMoundHeight(double x, double y, const std::vector<SillaMound>& mounds) {
  float dh = 0.0f;
  for (const SillaMound& m : mounds) {
    const double dx = x - m.x;
    const double dy = y - m.y;
    const double r = m.radiusM;
    if (r <= 1.0 || std::abs(dx) > r || std::abs(dy) > r) continue;
    const double d = std::hypot(dx, dy);
    if (d >= r) continue;
    // Smooth Cosine-Paraboloid Dome: C1 연속 경사 및 봉분 정상부의 둥근 곡률 구현
    const double u = d / r;
    const double cosineDome = 0.5 * (1.0 + std::cos(M_PI * u));
    const double paraboloid = 1.0 - u * u;
    const double shape = 0.65 * cosineDome + 0.35 * paraboloid;
    const float h = float(m.heightM * shape);
    if (h > dh) dh = h;
  }
  return dh;
}

std::shared_ptr<SceneMesh> SillaMoundReconstructor::buildMoundsMesh(
    const DemGrid& dem,
    const std::vector<SillaMound>& mounds) {
  auto mesh = std::make_shared<SceneMesh>();
  if (!dem.valid() || mounds.empty()) return mesh;

  const double cx = dem.centerX(), cy = dem.centerY();
  constexpr int kRings = 10;
  constexpr int kSectors = 28;

  for (const SillaMound& m : mounds) {
    float baseCenterZ = 0.0f;
    if (!dem.sample(m.x, m.y, &baseCenterZ)) continue;

    const uint32_t startIdx = uint32_t(mesh->vertices.size() / SceneMesh::kFloatsPerVertex);

    // 정점 배열: (kRings + 1) × (kSectors + 1)
    for (int ir = 0; ir <= kRings; ++ir) {
      const double u = double(ir) / double(kRings);  // 0(정상) ~ 1(자락)
      const double rad = m.radiusM * u;
      for (int is = 0; is <= kSectors; ++is) {
        const double ang = (2.0 * M_PI * double(is)) / double(kSectors);
        const double wx = m.x + rad * std::cos(ang);
        const double wy = m.y + rad * std::sin(ang);
        float groundZ = baseCenterZ;
        dem.sample(wx, wy, &groundZ);

        // 쌍분 능선까지 반영하기 위해 전체 고분장 높이 함수 사용
        const float domeH = (ir == kRings) ? -0.25f : sampleMoundHeight(wx, wy, mounds);
        const float wz = groundZ + domeH;

        // 수치 미분으로 부드러운 법선 계산
        const double eps = std::max(1.0, m.radiusM * 0.08);
        const float hx1 = sampleMoundHeight(wx + eps, wy, mounds);
        const float hx0 = sampleMoundHeight(wx - eps, wy, mounds);
        const float hy1 = sampleMoundHeight(wx, wy + eps, mounds);
        const float hy0 = sampleMoundHeight(wx, wy - eps, mounds);
        QVector3D normal(float(-(hx1 - hx0) / (2.0 * eps)),
                         float(-(hy1 - hy0) / (2.0 * eps)),
                         1.0f);
        normal.normalize();

        // 봉분 색: 자락 끝단(u >= 0.92)은 화강암 호석(둘레돌) 테두리, 상부는 자연스러운 신라 왕릉 잔디 그라데이션
        const float crest = float(1.0 - u);
        const Color col = (ir == kRings)
                              ? Color{0.72f, 0.70f, 0.65f, 1.0f}
                              : Color{0.34f + 0.24f * crest,
                                      0.50f + 0.22f * crest,
                                      0.20f + 0.12f * crest,
                                      1.0f};

        appendVertex(mesh.get(), QVector3D(float(wx - cx), float(wy - cy), wz), normal, col);
      }
    }

    const int stride = kSectors + 1;
    for (int ir = 0; ir < kRings; ++ir) {
      for (int is = 0; is < kSectors; ++is) {
        const uint32_t i00 = startIdx + uint32_t(ir * stride + is);
        const uint32_t i10 = startIdx + uint32_t((ir + 1) * stride + is);
        const uint32_t i11 = startIdx + uint32_t((ir + 1) * stride + is + 1);
        const uint32_t i01 = startIdx + uint32_t(ir * stride + is + 1);
        appendTri(mesh.get(), i00, i10, i11);
        appendTri(mesh.get(), i00, i11, i01);
      }
    }
    ++mesh->heritageAreas;
  }
  return mesh;
}

std::shared_ptr<const SceneMesh> SillaMoundReconstructor::buildBareEarthAndMoundScene(
    const DemGrid& dem,
    const std::vector<GeoFeature>& features,
    bool bareEarthMode,
    bool reconstructMounds,
    std::vector<SillaMound>* outMounds) {
  std::vector<SillaMound> mounds;
  if (reconstructMounds) {
    mounds = collectMounds(dem, features, true);
  }
  if (outMounds) *outMounds = mounds;

  if (!bareEarthMode) {
    auto base = std::make_shared<SceneMesh>(*buildSceneMesh(dem, features));
    if (reconstructMounds && !mounds.empty()) {
      auto moundMesh = buildMoundsMesh(dem, mounds);
      appendMesh(base.get(), *moundMesh);
    }
    return base;
  }

  // Bare-Earth 모드: 현대 회색 건물 박스를 제거(또는 0.25m 바닥 흔적선만 표시)하고 유산·고분만 부각한다.
  std::vector<GeoFeature> filtered;
  filtered.reserve(features.size());
  for (const GeoFeature& f : features) {
    if (f.kind == FeatureKind::Building && !f.heritage && !isTumulusFeature(f)) {
      continue;  // 현대 일반 건물은 고지형 모드에서 숨겨 선상지·구하도·방리 격자를 선명히 드러낸다.
    }
    filtered.push_back(f);
  }

  auto mesh = std::make_shared<SceneMesh>(*buildSceneMesh(dem, filtered));
  if (reconstructMounds && !mounds.empty()) {
    auto moundMesh = buildMoundsMesh(dem, mounds);
    appendMesh(mesh.get(), *moundMesh);
  }
  return mesh;
}

}  // namespace strata3d
