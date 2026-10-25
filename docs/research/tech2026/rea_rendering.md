# REA 직접 확인 기록 (2026-10-25)

방법: REA `capture_browser_scenario`(Chrome headless launch, 2.5 s 대기, 끝에 접근성 트리) → `reatext.js` 로 본문 글자에서 정규식 주변 추출. 원결과 JSON 은 scratchpad 에 보관.

## R01 Qt 공식 문서 — QRhi (https://doc.qt.io/qt-6/qrhi.html), 원결과 rea_qrhi.json
- 문서 판 선택 상자: 「Qt 6.12 LTS」[selected], 6.11, 6.10, 6.8 LTS → 지금 공식 문서의 기본 판은 Qt 6.12 LTS. 이 PC(OSGeo4W)는 6.11.1.
- 「Since: Qt 6.6」, 사용에는 `Qt6::GuiPrivate`(qmake `QT += gui-private`) 필요.
- 원문: "The QRhi family of classes in the Qt Gui module, including QShader and QShaderDescription, offer limited compatibility guarantees. There are no source or binary compatibility guarantees for these classes, meaning the API is only guaranteed to work with the Qt version the application was developed against. Source incompatible changes are however aimed to be kept at a minimum and will only be made in minor releases (6.7 …"
- `enum Implementation { Null, Vulkan, OpenGLES2, D3D11, D3D12, Metal }`.
- 원문: "Direct3D 12 on Windows 10 version 1703 and newer, with Shader Model 5.0 or newer. Qt requires ID3D12Device2 … minimum feature level of D3D_FEATURE_LEVEL_11_0."
- ResourceLimit 에 `ShadingRateImageTileSize`(가변 셰이딩 비율) 항목이 있음.

## R02 Qt 문서 — QRhiWidget (https://doc.qt.io/qt-6/qrhiwidget.html), rea_qrhiwidget.json
- 「Since: Qt 6.7」, `Qt6::Widgets`. 원문: "In many ways it is the portable equivalent of QOpenGLWidget that is not tied to a single 3D graphics API".
- 원문: "configured to use a platform-specific backend and graphics API by default: Metal on macOS and iOS, Direct 3D 11 on Windows, OpenGL otherwise. Call setApi() to override this."
- 원문: "If two QRhiWidget or QQuickWidget widgets in the window's widget hierarchy request different APIs, only one of them will function correctly." / "Note: While QRhiWidget is a public Qt API, the QRhi …"(QRhi 자체는 제한 호환).
- 색 버퍼 기본 RGBA8, setFixedColorBufferSize 로 절반 해상도 렌더 후 확대 가능.

## R03 Qt 블로그 — Qt 6.12 LTS Released! (https://www.qt.io/blog/qt-6.12-released)
- 「Sep 30, 2026」, Volker Hilsheimer. 원문: "Qt 6.12 is a Long-Term Support (LTS) release … five-year maintenance period." "There is a new Qt LTS release every 2 years, now 6.12 being the newest."

## R04 Qt 문서 — What's New in Qt 6.12 (https://doc.qt.io/qt-6/whatsnew612.html), rea_whatsnew612.json
- Qt GUI: "Added indirect rendering support to QRhi. Buffers of type IndirectDrawBuffer can now be created, and indirect draw calls issued via QRhiCommandBuffer." / QRhi "ShaderDrawParameters feature flag … (e.g. gl_DrawID)" / "Vulkan backend now uses VK_EXT_device_fault".

## R05 Blender 5.0 릴리스 노트 — Color Management (https://developer.blender.org/docs/release_notes/5.0/color_management/)
- 문서 목록: 5.4(alpha), 5.3(beta), 5.2 LTS, 5.1, 5.0 … → 현재 LTS 는 5.2.
- 원문: "New ACES 1.3 and 2.0 views, as an alternative to AgX and Filmic and for compatibility with ACES pipelines." / "New AgX HDR view." / "Blend files now have a working color space. The default is Linear Rec.709 as before, with options for Linear Rec.2020 and ACEScg." / "Improved compatibility with the OpenColorIO configuration for ACES 2.0."

## R06 OpenColorIO v2.6.0 (https://github.com/AcademySoftwareFoundation/OpenColorIO/releases/tag/v2.6.0)
- "released this 01 Oct … 3 weeks ago", 「Latest」. 원문: "This is the main new feature release for this year and is the base version for the CY2027 VFX Reference Platform (which specifies 2.6.x)." 주요: "Add 2.6.0 built-in configs for ACES", "Config compatibility validation for HDR displays", "Add DirectX 12 GPU unit test backend for Windows".

## R07 Godot — Dev snapshot: Godot 4.6 dev 5 (https://godotengine.org/article/dev-snapshot-godot-4-6-dev-5/)
- 「Thaddeus Crews 1 December 2025」. 제목 "Use D3D12 by default on Windows". 원문: "This is because Vulkan is unfortunately unstable on Windows, with GPU drivers being poorly maintained compared to their Direct3D 12 counterparts." (GH-113213)

## R08 로컬 확인(이 PC)
- OSGeo4W Qt 6.11.1 에 `bin/qsb.exe`, `lib/cmake/Qt6GuiPrivate`, `include/QtGui/6.11.1/QtGui/rhi/qrhi.h`, `include/QtWidgets/QRhiWidget` 있음 → 지금 Qt 로도 QRhiWidget 이전 시험 가능. 6.12 LTS 는 OSGeo4W 제공 여부 미확인.
- AO: 우리 `src/app/PostShaders.cpp` 는 Blender 4.2.3 `eevee_ambient_occlusion_lib`(`ambient_ambient_occlusion_search_horizon`, GTAO 식 7·8)의 이식. 같은 Blender 4.2.3 에 `eevee_horizon_scan_*`(셰이더 원문 `slice_bitmask`, 가림 비트마스크)가 따로 있음(docs/rea/decompiled/blender_eevee_next_curated_shaders.json) → 더 새 방식은 이식 안 됨.
