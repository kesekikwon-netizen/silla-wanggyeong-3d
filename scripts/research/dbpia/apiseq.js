// 한 창·한 대화에서 이어 묻기, API 직접 호출판(화면 입력 없음). 2026-10-25.
// 같은 대화(conversation_id)에는 질문을 차례로만 보낸다(동시에 보내면 문맥이 섞인다).
// 답이 오면 창을 새로 고쳐 사용자가 같은 창에서 볼 수 있게 한다. 새 탭을 만들지 않는다.
// usage: node apiseq.js <puppeteer-core path> <questions.json> <outDir> <id1,id2,...>
const fs = require('fs'), path = require('path');
const puppeteer = require(process.argv[2]);
const qs = JSON.parse(fs.readFileSync(process.argv[3], 'utf8'));
const outDir = process.argv[4];
const ids = process.argv[5].split(',');
const PLCT = ['025001', '025002', '025015', '025003', '025005', '025004', '025000'];
const NEW_CHAT_HINT = /새\s*대화|새로운\s*대화|대화를\s*새로|다른\s*에이전트|새\s*에이전트|대화.*(한도|용량).*(초과|가득)/;
const sleep = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await puppeteer.connect({ browserURL: 'http://127.0.0.1:9222', defaultViewport: null });
  const pages = (await b.pages()).filter(x => x.url().includes('ai.dbpia.co.kr'));
  if (pages.length !== 1) throw new Error('DBpia 탭이 정확히 하나여야 합니다: ' + pages.length);
  const page = pages[0];
  let convId = +(page.url().match(/\/search\/(\d+)/) || [])[1];
  if (!convId) throw new Error('열린 대화가 없습니다: ' + page.url());
  const getConv = () => page.evaluate(async id => (await fetch(`https://agent.dbpia.co.kr/vetter_conversations/${id}`, { credentials: 'include' })).json(), convId);
  const ask = q => page.evaluate(async (q, id, PLCT) => {
    const r = await fetch('https://agent.dbpia.co.kr/vetter_conversations/chat_message', {
      method: 'POST', credentials: 'include', headers: { 'Content-Type': 'application/json', 'Cache-Control': 'no-cache' },
      body: JSON.stringify({ message: q, plct_types: PLCT, is_criticized: false, client_type: 'web', conversation_id: id, referer: '' }) });
    const rd = r.body.getReader(); let n = 0;
    for (;;) { const { done, value } = await rd.read(); if (done) break; n += value.length; }
    return { status: r.status, bytes: n };
  }, q, convId, PLCT);
  const save = (item, conv, ms, via) => fs.writeFileSync(path.join(outDir, item.id + '.json'),
    JSON.stringify({ ...item, askedAt: new Date().toISOString(), conversationId: convId, via, ms, conv }, null, 1));
  // 같은 탭에서 새 대화로 넘어간다(에이전트가 새 대화를 권하거나 사용량 90 % 이상일 때만)
  const newConversation = async title => {
    const c = await page.evaluate(async t => (await (await fetch('https://agent.dbpia.co.kr/vetter_conversations/', {
      method: 'POST', credentials: 'include', headers: { 'Content-Type': 'application/json', 'Cache-Control': 'no-cache' },
      body: JSON.stringify({ title: t }) })).json()), title);
    convId = c.id;
    await page.goto('https://ai.dbpia.co.kr/ko/search/' + convId, { waitUntil: 'networkidle2' }).catch(() => {});
    console.log('NEW CONVERSATION', convId);
  };
  let switchNext = false;
  for (const id of ids) {
    const item = qs.find(x => x.id === id);
    if (!item) throw new Error('질문 없음: ' + id);
    if (switchNext) { await newConversation(item.q); switchNext = false; }
    let conv = await getConv();
    const lastUser = [...conv.messages].reverse().find(m => m.role === 'user');
    const t0 = Date.now();
    if (lastUser && lastUser.content.trim() === item.q.trim()) {
      // 이미 보낸 질문(이전 도구가 멈추기 전에 보냄): 다시 보내지 않고 답만 기다린다
      for (let i = 0; i < 60; i++) {
        const last = conv.messages[conv.messages.length - 1];
        if (last.role === 'assistant' && last.content) break;
        await sleep(5000);
        conv = await getConv();
      }
      save(item, conv, Date.now() - t0, 'already-sent');
    } else {
      const r = await ask(item.q);
      if (r.status !== 200) throw new Error(`${id}: HTTP ${r.status}`);
      conv = await getConv();
      save(item, conv, Date.now() - t0, 'api-sequential');
    }
    const a = conv.messages[conv.messages.length - 1];
    await page.reload({ waitUntil: 'networkidle2' }).catch(() => {});  // 같은 창에서 보이게
    console.log('OK', id, 'conv', convId, ((Date.now() - t0) / 1000).toFixed(0) + 's', 'chars', (a.content || '').length, 'refs', (a.references || []).length, 'usage_ratio', conv.usage_ratio);
    if (NEW_CHAT_HINT.test(a.content || '') || (typeof conv.usage_ratio === 'number' && conv.usage_ratio >= 90)) {
      console.log('SWITCH: 에이전트가 새 대화를 권하거나 사용량이 90 % 이상 → 다음 질문부터 같은 탭의 새 대화로');
      switchNext = true;
    }
  }
  b.disconnect();
})().catch(e => { console.error('ERR', e.message); process.exit(1); });
