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
      // 경계를 지형에 붙여 깔리는 띠로 그린다(5 m 간격으로 잘게 나눔).
      std::vector<QVector3D> line;
      bool inside = true;
      for (size_t i = 0; i < f.ring.size() && inside; ++i) {
        const Pt& a = f.ring[i];
        const Pt& b = f.ring[(i + 1) % f.ring.size()];
        const int steps = std::max(1, int(std::hypot(b.x - a.x, b.y - a.y) / 5.0));
        for (int s = 0; s < steps; ++s) {
          const double X = a.x + (b.x - a.x) * s / steps, Y = a.y + (b.y - a.y) * s / steps;
          float z = 0;
          if (!dem.sample(X, Y, &z)) { inside = false; break; }
          line.push_back(local(X, Y, z + 1.0f));
        }
      }
      if (!inside || line.size() < 3) { ++mesh->skipped; continue; }
      const Color col = {0.90f, 0.20f, 0.18f, 1};
      const float hw = style.ribbonWidthM * 0.5f;
      for (size_t i = 0; i < line.size(); ++i) {
        const QVector3D &p = line[i], &q = line[(i + 1) % line.size()];
        QVector3D d = q - p;
        d.setZ(0);
        if (d.length() < 1e-4f) continue;
        d.normalize();
        const QVector3D side(-d.y() * hw, d.x() * hw, 0);
        w.quad(p - side, q - side, q + side, p + side, col);
      }
      ++mesh->heritageAreas;
    } else {  // HeritagePoint
      float z = 0;
      if (!dem.sample(f.point.x, f.point.y, &z)) { ++mesh->skipped; continue; }
      const float h = style.markerWidthM * 0.5f;
      const QVector3D c = local(f.point.x, f.point.y, z);
      const Color col = markerColor(f.category);
      const float z0 = z - 2.0f, z1 = z + style.markerHeightM;
      const QVector3D p00(c.x() - h, c.y() - h, z0), p10(c.x() + h, c.y() - h, z0), p11(c.x() + h, c.y() + h, z0), p01(c.x() - h, c.y() + h, z0);
      auto up = [&](const QVector3D& p) { return QVector3D(p.x(), p.y(), z1); };
      w.quad(p00, p10, up(p10), up(p00), col);
      w.quad(p10, p11, up(p11), up(p10), col);
      w.quad(p11, p01, up(p01), up(p11), col);
      w.quad(p01, p00, up(p00), up(p01), col);
      w.quad(up(p00), up(p10), up(p11), up(p01), col);
      ++mesh->heritagePoints;
    }
  }
  return mesh;
}

}  // namespace strata3d
