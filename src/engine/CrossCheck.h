#pragma once

#include <QString>

namespace strata3d {

// 같은 유산을 서로 다른 출처가 어디에 찍었는지 대조한다(docs/BLUEPRINT.md 4.3).
// 거리는 두 점을 EPSG:5179(UTM-K) 미터로 옮겨 잰다.
enum class Agreement { Match, Near, Mismatch, SingleSource };

struct CrossCheckThresholds {
  double matchM = 10.0;  // 이하면 「일치」
  double nearM = 50.0;   // 이하면 「근접」, 넘으면 「확인 필요」
};

struct PositionCheck {
  QString sourceA;
  QString sourceB;
  double distanceM = 0;
  Agreement level = Agreement::SingleSource;
  QString error;  // 좌표 변환 실패 사유
};

PositionCheck comparePositions(const QString& sourceA, double ax, double ay, const QString& crsA, const QString& sourceB,
                               double bx, double by, const QString& crsB, const CrossCheckThresholds& t = {});

QString agreementLabel(Agreement a);  // 일치 근접 확인 필요 단일 출처

}  // namespace strata3d
