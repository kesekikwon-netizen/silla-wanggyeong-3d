# [Claude Code 작업 위임 명령서] 경주 실사 3D 지형·건물·고고학 평가 독립 엔진(`strata_3d_engine`) 및 데스크톱 앱(`strata-3d.exe`) 전수 구현

---

## 0. 현재 상태 구분: [이미 된 것] vs [안 되어 있어서 네가 반드시 해야 할 것]

### A. 이미 되어 있는 것 (보존 및 재사용할 것)
1. **원본 블렌더 아카이브 (설치 금지)**:
   - `C:\Users\권을\Documents\hgis-3d-engine\artifacts\blender-4.2.3-windows-x64.zip`
2. **1차 추출된 REA 문서 및 디컴파일 JSON (`docs/rea/`)**:
   - `docs/rea/blender_pdb_libraries.json` (252개 정적 라이브러리·5,919개 번역 단위)
   - `docs/rea/blender_binaries_inventory.json` (130개 바이너리 해시)
   - `docs/rea/decompiled/blender_core_symbols.json` (468개 심볼 VA 및 오프셋)
   - `docs/rea/decompiled/blender_core_functions_disassembly.json` (12개 함수 어셈블리 및 13축 K-DOP 상수 테이블)
   - `docs/rea/decompiled/blender_eevee_next_curated_shaders.json` (175개 EEVEE Next GLSL 셰이더 목록 및 6종 원문)

### B. 아직 안 되어 있거나 가짜로 되어 있어 네가 처음부터 끝까지 수행해야 할 작업 (필수 완수 목록)
이전 에이전트가 만들어 놓은 코드는 **C++ 데스크톱 앱이 아니라 Node.js + Three.js 웹페이지**이며, **경주 지형은 가우시안 수식(`Math.exp`), 위성사진은 단색 칠하기, 건물은 난수 박스 950개, 블렌더 이식은 주석 눈속임, `Strata` 공유는 연결 없는 로컬 JSON 파일 저장**으로 되어 있다.

**네가 반드시 완수해야 할 "안 한 것" 4대 과제는 다음과 같다:**

- [ ] **[미완료 1] 블렌더 미추출 핵심 렌더/메쉬 파이프라인 추가 REA 디컴파일 & 실제 위성/3D GIS 사이트 전수 REA 역공학**
  1. `blender.exe` / `blender.pdb`에서 아직 디컴파일하지 않은 **`BKE_pbvh`(지형/메쉬 분할·컬링·노멀 갱신), `DRW_manager`(드로우콜 배칭·인스턴싱), `eevee_next`(그림자·라이트프로브·하이트맵/노멀셰이딩·AgX 색변환 전체 셰이더)**를 REA MCP(`open_binary`, `procedure_pseudo_code`, `read_function_instructions`, `read_bytes`)로 빠짐없이 디컴파일하고 C++20/GLSL로 직접 이식하라. (작업 후 `close_binary` 필수)
  2. 외부 3D 지형·위성·유적 정보 사이트(VWorld 3D `xdworld.vworld.kr`, Google Earth/Satellite 타일, 국토지리정보원 수치지형도, 국가유산청 공간정보 서비스 등)와 필요한 앱/번들을 직접 다운로드하거나 헤드리스 브라우저로 열어 **REA MCP(`inspect_web_page`, `analyze_web_bundle`, `analyze_javascript_application`, `inspect_web_network_capture`)로 타일 URI·지형 메쉬 규격·건물 3D 포맷을 전수 역공학**해 C++ 로더에 반영하라.

- [ ] **[미완료 2] 가짜 웹 코드 전량 폐기 및 C++20 / Qt6 / OpenGL / OSGeo4W 기반 "엔진(`strata_3d_engine`) + 앱(`strata-3d.exe`)" 분리 구축**
  1. `C:\Users\권을\Documents\hgis-3d-engine` 내의 가짜 웹 파일(`server.js`, `public/`, `src/*.js`, `tests/*.js`, `scripts/*.js`, `package.json`, `dist/`, `shared_gis_bus/`)을 전량 삭제하라.
  2. `C:\Users\권을\Documents\hgis`와 동일한 **Visual Studio 17 2022 (x64) + CMake + `C:\OSGeo4W` (`Qt6`, `qgis-dev`, `gdal-dev`, `proj`)** 환경으로 `CMakeLists.txt`와 `CMakePresets.json`을 구축하라.
  3. **엔진(`src/engine/` → `strata_3d_engine` C++20 라이브러리)**과 **앱(`src/app/` → `strata-3d.exe` Qt6 데스크톱 실행 파일)**을 물리적·스레드 단위로 완전히 분리하여, 무거운 DEM/위성/메쉬/BVH/평가 연산이 메인 UI 스레드를 절대 막지 않게(`QThreadPool` + `OpenMP` 비동기 파이프라인) 설계하라.

- [ ] **[미완료 3] 경주(慶州) 지역 실제 DEM + 실제 위성사진 + 실제 건물·유적 3D 구현 및 실측 고고학 평가**
  1. **가짜 수식·난수 박스 절대 금지**:
     - **지형**: `GDAL`(`gdal_i.lib`) 및 `hgis`의 `DemAnalyzer` / `LayerOps::copernicusCogUriForWgs84` / 국토지리정보원 1:5,000 수치지형도(`TopographicCatalog`, `TopographicShapefileCache`)를 통해 **실제 경주 분지·남산·토함산·낭산·명활산·형산강의 실측 표고 데이터**를 읽어 크랙 없는 고해상도 멀티-LOD 3D 지형 메쉬를 생성하라.
     - **위성사진**: VWorld WMTS(API 키 로드 순서 준수: `%LOCALAPPDATA%\ka-hgis\ka-hgis-vworld.ini` → `HKCU\Software\ka-hgis` → `VWORLD_API_KEY` → `config/secrets.ini`) 및 Google Satellite XYZ 실사 위성 정사영상 타일을 비동기로 받아 3D 지형 위에 고해상도로 드레이핑하라.
     - **건물 및 유적 3D**: 실제 수치지형도 건물 폴리곤(층수/높이 기반 3D Extrusion), 국가유산청 문화유산 공간데이터(`NearbyHeritageCollect`, `HeritageImport`), 조사 프로젝트 GPKG 도메인 레이어(`survey_area`, `feature_poly`, `feature_line`, `section_line`, `control_points`, `artifact_point`, `trial_trench`)를 실제 좌표(`EPSG:5187`)로 3D 지형 위에 정밀 배치하라.
  2. **OpenGL 3.3 Core 하드웨어 가속 렌더링 (`QOpenGLWidget` + `QOpenGLFunctions_3_3_Core`)**:
     - 기존 `Strata`의 `Terrain3dService::renderPerspective()`(단일 스레드 CPU 이중 `for` 픽셀 루프)와 `maxEdge = 160` 제한을 대체하여, 추출한 Blender **EEVEE Next Horizon-Scan GTAO + Nishita 물리 대기 산란 + AgX 톤매핑 GLSL 셰이더**를 실제 GPU에서 60FPS로 구동하라.
  3. **경주 3D 고고학 정밀 평가**:
     - 조사구역 또는 3D 지형 클릭 위치 기준 **실제 반경 500m / 1,000m `JoinStyle::Round` 버퍼** 내 지정·비지정 문화유산 이격거리 및 방위 계산,
     - 실측 DEM 기반 경사도·향·TPI·미지형(하안단구/선상지/구릉/구하도) 입지 분석,
     - `BLI_kdopbvh` 3D 레이캐스팅 기반 왕경 거점 상호 가시권(Viewshed) 차폐 분석,
     - 시굴 트렌치(`trial_trench`) 3D 지형 단면 및 토층 프로파일 평가를 구현하라.

- [ ] **[미완료 4] 메인 GIS 앱(`Strata`, `C:\Users\권을\Documents\hgis`)과의 디자인 1:1 유지 및 계획~개발~평가~도면 전 주기 실시간 공유**
  1. **디자인 1:1 공유**:
     - `C:\Users\권을\Documents\hgis\src\app\KaTheme.h`, `KaTheme.cpp`, `KaThemeColors.cpp`, `KaThemePaper.cpp`, `data/theme/ka-hgis.qss`를 그대로 적용하여 5종 테마(`기본`, `새 모양·조용한 도판`, `하늘 눈밭`, `번개 노랑`, `벚꽃 파스텔`), 고대비, 큰 글씨, `Malgun Gothic` 한 줄 리본·칩(`KaChip`) UI를 100% 동일하게 유지하라.
  2. **양방향 실시간 연동 (`C:\Users\권을\Documents\hgis` ↔ `C:\Users\권을\Documents\hgis-3d-engine`)**:
     - 혼자 JSON만 쓰는 가짜 공유가 아니라, `Strata`(`ka-hgis.exe`)와 `strata-3d.exe`가 **실제 IPC(`QLocalServer`/`QLocalSocket`) 및 프로젝트 세션/GPKG**로 연결되게 하라.
     - `Strata`에서 계획·열람 중인 프로젝트(`.gpkg`/`.qgz`), 현재 지도 화면 범위(`EPSG:5187`), 조사구역·트렌치·유구 레이어 변경 사항이 `strata-3d.exe`에 즉시 반영되고,
     - `strata-3d.exe`에서 렌더링한 고해상도 3D 입체지형 뷰, `Terrain3dService::composeSheet` 도면(축척자·방위표·범례·좌표계), 3D 고고학 평가 결과 및 시굴 트렌치/버퍼 레이어가 `Strata`의 `KaTerrain3dStudio`, `KaDrawingStudio`(도면 스튜디오), `KaHwpReportStudio`(평가서/보고서 스튜디오)로 즉시 공유·반환되게 하라.

---

## 1. 작업 규칙 및 검증 절차 (`C:\Users\권을\Documents\hgis\CLAUDE.md` 준수)
1. **사전 코드 숙지**:
   - 작업 시작 전 반드시 `C:\Users\권을\Documents\hgis\CLAUDE.md`, `docs/agent/strata-rules-detail.md`, `src/core/Terrain3dService.h/.cpp`, `src/core/DemAnalyzer.h/.cpp`, `src/app/KaTerrain3dStudio.h/.cpp`, `src/app/KaTheme.h/.cpp`를 정독하라.
   - Qt, QGIS, GDAL, PROJ, CMake API 호출 전에는 추측하지 말고 `context7` 또는 `C:\OSGeo4W`의 실제 헤더를 확인하라.
2. **TDD 및 실제 데이터 검증**:
   - 가짜 데이터를 검사하는 테스트는 금지한다. 실제 경주 좌표(`EPSG:5187`), 실제 DEM 래스터, 실제 위성/레이어 드레이핑, 실제 BVH 레이캐스팅, 실제 `Strata` 브릿지 송수신을 검증하는 `QtTest`(`ctest -C Release --output-on-failure`)를 작성하고 통과시켜라.
3. **스모크 및 실행 검증**:
   - Release 빌드 완료 후 `strata-3d.exe --smoke-quit` (종료 코드 0) 및 `ka-hgis.exe --smoke-quit`을 모두 검증하라.
