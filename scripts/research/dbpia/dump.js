// usage: node dump.js <pptr> <convId> <out.json>
const fs = require('fs');
const puppeteer = require(process.argv[2]);
const id = process.argv[3], out = process.argv[4];
const sleep = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await puppeteer.connect({ browserURL: 'http://127.0.0.1:9222', defaultViewport: null });
  const p = (await b.pages()).find(x => x.url().includes('ai.dbpia'));
  for (let i = 0; i < 120; i++) {
    const busy = await p.evaluate(() => /답변을 생성하고 있습니다|검색중\.\.\.|참고할 논문을 확인/.test(document.body.innerText));
    if (!busy) break;
    await sleep(5000);
  }
  const res = await p.evaluate(async (id) => {
    const tryGet = async (u) => { try { const r = await fetch(u, { credentials: 'include' }); return { u, status: r.status, body: (await r.text()).slice(0, 400000) }; } catch (e) { return { u, err: String(e) }; } };
    return [await tryGet(`https://agent.dbpia.co.kr/vetter_conversations/${id}`), await tryGet(`https://agent.dbpia.co.kr/vetter_conversations/${id}/messages`)];
  }, id);
  const pageText = await p.evaluate(() => (document.querySelector('main') || document.body).innerText);
  fs.writeFileSync(out, JSON.stringify({ id, pageText, api: res }, null, 1));
  for (const r of res) console.log(r.u, r.status, (r.body || r.err || '').slice(0, 300));
  console.log('pageText chars', pageText.length);
  b.disconnect();
})().catch(e => { console.error('ERR', e.message); process.exit(1); });
