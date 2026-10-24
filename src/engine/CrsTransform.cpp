#include "CrsTransform.h"

#include <ogr_spatialref.h>

namespace strata3d {

namespace {
bool importCrs(OGRSpatialReference* srs, const QString& text) {
  srs->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
  return srs->SetFromUserInput(text.toUtf8().constData()) == OGRERR_NONE;
}
}  // namespace

struct CrsTransformer::Impl {
  OGRSpatialReference from;
  OGRSpatialReference to;
  std::unique_ptr<OGRCoordinateTransformation> ct;
};

CrsTransformer::CrsTransformer(const QString& fromCrs, const QString& toCrs) : impl_(std::make_unique<Impl>()) {
  if (importCrs(&impl_->from, fromCrs) && importCrs(&impl_->to, toCrs))
    impl_->ct.reset(OGRCreateCoordinateTransformation(&impl_->from, &impl_->to));
}

CrsTransformer::~CrsTransformer() = default;

bool CrsTransformer::valid() const { return bool(impl_->ct); }

bool CrsTransformer::transform(double* x, double* y) const { return impl_->ct && impl_->ct->Transform(1, x, y); }

bool transformPoint(const QString& fromCrs, const QString& toCrs, double* x, double* y, QString* errorOut) {
  const CrsTransformer t(fromCrs, toCrs);
  if (!t.valid()) {
    if (errorOut) *errorOut = QStringLiteral("좌표계를 해석하지 못했습니다: %1 → %2").arg(fromCrs, toCrs);
    return false;
  }
  if (!t.transform(x, y)) {
    if (errorOut) *errorOut = QStringLiteral("좌표 변환에 실패했습니다: %1 → %2").arg(fromCrs, toCrs);
    return false;
  }
  return true;
}

}  // namespace strata3d
