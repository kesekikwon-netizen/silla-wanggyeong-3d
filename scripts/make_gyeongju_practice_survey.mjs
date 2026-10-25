// 경주 연습용 조사(합성) .gpkg 생성 — 실제 유적·조사가 아니다.
// Strata 조사 파일의 7개 레이어·필드는 hgis/samples/demo_survey/demo.gpkg 에서 빈 틀로 복사하고(ogr2ogr -where 1=0),
// 도형은 황룡사지(국가유산청 129.23313E 35.83718N → EPSG:5187 221063.0, 360003.6) 동남동 약 350 m 가상 위치에 넣는다.
// 실행(bash): PATH=/c/OSGeo4W/apps/gdal-dev/bin:/c/OSGeo4W/bin:$PATH PROJ_DATA=C:/OSGeo4W/share/proj node scripts/make_gyeongju_practice_survey.mjs <demo.gpkg> <out.gpkg>
import { execFileSync } from 'node:child_process';
import { mkdtempSync, writeFileSync, rmSync, existsSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

const [src, out] = process.argv.slice(2);
const NOTE = '합성 자료 - 실제 유적·조사 아님 (strata-3d 연습용)';
const cx = 221063.0 + 330.0, cy = 360003.6 - 110.0;
const rect = (x, y, w, h) => [[[x, y], [x + w, y], [x + w, y + h], [x, y + h], [x, y]]];
const P = c => ({ type: 'Polygon', coordinates: c });
const L = c => ({ type: 'LineString', coordinates: c });
const Pt = (x, y) => ({ type: 'Point', coordinates: [x, y] });
const layers = {
  survey_area: [[{ survey_name: '경주 연습용 조사(합성)', site_name: '황룡사지 동편(가상)', note: NOTE }, P(rect(cx - 50, cy - 40, 100, 80))]],
  trial_trench: [0, 1, 2, 3].map(i => [{ name: `TR${i + 1}`, width: 2, length: 60 }, P(rect(cx - 36 + i * 24, cy - 30, 2, 60))]),
  feature_poly: [[{ kind: '건물지(가상)', period: '통일신라(가상)', feature_no: '1', note: NOTE }, P(rect(cx - 14, cy + 2, 12, 9))]],
  feature_line: [[{ kind: '담장(가상)', period: '통일신라(가상)', note: NOTE }, L([[cx - 40, cy + 18], [cx + 40, cy + 18]])]],
  section_line: [[{ section_id: 'S1', note: NOTE }, L([[cx - 35, cy - 25], [cx - 35, cy + 25]])]],
  control_points: [[cx - 55, cy - 45], [cx + 55, cy + 45]].map(([x, y], i) => [{ point_id: `CP${i + 1}`, x, y, z: 0, datum: 'GRS80', ellipsoid: 'GRS80', projection: 'TM', origin: '동부원점', accuracy_m: 0.05 }, Pt(x, y)]),
  artifact_point: [[{ kind: '기와편(가상)', period: '통일신라(가상)', artifact_no: '1', note: NOTE }, Pt(cx - 8, cy + 6)]],
};
if (existsSync(out)) rmSync(out);
const run = args => execFileSync('ogr2ogr', args, { stdio: 'inherit' });
run(['-f', 'GPKG', out, src, '-where', '1=0', '-a_srs', 'EPSG:5187']);   // 빈 틀(필드·도형형식 그대로)
const tmp = mkdtempSync(join(tmpdir(), 'gjps-'));
for (const [name, feats] of Object.entries(layers)) {
  const f = join(tmp, name + '.geojson');
  writeFileSync(f, JSON.stringify({ type: 'FeatureCollection', features: feats.map(([properties, geometry]) => ({ type: 'Feature', properties, geometry })) }));
  run(['-update', '-append', out, f, '-nln', name, '-s_srs', 'EPSG:5187', '-a_srs', 'EPSG:5187']);
}
rmSync(tmp, { recursive: true });
console.log('center EPSG:5187', cx.toFixed(1), cy.toFixed(1));
