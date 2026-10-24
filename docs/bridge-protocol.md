# Strata ↔ strata-3d 연동 프로토콜 (v1)

strata-3d 는 실행되면 로컬 이름 있는 파이프 `strata-3d-bridge-v1`(윈도: `\.\pipe\strata-3d-bridge-v1`)을 연다.
`--bridge-pipe <이름>` 으로 바꾸고 `--no-bridge` 로 끈다. 요청·응답은 **줄바꿈으로 끝나는 JSON 한 줄**이며
요청에 `id` 가 있으면 응답이 그대로 돌려준다. 응답에는 항상 `ok`(성공 여부)와 `protocol`(=1)이 있다.
한 번에 하나의 비동기 요청만 처리하고, 처리 중에 다른 요청이 오면 `{"ok":false,"error":"busy: …"}` 로 거절한다.

클라이언트 라이브러리: `src/bridge/StrataBridgeClient.{h,cpp}` (Qt Network 만 필요, hgis 쪽에서 그대로 링크 가능).
`strata3d_bridge::request(파이프, 요청, 제한시간ms, &오류)` 는 블로킹 호출이다(워커 스레드에서).

## 명령

| cmd | 요청 필드 | 응답 필드 |
|---|---|---|
| `hello` / `state` | — | `app`, `scene_complete`, `survey`, `commands` |
| `openSurvey` | `gpkg` (조사 프로젝트 경로) | `survey`(path, layers, center, half_extent_m, 레이어별 개수), `scene_complete`, `warnings`, `feature_count` |
| `evaluate` | `target:"survey"` 또는 `x`,`y`(작업 좌표계 미터) | `evaluation`(아래) |
| `profile` | `a{x,y}`, `b{x,y}`, `step`(m) | `profile[{d,x,y,z}]`, `basis` — 지표면 단면이며 층서가 아니다 |
| `renderSheet` | `png`, `pdf`(선택) | `sheet{png,pdf,png_bytes,pdf_bytes}` — 3072×3072 입체지형 도면 |
| `quit` | — | `ok` |

## 좌표계

작업 좌표계는 조사 파일을 따른다: 파일의 좌표계가 EPSG:5186(중부원점)이면 5186, 5187(동부원점)이면 5187, 그 밖이면 5187.
EPSG:5179 로 가정하지 않는다. `evaluation.crs` 가 응답 좌표의 좌표계다.

## evaluation

`terrain{elevation_m, slope_deg, aspect_deg, aspect_name, tpi_small_m, tpi_large_m, z_small, z_large, landform}`,
`radii[{radius_m, buffer_area_m2, buffer_max_reach_m, buildings, heritage[{id,name,category,source,distance_m,bearing_deg,bearing_name,elevation_m,delta_elevation_m}]}]`,
`visibility[{name,category,distance_m,visible,blocked_at_m}]`, `notes[]`, `summary`.

- 버퍼는 둥근 연결(사분면당 9분할)이다. 거리는 조사지(점·다각형)에서 지물까지 최단거리.
- `notes` 의 「평가에 쓴 자료」줄이 근거 자료의 출처·건수다. **자료에 없는 유산·건물은 "없음"이 아니라 "자료에 없음"이다.**
- 시야는 DEM(30 m) 지형만으로 계산했고 건물·수목은 넣지 않았다. 지형 분류는 DEM 형태(Weiss 2001)만으로 가른 참고값이다.

## 상태 파일

`%LOCALAPPDATA%\strata-3d\bridge-state.json` — 파이프 이름, pid, 갱신 시각, 열린 조사, 마지막 평가, 마지막 도면 경로.
앱이 꺼져 있어도 Strata 가 마지막 결과(도면 PNG/PDF 경로)를 읽어 도면 스튜디오·보고서에 가져갈 수 있다.

## 원본 보호

조사 파일(`.gpkg`)은 `IMMUTABLE=YES` 읽기 전용으로만 열며, `-wal`/`-shm` 도 만들지 않는다(시험이 해시와 부산물로 확인).
