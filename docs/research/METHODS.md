# 자료 수집 방법 기록

> 2026-10-25 시작. 이 문서는 **어떻게 모았는지**를 남긴다. 무엇을 찾았는지는 `FINDINGS.md`.
> 원자료(DBpia 답 전문, 페이지 캡처)는 `research/`(git 제외)에 두고, 도구와 정리본만 저장소에 둔다.
> DBpia 답은 저작권이 있는 서비스 출력이다. 저장소에는 요약·수치·인용 정보만 옮긴다.

## 1. 브라우저 연결

| 항목 | 값 |
|---|---|
| 브라우저 | Chrome 155, 프로필 `research/dbpia/chrome-profile` (DBpia 로그인 쿠키 보존) |
| 띄우기 | `scripts/research/open-dbpia-chrome.cmd` 를 **사용자가 더블클릭** (또는 작업 스케줄러 `/IT`) |
| 연결 | Chrome DevTools Protocol `http://127.0.0.1:9222` — puppeteer-core(`research/dbpia/node/node_modules`)와 REA MCP 가 같은 포트를 쓴다 |

주의: Claude Code 의 명령 창은 보이지 않는 데스크톱에서 돈다. 거기서 바로 띄운 Chrome 은 사용자 화면에 나타나지 않는다(2026-10-25 확인: `CopyFromScreen` 이 "The handle is invalid"). 사용자 화면에 띄우려면 사용자가 직접 실행하거나 `schtasks /Create … /IT` 후 `/Run` 한다. 쓰고 나면 작업을 지운다.

## 2. DBpia AI 에이전트

### 2.1 구조 (CDP 네트워크 기록으로 확인, `scripts/research/dbpia/capture_api.js`)

| 순서 | 요청 | 몸체 | 응답 |
|---|---|---|---|
| 1 | `POST https://agent.dbpia.co.kr/vetter_conversations/` | `{"title": 질문}` | `{"id", "title", "b2c_id", "vetter_version":"20250925", "created_at"}` |
| 2 | `POST https://agent.dbpia.co.kr/vetter_conversations/chat_message` | `{"message", "plct_types":["025001","025002","025015","025003","025005","025004","025000"], "is_criticized":false, "client_type":"web", "conversation_id", "referer":""}` | `text/event-stream` (SSE, 약 56 조각) |
| 3 | `GET https://agent.dbpia.co.kr/vetter_conversations/{id}` | — | `{"messages":[{role, content, hovers, references[], recommends[]}], "conversation_id", …context_size, "usage_ratio"}` |

- 인증: **쿠키만**(`credentials:"include"`). `Authorization` 머리글 없음. 응답 서버 `uvicorn`, CORS 는 `https://ai.dbpia.co.kr` 만 허용 → 반드시 로그인한 탭 **안에서** `fetch` 한다.
- `content` 의 인용은 `<CIT NODE01366552_0>` 꼴(논문 nodeId + 조각 번호). `references[]` 는 `node_id, node_title, node_autr, node_plct_nm, node_pbsh_yy, node_url, page, bbox, dmst_lstd_info(KCI 등재 여부)` 를 준다.
- `plct_types` 는 화면 「자료유형 전체」의 코드 목록(025001 학술저널, 025015 학위논문 등 — `references[].node_plct_type` 과 같은 체계).
- 이전 문서(`docs/dbpia_ai_browser_harvester.js`)의 `api-v3 …/ai-search/init-history` 경로는 이번 화면 흐름에서는 쓰이지 않았다. 에이전트 화면은 `agent.dbpia.co.kr` 만 쓴다.

### 2.2 빠르게 받는 법

화면에 글자를 치는 대신 위 세 요청을 로그인한 탭 안에서 직접 부른다(`scripts/research/dbpia/batch.js`).
동시에 3개씩 보내 질문당 33~89초, 13문항을 약 5분에 받았다(화면 입력은 1문항 약 2분 + 완료 판정 오류).
동시 수는 서비스 부담을 생각해 3으로 둔다.

```powershell
cd research\dbpia\tools   # 또는 scripts\research\dbpia
node batch.js ..\node\node_modules\puppeteer-core questions.json ..\harvest 3
node fetchconv.js ..\node\node_modules\puppeteer-core ..\harvest S01_hwangnyongsa=1583713   # 화면에서 물은 대화 저장
```

결과: `research/dbpia/harvest/<질문id>.json` (질문, 대화 id, 걸린 시간, SSE 앞부분, 대화 전문).

### 2.3 질문 작성 규칙

- 수치마다 「보고서·논문명, 기관, 연도, 쪽수」를 붙이고 「원문에 없는 수치는 쓰지 마라」를 넣는다.
- 질문 목록: `scripts/research/dbpia/questions.json` (유적 실측 S, 왕경 공간 W, 방법론 M).

### 2.4 답을 믿는 기준 (교차검증)

1. 수치 옆에 `<CIT …>` 가 **없으면** 근거 없는 값으로 본다(예: S01 의 서금당 25.4×15.2 m, 강당 48.9×22.3 m 는 인용 없음).
2. `<CIT>` 가 있어도 그 nodeId 의 논문이 2차 연구(학위논문·리뷰)면 원 보고서 쪽수를 따로 확인해야 B 등급이 된다.
3. 같은 값을 두 출처 이상이 말하면 「일치」, 하나뿐이면 「단일 출처」.
4. 화면의 「답변 검증하기」는 다음 질문에 `is_criticized:true` 로 붙는 모드로 보인다(눌러도 바로 요청이 나가지 않음). 미확인.

## 3. 국가유산 공간정보서비스 (gis-heritage.go.kr)

REA `inspect_web_page` (원자료 `research/rea/gis-heritage_inspect_web_page.json`) → 스크립트 목록 → `newHelpAPI.js` 정적 읽기.

| 기능 | 주소 | 비고 |
|---|---|---|
| WMS | `https://gis-heritage.go.kr/checkKey.do?domain=…&service=WMS&version=1.3.0&request=GetMap&LAYERS=…&crs=EPSG:…` | 지정구역·보호구역 그림. 도메인 키 확인 |
| 지정 목록(XML) | `https://gis-heritage.go.kr/openapi/xmlService/spca.do?ccbaKdcd=13` | 키 없이 응답. 사적 전국 목록: `ccbaMnm`(명칭), `ccceName`(시대), `ccmaName`(종목), `cnX/cnY`(중심점, EPSG:5179 값 범위), `crltsnoNm`(옛 지정번호), `vlocName` |
| 명칭 검색 | `ccbaMnm1=` 값은 화면 JS 가 `CryptoJS.AES.encrypt(이름, "secret-key-1")` 로 암호화해 보낸다 | 키가 스크립트에 공개돼 있음 |

- 2026-10-25 `ccbaKdcd=13`(사적) 응답 210 KB, 그중 소재지 「경상북도 경주시」 74건.
- **시대(`ccceName`)** 가 들어 있다 → 관계 표의 「시대」 열 자료원.
- 다각형(구역 경계)은 WMS 그림뿐이고 벡터 WFS 는 이 화면에서 찾지 못했다.

## 4. 국가유산 디지털 서비스 (digital.khs.go.kr)

REA `capture_browser_scenario`(연결 모드, 「천년 신라 왕경」 클릭) → `research/rea/khs-3d/silla_theme_click_capture.json`.

- 3D 에셋 서비스 `heritage/heriTage.do` → 테마 6개, 「천년 신라 왕경」 = `catalog.do?order=4&index=1`(클릭 경로로만 목록이 채워짐; curl 로는 기본 목록이 옴).
- 신라 왕경 에셋 약 **399개**(목록 `research/rea/khs-3d/silla_catalog_items.txt`): 대부분 **유물**(토기·귀걸이·기와·악기·불상), 일부 생활 소품(석등, 물시계 누각, 왕좌). **건물·유적 배치 규모 모델은 목록에서 보지 못했다.**
- 상세 `catalogDetail.do?order=4&index=&id=&type=&hrtg=&gubun=`, 미리보기 `record/imageViewObjectStorageMapFile3d.do?ichDataUid=…&type=T`, 웹 뷰어 `recommend/gltfViewer.do`(glTF).
- 받으려면 **「이용 신청하기」**(로그인) 를 거친다. 에셋마다 **공공누리 1~4유형**이 따로 붙는다.
- 유산 상세(`heri/heriDetail.do`)에는 연혁·정비 이력·「원형기록 통합DB 구축」(2021~2024) 이력이 있다.

## 5. 지금 엔진이 쓰는 외부 주소 (코드 기준)

| 자료 | 주소 | 메모 |
|---|---|---|
| 국가유산청 지정 목록 | `www.khs.go.kr/cha/SearchKindOpenapiList.do?ccbaCtcd=…` | 점 좌표 |
| 위성 | `mt1.google.com/vt/lyrs=s` (VWorld 키가 있으면 `api.vworld.kr/req/wmts`) | **Google 타일 직접 호출은 이용약관 문제**. 공개 발표·배포용으로는 VWorld 또는 사용자 정사영상으로 바꿔야 한다 |
| OSM | `overpass-api.de`, `overpass.kumi.systems` | |
| DEM | `copernicus-dem-30m.s3.amazonaws.com` (`/vsicurl/`) | |

## 6. REA 사용 기록

| 도구 | 대상 | 결과 |
|---|---|---|
| `list_browser_targets` | 9222 | DBpia 탭 1개 |
| `analyze_web_bundle` | DBpia 탭 | **MCP 연결 끊김**(큰 번들). 사용자가 재연결. 이후 CDP 직접 기록으로 대신함 |
| `inspect_web_page` | gis-heritage.go.kr | 565 KB Evidence, 스크립트 11개 |
| `capture_browser_scenario` | digital.khs.go.kr 3D 에셋 | 클릭 후 요청 목록 |

## 7. 사용자 지시에 따른 수집 규칙 (2026-10-25 추가)

1. **창은 하나.** 사용자가 띄운 Chrome 창의 DBpia 탭 하나만 쓴다. 조사용으로 열었던 탭(gis-heritage, digital.khs 2개)은 닫았다. 새 탭·새 창을 열지 않는다.
2. **에이전트 대화는 하나.** 질문마다 새 대화를 만들지 않고, 지금 열린 대화에 화면 입력으로 이어서 묻는다(`scripts/research/dbpia/followup.js`). 에이전트가 새 대화·다른 에이전트를 권하거나 대화 응답의 `usage_ratio` 가 90(%) 이상이면 멈추고 사용자에게 알린다.
3. 앞서 `batch.js` 로 만든 대화 15개(질문마다 새 대화, 3개 동시)는 이 규칙 이전의 기록이다. 이후로는 `batch.js` 를 쓰지 않는다.
4. **같은 대화에서는 API 직접 호출, 차례로**(`scripts/research/dbpia/apiseq.js`). 화면 입력(`followup.js`, 1문항 약 2분 + 완료 판정 대기) 대신 `chat_message` 를 같은 `conversation_id` 로 바로 부른다. 1문항 약 22초. 한 대화 안에서 동시에 보내면 문맥이 섞이므로 **동시 3개는 대화를 나눌 때만** 가능하다(규칙 2와 충돌 → 차례로). 답마다 창을 새로 고쳐 사용자가 같은 창에서 본다. 이미 보낸 질문(앞 도구가 멈추기 전에 보낸 것)은 다시 보내지 않는다.
5. 대화 1583715 사용량: 9 % → 56 % (질문 18개). 90 % 에서 멈춘다.
6. 대화 전환 기록: 1583715 에서 E08 뒤 사용량 **94 %** → 규칙대로 같은 탭에서 새 대화 **1583745** 를 열어 E09~E12 를 이어 물음(사용량 2 → 11 %). 질문 E01~E12 의 답 12개 모두 「참고문헌에 없는 인용」 0건.

## 8. 출처 나누기: 학술 사실과 3D 기술 (2026-10-25 추가, 사용자 지시)

사용자 지적: DBpia 수집에 10년 넘은 문헌이 섞이고, 3D 기술 정보는 최신이어야 한다. 확인 결과 지금까지 받은 답 49건의 인용 문헌(대화 전체 참고문헌 196편) 가운데 2016년 이전이 61편(31 %)이고, 3D 근거(G01~G08) 본문 인용은 가장 새것이 2024년이며 2025–2026년 문헌은 없다.

1. **DBpia 는 학술 사실에만 쓴다**: 발굴·조사 결과, 유구 배치, 방리·고분 구조, 국내 조사 관행·해석. 옛 원보고서도 1차 근거일 수 있으므로 버리지 않되, 인용마다 발행 연도를 붙이고(`digest.js` 가 `[nodeId, 연도]` 로 펼침) 뒤의 재조사·재해석이 있으면 그것을 우선한다.
2. **3D 기술은 DBpia 에 묻지 않는다**: 표현 방식, 자료 구조·규격, 알고리즘, 렌더링, 라이브러리. 2024–2026년 1차 출처(규격 원문, 공식 문서·출시 기록, 국제 학술지·학회, 사전공개본은 표시)만 쓰고, 출처마다 버전·발행일·확인한 날을 `TECH_2026.md` 에 적는다. 직접 열어 본 것만 적고 못 연 것은 「미확인」으로 둔다.
3. 법·기준(보호구역, 현상변경, 지정번호 표기 등)은 개정이 잦으므로 문헌이 아니라 현행 법령 원문으로 확인한다.
4. `digest.js` 셋째 인자로 기준 연도를 바꿀 수 있다(기본 2021). 기준 이전 문헌만 인용한 줄 수를 답마다 적는다.
