const fs = require('fs'); const puppeteer = require(process.argv[2]);
(async () => { const b = await puppeteer.connect({ browserURL: 'http://127.0.0.1:9222', defaultViewport: null });
 const p = (await b.pages()).find(x => x.url().includes('catalog.do'));
 console.log('URL', p.url());
 const r = await p.evaluate(() => { const t = document.body.innerText; const i = t.indexOf('천년 신라'); return { head: t.slice(t.indexOf('제작년도') - 50, t.indexOf('제작년도') + 1500), items: [...document.querySelectorAll('[onclick]')].map(e => (e.innerText || '').trim().replace(/\s+/g, ' ').slice(0, 60) + ' | ' + e.getAttribute('onclick')).filter(s => /3d|Detail|detail|view|View|down/i.test(s)).slice(0, 400) }; });
 console.log(r.head); console.log('--- clickable'); r.items.forEach(x => console.log(x));
 fs.writeFileSync('../../rea/khs-3d/silla_catalog_items.txt', r.items.join('\n'));
 b.disconnect(); })();
