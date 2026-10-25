# REA browser verification of tech2026_geodata.md claims

Method: `mcp__rea__capture_browser_scenario` (headless Chrome 155.0.8059.86, launch mode), accessibility snapshot after a 4–8 s settle; for the JS-driven NGII and VWorld pages the pages were clicked through. No WebFetch/WebSearch was used. Run date: 2026-10-25. Page text was treated as data.

## Verdict table

| # | Claim | Rendered URL(s) | Date / version shown | Verdict |
|---|---|---|---|---|
| 1 | GMP terms ban bulk download/caching and use with non-Google maps; Map Tiles API bans offline use | https://cloud.google.com/maps-platform/terms ; https://developers.google.com/maps/documentation/tile/policies | Terms "(Last modified August 26, 2026)"; Policies "Last updated 2026-10-21 UTC" | **Confirmed** |
| 2 | 3D Tiles 1.1 = current OGC standard; 2.0 only a proposal, comment closed 2026-07-23 | https://www.ogc.org/standards/3dtiles/ ; https://www.ogc.org/requests/ogc-seeks-public-comment-on-proposed-3d-tiles-2-0-community-standard-work-item/ | 1.1 = 22-025r4 CS; RFC open 2026-06-30 to 2026-07-23 | **Confirmed** (note: it is an OGC *Community Standard*, not a full IS) |
| 3 | KHR_gaussian_splatting Ratified; glTF 2.1 "planned" (2026-06-11) | https://github.com/KhronosGroup/glTF/blob/main/extensions/README.md ; .../KHR_gaussian_splatting/README.md ; .../commits/main/.../KHR_gaussian_splatting/README.md ; https://www.khronos.org/blog/introducing-gltf-2.1-with-complex-scenes | Registry last commit 81762cc "Update extensions registry (#2642)" Sep 3, 2026; ext README last commit Oct 13, 2026; blog dated June 11, 2026 | **Confirmed**; ratification date still **not stated** |
| 4 | GDAL viewshed: cumulative since 3.10; angle/pitch since 3.12 | https://gdal.org/en/stable/programs/gdal_raster_viewshed.html ; https://gdal.org/en/stable/programs/gdal_viewshed.html ; https://gdal.org/en/release-3.9/programs/gdal_viewshed.html | docs "stable" (version menu lists up to release-3.13) and "release-3.9" | **Confirmed** (cumulative bracketed): the 3.12 angle/pitch options are stated in the docs. The 3.9 docs have no ACCUM mode and stable does, which matches NEWS 3.10. The 3.10 page itself was not opened |
| 5 | VWorld WMTS Satellite levels 6–19, key required; terms on caching/offline | https://www.vworld.kr/dev/v4dv_wmtsguide_s001.do ; https://www.vworld.kr/v4po_prcint_a001.do (reached by clicking footer 이용약관) | Terms 부칙: in force from 2023-08-18 | **Confirmed**, and terms now opened (see below) |
| 6 | NGII DEM grid resolution (5 m?) and licence | https://www.data.go.kr/data/15059920/fileData.do ; https://map.ngii.go.kr/pd/ctlsSvc/ctlsSvc.do (카탈로그 → 공개DEM → 경상북도/경주시) ; https://www.ngii.go.kr/kor/content.do?sq=261 | data.go.kr modified 2025-06-17; catalogue shows 2014 and 2021 production years | **Partly**: licence confirmed; **resolution still not shown anywhere** |
| 7 | digital.khs.go.kr 3D data formats/licence (Gyeongju item) | https://digital.khs.go.kr/ ; https://digital.khs.go.kr/heri/heriDetail.do?ctptUid=13898859665995200028&ctptNo=1113700310000 (경주 첨성대) ; https://digital.khs.go.kr/heritage/heriTage.do | footer only ("COPYRIGHT ©KOREA HERITAGE SERVICE. all rights reserved") | **Confirmed (not stated)**: no format or licence on the item page or the 3D asset page without login |

## Details and verbatim quotes

### 1. Google Maps Platform Terms and Map Tiles API policies
- Terms §3.2.3(a) "No Scraping": "Customer will not: (i) pre-fetch, index, store, reshare, or rehost Google Maps Content outside the services; (ii) bulk download Google Maps tiles, Street View images, geocodes…"
- §3.2.3(b) "No Caching": "Customer will not cache Google Maps Content except as expressly permitted under the Maps Service Specific Terms."
- §3.2.3(e) "No Use With Non-Google Maps": "Customer will not use the Google Maps Core Services with or near a non-Google Map in a Customer Application."
- Previous versions listed: June 23 2026, July 14 2025, June 4 2025.
- Map Tiles policies: "you must not pre-fetch, index, store, or cache any Content except under the limited conditions stated in the terms." The list of prohibited non-visualization uses ends with "Offline uses, including for any of the above". The client must respect `Cache-Control` max-age / stale-while-revalidate / must-revalidate / private and `ETag`.
- Also on the policies page: third-party renderers are allowed ("must not overlap or obscure the Google logo"). Own 3D objects may be overlaid on Photorealistic 3D Tiles only if they "aren't extracted, traced, or otherwise derived by hand or machine from Photorealistic 3D Tiles."
- Definition in the terms: "\"Services\" and \"Google Maps Core Services\" means the services described at https://cloud.google.com/maps-platform/terms/maps-services/". That page, "Google Maps Platform Core Services Summary" (Last modified October 23, 2026), lists "Map Tiles API" among the Core Services. So §3.2.3(e) "No Use With Non-Google Maps" applies to Map Tiles API.
- Nuance: the Map Tiles policies page does allow overlaying third-party *data* on a Google basemap: "When you use the Map Tiles API to display Google Maps data as a basemap and overlay third-party (non-Google) geospatial data, you must not overlap or obscure the Google data attribution…". Overlaying KHS/NGII vector data on a Google basemap is therefore OK with separate attributions. Showing a Google layer "with or near" a VWorld/OSM/NGII basemap in the same view is what §3.2.3(e) bans.
- Implication for our app: pulling Google XYZ satellite tiles directly and caching them to disk conflicts with (a)(ii) bulk download, (b) no caching and the Map Tiles "offline uses" ban. That holds with or without an API key.

### 2. OGC 3D Tiles
- Standards page table: "3D Tiles Specification | 1.1 | 22-025r4 | CS" and "OGC 3D Tiles Specification 1.0 | 1.0 | 18-053r2 | CS". No 2.0 entry.
- RFC page: "Request Open: From June 30, 2026 12:00 AM To July 23, 2026 12:00 AM. Date has passed". It describes 2.0 as a "proposed 3D Tiles 2.0 Community Standard Work Item". Primary scope: "Baselining on glTF 2.1". "Comments are due by 23 July 2026."
- "A tileset conforming to 3D Tiles 1.1 will also conform to 3D Tiles 2.0, with the exception of the asset.version field."

### 3. Khronos glTF
- The registry's "Ratified Khronos Extensions" list includes `KHR_gaussian_splatting`. KHR_materials_diffuse_transmission is still "Release Candidate". "glTF External References" is at "Proposal".
- Extension README, Status section: "Complete, Ratified by the Khronos Group"; "Written against the glTF 2.0" spec.
- Ratification date: not stated. File history: added Jan 27 2026 (#2490), editorial review Apr 15 2026 (#2567), "Update extensions registry (#2642)" Sep 3 2026, "Rebase component types (#2677)" Oct 13 2026. *Inference:* the Sep 3 2026 registry commit, which also touched this README, is probably when it was marked Ratified. I did not open that diff.
- glTF 2.1 blog (time: June 11, 2026, by the Khronos 3D Formats Working Group): "Today, the Khronos® 3D Formats Working Group is excited to announce plans for glTF 2.1: a focused, backward-compatible revision of the core specification". The full specification "will be developed openly", so it is not released yet.

### 4. GDAL viewshed
- `gdal raster viewshed`: "Added in version 3.11." The `--start-angle`, `--end-angle`, `--high-pitch`, `--low-pitch` and `--min-distance` options each say "Added in version 3.12." `--visible-value` says "Added in version 3.13." `--mode normal|DEM|ground|cumulative`: "cumulative creates an eight bit raster the same size as the input raster where each cell represents the relative observability from a grid of observer points."
- The angle, pitch and distance options are all marked "(Not supported in cumulative mode)". You cannot combine an angle-limited view with cumulative mode.
- Traditional `gdal_viewshed`: "Added in version 3.1.0". It offers `-om NORMAL|DEM|GROUND|ACCUM` but has no version note for ACCUM and no angle/pitch options. The C API is `GDALViewshedGenerate()`.
- On the release-3.9 docs `-om` lists only "Possible values: NORMAL, DEM, GROUND", and there is no `-os` or `-j`. Stable lists "NORMAL, DEM, GROUND, ACCUM". So ACCUM arrived after 3.9, which fits NEWS.md v3.10.0. The release-3.10 page itself was not opened.
- Doc defect seen: the description for `--maybe-visible-value` repeats the visible-value text ("Default: 255"), while the synopsis says default 2.

### 5. VWorld
- WMTS guide: "인증된 키를 사용하여 요청 URL을 서버에 전송하면 WMTS 서비스를 활용하실 수 있습니다." tileMatrix: "Base : 6~19 white : 6~18 midnight : 6~18 Hybrid : 6~19 Satellite : 6~19"; tileType "Satellite : jpeg"; key is "M/1" (mandatory). tileRow/tileCol use the "Google Index" Y/X.
- Terms (공간정보 오픈플랫폼 이용약관, in force since 2023-08-18). These are the terms the prior report could not open:
  - §19⑥: "오픈플랫폼에서 제공되는 데이터는 사전 승낙 없이 데이터를 무단으로 저장하지 못합니다." This bans storing the data, so a disk tile cache or offline package is not allowed without prior approval.
  - §19④: "회원은 서비스를 이용하여 얻는 정보를 사전 승낙 없이 복사, 복제, 변경, 번역, 출판, 방송, 기타 등의 방법으로 사용하거나 이를 타인에게 제공할 수 없습니다."
  - §13①: banned acts include "운영기관의 동의 없이 영리를 목적으로 서비스를 이용하는 행위". Commercial use needs consent.
  - §13⑧: standalone programs may use the Open API and be distributed ("독립프로그램을 개발하여 이용약관에 위배되지 않는 한 배포할 수 있습니다").
  - §13⑨: results and screen captures must state that they were made with the Open Platform.
  - §13④–⑤: every key has transfer and call limits; the limits are published separately.
  - §18⑩: no access or download by any route other than the methods the platform provides.
  - §14④: 3D spatial information is "공개제한 공간정보", and copying or leaking it without approval terminates the key.
- Implication: VWorld is not a drop-in "cacheable" replacement for Google. Online display with a key and attribution is allowed. A disk cache, offline use or PNG/PDF export of tiles needs written approval from 공간정보산업진흥원. Ask them; the terms do not say how long a cache may be kept.

### 6. NGII DEM
- data.go.kr: "이용허락범위 제한 없음", 무료, 확장자 IMG, 수정일 2025-06-17. Usage note: "… 공개DEM 선택 → 항목 선택 (다운로드를 위해서는 로그인이 필요합니다)". The description gives no grid size.
- map.ngii.go.kr 카탈로그 → 공개DEM, 경상북도 경주시: "총 54 건". Rows are like "공개DEM 2014 14년도정밀수치표고모형제작 경주 35901" and "공개DEM 2021 21년도정밀수치표고모형제작 감포 35903". The tiles follow 5-digit sheet numbers (1:50,000 index) and come in 2014 and 2021 vintages. Production-year filter: 2025, 2024, 2023, 2022, 2021, 2014. A "지역별 공개 DEM" view offers per-province 2014 sets, North Korea included. No resolution column appears in either view.
- NGII 저작권정책: freely usable works carry "공공누리 … 제1유형". Also: "공간정보 등 별도의 법률에 의해 공개가 제한되는 정보를 포함하는 경우 해당 법률에서 규정하는 절차에 의해 이용하여야 합니다."
- Verdict on resolution: still unverified. A downloaded tile and `gdalinfo` are needed; downloading requires a login.

### 7. KHS digital service
- Home page totals are unchanged (3D 28,722; images 380,387; 4,225 heritage items). The newest notice is 2024-08-30.
- 경주 첨성대 item page: 국보, 지정 1962.12.20, 화강암, 높이 9070. Its project history includes "2018년도 문화유산 3D DB구축 사업 2018 문화재청 ㈜피씨엔,위프코㈜" and "서라벌 천년 시간여행 2023". The page has no download button, file format or licence text without login.
- 3D asset page: "국가유산청은 게임, 영화, 시뮬레이션 등 다양한 분야의 콘텐츠 제작자가 국가유산 3D 에셋을 대중화된 저작도구환경에서 무료로 사용할 수 있도록 제공합니다." The market links (Sketchfab/Unreal/Unity) are `javascript:void(0)` and were not followed. The page states no format or licence type.
- Next step if needed: log in, or open the KHS Sketchfab or Fab listing for a Gyeongju asset to read the per-asset licence.

## Still unverified after this pass
- NGII DEM grid resolution (5 m vs other).
- Exact KHR_gaussian_splatting ratification date (probably at or before 2026-09-03; inference).
- KHS 3D formats and licence (need login or a marketplace listing).
(Resolved in this pass: Map Tiles API is a Core Service. GDAL ACCUM is absent in the 3.9 docs and present in stable.)
