// 아이콘 id → SVG 표. Strata(hgis) 의 KaIconsMockupMap.cpp 를 고치지 않고 그대로 컴파일해 그 표를 먼저 찾고,
// 없을 때만 3D 전용 단추의 SVG(data/icons/strata3d-icons.qrc)를 찾는다. 같은 id 는 Strata 와 늘 같은 그림이다.
#include <QHash>
#include <QString>

// Strata 의 표 함수 이름만 바꿔 들여온다(그 파일 안의 kEntries·svgPathFor 가 그대로 컴파일된다).
#define svgPathFor strataSvgPathFor
#include "KaIconsMockupMap.cpp"
#undef svgPathFor

namespace KaIconsMockup {

QString svgPathFor(const QString& id);

namespace {
struct S3dEntry {
  const char* id;
  const char* svg;  // ":/strata-3d/icons/" 아래
};
constexpr S3dEntry kS3dEntries[] = {
    // 3D 보기
    {"view3d", "lucide/rotate-3d"},
    {"zscale", "lucide/move-vertical"},
    {"labels", "lucide/tag"},
    {"tiers", "lucide/badge-check"},
    {"subsurface", "lucide/layers-2"},
    // 3D 분석
    {"zones", "lucide/shield-check"},
    {"relations", "lucide/table-2"},
    {"depth", "lucide/arrow-down-to-line"},
    {"inventory", "lucide/list-checks"},
};
}  // namespace

QString svgPathFor(const QString& id) {
  const QString strata = strataSvgPathFor(id);
  if (!strata.isEmpty()) return strata;
  static const QHash<QString, QString> own = [] {
    QHash<QString, QString> table;
    for (const S3dEntry& e : kS3dEntries)
      table.insert(QString::fromLatin1(e.id), QStringLiteral(":/strata-3d/icons/") + QString::fromLatin1(e.svg) + QStringLiteral(".svg"));
    return table;
  }();
  return own.value(id);
}

}  // namespace KaIconsMockup
