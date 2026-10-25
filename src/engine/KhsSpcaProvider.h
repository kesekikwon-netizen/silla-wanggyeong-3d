#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <vector>

namespace strata3d {

// 국가유산 공간정보서비스(gis-heritage.go.kr) 지정 목록 /openapi/xmlService/spca.do?ccbaKdcd=종목코드.
// 키 없이 응답한다. 항목마다 명칭·종목·시대·지정일·분류·중심점(cnX,cnY = EPSG:5179)을 준다.
// 좌표계는 2026-10-25 국가유산청 점(경도·위도)과 대조해 확인했다(동궁과 월지 12 m). docs/research/METHODS.md 3장.
struct SpcaRecord {
  QString name;            // ccbaMnm  예: "경주 황룡사지"
  QString kind;            // ccmaName 예: "사적"
  QString period;          // ccceName 예: "신라시대", "통일신라시대" (빈 값·「미상」도 있다)
  QString classification;  // ctgrname 예: "유적건조물 / 종교신앙 / 불교 / 사찰"
  QString designatedOn;    // ccbaAsdt 예: "19630121"
  QString location;        // vlocName 예: "경상북도 경주시"
  double x5179 = 0;        // cnX
  double y5179 = 0;        // cnY
};

struct SpcaList {
  std::vector<SpcaRecord> records;
  QString kindCode;        // 요청한 종목코드(11 국보, 12 보물, 13 사적, 14 사적및명승, 15 명승, 16 천연기념물 …)
  QDateTime fetchedAt;     // 받은 시각(캐시면 캐시 파일 시각)
  QString sourceUrl;
};

// 네트워크 없이 XML 한 덩어리를 읽는다.
bool parseSpca(const QByteArray& xml, std::vector<SpcaRecord>* out, QString* errorOut = nullptr);

// 종목 하나를 받는다(cacheDir 가 있으면 7일 캐시). locationContains 가 비어 있지 않으면 소재지로 거른다.
bool loadSpca(const QString& kindCode, const QString& cacheDir, SpcaList* out, QString* errorOut = nullptr,
              const QString& locationContains = QString());

// 이름이 같은 항목(공백 무시). 없으면 nullptr.
const SpcaRecord* findSpcaByName(const SpcaList& list, const QString& name);

}  // namespace strata3d
