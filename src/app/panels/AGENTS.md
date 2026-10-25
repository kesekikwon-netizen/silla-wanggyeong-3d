# panels

**Parent context:** `../AGENTS.md`
**Generated:** 2026-10-25 · **Updated:** 2026-10-25

## Purpose
설계도 v2(`docs/BLUEPRINT.md` 5장)의 화면 패널. 값은 엔진이 계산하고 패널은 보여 주기만 한다.

## Key Files
| File | Description |
|------|-------------|
| `RelationPanel.{h,cpp}` | 관계 표(5.5): 구역 판정 줄(`zones_`: 분류마다 「안」/「밖, 가장 가까운 곳 n m」), 표(유산·종목·거리·방위·높이차·지형 단위·방리 칸·시대·위치 등급), 자료 한계 노트. 줄 선택 → `heritageChosen(name)` |

## For AI Agents

### Working In This Directory
- 입력은 `strata3d::RelationTable` 같은 엔진 결과형 하나. 패널 안에서 거리·판정을 다시 계산하지 않는다.
- 추정 값에는 등급 글자를 붙인다(예: 방리 칸 「n칸 (C)」). 자료가 없으면 빈 칸 대신 「자료 없음」과 사유.
- 새 패널은 `CMakeLists.txt` 의 `strata-3d` 소스 목록에 `.h`·`.cpp` 를 함께 넣는다.

### Testing Requirements
엔진 쪽 값은 `tests/test_relations.cpp`. 패널은 `--window-shot` 캡처로 글자 잘림·정렬을 확인(노트 잘림은 알려진 문제).

## Dependencies

### Internal
- `../../engine/RelationEngine.h`, KaTheme.

## Manual Notes
