cmake_minimum_required(VERSION 3.21)
project(strata-3d VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(MSVC)
  # hgis/CMakeLists.txt 와 같은 컴파일 옵션
  add_compile_options(/MP /Zc:__cplusplus /utf-8 /permissive- /W4 /external:anglebrackets /external:W0)
endif()

# 메인 GIS 앱(hgis). KaTheme 소스와 qss 를 복사하지 않고 여기서 직접 컴파일해 디자인을 1:1 공유한다.
set(HGIS_ROOT "${CMAKE_SOURCE_DIR}/../hgis" CACHE PATH "hgis (Strata) 소스 루트")
if(NOT EXISTS "${HGIS_ROOT}/src/app/KaTheme.cpp")
  message(FATAL_ERROR "HGIS_ROOT 에 KaTheme.cpp 가 없습니다: ${HGIS_ROOT}")
endif()

if(WIN32)
  if(EXISTS "C:/OSGeo4W")
    set(_osgeo_default "C:/OSGeo4W")
  elseif(EXISTS "D:/OSGeo4W")
    set(_osgeo_default "D:/OSGeo4W")
  elseif(EXISTS "A:/OSGeo4W")
    set(_osgeo_default "A:/OSGeo4W")
  endif()
  set(OSGEO4W_ROOT "${_osgeo_default}" CACHE PATH "OSGeo4W root")
  list(PREPEND CMAKE_PREFIX_PATH "${OSGEO4W_ROOT}/apps/Qt6" "${OSGEO4W_ROOT}/apps/Qt6/lib/cmake")
  set(S3D_GDAL_INCLUDES "${OSGEO4W_ROOT}/apps/gdal-dev/include" "${OSGEO4W_ROOT}/include")
  set(S3D_GDAL_LIBDIRS "${OSGEO4W_ROOT}/apps/gdal-dev/lib" "${OSGEO4W_ROOT}/lib")
  set(S3D_GDAL_LIB gdal_i)
else()
  set(S3D_GDAL_INCLUDES /usr/include/gdal)
  set(S3D_GDAL_LIBDIRS /usr/lib /usr/lib/x86_64-linux-gnu)
  set(S3D_GDAL_LIB gdal)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Network Concurrent Test Svg PrintSupport)

# ---- 엔진: UI 코드 없음 -------------------------------------------------------------------
add_library(strata_3d_engine STATIC
  src/engine/DemGrid.cpp
  src/engine/CrsTransform.cpp
  src/engine/DemProvider.cpp
  src/engine/Frustum.cpp
  src/engine/TerrainLodTree.cpp
  src/engine/HttpFetch.cpp
  src/engine/OsmProvider.cpp
  src/engine/KhsProvider.cpp
  src/engine/LocalVectorProvider.cpp
  src/engine/VworldKey.cpp
  src/engine/Triangulate.cpp
  src/engine/SceneMesh.cpp
  src/engine/SatelliteProvider.cpp
  src/engine/Kdop14Bvh.cpp
  src/engine/KdTree3.cpp
  src/engine/TerrainBvh.cpp
  src/engine/Archaeology3dEvaluator.cpp
  src/engine/EvaluationOverlay.cpp
  src/engine/EvaluationJson.cpp
  src/engine/StrataSurvey.cpp
  src/engine/AgxView.cpp
  src/engine/NishitaSky.cpp
  src/engine/DbpiaBangriLedger.cpp
  src/engine/SillaMoundReconstructor.cpp
  src/engine/BangriGridEngine.cpp
  src/engine/FossilizedRoadAnalyzer.cpp
  src/engine/BangriTerrainValidator.cpp
  src/engine/BangriSurveyEvaluator.cpp
  src/engine/HeritageNaming.cpp
  src/engine/EvidenceTier.cpp
  src/engine/KhsSpcaProvider.cpp
  src/engine/CrossCheck.cpp
  src/engine/RelationEngine.cpp
  src/engine/StrataProjectLayers.cpp
  src/engine/DrapeRaster.cpp
  # Strata(hgis) core 중 QGIS 무관 파일을 그대로 같이 컴파일한다(docs/STRATA_REUSE.md)
  ${HGIS_ROOT}/src/core/DemAnalyzer.cpp)
target_include_directories(strata_3d_engine PUBLIC ${CMAKE_SOURCE_DIR}/src/engine ${S3D_GDAL_INCLUDES} ${HGIS_ROOT}/src/core)
target_link_directories(strata_3d_engine PUBLIC ${S3D_GDAL_LIBDIRS})
target_link_libraries(strata_3d_engine PUBLIC Qt6::Core Qt6::Gui ${S3D_GDAL_LIB})
target_compile_definitions(strata_3d_engine PUBLIC S3D_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
if(MSVC)
  target_compile_options(strata_3d_engine PRIVATE /openmp)  # BVH 만들기 등 병렬 반복
endif()

# ---- 앱: hgis 의 KaTheme 를 그대로 컴파일 -------------------------------------------------
include(cmake/EmbedQss.cmake)
set(S3D_THEME_SOURCES
  ${HGIS_ROOT}/src/app/KaTheme.cpp
  ${HGIS_ROOT}/src/app/KaThemeSheet.cpp
  ${HGIS_ROOT}/src/app/KaThemeStyle.cpp
  ${HGIS_ROOT}/src/app/KaThemeOptions.cpp
  ${HGIS_ROOT}/src/app/KaThemeFonts.cpp
  ${HGIS_ROOT}/src/app/KaThemePaper.cpp
  ${HGIS_ROOT}/src/app/KaThemeColors.cpp)

# Strata 리본·아이콘·칩·상태줄(QGIS 무관 파일)을 그대로 컴파일한다. 아이콘 표는 S3dIconsMap.cpp 가
# Strata KaIconsMockupMap.cpp 를 들여와 쓰고 3D 전용 단추만 덧붙인다(hgis 파일은 고치지 않는다).
set(S3D_STRATA_UI_SOURCES
  ${HGIS_ROOT}/src/app/KaBeginnerRibbon.cpp
  ${HGIS_ROOT}/src/app/KaChip.cpp
  ${HGIS_ROOT}/src/app/KaStatusBar.cpp
  ${HGIS_ROOT}/src/app/KaIcons.cpp
  ${HGIS_ROOT}/src/app/KaIconsOutlineA.cpp
  ${HGIS_ROOT}/src/app/KaIconsOutlineB.cpp
  ${HGIS_ROOT}/src/app/KaIconsOutlineEngine.cpp
  ${HGIS_ROOT}/src/app/KaIconsOutlineUi.cpp
  ${HGIS_ROOT}/src/app/KaIconsMockupEngine.cpp
  ${HGIS_ROOT}/data/icons/icons-mockup.qrc
  ${CMAKE_SOURCE_DIR}/data/icons/strata3d-icons.qrc
  src/app/S3dIconsMap.cpp
  src/app/RibbonSpec.cpp
  src/app/RibbonSpec.h)

add_executable(strata-3d WIN32
  ${HGIS_ROOT}/data/theme/icons.qrc
  ${S3D_THEME_SOURCES}
  ${S3D_STRATA_UI_SOURCES}
  src/app/main.cpp
  src/app/MainWindow.cpp
  src/app/MainWindow.h
  src/app/ProfilePlot.cpp
  src/app/panels/RelationPanel.cpp
  src/app/panels/RelationPanel.h
  src/app/ProfilePlot.h
  src/app/KaGlTerrainViewport.cpp
  src/app/KaGlTerrainViewport.h
  src/app/SheetComposer.cpp
  src/app/StrataBridge.cpp
  src/app/StrataBridge.h
  src/bridge/StrataBridgeClient.cpp
  src/bridge/StrataBridgeClient.h
  src/app/SheetComposer.h
  src/app/TerrainShaders.cpp
  src/app/PostShaders.cpp
  src/app/PostShaders.h
  src/app/HdrPipeline.cpp
  src/app/HdrPipeline.h
  src/app/TerrainShaders.h)
target_include_directories(strata-3d PRIVATE ${CMAKE_SOURCE_DIR}/src/app ${CMAKE_SOURCE_DIR}/src/bridge ${HGIS_ROOT}/src/app ${HGIS_ROOT}/src ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(strata-3d PRIVATE strata_3d_engine Qt6::Widgets Qt6::OpenGLWidgets Qt6::OpenGL Qt6::Network Qt6::Concurrent Qt6::Svg Qt6::PrintSupport)

# ---- Strata ka_core(QGIS) 링크: 조사 열기·레이어 칸·속성 칸·주소 찾기를 Strata 코드 그대로 쓴다 -----------------
# hgis 를 먼저 Release 로 빌드해 둔 ka_core.lib 를 가져다 쓴다(설계도 v3 7장, 10장 결정 ②; spikes/ka_core_link 로 확인).
# 엔진(strata_3d_engine)과 시험은 QGIS 없이 그대로 둔다. 실행에는 scripts/dev-env.ps1 의 qgis-dev 경로가 필요하다.
if(WIN32)
  set(QGIS_PREFIX_PATH "${OSGEO4W_ROOT}/apps/qgis-dev" CACHE PATH "QGIS (OSGeo4W qgis-dev)")
  set(S3D_KA_CORE_LIB "${HGIS_ROOT}/build/Release/ka_core.lib" CACHE FILEPATH "hgis 가 빌드한 ka_core.lib")
  if(NOT EXISTS "${S3D_KA_CORE_LIB}")
    message(FATAL_ERROR "ka_core.lib 가 없습니다: ${S3D_KA_CORE_LIB} (hgis 를 먼저 Release 로 빌드하세요)")
  endif()
  find_package(Qt6 REQUIRED COMPONENTS Xml Pdf WebEngineWidgets)
  add_library(ka_core STATIC IMPORTED)
  set_target_properties(ka_core PROPERTIES IMPORTED_LOCATION "${S3D_KA_CORE_LIB}")
  target_sources(strata-3d PRIVATE
    ${HGIS_ROOT}/src/app/KaLayerInformation.cpp
    ${HGIS_ROOT}/src/app/KaLayerInformationView.cpp
    ${HGIS_ROOT}/src/app/KaLayerInformationPanel.cpp
    ${HGIS_ROOT}/src/app/KaLayerSectionDelegate.cpp
    ${HGIS_ROOT}/src/app/KaLayerListChrome.cpp
    ${HGIS_ROOT}/src/app/KaInspectorPanel.cpp
    ${HGIS_ROOT}/src/app/KaInspectorStyle.cpp
    ${HGIS_ROOT}/src/app/KaFeatureCard.cpp
    ${HGIS_ROOT}/src/app/KaFeatureCardEdit.cpp
    ${HGIS_ROOT}/src/app/KaFeatureCardEditors.cpp
    ${HGIS_ROOT}/src/app/KaFeatureCardPlate.cpp
    ${HGIS_ROOT}/src/app/KaFeatureFormDialog.cpp
    ${HGIS_ROOT}/src/app/KaSymbolSwatch.cpp
    ${HGIS_ROOT}/src/app/KaAppBar.cpp
    ${HGIS_ROOT}/src/app/KaRegionLocator.cpp
    ${HGIS_ROOT}/src/app/KaNewSurveyDialog.cpp
    ${HGIS_ROOT}/src/app/KaLongitudeRuler.cpp
    ${HGIS_ROOT}/src/app/KaHeritageBrowser.cpp
    ${HGIS_ROOT}/src/app/KaHeritageBrowserPlan.cpp
    ${HGIS_ROOT}/src/app/KaHeritageSetupDialogs.cpp
    ${HGIS_ROOT}/src/app/KaDownloadUi.cpp
    ${HGIS_ROOT}/src/app/KaReferenceDownloadJob.cpp
    ${HGIS_ROOT}/src/app/KaCrashGuard.cpp
    src/app/StrataReceive.cpp
    src/app/StrataReceive.h
    src/app/ProjectSurvey.cpp
    src/app/ProjectSurvey.h
    src/app/StrataLayerDock.cpp
    src/app/StrataLayerDock.h
    src/app/panels/HeritageCard.cpp
    src/app/panels/HeritageCard.h)
  target_include_directories(strata-3d PRIVATE ${QGIS_PREFIX_PATH}/include)
  target_link_directories(strata-3d PRIVATE ${QGIS_PREFIX_PATH}/lib)
  # hgis 앱과 같은 정의: QGIS 가 있는 빌드(헤더의 #if KA_HGIS_HAS_QGIS 가 hgis 와 같은 모양이 되게)
  target_compile_definitions(strata-3d PRIVATE KA_HGIS_HAS_QGIS=1 HWPSDK_STATIC
    "CORE_EXPORT=__declspec(dllimport)" "GUI_EXPORT=__declspec(dllimport)" "KA_HGIS_VERSION=\"strata-3d\"")
  target_link_libraries(strata-3d PRIVATE ka_core Qt6::Xml Qt6::Pdf Qt6::WebEngineWidgets qgis_core qgis_gui crypt32 sqlite3_i dbghelp)
  # 릴리스에도 PDB 를 만든다: 충돌하면 Strata KaCrashGuard 가 함수 이름·줄 번호까지 남긴다(hgis 와 같은 설정)
  if(MSVC)
    target_compile_options(strata-3d PRIVATE "$<$<CONFIG:Release>:/Zi>" "$<$<CONFIG:Release>:/FS>")
    target_link_options(strata-3d PRIVATE "$<$<CONFIG:Release>:/DEBUG>" "$<$<CONFIG:Release>:/OPT:REF>" "$<$<CONFIG:Release>:/OPT:ICF>")
  endif()
  # 주소·지번 찾기의 시·군·동 목록(Strata 와 같은 파일)
  add_custom_command(TARGET strata-3d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${HGIS_ROOT}/data/korea_dongs.json" "$<TARGET_FILE_DIR:strata-3d>/data/korea_dongs.json")
endif()
if(WIN32)
  target_link_libraries(strata-3d PRIVATE dwmapi)
endif()

# ---- 시험 ---------------------------------------------------------------------------------
enable_testing()
add_executable(test_strata_3d tests/test_strata_3d.cpp)
target_link_libraries(test_strata_3d PRIVATE strata_3d_engine Qt6::Test)
set_target_properties(test_strata_3d PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME strata_3d_engine COMMAND test_strata_3d)
set_tests_properties(strata_3d_engine PROPERTIES TIMEOUT 300)

add_executable(test_bangri_grid tests/test_bangri_grid.cpp)
target_link_libraries(test_bangri_grid PRIVATE strata_3d_engine Qt6::Test)
set_target_properties(test_bangri_grid PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME bangri_grid_unit COMMAND test_bangri_grid)
set_tests_properties(bangri_grid_unit PROPERTIES TIMEOUT 120)

add_executable(test_heritage_trust tests/test_heritage_trust.cpp)
target_link_libraries(test_heritage_trust PRIVATE strata_3d_engine Qt6::Test)
set_target_properties(test_heritage_trust PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME heritage_trust COMMAND test_heritage_trust)
set_tests_properties(heritage_trust PROPERTIES TIMEOUT 120)

add_executable(test_cross_check tests/test_cross_check.cpp)
target_link_libraries(test_cross_check PRIVATE strata_3d_engine Qt6::Test)
set_target_properties(test_cross_check PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME cross_check COMMAND test_cross_check)
set_tests_properties(cross_check PROPERTIES TIMEOUT 180)

add_executable(test_relations tests/test_relations.cpp)
target_link_libraries(test_relations PRIVATE strata_3d_engine Qt6::Test)
set_target_properties(test_relations PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME relations COMMAND test_relations)
set_tests_properties(relations PROPERTIES TIMEOUT 300)

# ---- 연동 시험: 실제 strata-3d.exe 를 띄워 파이프로 말을 건다 -----------------------------------
add_executable(test_strata_bridge tests/test_strata_bridge.cpp src/bridge/StrataBridgeClient.cpp)
target_include_directories(test_strata_bridge PRIVATE ${CMAKE_SOURCE_DIR}/src/bridge)
target_compile_definitions(test_strata_bridge PRIVATE
  S3D_HGIS_ROOT="${HGIS_ROOT}"
  S3D_APP_EXE="$<TARGET_FILE:strata-3d>")
target_link_libraries(test_strata_bridge PRIVATE Qt6::Test Qt6::Network Qt6::Gui)
set_target_properties(test_strata_bridge PROPERTIES WIN32_EXECUTABLE FALSE)
add_dependencies(test_strata_bridge strata-3d)
add_test(NAME strata_bridge_e2e COMMAND test_strata_bridge)
set_tests_properties(strata_bridge_e2e PROPERTIES TIMEOUT 600)

# 조사 파일 읽기 시험은 엔진 시험에 넣는다(hgis 의 시험용 표본을 읽기 전용으로).
target_compile_definitions(test_strata_3d PRIVATE S3D_HGIS_ROOT="${HGIS_ROOT}")

# 색 관리 자료(AgX LUT)와 시험 기준표 위치
target_compile_definitions(test_strata_3d PRIVATE S3D_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
# 앱 옆에 data/ 를 복사한다(AgX LUT 를 실행 파일 옆에서 찾는다)
add_custom_command(TARGET strata-3d POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:strata-3d>/data)
add_custom_command(TARGET test_strata_3d POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:test_strata_3d>/data)

# 실제 Strata 조사(사용자가 그린 경주 조사)의 프로젝트 기록·구역 판정 시험
add_executable(test_strata_project tests/test_strata_project.cpp)
target_link_libraries(test_strata_project PRIVATE strata_3d_engine Qt6::Test)
set_target_properties(test_strata_project PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME strata_project COMMAND test_strata_project)
set_tests_properties(strata_project PROPERTIES TIMEOUT 300)

# 리본 단추표 검사(설계도 v3 9장 0단계 완료 기준): Strata 와 같은 묶음 순서·아이콘 이름, 아이콘 그림이 실제로 있는지
add_executable(test_ribbon_spec tests/test_ribbon_spec.cpp src/app/RibbonSpec.cpp src/app/S3dIconsMap.cpp
  ${HGIS_ROOT}/data/icons/icons-mockup.qrc ${CMAKE_SOURCE_DIR}/data/icons/strata3d-icons.qrc)
target_include_directories(test_ribbon_spec PRIVATE ${CMAKE_SOURCE_DIR}/src/app ${HGIS_ROOT}/src/app)
target_compile_definitions(test_ribbon_spec PRIVATE S3D_HGIS_ROOT="${HGIS_ROOT}")
target_link_libraries(test_ribbon_spec PRIVATE Qt6::Gui Qt6::Test)
set_target_properties(test_ribbon_spec PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME ribbon_spec COMMAND test_ribbon_spec)
set_tests_properties(ribbon_spec PROPERTIES TIMEOUT 60)

# 실제 앱(strata-3d.exe)을 띄우는 끝까지 시험(Strata ka_core 링크 부분). 네트워크(Copernicus·OSM·국가유산청)를 쓴다.
#  · new_survey_e2e: 3D 에서 새 조사 → 조사구역 80 m 그리기 → Strata 저장 방식(persistWork) → GDAL 로 다시 읽어 넓이·좌표계·프로젝트 확인
#  · gyeongju_select_e2e: 사용자가 Strata 에서 그린 경주 조사를 열어 「경주 대릉원 일원」을 고르면 속성 칸에 기록이 열리는지
add_test(NAME new_survey_e2e COMMAND strata-3d --no-satellite --e2e-new-survey ${CMAKE_BINARY_DIR}/e2e_surveys --smoke-quit)
set_tests_properties(new_survey_e2e PROPERTIES TIMEOUT 240)
file(TO_CMAKE_PATH "$ENV{USERPROFILE}/Documents/경주시/경주시.gpkg" S3D_GYEONGJU_SURVEY)
add_test(NAME gyeongju_select_e2e COMMAND strata-3d --no-satellite --open-survey ${S3D_GYEONGJU_SURVEY} --select "경주 대릉원 일원" --smoke-quit)
set_tests_properties(gyeongju_select_e2e PROPERTIES TIMEOUT 240 FAIL_REGULAR_EXPRESSION "기록 없음")
#  · receive_soil_e2e: 경주 조사 사본(매번 새로 복사)에 Strata 받기 흐름으로 토양도를 받아 3D 지형에 입혔는지(원본은 건드리지 않는다)
get_filename_component(S3D_GYEONGJU_DIR "${S3D_GYEONGJU_SURVEY}" DIRECTORY)
add_test(NAME receive_copy_clean COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_BINARY_DIR}/e2e_recv")
set_tests_properties(receive_copy_clean PROPERTIES FIXTURES_SETUP gyeongju_copy)
add_test(NAME receive_copy_setup COMMAND ${CMAKE_COMMAND} -E copy_directory "${S3D_GYEONGJU_DIR}" "${CMAKE_BINARY_DIR}/e2e_recv/경주시")
set_tests_properties(receive_copy_setup PROPERTIES FIXTURES_SETUP gyeongju_copy DEPENDS receive_copy_clean)
add_test(NAME receive_soil_e2e COMMAND strata-3d --open-survey "${CMAKE_BINARY_DIR}/e2e_recv/경주시/경주시.gpkg" --e2e-receive soil --smoke-quit)
set_tests_properties(receive_soil_e2e PROPERTIES TIMEOUT 480 FIXTURES_REQUIRED gyeongju_copy)
