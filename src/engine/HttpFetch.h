#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace strata3d {

struct HttpResult {
  bool ok = false;  // 전송이 끝나고 본문이 있다. HTTP 오류 본문도 ok=true 일 수 있으니 호출자가 내용을 확인한다.
  QByteArray body;
  QString contentType;
  QString error;
};

// GDAL(libcurl) 로 요청한다. UI 스레드 밖(워커)에서 부른다. 헤더는 "이름: 값" 형식.
HttpResult httpGet(const QString& url, const QStringList& headers = {}, int timeoutSec = 60);
HttpResult httpPost(const QString& url, const QByteArray& body, const QStringList& headers = {},
                    int timeoutSec = 120);

}  // namespace strata3d
