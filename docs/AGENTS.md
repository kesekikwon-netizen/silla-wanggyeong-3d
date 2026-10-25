# docs

**Parent context:** `../AGENTS.md`
**Generated:** 2026-10-25 · **Updated:** 2026-10-25

## Purpose
제품 설계도, Strata 재사용 판정, 연동 규격, 상수 출처, 연구 수집 기록과 역공학 증거.

## Key Files
| File | Description |
|------|-------------|
| `BLUEPRINT.md` | **설계도 v2.** 제품 정의, 시나리오 S1–S5, 근거 등급·교차검증, 화면(탐색/분석/발표/도면), 설정표, `.s3d` 프로젝트 파일, 단계, 사용자 결정, 연구 근거(12–14장) |
| `STRATA_REUSE.md` | hgis 파일별 재사용 판정((a) 같이 컴파일 / (b) ka_core 링크 불가 / (c) 결과 파일·파이프), 기능 표, 겹침, 다음 할 일 |
| `bridge-protocol.md` | Strata ↔ strata-3d 파이프 v1 정본(명령·키·단위) |
| `sky-constants.md` | Nishita 하늘 상수의 Blender 실행 파일 주소·값 |
| `dbpia_silla_bangri_9ju5sogyeong_ledger.json` | 방리 문헌 원장(`DbpiaBangriLedger` 가 읽음) |
| `dbpia_ai_browser_harvester.js` | 예전 브라우저 수집기(현재는 `scripts/research/dbpia/`) |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| `research/` | 수집 방법·결과·설계 근거·개발 기록 (see `research/AGENTS.md`) |
| `rea/` | Blender 4.2.3 REA 역공학 보고서와 증거 JSON (see `rea/AGENTS.md`) |

## For AI Agents

### Working In This Directory
- 설계를 바꾸는 결정은 `BLUEPRINT.md` 에 반영하고, 근거는 `research/` 문서의 항목 id(D01, E05, G03 …)로 가리킨다.
- Strata 에서 무엇을 가져올지 판정이 바뀌면 `STRATA_REUSE.md` 를 고친다.
- 문서에는 확인한 것과 확인 못 한 것(「근거 미확인」)을 나눠 적는다. 법정 수치(500 m 등)는 조문 칸과 함께 설정값으로 다룬다.

## Manual Notes
