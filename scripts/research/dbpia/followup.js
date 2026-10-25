// 한 창·한 대화에서 이어 묻기 (사용자 지시 2026-10-25: 창은 하나, 에이전트가 새 대화를 권할 때까지 같은 대화에만 묻는다).
// 이미 열린 DBpia 탭(새 탭을 만들지 않음)의 입력칸에 질문을 쳐서 보내고, 같은 대화의 답이 끝날 때까지 기다린 뒤 대화 전문을 저장한다.
// usage: node followup.js <puppeteer-core path> <questions.json> <outDir> <id1,id2,...>
const fs = require('fs'), path = require('path');
const puppeteer = require(process.argv[2]);
const qs = JSON.parse(fs.readFileSync(process.argv[3], 'utf8'));
const outDir = process.argv[4];
const ids = process.argv[5].split(',');
const sleep = ms => new Promise(r => setTimeout(r, ms));
const NEW_CHAT_HINT = /새\s*대화|새로운\s*대화|대화를\s*새로|다른\s*에이전트|새\s*에이전트|대화.*(한도|용량).*(초과|가득)/;
(async () => {
  fs.mkdirSync(outDir, { recursive: true });
  const b = await puppeteer.connect({ browserURL: 'http://127.0.0.1:9222', defaultViewport: null });
  const pages = (await b.pages()).filter(x => x.url().includes('ai.dbpia.co.kr'));
  if (pages.length !== 1) throw new Error('DBpia 탭이 정확히 하나여야 합니다: ' + pages.length);
  const page = pages[0];
  const m = page.url().match(/\/search\/(\d+)/);
  if (!m) throw new Error('열린 대화가 없습니다: ' + page.url());
  const convId = m[1];
  const getConv = () => page.evaluate(async id => (await fetch(`https://agent.dbpia.co.kr/vetter_conversations/${id}`, { credentials: 'include' })).json(), convId);
  for (const id of ids) {
    const item = qs.find(x => x.id === id);
    if (!item) throw new Error('질문 없음: ' + id);
    const before = await getConv();
    const nBefore = before.messages.length;
    const box = await page.waitForSelector('textarea', { timeout: 20000 });
    await box.click();
    await page.keyboard.type(item.q, { delay: 3 });
    await sleep(300);
    await page.keyboard.press('Enter');
    const t0 = Date.now();
    let conv = before;
    for (let i = 0; i < 120; i++) {
      await sleep(5000);
      const busy = await page.evaluate(() => /답변을 생성하고 있습니다|검색중\.\.\.|참고할 논문을 확인/.test(document.body.innerText));
      conv = await getConv();
      const last = conv.messages[conv.messages.length - 1];
      if (!busy && conv.messages.length >= nBefore + 2 && last.role === 'assistant' && last.content) break;
    }
    const a = conv.messages[conv.messages.length - 1];
    fs.writeFileSync(path.join(outDir, id + '.json'), JSON.stringify({ ...item, askedAt: new Date().toISOString(), conversationId: +convId, via: 'ui-followup', ms: Date.now() - t0, conv }, null, 1));
    const ratio = conv.usage_ratio;
    console.log('OK', id, 'conv', convId, ((Date.now() - t0) / 1000).toFixed(0) + 's', 'chars', a.content.length, 'refs', (a.references || []).length, 'usage_ratio', ratio);
    if (NEW_CHAT_HINT.test(a.content) || (typeof ratio === 'number' && ratio >= 90)) {
      console.log('STOP: 에이전트가 새 대화를 권하거나 대화 용량이 찼습니다. 사용자에게 알리고 멈춥니다.');
      break;
    }
  }
  b.disconnect();
})().catch(e => { console.error('ERR', e.message); process.exit(1); });
