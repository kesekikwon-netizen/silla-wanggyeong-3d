# tests

**Parent context:** `../AGENTS.md`
**Generated:** 2026-10-25 · **Updated:** 2026-10-25

## Purpose
QtTest 시험. 실제 경주 좌표·실제 DEM·실제 서비스·실제 앱으로만 검증한다. 네트워크가 막히면 건너뛰지 않고 실패한다.

## Key Files
| File | ctest 이름 | Description |
|------|------|-------------|
| `test_strata_3d.cpp` | `strata_3d_engine` | Copernicus COG DEM, LOD 틈·겹침, BVH 전수 대조, AgX 기준표, hgis 표본 조사 판독 |
| `test_bangri_grid.cpp` | `bangri_grid_unit` | 방리 격자·평가 단위 시험 |
| `test_heritage_trust.cpp` | `heritage_trust` | 지정번호 제거·종목 정리, 도식 등급·문구, 매니페스트 311개 전수 |
| `test_cross_check.cpp` | `cross_check` | spca ↔ KHS 위치 대조(예: 황룡사지 278 m 불일치, 동궁과 월지 ≈12 m) |
| `test_relations.cpp` | `relations` | 관계 표: 거리·방위(황룡사지 492.68 m / 264.6°), 방리 칸, 이름 없는 지물 숨김, 트렌치, 경사 = `DemAnalyzer` |
| `test_strata_bridge.cpp` | `strata_bridge_e2e` | 실제 `strata-3d.exe` 와 파이프 전 주기 |
| `data/agx_reference.csv` | — | OCIO 2.6.0 으로 낸 Blender AgX 기준 1006색(`scripts/gen_agx_reference.py`) |

## For AI Agents

### Working In This Directory
- **정답(oracle)은 엔진과 독립적으로** 얻는다: GDAL/ogr 명령줄(`gdallocationinfo`, `ogrinfo -dialect SQLite` 의 ST_Intersects/ST_Distance), 직접 계산, 공개 API 원값. 엔진 출력으로 정답을 만들지 않는다.
- `QSKIP`, 가짜 데이터, 엔진을 그대로 베낀 기댓값은 금지.
- 새 시험 파일은 `CMakeLists.txt` 에 `add_executable` + `add_test` + `TIMEOUT` 을 추가한다. hgis 표본·데이터 경로는 `S3D_HGIS_ROOT`/`S3D_SOURCE_DIR` 정의로 받는다.
- Strata 캐시(SHP)를 쓰는 시험은 이 PC 의 `%LOCALAPPDATA%\ka-hgis\...` 에 의존한다는 것을 시험 메시지에 남긴다.

### Testing Requirements
```powershell
. .\scripts\dev-env.ps1; cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure        # -R relations 처럼 하나만도
```

## Dependencies

### Internal
- `strata_3d_engine`, `../src/bridge/StrataBridgeClient.cpp`, `../../hgis/samples/demo_survey/demo.gpkg`.

### External
- Qt Test, 실제 네트워크 서비스.

## Manual Notes
