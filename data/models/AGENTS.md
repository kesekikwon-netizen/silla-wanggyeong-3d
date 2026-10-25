# models

**Parent context:** `../AGENTS.md`
**Generated:** 2026-10-25 · **Updated:** 2026-10-25

## Purpose
Blender 4.2.3(bpy + bmesh)로 유형 틀에서 만든 경주 유적 **도식(D 등급)** 모델. 실측·근거 복원이 아니다.

## Key Files
| File | Description |
|------|-------------|
| `gyeongju_blender_heritage.s3dmesh` | `S3DMESH_V1` 번들(약 29 MB), 311개 유적 정점. `BangriTerrainValidator` 가 읽고 `applyTierStyle` 로 D 등급 색을 입힌다 |
| `gyeongju_heritage_manifest.json` | 311개 유적 메타(id·이름·종목·부제·경위도·라벨 높이·카메라) |
| `gyeongju_khs_source.json` | 매니페스트를 만든 국가유산청 원목록 |
| `gyeongju/*.ply` | 초기 20개 핵심 유적 개별 메시 |

## For AI Agents

### Working In This Directory
- 매니페스트 이름·부제에는 옛 지정번호(「사적 제6호」)와 「복원」 문구가 남아 있다. 원본을 고치지 말고 화면에 낼 때 `displayHeritageName()`·`schematicModelWording()` 을 거친다(`test_heritage_trust` 가 311개 전수 확인).
- 다시 만들 때는 `scripts/generate_gyeongju_heritage_blender.py`. 모델을 근거 있는 복원(B)으로 올리려면 출처·도면 근거를 `docs/research/` 에 먼저 남긴다.

## Manual Notes
