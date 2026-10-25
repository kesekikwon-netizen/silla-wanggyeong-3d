#include "SceneMesh.h"

#include <QVector3D>
#include <algorithm>
#include <array>
#include <cmath>

#include "Triangulate.h"

namespace strata3d {

namespace {

using Color = std::array<float, 4>;

class MeshWriter {
 public:
  explicit MeshWriter(SceneMesh* m) : m_(m) {}
  uint32_t vertex(const QVector3D& p, const QVector3D& n, const Color& c) {
    const uint32_t i = uint32_t(m_->vertices.size() / SceneMesh::kFloatsPerVertex);
    m_->vertices.insert(m_->vertices.end(), {p.x(), p.y(), p.z(), n.x(), n.y(), n.z(), c[0], c[1], c[2], c[3]});
    return i;
  }
  void tri(uint32_t a, uint32_t b, uint32_t c) { m_->indices.insert(m_->indices.end(), {a, b, c}); }
  void quad(const QVector3D& a, const QVector3D& b, const QVector3D& c, const QVector3D& d, const Color& col) {
    const QVector3D n = QVector3D::normal(a, b, c);
    const uint32_t ia = vertex(a, n, col), ib = vertex(b, n, col), ic = vertex(c, n, col), id = vertex(d, n, col);
    tri(ia, ib, ic);
    tri(ia, ic, id);
  }

 private:
  SceneMesh* m_;
};

Color buildingColor(const GeoFeature& f) {
  if (f.heritage) return {0.86f, 0.52f, 0.32f, 1};
  if (f.heightSource == HeightSource::Unknown) return {0.90f, 0.88f, 0.84f, 1};  // 높이 미상: 옅게
  return {0.78f, 0.74f, 0.70f, 1};
}

Color markerColor(const QString& category) {
  if (category.contains(QString::fromUtf16(u"국보")) || category.contains(QString::fromUtf16(u"보물"))) return {0.85f, 0.12f, 0.12f, 1};
  if (category.contains(QString::fromUtf16(u"사적"))) return {0.95f, 0.45f, 0.08f, 1};
  return {0.92f, 0.74f, 0.15f, 1};
}

}  // namespace

std::shared_ptr<const SceneMesh> buildSceneMesh(const DemGrid& dem, const std::vector<GeoFeature>& features,
                                                const SceneStyle& style) {
  auto mesh = std::make_shared<SceneMesh>();
  MeshWriter w(mesh.get());
  const double cx = dem.centerX(), cy = dem.centerY();
  auto local = [&](double X, double Y, float z) { return QVector3D(float(X - cx), float(Y - cy), z); };

  for (const GeoFeature& f : features) {
    if (f.kind == FeatureKind::Building) {
      std::vector<Pt> ring = f.ring;
      if (ring.size() < 3) { ++mesh->skipped; continue; }
      if (signedArea(ring) < 0) std::reverse(ring.begin(), ring.end());
      std::vector<float> ground(ring.size());
      bool inside = true;
      float base = 1e9f;
      for (size_t i = 0; i < ring.size(); ++i) {
        if (!dem.sample(ring[i].x, ring[i].y, &ground[i])) { inside = false; break; }
        base = std::min(base, ground[i]);
      }
      if (!inside) { ++mesh->skipped; continue; }
      if (style.footprintOnly) {
        if (!appendDrapedRibbon(mesh.get(), dem, ring, true, style.footprintWidthM, {0.36f, 0.36f, 0.35f, 1}, 0.4f)) {
          ++mesh->skipped;
          continue;
        }
        ++mesh->buildings;
        if (f.heightSource == HeightSource::Measured) ++mesh->heightMeasured;
        else if (f.heightSource == HeightSource::FromLevels) ++mesh->heightFromLevels;
        else ++mesh->heightUnknown;
        continue;
      }
      const float height = f.heightSource == HeightSource::Unknown ? style.unknownHeightM : float(f.heightM);
      const float top = base + std::max(height, 2.0f);
      const Color col = buildingColor(f);
      std::vector<uint32_t> tris;
      if (!triangulateRing(ring, &tris)) ++mesh->triangulationFailures;
      if (tris.empty()) { ++mesh->skipped; continue; }
      std::vector<uint32_t> roofIdx(ring.size());
      for (size_t i = 0; i < ring.size(); ++i) roofIdx[i] = w.vertex(local(ring[i].x, ring[i].y, top), QVector3D(0, 0, 1), col);
      for (size_t t = 0; t + 2 < tris.size(); t += 3) w.tri(roofIdx[tris[t]], roofIdx[tris[t + 1]], roofIdx[tris[t + 2]]);
      for (size_t i = 0; i < ring.size(); ++i) {
        const size_t j = (i + 1) % ring.size();
        w.quad(local(ring[i].x, ring[i].y, ground[i] - 0.3f), local(ring[j].x, ring[j].y, ground[j] - 0.3f),
               local(ring[j].x, ring[j].y, top), local(ring[i].x, ring[i].y, top), col);
      }
      ++mesh->buildings;
      if (f.heightSource == HeightSource::Measured) ++mesh->heightMeasured;
      else if (f.heightSource == HeightSource::FromLevels) ++mesh->heightFromLevels;
      else ++mesh->heightUnknown;
    } else if (f.kind == FeatureKind::HeritageArea) {
      if (style.drapeAreas) {
        ++mesh->heritageAreas;
        continue;
      }
      if (!appendDrapedRibbon(mesh.get(), dem, f.ring, true, style.ribbonWidthM, {0.90f, 0.20f, 0.18f, 1}, 1.0f)) {
        ++mesh->skipped;
        continue;
      }
      ++mesh->heritageAreas;
    } else {  // HeritagePoint
      if (!appendPole(mesh.get(), dem, f.point, style.markerWidthM, style.markerHeightM, markerColor(f.category))) {
        ++mesh->skipped;
        continue;
      }
      ++mesh->heritagePoints;
    }
  }
  return mesh;
}

bool appendDrapedRibbon(SceneMesh* mesh, const DemGrid& dem, const std::vector<Pt>& pts, bool closed, float widthM,
                        const std::array<float, 4>& color, float liftM) {
  // 5 m 간격으로 잘게 나눠 지형 표고를 따라가게 한다.
  const double cx = dem.centerX(), cy = dem.centerY();
  std::vector<QVector3D> line;
  const size_t segs = closed ? pts.size() : (pts.empty() ? 0 : pts.size() - 1);
  for (size_t i = 0; i < segs; ++i) {
    const Pt& a = pts[i];
    const Pt& b = pts[(i + 1) % pts.size()];
    const int steps = std::max(1, int(std::hypot(b.x - a.x, b.y - a.y) / 5.0));
    for (int s = 0; s < steps; ++s) {
      const double X = a.x + (b.x - a.x) * s / steps, Y = a.y + (b.y - a.y) * s / steps;
      float z = 0;
      if (!dem.sample(X, Y, &z)) return false;
      line.push_back(QVector3D(float(X - cx), float(Y - cy), z + liftM));
    }
  }
  if (!closed && !pts.empty()) {
    float z = 0;
    if (!dem.sample(pts.back().x, pts.back().y, &z)) return false;
    line.push_back(QVector3D(float(pts.back().x - cx), float(pts.back().y - cy), z + liftM));
  }
  if (line.size() < 2) return false;
  MeshWriter w(mesh);
  const float hw = widthM * 0.5f;
  const size_t n = closed ? line.size() : line.size() - 1;
  for (size_t i = 0; i < n; ++i) {
    const QVector3D &p = line[i], &q = line[(i + 1) % line.size()];
    QVector3D d = q - p;
    d.setZ(0);
    if (d.length() < 1e-4f) continue;
    d.normalize();
    const QVector3D side(-d.y() * hw, d.x() * hw, 0);
    w.quad(p - side, q - side, q + side, p + side, Color(color));
  }
  return true;
}

bool appendPole(SceneMesh* mesh, const DemGrid& dem, const Pt& at, float widthM, float heightM, const std::array<float, 4>& color) {
  float z = 0;
  if (!dem.sample(at.x, at.y, &z)) return false;
  MeshWriter w(mesh);
  const float h = widthM * 0.5f;
  const QVector3D c(float(at.x - dem.centerX()), float(at.y - dem.centerY()), z);
  const float z0 = z - 2.0f, z1 = z + heightM;
  const QVector3D p00(c.x() - h, c.y() - h, z0), p10(c.x() + h, c.y() - h, z0), p11(c.x() + h, c.y() + h, z0), p01(c.x() - h, c.y() + h, z0);
  auto up = [&](const QVector3D& p) { return QVector3D(p.x(), p.y(), z1); };
  const Color col(color);
  w.quad(p00, p10, up(p10), up(p00), col);
  w.quad(p10, p11, up(p11), up(p10), col);
  w.quad(p11, p01, up(p01), up(p11), col);
  w.quad(p01, p00, up(p00), up(p01), col);
  w.quad(up(p00), up(p10), up(p11), up(p01), col);
  return true;
}

}  // namespace strata3d
