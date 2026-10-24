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
