/**
 * DBpia AI (https://ai.dbpia.co.kr/ko/search) In-Browser Full-Text & Agent Synthesis Harvester
 * --------------------------------------------------------------------------------------------
 * Reverse-engineered via REA MCP (Modules 31684, 54330, 94945, 1577, 96309).
 * Runs inside the logged-in https://ai.dbpia.co.kr browser context (yuseong 구독 중).
 * Automatically includes HttpOnly cookies (credentials: "include") and sessionStorage["dbp_pxy"].
 *
 * Harvests:
 * 1. Existing AI Agent (Vetter) conversations & full paper-backed answers (/vetter_conversations)
 * 2. Existing AI DeepResearch conversations & reports (/deepresearch_conversations)
 * 3. Live AI Search full-text synthesis (/init-history -> /extract-keywords -> /search-articles -> /generate-answer -> /generate-answer-extend)
 * 4. Automatically downloads `dbpia_ai_harvest_live.json` to the Downloads folder.
 */
(async function runDbpiaAiFullHarvester() {
  const BASE_AI_SEARCH = "https://api-v3.dbpia.co.kr/ai-search";
  const BASE_AGENT = "https://agent.dbpia.co.kr";
  const BASE_DRS = "https://drs-api.dbpia.co.kr";

  function buildHeaders() {
    const h = {
      "Content-Type": "application/json",
      "Cache-Control": "no-cache"
    };
    const pxy = sessionStorage.getItem("dbp_pxy");
    let flt = undefined;
    try {
      flt = JSON.parse(sessionStorage.getItem("floatingSession") || "{}")?.sessionId;
    } catch (_) {}
    if (pxy) h["Authorization"] = pxy;
    if (flt) h["Authorization"] = flt;
    return h;
  }

  async function apiCall(baseUrl, path, method = "POST", bodyObj = undefined) {
    const res = await fetch(baseUrl + path, {
      method,
      headers: buildHeaders(),
      credentials: "include",
      body: bodyObj !== undefined ? JSON.stringify(bodyObj) : undefined
    });
    if (!res.ok) {
      const errText = await res.text().catch(() => "");
      throw new Error(`HTTP ${res.status} on ${baseUrl}${path}: ${errText}`);
    }
    return await res.json();
  }

  const harvestPayload = {
    harvestedAt: new Date().toISOString(),
    locationHref: location.href,
    vetterRecent: [],
    vetterDetails: [],
    deepResearchRecent: [],
    deepResearchDetails: [],
    aiSearchSynthesis: []
  };

  try {
    document.title = "[HARVESTING 1/3] AI Agent Vetter Conversations...";
    const vRec = await apiCall(BASE_AGENT, "/vetter_conversations/recent-conversations", "GET").catch(e => ({ error: String(e) }));
    const vList = Array.isArray(vRec?.data) ? vRec.data : (Array.isArray(vRec) ? vRec : []);
    harvestPayload.vetterRecent = vList;

    const sillaKeywords = ["신라", "왕경", "방리", "경주", "성동동", "황오동", "동천동", "황남동", "구황동", "인왕동", "문화재", "발굴", "소경", "고분", "도로"];
    const targetVetter = vList.filter(it => {
      const t = String(it?.title || it?.name || "");
      return sillaKeywords.some(k => t.includes(k));
    }).slice(0, 20);

    for (let i = 0; i < targetVetter.length; i++) {
      const cid = targetVetter[i]?.id || targetVetter[i]?.conversation_id || targetVetter[i]?.conversationId;
      if (!cid) continue;
      document.title = `[HARVESTING 1/3] Vetter (${i + 1}/${targetVetter.length})...`;
      const dtl = await apiCall(BASE_AGENT, `/vetter_conversations/${cid}`, "GET").catch(e => ({ error: String(e) }));
      harvestPayload.vetterDetails.push({
        id: cid,
        title: targetVetter[i]?.title,
        detail: dtl?.data ?? dtl
      });
    }
  } catch (e) {
    harvestPayload.vetterError = String(e);
  }

  try {
    document.title = "[HARVESTING 2/3] DeepResearch Conversations...";
    const dRec = await apiCall(BASE_DRS, "/deepresearch_conversations/recent-conversations", "GET").catch(e => ({ error: String(e) }));
    const dList = Array.isArray(dRec?.data) ? dRec.data : (Array.isArray(dRec) ? dRec : []);
    harvestPayload.deepResearchRecent = dList;
    for (let i = 0; i < Math.min(dList.length, 10); i++) {
      const cid = dList[i]?.id || dList[i]?.conversation_id || dList[i]?.conversationId;
      if (!cid) continue;
      const dtl = await apiCall(BASE_DRS, `/deepresearch_conversations/${cid}`, "GET").catch(e => ({ error: String(e) }));
      harvestPayload.deepResearchDetails.push({
        id: cid,
        title: dList[i]?.title,
        detail: dtl?.data ?? dtl
      });
    }
  } catch (e) {
    harvestPayload.deepResearchError = String(e);
  }

  const deepQuestions = [
    "신라 왕경 황룡사 동편·성동동·황오동·동천동 방리구획의 1방(坊) 실측 크기(동서×남북 m), 대로·중로·소로 폭(m), 양측 측구(배수로) 폭과 깊이, 고구려척·당척 적용 수치를 논문 본문 근거로 상세히 정리해줘.",
    "통일신라 5소경(금관경 김해, 남원경 남원, 중원경 충주 탑평리·충주읍성, 서원경 청주, 북원경 원주)과 9주 치소(상주 복룡동 사벌주, 진주 무촌유적 청주, 광주 무진주, 하남 한주)에서 발굴되거나 복원된 방리구획 규격(m), 도로 폭, 측구, 방위각 편차를 각각 상세히 비교해줘.",
    "경주 북천(알천) 구하도와 홍수 치수 대책, 그리고 대릉원·쪽샘 등 대형 적석목곽분 고분군 배치가 신라 왕경 방리 도로망의 단절·우회·확장 과정에 미친 영향을 논문 본문 수치와 유구 사례로 설명해줘."
  ];

  for (let idx = 0; idx < deepQuestions.length; idx++) {
    const q = deepQuestions[idx];
    document.title = `[HARVESTING 3/3] AI Search Query (${idx + 1}/${deepQuestions.length})...`;
    try {
      const initRes = await apiCall(BASE_AI_SEARCH, "/init-history", "POST", { query: q });
      const historyId = initRes?.data?.historyId ?? initRes?.historyId ?? initRes?.data?.id ?? initRes?.id;

      const kwRes = await apiCall(BASE_AI_SEARCH, "/extract-keywords", "POST", { historyId }).catch(() => null);
      const artRes = await apiCall(BASE_AI_SEARCH, "/search-articles", "POST", { historyId, titleOnly: "true" }).catch(() => null);
      const ansRes = await apiCall(BASE_AI_SEARCH, "/generate-answer", "POST", { historyId }).catch(() => null);
      const extRes = await apiCall(BASE_AI_SEARCH, "/generate-answer-extend", "POST", { historyId }).catch(() => null);
      const dtlRes = await apiCall(BASE_AI_SEARCH, `/ai-search-history/${historyId}`, "GET").catch(() => null);

      harvestPayload.aiSearchSynthesis.push({
        query: q,
        historyId,
        keywords: kwRes?.data ?? kwRes,
        articles: artRes?.data ?? artRes,
        answer: ansRes?.data ?? ansRes,
        answerExtend: extRes?.data ?? extRes,
        detail: dtlRes?.data ?? dtlRes
      });
    } catch (err) {
      harvestPayload.aiSearchSynthesis.push({ query: q, error: String(err) });
    }
  }

  window.__DBPIA_AI_HARVEST_RESULT__ = harvestPayload;
  const jsonText = JSON.stringify(harvestPayload, null, 2);

  try {
    const blob = new Blob([jsonText], { type: "application/json" });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = "dbpia_ai_harvest_live.json";
    document.body.appendChild(a);
    a.click();
    setTimeout(() => a.remove(), 2000);
  } catch (_) {}

  document.title = "[HARVEST_DONE] DBpia AI";
  return jsonText;
})();
