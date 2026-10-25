// usage: node fetchconv.js <pptr> <outDir> <id>=<convId> ...  (saves existing conversation from the agent API)
const fs = require('fs'), path = require('path');
const puppeteer = require(process.argv[2]); const outDir = process.argv[3];
const qs = JSON.parse(fs.readFileSync(path.join(__dirname, 'questions.json'), 'utf8'));
(async () => {
  const b = await puppeteer.connect({ browserURL: 'http://127.0.0.1:9222', defaultViewport: null });
  const page = (await b.pages()).find(x => x.url().includes('ai.dbpia'));
  for (const pair of process.argv.slice(4)) {
    const [id, cid] = pair.split('=');
    const conv = await page.evaluate(async cid => (await fetch(`https://agent.dbpia.co.kr/vetter_conversations/${cid}`, { credentials: 'include' })).json(), cid);
    const item = qs.find(x => x.id === id) || { id };
    fs.writeFileSync(path.join(outDir, id + '.json'), JSON.stringify({ ...item, askedAt: new Date().toISOString(), conversationId: +cid, via: 'ui', conv }, null, 1));
    const a = conv.messages.filter(m => m.role === 'assistant').pop();
    console.log('saved', id, cid, a ? a.content.length : 0, 'refs', a && a.references ? a.references.length : 0);
  }
  b.disconnect();
})();
