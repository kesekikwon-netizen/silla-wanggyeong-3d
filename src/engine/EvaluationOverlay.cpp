#include "EvaluationOverlay.h"

namespace strata3d {

std::shared_ptr<const SceneMesh> buildEvaluationOverlay(const DemGrid& dem, const Evaluation& ev) {
  auto mesh = std::make_shared<SceneMesh>();
  const std::array<std::array<float, 4>, 2> ringColors = {{{0.10f, 0.45f, 0.95f, 1}, {0.55f, 0.25f, 0.85f, 1}}};
  for (size_t i = 0; i < ev.radii.size(); ++i)
    appendDrapedRibbon(mesh.get(), dem, ev.radii[i].bufferRing, true, 6.0f, ringColors[std::min<size_t>(i, 1)], 2.5f);
  if (ev.sitePolygon.size() >= 3) appendDrapedRibbon(mesh.get(), dem, ev.sitePolygon, true, 4.0f, {0.15f, 0.75f, 0.30f, 1}, 3.0f);
  appendPole(mesh.get(), dem, ev.site, 4.0f, 90.0f, {0.15f, 0.75f, 0.30f, 1});
  return mesh;
}

std::shared_ptr<const SceneMesh> buildProfileOverlay(const DemGrid& dem, const std::vector<ProfilePoint>& profile) {
  auto mesh = std::make_shared<SceneMesh>();
  std::vector<Pt> line;
  for (const ProfilePoint& p : profile) line.push_back({p.x, p.y});
  appendDrapedRibbon(mesh.get(), dem, line, false, 5.0f, {1.0f, 0.55f, 0.05f, 1}, 3.5f);
  return mesh;
}

}  // namespace strata3d

namespace strata3d {

void appendMesh(SceneMesh* dst, const SceneMesh& src) {
  const uint32_t base = uint32_t(dst->vertices.size() / SceneMesh::kFloatsPerVertex);
  dst->vertices.insert(dst->vertices.end(), src.vertices.begin(), src.vertices.end());
  for (uint32_t i : src.indices) dst->indices.push_back(base + i);
}

std::shared_ptr<const SceneMesh> buildSurveyOverlay(const DemGrid& dem, const StrataSurvey& s) {
  auto mesh = std::make_shared<SceneMesh>();
  const std::array<float, 4> green{0.15f, 0.75f, 0.30f, 1}, orange{1.0f, 0.55f, 0.05f, 1}, red{0.90f, 0.20f, 0.18f, 1}, teal{0.05f, 0.65f, 0.70f, 1};
  for (const auto& r : s.surveyAreas) appendDrapedRibbon(mesh.get(), dem, r, true, 5.0f, green, 3.0f);
  for (const auto& r : s.trenches) appendDrapedRibbon(mesh.get(), dem, r, r.size() >= 3, 4.0f, orange, 3.5f);
  for (const auto& r : s.featurePolys) appendDrapedRibbon(mesh.get(), dem, r, true, 3.0f, red, 3.2f);
  for (const auto& l : s.featureLines) appendDrapedRibbon(mesh.get(), dem, l, false, 3.0f, teal, 3.2f);
  for (const auto& l : s.sectionLines) appendDrapedRibbon(mesh.get(), dem, l, false, 3.0f, teal, 3.4f);
  for (const Pt& p : s.controlPoints) appendPole(mesh.get(), dem, p, 2.0f, 25.0f, {0.2f, 0.2f, 0.9f, 1});
  for (const Pt& p : s.artifactPoints) appendPole(mesh.get(), dem, p, 2.0f, 18.0f, {0.8f, 0.2f, 0.8f, 1});
  return mesh;
}

}  // namespace strata3d
