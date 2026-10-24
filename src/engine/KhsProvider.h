#pragma once

#include <QByteArray>
#include <vector>

#include "GeoFeature.h"

namespace strata3d {

// 국가유산청 Open API(www.khs.go.kr/cha/SearchKindOpenapiList.do, 키 불필요)의 지정·등록 유산 목록.
// 항목마다 경도·위도가 있고, 요청 창 안에 들어오는 것만 HeritagePoint 로 돌려준다.
// cityContains 는 시군구명에 들어 있어야 하는 글자(기본 "경주").
bool loadKhsHeritage(const FeatureRequest& req, std::vector<GeoFeature>* out, QString* errorOut = nullptr,
                     const QString& cityContains = QString::fromUtf16(u"경주"), const QString& provinceCode = QStringLiteral("37"));

// 전국 시도(17곳) 목록을 받아(시도별 7일 캐시) 창 안의 지정·등록 유산만 돌려준다. 어느 지역 조사든 쓸 수 있다.
// 일부 시도가 실패해도 나머지로 계속하고, 실패한 시도는 errorOut 에 모아 적는다(하나라도 성공하면 true).
bool loadKhsHeritageNearby(const FeatureRequest& req, std::vector<GeoFeature>* out, QString* errorOut = nullptr);

// 시험용: 목록 XML 한 덩어리를 파싱한다(네트워크 없음).
bool parseKhsList(const QByteArray& xml, const QString& workCrs, const QString& cityContains,
                  std::vector<GeoFeature>* out, QString* errorOut = nullptr);

}  // namespace strata3d
