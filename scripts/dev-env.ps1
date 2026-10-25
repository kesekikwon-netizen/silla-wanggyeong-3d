$ErrorActionPreference = "Stop"
# hgis/scripts/dev-env.ps1 과 같은 순서로 OSGeo4W 를 찾는다: env → C:\OSGeo4W → D:\OSGeo4W → A:\OSGeo4W
$OSGEO = $null
if ($env:OSGEO4W_ROOT -and (Test-Path -LiteralPath $env:OSGEO4W_ROOT)) {
  $OSGEO = $env:OSGEO4W_ROOT
} elseif (Test-Path -LiteralPath "C:\OSGeo4W") {
  $OSGEO = "C:\OSGeo4W"
} elseif (Test-Path -LiteralPath "D:\OSGeo4W") {
  $OSGEO = "D:\OSGeo4W"
} elseif (Test-Path -LiteralPath "A:\OSGeo4W") {
  $OSGEO = "A:\OSGeo4W"
} else {
  throw "OSGEO4W_ROOT not found. Set OSGEO4W_ROOT or install OSGeo4W at C:\OSGeo4W."
}
$env:OSGEO4W_ROOT = $OSGEO
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
  foreach ($cmakeDir in @("${env:ProgramFiles}\CMake\bin", "C:\CMake\bin")) {
    if (Test-Path -LiteralPath (Join-Path $cmakeDir "cmake.exe")) {
      $env:PATH = "$cmakeDir;$env:PATH"
      break
    }
  }
}
# Strata 3D 앱은 Strata 의 ka_core(QGIS)를 링크한다: qgis-dev 의 DLL·플러그인·웹 브라우저 경로가 hgis 와 같아야 한다.
$qgis = Join-Path $OSGEO "apps\qgis-dev"
if (-not (Test-Path $qgis)) { throw "qgis-dev missing under $OSGEO" }
$env:QGIS_PREFIX_PATH = $qgis
$bins = @(
  (Join-Path $qgis "bin"),
  (Join-Path $OSGEO "apps\Qt6\bin"),
  (Join-Path $OSGEO "apps\gdal-dev\bin"),
  (Join-Path $OSGEO "apps\pdal-dev\bin"),
  (Join-Path $OSGEO "bin")
)
$env:PATH = ($bins + $env:PATH) -join ";"
$env:QT_PLUGIN_PATH = (Join-Path $OSGEO "apps\Qt6\plugins") + ";" + (Join-Path $qgis "qtplugins")
$env:QGIS_PLUGIN_PATH = Join-Path $qgis "plugins"
$env:QTWEBENGINEPROCESS_PATH = Join-Path $OSGEO 'apps/Qt6/bin/QtWebEngineProcess.exe'
$env:QTWEBENGINE_RESOURCES_PATH = Join-Path $OSGEO 'apps/Qt6/resources'
$env:QTWEBENGINE_LOCALES_PATH = Join-Path $OSGEO 'apps/Qt6/translations/qtwebengine_locales'
$gdalData = Join-Path $OSGEO "apps\gdal-dev\share\gdal"
if (Test-Path $gdalData) { $env:GDAL_DATA = $gdalData }
$proj = Join-Path $OSGEO "share\proj"
if (Test-Path $proj) { $env:PROJ_DATA = $proj; $env:PROJ_LIB = $proj }
$env:CMAKE_BUILD_PARALLEL_LEVEL = "$([Environment]::ProcessorCount)"
$env:CTEST_PARALLEL_LEVEL = "$([Environment]::ProcessorCount)"
Write-Host "OSGEO4W_ROOT=$env:OSGEO4W_ROOT"
