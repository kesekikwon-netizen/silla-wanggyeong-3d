#include "StrataBridgeClient.h"

#include <QDir>
#include <QElapsedTimer>
#include <algorithm>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QThread>

namespace strata3d_bridge {

QString defaultPipeName() { return QStringLiteral("strata-3d-bridge-v1"); }

QString stateFilePath() {
  const QString local = qEnvironmentVariable("LOCALAPPDATA");
  const QString base = local.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) : local;
  return QDir(base).filePath(QStringLiteral("strata-3d/bridge-state.json"));
}

QJsonObject request(const QString& pipeName, const QJsonObject& req, int timeoutMs, QString* errorOut) {
  QLocalSocket sock;
  // 앱이 그림을 그리느라 바쁜 사이에는 파이프가 잠깐 안 받을 수 있다. 제한시간 안에서 다시 시도한다.
  QElapsedTimer connectTimer;
  connectTimer.start();
  while (true) {
    sock.connectToServer(pipeName);
    if (sock.waitForConnected(2000)) break;
    if (connectTimer.elapsed() >= std::min(timeoutMs, 30000)) {
      if (errorOut) *errorOut = QStringLiteral("strata-3d 에 연결하지 못했습니다(%1): %2").arg(pipeName, sock.errorString());
      return {};
    }
    sock.abort();
    QThread::msleep(200);
  }
  sock.write(QJsonDocument(req).toJson(QJsonDocument::Compact) + '\n');
  if (!sock.waitForBytesWritten(5000)) {
    if (errorOut) *errorOut = QStringLiteral("요청을 보내지 못했습니다.");
    return {};
  }
  QByteArray buf;
  QElapsedTimer t;
  t.start();
  while (!buf.contains('\n')) {
    const int left = timeoutMs - int(t.elapsed());
    if (left <= 0 || !sock.waitForReadyRead(left)) {
      if (errorOut) *errorOut = QStringLiteral("응답 시간이 지났습니다(%1 ms).").arg(timeoutMs);
      return {};
    }
    buf += sock.readAll();
  }
  QJsonParseError pe;
  const QJsonDocument doc = QJsonDocument::fromJson(buf.left(buf.indexOf('\n')), &pe);
  if (!doc.isObject()) {
    if (errorOut) *errorOut = QStringLiteral("응답을 읽지 못했습니다: %1").arg(pe.errorString());
    return {};
  }
  return doc.object();
}

}  // namespace strata3d_bridge
