# Strata(2D) 화면 목록 — Strata 3D 설계용 (2026-10-25, 코드 조사)

파일:줄은 `C:\Users\권을\Documents\hgis\src\` 기준. [미확인] 표시는 직접 읽지 않은 것.

## 리본 (`app/MainWindowRibbon.cpp`, 묶음 248–254)

| 묶음 | 칩 · 아이콘 id → SVG · 동작 · 줄 |
|---|---|
| 조사 | 새 조사 new→file-plus newSurvey Ctrl+N :270 · 열기 open→folder :274 · 저장 save→save :277(저장 안 된 편집이 있으면 save_unsaved) · 다른 이름 save_as :285 |
| 기록 | 선택 select→mouse-pointer-2 :300 · 측거 measure→ka-ruler-horizontal :330 · 그리기 draw_poly→vector-polygon Ctrl+D :339 · 시굴격자 trench_grid→grid-3x3 :363 · 버퍼 buffer→circle-dot-dashed :525 |
| 자료 받기 | 지적 cadastral→ka-cadastral downloadCadastral :471 · 수치지형 topo_download→download :382 · 유산 heritage→house fetchNearbyHeritage :541 |
| 배경 지도 | 지형 contour→mountain :369 · DEM dem→mountain-snow :399(오른쪽 클릭: 국토지리원 DEM .img, 음영, 표현, 측량점 등고선) · 등고선 survey_contour→target :429 · 토양 soil→layers :437 · 고지형 paleo→rotate-ccw-clock :462 · 지질 geology→ka-geology :501 · 수계 river→waves-horizontal :508 · 옛 지도 old_map→map(대동여지도, 1919 조선지형도) :485 |
| 정합 | 정합 georef→locate-fixed :522 |
| 내보내기 | 도면 pdf→file-text Ctrl+L :552 · 인쇄 print→printer Ctrl+P :559 · GeoTIFF geotiff→image :566 · 검수·제출 export_convert→box Ctrl+E :574 |
| 기타 | 더보기 more→ellipsis :623 (불러오기·배경 / 화면 / 계정과 키 / 상태와 정보) |

- 2차 막대(아이콘 20 px): 버퍼 → 「주변유적경계 ›」 500m · 1000m · 엑셀 내보내기 · 닫기(:795–809). 그리기 → 도형선택 · 자석 · 쉽게그리기 · 조사구역 그리기 · 유구 그리기 · 유구 선 · 유물위치표시 · … · 폴리곤 묶기·나누기 · 구간 분리 · 겹친 곳 지우기 · 닫기(:841–944).
- 왼쪽 칸(`MainWindow.cpp:786–949`): 「레이어」 제목 + 파일함·전체 끄기/켜기, 구역 조사 데이터 / 지적도 / 주변·참조 / 배경 지도(`KaLayerSectionDelegate.cpp:42–45`), 아래 파일함(세로 나눔 380/260, 폭 160–720).
- 오른쪽 칸: KaInspectorPanel(「선택한 유구」, 속성/스타일 탭, 폭 272 또는 300, 260–380).
- 상태줄(`KaStatusBar.cpp:48–140`): 자석 · 저장 안 됨 n건 · X Y · 축척(1:100…1:500000) · 작업 5186 · → 제출 5179 · 지도갱신.

## 아이콘과 이식성

- `KaIcons::icon(id)`(KaIcons.cpp:1044): KaIconsMockupMap.cpp:16–76 에 있는 id 는 둥근 타일(반경 0.25) + Lucide SVG(타일의 18/32). SVG `:/ka-hgis/icons/{lucide|ka}/*.svg`(icons-mockup.qrc: Lucide 1.49.0 45개, ISC + ka 3개). 그 밖(layout_*, crs, terrain_3d …)은 KaIconsOutline 이 직접 그림.
- QGIS 없이 빌드: KaBeginnerRibbon, KaIcons*, KaTheme*, KaChip, KaStatusBar(+ core/EditBufferSummary.h). KaThemeOptions 는 Windows 전용(dwmapi).

## 받기

| 자료 | 화면 | 범위 | 원천 | 저장 | QGIS |
|---|---|---|---|---|---|
| 유산 | KaHeritageRegionDialog → KaHeritageBrowser(WebEngine) | 조사구역의 시·군, 5 km 로 자름 | intranet.gis-heritage.go.kr 6종 | AppLocalData/주변유적/<시도 시군>/원본, 서약서 <조사>/주변유적/receipts | 창·브라우저 아님, 지역 찾기·불러오기 예 |
| 지적 | KaDownloadProgressDialog | 조사구역 5 km | VWorld(계정) | <조사>/지적도 | 예 |
| 수치지형 | KaTopographicBrowser(WebEngine) + KaTopographicScopePanel | 화면 중심 5 km | 국토지리정보원(로그인) | <조사>/지형도/원본, 합침 | 브라우저 아님, 범위·불러오기 예 |
| DEM | 진행 창 | 화면 | Copernicus GLO-30 | 조사 폴더 DEM.tif | 예 |
| 토양·고지형 | 진행 창 | 화면, ≤80 km | gis.naas.go.kr | 토양도_흙토람.gpkg | 예 |
| 지질 | 진행 창 | 화면, ≤80 km | data.kigam.re.kr | 지질도_KIGAM.gpkg | 예 |
| 수계 | 진행 창 | 화면, ≤160 km | VWorld WFS(키) | 수계도_VWorld.gpkg | 예 |
| 지형·옛 지도·위성 | 없음(온라인 타일) | — | OpenTopoMap · 옛 지도 WMS · VWorld 위성 / Google | 저장 안 함 | 예 |

## 도면

- KaDrawingStudio(QgsLayoutView + QgsPrintLayout): A4/A3, 「현장」 317×220 mm. 곁 카드: 도면 항목(범례·표제란·PDF·인쇄), 방위(4종, 28 mm), 도면 정보(축척 칩 20…25000, 축척 막대 3종, 좌표계, 좌표 격자). 아래 아이콘 줄(:1273–1335).
- 자동 도면(`core/LayoutService.cpp:110–155`): 조사구역도 1:5000 격자 100 m · 유적위치도 1:25000 격자 1000 m · 유구배치도 1:1000 격자 20 m · 개별유구실측도 1:100 격자 2 m. 각 틀: 제목 띠, 좌표 격자 테두리, 방위, 범례, 「도면 정보」 표제란, 축척 막대, 1:N, 좌표계. **출처표 없음.** 층위도 코드 없음.
- 3D 그림: 입체지형 탭 → 1600×1000 PNG → `Terrain3dLayoutService::buildSheet` 가 420×297 `terrain3d_sheet`(그림 + 축척용 보이지 않는 지도 항목 + 방위·축척 막대·좌표계), `replacePicture` 로 갈아 끼움(MainWindowExport.cpp:318–440). `KaDrawingStudio::placeTerrain3dPicture`(:1361). KaStrata3dLink 「도면 받기」는 PNG·PDF 저장만(도면 틀에 넣지 않음).

## 테마

- 글꼴 맑은 고딕(본문 13 px), 고정폭 Consolas. 리본 타일 32 px(56…20), 이름 13 px, 묶음 제목 11 px 굵게, 칩 모서리 8. 상태 칩 높이 22, 모서리 11. 상태줄 34 px.
- 기본 색은 파랑(#1F6FB2) 계열, 「새 모양」(paper)은 흙색 #B5573A, 바탕 #F5F4ED, 타일 #F0EEE6(KaThemePaper.cpp). Strata 3D 는 paper.

## 주변유적

- 500m/1000m: BufferAnalysis::addDistanceRing, 상태줄 「포함 주변유적 N곳」.
- 엑셀 내보내기: NearbyHeritageExport::collect → 「주변유적_현황」 17열(연번, 자료구분, 레이어명, 유적명, 유산코드, 보고서명(서지), 사업명, 소재지, 조사기관, 시대, 유적유무, 허가번호, 반경구분, 방향(8방위), 거리(m), 방위각(도), 위치설명). 화면 표 없음.

[미확인] createSurveyContours 세부, 브라우저 안 단계, 그리기 막대의 스케치 단추, KaTerrain3dLayoutStudio 세부, VWorld 위성 주소 조립.
