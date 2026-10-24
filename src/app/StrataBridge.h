#pragma once

#include <QJsonObject>
#include <QLocalServer>
#include <QObject>

class MainWindow;
class QLocalSocket;

// Strata(hgis) ↔ strata-3d 로컬 파이프 서버. 한 번에 하나의 비동기 요청만 처리한다(busy 면 거절).
class StrataBridge : public QObject {
  Q_OBJECT
 public:
  StrataBridge(MainWindow* window, QObject* parent = nullptr);
  bool listen(const QString& pipeName, QString* errorOut = nullptr);
  QString pipeName() const { return pipe_; }

 private:
  void onConnection();
  void onReadyRead(QLocalSocket* s);
  void handle(QLocalSocket* s, const QJsonObject& req);
  void reply(QLocalSocket* s, QJsonObject res, const QJsonObject& req);
  QJsonObject surveyJson() const;
  void writeState();

  MainWindow* w_;
  QLocalServer server_;
  QString pipe_;
  bool busy_ = false;
  QJsonObject lastEvaluation_;
  QJsonObject lastSheet_;
};
