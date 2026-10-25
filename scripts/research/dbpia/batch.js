// DBpia AI agent batch runner (direct API from the logged-in page; cookies only).
// usage: node batch.js <puppeteer-core path> <questions.json> <outDir> [concurrency=3] [only ids comma]
const fs = require('fs'), path = require('path');
const puppeteer = require(process.argv[2]);
const qs = JSON.parse(fs.readFileSync(process.argv[3], 'utf8'));
const outDir = process.argv[4]; const conc = +(process.argv[5] || 3);
const only = process.argv[6] ? process.argv[6].split(',') : null;
const PLCT = ["025001","025002","025015","025003","025005","025004","025000"];
async function askInPage(page, q, criticized) {
  return page.evaluate(async (q, PLCT, criticized) => {
    const base = 'https://agent.dbpia.co.kr/vetter_conversations';
    const H = { 'Content-Type': 'application/json', 'Cache-Control': 'no-cache' };
    const t0 = Date.now();
    const c = await (await fetch(base + '/', { method: 'POST', headers: H, credentials: 'include', body: JSON.stringify({ title: q }) })).json();
    const r = await fetch(base + '/chat_message', { method: 'POST', headers: H, credentials: 'include',
      body: JSON.stringify({ message: q, plct_types: PLCT, is_criticized: criticized, client_type: 'web', conversation_id: c.id, referer: '' }) });
    const reader = r.body.getReader(); const dec = new TextDecoder(); let sse = '';
    for (;;) { const { done, value } = await reader.read(); if (done) break; sse += dec.decode(value, { stream: true }); }
    const conv = await (await fetch(`${base}/${c.id}`, { credentials: 'include' })).json();
    return { conversationId: c.id, status: r.status, ms: Date.now() - t0, sseHead: sse.slice(0, 3000), sseBytes: sse.length, conv };
  }, q, PLCT, criticized);
}
(async () => {
  fs.mkdirSync(outDir, { recursive: true });
  const b = await puppeteer.connect({ browserURL: 'http://127.0.0.1:9222', defaultViewport: null });
  const page = (await b.pages()).find(x => x.url().includes('ai.dbpia'));
  const todo = qs.filter(x => !only || only.includes(x.id)).filter(x => !fs.existsSync(path.join(outDir, x.id + '.json')));
  let i = 0;
  async function worker() {
    while (i < todo.length) {
      const item = todo[i++];
      try {
        const res = await askInPage(page, item.q, !!item.criticized);
        fs.writeFileSync(path.join(outDir, item.id + '.json'), JSON.stringify({ ...item, askedAt: new Date().toISOString(), ...res }, null, 1));
        const a = (res.conv.messages || []).filter(m => m.role === 'assistant').pop();
        console.log('OK', item.id, res.conversationId, (res.ms / 1000).toFixed(0) + 's', 'chars', a ? a.content.length : 0, 'refs', a && a.references ? a.references.length : 0);
      } catch (e) { console.log('FAIL', item.id, e.message); }
    }
  }
  await Promise.all(Array.from({ length: conc }, worker));
  b.disconnect();
})().catch(e => { console.error('ERR', e.message); process.exit(1); });
