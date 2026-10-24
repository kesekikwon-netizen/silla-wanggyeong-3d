#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSurfaceFormat>
#include <QTextStream>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "KaGlTerrainViewport.h"
#include "KaTheme.h"
#include "MainWindow.h"
#include "StrataBridge.h"
#include "StrataBridgeClient.h"

namespace {

// 설치 옆에 GDAL/PROJ 자료 폴더가 있으면 환경 변수로 알려 준다(dev-env.ps1 없이 실행해도 좌표계가 열리게).
void primeGeoEnvironment() {
  const QString root = qEnvironmentVariable("OSGEO4W_ROOT");
  if (root.isEmpty()) return;
  if (qEnvironmentVariableIsEmpty("PROJ_DATA")) qputenv("PROJ_DATA", QDir(root).filePath("share/proj").toUtf8());
  if (qEnvironmentVariableIsEmpty("GDAL_DATA")) qputenv("GDAL_DATA", QDir(root).filePath("apps/gdal-dev/share/gdal").toUtf8());
}

// 화면에 지형이 정말 그려졌는지 본다: 배경(하늘색) 아닌 점이 충분히 많고 색이 단조롭지 않아야 한다.
struct FrameCheck {
  bool ok = false;
  QString text;
};
FrameCheck checkFrame(const QImage& img) {
  FrameCheck r;
  if (img.isNull()) {
    r.text = QStringLiteral("프레임을 가져오지 못했습니다.");
    return r;
  }
  const QImage px = img.convertToFormat(QImage::Format_RGB32);
  const QRgb sky = qRgb(189, 204, 224);  // KaGlTerrainViewport 의 glClearColor
  qint64 terrain = 0, total = 0;
  double sum = 0, sum2 = 0;
  for (int y = 0; y < px.height(); y += 2) {
    for (int x = 0; x < px.width(); x += 2) {
      const QRgb c = px.pixel(x, y);
      ++total;
      if (std::abs(qRed(c) - qRed(sky)) + std::abs(qGreen(c) - qGreen(sky)) + std::abs(qBlue(c) - qBlue(sky)) > 24) {
        ++terrain;
        const double l = 0.299 * qRed(c) + 0.587 * qGreen(c) + 0.114 * qBlue(c);
        sum += l;
        sum2 += l * l;
      }
    }
  }
  const double frac = total ? double(terrain) / double(total) : 0.0;
  const double mean = terrain ? sum / double(terrain) : 0.0;
  const double sd = terrain ? std::sqrt(std::max(0.0, sum2 / double(terrain) - mean * mean)) : 0.0;
  r.ok = frac > 0.30 && sd > 6.0;
  r.text = QStringLiteral("프레임 %1×%2, 지형 픽셀 %3%, 밝기 표준편차 %4").arg(img.width()).arg(img.height())
               .arg(frac * 100, 0, 'f', 1).arg(sd, 0, 'f', 1);
  return r;
}

}  // namespace

int main(int argc, char** argv) {
  primeGeoEnvironment();
  QSurfaceFormat fmt;
  fmt.setVersion(3, 3);
  fmt.setProfile(QSurfaceFormat::CoreProfile);
  fmt.setDepthBufferSize(24);
  fmt.setSamples(4);
  QSurfaceFormat::setDefaultFormat(fmt);

  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName(QStringLiteral("strata-3d"));
  QCoreApplication::setApplicationName(QStringLiteral("strata-3d"));
  KaTheme::apply(&app);

  QCommandLineParser parser;
  parser.addHelpOption();
  QCommandLineOption smoke(QStringLiteral("smoke-quit"), QStringLiteral("실측 DEM을 읽어 한 장 그린 뒤 종료(성공 0)."));
  QCommandLineOption report(QStringLiteral("smoke-report"), QStringLiteral("스모크 결과를 이 파일에도 적는다."), QStringLiteral("file"));
  QCommandLineOption image(QStringLiteral("smoke-image"), QStringLiteral("스모크에서 그린 프레임을 PNG 로 저장."), QStringLiteral("file"));
  QCommandLineOption cam(QStringLiteral("cam"), QStringLiteral("시작 카메라 yaw,pitch,거리(m)."), QStringLiteral("y,p,d"));
  QCommandLineOption dem(QStringLiteral("dem"), QStringLiteral("DEM 경로 또는 /vsicurl/ 주소(기본: Copernicus GLO-30)."), QStringLiteral("path"));
  QCommandLineOption lon(QStringLiteral("lon"), QStringLiteral("중심 경도(WGS84)."), QStringLiteral("deg"));
  QCommandLineOption lat(QStringLiteral("lat"), QStringLiteral("중심 위도(WGS84)."), QStringLiteral("deg"));
  QCommandLineOption noFeat(QStringLiteral("no-features"), QStringLiteral("건물·유적을 받지 않는다."));
  QCommandLineOption noSat(QStringLiteral("no-satellite"), QStringLiteral("위성영상을 받지 않는다."));
  QCommandLineOption evalCenter(QStringLiteral("eval-center"), QStringLiteral("불러온 뒤 지형 한가운데(첨성대 부근)를 평가한다."));
  QCommandLineOption sheetOut(QStringLiteral("sheet-out"), QStringLiteral("스모크 끝에 입체지형 도면 PNG(+같은 이름 PDF)를 저장."), QStringLiteral("file.png"));
  QCommandLineOption noInitial(QStringLiteral("no-initial-scene"), QStringLiteral("시작할 때 경주 기본 장면을 불러오지 않는다(연동으로 조사를 받을 때)."));
  QCommandLineOption noAo(QStringLiteral("no-ao"), QStringLiteral("환경 차폐(GTAO)를 끈다."));
  QCommandLineOption modeOpt(QStringLiteral("mode"), QStringLiteral("보기 방식 0 점토 1 표고 2 경사 3 위성."), QStringLiteral("n"));
  QCommandLineOption noBridge(QStringLiteral("no-bridge"), QStringLiteral("Strata 연동 파이프를 열지 않는다."));
  QCommandLineOption bridgePipe(QStringLiteral("bridge-pipe"), QStringLiteral("연동 파이프 이름(기본 strata-3d-bridge-v1)."), QStringLiteral("name"));
  parser.addOptions({smoke, report, image, cam, dem, lon, lat, noFeat, noSat, evalCenter, sheetOut, noBridge, bridgePipe, noInitial, noAo, modeOpt});
  parser.process(app);

  MainWindow::Options opt;
  opt.demPath = parser.value(dem);
  if (parser.isSet(lon)) opt.lon = parser.value(lon).toDouble();
  if (parser.isSet(lat)) opt.lat = parser.value(lat).toDouble();
  opt.loadFeatures = !parser.isSet(noFeat);
  opt.loadSatellite = !parser.isSet(noSat);

  MainWindow w(opt);
  if (parser.isSet(noAo)) w.viewport()->setAmbientOcclusion(false);
  if (parser.isSet(modeOpt)) w.viewport()->setMode(KaGlTerrainViewport::Mode(parser.value(modeOpt).toInt()));
  if (parser.isSet(cam)) {
    const QStringList c = parser.value(cam).split(QLatin1Char(','));
    if (c.size() == 3) w.viewport()->setCameraPreset(c[0].toFloat(), c[1].toFloat(), c[2].toFloat());
  }
  w.show();
  StrataBridge bridge(&w);
  if (!parser.isSet(noBridge) && !parser.isSet(smoke)) {
    QString err;
    const QString pipe = parser.isSet(bridgePipe) ? parser.value(bridgePipe) : strata3d_bridge::defaultPipeName();
    if (!bridge.listen(pipe, &err)) std::fprintf(stderr, "Strata 연동 파이프를 열지 못했습니다: %s\n", err.toUtf8().constData());
  }
  if (!parser.isSet(noInitial)) QTimer::singleShot(0, &w, &MainWindow::loadScene);

  if (!parser.isSet(smoke)) return app.exec();

  auto finish = [&](int code, const QString& text) {
    const QString line = QStringLiteral("strata-3d smoke: %1 (%2)").arg(code == 0 ? "PASS" : "FAIL", text);
    std::fprintf(code == 0 ? stdout : stderr, "%s\n", line.toUtf8().constData());
    if (parser.isSet(report)) {
      QFile f(parser.value(report));
      if (f.open(QIODevice::WriteOnly | QIODevice::Text)) QTextStream(&f) << line << "\n";
    }
    QCoreApplication::exit(code);
  };
  QString loadText;
  bool satReady = !opt.loadSatellite;
  bool evalReady = !parser.isSet(evalCenter);
  bool evalStarted = false;
  QString evalText;
  auto maybeEvaluate = [&] {
    if (evalReady || evalStarted || !satReady || !w.lastLoad().tree) return;
    evalStarted = true;
    const strata3d::DemGrid& g = w.lastLoad().tree->grid();
    w.evaluateAt(g.centerX(), g.centerY());
  };
  QObject::connect(&w, &MainWindow::evaluationFinished, &app, [&](bool ok, const QString& msg) {
    if (!ok) finish(1, QStringLiteral("평가 실패: ") + msg);
    evalText = msg;
    evalReady = true;
    w.viewport()->update();
  });
  QObject::connect(&w, &MainWindow::loadFinished, &app, [&](bool ok, const QString& msg) {
    if (!ok) finish(1, QStringLiteral("DEM 불러오기 실패: ") + msg);
    loadText = msg;
  });
  QObject::connect(&w, &MainWindow::satelliteFinished, &app, [&](bool ok, const QString& msg) {
    if (!ok) finish(1, QStringLiteral("위성영상 실패: ") + msg);
    loadText += QStringLiteral(" | ") + msg;
    satReady = true;
    maybeEvaluate();
  });
  QObject::connect(w.viewport(), &KaGlTerrainViewport::frameRendered, &app, [&] {
    auto* vp = w.viewport();
    if (!vp->hasTerrain()) return;
    if (!satReady) return;
    maybeEvaluate();
    if (!evalReady) return;
    if (vp->stats().frames < 3) {  // 그리기는 update() 때만 일어나므로 몇 장 더 그리게 한다
      vp->update();
      return;
    }
    const QImage frame = vp->grabFramebuffer();
    if (parser.isSet(image)) frame.save(parser.value(image));
    const FrameCheck c = checkFrame(frame);
    QString text = loadText + QStringLiteral(" | ") + c.text + QStringLiteral(" | ") + vp->glInfo();
    if (!evalText.isEmpty()) text += QStringLiteral(" | 평가: ") + evalText;
    if (!c.ok || !parser.isSet(sheetOut)) {
      finish(c.ok ? 0 : 1, text);
      return;
    }
    QTimer::singleShot(0, &app, [&, text] {  // 그리는 도중이 아니라 한 박자 뒤에 도면을 만든다
      const QString png = parser.value(sheetOut);
      QString pdf = png;
      pdf.replace(QStringLiteral(".png"), QStringLiteral(".pdf"));
      const bool ok = w.exportSheet(png, pdf);
      const qint64 pngSize = QFileInfo(png).size(), pdfSize = QFileInfo(pdf).size();
      finish(ok && pngSize > 200000 && pdfSize > 20000 ? 0 : 1, text + QStringLiteral(" | 도면 PNG %1 KB, PDF %2 KB").arg(pngSize / 1024).arg(pdfSize / 1024));
    });
  });
  QTimer::singleShot(180000, &app, [&] { finish(2, QStringLiteral("180초 안에 끝나지 않았습니다.")); });
  return app.exec();
}
