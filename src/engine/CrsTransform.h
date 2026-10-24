#pragma once

#include <QString>
#include <memory>

namespace strata3d {

// GDAL/PROJ(OSR) 로 점 하나를 옮긴다. 축 순서는 항상 (경도|X, 위도|Y). 직접 투영식을 쓰지 않는다.
// crs 는 "EPSG:5187" 같은 문자열. 실패하면 false 와 사유.
bool transformPoint(const QString& fromCrs, const QString& toCrs, double* x, double* y,
                    QString* errorOut = nullptr);

// 여러 점을 옮길 때 변환 객체를 한 번만 만든다.
class CrsTransformer {
 public:
  CrsTransformer(const QString& fromCrs, const QString& toCrs);
  ~CrsTransformer();
  bool valid() const;
  bool transform(double* x, double* y) const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace strata3d
