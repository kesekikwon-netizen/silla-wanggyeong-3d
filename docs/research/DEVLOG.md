# 개발 기록 (설계도 v2 단계별)

> 각 단계: 무엇을 바꿨나 · 어떻게 확인했나(시험·캡처) · 남은 것. 확인하지 않은 것은 「미확인」으로 쓴다.

## 2026-10-25 M0 — 경주 연습용 조사(합성)

- `scripts/make_gyeongju_practice_survey.mjs` → `data/samples/gyeongju_practice/gyeongju_practice_synthetic.gpkg`
- Strata 7개 레이어 빈 틀(demo.gpkg) + EPSG:5187, 황룡사지 동남동 약 350 m 가상 위치(중심 221393.0, 359893.6). 조사구역 100×80 m, 트렌치 4개(2×60 m), 유구·담장·단면선·기준점 2·유물 1. 모든 속성에 「합성」.
- 확인: `ogrinfo -so -al` — 7개 레이어, 개수, 범위, `ID["EPSG",5187]`.
- 막힌 것: OSGeo4W Python 의 `osgeo` 가 이 셸에서 DLL 을 못 찾음(`_gdal`) → Python 대신 Node + `ogr2ogr` 로 바꿈. PowerShell 파이프로 `gdaltransform` 에 넣으면 입력이 깨짐 → bash 로 실행.

## 2026-10-25 M1-1·2 — 지정번호 표기 정리, 근거 등급(D 도식)

- 새 엔진: `src/engine/HeritageNaming.*` (`displayHeritageName`, `designationKind`, `schematicModelWording`), `src/engine/EvidenceTier.*` (A~D, 등급별 칠하기).
- `BangriTerrainValidator`: 카탈로그 이름·종목·부제를 정리, 메타에 `tier`(기본 D), 번들 3D 메쉬 정점 색을 D 스타일(채도 90 % 제거, 종이색 40 %)로.
- 화면: 왼쪽 위 「3D 실측 복원 안내」 배너 → 「자료 등급」 범례(A~D). 이름표 부제 앞에 「3D 도식(D)」. 패널 문구의 지정번호·「실제 모양 그대로 복원」 삭제.
- 시험 `tests/test_heritage_trust.cpp`(ctest `heritage_trust`): 정리 규칙 9가지, **번들 311곳 전체** 이름에 지정번호 없음, 카탈로그 300곳 이상·전부 D·부제에 「복원」 없음, D 색이 회색(채널 차 < 0.08), 실측은 색 불변. 16개 통과.
- 캡처: `out/m1_tier_window.png` (위성 보기, 황룡사 프리셋).
- 남은 것: 반투명 D(블렌딩+깊이 정렬)는 2단계. 등급별 칠하기 강도는 설정(M2)으로.

## 2026-10-25 M1-3 — 시대·출처 대조 (국가유산 공간정보 spca)

- 새 엔진: `src/engine/KhsSpcaProvider.*` (spca.do 읽기, 7일 캐시, 이름 공백 무시 찾기), `src/engine/CrossCheck.*` (두 출처 점을 EPSG:5179 로 옮겨 거리 → 일치 ≤10 m / 근접 ≤50 m / 확인 필요, 기준값은 설정 가능).
- 좌표계 확인: spca `cnX/cnY` = **EPSG:5179** (동궁과 월지: 국가유산청 점과 11.98 m).
- **발견: 경주 황룡사지 — 국가유산청 점(129.2307733, 35.83572849)과 공간정보 중심점이 278.1 m 다름** → 「확인 필요」. 지정면적 390,418 ㎡ 의 넓은 유적이라 대표점 기준 차이일 수 있으나, 어느 쪽이 맞는지는 미확인.
- 원자료 품질: 「경주 분황사지」 시대 = 「생성시대」(공간정보 원자료 그대로).
- 시험 `tests/test_cross_check.cpp`(ctest `cross_check`, 실제 네트워크): 경주 사적 ≥60건(오늘 74), 시대 값, 동궁 11.98±0.5 m 근접, 황룡사지 278.1±1 m 확인 필요, 기준값 바꾸면 판정 바뀜. 통과.

## 2026-10-25 M1-4·5 — 관계 표 (엔진 + 화면)

- 새 엔진: `src/engine/RelationEngine.*` — 조사구역(없으면 첫 트렌치) ↔ 반경 안 유산: 경계 최단거리·방위·높이차(기존 평가 엔진), **모든 유산 시선**, 유산 자리 Weiss 지형 분류·같음 여부, **추정 방리 칸 차이**(체비쇼프, 등급 C), **시대**(spca 11·12·13 이름 일치). 이름 없는 지물은 기본으로 빼고 개수만 적는다. 표 아래 한계 문장(30 m DEM 시선은 권장 20 m 이하보다 거침 등)을 함께 준다.
- `Archaeology3dEvaluator::terrainMetricsAt` 공개.
- 화면: 새 파일 `src/app/panels/RelationPanel.*`, 아래 칸 「관계 표」(시굴 트렌치 단면과 탭). 조사를 열면 워커에서 계산. 명령줄 `--open-survey <gpkg>`(스모크는 관계 표까지 기다림).
- 시험 `tests/test_relations.cpp`(ctest `relations`, 실제 Copernicus·국가유산청·spca):
  - 연습 조사구역 → 경주 황룡사지 **492.68 m / 264.6°** (gdaltransform 으로 따로 계산한 값, 허용 1 m / 0.5°), 시대 신라시대
  - → 경주 동궁과 월지 **888.38 m**, 통일신라시대, 방리 칸이 황룡사지보다 더 멂
  - 방리 칸 계산 = 격자 생성기의 칸 번호(모든 칸)
  - 이름 없는 지물 숨김(평가 엔진이 채우는 「(이름 없음)」 포함) — 처음 화면 캡처에서 걸러지지 않은 것을 보고 고침
- 실제 값(오늘): 13곳 중 시선 「가림」 4곳(낭산 쪽 3곳 + 동궁과 월지 802 m 지점). 동궁과 월지는 평지인데 가림 → 30 m DEM 굴곡 영향 가능성, 미확인.
- 캡처: `out/m1_relations_window.png` (스모크 PASS, 관계 17곳).
- 남은 것: 아래 칸 높이가 작아 한계 문장이 한 줄만 보임(M3 배치에서). 줄을 고르면 3D 에서 두 곳을 잇는 선(설계 5.5)은 아직. 하천 거리 열은 아직(OSM 하천 미수집).

## 시험 현황 (2026-10-25)

`heritage_trust` 16 · `cross_check` · `relations` · 기존 `strata_3d_engine` · `bangri_grid_unit` 모두 통과(ctest).

## 2026-10-25 독립 검토 반영 (code-reviewer, 9건)

| # | 문제 | 고침 | 확인 |
|---|---|---|---|
| 1 | spca 캐시 7일 뒤 다시 쓰지 않음, 깨진 캐시면 실패 | 받으면 늘 다시 씀, 못 받으면 오래된 캐시 사용, 캐시 못 읽으면 네트워크 | cross_check 통과 |
| 2 | 조사 열기 실패 시 스모크 180초 대기 | `surveyOpened(false)` → 즉시 실패 | 없는 파일로 실행: **1초에 FAIL(exit 1)** |
| 3 | spca 3종 × 120초 | 30초 | — |
| 4 | 실패 뒤 PASS 로 덮어쓰기 가능 | `finish` 한 번만, 실패 처리 뒤 `return` | — |
| 5 | 지물 id 로 대표점 찾기(못 찾으면 조사구역 자리로 계산) | `NearbyItem::point` 를 평가 엔진이 채움 | relations·strata_3d_engine 통과 |
| 6 | 「제98호분」 고분 번호까지 지움 | `호(?![가-힣])` | heritage_trust 행 추가 |
| 7 | 괄호 안 「1,245」 쉼표가 「·」로 바뀜 | 뺀 조각이 있을 때만 다시 붙임 | heritage_trust 행 추가 |
| 8 | 선 트렌치를 면으로/조사 중심으로 | 3점 이상 면, 2점이면 선 가운데 점 | — |
| 9 | XML 태그 이름 수명 | 문자열로 복사 | — |

전체 ctest 6개 통과(heritage_trust, relations, bangri_grid_unit, cross_check, strata_bridge_e2e, strata_3d_engine).

## 2026-10-25 제품 정의 확정 · Strata 재사용 시작

- 사용자 결정: **Strata = 2D 전용, Strata 3D = 3D 전용**(같은 디자인, 3D 만 더함). 창 제목·표시 이름 「Strata 3D」(설정 경로 보존 위해 내부 이름 strata-3d 유지). 설계도 맨 앞에 정의.
- Strata 코드 조사 → `docs/STRATA_REUSE.md`(빌드 구조, 파일별 재사용 판정 a/b/c, 기능 표, 겹침 표, 3D 전용 목록).
- 첫 재사용: `${HGIS_ROOT}/src/core/DemAnalyzer.cpp` 를 `strata_3d_engine` 에 같이 컴파일. 평가 엔진의 경사는 이제 Strata `DemAnalyzer::slopeDegrees`(Horn). 시험 `relations::slopeComesFromStrataDemAnalyzer` — 예전 3D 식과 수백 칸에서 0.01° 이내 일치. 전체 ctest 6개 통과.
- 다음(STRATA_REUSE 5장): `Terrain3dService`(위성 XYZ URI 하나로)·`SurveyContour*`(측량점 DEM) 같이 컴파일, 보호구역 안/밖은 Strata 가 받은 **보호구역 폴리곤**(지정유산 ZIP)으로 판정, 수치지형도 건물·등고선 SHP 를 OSM 보다 우선, 방위는 보고서용 8방위로 맞춤.

## 2026-10-25 M1-6 — 가시권 삭제 · 조사에 딸린 자료 읽기 · 구역 판정

- **가시권·시선 삭제(사용자 결정)**: "가시권은 하지마라, 추정 아닌가". 옛 지형·건물 높이·수목을 모르는 채 낸 값은 추정이라 근거가 될 수 없다. `Archaeology3dEvaluator::lineOfSight`·`VisibilityItem`, 관계 표 「시선」 열, 평가 패널 시선 표, 평가 JSON `visibility`(파이프 규격 포함) 삭제. `BASIS_3D.md` G05·우선순위 4, `TECH_2026.md` 3.2 에 「제외」 표시.
- **조사 하나에 딸린 자료**: Strata 는 조사구역을 그린 뒤 그 둘레로 자료를 받는다(사용자 설명). 새 `StrataProjectLayers`: 조사 `.gpkg` 의 `qgis_projects`(QGIS 가 `.qgz` 바이트를 **16진수 글자**로 저장 — 경주시.gpkg 로 확인) → `/vsimem/`+`/vsizip/`(확장자 `.zip` 이어야 인식) → `<maplayer>` 출처·`ka_hgis/*` 속성, 프로젝트 속성 `ka_hgis/survey_dir`. 없으면 옆의 `.qgz`/`.qgs`. 조사를 열면 그 조사의 `ka_hgis/heritage_dataset` 레이어만 읽는다(캐시 전체를 뒤지지 않음).
- **읽기 결함 수정**: 다중 다각형의 첫 조각만 읽던 것 → 조각마다. 구멍(내부 고리)을 버리던 것 → 보존(국가지정유산보호구역 25조각 중 17개가 구멍 있음). 중심점이 창 밖인 큰 구역을 버리던 것 → 외곽 상자로 판정. GPKG 에 CP949 열기 옵션을 주던 것 → SHP 에만.
- **현상변경허용기준**: 같은 구역 도형이 유산마다 따로 등록된다(경주 조사: 겹치는 기록 182건 = 도형 4개, 기준 유산 91건, 구역 1·2-4·2-5·3구역). 이름을 「국가유산명」+`구역명` 으로 잡아 서로 다른 기준을 합치지 않게 했고, 화면은 「안 — 1구역, 2-4구역, 2-5구역, 3구역 · 기준 유산 91건」으로 묶는다.
- **시험 `strata_project`**(실제 경주 조사, 2026-10-25 사용자가 Strata 에서 그리고 저장): 프로젝트에서 주변유적 12종 확인, 구역 6분류의 겹침 수·바깥 최단거리·1 km 안 수가 GDAL 명령줄(`ogr2ogr -explodecollections -t_srs EPSG:5187` + `ogrinfo -dialect SQLite` ST_Intersects/ST_Distance)로 따로 계산한 값과 모두 일치(최단거리 ±0.01 m). 발굴사업허가구역 512 기록 중 2건은 사업명·유산코드·도형이 같은 중복 등록이라 510.

| 분류 | 겹침 | 바깥 최단 | 1 km 안 |
|---|---|---|---|
| 국가지정유산보호구역 | 0 | 324.299 m | 5 |
| 시도지정유산보호구역 | 0 | 23.708 m | 5 |
| 현상변경허용기준 | 182(유산 91건, 4구역) | 24.177 m | 1670 |
| 매장유산유존지역 | 2 | 12.651 m | 144 |
| 발굴사업허가구역 | 4 | 6.311 m | 510 |
| 지표사업허가구역 | 1 | 6.023 m | 48 |

- Strata 쪽에서 발견한 문제(키 평문 저장, VWorld/Google 약관, 받는 범위 불일치 등)는 사용자 지시대로 바탕화면 `Strata_GIS앱_고칠점.txt` 에 따로 적었다.

## 2026-10-25 설계도 v3 · 0단계 첫 확인

- 사용자 방향: Strata 2D 를 바탕으로 3D 전용 앱. 2D 받기·도면 방식과 아이콘·배치는 그대로, 화면은 3D 만, 경주 먼저. 이름표는 유적 이름만(`siteNameOnly`), 겹침 없음(8방향 자리 찾기), Ctrl+휠·Ctrl+=/−/0 크기. `heritage_trust` 19개 통과, 경주 캡처로 확인.
- 설계도 v3: 아티팩트 https://claude.ai/artifact/PJQoQYYuVBZoCxajNq84M8 , 사본 `docs/PLAN_3D_v3.html`. 별도 검토(판정 REVISE, 막음 2·큼 8·작음 다수)를 모두 반영, 11장에 기록. 10장 결정은 사용자 「계속 진행」으로 추천대로.
- Strata 화면 목록: `docs/research/tech2026/strata_ui.md`.
- 0단계 첫 확인 `spikes/ka_core_link`: hgis 가 빌드한 `ka_core.lib`(IMPORTED) + qgis_core/gui + gdal_i 를 링크한 별도 실행 파일이 Strata 의 `SurveyStorage::readEmbedded` 로 경주시.gpkg 를 열어 레이어 20개 모두 유효. 실행에는 hgis `scripts/dev-env.ps1` 환경이 필요(없으면 0xC0000135, DLL 없음).

## 2026-10-25 0단계 A — Strata 리본·상태줄, 3D 전용 창 뼈대, 10장 결정 반영

- **리본**: Strata `KaBeginnerRibbon`·`KaIcons*`·`KaChip`·`KaStatusBar` 를 hgis 소스 그대로 같이 컴파일(QGIS 무관 확인: `KaStatusBar` 는 `core/EditBufferSummary.h` 앞선 선언만 씀). 단추 표는 `src/app/RibbonSpec.cpp` 한 곳(묶음: 조사 · 기록 · 자료 받기 · 지형 위에 올리기 · 3D 보기 · 3D 분석 · 내보내기 · 기타). 아이콘 표는 `S3dIconsMap.cpp` 가 Strata `KaIconsMockupMap.cpp` 를 이름만 바꿔 들여와 먼저 찾고, 3D 전용 단추 9개만 덧붙임(Lucide 1.49.0, Strata 와 같은 판, `data/icons/strata3d-icons.qrc`).
- **시험 `ribbon_spec`**(새, 8번째): Strata `MainWindowRibbon.cpp` 를 읽어 ① 공통 묶음이 Strata 순서(정합만 뺌) ② Strata 묶음 단추의 아이콘 id 가 Strata 리본에 있고, Strata `addIcon` 단추는 이름도 같음(의도한 차이: GeoTIFF → 「이미지」, 위성 칩) ③ 모든 단추 아이콘이 실제 SVG 리소스.
- **창**: 툴바·방리 칸·평가 칸을 걷어내고 가운데는 3D 화면 하나. 아래 칸(구역·관계 표 / 단면 / 버퍼)은 필요할 때만 열림. 상태줄은 `KaStatusBar`(축척 칸 대신 커서 표고·높이 배율, 작업 5187 · 제출 5179). 화면 오른쪽 위 「N ↑ · 시점 방위 · 고도」, 오른쪽 아래 「높이 ×1.0」 늘 표시.
- **아직 잇지 않은 단추**는 누를 수 없게 두고 이름표에 단계를 적음(새 조사·그리기 0단계 B, 지적·수치지형·유산 0b, 등고선·토양·고지형·지질·수계·옛 지도·땅속·깊이 2단계, 현황표 3단계, 검수·제출은 Strata).
- **10장 결정 코드 반영**: 관계 표 「방리 칸」·「지형 분류」 열과 `bangriCellOf`·`bangriCellDistance` 삭제(시험도), 화면 등급 범례에서 C 삭제, 등급 B 이름 「근거」(「복원」 낱말 안 씀), 도식 모델 311개 기본 끔·켜면 회색 선화(`BangriTerrainValidator::buildSchematicModels` + `glPolygonMode(GL_LINE)`), 건물은 바닥 외곽선만(`SceneStyle::footprintOnly`), 이름표는 방리 목록이 아니라 실제 받은 지정·등록 유산(국가유산청·Strata 자료, OSM·구역·위치도 제외)에서. `--bangri-preset`·`--export-bangri-sheet` 실행 옵션 삭제. 방리 엔진 코드와 `bangri_grid_unit` 시험은 아직 남아 있음(화면·관계 표·파이프에는 안 나감).
- **관계 표 중복**: 여러 조각인 유산(대릉원 일원 등)이 조각마다 줄로 나오던 것을 이름·분류가 같으면 가장 가까운 조각 한 줄로(「· n조각」). 분류가 다르면(출처가 다른 자료) 따로 둔다. `relations` 시험에 중복 없음 검사.
- **3D 설정 저장**: 「저장」은 조사 옆 `<조사>.s3d.json`(카메라·높이 배율·보기·이름표 크기·D 켬). 조사 .gpkg 는 쓰지 않는다. 조사를 다시 열면 읽어 들임.
- 확인: 전체 빌드 경고 0, ctest 8개 통과, 경주시.gpkg 창 캡처 `out/p0_window.png`(스모크 PASS, 관계 945곳).

## 2026-10-25 0단계 B — Strata ka_core 링크, 레이어 칸·속성 칸·주소 찾기, 새 조사·그리기·저장

- **ka_core 링크**: 앱만 hgis `build/Release/ka_core.lib` + qgis_core/gui 를 링크(엔진·시험은 QGIS 없이 그대로). `main.cpp` 는 `QgsApplication`. `scripts/dev-env.ps1` 에 hgis 와 같은 qgis-dev 경로(bin·플러그인·QtWebEngine). DemAnalyzer 가 엔진과 ka_core 양쪽에 있으나 링크 경고·오류 없음(정적 라이브러리는 필요한 오브젝트만 당긴다).
- **레이어 칸**: Strata `KaLayerInformationModel/View`·`KaLayerListChrome`·`KaLayerInformationPanel`·`KaLayerSectionDelegate` 를 그대로 컴파일, `StrataLayerDock` 이 hgis 레이어 카드와 같은 짜임으로 세움. 조사를 열면 `SurveyStorage::readEmbedded` 로 `QgsProject::instance()` 에 Strata 프로젝트를 그대로 연다. 체크를 끄면 그 레이어의 3D 지물·이름표·조사 겹침을 뺀다(레이어 이름 = 3D 지물 분류, 조사 레이어는 `ka_hgis/layer_key`). Strata 에서 꺼 두고 저장한 「현상변경허용기준」은 3D 에서도 꺼진 채(캡처 확인).
- **속성 칸**: Strata `KaInspectorPanel` + `KaFeatureCard`(조사 레이어용). 3D 에서 누르면(이름표 또는 지형 위) 지물 id `shp:<레이어>/<fid>` 로 Strata 레이어의 그 기록을 찾는다. 주변유적(참조) 기록은 Strata 카드가 제목 「이름 없음」·원 기록 칸 없음이라(바탕화면 고칠 점 S8) 3D `HeritageCard` 로 원 기록(지정번호·내부 코드 칸 제외)과 3D 계산(관계 표와 같은 거리·방위·높이차·시대)을 보인다. 경주 대릉원 일원: 47.0 m · 남 182° · +0.5 m.
- **주소·지번 찾기**: Strata `KaAppBar`·`KaRegionLocator`·`LocationSearch` 를 리본 오른쪽 끝에. 결과가 지금 지형 안이면 카메라만, 밖이면 그 자리로 지형을 다시 받음.
- **새 조사·그리기·저장**: 「새 조사」= Strata `KaNewSurveyDialog` + `SurveyProjectFactory::createNewSurvey` + 첫 저장. 「그리기」= 조사구역·유구(면)·유구 선·단면선·유물을 지형 위에 찍어(두 번 누르기/Enter 마침) `LayerOps::ensureDomainLayer` 레이어에 `FeatureRecord::stampNew` 하고 넣은 뒤 Strata 「이름·번호」 창과 카드. 「저장」= Strata `SurveySession::persistWork`(같은 저장 방식) + `SurveyFileFingerprint` 로 다른 곳에서 저장됐는지 경고 + `.s3d.json`. 조사 겹침은 프로젝트 레이어에서 다시 만들어(`ProjectSurvey`) 저장 전 편집도 3D 에 보인다. 상태줄 「저장 안 됨 n건」, 닫을 때 묻기.
- **시험(10개 통과)**: `new_survey_e2e` — 실제 exe 가 새 조사를 만들고 80 m 조사구역을 그려 저장, GDAL 로 따로 다시 읽어 조사구역 1개·넓이 6400.00 ㎡·EPSG:5187·조사 파일 안 프로젝트(1 레이어) 확인. `gyeongju_select_e2e` — 경주시.gpkg 를 열어 「경주 대릉원 일원」을 고르면 속성 칸에 기록(`shp:국가지정유산/126`).

## 2026-10-25 0b단계 일부 · 지형 위에 입히기 — 다른 컴퓨터에서 이어하기

- **된 것**: 구역·유산 면을 지형 텍스처로 입힘(`DrapeRaster`, 분류별 색·범례, 조사구역 가장 위·주황, 이름표가 조사구역을 피함). 받기 `StrataReceive`(유산·지적·토양·지질·수계, Strata 함수 그대로, 범위 = 조사구역 + 5 km). 받은 Strata 레이어(토양·지질·수계·지적 …)는 QGIS 가 Strata 스타일로 그린 그림을 지형에 입힘(`rebuildQgisDrape`). 앱 조직·이름을 Strata 와 같은 `ka-hgis` 로(계정·키·받은 주변유적 폴더를 같이 씀, 3D 캐시는 `%LOCALAPPDATA%\strata-3d\strata-3d\cache` 그대로). 조사 프로젝트를 열면 3D 는 파일이 아니라 열린 프로젝트의 주변유적 목록을 따른다(받고 저장 전인 레이어도 보임). 확인: `receive_soil_e2e`(경주 조사 사본에 토양도를 받아 지형에 입힘, 캡처 확인). Strata `KaCrashGuard` 를 붙이고 릴리스에도 PDB.
- **ctest 13개**(fixture 2개 포함). 마지막 전체 실행에서 `new_survey_e2e` 만 실패(SegFault). 원인 하나(스모크 단계가 그리는 도중에 프로젝트를 비우고 저장)는 고쳤으나, 고친 뒤에도 8번 중 1번 실패한다 — **남은 결함**. 혼자 돌리면 대개 통과. 다음에 할 일: 실패할 때 `KaCrashGuard` 로그(지금은 `%LOCALAPPDATA%\ka-hgis\logs` 로 감, `KA_HGIS_LOG_DIR` 를 이 ka_core 판이 읽지 않음)로 스택 확인.
- **아직 안 된 받기**: 수치지형(숨긴 QgsMapCanvas 필요), DEM 원본 고르기(국토지리원 .img), 고지형(조사 .gpkg 에 표를 씀). 유산·지적 받기는 사용자 계정·서약서 동의가 필요해 자동 시험하지 않았다(사람이 눌러 확인할 것).
- **다른 컴퓨터 준비**: OSGeo4W(qgis-dev, gdal-dev, Qt6) · hgis 저장소를 형제 폴더 `../hgis` 에 두고 **hgis 를 먼저 Release 로 빌드**(`build/Release/ka_core.lib` 필요) · `. .\scripts\dev-env.ps1` → `cmake --preset vs` → 빌드 → ctest. `strata_project`·`gyeongju_select_e2e`·`receive_soil_e2e` 는 `%USERPROFILE%\Documents\경주시\경주시.gpkg`(사용자가 Strata 에서 그린 조사, 저장소에 없음)가 있어야 한다. 계정·키는 Strata 설정(`%LOCALAPPDATA%\ka-hgis\ka-hgis\*.ini`, DPAPI 라 컴퓨터마다 다시 넣어야 함).
- **다음 순서**: ① `new_survey_e2e` 간헐 실패 ② 수치지형 받기(숨긴 캔버스) + 도엽 틀 3D 띠 ③ DEM 원본 고르기 ④ 2단계 나머지(유구 깊이·땅속 보기·기준점·유물 3D 점) ⑤ 3단계 현황표 17열·측정 ⑥ 4단계 도면 ⑦ 1단계 렌더러(QRhi). 설계도 `docs/PLAN_3D_v3.html`, Strata 쪽 고칠 점은 바탕화면 `Strata_GIS앱_고칠점.txt`(이 컴퓨터에만 있음, S1–S8).
- **11:05 추가(빌드·시험 전, 이 컴퓨터 개발 중단)**: `new_survey_e2e` 충돌 스택을 잡았다 — `QThreadPool` 워커 안(두 번 저장을 마친 뒤). 스모크가 끝나 Qt 가 내려갈 때 지형 입히기(DrapeRaster 4096² 두 장) 같은 워커가 아직 돌던 것으로 본다. 고침(검증 전): `MainWindow::~MainWindow` 에서 QGIS 그리기 취소 + 모든 워커 기다림 + `QThreadPool::globalInstance()->waitForDone()`. 또 충돌 기록 폴더: `qputenv` 가 한글 경로를 ANSI 로 깨뜨려 기록이 안 남던 것을 `_wputenv_s` 로(이건 확인됨 — `%LOCALAPPDATA%\strata-3d\logs` 에 crash-*.log 가 남음). **다음 컴퓨터에서 첫 일: 빌드 → `new_survey_e2e` 10번 반복해 0/10 실패인지 확인.**
