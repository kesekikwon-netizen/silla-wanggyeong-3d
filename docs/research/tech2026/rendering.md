# Strata 3D: research on the rendering foundation (as of 2026-10-25)

Scope: Windows desktop, C++20, Qt 6 Widgets, currently OpenGL 3.3 Core through QOpenGLWidget. Installed Qt is 6.11.1 (OSGeo4W).
Method: every row marked "yes" was opened with WebFetch during this session. Rows marked "no" come only from search snippets and are repeated in the "Unverified" list. Sources older than 2024 are labelled "(background)".

---

## Q1. Qt QRhi, QRhiWidget, shaders, Qt Quick 3D, and the future of OpenGL in Qt

| finding | source title | URL | publisher | date / version | source type | verified (opened) |
|---|---|---|---|---|---|---|
| QRhi is public since Qt 6.6. The QRhi classes and QShader/QShaderDescription have "limited compatibility": "There are no source or binary compatibility guarantees for these classes". Source-incompatible changes are "kept at a minimum and will only be made in minor releases" | QRhi Class | https://doc.qt.io/qt-6/qrhi.html | The Qt Company | Qt 6.12.0 docs | docs | yes |
| QRhi backends: OpenGL 2.1 / ES 2.0+, D3D11.2+ (DXGI 1.3, SM 5.0), D3D12 (Win10 1703+, SM 5.0), Vulkan 1.0+ (some 1.1), Metal 1.2+, Null | QRhi Class | https://doc.qt.io/qt-6/qrhi.html | The Qt Company | Qt 6.12.0 docs | docs | yes |
| QRhi::Feature enum, "since" versions: MultiView 6.7; TextureViewFormat and ResolveDepthStencil 6.8; VariableRateShading and PerRenderTargetBlending 6.9; DepthClamp and InstanceIndexIncludesBaseInstance 6.11; **DrawIndirect, DrawIndirectMulti and ShaderDrawParameters 6.12**. Compute, Tessellation, GeometryShader, TextureArrays and 3D textures exist with no "since" tag. **There is no mesh-shader feature.** | QRhi Class (Feature enum) | https://doc.qt.io/qt-6/qrhi.html | The Qt Company | Qt 6.12.0 docs | docs | yes |
| Shader pipeline: you write one language, it is compiled to SPIR-V and translated by Qt Shader Tools into `.qsb` packages, using either `qsb` or `qt_add_shaders()`. By default you get SPIR-V (Vulkan 1.0), GLSL 100es/120/150, HLSL SM 5.0 and MSL 1.2. `PRECOMPILE` runs fxc to produce DXBC on Windows. Compute shaders need GLSL "310 es,430"-style versions | qt_add_shaders | https://doc.qt.io/qt-6/qtshadertools-build.html | The Qt Company | Qt 6.12.0 docs | docs | yes |
| QRhiWidget arrived in Qt 6.7 (Qt Widgets). It is "the portable equivalent of QOpenGLWidget". You subclass it and implement initialize(cb), render(cb) and releaseResources(). Default API is D3D11 on Windows; setApi() supports OpenGL, Vulkan, D3D11, D3D12, Metal and Null. Each window can use only one API, so you cannot mix QRhiWidget or QQuickWidget with different APIs. It always renders into a backing texture, which costs extra resources. Subclasses must link `Qt::GuiPrivate` and include `<rhi/qrhi.h>`. It provides grabFramebuffer() and native interop through beginExternal() | QRhiWidget Class | https://doc.qt.io/qt-6/qrhiwidget.html | The Qt Company | Qt 6.12.0 docs | docs | yes |
| Qt 6.8 added QRhi multiview on Vulkan 1.1, automatic MSAA depth resolve, depfiles for qt_add_shaders and a MULTIVIEW keyword. It also brought cascaded shadow maps and PCF to Quick 3D | What's New in Qt 6.8 | https://doc.qt.io/qt-6/whatsnew68.html | The Qt Company | Qt 6.8 | release-notes | yes |
| Qt 6.10: QRhi can enumerate adapters (physical devices) on D3D11, D3D12 and Vulkan. Quick 3D lightmap baking moved to its own RHI context and gained GPU NLM denoising | What's New in Qt 6.10 | https://doc.qt.io/qt-6/whatsnew610.html | The Qt Company | Qt 6.10 | release-notes | yes |
| Qt 6.11: Qt OpenGL module "No noteworthy changes". Quick 3D gained user-defined render passes in QML, **SSGI in ExtendedSceneEnvironment**, linked-list OIT, per-object motion vectors (for TAA and motion blur), and blue-noise PCF. Canvas Painter shipped as a tech preview | What's New in Qt 6.11 | https://doc.qt.io/qt-6/whatsnew611.html | The Qt Company | Qt 6.11 | release-notes | yes |
| Qt 6.11.0 was officially released on 23 Mar 2026 | [Announce] Qt 6.11.0 Released | https://lists.qt-project.org/pipermail/announce/2026-March/000613.html | Qt Project (J. Heikkinen) | 2026-03-23 | release-notes | yes |
| Qt 6.11.3 (target Sep 2026) is "the last release from Qt 6.11 series", and 6.12 LTS is already available | Qt 6.11.2 Released | https://qt.io/blog/qt-6.11.2-released | The Qt Company | 2026-08-18 | release-notes/blog | yes |
| Qt 6.12 LTS was released with 5-year maintenance. The blog has no OpenGL, Vulkan or D3D items, and Canvas Painter is now supported | Qt 6.12 Released | https://qt.io/blog/qt-6.12-released | The Qt Company (V. Hilsheimer) | 2026-09-30 | release-notes/blog | yes |
| Qt 6.12: "Added indirect rendering support to QRhi" (IndirectDrawBuffer), the ShaderDrawParameters feature, and VK_EXT_device_fault on device loss. "The shader disk cache is re-enabled for current Intel GPU drivers" on Windows. No graphics deprecations are listed | What's New in Qt 6.12 | https://doc.qt.io/qt-6/whatsnew612.html | The Qt Company | Qt 6.12 | release-notes | yes |
| QOpenGLWidget docs (6.12) do not deprecate it. They still call it "The stable, cross-platform solution", and I found no Qt statement on the future of OpenGL | QOpenGLWidget Class | https://doc.qt.io/qt-6/qopenglwidget.html | The Qt Company | Qt 6.12.0 docs | docs | yes |
| Qt Quick 3D requirements: OpenGL 3.0+ (3.3 strongly recommended), D3D11.1, D3D12, Vulkan 1.0+, Metal 1.2+ | Qt Quick 3D Graphics Requirements | https://doc.qt.io/qt-6/qtquick3d-requirements.html | The Qt Company | Qt 6.12.0 docs | docs | yes |
| QQuick3DRenderExtension (since 6.7) injects custom code into the Quick 3D pipeline via updateSpatialNode() → QSSGRenderExtension. The page has no tech-preview flag | QQuick3DRenderExtension Class | https://doc.qt.io/qt-6/qquick3drenderextension.html | The Qt Company | Qt 6.12.0 docs | docs | yes |
| ExtendedSceneEnvironment: `ssgiEnabled` (since 6.11) "also provides ambient occlusion" and "may have a big impact on GPU load". The tonemap modes are None, Linear (default), ACES, Hejl-Dawson and Filmic. **There is no AgX** | ExtendedSceneEnvironment QML Type | https://doc.qt.io/qt-6/qml-qtquick3d-helpers-extendedsceneenvironment.html | The Qt Company | Qt 6.12.0 docs | docs | yes |

**Implication for Strata 3D (Q1): MIGRATE to QRhiWidget, and plan to target Qt 6.12 LTS. Do not adopt Qt Quick 3D.**
- QRhiWidget is the direct, officially documented successor to QOpenGLWidget for Widgets apps. On Windows it gives D3D11 by default, and D3D12 or Vulkan through setApi(). Your current pipeline needs R32F, RGBA16F, instancing, compute (optional) and texelFetch, and QRhi has all of these.
- Qt 6.11 is end-of-line: 6.11.3 is the last release. 6.12 LTS (5 years) also adds QRhi DrawIndirect, which GPU-driven terrain culling would need. QRhi has limited compatibility (source changes possible in minor releases), so pin to one LTS. This assumes the OSGeo4W build ships 6.12 with the GuiPrivate headers, which is a **local check** and not something the web can answer.
- Cost of the switch: you must rewrite the GLSL 3.3 shaders as Vulkan-style GLSL 440 (with explicit bindings), compiled by qt_add_shaders. QRhiWidget renders through an offscreen texture, and all 3D widgets in one window must use the same API.
- Nothing forces the move: QOpenGLWidget is not deprecated, and the Qt OpenGL module simply had "no noteworthy changes" in 6.11.
- Qt Quick 3D is a poor fit as the foundation. Its tonemappers lack AgX, so you would lose your Blender-matched AgX. Custom terrain is only possible through QQuick3DGeometry or RenderExtension inside a QML scene. Its 6.11 SSGI is useful only as a reference for comparison.

---

## Q2. OpenGL on Windows in 2025–2026, and what engines use instead

| finding | source title | URL | publisher | date / version | source type | verified (opened) |
|---|---|---|---|---|---|---|
| Intel moved its 6th to 10th gen iGPUs (and related Atom/Pentium/Celeron) to a legacy model: "critical fixes and security vulnerabilities only", quarterly cadence, effective 27 Jul 2022. The page was last reviewed 30 Sep 2026 | Graphics Driver Support Update for 10th Generation and Older Intel Processor Graphics | https://intel.com/content/www/us/en/support/articles/000091662.html | Intel | reviewed 2026-09-30 | docs (vendor support) | yes |
| GL_EXT_mesh_shader is "Shipping". It is Revision 1, last modified 1 Jul 2025, requires OpenGL 4.5 or ES 3.1, and comes from AMD (Qiang Yu). So OpenGL still gets new cross-vendor extensions | EXT_mesh_shader | https://registry.khronos.org/OpenGL/extensions/EXT/EXT_mesh_shader.txt | Khronos OpenGL Registry | rev 1, 2025-07-01 | spec | yes |
| Mesa GL-on-D3D12 (Microsoft, Jesse Natalie) reached OpenGL 4.6. It targets "Windows on ARM devices lacking native OpenGL drivers" and WSLg | Microsoft Enables OpenGL 4.6 Support Over Direct3D 12 | https://www.phoronix.com/news/Microsoft-OpenGL-4.6-D3D12 | Phoronix | 2023-11-19 (background) | news | yes |
| Microsoft/Collabora OpenCL & OpenGL Compatibility Pack (mapping layers over D3D12, for Windows on ARM) | Announcing the OpenCL and OpenGL Compatibility Pack for Windows 10 on ARM | https://devblogs.microsoft.com/directx/announcing-the-opencl-and-opengl-compatibility-pack-for-windows-10-on-arm/ | Microsoft DirectX blog | 2020-11-17 (background) | blog | no |
| Godot 4.6-dev5 made D3D12 the default RenderingDevice driver on Windows for new projects. The stated reason is "Vulkan is unfortunately unstable on Windows" because of less-maintained drivers. This is a choice between Vulkan and D3D12, not a statement about OpenGL | Dev snapshot: Godot 4.6 dev 5 | https://godotengine.org/article/dev-snapshot-godot-4-6-dev-5/ | Godot Foundation | 2025-12-01 | blog/release-notes | yes |
| Unity 6.1: "The default Auto Graphics API in Unity 6.1 is DirectX12" | Upgrade to Unity 6.1 | https://docs.unity.cn/6000.2/Documentation/Manual/UpgradeGuideUnity61.html | Unity | Unity 6.1/6.2 docs | docs | yes |
| Unreal Nanite requires "DirectX 12 with Shader Model 6 (SM6)" | Nanite Virtualized Geometry | https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-virtualized-geometry-in-unreal-engine | Epic Games | UE 5.8 docs header | docs | yes |
| Qt itself defaults QRhiWidget to D3D11 on Windows | QRhiWidget Class | https://doc.qt.io/qt-6/qrhiwidget.html | The Qt Company | Qt 6.12.0 | docs | yes |
| Qt 6.12 re-enabled the shader disk cache for current Intel GPU drivers on Windows | What's New in Qt 6.12 | https://doc.qt.io/qt-6/whatsnew612.html | The Qt Company | Qt 6.12 | release-notes | yes |

**Implication for Strata 3D (Q2): MIGRATE the default backend to D3D11 (via QRhi), and keep OpenGL as a fallback backend.**
- I found **no 2024–2026 Khronos or Microsoft policy statement** deprecating OpenGL on Windows. OpenGL still gets new extensions (EXT_mesh_shader, 2025).
- The verified signals still point one way. Godot (D3D12, Dec 2025), Unity 6.1 (DX12) and Unreal Nanite (DX12 SM6) all default to D3D12 on Windows, and Qt's own default is D3D11. Your audience is the risky one: Intel 6th to 10th gen iGPUs, common in older laptops and lecture-room PCs, are on critical-fixes-only drivers. On Windows-on-ARM laptops, OpenGL may run only through the GLon12 mapping layer.
- Making D3D11 the default and keeping OpenGL behind a setApi() switch removes the dependence on per-vendor OpenGL drivers at no feature cost to your current pipeline.

---

## Q3. Terrain rendering practice in 2024–2026

| finding | source title | URL | publisher | date / version | source type | verified (opened) |
|---|---|---|---|---|---|---|
| Concurrent Binary Trees (CBT) extended from square domains to arbitrary polygon meshes. The CBT acts as a GPU memory pool for adaptive bisection, enables planetary scale, and "evaluates in less than 0.2ms on console-level hardware" | Concurrent Binary Trees for Large-Scale Game Components (arXiv:2407.02215, DOI 10.1145/3675371) | https://arxiv.org/abs/2407.02215 | A. Benyoub, J. Dupuy (Intel); HPG 2024 | 2024-07-02 | paper/preprint | yes |
| Intel's article on the CBT work: a single representation, and the memory manager limits triangles in view. **It says nothing about iGPU performance or required API features** | CBT for Large-Scale Game Components | https://www.intel.com/content/www/us/en/developer/articles/technical/cbt-for-large-scale-game-components.html | Intel | 2024-07-02 | blog (vendor) | yes |
| SIGGRAPH 2024 Advances talk "Achieving Scalable Performances for Large Scale Components with CBTs" (Intel), alongside a GPU-driven mobile adaptive LOD talk (Tencent) and VRS with a visibility buffer (Hable) | Advances in Real-Time Rendering in Games, SIGGRAPH 2024 | https://advances.realtimerendering.com/s2024/index.html | Advances course (N. Tatarchuk) | 2024-07-30 | conference course | yes |
| SIGGRAPH 2025 Advances has no terrain, virtual texturing, mesh shader or AO talks. Its focus is GI and lighting: idTech8 GI, AC Shadows RT GI, MegaLights | Advances in Real-Time Rendering in Games, SIGGRAPH 2025 | https://advances.realtimerendering.com/s2025/index.html | Advances course | 2025 | conference course | yes |
| Nanite "can be enabled for landscape terrain to improve performance". Nanite tessellation gives runtime programmable displacement. Requires DX12 SM6 | Nanite Virtualized Geometry | https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-virtualized-geometry-in-unreal-engine | Epic Games | UE 5.8 docs | docs | yes |
| Godot Terrain3D (open source) uses a "Geomorphing Geometric Clipmap Mesh Terrain, as used in The Witcher 3", centered on the camera, with height data in 256x256 regions. Geometry clipmaps are still current practice in shipping open-source tools | Terrain3D Introduction | https://terrain3d.readthedocs.io/en/stable/docs/introduction.html | Terrain3D project | "stable" docs (no version or date shown) | docs | yes |
| OpenGL mesh shaders need GL 4.5 / ES 3.1 (EXT_mesh_shader). QRhi has **no** mesh-shader feature, and its DrawIndirect support only arrives in Qt 6.12 | EXT_mesh_shader; QRhi Class | (see Q1, Q2) | Khronos; Qt | 2025; 6.12 | spec/docs | yes |

**Implication for Strata 3D (Q3): KEEP CDLOD (instanced 33x33 grid + quadtree + R32F height texture). Optionally ADD GPU-side culling after moving to 6.12.**
- CDLOD and geometry clipmaps (Terrain3D, 2024–2026) are both still in use. The verified "newer" options require things Strata cannot rely on:
  - Nanite needs DX12 SM6.
  - Mesh shaders are not exposed by QRhi at all.
  - CBT needs compute plus indirect dispatch or draw, which QRhi only gets in 6.12.
- Realistic upgrade path after QRhi + 6.12: use a compute pass for frustum and LOD selection, then DrawIndirect for chunk instances. This is optional; for a DEM of one site region, CPU quadtree selection is not the bottleneck.
- For large orthophoto or texture coverage, virtual texturing or texture clipmaps are the established answer, but I found **no 2024–2026 primary source** for them in this session.
- I have **no verified data** on how CBT or GPU-driven terrain performs on integrated GPUs, so I make no claim.

---

## Q4. Color pipeline currency: Blender, OCIO, ACES 2.0

| finding | source title | URL | publisher | date / version | source type | verified (opened) |
|---|---|---|---|---|---|---|
| Blender 5.0 color management adds new "ACES 1.3 and 2.0 views, as an alternative to AgX and Filmic". It also adds a "New AgX HDR view", a per-file working color space (default Linear Rec.709, options Rec.2020 and ACEScg), Rec.2100-PQ/HLG displays, better ACES 2.0 OCIO config compatibility, and a `BLENDER_OCIO` env var. **The page lists no change to SDR AgX itself** | Blender 5.0 Release Notes: Color Management | https://developer.blender.org/docs/release_notes/5.0/color_management/ | Blender Foundation | Blender 5.0 | release-notes | yes |
| Blender 5.0 manual views: Standard, ACES 1.3, ACES 2.0 ("more neutral look"), Khronos PBR Neutral, AgX ("improves on Filmic"), Filmic (deprecated, "superseded by AgX"), Filmic Log, False Color, Raw. **The page does not state the default view** | Blender 5.0 Manual: Displays and Views | https://docs.blender.org/manual/en/5.0/render/color_management/displays_views.html | Blender Foundation | Blender 5.0 manual | docs | yes |
| Blender 5.0 released 18 Nov 2025, 5.1 on 17 Mar 2026, 5.2 LTS on 14 Jul 2026 | (search result: blender.org release list); 5.1 and 5.2 notes pages | https://developer.blender.org/docs/release_notes/5.1/ ; https://developer.blender.org/docs/release_notes/5.2/ | Blender Foundation | 5.1 page: "March 17, 2026"; 5.2 page: "July 14, 2026" | release-notes | 5.1/5.2: yes; 5.0 date: no |
| Blender 5.1 upgrades libraries to "OpenColorIO 2.5" (VFX platform 2026) | Blender 5.1 Release Notes (Compatibility) | https://developer.blender.org/docs/release_notes/5.1/ | Blender Foundation | 2026-03-17 | release-notes | yes |
| Blender 5.2 color management only adds family-grouped color space menus, camera input spaces, Adobe RGB and gamma spaces. **No view transform or AgX change** | Blender 5.2 Release Notes: Rendering | https://developer.blender.org/docs/release_notes/5.2/rendering/ | Blender Foundation | 2026-07-14 | release-notes | yes |
| OCIO timeline: 2.4.0 (ACES 2 output transforms as Preview); 2.4.2 ("ACES 2 support in OCIO is now officially released", configs shipped separately); 2.5.0 (built-in ACES 2 configs, Vulkan support; base for CY2026); 2.5.1/2.5.2 (fixes, incl. CVE-2026-42450); **2.6.0 is the latest** (CLF ST 2136-1, HDR display validation, D3D12 GPU test backend; base for CY2027). The releases page shows days without years, so the years are inferred from the VFX platform labels: 2.4.0 Oct 2024, 2.4.2 Mar 2025, 2.5.0 Oct 2025, 2.6.0 1 Oct 2026 | OpenColorIO Releases; v2.6.0 tag | https://github.com/AcademySoftwareFoundation/OpenColorIO/releases ; https://github.com/AcademySoftwareFoundation/OpenColorIO/releases/tag/v2.6.0 | ASWF | v2.6.0 (latest) | release-notes | yes (years inferred) |
| In OCIO 2.4, ACES 2 is built from fixed-function ops (ACES_OUTPUT_TRANSFORM_20, RGB_TO_JMH_20, TONESCALE_COMPRESS_20, GAMUT_COMPRESS_20) labelled work-in-progress. The 2.4.2 notes say ACES 2 is "noticeably slower than ACES 1" | OCIO 2.4 Release | https://opencolorio.readthedocs.io/en/latest/releases/ocio_2_4.html | ASWF | OCIO 2.4 | release-notes | yes |
| ACES 2: "completely redesigned suite of rendering transforms". It has a less aggressive tone scale (lower mid-tone contrast, gentler highlight rolloff), robust gamut mapping, faster inverse transforms, and presets for common display standards | About ACES 2 | https://docs.acescentral.com/background/about-aces-2/ | Academy / ACES Central | page dated 2025-09-10 | docs | yes |
| "ACES 2.0 Released for End-Users", announced at NAB (around April 2025), with OCIO, Resolve 20 beta, SGO, Pomfort and Autodesk Flame support | ACES 2.0 Released for End-Users | https://acescentral.com/?p=5016 | ACES Central | ~Apr 2025 (inferred from NAB / 2025-04 image paths) | blog | yes |

**Implication for Strata 3D (Q4): KEEP AgX as the default. Optionally ADD an "ACES 2.0" view and a "Khronos PBR Neutral" view.**
- AgX in Blender 4.2.3 is not obsolete. Blender 5.0, 5.1 and 5.2 release notes list no change to SDR AgX, and Filmic is the view marked deprecated. Blender 5.x adds ACES 1.3 and 2.0 views beside AgX, and an AgX HDR view.
- If you want parity with current Blender, add an ACES 2.0 SDR view, generated from the OCIO 2.5/2.6 built-in ACES 2 config and baked into a 3D LUT or ported shader. Khronos PBR Neutral is an option for color-accurate material and artifact display.
- HDR output (Rec.2100-PQ) is not relevant to projectors and laptops.
- Re-check that your baked AgX LUT matches the Blender 5.x config if you regenerate it. I did not diff the configs this session.

---

## Q5. Ambient occlusion and GI practice

| finding | source title | URL | publisher | date / version | source type | verified (opened) |
|---|---|---|---|---|---|---|
| The EEVEE (4.2.3) horizon-scan shader header says: "Implementation of Horizon Based Global Illumination and Ambient Occlusion. This mostly follows the paper: Screen Space Indirect Lighting with Visibility Bitmask" (Therrien, Levesque, Gilet). The body uses "Bitmask representing the occluded sectors on the slice" and also cites "Eq. 8 of GTAO paper" | eevee_horizon_scan_eval_lib.glsl @ v4.2.3 | https://raw.githubusercontent.com/blender/blender/v4.2.3/source/blender/draw/engines/eevee_next/shaders/eevee_horizon_scan_eval_lib.glsl | Blender Foundation (source) | v4.2.3 | source code | yes |
| Visibility bitmask paper: it replaces GTAO's two horizon angles with an N-sector bitmask, so light passes behind thin, constant-thickness surfaces. It gives better AO and indirect diffuse at low overhead | Screen Space Indirect Lighting with Visibility Bitmask (arXiv:2301.11376; The Visual Computer, DOI 10.1007/s00371-022-02703-y) | https://arxiv.org/abs/2301.11376 | Therrien, Levesque, Gilet | 2023-01 (background) | paper | yes |
| Blender 4.2 manual, Fast GI Approximation: Method "Ambient Occlusion" ("fastest") or "Global Illumination" (bounce light from surrounding objects), with Thickness Near and Far | Blender 4.2 Manual: Raytracing | https://docs.blender.org/manual/en/4.2/render/eevee/render_settings/raytracing.html | Blender Foundation | 4.2 LTS | docs | yes |
| Blender 4.2 EEVEE Next: screen-space ray tracing for every BSDF with no BSDF-count limit, plus improved volume/sphere light probes | Blender 4.2 Release Notes: EEVEE | https://developer.blender.org/docs/release_notes/4.2/eevee/ | Blender Foundation | 4.2 | release-notes | yes |
| Blender 5.2 fixed "a few critical bugs" in Fast GI ("AO pass and AO shading node are affected"). It **removed Far Thickness** ("caused dark halos around objects"), added a Backface option to reduce leaking, and fixed energy conservation (some scenes render darker than 5.1) | Blender 5.2 Release Notes: EEVEE & Viewport | https://developer.blender.org/docs/release_notes/5.2/eevee/ | Blender Foundation | 2026-07-14 | release-notes | yes |
| Qt Quick 3D 6.11 added SSGI that "also provides ambient occlusion, and it will often give better quality results" than SSAO | ExtendedSceneEnvironment | https://doc.qt.io/qt-6/qml-qtquick3d-helpers-extendedsceneenvironment.html | The Qt Company | Qt 6.11+ | docs | yes |
| AMD FidelityFX Brixelizer GI 1.0.1 (SDK 1.1.4): compute-based dynamic GI on sparse distance fields, DX12 and Vulkan, GCN+, MIT | AMD FidelityFX Brixelizer/GI | https://gpuopen.com/fidelityfx-brixelizer/ | AMD GPUOpen | SDK 1.1.4, May 2025 | docs (vendor) | yes |
| AAA trend 2024–2025: GI talks dominate (Frostbite GIBS surfels 2024; idTech8 GI, AC Shadows RT GI, UE5 MegaLights 2025). There is no screen-space AO talk except CoD hemispherical lighting/AO (2024) | SIGGRAPH Advances 2024 / 2025 | (see Q3) | Advances course | 2024, 2025 | conference course | yes |

**Implication for Strata 3D (Q5): KEEP the horizon-based AO, FIX it to the Blender 5.2 behaviour, and optionally ADD the bitmask "GI" mode.**
- Blender 4.2.3 EEVEE AO is already the visibility-bitmask horizon scan (a GTAO derivative), not classic GTAO. The 4.2 Python API still uses `gtao_*` property names, which is a likely source of the "GTAO" label (this API detail is from a search snippet; see Unverified).
- **Local check:** grep the Strata AO shader for "horizon"/"bitmask"/"Therrien" to see which algorithm was actually ported.
- Blender 5.2 removed Far Thickness because it caused dark halos, and fixed critical Fast GI bugs. If the port carries 4.2.3 Far Thickness, consider dropping it to match 5.2. Re-port the 5.2 fix if source parity matters; I did not open the 5.2 shader diff.
- The same horizon scan can output one-bounce indirect diffuse (the "Global Illumination" method), which would be a cheap, consistent step up.
- World-space GI (Brixelizer, surfels, RT GI) requires DX12/Vulkan compute and is overkill for terrain under a sky.

---

## Unverified (search-snippet only, not opened, or not stated on any page opened)

1. **Blender 5.x default view transform.** None of the opened pages states it, so I do not claim that AgX is still the default in 5.0, 5.1 or 5.2. Blender 4.x defaulting to AgX is also not verified this session.
2. Blender 5.0 release date of 18 Nov 2025. This came from search snippets of the blender.org release list; the 5.0 notes page I opened does not show it.
3. The Microsoft OpenCL/OpenGL Compatibility Pack: 2020 announcement content, whether it currently exposes GL 4.6, and the Dzn Vulkan layer on Windows-on-ARM. All come from snippets only.
4. Godot 4.6 **stable** release notes, and the claim that D3D12 is "on par with Vulkan". Only dev5 was opened.
5. The ACES 2.0 exact announcement date (NAB, April 2025) and the oscars.org press release, which I did not open. The Variety article redirected (tollbit) and the AOL copy returned 404.
6. OCIO release **years** are inferred from the VFX-reference-platform labels; the GitHub page shows day and month only.
7. EEVEE design issue #112979 and PR #114150 (AO pass → horizon scan, merged 2023-11-02): projects.blender.org returned 403. The 4.2 API using `gtao_*` names and 4.4/4.5 adding `fast_gi_method` also come from a snippet only.
8. Any Khronos or Microsoft 2024–2026 **policy** statement on OpenGL's future: none found. The "OpenGL resurgence Oct 2025" item is a Phoronix snippet only.
9. NVIDIA driver support for GL_EXT_mesh_shader, and Mesa RadeonSI/Zink support: Phoronix snippets only.
10. Performance of CBT, GPU-driven or mesh-shader terrain on integrated GPUs, and whether Intel/AMD/Qualcomm iGPUs expose mesh shaders: no source opened.
11. Unreal Nanite Landscape's history (5.1+, experimental status), from forum snippets only.
12. A 2024–2026 primary source for virtual texturing or texture clipmaps for terrain: none found.
13. Local, not web: whether the OSGeo4W Qt build ships 6.12 and includes the Qt::GuiPrivate headers and the Qt Shader Tools `qsb` tool.
