// usage: node reatext.js <rea-result.json> <regex> [ctxChars=220]  — REA 캡처의 접근성 텍스트에서 정규식 주변을 뽑는다
const fs = require('fs');
const [file, re, ctxArg] = process.argv.slice(2);
const ctx = Number(ctxArg || 220);
const j = JSON.parse(fs.readFileSync(file, 'utf8'));
const out = [];
(function walk(o, k) {
  if (typeof o === 'string') { if (/^(name|value|text|description)$/.test(k) && o.trim()) out.push(o.trim()); return; }
  if (Array.isArray(o)) return o.forEach(x => walk(x, k));
  if (o && typeof o === 'object') for (const [kk, v] of Object.entries(o)) walk(v, kk);
})(j, '');
const seen = new Set(); const text = out.filter(s => !seen.has(s) && seen.add(s)).join(' | ');
const steps = (j.normalized_result || {}).steps || [];
console.log('url:', JSON.stringify(steps.map(s => s.url || (s.captures||{}).url)).slice(0, 300), 'textLen:', text.length);
const rx = new RegExp(re, 'gi'); let m, n = 0;
while ((m = rx.exec(text)) && n < 12) { n++; console.log('---', text.slice(Math.max(0, m.index - ctx), m.index + ctx).replace(/\s+/g, ' ')); }
if (!n) console.log('no match');
