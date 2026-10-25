# bridge

**Parent context:** `../AGENTS.md`
**Generated:** 2026-10-25 · **Updated:** 2026-10-25

## Purpose
Strata(hgis) ↔ strata-3d 로컬 파이프 클라이언트. Qt Network 만 쓰므로 hgis 쪽에서도 그대로 링크된다. 서버는 `../app/StrataBridge.*`.

## Key Files
| File | Description |
|------|-------------|
| `StrataBridgeClient.{h,cpp}` | 줄바꿈으로 끝나는 JSON 한 줄 요청·응답. 규격은 `docs/bridge-protocol.md`(v1) |

## For AI Agents

### Working In This Directory
- hgis 에 사본(`src/core/Strata3dLink.*` 계열)이 있다. 프로토콜을 바꾸면 `docs/bridge-protocol.md`, 서버(`app/StrataBridge.*`), hgis 쪽을 함께 맞춘다. 반대 방향 명령(`nearby`, `paths`)은 v1 확장 합의 전에는 넣지 않는다.
- 엔진·위젯 의존을 넣지 않는다.

### Testing Requirements
`strata_bridge_e2e`(`tests/test_strata_bridge.cpp`): 실제 `strata-3d.exe` 를 띄워 열기→평가→단면→도면→상태→종료, 조사 원본 해시 불변 확인.

## Dependencies

### External
- Qt Core/Network(`QLocalSocket`).

## Manual Notes
