// CDP Network capture of one UI-driven question: full request bodies + response bodies for agent/api dbpia hosts
const fs = require('fs');
const puppeteer = require(process.argv[2]);
const out = process.argv[3], question = process.argv[4];
const sleep = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await puppeteer.connect({ browserURL: 'http://127.0.0.1:9222', defaultViewport: null });
  const p = (await b.pages()).find(x => x.url().includes('ai.dbpia'));
  const cdp = await p.createCDPSession();
  await cdp.send('Network.enable', { maxPostDataSize: 65536 });
  const tx = {};
  const want = u => /(agent|api-v\d|drs-api|ai-reader-api)\.dbpia\.co\.kr/.test(u);
  cdp.on('Network.requestWillBeSent', e => { if (want(e.request.url)) tx[e.requestId] = { url: e.request.url, method: e.request.method, reqHeaders: Object.keys(e.request.headers), postData: e.request.postData || null }; });
  cdp.on('Network.responseReceived', e => { if (tx[e.requestId]) Object.assign(tx[e.requestId], { status: e.response.status, mime: e.response.mimeType, resHeaders: e.response.headers }); });
  cdp.on('Network.dataReceived', e => { if (tx[e.requestId]) tx[e.requestId].chunks = (tx[e.requestId].chunks || 0) + 1; });
  cdp.on('Network.loadingFinished', async e => { const t = tx[e.requestId]; if (!t) return; try { const r = await cdp.send('Network.getResponseBody', { requestId: e.requestId }); t.body = r.base64Encoded ? '[base64]' : r.body.slice(0, 200000); } catch (err) { t.bodyErr = err.message; } });
  await p.goto('https://ai.dbpia.co.kr/ko/search', { waitUntil: 'networkidle2' });
  const box = await p.waitForSelector('textarea', { timeout: 20000 });
  await box.click(); await p.keyboard.type(question, { delay: 3 }); await sleep(300); await p.keyboard.press('Enter');
  await sleep(8000);
  for (let i = 0; i < 120; i++) { const busy = await p.evaluate(() => /답변을 생성하고 있습니다|검색중\.\.\.|참고할 논문을 확인/.test(document.body.innerText)); if (!busy) break; await sleep(5000); }
  await sleep(3000);
  fs.writeFileSync(out, JSON.stringify({ question, url: p.url(), tx: Object.values(tx) }, null, 1));
  for (const t of Object.values(tx)) console.log(t.method, t.status, t.mime, t.url, 'post', (t.postData || '').length, 'body', (t.body || t.bodyErr || '').length, 'chunks', t.chunks || 0);
  b.disconnect();
})().catch(e => { console.error('ERR', e.message); process.exit(1); });
