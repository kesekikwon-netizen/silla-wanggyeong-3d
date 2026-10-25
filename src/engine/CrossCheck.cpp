#include "CrossCheck.h"

#include <cmath>

#include "CrsTransform.h"

namespace strata3d {

PositionCheck comparePositions(const QString& sourceA, double ax, double ay, const QString& crsA, const QString& sourceB,
                               double bx, double by, const QString& crsB, const CrossCheckThresholds& t) {
  PositionCheck c;
  c.sourceA = sourceA;
  c.sourceB = sourceB;
  const QString utmk = QStringLiteral("EPSG:5179");
  if (!transformPoint(crsA, utmk, &ax, &ay, &c.error) || !transformPoint(crsB, utmk, &bx, &by, &c.error)) {
    c.level = Agreement::SingleSource;
    return c;
  }
  c.distanceM = std::hypot(ax - bx, ay - by);
  c.level = c.distanceM <= t.matchM ? Agreement::Match : c.distanceM <= t.nearM ? Agreement::Near : Agreement::Mismatch;
  return c;
}

QString agreementLabel(Agreement a) {
  switch (a) {
    case Agreement::Match: return QStringLiteral("일치");
    case Agreement::Near: return QStringLiteral("근접");
    case Agreement::Mismatch: return QStringLiteral("확인 필요");
    case Agreement::SingleSource: return QStringLiteral("단일 출처");
  }
  return {};
}

}  // namespace strata3d
