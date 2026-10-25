// usage: node opentab.js <pptr> <url> [waitMs]  -> opens a new tab, prints CDP target id
const puppeteer = require(process.argv[2]);
(async () => {
  const b = await puppeteer.connect({ browserURL: 'http://127.0.0.1:9222', defaultViewport: null });
  const p = await b.newPage();
  await p.goto(process.argv[3], { waitUntil: 'networkidle2', timeout: 60000 }).catch(e => console.log('goto', e.message));
  await new Promise(r => setTimeout(r, +(process.argv[4] || 2000)));
  console.log(p.target()._targetId, p.url(), await p.title());
  b.disconnect();
})();
