#include "CrsTransform.h"

#include <ogr_spatialref.h>

#include <memory>

namespace strata3d {

namespace {
bool importCrs(OGRSpatialReference* srs, const QString& text) {
  srs->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
  return srs->SetFromUserInput(text.toUtf8().constData()) == OGRERR_NONE;
}
}  // namespace

bool transformPoint(const QString& fromCrs, const QString& toCrs, double* x, double* y,
                    QString* errorOut) {
  OGRSpatialReference from;
  OGRSpatialReference to;
  if (!importCrs(&from, fromCrs) || !importCrs(&to, toCrs)) {
    if (errorOut) *errorOut = QStringLiteral("좌표계를 해석하지 못했습니다: %1 → %2").arg(fromCrs, toCrs);
    return false;
  }
  std::unique_ptr<OGRCoordinateTransformation> ct(OGRCreateCoordinateTransformation(&from, &to));
  if (!ct || !ct->Transform(1, x, y)) {
    if (errorOut) *errorOut = QStringLiteral("좌표 변환에 실패했습니다: %1 → %2").arg(fromCrs, toCrs);
    return false;
  }
  return true;
}

}  // namespace strata3d
