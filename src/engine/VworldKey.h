#pragma once

#include <QString>

namespace strata3d {

// Strata(VworldSettings::loadApiKey)와 같은 순서로 VWorld 키를 찾는다. 쓰기는 하지 않는다.
//   1) %LOCALAPPDATA%\ka-hgis\ka-hgis\ka-hgis-vworld.ini (또는 %LOCALAPPDATA%\ka-hgis\ka-hgis-vworld.ini)
//   2) HKCU\Software\ka-hgis\ka-hgis  (VWorld/ApiKey, vworld/apiKey)
//   3) 환경 변수 VWORLD_API_KEY
//   4) 앱 폴더 옆 config/secrets.ini
// 키 값은 로그·오류문·화면에 절대 쓰지 않는다. 없으면 빈 문자열.
QString loadVworldApiKey();

// 키를 가볍게 한 번 불러 본다. 무효·만료면 false 와 사유(키 값은 넣지 않음).
// VWorld 는 만료 키에도 HTTP 200 + 오류 본문을 주므로 본문을 확인한다.
bool checkVworldApiKey(const QString& key, QString* reasonOut = nullptr);

}  // namespace strata3d
