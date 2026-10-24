#include "HttpFetch.h"

#include <cpl_http.h>
#include <cpl_string.h>
#include <gdal_priv.h>

#include <mutex>

namespace strata3d {

namespace {

HttpResult fetch(const QString& url, const QByteArray& post, const QStringList& headers, int timeoutSec) {
  static std::once_flag once;
  std::call_once(once, [] { GDALAllRegister(); });
  CPLStringList opts;
  opts.SetNameValue("TIMEOUT", QByteArray::number(timeoutSec).constData());
  opts.SetNameValue("MAX_RETRY", "2");
  opts.SetNameValue("RETRY_DELAY", "2");
  const QByteArray hdr = headers.join(QLatin1Char('\n')).toUtf8();
  if (!hdr.isEmpty()) opts.SetNameValue("HEADERS", hdr.constData());
  if (!post.isEmpty()) {
    opts.SetNameValue("CUSTOMREQUEST", "POST");
    opts.SetNameValue("POSTFIELDS", post.constData());
  }
  HttpResult out;
  CPLHTTPResult* r = CPLHTTPFetch(url.toUtf8().constData(), opts.List());
  if (!r) {
    out.error = QStringLiteral("HTTP 요청을 시작하지 못했습니다.");
    return out;
  }
  if (r->nStatus != 0 || r->pszErrBuf) {
    out.error = QString::fromUtf8(r->pszErrBuf ? r->pszErrBuf : "전송 실패");
  } else {
    out.ok = true;
  }
  if (r->pabyData && r->nDataLen > 0) out.body = QByteArray(reinterpret_cast<const char*>(r->pabyData), r->nDataLen);
  if (r->pszContentType) out.contentType = QString::fromUtf8(r->pszContentType);
  CPLHTTPDestroyResult(r);
  if (out.body.isEmpty() && out.ok) {
    out.ok = false;
    out.error = QStringLiteral("빈 응답");
  }
  return out;
}

}  // namespace

HttpResult httpGet(const QString& url, const QStringList& headers, int timeoutSec) {
  return fetch(url, {}, headers, timeoutSec);
}

HttpResult httpPost(const QString& url, const QByteArray& body, const QStringList& headers, int timeoutSec) {
  return fetch(url, body, headers, timeoutSec);
}

}  // namespace strata3d
