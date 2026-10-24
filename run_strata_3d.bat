@echo off
setlocal
set "OSGEO4W_ROOT=C:\OSGeo4W"
set "PATH=%OSGEO4W_ROOT%\apps\Qt6\bin;%OSGEO4W_ROOT%\apps\gdal-dev\bin;%OSGEO4W_ROOT%\bin;%PATH%"
set "QT_PLUGIN_PATH=%OSGEO4W_ROOT%\apps\Qt6\plugins"
set "GDAL_DATA=%OSGEO4W_ROOT%\apps\gdal-dev\share\gdal"
set "PROJ_DATA=%OSGEO4W_ROOT%\share\proj"
set "PROJ_LIB=%OSGEO4W_ROOT%\share\proj"
start "" "%~dp0build\Release\strata-3d.exe" --bangri-preset gyeongju_hwangnyongsa %*
