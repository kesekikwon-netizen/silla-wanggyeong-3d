// usage: node ask.js <puppeteer-core path> <out.json> <question>
const fs = require('fs');
const puppeteer = require(process.argv[2]);
const out = process.argv[3];
const question = process.argv[4];
const sleep = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await puppeteer.connect({ browserURL: 'http://127.0.0.1:9222', defaultViewport: null });
  const p = (await b.pages()).find(x => x.url().includes('ai.dbpia'));
  const net = [];
  const onReq = r => { const u = r.url(); if (/dbpia\.co\.kr/.test(u) && !/\.(js|css|png|svg|woff2?|ico|jpg)(\?|$)/.test(u) && r.resourceType() !== 'image') net.push({ t: Date.now(), method: r.method(), url: u, type: r.resourceType(), body: (r.postData() || '').slice(0, 600) }); };
  p.on('request', onReq);
  await p.goto('https://ai.dbpia.co.kr/ko/search', { waitUntil: 'networkidle2' });
  const box = await p.waitForSelector('textarea', { timeout: 20000 });
  await box.click();
  await p.keyboard.type(question, { delay: 5 });
  await sleep(300);
  await p.keyboard.press('Enter');
  // wait until answer text stops growing
  let last = -1, stable = 0, txt = '';
  for (let i = 0; i < 240; i++) {
    await sleep(2500);
    txt = await p.evaluate(() => (document.querySelector('main') || document.body).innerText);
    if (txt.length === last) { if (++stable >= 4 && i > 6) break; } else { stable = 0; last = txt.length; }
  }
  const links = await p.evaluate(() => [...document.querySelectorAll('main a[href]')].map(a => ({ text: a.innerText.trim().slice(0, 200), href: a.href })).filter(x => /node|article|dbpia/.test(x.href)));
  p.off('request', onReq);
  fs.writeFileSync(out, JSON.stringify({ askedAt: new Date().toISOString(), question, pageUrl: p.url(), answerText: txt, links, network: net }, null, 1));
  console.log('url', p.url(), 'chars', txt.length, 'links', links.length, 'requests', net.length);
  b.disconnect();
})().catch(e => { console.error('ERR', e.message); process.exit(1); });
