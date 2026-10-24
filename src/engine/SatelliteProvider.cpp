#include "SatelliteProvider.h"

#include <cpl_string.h>
#include <gdal_priv.h>
#include <gdal_utils.h>

#include <QDir>
#include <cmath>
#include <memory>

#include "VworldKey.h"

namespace strata3d {

namespace {

QString wmsXml(const QString& serverUrl, const QString& cachePath) {
  // 웹 메르카토르 전 세계, 타일 단계 20까지. ${x},${y},${z} 는 GDAL 이 채운다(위가 원점).
  QString xml = QStringLiteral(
      "<GDAL_WMS>"
      "<Service name=\"TMS\"><ServerUrl>%1</ServerUrl></Service>"
      "<DataWindow><UpperLeftX>-20037508.34</UpperLeftX><UpperLeftY>20037508.34</UpperLeftY>"
      "<LowerRightX>20037508.34</LowerRightX><LowerRightY>-20037508.34</LowerRightY>"
      "<TileLevel>20</TileLevel><TileCountX>1</TileCountX><TileCountY>1</TileCountY><YOrigin>top</YOrigin></DataWindow>"
      "<Projection>EPSG:3857</Projection><BlockSizeX>256</BlockSizeX><BlockSizeY>256</BlockSizeY><BandsCount>3</BandsCount>"
      "<MaxConnections>12</MaxConnections><UserAgent>strata-3d/0.1 (archaeology GIS desktop app)</UserAgent>"
      "<ZeroBlockHttpCodes>204,404</ZeroBlockHttpCodes><ZeroBlockOnServerException>true</ZeroBlockOnServerException>")
      .arg(serverUrl.toHtmlEscaped());
  if (!cachePath.isEmpty()) xml += QStringLiteral("<Cache><Path>%1</Path><Depth>2</Depth></Cache>").arg(cachePath.toHtmlEscaped());
  xml += QStringLiteral("</GDAL_WMS>");
  return xml;
}

}  // namespace

bool loadSatelliteImage(const SatelliteRequest& req, SatelliteResult* out, QString* errorOut) {
  GDALAllRegister();
  SatelliteSource src = req.source;
  QString vkey = req.vworldKey;
  if (src == SatelliteSource::Auto) {
    if (vkey.isEmpty()) vkey = loadVworldApiKey();
    src = (!vkey.isEmpty() && checkVworldApiKey(vkey)) ? SatelliteSource::Vworld : SatelliteSource::Google;
  }
  QString url, name;
  if (src == SatelliteSource::Vworld) {
    url = QStringLiteral("https://api.vworld.kr/req/wmts/1.0.0/%1/Satellite/${z}/${y}/${x}.jpeg").arg(vkey);
    name = QStringLiteral("VWorld Satellite WMTS");
  } else {
    url = QStringLiteral("https://mt1.google.com/vt/lyrs=s&x=${x}&y=${y}&z=${z}");
    name = QStringLiteral("Google Satellite XYZ");
  }
  QString cachePath;
  if (!req.cacheDir.isEmpty()) {
    cachePath = QDir(req.cacheDir).filePath(src == SatelliteSource::Vworld ? QStringLiteral("wms-vworld") : QStringLiteral("wms-google"));
    QDir().mkpath(cachePath);
  }
  const QByteArray xml = wmsXml(url, cachePath).toUtf8();
  std::unique_ptr<GDALDataset> srcDs(GDALDataset::Open(xml.constData(), GDAL_OF_RASTER | GDAL_OF_READONLY));
  if (!srcDs) {
    if (errorOut) *errorOut = QStringLiteral("위성 타일 원본(%1)을 열지 못했습니다.").arg(name);
    return false;
  }
  const double xMin = req.centerX - req.halfExtentM, xMax = req.centerX + req.halfExtentM;
  const double yMin = req.centerY - req.halfExtentM, yMax = req.centerY + req.halfExtentM;
  const QByteArray ts = QByteArray::number(req.texels);
  CPLStringList args;
  for (const char* a : {"-t_srs"}) args.AddString(a);
  args.AddString(req.workCrs.toUtf8().constData());
  args.AddString("-te");
  for (double v : {xMin, yMin, xMax, yMax}) args.AddString(QByteArray::number(v, 'f', 3).constData());
  for (const char* a : {"-ts"}) args.AddString(a);
  args.AddString(ts.constData());
  args.AddString(ts.constData());
  for (const char* a : {"-r", "bilinear", "-ot", "Byte", "-of", "MEM", "-multi"}) args.AddString(a);
  GDALWarpAppOptions* opts = GDALWarpAppOptionsNew(args.List(), nullptr);
  GDALDatasetH hSrc = srcDs.get();
  int usage = 0;
  GDALDatasetH warped = GDALWarp("", nullptr, 1, &hSrc, opts, &usage);
  GDALWarpAppOptionsFree(opts);
  if (!warped) {
    if (errorOut) *errorOut = QStringLiteral("위성 타일을 작업 좌표계로 옮기지 못했습니다(%1).").arg(name);
    return false;
  }
  std::unique_ptr<GDALDataset> ds(GDALDataset::FromHandle(warped));
  const int w = ds->GetRasterXSize(), h = ds->GetRasterYSize();
  if (ds->GetRasterCount() < 3) {
    if (errorOut) *errorOut = QStringLiteral("위성 영상에 RGB 밴드가 없습니다.");
    return false;
  }
  QImage img(w, h, QImage::Format_RGB888);
  std::vector<uint8_t> band(size_t(w) * h);
  qint64 nonBlack = 0;
  for (int b = 0; b < 3; ++b) {
    if (ds->GetRasterBand(b + 1)->RasterIO(GF_Read, 0, 0, w, h, band.data(), w, h, GDT_Byte, 0, 0, nullptr) != CE_None) {
      if (errorOut) *errorOut = QStringLiteral("위성 영상을 읽지 못했습니다.");
      return false;
    }
    for (int y = 0; y < h; ++y) {
      uchar* line = img.scanLine(y);
      for (int x = 0; x < w; ++x) {
        const uint8_t v = band[size_t(y) * w + x];
        line[x * 3 + b] = v;
        if (v > 8) ++nonBlack;
      }
    }
  }
  const double filled = double(nonBlack) / (3.0 * double(w) * double(h));
  if (filled < 0.5) {
    if (errorOut) *errorOut = QStringLiteral("받은 위성 타일이 대부분 비었습니다(%1). 키·네트워크를 확인하세요.").arg(name);
    return false;
  }
  out->image = std::move(img);
  out->sourceName = name;
  const double mPerPx = 2 * req.halfExtentM / req.texels;
  out->zoomLevel = int(std::round(std::log2(156543.03 * std::cos(36.0 * M_PI / 180.0) / mPerPx)));
  return true;
}

}  // namespace strata3d
