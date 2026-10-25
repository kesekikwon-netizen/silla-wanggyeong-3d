#include "EvidenceTier.h"

#include <algorithm>

namespace strata3d {

QChar tierLetter(EvidenceTier t) {
  return QChar(u'A' + int(t));
}

QString tierLabel(EvidenceTier t) {
  switch (t) {
    case EvidenceTier::Measured: return QStringLiteral("실측");
    case EvidenceTier::Documented: return QStringLiteral("근거");
    case EvidenceTier::Inferred: return QStringLiteral("추정");
    case EvidenceTier::Schematic: return QStringLiteral("도식");
  }
  return {};
}

TierStyle defaultTierStyle(EvidenceTier t) {
  TierStyle s;
  switch (t) {
    case EvidenceTier::Measured:
    case EvidenceTier::Documented: break;
    case EvidenceTier::Inferred: s.desaturate = 0.45f; s.fade = 0.20f; break;
    case EvidenceTier::Schematic: s.desaturate = 0.90f; s.fade = 0.40f; break;
  }
  return s;
}

void applyTierStyle(SceneMesh* mesh, std::size_t firstVertex, std::size_t endVertex, const TierStyle& style) {
  if (!mesh || (style.desaturate <= 0.0f && style.fade <= 0.0f)) return;
  constexpr int kColor = 6;  // 정점 = 위치 3, 법선 3, 색 4
  const std::size_t n = mesh->vertices.size() / SceneMesh::kFloatsPerVertex;
  endVertex = std::min(endVertex, n);
  for (std::size_t v = firstVertex; v < endVertex; ++v) {
    float* c = &mesh->vertices[v * SceneMesh::kFloatsPerVertex + kColor];
    const float luma = 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
    for (int i = 0; i < 3; ++i) {
      const float grey = c[i] + (luma - c[i]) * style.desaturate;
      c[i] = grey + (style.paper[i] - grey) * style.fade;
    }
  }
}

}  // namespace strata3d
