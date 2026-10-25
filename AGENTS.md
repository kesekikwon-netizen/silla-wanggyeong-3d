# strata-3d (Strata 3D)

**Generated:** 2026-10-25 · **Updated:** 2026-10-25

## Purpose
경주 문화유산을 3D 지형 위에 보여 주고, 유적의 입지·서로의 관계·내가 들어가는 시굴 구역과의 관계를 자문회의·학술발표에서 설명하는 데스크톱 앱(`strata-3d.exe`)과 엔진(`strata_3d_engine`).
C++20 / Qt 6 Widgets / OpenGL 3.3 Core / GDAL·OGR·PROJ(OSGeo4W). 웹·Node·Three.js 는 앱에 쓰지 않는다(Node 는 수집 스크립트에만).

제품 정의: **Strata(ka-hgis, `../hgis`) = 2D 전용**, 자료 수집(지적도·수치지형도·주변유적 도형·토양도·지질도 등)과 문화재 2D 도면이 이미 다 된다. **Strata 3D = 3D 전용**, 같은 디자인(KaTheme 를 hgis 에서 그대로 컴파일)으로 Strata 가 받아 둔 자료를 3D 로 올리고 3D 에서만 되는 것만 더한다.

## Key Files
| File | Description |
|------|-------------|
| `CMakeLists.txt` | 엔진(STATIC)·앱·시험 7개. `HGIS_ROOT`(기본 `../hgis`)의 KaTheme·`src/core/DemAnalyzer.cpp` 를 같이 컴파일 |
| `CMakePresets.json` | `vs` 구성(VS2022 x64, `build/`), `release`/`debug` 빌드·시험 프리셋 |
| `README.md` | 되는 것·한계·빌드 명령(사용자용, 시험으로 확인한 것만 적는다) |
| `CLAUDE_CODE_MASTER_PLAN.md` | 초기 작업 명령서. 1장 작업 규칙(hgis `CLAUDE.md` 준수, 가짜 데이터 시험 금지, 스모크) |
| `SILLA_BANGRI_3D_PLAN.md` | 신라 방리구획 3D 복원·검증 지시서(`Bangri*` 코드의 근거) |
| `ANTIGRAVITY_DESKTOP_HANDOFF.md` | 다른 도구용 이어하기 안내(과거 기록) |
| `THIRD_PARTY_NOTICES.md` | 제3자 고지(Blender 유래 상수·셰이더 등) |
| `run_strata3d.cmd`, `run_strata_3d.bat` | 빌드된 앱 실행 바로가기 |
| `strata3d-도면.{png,pdf}` | 예전에 내보낸 입체지형 도면 표본 |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| `src/` | 엔진·앱·연동 클라이언트 소스 (see `src/AGENTS.md`) |
| `tests/` | QtTest 시험 7개, 실제 네트워크·실제 앱 사용 (see `tests/AGENTS.md`) |
| `scripts/` | 개발 환경·빌드·스모크·자료 생성·수집 스크립트 (see `scripts/AGENTS.md`) |
| `docs/` | 설계도, Strata 재사용 지도, 연동 규격, 수집 기록 (see `docs/AGENTS.md`) |
| `data/` | 앱 옆에 복사되는 자료: AgX LUT, 유적 도식 모델, 연습용 합성 조사 (see `data/AGENTS.md`) |
| `cmake/` | `EmbedQss.cmake` — hgis `ka-hgis.qss` 를 8 KiB 조각 `.inc` 로 내장(MSVC 문자열 한도) |
| `artifacts/`, `build/`, `out/`, `research/` | git 제외. 역공학 원본, 빌드, 캡처, DBpia 원답(저작권) |

## For AI Agents

### Working In This Directory
- **커밋은 사용자가 요청할 때만.** `master` 에서 커밋해야 하면 먼저 브랜치를 만든다.
- **Strata 재사용이 먼저다.** 새로 만들기 전에 `docs/STRATA_REUSE.md` 를 보고 hgis 에 있는 것(QGIS 무관 파일은 같이 컴파일, 나머지는 Strata 가 남긴 파일·캐시·파이프로)을 쓴다. 2D 수집·2D 도면은 다시 만들지 않는다.
- **비밀값을 읽거나 출력하지 않는다.** `%LOCALAPPDATA%\ka-hgis\ka-hgis\` 의 `ngii-account.ini`, `heritage-account.ini`(username/password_dpapi), VWorld 키 INI 는 키 이름만 다룬다.
- `/research/`(DBpia 원답, Chrome 프로필)는 git 에 넣지 않는다. 정리본만 `docs/research/` 에.
- 화면·도면에 나가는 유산 이름은 `displayHeritageName()` 을 거친다(지정번호 미표기, 2021-11-19 이후). 화면에 올리는 지물마다 근거 등급(`EvidenceTier` A 실측/B 근거 복원/C 추정/D 도식)을 단다.
- 좌표: 작업 좌표계 EPSG:5187(동부원점), 출처 대조 거리는 EPSG:5179. 투영식을 직접 쓰지 말고 `CrsTransformer`(OSR).
- 사용자는 고고학 전문가다. 설계도(`docs/BLUEPRINT.md`)에 다음 할 일이 있으면 되묻지 말고 진행한다. 눈으로 보거나 시험한 것만 「된다」고 말한다.
- MSVC `/W4 /permissive- /utf-8`. 소스 주석과 문서는 한국어, 짧은 평서문으로 「왜」를 적는 기존 문체를 따른다.

### Testing Requirements
```powershell
. .\scripts\dev-env.ps1                                   # PATH/GDAL/PROJ (같은 셸에서)
cmake --build build --config Release --parallel           # 전부. build-now.ps1 은 strata-3d·test_strata_3d 두 개만 짓는다
ctest --test-dir build -C Release --output-on-failure     # 7개
build\Release\strata-3d.exe --open-survey data\samples\gyeongju_practice\gyeongju_practice_synthetic.gpkg --bangri-preset gyeongju_hwangnyongsa --mode 3 --window-shot out\shot.png
```
시험은 실제 네트워크(Copernicus, VWorld, Overpass, 국가유산청)를 쓰고 막히면 실패한다(건너뛰지 않음). 화면 변경은 `--window-shot`/`--smoke-image` 캡처를 열어 눈으로 확인한다.

### Common Patterns
- 엔진(`src/engine`)에는 위젯 코드가 없다. 무거운 계산은 `QtConcurrent` 워커, 결과는 시그널로 UI 에.
- 실패는 `bool` + `QString* err` 로 사유와 함께 돌려준다. 가짜 값으로 메우지 않는다(「자료에 없음」≠「없음」).
- 원본 자료(Strata 조사 .gpkg, SHP 캐시)는 읽기 전용으로만 연다.

## Dependencies

### Internal
- `../hgis` (Strata): `src/app/KaTheme*.cpp`, `data/theme/`, `src/core/DemAnalyzer.*`, 시험 표본 `samples/demo_survey/demo.gpkg`, 조사 스키마 `data/schemas/ka_hgis_layers.yaml`, 연동 짝 `src/core/Strata3dLink.*`.

### External
- Qt 6 (Core Gui Widgets OpenGL OpenGLWidgets Network Concurrent Test), GDAL/OGR/PROJ — OSGeo4W(`C:\OSGeo4W`) 의 것.
- 데이터 서비스: Copernicus GLO-30 COG, VWorld WMTS(Google 은 약관상 쓰지 않음), OSM Overpass, 국가유산청 Open API·국가유산 공간정보(spca).

## Manual Notes
