# strata-3d

Strata(`ka-hgis`)와 같은 OSGeo4W 위에서 빌드되는 C++20 / Qt 6 / OpenGL 3.3 Core 데스크톱 3D 지형 앱(`strata-3d.exe`)과 엔진(`strata_3d_engine`).
웹·Node·Three.js 는 쓰지 않는다. 지침: `CLAUDE_CODE_MASTER_PLAN.md`. 제3자 고지: `THIRD_PARTY_NOTICES.md`.

## 되는 것 (2026-10-24, 시험으로 확인)

| 영역 | 내용 | 확인 |
|---|---|---|
| 실측 DEM | GDAL(GDALWarp)로 Copernicus GLO-30 COG(`/vsicurl/`)를 작업 좌표계(5186/5187) 미터 격자로. GeoTIFF 캐시 | GDAL 명령줄 값과 대조 |
| 지형 | R32F 높이 텍스처 + 인스턴싱 4분트리 LOD + 치마 + 절두체 컬링, 소벨 평활 법선 | 틈·겹침 없음 시험 |
| 위성 실사 | VWorld 위성 WMTS(Strata 와 같은 키, Referer 인증) 전체 4096 px + 중심 ±3 km 4096 px. 약관상 디스크에 저장하지 않음, Google 은 쓰지 않음 | 시험 `satelliteTilesCoverTheWindow`(VWorld 출처 확인) |
| 건물·유적 | OSM 외곽선(높이 태그 없으면 「미상」 6 m 추정, 옅은 색), 국가유산청 전국 지정유산, Strata SHP 캐시 | OSM–국가유산청 첨성대 1.06 m 일치 |
| 공간 가속 | 14-DOP BVH(Blender BLI_kdopbvh 와 같은 축 표), k-d 트리 | 전수 검사와 동일 결과, 약 2,600배 빠름 |
| 고고학 평가 | 500/1000 m 둥근 버퍼, 이격거리·방위, 경사·향·TPI·Weiss 분류, 지형 단면 | 반경 500.0 m(Miter 돌출 없음), 측지 거리와 0.2 % |
| 렌더 | Nishita 하늘 → Blender GTAO 이식 → AgX 합성의 HDR 파이프라인 | AgX: OCIO 기준표 1006색 최대 오차 1.7e-5 |
| 도면 | 3072² 캡처 + 제목·축척·방위·범례·출처·좌표계·평가 요약 → PNG, A3 PDF | 파일 크기·내용 확인 |
| 시굴 트렌치 | 「시굴 트렌치 그리기」로 지형 위 두 점을 눌러 3D 띠 + 하단 거리-표고 단면 패널(길이·방위·기울기), 도면 요약·범례에 포함 | 실제 마우스 이벤트로 찍은 전체 창·도면 캡처를 눈으로 확인 |
| Strata 연동 | 로컬 파이프 JSON(openSurvey/evaluate/profile/renderSheet), 상태 파일. Strata 입체지형 탭의 「strata-3d로 열기」·「도면 받기」(hgis `c04aa37`)가 이 파이프를 호출 | 실제 앱을 띄운 e2e(양쪽 저장소), 조사 원본 해시 불변 |

## 한계 (숨기지 않는다)

- **VWorld**: 예전에 키가 `INCORRECT_KEY` 로 거절된 것은 Referer 머리글을 빠뜨렸기 때문이었다(2026-10-25 수정, Strata 와 같은 `https://localhost`). 이용약관 §19⑥ 때문에 타일은 메모리에서만 쓴다. 건물 WFS 는 아직 구현하지 않았다.
- **건물 높이**: OSM 에서 높이 태그가 있는 것은 5%도 안 된다. 나머지는 추정 높이이고 화면에서 옅게 구분한다. 유적은 표지 기둥과 경계 띠이며 복원 모델이 아니다.
- **DEM**: 30 m 격자(국토지리정보원 1:5,000 표고가 아님). 근접 지형은 부드럽다.
- **평가**: 지형·좌표·지정유산 목록으로만 계산한다. 층서·유물은 만들지 않는다. 「자료에 없음」은 「없음」이 아니다.
- **하늘 색**: 기하·밀도·위상 상수는 Blender 4.2.3 실행 파일에서 확인했지만 파장 계수는 Nishita 논문 RGB 값이다(`docs/sky-constants.md`).
- **GTAO**: Blender 원문의 이식이되 HiZ 광선 추적 대신 선형 깊이 밉, 청색 소음 대신 IGN 을 쓴다. 효과는 측정되지만(평균 밝기 −1/255, 2 % 픽셀) 건물 밀집부에서도 미묘하다.
- **hgis 쪽**: 호출 단추는 hgis 저장소(`src/core/Strata3dLink.*`, `src/app/KaStrata3dLink.*`)에 있고, 클라이언트는 이 저장소 `src/bridge/StrataBridgeClient.*` 의 사본이다(규격 `docs/bridge-protocol.md`). 단추 화면은 시험 위젯에서만 캡처했고 실제 Strata 본체 창에서는 눈으로 보지 못했다.
- **트렌치**: DEM 30 m 격자의 지표면 단면이다. 수 m 규모 트렌치는 격자 해상도 밖이며 층서가 아니다. 트렌치 좌표는 화면에만 있고 Strata 조사 파일로 저장하지 않는다(조사 원본은 읽기 전용).
- **BVH**: Blender 의 알고리즘 설계와 축 표, 분기 노드 수 식을 따랐지만 바이트 단위 이식은 아니다.

## 빌드·시험

```powershell
.\scripts\build-now.ps1 -Test              # 첫 구성 + Release 빌드 + ctest
. .\scripts\dev-env.ps1                     # 같은 셸에서 PATH/GDAL/PROJ 설정
.\scripts\shot.ps1 -Out out.png -Cam "200,32,1500"   # 화면 한 장(스모크) 저장
build\Release\strata-3d.exe --smoke-quit --eval-center --sheet-out 도면.png
```

시험은 실제 네트워크(Copernicus, VWorld, Overpass, 국가유산청)를 쓴다. 막히면 건너뛰지 않고 실패한다.
`HGIS_ROOT`(기본 `../hgis`)의 `KaTheme*.cpp`, `data/theme/` 를 그대로 컴파일해 디자인을 공유한다.
