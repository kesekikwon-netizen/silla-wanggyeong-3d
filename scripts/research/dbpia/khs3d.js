const puppeteer = require(process.argv[2]);
(async () => { const b = await puppeteer.connect({ browserURL: 'http://127.0.0.1:9222', defaultViewport: null });
 const p = (await b.pages()).find(x => x.url().includes('heriTage.do'));
 const r = await p.evaluate(() => ({ text: document.body.innerText.slice(0, 3000), forms: [...document.querySelectorAll('form')].map(f => f.action + ' ' + [...f.elements].map(e => e.name).filter(Boolean).join(',')), links: [...document.querySelectorAll('a[href],[onclick]')].map(a => (a.innerText || '').trim().slice(0, 40) + ' -> ' + (a.getAttribute('href') || '') + ' ' + (a.getAttribute('onclick') || '')).slice(0, 80) }));
 console.log(r.text); console.log('--- forms'); r.forms.forEach(x => console.log(x)); console.log('--- links'); r.links.forEach(x => console.log(x)); b.disconnect(); })();
