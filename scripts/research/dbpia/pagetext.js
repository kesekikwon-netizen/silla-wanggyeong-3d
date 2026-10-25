const puppeteer = require(process.argv[2]);
(async () => { const b = await puppeteer.connect({ browserURL: 'http://127.0.0.1:9222', defaultViewport: null });
 const p = (await b.pages()).find(x => x.url().includes(process.argv[3]));
 const r = await p.evaluate(() => ({ text: document.body.innerText.slice(0, 4000), links: [...document.querySelectorAll('a[href]')].map(a => a.innerText.trim() + ' -> ' + a.getAttribute('href')).filter(s => /3D|3d|입체|다운|download|obj|ply|glb|스캔/i.test(s)).slice(0, 40) }));
 console.log(r.text); console.log('--- 3D/download links'); r.links.forEach(l => console.log(l)); b.disconnect(); })();
