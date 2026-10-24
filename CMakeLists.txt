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

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Network Concurrent Test)

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
  src/engine/BangriSurveyEvaluator.cpp)
target_include_directories(strata_3d_engine PUBLIC ${CMAKE_SOURCE_DIR}/src/engine ${S3D_GDAL_INCLUDES})
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

add_executable(strata-3d WIN32
  ${HGIS_ROOT}/data/theme/icons.qrc
  ${S3D_THEME_SOURCES}
  src/app/main.cpp
  src/app/MainWindow.cpp
  src/app/MainWindow.h
  src/app/BangriDockPanel.cpp
  src/app/BangriDockPanel.h
  src/app/ProfilePlot.cpp
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
target_include_directories(strata-3d PRIVATE ${CMAKE_SOURCE_DIR}/src/app ${CMAKE_SOURCE_DIR}/src/bridge ${HGIS_ROOT}/src/app ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(strata-3d PRIVATE strata_3d_engine Qt6::Widgets Qt6::OpenGLWidgets Qt6::OpenGL Qt6::Network Qt6::Concurrent)
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
