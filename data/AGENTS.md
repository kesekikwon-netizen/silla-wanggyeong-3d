# data

**Parent context:** `../AGENTS.md`
**Generated:** 2026-10-25 · **Updated:** 2026-10-25

## Purpose
실행 자료. 빌드 후 `copy_directory` 로 `strata-3d.exe`·`test_strata_3d.exe` 옆 `data/` 에 통째로 복사된다.

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| `colormanagement/` | `AgX_Base_sRGB.cube`(57³) — Blender 4.2.3 AgX LUT, `AgxView` 가 읽음 |
| `models/` | 경주 유적 도식 모델 번들·매니페스트 (see `models/AGENTS.md`) |
| `samples/gyeongju_practice/` | 경주 연습용 **합성** 조사 .gpkg 와 README(실제 유적·조사가 아님) |

## For AI Agents

### Working In This Directory
- 여기 넣은 파일은 모두 배포물 옆에 복사된다. 큰 파일·비밀·원답을 넣지 않는다.
- 연습용 조사는 `scripts/make_gyeongju_practice_survey.mjs` 로 다시 만든다. 손으로 고치지 않는다.

## Manual Notes
