# Antigravity Desktop (Antigravity 2.0 / IDE) 이어하기 핸드오프 가이드

> **이전 대화 ID**: `ae3e59ec-27e3-4036-ae5e-c44d4ada6046`  
> **브레인/아티팩트 경로**: `C:\Users\권을\.gemini\antigravity-cli\brain\ae3e59ec-27e3-4036-ae5e-c44d4ada6046`  
> **3D 엔진 저장소**: `C:\Users\권을\Documents\hgis-3d-engine`  
> **Strata 본체 저장소**: `C:\Users\권을\Documents\hgis`

---

## 1. Antigravity 데스크탑 버전 설치 후 바로 이어가는 방법

Antigravity 데스크탑 버전(**Antigravity 2.0** 또는 **Antigravity IDE**)은 현재 CLI와 동일한 로컬 설정 디렉토리(`C:\Users\권을\.gemini\antigravity-cli`)를 공유하므로, 기존 대화 기록·스킬·MCP(`rea`, `claude-code`)가 그대로 유지됩니다.

### Step 1: 워크스페이스 열기 및 이전 대화 불러오기
1. Antigravity 데스크탑 앱을 실행하고 프로젝트 폴더로 `C:\Users\권을\Documents\hgis` 또는 `C:\Users\권을\Documents\hgis-3d-engine`을 엽니다.
2. 좌측 사이드바에서 기존 대화(`ae3e59ec-27e3-4036-ae5e-c44d4ada6046`)를 바로 클릭해 이어가거나, 새 대화창에서 **`/browser`** 슬래시 명령어와 함께 아래 프롬프트를 그대로 붙여넣으시면 됩니다:

```text
/browser
C:\Users\권을\Documents\hgis-3d-engine\ANTIGRAVITY_DESKTOP_HANDOFF.md 와 SILLA_BANGRI_3D_PLAN.md 를 읽고 작업을 이어가라.
내장 브라우저로 https://ai.dbpia.co.kr/ko/search 에 접속해(로그인 상태 확인 또는 내가 로그인한 뒤) docs/dbpia_ai_browser_harvester.js 를 실행하거나 검색창/AI 에이전트/딥리서치를 직접 구동해서 신라왕경 및 9주 5소경 방리구획의 논문 본문 상세 수치를 추가로 뽑아내고, SILLA_BANGRI_3D_PLAN.md 를 보강한 뒤 hgis-3d-engine 에 6대 모듈과 세밀한 도면 출력기까지 구현하라.
```

---

## 2. 지금까지 완료되어 저장된 자산 목록 (100% 보존됨)

1. **`https://ai.dbpia.co.kr/ko/search` REA 역공학 결과 및 번들 (33개 JS, 218개 모듈)**
   - 경로: `C:\Users\권을\.gemini\antigravity-cli\brain\ae3e59ec-27e3-4036-ae5e-c44d4ada6046\scratch\ai_dbpia_scripts\files`
   - 핵심 모듈:
     - **Module `31684` (`source-23-*.js`)**: `https://api-v3.dbpia.co.kr/ai-search` (`/init-history` → `/extract-keywords` → `/search-articles` → `/generate-answer` → `/generate-answer-extend` → `/ai-search-history/{id}`)
     - **Module `1577` (`5196-*.js`)**: `https://agent.dbpia.co.kr/vetter_conversations` (`/`, `/chat_message`, `/critic/`)
     - **Module `96309` (`5196-*.js`)**: `https://drs-api.dbpia.co.kr/deepresearch_conversations` (`/`, `/chat_message`, `/stream`, `/approve`)
     - **Module `94945` (`5399-*.js`)**: `fetch` 래퍼 — `credentials: "include"`(HttpOnly 로그인 쿠키 포함) + `sessionStorage.getItem("dbp_pxy")` 또는 `floatingSession.sessionId`를 `Authorization` 헤더에 주입.
2. **DBpia 실제 학술논문 200편 원장 (신라왕경 방리 + 9주 5소경)**
   - 경로: `C:\Users\권을\Documents\hgis-3d-engine\docs\dbpia_silla_bangri_9ju5sogyeong_ledger.json`
   - 25개 학술 질의로 수집된 고유 논문 200편(`nodeId`, 제목, 저자, 학술지, 연도, 초록, URL) 수록.
3. **내장 브라우저용 DBpia AI 본문 심층 추출 스크립트**
   - 경로: `C:\Users\권을\Documents\hgis-3d-engine\docs\dbpia_ai_browser_harvester.js`
   - Antigravity 데스크탑의 내장 브라우저(`/browser`)에서 `https://ai.dbpia.co.kr/ko/search` 로그인 후 실행하면, `HttpOnly` 쿠키와 `dbp_pxy` 토큰을 자동으로 실어 `api-v3.dbpia.co.kr/ai-search`의 본문 기반 상세 답변(`generate-answer`, `generate-answer-extend`)을 JSON으로 일괄 추출합니다.
4. **신라왕경 & 9주 5소경 3D 방리구획 복원 및 세밀한 도면 제작 마스터 지시서**
   - 경로: `C:\Users\권을\Documents\hgis-3d-engine\SILLA_BANGRI_3D_PLAN.md`
   - 12개 도시/권역 프리셋(신라왕경 3권역 + 5소경[남원경·서원경·중원경·금관경·북원경] + 9주 치소[상주 사벌주·진주 청주·광주 무진주·하남 한주]), 6대 핵심 C++20/OpenGL 모듈(Bare-Earth 고지형 + 3D 고분 봉분 융기, 발굴 Anchor 기반 방리 격자 엔진 + Auto-Fit RMSE, 현대 도로 화석화 일치율, 지형/구하도/고분 간섭 마스킹, 최적 직교 시굴 트렌치 배치, 5패널 정밀 고고학 도면 `SheetComposer`) 상세 명세 수록.
