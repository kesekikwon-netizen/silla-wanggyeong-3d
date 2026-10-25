const puppeteer = require(process.argv[2]);
(async () => {
  const b = await puppeteer.connect({ browserURL: 'http://127.0.0.1:9222', defaultViewport: null });
  const p = (await b.pages()).find(x => x.url().includes('ai.dbpia'));
  await p.screenshot({ path: process.argv[3] });
  const txt = await p.evaluate(() => document.body.innerText.slice(0, 1500));
  console.log(txt);
  b.disconnect();
})().catch(e => { console.error(e.message); process.exit(1); });
