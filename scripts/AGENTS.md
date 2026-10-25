# scripts

**Parent context:** `../AGENTS.md`
**Generated:** 2026-10-25 · **Updated:** 2026-10-25

## Purpose
개발 환경·빌드·스모크 캡처, 자료 생성(도식 모델·AgX 기준표·연습용 합성 조사), 자료 수집 스크립트.

## Key Files
| File | Description |
|------|-------------|
| `dev-env.ps1` | OSGeo4W 찾기(env → C:\ → D:\ → A:\), PATH·GDAL_DATA·PROJ 설정. 점 소싱(`. .\scripts\dev-env.ps1`) |
| `build-now.ps1` | 첫 구성 + `strata-3d`·`test_strata_3d` 만 빌드, `-Test` 면 ctest. 다른 시험 타깃은 짓지 않는다 |
| `shot.ps1` | 빌드 후 `--smoke-quit` 한 장 PNG(`-Cam`, `-NoBuild`) |
| `make_gyeongju_practice_survey.mjs` | Node + ogr2ogr 로 경주 연습용 **합성** 조사 .gpkg 생성(hgis 표본의 빈 스키마, 황룡사지 동남동 가상 위치) |
| `generate_gyeongju_heritage_blender.py` | Blender 4.2.3(bpy) 로 유적 도식 모델 번들 `data/models/*.s3dmesh` 생성 |
| `gen_agx_reference.py` | OCIO 로 AgX 기준표 `tests/data/agx_reference.csv` 생성 |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| `research/` | DBpia AI 에이전트 수집용 Chrome 실행·CDP 스크립트 (see `research/AGENTS.md`) |

## For AI Agents

### Working In This Directory
- OSGeo4W Python 은 이 PC 에서 GDAL 바인딩이 깨져 있다. GDAL 작업은 Node + `ogr2ogr`/`gdaltransform` 명령줄로 하고, 파이프 입력은 PowerShell 대신 bash 로 넘긴다(인코딩 손상).
- 합성 자료는 모든 도형의 비고에 「합성」을 남기고 실제 유적·조사로 보이게 만들지 않는다.

## Dependencies

### External
- PowerShell 5.1, Node, OSGeo4W GDAL 명령줄, Blender 4.2.3, OpenColorIO.

## Manual Notes
