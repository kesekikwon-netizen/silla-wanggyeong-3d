#include <QApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QMouseEvent>
#include <QSurfaceFormat>
#include <QTextStream>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <qgsapplication.h>

#include "KaGlTerrainViewport.h"
#include "StrataProjectLayers.h"
#include "StrataSurvey.h"
#include "KaTheme.h"
#include "KaCrashGuard.h"
#include "MainWindow.h"
#include "StrataReceive.h"
#include "StrataBridge.h"
#include "StrataBridgeClient.h"

namespace {

// 설치 옆에 GDAL/PROJ 자료 폴더가 있으면 환경 변수로 알려 준다(dev-env.ps1 없이 실행해도 좌표계가 열리게).
void primeGeoEnvironment() {
  QString root = qEnvironmentVariable("OSGEO4W_ROOT");
  if (root.isEmpty()) {
    for (const char* cand : {"C:/OSGeo4W", "D:/OSGeo4W", "A:/OSGeo4W"}) {
      if (QDir(QString::fromLatin1(cand)).exists()) {
        root = QString::fromLatin1(cand);
        qputenv("OSGEO4W_ROOT", root.toUtf8());
        break;
      }
    }
  }
  if (root.isEmpty()) return;
  if (qEnvironmentVariableIsEmpty("PROJ_DATA")) qputenv("PROJ_DATA", QDir(root).filePath("share/proj").toUtf8());
  if (qEnvironmentVariableIsEmpty("PROJ_LIB")) qputenv("PROJ_LIB", QDir(root).filePath("share/proj").toUtf8());
  if (qEnvironmentVariableIsEmpty("GDAL_DATA")) qputenv("GDAL_DATA", QDir(root).filePath("apps/gdal-dev/share/gdal").toUtf8());
  const QString qtPlugins = QDir(root).filePath("apps/Qt6/plugins");
  if (qEnvironmentVariableIsEmpty("QT_PLUGIN_PATH") && QDir(qtPlugins).exists()) {
    qputenv("QT_PLUGIN_PATH", qtPlugins.toUtf8());
  }
  if (QDir(qtPlugins).exists()) {
    QCoreApplication::addLibraryPath(qtPlugins);
  }
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
  // 충돌 기록(심볼 스택 + 미니덤프)은 Strata 로그와 섞이지 않게 3D 폴더에 둔다
  if (qEnvironmentVariableIsEmpty("KA_HGIS_LOG_DIR")) {
    const QString base = qEnvironmentVariable("LOCALAPPDATA");
    // qputenv 는 ANSI 바이트로 넘겨 한글 사용자 폴더(예: 「권을」)가 깨진다: 넓은 문자 API 로 넣는다
    if (!base.isEmpty()) {
      const std::wstring dir = QDir::toNativeSeparators(QDir(base).filePath(QStringLiteral("strata-3d/logs"))).toStdWString();
      _wputenv_s(L"KA_HGIS_LOG_DIR", dir.c_str());
    }
  }
  KaCrashGuard::install();
  primeGeoEnvironment();
  QSurfaceFormat fmt;
  fmt.setVersion(3, 3);
  fmt.setProfile(QSurfaceFormat::CoreProfile);
  fmt.setDepthBufferSize(24);
  fmt.setSamples(4);
  QSurfaceFormat::setDefaultFormat(fmt);

  // Strata 코드(조사 열기·레이어 칸)는 QGIS 위에서 돈다: QApplication 대신 QgsApplication(hgis 와 같은 qgis-dev).
  QgsApplication app(argc, argv, true);
  {
    QString prefix = qEnvironmentVariable("QGIS_PREFIX_PATH");
    if (prefix.isEmpty()) prefix = QDir(qEnvironmentVariable("OSGEO4W_ROOT", QStringLiteral("C:/OSGeo4W"))).filePath(QStringLiteral("apps/qgis-dev"));
    QgsApplication::setPrefixPath(prefix, true);
    QgsApplication::initQgis();
  }
  // 조직·앱 이름은 Strata 와 같게 둔다: Strata 코드는 이 이름으로 계정·키 폴더(KaPortableRuntime::userConfigDir),
  // 받은 주변유적 폴더(AppLocalData/주변유적), 화면 모양 설정(KaThemeOptions)을 찾는다. 같은 자료·같은 모양을 쓰려는 것.
  // 3D 자신의 캐시는 s3dCacheDir()(%LOCALAPPDATA%\strata-3d\strata-3d\cache, 예전 자리 그대로)에 따로 둔다.
  QCoreApplication::setOrganizationName(QStringLiteral("ka-hgis"));
  QCoreApplication::setApplicationName(QStringLiteral("ka-hgis"));
  QGuiApplication::setApplicationDisplayName(QStringLiteral("Strata 3D"));
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
  QCommandLineOption trenchClicks(QStringLiteral("trench-clicks"), QStringLiteral("스모크: 트렌치 모드를 켜고 뷰포트 비율 좌표(0~1)에서 실제 마우스 클릭 두 번."), QStringLiteral("x1,y1,x2,y2"));
  QCommandLineOption windowShot(QStringLiteral("window-shot"), QStringLiteral("스모크 끝에 KaTheme 가 적용된 전체 창을 PNG 로 저장."), QStringLiteral("file.png"));
  QCommandLineOption openSurveyOpt(QStringLiteral("open-survey"), QStringLiteral("시작할 때 Strata 조사 파일(.gpkg)을 연다(읽기 전용). 스모크에서는 관계 표까지 기다린다."), QStringLiteral("file.gpkg"));
  parser.addOption(openSurveyOpt);
  QCommandLineOption selectOpt(QStringLiteral("select"), QStringLiteral("스모크: 장면이 올라온 뒤 이 이름의 유산을 골라 속성 칸에 연다."), QStringLiteral("name"));
  parser.addOption(selectOpt);
  QCommandLineOption e2eNewOpt(QStringLiteral("e2e-new-survey"), QStringLiteral("스모크: 이 폴더에 새 조사를 만들고 화면 가운데에 80 m 조사구역을 그려 저장한 뒤 다시 읽어 확인한다."), QStringLiteral("dir"));
  parser.addOption(e2eNewOpt);
  QCommandLineOption e2eRecvOpt(QStringLiteral("e2e-receive"), QStringLiteral("스모크: 연 조사에 soil|geology|river 를 Strata 받기 흐름으로 받아 지형에 입힌 뒤 캡처(조사 사본에만 쓴다)."), QStringLiteral("kind"));
  parser.addOption(e2eRecvOpt);
  parser.addOptions({smoke, report, image, cam, dem, lon, lat, noFeat, noSat, evalCenter, sheetOut, noBridge, bridgePipe, noInitial, noAo, modeOpt, trenchClicks, windowShot});
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
  if (parser.isSet(openSurveyOpt)) {
    const QString gpkg = QFileInfo(parser.value(openSurveyOpt)).absoluteFilePath();
    QTimer::singleShot(0, &w, [&w, gpkg] { w.openSurvey(gpkg); });
  } else if (!parser.isSet(noInitial)) {
    QTimer::singleShot(0, &w, &MainWindow::loadScene);
  }

  if (!parser.isSet(smoke)) return app.exec();

  bool finished = false;
  auto finish = [&](int code, const QString& text) {
    if (finished) return;  // 처음 판정만 남긴다(실패 뒤에 PASS 로 덮지 않게)
    finished = true;
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
  bool trenchClicked = false;
  bool recvStarted = false, recvDone = false, e2eNewStarted = false, selectQueued = false;
  QString recvText;
  bool relReady = !parser.isSet(openSurveyOpt);
  QObject::connect(&w, &MainWindow::surveyOpened, &app, [&](bool ok, const QString& msg) {
    if (!ok) finish(1, QStringLiteral("조사 열기 실패: ") + msg);
  });
  QString relText;
  QObject::connect(&w, &MainWindow::relationsFinished, &app, [&](bool ok, const QString& msg) {
    if (!ok) { finish(1, QStringLiteral("관계 표 실패: ") + msg); return; }
    relText = msg;
    relReady = true;
    w.viewport()->update();
  });
  int trenchFrames = 0;
  auto maybeEvaluate = [&] {
    if (evalReady || evalStarted || !satReady || !w.lastLoad().tree) return;
    evalStarted = true;
    const strata3d::DemGrid& g = w.lastLoad().tree->grid();
    w.evaluateAt(g.centerX(), g.centerY());
  };
  QObject::connect(&w, &MainWindow::evaluationFinished, &app, [&](bool ok, const QString& msg) {
    if (!ok) { finish(1, QStringLiteral("평가 실패: ") + msg); return; }
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
    if (!evalReady || !relReady) return;
    if (parser.isSet(selectOpt) && w.selectedFeatureId().isEmpty()) {
      if (!selectQueued) {  // 고르기는 속성 칸 위젯을 바꾸므로 그리는 도중이 아니라 한 박자 뒤에
        selectQueued = true;
        QTimer::singleShot(0, &app, [&] {
          if (!w.selectByName(parser.value(selectOpt))) { finish(1, QStringLiteral("고를 유산이 없습니다: ") + parser.value(selectOpt)); return; }
          w.viewport()->update();
        });
      }
      return;
    }
    if (parser.isSet(e2eRecvOpt) && !recvDone) {
      if (!recvStarted) {
        recvStarted = true;
        const QString k = parser.value(e2eRecvOpt);
        const auto kind = k == QLatin1String("geology") ? StrataReceive::Kind::Geology : k == QLatin1String("river") ? StrataReceive::Kind::River : StrataReceive::Kind::Soil;
        QObject::connect(w.receiver(), &StrataReceive::received, &app, [&](const QString& what) {
          recvText = what;
          // 받은 뒤 3D 다시 불러오기(1.5 s 뒤) + QGIS 그림이 지형에 오를 때까지 기다린다
          QTimer::singleShot(20000, &app, [&] { recvDone = true; w.viewport()->update(); });
        });
        QTimer::singleShot(0, &app, [&w, kind] { w.receiver()->fetchThematic(kind); });  // 그리는 도중이 아니라 한 박자 뒤에
      }
      return;
    }
    if (vp->stats().frames < 3) {  // 그리기는 update() 때만 일어나므로 몇 장 더 그리게 한다
      vp->update();
      return;
    }
    if (parser.isSet(trenchClicks)) {
      if (!trenchClicked) {  // 사람이 누르는 것과 같은 길(마우스 이벤트 → 레이캐스트)로 트렌치를 찍는다
        trenchClicked = true;
        const QStringList f = parser.value(trenchClicks).split(QLatin1Char(','));
        if (f.size() != 4) { finish(1, QStringLiteral("--trench-clicks 는 x1,y1,x2,y2")); return; }
        w.setTrenchMode(true);
        for (int i = 0; i < 2; ++i) {
          const QPointF at(f[2 * i].toDouble() * vp->width(), f[2 * i + 1].toDouble() * vp->height());
          for (auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
            QMouseEvent ev(type, at, vp->mapToGlobal(at), Qt::LeftButton, type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(vp, &ev);
          }
        }
        w.setTrenchMode(false);
        if (w.trenchProfile().size() < 2) { finish(1, QStringLiteral("트렌치 클릭이 지형에 닿지 않았습니다(하늘?)")); return; }
        trenchFrames = 0;
      }
      if (++trenchFrames < 4) {  // 겹침이 올라가 다시 그려질 때까지
        vp->update();
        return;
      }
    }
    const QImage frame = vp->grabFramebuffer();
    if (parser.isSet(image)) frame.save(parser.value(image));
    if (parser.isSet(windowShot)) w.grab().save(parser.value(windowShot));
    const FrameCheck c = checkFrame(frame);
    QString text = loadText + QStringLiteral(" | ") + c.text + QStringLiteral(" | ") + vp->glInfo();
    if (!evalText.isEmpty()) text += QStringLiteral(" | 평가: ") + evalText;
    if (!relText.isEmpty()) text += QStringLiteral(" | ") + relText;
    if (parser.isSet(e2eRecvOpt)) {
      text += QStringLiteral(" | 받음 %1 · 지형에 입힌 Strata 레이어 %2개 · 입힘 %3").arg(recvText).arg(w.qgisDrapeLayerCount()).arg(w.qgisDrapeReady() ? QStringLiteral("됨") : QStringLiteral("안 됨"));
      if (recvText.isEmpty() || !w.qgisDrapeReady()) { finish(1, text); return; }
    }
    if (parser.isSet(selectOpt)) text += QStringLiteral(" | 고름 %1 · 속성 칸 %2").arg(w.selectedFeatureId(), w.inspectorShowsRecord() ? QStringLiteral("기록 열림") : QStringLiteral("기록 없음"));
    if (c.ok && parser.isSet(e2eNewOpt)) {
      if (e2eNewStarted) return;
      e2eNewStarted = true;
      // 3D 에서 새 조사 → 조사구역 그리기 → Strata 저장 방식으로 저장 → 다른 읽기(GDAL)로 다시 읽어 같은지 본다.
      // frameRendered 는 그리는 도중(paintGL)에 나오므로 프로젝트를 비우고 저장하는 일은 한 박자 뒤에 한다.
      QTimer::singleShot(0, &app, [&, text] {
      w.setQuietSave(true);
      QString err;
      const QString dir = QFileInfo(parser.value(e2eNewOpt)).absoluteFilePath();
      QDir().mkpath(dir);
      const QString name = QStringLiteral("e2e_%1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
      const QString crs = w.lastLoad().tree->grid().crs;
      if (!w.createSurveyAt(dir, name, crs, &err)) { finish(1, text + QStringLiteral(" | 새 조사 실패: ") + err); return; }
      double cx = 0, cy = 0;
      w.viewport()->currentTargetWorld(&cx, &cy);
      const std::vector<strata3d::Pt> sq{{cx - 40, cy - 40}, {cx + 40, cy - 40}, {cx + 40, cy + 40}, {cx - 40, cy + 40}};
      if (!w.addSketchFeature(QStringLiteral("survey_area"), sq, nullptr, &err)) { finish(1, text + QStringLiteral(" | 그리기 실패: ") + err); return; }
      const bool unsavedBefore = w.hasUnsavedSurveyWork();
      if (!w.saveSurveyWork()) { finish(1, text + QStringLiteral(" | 저장 실패: ") + w.lastSaveError()); return; }
      const QString path = QDir(dir).filePath(name + QStringLiteral(".gpkg"));
      strata3d::StrataSurvey back;
      if (!strata3d::loadStrataSurvey(path, QStringLiteral("auto"), &back, &err)) { finish(1, text + QStringLiteral(" | 다시 읽기 실패: ") + err); return; }
      double area = 0;
      if (back.surveyAreas.size() == 1) {
        const auto& r = back.surveyAreas.front();
        for (size_t i = 0; i < r.size(); ++i) area += r[i].x * r[(i + 1) % r.size()].y - r[(i + 1) % r.size()].x * r[i].y;
        area = std::abs(area) * 0.5;
      }
      strata3d::StrataProject proj;
      const bool projOk = strata3d::readStrataProject(path, &proj, &err);
      const bool ok = unsavedBefore && !w.hasUnsavedSurveyWork() && back.surveyAreas.size() == 1 && std::abs(area - 6400.0) < 1.0 && back.workCrs == crs && projOk;
      finish(ok ? 0 : 1, text + QStringLiteral(" | 새 조사 %1: 저장 전 미저장 %2, 다시 읽은 조사구역 %3개 넓이 %4 ㎡(기대 6400), 좌표계 %5, 프로젝트 %6(%7 레이어)")
                                   .arg(QFileInfo(path).fileName()).arg(unsavedBefore ? QStringLiteral("있음") : QStringLiteral("없음"))
                                   .arg(back.surveyAreas.size()).arg(area, 0, 'f', 2).arg(back.workCrs)
                                   .arg(projOk ? proj.projectFrom : err).arg(proj.layers.size()));
      });
      return;
    }
    if (!c.ok || !parser.isSet(sheetOut)) {
      finish(c.ok ? 0 : 1, text);
      return;
    }
    QTimer::singleShot(0, &app, [&, text] {  // 그리는 도중이 아니라 한 박자 뒤에 도면을 만든다
      const QString png = parser.value(sheetOut);
      QFileInfo(png).dir().mkpath(QStringLiteral("."));
      QString pdf = png;
      pdf.replace(QStringLiteral(".png"), QStringLiteral(".pdf"));
      const bool ok = w.exportSheet(png, pdf);
      const qint64 pngSize = QFileInfo(png).size(), pdfSize = QFileInfo(pdf).size();
      finish(ok && pngSize > 200000 && pdfSize > 20000 ? 0 : 1,
             text + QStringLiteral(" | 도면 PNG %1 KB, PDF %2 KB").arg(pngSize / 1024).arg(pdfSize / 1024));
    });
  });
  QTimer::singleShot(parser.isSet(e2eRecvOpt) ? 420000 : 180000, &app, [&] { finish(2, QStringLiteral("시간 안에 끝나지 않았습니다.")); });
  return app.exec();
}
