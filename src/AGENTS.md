# src

**Parent context:** `../AGENTS.md`
**Generated:** 2026-10-25 · **Updated:** 2026-10-25

## Purpose
Strata 3D 소스. 세 갈래로 나뉜다: UI 없는 계산 엔진(`engine/` → `strata_3d_engine` STATIC), Qt 위젯·OpenGL 앱(`app/` → `strata-3d.exe`), Strata 와 말하는 파이프 클라이언트(`bridge/`).

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| `engine/` | DEM·위성·지물 제공자, 지형 LOD·BVH, 고고학 평가·관계 표, 방리·고분 도식, 근거 등급·표기 (see `engine/AGENTS.md`) |
| `app/` | 메인 창, GL 지형 뷰포트, HDR 후처리, 도면, Strata 파이프 서버, 패널 (see `app/AGENTS.md`) |
| `bridge/` | `StrataBridgeClient` — hgis 쪽에도 사본이 있는 파이프 클라이언트 (see `bridge/AGENTS.md`) |

## For AI Agents

### Working In This Directory
- 의존 방향은 `app → engine`, `app → bridge` 만. 엔진은 `QWidget`/GL 을 포함하지 않는다(시험이 엔진만 링크한다).
- 새 `.cpp` 는 `CMakeLists.txt` 의 해당 타깃 목록에 직접 추가한다(glob 없음).
- hgis 파일을 같이 컴파일할 때는 사본을 만들지 말고 `${HGIS_ROOT}/...` 경로로 목록에 넣는다.

### Testing Requirements
엔진 변경은 `tests/` 의 해당 시험, 앱 변경은 빌드 + `--window-shot` 캡처 확인, 파이프 변경은 `strata_bridge_e2e`.

## Dependencies

### Internal
- `../../hgis/src/app` (KaTheme), `../../hgis/src/core` (DemAnalyzer).

### External
- Qt 6, GDAL/OGR/PROJ.

## Manual Notes
