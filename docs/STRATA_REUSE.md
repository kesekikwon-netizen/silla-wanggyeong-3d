# Strata 재사용 지도 (Strata 3D 용)

작성 2026-10-25. 대상: Strata(ka-hgis, `C:\Users\권을\Documents\hgis`) ↔ Strata 3D(`hgis-3d-engine`).
원칙: Strata 가 이미 하는 2D 일은 Strata 3D 가 다시 만들지 않는다. 「QGIS 무관」은 링크까지 따라가 본 결과다(코드 조사 에이전트, 파일·함수 이름은 소스에서 확인).

## 0. 핵심 결론

1. hgis 에는 **QGIS 없이 쓸 수 있는 라이브러리 타깃이 없다.** `ka_core`(STATIC, `hgis/CMakeLists.txt` 156행)가 core 전체를 묶고 PUBLIC 으로 `qgis_core qgis_gui Qt6::Widgets …` 를 끌고 온다.
2. 재사용 방식은 셋: (a) 소스 같이 컴파일(KaTheme 처럼 `HGIS_ROOT`) — **QGIS 무관 파일만**, (b) `ka_core` 링크 — QGIS SDK 전체가 따라와 비현실적, (c) 파일·파이프로 데이터만 받기 — 이미 주력.
3. 국가유산청 **공개 API**(`KhsProvider`, `KhsSpcaProvider`)는 Strata 와 겹치지 않는다. Strata 는 GIS 통합인트라넷(로그인 + WebEngine) SHP 만 쓴다.

## 1. 파일별 재사용 판정

**(a) 같이 컴파일 가능** (`hgis/src/core/`, Qt Core/Gui/Network + GDAL 만)

| 파일 | 쓰임 |
|---|---|
| `DemAnalyzer.{h,cpp}`, `DemHillshadeFile.cpp` | 래스터 읽기 `readFloatBand/readFloatWindow`, 음영 `hillshadeHorn`, 경사 `slopeDegrees` |
| `Terrain3dService.{h,cpp}` | `loadDemClip`, `buildMesh`, CPU `renderPerspective`, `composeSheet`, `googleSatelliteXyzUri()` |
| `Strata3dLink.{h,cpp}` | 파이프 v1 클라이언트(시험 클라이언트로도) |
| `SurveyContourBuilder/Tin/Clip/Write`, `SurveyContourMath.h`, `SurveyPointSchema` | 실측점 → TIN 격자·등고선 (`SurveyPointReader.cpp` 는 불필요) |
| `TopographicArchive` | 수치지형도 ZIP 안전 풀기 |
| `HeritageFormParser`, `HeritageFetchPlan`, `KoreaRegionCatalog` | 시·군 목록, 인트라넷 폼 파싱 |
| `ChecklistEngine`, `WorkflowGuide` | 검수 규칙 엔진(입력 JSON 은 Strata 가 만듦), 조사 단계 `WorkflowStep` |
| `TrenchGridGenerator/TrenchLShape/TrenchGridSnap/TrenchPlanCache`, `StandardScales` | 시굴 트렌치 격자, 표준 축척 |

**(a) 불가 → (c) 결과 파일·설정 직접 읽기**: `VworldSettings`·`TopographicSettings`·`HeritageIntranetSettings`·`LocationSearch`·`AdminBoundaryService`(→ `KaPortableRuntime` 가 QGIS), `HeritageIntranetFlow`·`HeritageHttpClient`(→ `HeritageStyle.cpp`), `SurveySchema`·`SurveyStorage`·`SurveyProjectFactory`·`NearbyHeritage*`·`HeritageImport`·`BufferAnalysis`·`DemDownloadService`·`Topographic{Catalog,Sheets,SourceCrs}`·`LayerOps`·`Terrain3dLayoutService`(직접 qgs), `Hwp*`·`GisFactHarness`·`AssessmentContract`(헤더 사슬).

## 2. 기능 표

| 기능 | Strata 파일·함수 | Strata 3D 재사용 |
|---|---|---|
| 국가유산 자료(인트라넷) | `HeritageIntranetFlow`, `HeritageImport::loadDataset(QgsProject*, HeritageDataset, files, archiveRoot, region)` | (c) Strata 가 받은 SHP 캐시(`%LOCALAPPDATA%\ka-hgis\ka-hgis\주변유적\SHP\<해시>\payload\*.shp`)를 `LocalVectorProvider` 로 읽음 — **이미 재사용 중** |
| 자료 종류 | `HeritageStyle.h` `enum HeritageDataset{DesignatedHeritage, AlterationStandard, BuriedHeritageArea, HeritageDistributionMap, SurfaceSurveyArea, ExcavationSurveyArea}` | 이름·색 규칙 기준 |
| 지정구역·**보호구역 폴리곤** | 지정유산 ZIP 안 「국가지정유산보호구역」「시도지정유산보호구역」 SHP (`HeritageImport.cpp:232-235`) | (c) 같은 캐시 → 관계 표 「보호구역 안/밖」은 **이 폴리곤으로** 판정 |
| 역사문화환경 보존지역 | 같은 이름 코드 없음. 가장 가까운 것 `AlterationStandard`(현상변경허용기준) | (c) 같은 캐시 |
| 주변유적 수집 | `NearbyHeritageCollect::collect(QgsProject*, src, maxRadius)`, `cardinalDirection8()`, `Item{siteName, radiusLabel, direction, distanceMeters, heritageCode, era…}`, `.xlsx` 내보내기 | (c) .xlsx 또는 파이프 명령 추가 |
| 조사 스키마 | `SurveySchema` `kCurrentVersion=3`, 7개 테이블, `hgis/data/schemas/ka_hgis_layers.yaml` | `StrataSurvey` 로 읽기만, `KA_HGIS_SCHEMA_VERSION` 확인할 것 |
| 조사 단계 | `WorkflowGuide` `WorkflowStep{Survey, Background, SurveyArea, Features, ControlPoints, Review, Submission}` | (a) |
| DEM 받기 | `DemDownloadService::prepare(QgsRectangle, …)` — Copernicus URL 공식이 3D `DemProvider` 와 같음 | 3D 는 재투영 미터 격자가 필요해 `DemProvider` 유지, URL 공식은 공유 후보 |
| DEM 분석 | `DemAnalyzer` | **(a) 바로 씀** — 3D 의 경사·음영 중복 제거 |
| 수치지형도 1:5000 | `Topographic*`, `TopographicCatalog::Category{Contour, Road, Building, Water…}` | Archive 는 (a), 풀린 SHP 는 (c) `LocalVectorProvider` → **건물 외곽·층수, 등고선의 국가 기준 출처**(OSM 보다 우선) |
| 실측점 등고선·TIN | `SurveyContourBuilder::build()` 등 | **(a)** → 조사 측량점을 3D 지형으로 |
| Strata 내장 3D | `Terrain3dService`(CPU 한 장), `KaTerrain3dStudio` | 3D 는 GPU 판(`KaGlTerrainViewport`) |
| VWorld | `LayerOps::addVworld*`, `VworldSettings::loadApiKey()` | (c) 키는 `VworldKey.cpp` 가 같은 INI/QSettings 를 읽음 |
| 주제도 | `SoilMapService`, `GeologyMapService`, `PaleoLandformService`, `RiverMapService` | (c) 결과 레이어를 3D 지형에 입힐 후보(**하천은 `RiverMapService` 를 먼저 볼 것**) |
| 버퍼 | `BufferAnalysis::addDistanceRing` | 3D 는 그리기만 |
| 도면 | `KaDrawingStudio`, `LayoutService` | (c) 3D 는 `renderSheet` PNG/PDF → Strata 가 `Terrain3dLayoutService::replacePicture` |
| 보고서 | `KaHwpReportStudio`, `HwpSurveyReportService` | (c) 3D 평가 요약을 파이프로 |
| 문헌·인용 | `KaGeminiBridgeClient`(dbpia `/api/search/list`), `GisFactHarness::buildLedger`, `KaDbpiaLedgerText` | 3D `DbpiaBangriLedger` 와 원장 형식 통일 필요 |
| 연동 | `Strata3dLink`(`pipeName()` 기본 `strata-3d-bridge-v1`, `openAndEvaluate`, `fetchSheet`), `KaStrata3dLink` | 3D `StrataBridge` 가 짝. 반대 방향 명령(`nearby`, `paths`)은 v1 확장 필요 |

## 3. 겹침 정리 (Strata 3D `src/engine/*`)

| 3D 파일 | 겹침 | 판정 |
|---|---|---|
| `KhsProvider`, `KhsSpcaProvider`, `CrossCheck`, `HeritageNaming`, `OsmProvider` | 없음 | 3D 전용 유지. `HeritageNaming` 은 Strata 에도 필요(지정번호) → hgis core 로 올릴 후보 1순위 |
| `LocalVectorProvider` | Strata 캐시 재사용 중 | 유지, 수치지형도 풀린 폴더도 같은 방식으로 |
| `StrataSurvey` | `SurveySchema`(QGIS) | 유지(QGIS 무관 판독기) |
| `RelationEngine`, `Archaeology3dEvaluator` | `NearbyHeritageCollect`(경계 최단거리·8방위) | 부분 겹침: 거리 정의를 Strata 와 맞추고(경계, 안이면 0), 보고서로는 **8방위**. 방리·시대는 3D 전용. 경사·음영은 `DemAnalyzer` 로 |
| `DemProvider`, `SatelliteProvider` | `DemDownloadService`, `googleSatelliteXyzUri()` | 유지(출력이 다름), URL 공식은 `Terrain3dService` 로 하나로 |

## 4. Strata 에 없는 것 = Strata 3D 가 만드는 3D 전용

GPU 실시간 지형(`KaGlTerrainViewport`, 셰이더, HDR) · LOD·절두체·BVH · 지표 단면 · 하늘·AgX · 3D 지물 메시 · 평가 겹그림 · 근거 등급·출처 대조 · 신라 방리·고분 도식 · 고해상도 입체지형 도면(3072 px).

## 5. 다음 할 일 (우선순위)

1. `${HGIS_ROOT}/src/core/DemAnalyzer.cpp`, `DemHillshadeFile.cpp`, `Terrain3dService.cpp` 를 `strata_3d_engine` 에 같이 컴파일(include `${HGIS_ROOT}/src`), 3D 쪽 경사·음영·위성 URI 중복 제거.
2. `SurveyContour*` 를 같이 컴파일 → 조사 측량점 DEM 을 3D 지형으로.
3. 관계 표 「보호구역 안/밖」을 Strata 캐시의 **보호구역 폴리곤**으로 판정(반경 가정 대신), 하천은 `RiverMapService` 결과 확인 후.
4. 수치지형도 건물·등고선 SHP 를 `LocalVectorProvider` 로 읽어 OSM 보다 우선.
5. 파이프 v1 반대 방향 명령(`nearby`, `paths`)을 Strata 와 합의, 그 전에는 파일 읽기.
6. 방위(8/16)·거리 정의·이름 표기를 두 앱이 똑같이.
