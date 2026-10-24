#include "StrataBridge.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>

#include "EvaluationJson.h"
#include "MainWindow.h"
#include "StrataBridgeClient.h"

StrataBridge::StrataBridge(MainWindow* window, QObject* parent) : QObject(parent), w_(window) {
  connect(&server_, &QLocalServer::newConnection, this, &StrataBridge::onConnection);
}

bool StrataBridge::listen(const QString& pipeName, QString* errorOut) {
  pipe_ = pipeName;
  QLocalServer::removeServer(pipeName);  // 윈도의 이름 있는 파이프는 남지 않지만 다른 OS 를 위해
  if (!server_.listen(pipeName)) {
    if (errorOut) *errorOut = server_.errorString();
    return false;
  }
  writeState();
  return true;
}

void StrataBridge::onConnection() {
  while (QLocalSocket* s = server_.nextPendingConnection()) {
    connect(s, &QLocalSocket::readyRead, this, [this, s] { onReadyRead(s); });
    connect(s, &QLocalSocket::disconnected, s, &QObject::deleteLater);
  }
}

void StrataBridge::onReadyRead(QLocalSocket* s) {
  while (s->canReadLine()) {
    const QByteArray line = s->readLine().trimmed();
    if (line.isEmpty()) continue;
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &pe);
    if (!doc.isObject()) {
      reply(s, {{"ok", false}, {"error", QStringLiteral("JSON 이 아닙니다: %1").arg(pe.errorString())}}, {});
      continue;
    }
    handle(s, doc.object());
  }
}

void StrataBridge::reply(QLocalSocket* s, QJsonObject res, const QJsonObject& req) {
  if (req.contains(QStringLiteral("id"))) res["id"] = req.value(QStringLiteral("id"));
  res["protocol"] = strata3d_bridge::kProtocolVersion;
  if (s && s->state() == QLocalSocket::ConnectedState) {
    s->write(QJsonDocument(res).toJson(QJsonDocument::Compact) + '\n');
    s->flush();
  }
}

QJsonObject StrataBridge::surveyJson() const {
  const strata3d::StrataSurvey* sv = w_->survey();
  if (!sv) return {};
  return QJsonObject{{"path", sv->path},
                     {"layers", QJsonArray::fromStringList(sv->presentLayers)},
                     {"center", QJsonObject{{"x", sv->center.x}, {"y", sv->center.y}}},
                     {"half_extent_m", sv->halfExtentM},
                     {"survey_areas", int(sv->surveyAreas.size())},
                     {"trenches", int(sv->trenches.size())},
                     {"feature_polys", int(sv->featurePolys.size())},
                     {"artifact_points", int(sv->artifactPoints.size())}};
}

void StrataBridge::writeState() {
  QJsonObject st{{"protocol", strata3d_bridge::kProtocolVersion},
                 {"pipe", pipe_},
                 {"pid", qint64(QCoreApplication::applicationPid())},
                 {"updated", QDateTime::currentDateTime().toString(Qt::ISODate)},
                 {"scene_complete", w_->sceneComplete()},
                 {"survey", surveyJson()},
                 {"last_evaluation", lastEvaluation_},
                 {"last_sheet", lastSheet_}};
  const QString path = strata3d_bridge::stateFilePath();
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile f(path);
  if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(QJsonDocument(st).toJson());
}

void StrataBridge::handle(QLocalSocket* s, const QJsonObject& req) {
  const QString cmd = req.value(QStringLiteral("cmd")).toString();
  auto fail = [&](const QString& msg) { reply(s, {{"ok", false}, {"error", msg}}, req); };

  if (cmd == QLatin1String("hello") || cmd == QLatin1String("state")) {
    reply(s, {{"ok", true}, {"app", "strata-3d"}, {"scene_complete", w_->sceneComplete()}, {"survey", surveyJson()},
              {"commands", QJsonArray{"hello", "state", "openSurvey", "evaluate", "profile", "renderSheet", "quit"}}}, req);
    return;
  }
  if (cmd == QLatin1String("quit")) {
    reply(s, {{"ok", true}}, req);
    QMetaObject::invokeMethod(qApp, &QCoreApplication::quit, Qt::QueuedConnection);
    return;
  }
  if (busy_) {
    fail(QStringLiteral("busy: 앞선 요청을 처리하는 중입니다."));
    return;
  }

  if (cmd == QLatin1String("openSurvey")) {
    const QString gpkg = req.value(QStringLiteral("gpkg")).toString();
    if (gpkg.isEmpty()) return fail(QStringLiteral("gpkg 경로가 필요합니다."));
    busy_ = true;
    connect(w_, &MainWindow::surveyOpened, this, [this, s, req](bool ok, const QString& msg) {
      busy_ = false;
      QJsonObject res{{"ok", ok}, {"message", msg}, {"scene_complete", w_->sceneComplete()}, {"survey", surveyJson()}};
      res["warnings"] = QJsonArray::fromStringList(w_->lastLoad().warnings);
      if (w_->lastLoad().features) res["feature_count"] = int(w_->lastLoad().features->size());
      if (!ok) res["error"] = msg;
      writeState();
      reply(s, res, req);
    }, Qt::SingleShotConnection);
    w_->openSurvey(gpkg);
    return;
  }

  if (cmd == QLatin1String("evaluate")) {
    if (!w_->lastLoad().evaluator) return fail(QStringLiteral("장면이 아직 준비되지 않았습니다."));
    const bool bySurvey = req.value(QStringLiteral("target")).toString() == QLatin1String("survey");
    const bool byPoint = req.contains(QStringLiteral("x")) && req.contains(QStringLiteral("y"));
    if (!bySurvey && !byPoint) return fail(QStringLiteral("target:\"survey\" 또는 x,y 가 필요합니다."));
    busy_ = true;
    connect(w_, &MainWindow::evaluationFinished, this, [this, s, req](bool ok, const QString& msg) {
      busy_ = false;
      QJsonObject res{{"ok", ok}};
      if (ok && w_->lastEvaluation()) {
        lastEvaluation_ = strata3d::evaluationToJson(*w_->lastEvaluation());
        res["evaluation"] = lastEvaluation_;
      } else {
        res["error"] = msg;
      }
      writeState();
      reply(s, res, req);
    }, Qt::SingleShotConnection);
    if (bySurvey) w_->evaluateSurvey();
    else w_->evaluateAt(req.value(QStringLiteral("x")).toDouble(), req.value(QStringLiteral("y")).toDouble());
    return;
  }

  if (cmd == QLatin1String("profile")) {
    if (!w_->lastLoad().evaluator) return fail(QStringLiteral("지형이 아직 없습니다."));
    const QJsonObject a = req.value(QStringLiteral("a")).toObject(), b = req.value(QStringLiteral("b")).toObject();
    const auto prof = w_->lastLoad().evaluator->terrainProfile({a.value(QStringLiteral("x")).toDouble(), a.value(QStringLiteral("y")).toDouble()},
                                                              {b.value(QStringLiteral("x")).toDouble(), b.value(QStringLiteral("y")).toDouble()},
                                                              req.value(QStringLiteral("step")).toDouble(10.0));
    QJsonArray arr;
    for (const auto& p : prof) arr.append(QJsonObject{{"d", p.distanceM}, {"x", p.x}, {"y", p.y}, {"z", p.elevationM}});
    reply(s, {{"ok", !prof.empty()}, {"profile", arr}, {"basis", QStringLiteral("Copernicus GLO-30 지표면(층서 아님)")}}, req);
    return;
  }

  if (cmd == QLatin1String("renderSheet")) {
    const QString png = req.value(QStringLiteral("png")).toString();
    if (png.isEmpty()) return fail(QStringLiteral("png 경로가 필요합니다."));
    if (!w_->sceneComplete()) return fail(QStringLiteral("장면이 아직 모두 올라오지 않았습니다(위성 포함)."));
    const QString pdf = req.value(QStringLiteral("pdf")).toString();
    QDir().mkpath(QFileInfo(png).absolutePath());
    const bool ok = w_->exportSheet(png, pdf);
    lastSheet_ = QJsonObject{{"png", png}, {"pdf", pdf}, {"png_bytes", qint64(QFileInfo(png).size())}, {"pdf_bytes", qint64(QFileInfo(pdf).size())}};
    writeState();
    reply(s, {{"ok", ok}, {"sheet", lastSheet_}}, req);
    return;
  }
  fail(QStringLiteral("모르는 명령: %1").arg(cmd));
}
