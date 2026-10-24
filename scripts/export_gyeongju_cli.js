/**
 * @file export_gyeongju_cli.js
 * @description Command-line pipeline to evaluate Gyeongju royal capital sites and export:
 * 1. `dist/gyeongju_archaeology_evaluation.md` (Comprehensive Archaeological Evaluation Report)
 * 2. `dist/gyeongju_blender42_scene.py` (Blender 4.2.3 EEVEE-Next + Nishita Sky + AgX + BVHTree Scene Script)
 * 3. `dist/gyeongju_terrain_sample.obj` (Wavefront OBJ 3D Terrain Mesh)
 */

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { GyeongjuTerrainQuadtree } from '../src/core/gyeongju_dem_terrain.js';
import { GyeongjuArchaeologyEvaluator } from '../src/core/archaeology_evaluator.js';
import {
  generateBlender42PythonScript,
  exportGyeongjuWavefrontObj,
  generateArchaeologicalMarkdownReport,
} from '../src/bridge/blender_exporter.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const distDir = path.resolve(__dirname, '..', 'dist');
fs.mkdirSync(distDir, { recursive: true });

const qt = new GyeongjuTerrainQuadtree({
  extentM: 6000,
  maxDepth: 2,
  chunkGridSegments: 12,
  skirtDepthM: 15,
}).build();

const evaluator = new GyeongjuArchaeologyEvaluator(qt.bvh);

// Evaluate prime Gyeongju Royal Capital corridor between Wolseong, Cheomseongdae, Jjoksaem, and Donggung
const report = evaluator.evaluateSite({
  lat: 35.8342,
  lon: 129.2215,
  surveyName: '경주 인왕동 왕경 중심지구(월성 북편·첨성대 동편) 정밀 발굴·시굴 평가구역',
  surveyAreaSqm: 2400,
});

const mdPath = path.join(distDir, 'gyeongju_archaeology_evaluation.md');
const pyPath = path.join(distDir, 'gyeongju_blender42_scene.py');
const objPath = path.join(distDir, 'gyeongju_terrain_sample.obj');

fs.writeFileSync(mdPath, generateArchaeologicalMarkdownReport(report), 'utf8');
fs.writeFileSync(pyPath, generateBlender42PythonScript(report, { gridResolution: 32, extentM: 4000 }), 'utf8');
fs.writeFileSync(objPath, exportGyeongjuWavefrontObj(qt), 'utf8');

console.log('[export_gyeongju_cli] Generated artifacts:');
console.log(' - Markdown Report:', mdPath);
console.log(' - Blender 4.2 Script:', pyPath);
console.log(' - Wavefront 3D OBJ:', objPath);
console.log(' - Archaeological Score:', report.assessmentSummary.archaeologicalPotentialScore, '/', 100);
console.log(' - Recommended Grade:', report.assessmentSummary.recommendedActionGrade);
