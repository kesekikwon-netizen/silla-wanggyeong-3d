#pragma once

#include <QJsonObject>
#include <QString>

// strata-3d 와 Strata(hgis) 사이의 로컬 파이프 클라이언트. Qt Network 만 있으면 hgis 쪽에서도 그대로 링크된다.
// 프로토콜은 docs/bridge-protocol.md. 요청·응답은 줄바꿈으로 끝나는 JSON 한 줄이다.
namespace strata3d_bridge {

constexpr int kProtocolVersion = 1;
QString defaultPipeName();
// %LOCALAPPDATA%\strata-3d\bridge-state.json — 앱이 실행 중이 아니어도 마지막 결과 경로를 읽을 수 있다.
QString stateFilePath();

// 블로킹 호출(워커 스레드 또는 짧은 도구에서). 연결·응답 시간 안에 못 받으면 빈 객체와 사유.
QJsonObject request(const QString& pipeName, const QJsonObject& req, int timeoutMs, QString* errorOut = nullptr);

}  // namespace strata3d_bridge
