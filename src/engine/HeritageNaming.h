#pragma once

#include <QString>

namespace strata3d {

// 국가유산 표기 정리. 2021-11-19 시행 문화재보호법 시행령·시행규칙 개정으로 지정번호(「사적 제6호」)를
// 표기하지 않는다(「사적 경주 황룡사지」). 화면·도면에 나가는 이름은 모두 이 함수를 거친다.

// 「제6호」「제19·188호」 같은 지정번호를 지우고, 종목만 남은 괄호 조각도 지운다.
// 예: "황룡사 9층목탑 · 대가람 (사적 제6호)" → "황룡사 9층목탑 · 대가람"
//     "동궁과 월지 · 안압지 (사적 제18호)" → "동궁과 월지 · 안압지"
//     "대릉원 황남대총 (높이 23m 남북 쌍분 · 사적 제512호)" → "대릉원 황남대총 (높이 23m 남북 쌍분)"
QString displayHeritageName(const QString& raw);

// 문자열에 들어 있는 지정 종목(국보·보물·사적·명승·천연기념물·국가민속문화유산 등). 없으면 빈 문자열.
// 「사적 및 명승」처럼 긴 이름을 먼저 찾는다.
QString designationKind(const QString& text);

// 도식 모델 설명에 남은 「3D 복원」「실측 복원」을 「3D 도식」「도식」으로 바꾼다(유형 틀 모델은 복원이 아니다).
QString schematicModelWording(const QString& text);

// 3D 화면 이름표용: 유적 이름만. 지정번호, 앞의 종목(「사적 」「국보 」), 괄호 설명을 모두 뺀다(사용자 지시 2026-10-25:
// 「사적 몇 호, 국보 몇 호까지 쓰면 글자가 가려진다」). 예: "사적 경주 황룡사지" → "경주 황룡사지",
// "황룡사 9층목탑 · 대가람 (사적 제6호)" → "황룡사 9층목탑 · 대가람", "대릉원 황남대총 (높이 23m …)" → "대릉원 황남대총"
QString siteNameOnly(const QString& raw);

// 지정번호 꼴(제 + 숫자 + 호)이 남아 있으면 true. 시험과 점검용.
bool containsDesignationNumber(const QString& text);

}  // namespace strata3d
