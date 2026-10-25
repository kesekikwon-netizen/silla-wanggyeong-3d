# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Strata 3D (`strata-3d.exe` + `strata_3d_engine`): Windows desktop app, C++20 / Qt 6 Widgets / OpenGL 3.3 Core / GDAL·OGR·PROJ / QGIS (via Strata's `ka_core`) from OSGeo4W. It shows Gyeongju heritage on real 3D terrain and explains each site's setting and its relation to the user's trial-trench survey area, for expert meetings and talks. It is the 3D-only twin of Strata (ka-hgis, sibling repo `../hgis`, 2D-only): same ribbon, icons, layer panel, inspector, status bar and survey files, but the centre of the window shows only 3D. The plan is `docs/PLAN_3D_v3.html` (blueprint v3; phases 0 → 0b → 1 → 2 → 3 → 4, Gyeongju first).

Project rules and per-directory guides live in `AGENTS.md` files (root imported below; nested ones are injected by OMC hooks when a directory is touched).

@AGENTS.md

## Commands

Run from PowerShell in the repo root. `dev-env.ps1` must be dot-sourced in the same shell (PATH incl. `apps\qgis-dev\bin`, `QGIS_PREFIX_PATH`, Qt/QGIS plugin paths, `GDAL_DATA`, `PROJ_DATA`). Without it `strata-3d.exe` dies at start with 0xC0000135 (QGIS DLLs not found).

```powershell
. .\scripts\dev-env.ps1
cmake --preset vs                                         # first configure only (VS2022 x64 → build/)
cmake --build build --config Release --parallel           # all targets
cmake --build build --config Release --target test_relations   # one target
ctest --test-dir build -C Release --output-on-failure     # all 10 tests
ctest --test-dir build -C Release -R relations --output-on-failure   # one test (ctest names below)
build\Release\test_relations.exe relationsToDesignatedSites   # one QtTest slot directly
```

`scripts\build-now.ps1 [-Test]` builds only `strata-3d` and `test_strata_3d`; use the full build above before running the whole ctest suite.

ctest names → files: `strata_3d_engine` (test_strata_3d), `bangri_grid_unit`, `heritage_trust`, `cross_check`, `relations`, `strata_project` (reads the real Gyeongju survey the user saved in Strata), `ribbon_spec` (ribbon table vs Strata `MainWindowRibbon.cpp`), `strata_bridge_e2e` (launches the real exe over the pipe), `new_survey_e2e` and `gyeongju_select_e2e` (launch the real exe with `--e2e-new-survey` / `--select`). Tests hit real services (Copernicus COG, VWorld, Overpass, 국가유산청) and fail rather than skip when the network is blocked.

Run / visual check (there is no linter; MSVC `/W4 /permissive-` warnings are the lint):

```powershell
build\Release\strata-3d.exe --open-survey "$env:USERPROFILE\Documents\경주시\경주시.gpkg" --select "경주 대릉원 일원" --smoke-quit --window-shot out\shot.png
build\Release\strata-3d.exe --smoke-quit --eval-center --sheet-out out\sheet.png   # prints "strata-3d smoke: PASS|FAIL", exit 0 on pass
build\Release\strata-3d.exe --no-satellite --e2e-new-survey out\e2e --smoke-quit   # new survey → draw → save → re-read
.\scripts\shot.ps1 -Out out\frame.png -Cam "200,32,1500"
```

`--mode`: 0 clay, 1 elevation, 2 slope, 3 satellite. Other flags: `--no-satellite`, `--no-features`, `--no-ao`, `--no-bridge`, `--trench-clicks`, `--smoke-image`, `--smoke-report`, `--lon/--lat`, `--dem`. The exe prints QGIS debug lines on stderr; redirect to a file (`cmd /c "... > log.txt 2>&1"`) instead of letting PowerShell treat them as errors. UI changes are only "done" after opening the captured PNG.

Configure fails unless `HGIS_ROOT` (default `../hgis`) contains `src/app/KaTheme.cpp` and `build/Release/ka_core.lib` (build hgis Release first). Post-build copies `data/` and hgis `data/korea_dongs.json` next to the exe.

## Architecture

**Ways Strata is reused** — check `docs/STRATA_REUSE.md` and `docs/research/tech2026/strata_ui.md` before writing anything Strata might already do:
1. *Link `ka_core`* (IMPORTED `${HGIS_ROOT}/build/Release/ka_core.lib` + qgis_core/gui, app target only): opening a survey (`SurveyStorage::readEmbedded`), saving (`SurveySession::persistWork`, `SurveyFileFingerprint` warns if saved elsewhere), new survey (`SurveyProjectFactory`), domain layers (`LayerOps`), place search (`LocationSearch`). `QgsApplication` replaces `QApplication` in `main.cpp`.
2. *Compile hgis `src/app` files in place* (listed in `CMakeLists.txt`): KaTheme* (qss via `cmake/EmbedQss.cmake`), the ribbon/icons/chips/status bar (QGIS-free), and the layer panel, inspector, feature card, new-survey dialog, app bar (QGIS). Never edit hgis; wrap instead (`StrataLayerDock`, `S3dIconsMap.cpp` includes `KaIconsMockupMap.cpp` and adds the 9 3D-only icons from `data/icons/strata3d-icons.qrc`).
3. *Read what Strata left on disk with GDAL* (engine + tests stay QGIS-free): survey `.gpkg` (`StrataSurvey`, IMMUTABLE read), embedded project layer list (`StrataProjectLayers`), heritage layers listed in the survey project (`LocalVectorProvider`, category = project layer name, feature id `shp:<layer>/<fid>`), VWorld key (`VworldKey`).
4. *Local pipe* `strata-3d-bridge-v1` (`src/app/StrataBridge`, client `src/bridge/StrataBridgeClient` also copied into hgis). Spec: `docs/bridge-protocol.md`.

**Window** (`MainWindow`): ribbon built from the table in `src/app/RibbonSpec.cpp` (groups 조사 · 기록 · 자료 받기 · 지형 위에 올리기 · 3D 보기 · 3D 분석 · 내보내기 · 기타; buttons not wired yet are disabled with their phase in the tooltip, `pendingButtons()`); left `StrataLayerDock` on `QgsProject::instance()` (unchecking a layer hides its 3D features/labels/survey overlay); right `KaInspectorPanel` with Strata's `KaFeatureCard` for survey layers and `panels/HeritageCard` for reference/heritage records; bottom docks (구역·관계 표 / 단면 / 버퍼) open on demand; `KaStatusBar`. Drawing (`startSketch`→`addSketchFeature`) writes into Strata's survey layers through the QGIS edit buffer; `저장` = `persistWork` + `<survey>.s3d.json` (camera, z-scale, view; never written into the .gpkg). Survey overlay is rebuilt from project layers (`ProjectSurvey`), so unsaved edits show in 3D.

**Scene pipeline** (`QtConcurrent` worker, results via signals): `DemProvider` (Copernicus GLO-30 → metre grid in work CRS, row 0 = north) → `SatelliteProvider` (VWorld WMTS, Referer `https://localhost`, memory only, no Google per ToS) → feature providers (`OsmProvider`, `KhsProvider`, `KhsSpcaProvider`, `LocalVectorProvider`) → `SceneMesh` (buildings as footprint ribbons only, `SceneStyle::footprintOnly`) → `KaGlTerrainViewport` (CPU `TerrainLodTree` + `Frustum`, instanced 33×33 template on R32F height texture, schematic D models as grey wireframe via `setSchematic`) → `HdrPipeline` (RGBA16F → Nishita sky → GTAO → AgX; constants from Blender 4.2.3 via REA, `docs/rea/`). HUD: tier legend (A/B/D), view bearing/pitch, height scale; labels are site names only, placed without overlap, sized with Ctrl+wheel / Ctrl+= / Ctrl+- / Ctrl+0.

**Coordinates**: work CRS follows the survey file (EPSG:5186 or 5187, else 5187). Scene space is local metres with the DEM centre at origin (+X east, +Y north, +Z up). Cross-source checks (`CrossCheck`) in EPSG:5179. Transforms go through `CrsTransformer` (OSR) in the engine and `QgsCoordinateTransform` in QGIS-side app code; never hand-written projections.

**Evaluation stack**: `Archaeology3dEvaluator` (buffers, boundary distance/bearing, height difference, slope via DemAnalyzer, profile) → `RelationEngine` (distance, bearing, height difference, period from spca, one row per name+category with piece count, zone overlap against real 보호구역/현상변경허용기준/유존지역/허가구역 polygons incl. holes) → `RelationPanel`; `EvaluationJson` for the pipe.

**No speculation** (user decisions 2026-10-25, blueprint v3 §10): no viewshed/line of sight, no 방리 cell or landform-class columns, no C-tier layers on screen, no extruded buildings, no ancient-surface reconstruction. Data shown = measured/recorded only; computed values carry their source and resolution. Do not add these back.

**Trust layer**: `HeritageNaming::displayHeritageName` / `siteNameOnly` strip designation numbers (not shown since 2021-11-19, also hidden from record cards); every feature carries an `EvidenceTier` (A 실측, B 근거, D 도식 — all 311 Blender models, off by default). Never the word 「복원」 on screen.

**Bangri subsystem** (`Bangri*`, `SillaMoundReconstructor`, `FossilizedRoadAnalyzer`, `DbpiaBangriLedger`, `BangriDockPanel.cpp` no longer built, `composeBangriSheet`): legacy hypothetical grid code. It no longer reaches the window, relation table or pipe; only `BangriTerrainValidator::buildSchematicModels` (D models) and `buildUserDrawingsOverlay`/`makeGridFromTwoPoints` (measure lines, trench grid) are used. `bangri_grid_unit` still tests the engine code.

## Where decisions live

- `docs/PLAN_3D_v3.html` — current blueprint (decisions §10, phases and Gyeongju acceptance criteria §9). `docs/BLUEPRINT.md` is v2 (older).
- `docs/research/` — collection methods, findings with citation checks, design/3D basis, `DEVLOG.md` (record what was verified and how).
- `.omc/handoffs/next-steps.md` — latest work checkpoint (not committed).
- `README.md` — user-facing list of what works and its limits; only list things verified by test or capture.
- Strata (2D) bugs found while reusing its code go to `C:\Users\권을\Desktop\Strata_GIS앱_고칠점.txt`, not to hgis.
