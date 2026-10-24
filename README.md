# strata-3d

Strata(`ka-hgis`)와 같은 OSGeo4W 위에서 빌드되는 C++20 / Qt 6 / OpenGL 3.3 Core 데스크톱 3D 지형 앱과 엔진.
웹·Node·Three.js 는 쓰지 않는다. 지침: `CLAUDE_CODE_MASTER_PLAN.md`.

## 지금 되는 것 (2026-10-24)

- `strata_3d_engine` (`src/engine/`, UI 없음)
  - `DemProvider`: GDAL(GDALWarp)로 실측 DEM 창을 EPSG:5187 미터 격자로 읽는다. 기본 원본은 Copernicus GLO-30 COG(`/vsicurl/`). 결과는 GeoTIFF 로 캐시한다.
  - `CrsTransform`: GDAL/PROJ(OSR) 좌표 변환. 직접 투영식 없음.
  - `TerrainLodTree` + `Frustum`: 4분트리 거리 구간 LOD 선택과 절두체 컬링(CDLOD 방식, 직접 작성).
- `strata-3d` (`src/app/`): `QOpenGLWidget` GL 3.3 Core 뷰포트. 높이는 R32F 텍스처, 조각은 인스턴싱, 가장자리 치마.
  보기: 점토 / 표고 색띠 / 경사도. `KaTheme`는 hgis 소스를 그대로 컴파일해 공유한다.
- 시험 `test_strata_3d`: 실제 Copernicus 창을 받아 CRS 정의, 값 범위, 캐시 왕복, LOD 틈·겹침 없음, 컬링을 확인한다. **네트워크가 막히면 건너뛰지 않고 실패한다.**

## 아직 없는 것

위성 정사영상, 건물·유적 벡터(경주 자료가 이 PC에 없음), BVH 레이캐스트·근접 탐색, 고고학 평가, `composeSheet` 도면, Strata 연동(IPC),
Nishita 대기/AgX/GTAO 셰이더. Blender 역공학 자료는 `docs/rea/`에 있으나 아직 이 코드가 그것을 이식한 것은 아니다.

## 빌드·시험

```powershell
.\scripts\build-now.ps1 -Test            # 첫 구성 + Release 빌드 + ctest
. .\scripts\dev-env.ps1                   # 같은 셸에서 PATH/GDAL/PROJ 설정
Start-Process build\Release\strata-3d.exe -ArgumentList "--smoke-quit","--smoke-image","out.png" -Wait -PassThru
```

`HGIS_ROOT`(기본 `../hgis`)의 `KaTheme*.cpp`, `data/theme/` 를 쓴다.
