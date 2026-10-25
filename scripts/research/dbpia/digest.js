// usage: node digest.js <harvestDir> <out.md> [cutoffYear=2021]  — 답 전문을 인용 표시 [nodeId, 연도] 와 함께 펼치고,
// 인용 없는 수치 줄과 cutoffYear 이전 문헌만 인용한 줄을 표시한다(3D 기술 근거는 최신 1차 출처로 다시 확인해야 하므로).
const fs = require('fs'), path = require('path');
const dir = process.argv[2], out = process.argv[3];
const cutoff = Number(process.argv[4] || 2021);
const files = fs.readdirSync(dir).filter(f => /^[SWMFDEG]\d\d_.*\.json$/.test(f)).sort();
let md = '# DBpia 답 펼침본 (자동 생성, 원자료 research/dbpia/harvest)\n\n';
const allRefs = {};
for (const f of files) {
  const j = JSON.parse(fs.readFileSync(path.join(dir, f), 'utf8'));
  const a = (j.conv.messages || []).filter(m => m.role === 'assistant').pop();
  if (!a) continue;
  const refs = a.references || [];
  refs.forEach(r => { allRefs[r.node_id] = r; });
  const refById = new Map((j.conv.messages || []).flatMap(m => (m.references || []).map(r => [r.node_id, r])));
  const yearOf = id => (refById.get(id) || {}).node_pbsh_yy;
  const body = a.content.replace(/<CIT ([A-Z0-9]+)_(\w+)>/g, (_, id) => `[${id}, ${yearOf(id) || '연도 미상'}]`);
  const numLines = body.split('\n').filter(l => /\d+(\.\d+)?\s*(m|㎡|cm|척|°|km)/.test(l));
  const uncited = numLines.filter(l => !/\[[A-Z0-9]+, /.test(l));
  // 인용이 있는 줄 가운데 인용 문헌이 모두 cutoff 이전인 줄
  const oldOnly = body.split('\n').filter(l => {
    const ys = [...l.matchAll(/\[[A-Z0-9]+, (\d{4})\]/g)].map(m => +m[1]);
    return ys.length && ys.every(y => y < cutoff);
  });
  // 본문 인용 nodeId 가 references[] 에 실제로 있는지(없으면 「참고문헌에 없는 인용」)
  // 같은 대화의 앞 턴에서 받은 참고문헌을 다시 인용할 수 있으므로 대화 전체와 대조한다
  const refIds = new Set((j.conv.messages || []).flatMap(m => (m.references || []).map(r => r.node_id)));
  const cited = [...new Set([...a.content.matchAll(/<CIT ([A-Z0-9]+)_\w+>/g)].map(m => m[1]))];
  const phantom = cited.filter(id => !refIds.has(id));
  const ys = cited.map(yearOf).filter(Boolean).sort((x, y) => x - y);
  const yearLine = ys.length
    ? `인용 문헌 ${ys.length}편 · 최신 ${ys[ys.length - 1]} · 중간값 ${ys[ys.length >> 1]} · ${cutoff}년 이전 ${ys.filter(y => y < cutoff).length}편 · ${cutoff}년 이전 문헌만 인용한 줄 ${oldOnly.length}`
    : '인용 문헌 없음';
  md +=`## ${j.id} (${j.group}) — 대화 ${j.conversationId}\n\n**질문** ${j.q}\n\n**인용 없는 수치 줄 ${uncited.length} / 수치 줄 ${numLines.length} · 참고문헌에 없는 인용 ${phantom.length}${phantom.length ? ' (' + phantom.join(', ') + ')' : ''}**\n\n**${yearLine}**\n\n${body}\n\n**참고문헌**\n\n`;
  refs.forEach(r => { md += `- [${r.node_id}] ${String(r.node_autr || '').replace(/[<>]/g, '')}, ${r.node_pbsh_yy}, 「${r.node_title}」, 『${r.node_plct_nm}』(${r.node_iprd_nm}) ${r.dmst_lstd_info || ''} ${r.page != null ? 'p.' + r.page : ''}\n`; });
  md += '\n---\n\n';
}
md += `## 전체 참고문헌 ${Object.keys(allRefs).length}건\n\n`;
Object.values(allRefs).sort((x, y) => x.node_pbsh_yy - y.node_pbsh_yy).forEach(r => { md += `- [${r.node_id}] ${String(r.node_autr || '').replace(/[<>]/g, '')} ${r.node_pbsh_yy} 「${r.node_title}」 『${r.node_plct_nm}』 ${r.dmst_lstd_info || ''} ${r.node_url}\n`; });
fs.writeFileSync(out, md);
console.log('answers', files.length, 'refs', Object.keys(allRefs).length, 'bytes', md.length);
