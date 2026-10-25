# research

**Parent context:** `../AGENTS.md`
**Generated:** 2026-10-25 · **Updated:** 2026-10-25

## Purpose
연구 자료 수집 도구. 방법·판정 기준의 정본은 `docs/research/METHODS.md`.

## Key Files
| File | Description |
|------|-------------|
| `open-dbpia-chrome.cmd` | 저장소 `research\dbpia\chrome-profile` 프로필 + 원격 디버깅 포트 9222 로 Chrome 을 DBpia AI 검색 창 하나로 연다. 사용자가 직접 더블클릭(대화형 데스크톱에서 떠야 보인다) |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| `dbpia/` | CDP(puppeteer-core) 로 DBpia AI 에이전트에 묻고 답을 모으는 Node 스크립트 (see `dbpia/AGENTS.md`) |

## For AI Agents

### Working In This Directory
- 비대화형 셸에서 띄운 Chrome 은 화면에 안 보인다. 사용자에게 cmd 더블클릭을 부탁하거나 `schtasks /IT` 로 띄운다. 모니터는 하나다.
- 원답·프로필은 `/research/`(git 제외)에만 둔다.

## Manual Notes
