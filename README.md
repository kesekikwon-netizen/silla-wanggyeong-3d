# HGIS 3D Engine — 경주(慶州) 3D 실사 지형 · 신라 왕경 복원 · 고고학 정밀 평가 엔진

> **Blender 4.2.3 LTS (`blender-4.2.3-windows-x64.zip`) 무설치 REA MCP 전수 역공학 아키텍처 기반 고해상도·무렉(Zero-Lag) 3D 지형·건물·고고학 평가 엔진**

---

## 1. 프로젝트 개요

본 저장소(`hgis-3d-engine`)는 호스트 OS에 Blender를 설치하지 않고 `blender-4.2.3-windows-x64.zip` 아카이브 전체(`5,699` 그래프 노드, `5,521` 파일, `130` PE32+ 바이너리, `blender.pdb` 내 `252`개 정적 라이브러리 및 `5,919`개 번역 단위)를 **REA MCP (`rea v6.7.0`)**로 전수 역공학한 결과를 바탕으로, **경주(慶州) 지역의 위성사진·수치표고모형(DEM)·현대 건물(한옥지구 포함)·신라 왕경 고고학 유적 및 고분군을 실시간 60FPS 3D로 가시화하고 고고학적으로 정밀 평가**하기 위해 구축된 독립 엔진이다.

### 1.1 REA MCP 전수 역공학 및 디컴파일 문서 (`docs/rea/`)
- [DECOMPILED_BLENDER_AND_WEB_REA.md](docs/rea/DECOMPILED_BLENDER_AND_WEB_REA.md): `blender.exe` 468개 코어 심볼·함수 디스어셈블리(`BLI_bvhtree_balance` `0x14061e870`, `BLI_bvhtree_find_nearest_ex` `0x14061eab0`, `bvhtree_kdop_axes` `0x1430a2de0`, `SKY_nishita_skymodel_precompute_sun` `0x14217e500`), 175개 내장 EEVEE Next GLSL 셰이더 추출 및 웹/라이브 브라우저 REA MCP(`ev_a0c78d33...`, `ev_14f0ead6...`, `ev_31657eea...`, `ev_14275fc9...`, `ev_9e384878...`) 종합 보고서
- [BLENDER_4_2_3_REA_COMPLETE.md](docs/rea/BLENDER_4_2_3_REA_COMPLETE.md): 아카이브 그래프(`agm_ee9e882fa35c55e0de8ddeeb408f016c0dabe6866b7b4d26591b1d8f94c5d4e1`), PE 리소스(`inspect_pe_resources`), COFF 섹션/익스포트 테이블, `blender.pdb` 252개 정적 라이브러리 및 7대 핵심 아키텍처 매핑 보고서
- [docs/rea/decompiled/blender_core_symbols.json](docs/rea/decompiled/blender_core_symbols.json): `blender.exe` + `blender.pdb`에서 추출한 468개 코어 함수/데이터 심볼 VA 및 오프셋
- [docs/rea/decompiled/blender_core_functions_disassembly.json](docs/rea/decompiled/blender_core_functions_disassembly.json): 12개 핵심 `.text` 함수 Intel x86_64 디스어셈블리 및 `BVHTree` 구조체 오프셋
- [docs/rea/decompiled/blender_eevee_next_curated_shaders.json](docs/rea/decompiled/blender_eevee_next_curated_shaders.json): `.rdata`에서 추출한 175개 EEVEE Next GLSL 셰이더 인벤토리 및 핵심 6종 전문
- [blender_pdb_libraries.json](docs/rea/blender_pdb_libraries.json): `blender.pdb`에 링크된 252개 정적 라이브러리(`bf_draw.lib`, `bf_gpu.lib`, `bf_blenkernel.lib`, `bf_blenlib.lib`, `bf_depsgraph.lib`, `bf_intern_sky.lib`, `cycles_bvh.lib` 등) 및 5,919개 번역 단위(TU) 명세
- [blender_binaries_inventory.json](docs/rea/blender_binaries_inventory.json): 130개 `.exe` / `.dll` / `.pyd` 바이너리 크기 및 SHA-256 해시 전수 인벤토리
- [blender_addons_core.json](docs/rea/blender_addons_core.json): 내장 36개 코어 애드온 목록

---

## 2. 앱(`src/app/`) · 엔진(`src/engine/`) 분리 및 HGIS 공유 아키텍처 (`src/`)

| 모듈 파일 | 역공학/연동 원본 | 핵심 구현 기능 |
|---|---|---|
| [src/engine/decompiled_blender_kernels.js](src/engine/decompiled_blender_kernels.js) | `BLI_kdopbvh` (`0x14061e870`, `0x1430a2de0`), `SKY_nishita` (`0x14217e500`), `datatoc_eevee_*`, Web REA (`ev_a0c78d33...`) | 13축 K-DOP(`compute14DopHull`, `test14DopOverlap`), 16비트 힐베르트 공간 정렬(`hilbertIndex2D`), Nishita 태양 분광 복사조도(`evaluateNishitaSunIrradiance`), EEVEE Horizon-Scan GTAO(`computeEeveeHorizonScanAO`), 다각형 삼각분할(`triangulatePolygon2D`) |
| [src/engine/engine_core.js](src/engine/engine_core.js) & [src/engine/engine_worker.js](src/engine/engine_worker.js) | `DRW_draw_render_loop_ex` (`0x141222fc0`) + Web Worker 오프로드 | UI 메인 스레드와 완전히 분리된 비동기 3D 연산·공간평가 엔진 (`Hgis3DEngineCore`) |
| [src/app/ka_theme.js](src/app/ka_theme.js) | `hgis/src/app/KaTheme.h`, `KaTheme.cpp`, `KaThemeColors.cpp`, `KaThemePaper.cpp` | 메인 GIS 앱(`hgis`)과 1:1 동일한 5종 테마(`default`, `paper` 새 모양/조용한 도판, `sky` 하늘 눈밭, `bolt` 번개 노랑, `blossom` 벚꽃 파스텔), 고대비, 큰 글씨, WCAG 2.0 대비비(`contrastRatio`) |
| [src/app/engine_client.js](src/app/engine_client.js) | 비동기 RPC 클라이언트 | Web Worker 오프로드 및 인프로세스 폴백(`HgisEngineClient`), 실시간 워커 연산 텔레메트리 |
| [src/bridge/hgis_share_protocol.js](src/bridge/hgis_share_protocol.js) | `hgis/src/core/Terrain3dService.h` (`DemScene`, `composeSheet`) | 계획 → REA 디컴파일 → 엔진 분리 → 고고학 평가 → 입체지형 도면(`composeSheet`·축척막대·방위표·범례) 및 `Terrain3dService::DemScene`, `EPSG:5187` GeoJSON 양방향 공유 프로토콜 |
| [src/core/gyeongju_crs.js](src/core/gyeongju_crs.js) | `bf_blenlib` 수학 투영 | `EPSG:4326`(WGS84) ↔ `EPSG:5187`(GRS80 동부원점 미터 좌표) ↔ 경주 왕경 중심 로컬 3D 좌표 ↔ 위성 타일(`z/x/y`) 정밀 변환 |
| [src/core/bvh_spatial.js](src/core/bvh_spatial.js) | `BLI_kdopbvh.c`, `pbvh.cc`, `kdtree_3d.c` | **`PBVHTree`** ($O(\log N)$ 지형 레이캐스팅, 6-평면 절두체 컬링, 3D 가시권 시선 차폐 검사) + **`KDTree3D`** (반경 500m / 1,000m 유적 즉시 탐색) + `JoinStyle::Round` 버퍼 생성 |
| [src/core/gyeongju_dem_terrain.js](src/core/gyeongju_dem_terrain.js) | `draw_pbvh.cc`, `PyInit_mathutils_noise` | 경주 분지 실측 지형 보정 DEM (남산 금오봉·고위봉, 낭산, 월성, 명활산, 선도산, 소금강산, 토함산 + 형산강·남천·북천 수계 및 하안단구/선상지 분류) + 무균열 스커트 **Quadtree CDLOD** 청크 생성기 |
| [src/core/gyeongju_archaeology_db.js](src/core/gyeongju_archaeology_db.js) | `draw_instance_data.cc` | 경주 핵심 고고학 유적 20개소(월성, 동궁과 월지, 첨성대, 황룡사지 9층목탑, 분황사, 대릉원 황남대총·천마총·미추왕릉, 봉황대, 쪽샘, 월정교, 사천왕사지, 선덕여왕릉, 명활산성, 남산신성 등) + **신라 왕경 방리제(坊里制) 도로망** + 950동 GPU 인스턴싱 건물/한옥 데이터 |
| [src/core/archaeology_evaluator.js](src/core/archaeology_evaluator.js) | `BVHTree` + `KDTree` 복합 공간분석 | 지형 더블클릭 또는 좌표 입력 시 **반경 500m/1,000m 유적 이격거리·방위**, **지형위치지수(TPI)·경사도·하안단구 입지**, **왕경 6대 거점 3D 상호 가시권(Viewshed)**, **방리제 고대 도로 인접도**, **가상 시굴 트렌치(T-1·T-2) 4개 층위 단면 및 보존조치 등급** 자동 판정 |
| [src/core/eevee_sky_shader.js](src/core/eevee_sky_shader.js) | `bf_intern_sky.lib` (`sky_nishita.cpp`), `AgX`, `eevee_ambient_occlusion.cc` | 경주 위도/경도 기준 태양 고도·방위각 천문 계산 + **Nishita 물리 대기 산란 셰이더** + **AgX Filmic 톤매핑** + 4종 지형 분석 셰이더(위성실사 PBR / EEVEE Clay 미기복 / 매장유산 적합도 히트맵 / 경사도·TPI) |
| [src/bridge/blender_exporter.js](src/bridge/blender_exporter.js) | `BLENDER_EEVEE_NEXT` / `bpy` 브릿지 | 평가 구역의 3D 지형·신라 유적·500m/1,000m 버퍼를 **Blender 4.2 전용 Python 씬 생성 스크립트(`.py`)**, **Wavefront `.obj`**, **고고학 정밀 평가보고서(`.md`)**로 즉시 내보내기 |

---

## 3. 실행 및 시험 방법

```bash
# 1. 단위·통합 자동 검증 테스트 실행
npm test

# 2. CLI 기반 경주 왕경 중심지구 고고학 평가보고서(.md) + Blender 4.2 씬(.py) + 3D 지형(.obj) 일괄 생성
npm run export:gyeongju

# 3. 인터랙티브 3D WebGL2 경주 고고학 스튜디오 서버 실행 (http://localhost:4230)
npm start
```
