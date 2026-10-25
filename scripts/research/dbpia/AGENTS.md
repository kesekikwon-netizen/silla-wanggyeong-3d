# dbpia

**Parent context:** `../AGENTS.md`
**Generated:** 2026-10-25 · **Updated:** 2026-10-25

## Purpose
로그인된 DBpia AI 탭(CDP 포트 9222)에서 쿠키만으로 에이전트 API 를 불러 질문하고, 답·인용(`<CIT NODEID_n>`, `references[]`)을 저장·정리한다. 정리 결과는 `docs/research/*.md` 로 간다.

## Key Files
| File | Description |
|------|-------------|
| `apiseq.js` | **주 도구.** 한 창·한 대화에 차례로 묻기(약 22 s/질문). 답마다 탭 새로고침, 에이전트가 새 대화를 권하거나 `usage_ratio ≥ 90`(%) 이면 같은 탭에서 새 대화로 옮김 |
| `questions.json` | 질문 목록(id 접두 S/W/M/F/D/E/G) |
| `digest.js` | 수집 폴더 → md. 인용 표시를 펼치고, 인용 없는 수치 줄과 대화 전체 references 에 없는 인용(유령 인용)을 표시 |
| `followup.js` | UI 입력판 이어 묻기(예전 방식, 느림) |
| `batch.js` | 병렬·질문마다 새 대화(예전 방식). **사용자 지시로 지금은 쓰지 않음** |
| `fetchconv.js`, `dump.js` | 이미 있는 대화를 API 로 저장 |
| `ask.js`, `capture_api.js` | 단발 질문, CDP 네트워크 캡처(API 구조 파악용) |
| `opentab.js`, `pagetext.js`, `shot.js` | 탭 열기·본문 텍스트·스크린샷 보조 |
| `khs3d.js`, `khs3d_list.js` | 국가유산청 디지털 3D 페이지 조사 |

## For AI Agents

### Working In This Directory
- **창 하나, 대화 하나.** 새 탭·새 창을 만들지 않는다. 같은 대화에는 동시에 보내지 않는다(문맥이 섞인다). 에이전트가 새 대화를 권할 때까지 같은 대화에 묻는다.
- 질문은 전문가 수준으로. DBpia 에는 **학술 사실**(발굴·유구·방리·고분·국내 조사 관행)만 묻는다. 3D 기술(표현·규격·알고리즘·렌더링)은 DBpia 가 최신을 담지 못하므로 2024–2026년 국제 1차 출처로 조사해 `docs/research/TECH_2026.md` 에 둔다(`docs/research/METHODS.md` 8장). 기초 질문 금지.
- `digest.js <harvest> <out.md> [기준연도=2021]` 은 인용을 `[nodeId, 연도]` 로 펼치고 기준 이전 문헌만 인용한 줄을 센다.
- 실행: `node apiseq.js <research/dbpia/node/node_modules/puppeteer-core 경로> questions.json <research/dbpia/harvest> <id,...>`. 산출은 `/research/dbpia/harvest/`(git 제외, 저작권).
- 답의 수치는 인용과 대조하기 전에는 근거로 쓰지 않는다(`docs/research/FINDINGS.md` 판정 기준).

### Testing Requirements
`node --check <file>` 로 문법 확인. 실제 동작은 로그인된 탭이 있어야 한다.

## Dependencies

### External
- Node, puppeteer-core(`research/dbpia/node/node_modules`), Chrome CDP. 엔드포인트 `https://agent.dbpia.co.kr/vetter_conversations/`(생성·조회), `…/chat_message`(SSE).

## Manual Notes
