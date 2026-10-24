# Blender 4.2.3 LTS (`blender-4.2.3-windows-x64.zip`) REA MCP 전수 역공학 보고서

> **분석 대상 아카이브**: `artifacts/blender-4.2.3-windows-x64.zip` (호스트 OS 무설치 정적·아카이브·바이너리 역공학)
> **REA MCP 버전**: `rea v6.7.0`
> **증거 번들(Evidence Bundle)**: [blender-4.2.3-rea-evidence-bundle.json](file:///C:/Users/권을/Documents/hgis-3d-engine/docs/rea/blender-4.2.3-rea-evidence-bundle.json) (`16,738,410` bytes, `20` deterministic evidence records)
> **PDB 정적 라이브러리 전수 목록**: [blender_pdb_libraries.json](file:///C:/Users/권을/Documents/hgis-3d-engine/docs/rea/blender_pdb_libraries.json) (`252` static `.lib` + `DIRECT_OBJ`, `5,919` Translation Units)
> **실행 파일·DLL·PYD 바이너리 전수 해시 목록**: [blender_binaries_inventory.json](file:///C:/Users/권을/Documents/hgis-3d-engine/docs/rea/blender_binaries_inventory.json) (`130` PE32+ binaries)

---

## 1. 아카이브 그래프 및 무결성 검증 (`inspect_artifact` / `extract_artifact`)

| 항목 | 값 / 식별자 |
|---|---|
| 파일 경로 | `C:\Users\권을\Documents\hgis-3d-engine\artifacts\blender-4.2.3-windows-x64.zip` |
| 바이트 크기 | `383,613,057` bytes (`365.84 MiB`) |
| 아카이브 SHA-256 | `7f8be9b1dc904e7689b3112659791c29cc9ed9d46ce57c4ea5b31a0621489b3a` |
| Root Artifact ID | `art_729b9a5604376f4fd32225bf8fb8350f6776584eb807a8c60e892a4d4db2b05b` |
| Manifest ID | `agm_ee9e882fa35c55e0de8ddeeb408f016c0dabe6866b7b4d26591b1d8f94c5d4e1` |
| Graph SHA-256 | `35eb2ca92e6001fdc5c418c406327dbc6bab00055b0a4b037e51ea033f53673f` |
| Summary Evidence ID | `ev_6eea3ba0bfe34f4cb1cbbf7fdc9f87719ca995b900f8e9446c785c0767b3e881` |
| Complete Parent Evidence ID | `ev_a53da53a2ddb5cda978fb931b614c9550a718d2ff8a722a2ba4630470f37f677` |
| Inventory Evidence ID | `ev_fd64b197cc3c89d06dd7761ea3bf4366b97a6d995d39d3922a6825534a2fd6b9` |
| Extraction Evidence ID | `ev_588cdfab173f56ad22eea7594cb75fe35859e289edd847a68f10e4ff100cd27a` |
| Extraction ID | `aex_1ddaefd31b0686e5e383903c57343df8b91ec42e2bcd74b2f78b530445fb1da2` |
| Extracted Files SHA-256 | `4c2ef71ec0f41ac4c5620cfba1bb49636ca9c7f216a38b5af2d7fa14e45742d1` |
| 그래프 노드 / 오커런스 수 | `node_count: 5,699` / `occurrence_count: 6,255` (`files: 5,521`, `directories: 734`, `symlinks: 0`, `contradictions: 0`) |

### 1.1 서브시스템별 디렉터리 및 파일 분포 (총 5,520개 추출 파일 / 1,103.52 MB 비압축)

| 서브시스템 경로 | 파일 수 | 비압축 용량 (MB) | 역할 및 아키텍처 기능 |
|---|---:|---:|---|
| `<root>` (`blender.exe`, `blender.pdb`, 핵심 DLL) | 78 | 665.24 | 메인 실행 바이너리, 심볼 데이터베이스(`blender.pdb`), 66개 C/C++ 런타임 및 렌더링/기하/시뮬레이션 코어 DLL |
| `4.2/scripts` (`addons_core`, `startup`, `modules`, `presets`) | 2,400 | 49.49 | 36개 코어 애드온(`io_scene_gltf2`, `bl_pkg`, `cycles`, `hydra_storm`, `rigify`, `node_wrangler` 등), UI/RNA 스크립트, BPy 모듈 |
| `4.2/python` (내장 Python 3.11.7 배포판) | 2,447 | 132.49 | `python311.dll`, 표준 라이브러리, `numpy`, `cython`, `requests`, `zstandard`, `pxr` (OpenUSD Python 바인딩) |
| `4.2/datafiles` (`colormanagement`, `studiolights`, `fonts`, `icons`) | 585 | 256.10 | OpenColorIO `AgX` / `Filmic` / `PBR Neutral` 3D LUT(`.cube`, `.spi3d`), HDRI 스튜디오 라이트, 브러시 에셋, 다국어 로케일 |
| `blender.shared` | 10 | 0.20 | Windows Side-by-Side 매니페스트 공유 로더 스텁 |

---

## 2. 핵심 PE32+ (`x86_64`) 바이너리 및 REA PE 리소스 분석 (`inspect_pe_resources`)

| 바이너리 경로 | 크기 (Bytes) | SHA-256 | REA Evidence ID | PE 리소스 구조 요약 |
|---|---:|---|---|---|
| `blender.exe` | `82,756,224` | `ca6c421d16a3e7716e2700d8e8c468205c17f8c023e00748f7b55196c3f128d1` | `ev_90bfd4a42fce7029df1ec575f9045be7e876a5e0a53425d813731d2c5948323b` | `238` 디렉터리 엔트리, `116` 리소스 (`RT_CURSOR` 72, `RT_ICON` 16, `RT_GROUP_CURSOR` 24, `RT_GROUP_ICON` 2, `RT_VERSION` 1, `RT_MANIFEST` 1) |
| `cycles_kernel_oneapi_aot.dll` | `139,144,320` | `f4edb0a9f7689a2704752425049d51d7f892f76627ee2a1f98d42085f425e715` | `ev_75f15f407324f0949fffbcbf7d6c39fe8b4253aa1c148c3400f8c9f4818eb0db` | `0` 리소스 (순수 Intel OneAPI SYCL AOT GPU 레이트레이싱 커널 컨테이너) |
| `oslexec.dll` | `57,560,064` | `0cb9429bab910da6e929abd96e4f5b3ac250de66a303c7b80484eab159b4b4dc` | `ev_02a3f5187da1acf4e33d16380970c8351bb75576a11dcaecd9cb2842949a6e84` | OpenShadingLanguage JIT 실행 엔진 (`RT_MANIFEST` 1) |
| `OpenImageDenoise_core.dll` | `46,937,088` | `961f0ac1c32039182e0373ee52f0c83c2431add2665abd8f65cd4548104f3ea8` | `ev_49758e72122b0873a841bf46bdb04c30a8501eb820492a0d6e1510671a2d2b3a` | Intel OIDN AI 디노이저 텐서 가중치 및 추론 코어 (`RT_VERSION` 1, `RT_MANIFEST` 1) |
| `usd_ms.dll` | `31,522,816` | `eab0d547bab8c1efc61e181f6a8662482dc3ee1a40d5f98c8a6b310e02eb3b7c` | `ev_d189bb37438dc572ba0f4d52b522bcd3b9ee3f433ab565380ac5bb42e37af441` | Pixar OpenUSD 모놀리식 런타임 및 Hydra 씬 그래프 (`RT_MANIFEST` 1) |
| `embree4.dll` | `26,682,880` | `4bebd29ca111987fc6c9de52ae9365b72a3d8c1929ea720d7f745707ba9e5dff` | `ev_4fd66271e5e9a499123f581b4a6fe178a1442b30b3d1b09e77eaa06f29048501` | Intel Embree 4 SIMD BVH4/BVH8 레이트레이싱 가속기 (`RT_VERSION` 1, `RT_MANIFEST` 1) |
| `openvdb.dll` | `24,628,736` | `232f183c018450ef531a041ac22290d6b39dd6545032aedd2bf731aae26210b4` | `ev_5bf6a87597975bc055e38d5d8c8a8e8f623bdc6d0e904ac635d11f2ef289e671` | 희소 볼륨 계층 트리(B+Tree/VDB) 및 레벨셋 지형·볼륨 처리 (`RT_VERSION` 1, `RT_MANIFEST` 1) |
| `openimageio.dll` | `9,396,736` | `d2d2fc31dafb093426487e41d2b1e6d770e51bc9b3a4bbba9cc912a28125a825` | `ev_d3cf016a5f743aa04d0be2f52fce3a82fe2d8741bee9e7d58f0d7afc943096cd` | 고해상도 타일 이미지 스트리밍·텍스처 캐시 (`RT_MANIFEST` 1) |
| `shaderc_shared.dll` | `4,513,280` | `f71e75b83cc2d936ffef5b3de5a415b1556eafbc74e7b11701d1ff28a2071a52` | `ev_0fbcc6adc5b70e158ed7435fe874b4cd708bd88ab3cad99799c6f47b31aed3b2` | GLSL → SPIR-V 실시간 셰이더 컴파일러 (`RT_MANIFEST` 1) |
| `opencolorio_2_3.dll` | `4,328,448` | `98ac33134af7792227615d0793efb9c7f8c0d7212e40bb1f8009554d75b4173c` | `ev_db2e334a1692f7c6a850175496f6d4c97b04f540f02dfe314b6c348d3536da37` | ACEScg / AgX / Filmic 컬러 파이프라인 (`RT_MANIFEST` 1) |
| `blender-launcher.exe` | `1,074,808` | `d6683ecba9c319b2b1943d9b8f167b21901bba44cc233034cf105da08735a6e1` | `ev_12b7ff0b8c8d6b60c7d8c46204f3848105af563412623ed0a9e34898ce2cddda` | 콘솔 창 억제 GUI 런처 (`116` 리소스) |
| `BlendThumb.dll` | `621,184` | `a612cd88768472f57e95bc6e101fca88f4203afc917b8775a3eac58d971c9223` | `ev_4905fbe898862ef1c8939fd31c22bd1bb2b1e3581d5745db51b4380ac7ae348a` | `.blend` 썸네일 추출 Shell 확장 (`2` 리소스) |
| `blender_cpu_check.dll` | `21,112` | `589790bc337f7bf6536815727fd5a41cfbec344eb4fd361eb39fd2bed1fe01b9` | `ev_7e2d24e3b3320d7f6e468797cdc6821fe38817e9236d8120bac7d77af6a90a39` | SSE4.2 / AVX / AVX2 / FMA 하드웨어 명령어셋 검증기 (`1` 리소스) |

---

## 3. `blender.exe` COFF 섹션·심볼·임포트·익스포트 및 `blender.pdb` 정적 라이브러리 분해

### 3.1 COFF 헤더 및 메모리 섹션 레이아웃
- **Machine**: `IMAGE_FILE_MACHINE_AMD64 (0x8664)`
- **Linker Version**: `MSVC 14.29` (`ImageBase: 0x140000000`, `SizeOfImage: 84,307,968` bytes)
- **Debug PDB**: `C:\Users\blender\git\blender-v420\build_release\bin\Release\blender.pdb` (GUID `{9F72697E-6960-411E-93F3-4FC0EDA697FD}`, Age `1`)
- **8개 COFF 섹션**:
  1. `.text` (`50,641,259` bytes, `RVA 0x1000`): 실행 기계어 코드 (`299,013` public symbols)
  2. `.rdata` (`21,150,790` bytes, `RVA 0x304D000`): 읽기 전용 상수, GLSL 내장 셰이더 소스, RNA 메타데이터 테이블, RTTI
  3. `.data` (`7,255,392` bytes, `RVA 0x4479000`): 전역 상태 및 DNA 기본 구조체 테이블
  4. `.pdata` (`1,834,620` bytes, `RVA 0x4CE3000`): x64 예외 처리 unwind 테이블
  5. `.tgtsym` (`2,560` bytes, `RVA 0x4EA3000`): 타겟 심볼 메타데이터
  6. `_RDATA` (`80,380` bytes, `RVA 0x4EA4000`): 보조 읽기 전용 세그먼트
  7. `.rsrc` (`1,053,664` bytes, `RVA 0x4EB8000`): PE 리소스 트리
  8. `.reloc` (`733,196` bytes, `RVA 0x4FB9000`): ASLR 재배치 테이블

### 3.2 `blender.exe` Export Table (10개 공개 심볼)
1. `NvOptimusEnablement` (RVA `0x4479000`) — NVIDIA 외장 GPU 강제 활성화 플래그 (노트북/하이브리드 GPU 렉 방지 핵심)
2. `BPyInit_bl_math` (RVA `0xEF9D00`)
3. `PyInit_aud` (RVA `0x227A480`)
4. `PyInit_manta_main` (RVA `0x252B190`)
5. `PyInit_mathutils` (RVA `0xF00100`)
6. `PyInit_mathutils_bvhtree` (RVA `0xF08B20`) — C 레벨 K-DOP BVH 트리 공간 색인 Python 바인딩
7. `PyInit_mathutils_geometry` (RVA `0xF0ACC0`) — Delaunay 삼각분할, 평면·선분 교차, 볼록 껍질(Convex Hull)
8. `PyInit_mathutils_interpolate` (RVA `0xF0DB90`)
9. `PyInit_mathutils_kdtree` (RVA `0xF0E060`) — 3D KD-Tree 최근접 이웃·반경 탐색(`find_range`)
10. `PyInit_mathutils_noise` (RVA `0xF0EEF0`) — 다중 프랙탈·Perlin·Voronoi 지형 노이즈 생성기

### 3.3 `blender.pdb` 252개 정적 라이브러리(`*.lib`) 및 5,919개 번역 단위(TU) 핵심 아키텍처 매핑

전체 252개 라이브러리는 [blender_pdb_libraries.json](file:///C:/Users/권을/Documents/hgis-3d-engine/docs/rea/blender_pdb_libraries.json)에 수록되어 있으며, **대규모 3D 실사 지형·건물·고고학 평가 엔진에서 렉(Lag)을 제거하고 실사 품질을 달성하는 7대 핵심 서브시스템**은 다음과 같다:

| 순위 | 정적 라이브러리 | TU 수 | 대표 오브젝트(`.obj`) 모듈 | 경주 3D 지형·고고학 엔진 설계 반영 사항 |
|---:|---|---:|---|---|
| 1 | `bf_draw.lib` + `bf_draw_shaders.lib` | `180` + `409` | `draw_manager.cc`, `draw_command.cc`, `draw_pbvh.cc`, `eevee_hizbuffer.cc`, `eevee_ambient_occlusion.cc`, `eevee_shadow.cc`, `eevee_lightprobe_volume.cc`, `eevee_raytrace.cc`, `overlay_armature.cc` | **EEVEE Next & GPU-Driven Draw Manager**: 오브젝트별 개별 드로우콜 대신 GPU 인스턴싱/배치 버퍼(`draw_instance_data.cc`)로 묶어 수천 동의 경주 건물·고분·유구를 단일 패스로 렌더링. Hi-Z 및 GTAO 기반 지형 미기복 음영 강화. |
| 2 | `bf_gpu.lib` + `bf_gpu_shaders.lib` | `97` + `179` | `gpu_batch.cc`, `gpu_storage_buffer.cc`, `gpu_compute.cc`, `gl_drawlist.cc`, `vk_backend.cc` | **GPU Batch & VBO/IBO Attribute Extraction**: 지형 청크와 3D 건물의 정점(Pos, Normal, UV, Color/TPI)을 인터리브드 VBO/Instanced Array로 고정 배치하여 CPU-GPU 병목 제거. |
| 3 | `bf_blenkernel.lib` + `bf_blenlib.lib` | `275` + `133` | `pbvh.cc`, `pbvh_bmesh.cc`, `BLI_kdopbvh.c`, `kdtree_3d.c`, `mesh_evaluate.cc`, `task_scheduler.cc`, `task_pool.cc` | **PBVH (Partitioned BVH) & 3D KD-Tree 공간 색인**: 지형 삼각형과 유적을 계층적 AABB 노드로 분할(`BLI_kdopbvh`)하여 카메라 절두체 컬링(Frustum Culling), 마우스 레이캐스팅($O(\log N)$), 조사구역 반경 500m/1000m 유적 탐색(`kdtree_3d_find_range`)을 즉시 수행. |
| 4 | `bf_intern_sky.lib` | `4` | `sky_model.cpp`, `sky_nishita.cpp`, `sky_float3.h` | **Nishita 물리 대기 산란 모델**: 태양 고도·방위각·오존·에어로졸(미세먼지)·공기 분자 산란(Rayleigh/Mie)을 물리적으로 적분하여 경주 분지의 시간대별 실사 하늘·태양광·일조 그림자 시뮬레이션 구현. |
| 5 | `bf_depsgraph.lib` | `48` | `deg_eval.cc`, `deg_eval_copy_on_write.cc`, `deg_eval_flush.cc`, `deg_builder_nodes.cc` | **비동기 더티 플래그 & Copy-on-Write 평가 그래프**: 카메라 이동·태양 각도 변경·시굴 트렌치 편집 시 변경된 타일/레이어만 재계산하여 프레임 드랍(0ms 스터터링) 방지. |
| 6 | `cycles_bvh.lib` + `cycles_kernel.lib` + `cycles_scene.lib` | `9` + `9` + `31` | `bvh.cpp`, `bvh_embree.cpp`, `bvh_unaligned.cpp`, `light.cpp`, `shader.cpp` | **고정밀 가시권(Viewshed) 및 오프라인 실사 레이트레이싱**: 왕경 주요 거점(월성, 첨성대, 황룡사지, 남산, 명활산성) 간 상호 가시권(Line-of-Sight / Viewshed) 레이캐스팅 및 무설치 추출 `blender.exe --background` 연동 Cycles 렌더 파이프라인. |
| 7 | `opencolorio_2_3.dll` + `datafiles/colormanagement` | LUT | `AgX_Base_sRGB.cube`, `pbrNeutral.cube` | **AgX / PBR Neutral 톤매핑**: 위성사진(VWorld/ESRI/Google) 알베도와 직사광 하이라이트가 백화(Clipping)되지 않고 자연스러운 필름 다이나믹 레인지를 유지하도록 셰이더 내 AgX S-Curve 색보정 적용. |
