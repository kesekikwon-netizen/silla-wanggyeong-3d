#include "DemProvider.h"

#include <cpl_conv.h>
#include <cpl_string.h>
#include <gdal_priv.h>
#include <gdal_utils.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <mutex>

#include "CrsTransform.h"

namespace strata3d {

namespace {

constexpr float kNoData = -9999.0f;

void initGdal() {
  static std::once_flag once;
  std::call_once(once, [] {
    GDALAllRegister();
    // /vsicurl/ 이 폴더 목록을 뒤지지 않고 .tif 만 바로 열게 한다.
    CPLSetConfigOption("GDAL_DISABLE_READDIR_ON_OPEN", "EMPTY_DIR");
    CPLSetConfigOption("CPL_VSIL_CURL_ALLOWED_EXTENSIONS", ".tif");
    CPLSetConfigOption("GDAL_HTTP_MAX_RETRY", "3");
    CPLSetConfigOption("GDAL_HTTP_RETRY_DELAY", "1");
  });
}

QString cacheFileFor(const DemRequest& req, const QStringList& sources) {
  QCryptographicHash h(QCryptographicHash::Sha256);
  const QString key = QStringLiteral("%1|%2|%3|%4|%5|%6")
                          .arg(req.workCrs)
                          .arg(req.centerX, 0, 'f', 1)
                          .arg(req.centerY, 0, 'f', 1)
                          .arg(req.halfExtentM, 0, 'f', 1)
                          .arg(req.pixelSizeM, 0, 'f', 3)
                          .arg(sources.join(QLatin1Char(';')));
  h.addData(key.toUtf8());
  return QDir(req.cacheDir).filePath(QStringLiteral("dem_%1.tif").arg(QString::fromLatin1(h.result().toHex().left(24))));
}

bool readGrid(GDALDataset* ds, const DemRequest& req, const QString& source, DemGrid* out,
              QString* errorOut) {
  double gt[6];
  if (ds->GetGeoTransform(gt) != CE_None || gt[2] != 0 || gt[4] != 0) {
    if (errorOut) *errorOut = QStringLiteral("DEM 격자 변환이 회전되었거나 없습니다.");
    return false;
  }
  DemGrid g;
  g.width = ds->GetRasterXSize();
  g.height = ds->GetRasterYSize();
  g.originX = gt[0];
  g.originY = gt[3];
  g.pixelSize = gt[1];
  g.crs = req.workCrs;
  g.source = source;
  g.z.resize(size_t(g.width) * g.height);
  if (ds->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, g.width, g.height, g.z.data(), g.width,
                                     g.height, GDT_Float32, 0, 0, nullptr) != CE_None) {
    if (errorOut) *errorOut = QStringLiteral("DEM 래스터를 읽지 못했습니다.");
    return false;
  }
  float lo = std::numeric_limits<float>::max();
  float hi = std::numeric_limits<float>::lowest();
  for (float v : g.z) {
    if (v == kNoData || !std::isfinite(v)) continue;
    lo = std::min(lo, v);
    hi = std::max(hi, v);
  }
  if (lo > hi) {
    if (errorOut) *errorOut = QStringLiteral("요청한 창에 유효한 표고가 없습니다.");
    return false;
  }
  for (float& v : g.z) {
    if (v == kNoData || !std::isfinite(v)) {
      v = lo;
      ++g.noDataCount;
    }
  }
  g.zMin = lo;
  g.zMax = hi;
  *out = std::move(g);
  return true;
}

}  // namespace

QStringList copernicusSourcesFor(const DemRequest& req, QString* errorOut) {
  double lonMin = 1e9, lonMax = -1e9, latMin = 1e9, latMax = -1e9;
  constexpr int kSteps = 8;
  for (int i = 0; i <= kSteps; ++i) {
    for (int j = 0; j <= kSteps; ++j) {
      if (i != 0 && i != kSteps && j != 0 && j != kSteps) continue;  // 가장자리만
      double x = req.centerX - req.halfExtentM + 2 * req.halfExtentM * i / kSteps;
      double y = req.centerY - req.halfExtentM + 2 * req.halfExtentM * j / kSteps;
      if (!transformPoint(req.workCrs, QStringLiteral("EPSG:4326"), &x, &y, errorOut)) return {};
      lonMin = std::min(lonMin, x);
      lonMax = std::max(lonMax, x);
      latMin = std::min(latMin, y);
      latMax = std::max(latMax, y);
    }
  }
  QStringList out;
  for (int lat = int(std::floor(latMin)); lat <= int(std::floor(latMax)); ++lat) {
    for (int lon = int(std::floor(lonMin)); lon <= int(std::floor(lonMax)); ++lon) {
      const QString name = QStringLiteral("Copernicus_DSM_COG_10_%1%2_00_%3%4_00_DEM")
                               .arg(lat >= 0 ? QLatin1Char('N') : QLatin1Char('S'))
                               .arg(std::abs(lat), 2, 10, QLatin1Char('0'))
                               .arg(lon >= 0 ? QLatin1Char('E') : QLatin1Char('W'))
                               .arg(std::abs(lon), 3, 10, QLatin1Char('0'));
      out << QStringLiteral("/vsicurl/https://copernicus-dem-30m.s3.amazonaws.com/%1/%1.tif").arg(name);
    }
  }
  return out;
}

bool loadDemWindow(const DemRequest& req, DemGrid* out, QString* errorOut) {
  initGdal();
  QStringList sources = req.sources;
  if (sources.isEmpty()) {
    sources = copernicusSourcesFor(req, errorOut);
    if (sources.isEmpty()) return false;
  }
  const QString joined = sources.join(QStringLiteral(" + "));

  QString cachePath;
  if (!req.cacheDir.isEmpty()) {
    QDir().mkpath(req.cacheDir);
    cachePath = cacheFileFor(req, sources);
    if (QFile::exists(cachePath)) {
      std::unique_ptr<GDALDataset> cached(GDALDataset::Open(
          cachePath.toUtf8().constData(), GDAL_OF_RASTER | GDAL_OF_READONLY));
      if (cached && readGrid(cached.get(), req, joined, out, nullptr)) return true;
    }
  }

  std::vector<GDALDatasetH> opened;
  for (const QString& s : sources) {
    GDALDatasetH h = GDALOpenEx(s.toUtf8().constData(), GDAL_OF_RASTER | GDAL_OF_READONLY, nullptr, nullptr, nullptr);
    if (h) opened.push_back(h);
  }
  if (opened.empty()) {
    if (errorOut) *errorOut = QStringLiteral("DEM 원본을 열지 못했습니다(네트워크 또는 경로 확인): %1").arg(joined);
    return false;
  }

  const double xMin = req.centerX - req.halfExtentM, xMax = req.centerX + req.halfExtentM;
  const double yMin = req.centerY - req.halfExtentM, yMax = req.centerY + req.halfExtentM;
  const QByteArray ps = QByteArray::number(req.pixelSizeM, 'f', 3);
  CPLStringList args;
  args.AddString("-t_srs");
  args.AddString(req.workCrs.toUtf8().constData());
  args.AddString("-te");
  for (double v : {xMin, yMin, xMax, yMax}) args.AddString(QByteArray::number(v, 'f', 3).constData());
  args.AddString("-tr");
  args.AddString(ps.constData());
  args.AddString(ps.constData());
  args.AddString("-r");
  args.AddString("bilinear");
  args.AddString("-ot");
  args.AddString("Float32");
  args.AddString("-dstnodata");
  args.AddString("-9999");
  args.AddString("-of");
  args.AddString(cachePath.isEmpty() ? "MEM" : "GTiff");
  if (!cachePath.isEmpty()) {
    for (const char* co : {"COMPRESS=DEFLATE", "TILED=YES"}) {
      args.AddString("-co");
      args.AddString(co);
    }
  }
  GDALWarpAppOptions* opts = GDALWarpAppOptionsNew(args.List(), nullptr);
  int usageError = 0;
  const QByteArray dest = cachePath.toUtf8();
  GDALDatasetH warped = GDALWarp(cachePath.isEmpty() ? "" : dest.constData(), nullptr, int(opened.size()),
                                 opened.data(), opts, &usageError);
  GDALWarpAppOptionsFree(opts);
  for (GDALDatasetH h : opened) GDALClose(h);
  if (!warped) {
    if (errorOut) *errorOut = QStringLiteral("DEM 재배열(GDALWarp)에 실패했습니다: %1").arg(joined);
    return false;
  }
  std::unique_ptr<GDALDataset> ds(GDALDataset::FromHandle(warped));
  return readGrid(ds.get(), req, joined, out, errorOut);
}

}  // namespace strata3d
