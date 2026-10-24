# [작업 위임 명령서] 경주 실사 3D 지형·건물·고고학 평가 독립 엔진 및 데스크톱 앱 (`strata-3d`) 구축

## 0. 배경 및 절대 금지사항 (최우선 준수)
이전 작업에서 에이전트가 `C:\Users\권을\Documents\hgis`의 규칙(`Windows 데스크톱, 서버·웹 없음`, `Standalone C++20/Qt 6 Widgets app on OSGeo4W`)을 어기고, `C:\Users\권을\Documents\hgis-3d-engine`에 Node.js + Three.js 웹페이지와 가짜 수학 공식(가우시안 봉우리 DEM, 난수 박스 건물, 주석으로만 붙인 블렌더 심볼)을 만들어 놓았다.

**다음 행위는 어떤 경우에도 절대 금지한다:**
1. **웹/브라우저/Node.js/Three.js/WebEngine 사용 금지**: 반드시 `Strata`(`ka-hgis.exe`)와 동일한 **OSGeo4W 기반 C++20 / Qt 6 Widgets / OpenGL 3.3+ 네이티브 Windows 데스크톱 애플리케이션(`.exe`)**과 분리된 **C++20 3D 엔진 라이브러리**로 개발한다.
2. **가짜 수식 지형·난수 건물·더미 데이터 절대 금지**:
   - `Math.exp`/`sin`/`cos` 등으로 만든 가짜 지형 수식, 의사난수로 찍은 박스 건물, 하드코딩된 가상 좌표를 일체 쓰지 마라.
   - 반드시 **GDAL / QGIS 프로바이더를 통해 실제 DEM 래스터(로컬 GeoTIFF, 국토지리정보원 1:5,000 수치지형도 표고/등고선, 또는 `/vsicurl/` Copernicus 실측 DEM COG)**, **실제 위성 정사영상(VWorld WMTS / Google Satellite XYZ 타일)**, **실제 경주 건물·문화유산·조사구역 벡터(`GPKG` / `SHP`)**를 읽어서 렌더링하라.
3. **이름만 빌려온 가짜 역공학(눈속임) 금지**:
   - 주석이나 UI 배지에만 Blender 함수명/주소를 적는 눈속임을 금지한다.
   - REA MCP로 분석·디컴파일한 Blender 4.2.3(`C:\Users\권을\Documents\hgis-3d-engine\artifacts\blender-4.2.3-windows-x64.zip` 및 `docs/rea/decompiled/`)의 실제 C/C++ 구조(`BLI_kdopbvh` 14-DOP/AABB BVH, `BKE_pbvh` 공간 분할·절두체 컬링, `BLI_kdtree_3d`, `EEVEE Next` GLSL 셰이더, `SKY_nishita` 대기 산란, `AgX` 톤매핑)를 **C++20 네이티브 코드와 OpenGL 3.3 Core GLSL 셰이더로 직접 구현·컴파일**하라.

---

## 1. 핵심 목표 아키텍처 (앱과 엔진의 분리 + `Strata` 전 주기 공유)

### 1.1 왜 별도 엔진과 별도 앱인가? (`Strata`의 현재 병목 해결)
현재 메인 GIS 앱(`C:\Users\권을\Documents\hgis`)의 입체지형 코드는 다음과 같은 한계가 있다:
- `src/core/Terrain3dService.cpp`의 `Terrain3dService::renderPerspective()`(237~420행)가 GPU(OpenGL)를 쓰지 않고 **메인 UI 스레드에서 CPU 이중 `for` 루프로 `QImage` 픽셀을 하나씩 찍는 소프트웨어 래스터라이저**다.
- 이 CPU 부하 때문에 `src/app/KaTerrain3dStudio.cpp`의 `rebuildMesh()`(155행)는 격자 해상도를 `maxEdge = 160`으로 제한하고, `src/app/KaTerrain3dView.cpp`(48~50행)는 마우스 드래그 중 화면 해상도를 1/2로 낮추며, `captureSatellite()`(258~265행)는 메인 스레드에서 동기 루프를 돈다.

따라서 메인 GIS 앱(`Strata`)의 부담을 완전히 덜어주도록 **무거운 3D 공간연산·GPU 렌더링 엔진(`strata_3d_engine`)**과 **별도 네이티브 데스크톱 앱(`strata-3d.exe`)**을 분리 설계하되, 계획부터 개발·고고학 평가·도면 출력까지 **`Strata`와 실시간으로 100% 공유**되게 해야 한다.

### 1.2 저장소 및 타깃 구조 (`C:\Users\권을\Documents\hgis-3d-engine`)
기존의 가짜 웹 파일(`server.js`, `public/`, `src/*.js`, `tests/*.js`, `scripts/*.js`, `package.json`)은 모두 삭제하고, `C:\Users\권을\Documents\hgis\CMakeLists.txt` 및 `scripts/dev-env.ps1`과 동일한 **CMake + Visual Studio 17 2022 (x64) + `C:\OSGeo4W` (`Qt6`, `qgis-dev`, `gdal-dev`)** 빌드 체계로 재구축한다:

1. **`strata_3d_engine` (`src/engine/` — C++20 정적/동적 엔진 라이브러리, UI 코드 없음)**
   - **REA 기반 C++ 공간가속 커널 (`BvhSpatialEngine.cpp/.h`)**:
     - Blender `BLI_kdopbvh`(`0x14061e870` `BLI_bvhtree_balance`, `0x14061eab0` `BLI_bvhtree_find_nearest_ex`, `0x14061f100` `BLI_bvhtree_ray_cast`, `.rdata` `0x1430a2de0` 13축 K-DOP) 구조체를 C++20 `std::vector` 연속 메모리와 `OpenMP` 병렬 빌드로 구현.
     - `BKE_pbvh` 방식의 청크 단위 AABB/14-DOP 절두체 컬링(Frustum Culling) 및 화면 오클루전 컬링.
     - `BLI_kdtree_3d` 기반 반경 500m / 1,000m 문화유산·건물 고속 근접 탐색.
   - **실측 GIS 데이터 로더 & CDLOD 지형/건물 메쉬 빌더 (`RealGisSceneBuilder.cpp/.h`)**:
     - `GDAL` (`gdal_i.lib`) 및 `hgis`의 `DemAnalyzer` / `Terrain3dService` 방식을 사용해 **실제 경주 지역 DEM(NGII 1:5,000 수치지형도 표고/등고선, 로컬 GeoTIFF, `/vsicurl/` Copernicus DEM COG)**을 읽고, 크랙 방지 스커트(Skirt)가 적용된 고해상도 멀티-LOD 지형 메쉬 생성 (`maxEdge = 160` 제한 해제, 타일/청크 기반 수천×수천 격자 무렉 처리).
     - **실제 위성 정사영상 스트리밍**: VWorld WMTS (`api.vworld.kr` / `xdworld`, API 키 로드 순서는 `CLAUDE.md` 규칙 준수: `%LOCALAPPDATA%\ka-hgis\ka-hgis-vworld.ini` → `HKCU\Software\ka-hgis` → `VWORLD_API_KEY` → `config/secrets.ini`) 및 Google Satellite XYZ 타일을 백그라운드 워커 스레드에서 비동기 다운로드·합성하여 고해상도 밉맵(Mipmap) 텍스처로 GPU에 업로드.
     - **실제 경주 건물 및 문화유산 3D 복원**: 국토지리정보원 수치지형도 건물 레이어(`TopographicCatalog` / `TopographicShapefileCache`), 문화재 공간정보(`NearbyHeritageCollect` / `HeritageImport`), 조사 프로젝트 GPKG 레이어(`survey_area`, `feature_poly`, `feature_line`, `trial_trench`)의 **실제 폴리곤 좌표와 층수/높이 속성**을 읽어 3D 건물 메쉬(Extrusion)와 유적 범위를 생성.
   - **고고학 3D 정밀 평가 엔진 (`Archaeology3dEvaluator.cpp/.h`)**:
     - 작업 좌표계 `EPSG:5187`(동부원점, 경주 기본) / `EPSG:5186` 정밀 유지 (`PROJ` / `QgsCoordinateTransform` 사용).
     - 조사구역(`survey_area`) 또는 지형 클릭 지점 기준 반경 500m / 1,000m `JoinStyle::Round` 버퍼 생성 및 실제 문화유산 이격거리·방위 산출.
     - 실측 DEM 기반 경사도(Slope), 향(Aspect), 지형위치지수(TPI), 하안단구/선상지/구릉 미지형 분류, 시굴 트렌치(`trial_trench`) 지형 단면 및 토층 프로파일 계산.
     - PBVH 레이캐스팅 기반 경주 왕경 주요 거점 상호 가시권(3D Viewshed) 차폐 분석.
   - **비동기 워커 오프로드 (`EngineWorkerPool.cpp/.h`)**:
     - 모든 DEM 디코딩, 위성 타일 합성, 메쉬 테셀레이션, BVH 빌드, 가시권 레이캐스팅을 `QThreadPool` / `OpenMP` 백그라운드 스레드에서 수행하고, 완료된 `Float32`/`Uint32` 버퍼만 OpenGL 컨텍스트로 넘겨 UI 스레드 블로킹(렉)을 0ms로 유지.

2. **`strata-3d` (`src/app/` — C++20 / Qt 6 Widgets / OpenGL 3.3 Core 데스크톱 `.exe` 앱)**
   - **하드웨어 가속 OpenGL 3.3 Core 뷰포트 (`KaGlTerrainViewport.cpp/.h` : `QOpenGLWidget`, `QOpenGLFunctions_3_3_Core`)**:
     - 기존 `Terrain3dService::renderPerspective`의 CPU 픽셀 루프를 대체하는 60FPS+ 하드웨어 GPU 렌더러.
     - Blender 4.2.3 `.rdata`에서 추출한 `datatoc_eevee_ambient_occlusion_lib_glsl`, `datatoc_eevee_horizon_scan_eval_lib_glsl`, `SKY_nishita`, `AgX` 색공간 변환을 OpenGL 3.3 Core GLSL 버텍스/프래그먼트 셰이더로 컴파일.
     - 위성실사 PBR 모드 / EEVEE Clay 미기복 강조 모드(고분·성곽·고도로 윤곽 판독용 Horizon-Scan GTAO) / 경사도·TPI 분석 히트맵 실시간 전환.
     - 건물/유구 GPU Instancing 및 Frustum Culling으로 수만 동 건물에서도 60FPS 유지.
   - **`Strata`(`hgis`) 디자인 시스템 1:1 유지 (`KaTheme` 공유)**:
     - `C:\Users\권을\Documents\hgis\src\app\KaTheme.h`, `KaTheme.cpp`, `KaThemeColors.cpp`, `KaThemePaper.cpp`, `data/theme/ka-hgis.qss`를 그대로 공유/컴파일하여 **기본(네이비·블루), 새 모양·조용한 도판(Paper), 하늘 눈밭(Sky), 번개 노랑(Bolt), 벚꽃 파스텔(Blossom)**, 고대비, 큰 글씨, `Malgun Gothic` 한 줄 리본·칩(`KaChip`) 디자인을 100% 동일하게 유지한다.
   - **`Strata`(`hgis`)와의 전 주기(계획 → 개발 → 평가 → 도면) 양방향 실시간 연동 (`StrataGisBridge.cpp/.h`)**:
     - `Strata`(`ka-hgis.exe`)에서 작업 중인 조사 프로젝트(`.gpkg` / `.qgz`), 현재 지도 캔버스 범위(`EPSG:5187`), 도메인 레이어(`survey_area`, `trial_trench`, `feature_poly` 등), 선택된 DEM 경로를 IPC(`QLocalServer`/`QLocalSocket` + 공유 프로젝트 상태 파일)로 즉시 수신.
     - `strata-3d.exe`에서 수행한 3D 고고학 입지 평가 결과, 생성한 시굴 트렌치/버퍼, 고해상도 3D 입체지형 렌더 이미지(FBO 오프스크린 3072×3072 캡처) 및 `Terrain3dService::composeSheet`(축척자·방위표·고도 범례·좌표계 포함 도면)를 `Strata`의 도면 스튜디오(`KaDrawingStudio`)와 보고서 스튜디오(`KaHwpReportStudio`)로 즉시 반환.
     - `Strata` 본체(`C:\Users\권을\Documents\hgis`)의 `KaTerrain3dStudio`에서도 버튼 하나로 `strata-3d.exe` 엔진을 호출하거나 내장 위젯으로 구동할 수 있게 연결.

---

## 2. REA MCP 활용 지침 (정보 수집 및 역공학 원칙)
1. **로컬 바이너리/번들 REA**:
   - `C:\Users\권을\Documents\hgis-3d-engine\artifacts\blender-4.2.3-windows-x64.zip` (호스트 OS에 설치 금지) 및 이미 추출된 `docs/rea/decompiled/*.json`을 활용하되, 추가 렌더 파이프라인(`draw_manager`, `eevee_next`, `gpu_viewport`)이나 메쉬 단순화/LOD 알고리즘이 필요하면 REA MCP(`open_binary`, `search_procedures`, `procedure_pseudo_code`, `read_bytes` 등)로 직접 디스어셈블·디컴파일하여 C++ 코드로 이식하라 (`close_binary` 필수).
2. **웹/외부 3D 지형·GIS 사이트 및 레퍼런스 REA**:
   - 고해상도 위성·3D 지형 타일 규격(VWorld 3D, Cesium/Quantized-Mesh, NGII, 국가유산청 공간정보 등)이나 최신 웹/앱 3D 엔진의 타일 스케줄링·LOD 기법이 필요할 때는 추측하지 말고 필요한 번들을 다운로드하거나 브라우저로 띄워 REA MCP(`inspect_web_page`, `analyze_web_bundle`, `analyze_javascript_application`, `inspect_web_network_capture`)로 패킷·스키마·알고리즘을 역공학해 C++ 엔진에 반영하라.

---

## 3. 단계별 수행 절차 (`C:\Users\권을\Documents\hgis\CLAUDE.md` 준수)

1. **1단계 — 기존 가짜 웹 코드 정리 및 기존 `Strata` 코드 정밀 확인**:
   - `C:\Users\권을\Documents\hgis`의 `CLAUDE.md`, `docs/agent/strata-rules-detail.md`, `src/core/Terrain3dService.h/.cpp`, `src/core/DemAnalyzer.h/.cpp`, `src/app/KaTerrain3dStudio.h/.cpp`, `src/app/KaTheme.h/.cpp`를 먼저 읽는다.
   - `C:\Users\권을\Documents\hgis-3d-engine`에서 `docs/rea/`와 `artifacts/`만 보존하고, 기존의 가짜 웹 코드(`server.js`, `public/`, `src/*.js`, `tests/*.js`, `scripts/*.js`, `package.json`)를 모두 제거한다.
2. **2단계 — CMake + MSVC + OSGeo4W 네이티브 빌드 파이프라인 구축**:
   - `C:\Users\권을\Documents\hgis-3d-engine\CMakeLists.txt`, `CMakePresets.json`, `scripts/dev-env.ps1`, `scripts/build-now.ps1`을 구성하여 `strata_3d_engine`(C++20 엔진 라이브러리), `strata-3d`(Qt6/OpenGL 데스크톱 `.exe`), `test_strata_3d`(QtTest 실행 파일)가 `C:\OSGeo4W`와 링크되어 빌드되게 한다.
   - Qt, QGIS, GDAL, PROJ, CMake API를 쓸 때는 추측하지 말고 `context7` 또는 `C:\OSGeo4W` 헤더를 직접 확인한다.
3. **3단계 — `strata_3d_engine` (실측 DEM/위성/건물 로더 + Blender C++ BVH/GLSL 커널 + 백그라운드 워커) 구현**:
   - 경주 지역(첨성대·월성·황룡사지·대릉원·남산·낭산·명활산 일대, `EPSG:5187`)의 **실제 DEM(NGII 수치지형도 / Copernicus COG)과 실제 위성 타일(VWorld / Google XYZ), 실제 건물·유적 데이터**를 로드하는지 검증하는 QtTest를 먼저 작성(TDD)하고 구현한다.
4. **4단계 — `strata-3d.exe` (`QOpenGLWidget` 60FPS GPU 뷰포트 + `KaTheme` UI + `Strata` 양방향 브릿지) 구현**:
   - `--smoke-quit` 인자를 지원하여 자동 스모크 테스트가 가능하도록 하고, 실제 화면을 띄워 경주 지형·위성영상·건물·유적 3D 렌더링과 더블클릭 고고학 평가, `composeSheet` 도면 출력, `Strata` 연동이 무렉으로 동작하는지 확인한다.
5. **5단계 — 빌드·시험·검증 완료 보고**:
   - Release 빌드 → QtTest 실행(`ctest -C Release --output-on-failure`) → `strata-3d.exe --smoke-quit` 통과 확인 후 커밋한다.
