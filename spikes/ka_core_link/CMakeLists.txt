# 0단계 위험 확인: Strata(hgis)의 ka_core(QGIS 포함)를 다른 실행 파일에 링크할 수 있는가.
# hgis 를 먼저 Release 로 빌드해 둔 ka_core.lib 를 가져다 쓴다(설계도 v3 10장 결정 ②: 소스는 hgis 그대로).
cmake_minimum_required(VERSION 3.21)
project(ka_core_link_spike LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
if(MSVC)
  add_compile_options(/MP /Zc:__cplusplus /utf-8 /permissive-)
endif()
set(HGIS_ROOT "${CMAKE_SOURCE_DIR}/../../../hgis" CACHE PATH "hgis 소스 루트")
set(OSGEO4W_ROOT "C:/OSGeo4W" CACHE PATH "OSGeo4W")
set(QGIS_PREFIX_PATH "${OSGEO4W_ROOT}/apps/qgis-dev")
list(PREPEND CMAKE_PREFIX_PATH "${OSGEO4W_ROOT}/apps/Qt6" "${OSGEO4W_ROOT}/apps/Qt6/lib/cmake")
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Xml PrintSupport Pdf Network Svg)

add_library(ka_core STATIC IMPORTED)
set_target_properties(ka_core PROPERTIES IMPORTED_LOCATION "${HGIS_ROOT}/build/Release/ka_core.lib")

add_executable(ka_core_link_spike main.cpp)
target_include_directories(ka_core_link_spike PRIVATE ${HGIS_ROOT}/src ${QGIS_PREFIX_PATH}/include
                           ${OSGEO4W_ROOT}/apps/gdal-dev/include ${OSGEO4W_ROOT}/include)
target_link_directories(ka_core_link_spike PRIVATE ${QGIS_PREFIX_PATH}/lib ${OSGEO4W_ROOT}/apps/gdal-dev/lib ${OSGEO4W_ROOT}/lib)
target_compile_definitions(ka_core_link_spike PRIVATE KA_HGIS_HAS_QGIS=1 HWPSDK_STATIC
                           "CORE_EXPORT=__declspec(dllimport)" "GUI_EXPORT=__declspec(dllimport)")
target_link_libraries(ka_core_link_spike PRIVATE ka_core Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Xml Qt6::PrintSupport
                      Qt6::Pdf Qt6::Network Qt6::Svg qgis_core qgis_gui gdal_i crypt32 sqlite3_i)
