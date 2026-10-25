# app

**Parent context:** `../AGENTS.md`
**Generated:** 2026-10-25 · **Updated:** 2026-10-25

## Purpose
`strata-3d.exe`. hgis 의 KaTheme(paperLook, 「조용한 도판」)로 꾸민 Qt 위젯 창, OpenGL 3.3 Core 지형 뷰포트와 HDR 후처리, 입체지형 도면 합성, Strata 가 부르는 로컬 파이프 서버.

## Key Files
| File | Description |
|------|-------------|
| `main.cpp` | 명령줄(`--open-survey`, `--bangri-preset`, `--mode`, `--window-shot`, `--smoke-quit/-image/-report`, `--sheet-out`, `--trench-clicks`, `--no-satellite` …), 스모크 종료 처리. 표시 이름 「Strata 3D」, org/app 이름 `strata-3d` |
| `MainWindow.*` | 창·도크 배치, 자료 받기 워커, 조사 열기, 평가, 관계 표 계산(`computeSurveyRelations`), 트렌치 그리기 |
| `KaGlTerrainViewport.*` | R32F 높이 텍스처 + 33×33 틀 인스턴싱 지형, 건물·유적 메시, 등급 범례 |
| `TerrainShaders.*`, `PostShaders.*`, `HdrPipeline.*` | GLSL. RGBA16F 장면 → Nishita 하늘 → GTAO(Blender eevee 이식) → 블러 → AgX 합성 |
| `SheetComposer.*` | 3072 px 캡처 + 제목·축척자·방위·범례·출처·좌표계·평가 요약 → PNG / A3 PDF, 방리 5패널 도면 |
| `ProfilePlot.*` | 트렌치 지표면 거리-표고 단면 |
| `BangriDockPanel.*` | 방리 격자·유적 도식 모델 켜고 끄기, 3D 선·격자 그리기 설정 |
| `StrataBridge.*` | 파이프 서버 `strata-3d-bridge-v1`(hello/state/openSurvey/evaluate/profile/renderSheet/quit), 한 번에 요청 하나 |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| `panels/` | 설계도 v2 화면 패널(관계 표 등) (see `panels/AGENTS.md`) |

## For AI Agents

### Working In This Directory
- 색·글꼴·간격은 KaTheme 토큰(hgis `KaThemePaper`)만 쓴다. 하드코딩 색을 넣지 않는다(종이 `#F5F4ED`, 먹 `#141413`, 점토 `#B5573A` 는 토큰 값의 참고일 뿐).
- 계산은 엔진에 두고 여기서는 워커 실행·표시만. 긴 작업은 `QtConcurrent`, UI 갱신은 메인 스레드 시그널로.
- 화면 문구: 지정번호 쓰지 않음, 도식 모델은 「도식」(「복원」 금지), 추정은 등급과 함께.
- 스모크 경로(`main.cpp` 의 `finish()`)는 한 번만 끝나야 한다. 새 비동기 단계를 넣으면 준비 신호를 기다리게 묶는다.

### Testing Requirements
빌드 후 `--open-survey data\samples\gyeongju_practice\gyeongju_practice_synthetic.gpkg --bangri-preset gyeongju_hwangnyongsa --mode 3 --window-shot <png>` 로 전체 창을 캡처해 눈으로 확인. 파이프·도면은 `strata_bridge_e2e`.

### Common Patterns
- 도크는 탭으로 묶는다(관계 표 ↔ 단면). 패널은 `setBusy/setTable/clear` 세 상태.
- 셰이더 문자열은 `.cpp` 안의 raw 문자열, 헤더는 선언만.

## Dependencies

### Internal
- `../engine` 전체, `../bridge`, `${HGIS_ROOT}/src/app/KaTheme*.cpp`, `${HGIS_ROOT}/data/theme/icons.qrc`, 생성물 `build/generated/ka-hgis.qss.inc`.

### External
- Qt Widgets/OpenGLWidgets/OpenGL/Network/Concurrent, Windows `dwmapi`.

## Manual Notes
