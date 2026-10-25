# engine

**Parent context:** `../AGENTS.md`
**Generated:** 2026-10-25 · **Updated:** 2026-10-25

## Purpose
`strata_3d_engine` 정적 라이브러리. 실측 자료를 받아(DEM·위성·지물) 지형 메시와 가속 구조를 만들고, 조사구역 하나를 주변 유산과 견주어 평가한다. UI 코드는 없고 Qt Core/Gui + GDAL 만 쓴다. `include` 경로에 `${HGIS_ROOT}/src/core` 가 있어 Strata 의 `DemAnalyzer.h` 를 그대로 쓴다.

## Key Files
| File | Description |
|------|-------------|
| `GeoFeature.h` | 공통 형: `Pt`, `GeoFeature`(kind·category·ring·높이 출처 `HeightSource`), `FeatureRequest`(작업 좌표계 창) |
| `DemGrid.*`, `DemProvider.*` | 실측 DEM 한 장(행 0 = 북). Copernicus GLO-30 COG(`/vsicurl/`)를 GDALWarp 로 작업 좌표계 미터 격자 + GeoTIFF 캐시 |
| `CrsTransform.*` | OSR 로 점 변환. 축 순서는 항상 (경도|X, 위도|Y) |
| `HttpFetch.*` | GDAL(libcurl) HTTP. 워커에서만 부른다 |
| `SatelliteProvider.*`, `VworldKey.*` | VWorld WMTS 위성 → 작업 좌표계로 warp(Referer `https://localhost` 필수, 디스크 캐시 없음, Google 금지). 키는 Strata 와 같은 순서로 찾고 값은 로그에 남기지 않는다 |
| `OsmProvider.*` | Overpass 건물 외곽·historic. 높이 태그 없으면 `HeightSource::Unknown` |
| `KhsProvider.*` | 국가유산청 Open API 지정유산 점(경위도) |
| `KhsSpcaProvider.*` | 국가유산 공간정보 spca 목록(시대·지정일·중심점 EPSG:5179). 7일 캐시 → 네트워크 → 오래된 캐시 |
| `LocalVectorProvider.*` | Strata SHP 캐시(`%LOCALAPPDATA%\ka-hgis\ka-hgis\주변유적\SHP\<해시>\payload\*.shp`)를 OGR 로 읽기 전용. category = 파일 이름 |
| `StrataSurvey.*` | Strata 조사 .gpkg 7개 레이어(survey_area, feature_poly, trial_trench …) 읽기 전용 판독 |
| `CrossCheck.*` | 같은 유산의 출처별 위치 대조(일치 ≤10 m, 근접 ≤50 m, EPSG:5179) |
| `HeritageNaming.*` | 지정번호 제거·종목 정리 `displayHeritageName`, 도식 문구 `schematicModelWording` |
| `EvidenceTier.*` | 근거 등급 A–D 와 정점 색 탈채도·종이색 섞기 `applyTierStyle` |
| `Archaeology3dEvaluator.*` | 반경 버퍼·이격거리·방위·높이차·경사(DemAnalyzer)·TPI·Weiss 지형 분류·단면 |
| `RelationEngine.*` | 관계 표: 지형 단위·방리 칸·시대 + 보호구역/현상변경허용기준/유존지역/허가구역 겹침 판정(`ZoneHit`) |
| `EvaluationOverlay.*`, `EvaluationJson.*` | 평가 결과의 3D 겹침 띠, Strata 로 돌려줄 JSON(키·단위는 `docs/bridge-protocol.md`) |
| `TerrainLodTree.*`, `Frustum.*` | CDLOD 4분트리 조각 선택, 절두체 컬링 |
| `Kdop14Bvh.*`, `TerrainBvh.*`, `KdTree3.*` | 14-DOP BVH(Blender BLI_kdopbvh 축), 지형 광선 교차, 3D k-d 트리 |
| `SceneMesh.*`, `Triangulate.*` | 건물·유적 삼각형 덩어리(정점 10 float: 위치3·법선3·색4), 귀 자르기 |
| `AgxView.*`, `NishitaSky.*` | Blender 4.2.3 AgX 보기 변환, Nishita 하늘(상수 출처 `docs/sky-constants.md`) |
| `BangriGridEngine.*`, `BangriSurveyEvaluator.*`, `BangriTerrainValidator.*`, `FossilizedRoadAnalyzer.*` | 신라 방리 추정 격자(등급 C), 조사구역의 방리 위치·권장 트렌치, 311개 유적 도식 모델 번들, 현대 도로의 방리축 잔존 |
| `SillaMoundReconstructor.*` | 고분·능묘 판정과 타원 봉분 도식 |
| `DbpiaBangriLedger.*` | `docs/dbpia_silla_bangri_9ju5sogyeong_ledger.json` 문헌 원장 |

## For AI Agents

### Working In This Directory
- 위젯·GL 헤더를 넣지 않는다. 실패는 `bool` + `QString* err`.
- 원본 표고·좌표만 쓴다. 수식으로 만든 높이·추정값은 등급(C/D)과 `HeightSource` 로 구분해 내보낸다.
- 유산 이름을 내보내는 곳은 `displayHeritageName()` 을 거친다. 도식 모델은 `EvidenceTier::Schematic`.
- Strata 에 이미 있는 계산(경사·음영 등)은 `${HGIS_ROOT}/src/core` 의 QGIS 무관 파일을 `CMakeLists.txt` 에 넣어 쓴다. 판정 목록은 `docs/STRATA_REUSE.md` 1장.
- 거리 정의는 Strata `NearbyHeritageCollect` 와 맞춘다: 조사구역 경계 기준, 안이면 0.

### Testing Requirements
`test_strata_3d`(DEM·LOD·BVH·AgX·조사 판독), `test_relations`(관계 표·구역 판정), `test_cross_check`(spca·KHS 대조), `test_heritage_trust`(표기·등급, 311개 전수), `test_bangri_grid`. 정답은 엔진과 독립된 계산(GDAL 명령줄, ogrinfo SQL, 직접 계산)에서 가져온다.

### Common Patterns
- 제공자는 `FeatureRequest` 창(작업 좌표계 중심 ± 반경)을 받아 창 안의 것만 돌려준다.
- 지역 미터 좌표: DEM 중심 원점, +X 동, +Y 북, +Z 위.
- OpenMP(`/openmp`)는 BVH 빌드 같은 독립 반복에만.

## Dependencies

### Internal
- `${HGIS_ROOT}/src/core/DemAnalyzer.{h,cpp}`.

### External
- GDAL/OGR(`ogr_geometry.h` Intersects/Distance 등), PROJ, Qt Core/Gui/Network.

## Manual Notes
