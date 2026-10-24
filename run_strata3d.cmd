@echo off
set OSGEO4W_ROOT=C:\OSGeo4W
set QGIS_PREFIX_PATH=C:\OSGeo4W\apps\qgis-dev
set PATH=C:\OSGeo4W\apps\qgis-dev\bin;C:\OSGeo4W\apps\Qt6\bin;C:\OSGeo4W\apps\gdal-dev\bin;C:\OSGeo4W\apps\pdal-dev\bin;C:\OSGeo4W\bin;%PATH%
set QT_PLUGIN_PATH=C:\OSGeo4W\apps\Qt6\plugins;C:\OSGeo4W\apps\qgis-dev\qtplugins
set GDAL_DATA=C:\OSGeo4W\apps\gdal-dev\share\gdal
set PROJ_DATA=C:\OSGeo4W\share\proj
set PROJ_LIB=C:\OSGeo4W\share\proj
cd /d "%USERPROFILE%\Documents\hgis-3d-engine"
start "" "%USERPROFILE%\Documents\hgis-3d-engine\build\Release\strata-3d.exe" --bangri-preset gyeongju_hwangnyongsa --cam 16,34,1350
