/**
 * @file gyeongju_archaeology_db.js
 * @description Georeferenced 3D Dataset of Gyeongju (경주) Archaeological Sites,
 * Silla Royal Tumuli (대릉원·황남대총·천마총·봉황대·쪽샘), Palaces & Fortresses (월성·동궁과 월지·명활산성·남산신성),
 * Temples (황룡사지 9층목탑·분황사·사천왕사지·망덕사지·흥륜사지·황복사지),
 * Silla Bangrije (방리제 坊里制) Ancient Street Grid, and 3D Hanok / Urban Building Footprints.
 */

import { wgs84ToLocal3D } from './gyeongju_crs.js';
import { sampleGyeongjuElevation } from './gyeongju_dem_terrain.js';

export const RAW_GYEONGJU_SITES = Object.freeze([
  {
    id: 'GJ-001',
    name: '경주 월성 (반월성·신라 왕궁)',
    category: 'PALACE_FORTRESS',
    designation: '사적 제16호 / 유네스코 세계유산',
    period: '삼국(신라 파사이사금 22년, 101년)~통일신라',
    lat: 35.8300,
    lon: 129.2235,
    archetype3D: 'fortress_palace',
    footprintRadiusM: 320.0,
    heightM: 12.5,
    reconstructedHeightM: 18.0,
    buriedFeatureDepthM: [0.4, 4.2],
    summary: '남천 북안의 초승달 모양 독립구릉에 축조된 신라 천년의 정궁(正宮). 해자·성벽·A~D지구 대형 건물지 및 인신공희 유골 출토.',
    stratigraphy: [
      'I층(0.0~0.3m): 현 표토 및 잔디 복토층',
      'II층(0.3~1.1m): 통일신라 후기 와적층 및 초석 건물지층',
      'III층(1.1~2.4m): 신라 중고기(6세기) 성벽 보축 및 대지 조성층',
      'IV층(2.4~4.2m): 초기 철기~원삼국(사로국) 환호 및 목책·수혈주거지층',
      'V층(4.2m 이하): 기반 풍화암반층',
    ],
  },
  {
    id: 'GJ-002',
    name: '경주 동궁과 월지 (안압지)',
    category: 'PALACE_GARDEN',
    designation: '사적 제18호 / 유네스코 세계유산',
    period: '통일신라 (문무왕 14년, 674년)',
    lat: 35.8348,
    lon: 129.2266,
    archetype3D: 'pond_palace',
    footprintRadiusM: 145.0,
    heightM: 11.0,
    reconstructedHeightM: 15.0,
    buriedFeatureDepthM: [0.5, 3.5],
    summary: '삼국통일 직후 조영된 신라 태자궁과 대형 인공 원지(苑池). 임해전 등 26개소 전각터와 목선·금동판불·목간 3만여 점 출토.',
    stratigraphy: [
      'I층(0.0~0.4m): 정비 복토층',
      'II층(0.4~1.6m): 통일신라 전각 기단·배수로·호안 석축층',
      'III층(1.6~3.2m): 원지 내부 유기물 부식뻘층 (목간·금속공예품 다량 포유)',
      'IV층(3.2m 이하): 하안단구 사질점토 생토층',
    ],
  },
  {
    id: 'GJ-003',
    name: '경주 첨성대',
    category: 'ROYAL_OBSERVATORY',
    designation: '국보 제31호 / 유네스코 세계유산',
    period: '삼국(신라 선덕여왕대, 7세기 전반)',
    lat: 35.8347,
    lon: 129.2190,
    archetype3D: 'observatory_tower',
    footprintRadiusM: 5.2,
    heightM: 9.17,
    reconstructedHeightM: 9.17,
    buriedFeatureDepthM: [0.3, 2.2],
    summary: '선덕여왕대 축조된 현존 동양 최고(最古)의 천문 관측대. 월성 북서쪽 왕경 중심축 기준점에 위치.',
    stratigraphy: [
      'I층(0.0~0.4m): 공원 정비 표토층',
      'II층(0.4~1.3m): 통일신라 도시 도로·담장·배수로 문화층',
      'III층(1.3~2.5m): 5~7세기 신라 적석기단 및 정지층',
    ],
  },
  {
    id: 'GJ-004',
    name: '경주 황룡사지 (9층목탑·중금당)',
    category: 'ROYAL_TEMPLE',
    designation: '사적 제6호 / 유네스코 세계유산',
    period: '삼국(신라 진흥왕 14년, 553년 창건 ~ 645년 9층목탑 완공)',
    lat: 35.8375,
    lon: 129.2333,
    archetype3D: 'pagoda_9story',
    footprintRadiusM: 185.0,
    heightM: 3.5,
    reconstructedHeightM: 80.0,
    buriedFeatureDepthM: [0.3, 3.8],
    summary: '신라 최대 국찰(國刹). 선덕여왕 14년(645) 자장율사 건의로 아비지가 건립한 높이 약 80m(225척)의 9층목탑지 및 장육존상 금당지.',
    stratigraphy: [
      'I층(0.0~0.3m): 사적지 정비 잔디층',
      'II층(0.3~0.9m): 고려 고종 25년(1238) 몽골 병화 소토층 및 와적층',
      'III층(0.9~1.9m): 통일신라 중창 가람 기단·판축층',
      'IV층(1.9~3.5m): 6세기 중엽 창건기 가람 대지 성토층 및 늪지 매립층',
    ],
  },
  {
    id: 'GJ-005',
    name: '경주 분황사 (모전석탑)',
    category: 'ROYAL_TEMPLE',
    designation: '국보 제30호 / 사적',
    period: '삼국(신라 선덕여왕 3년, 634년)',
    lat: 35.8406,
    lon: 129.2338,
    archetype3D: 'stone_pagoda',
    footprintRadiusM: 85.0,
    heightM: 9.3,
    reconstructedHeightM: 27.0,
    buriedFeatureDepthM: [0.4, 2.8],
    summary: '황룡사 북쪽에 인접한 선덕여왕 창건 왕실 사찰. 안산암을 벽돌 모양으로 다듬어 쌓은 신라 최고(最古)의 모전석탑 현존.',
    stratigraphy: [
      'I층(0.0~0.4m): 경내 마사토 표토층',
      'II층(0.4~1.5m): 고려~조선 중창 문화층',
      'III층(1.5~2.8m): 7세기 창건기 일탑삼금당 가람 기단층',
    ],
  },
  {
    id: 'GJ-006',
    name: '경주 대릉원 황남대총 (98호 남북쌍분)',
    category: 'ROYAL_TUMULUS',
    designation: '사적 제512호 / 유네스코 세계유산',
    period: '삼국(신라 마립간기, 4세기 말~5세기 전반)',
    lat: 35.8373,
    lon: 129.2128,
    archetype3D: 'tumulus_double',
    footprintRadiusM: 60.0,
    heightM: 23.0,
    reconstructedHeightM: 23.0,
    buriedFeatureDepthM: [0.0, 5.5],
    summary: '남북 길이 120m, 동서 직경 80m, 높이 23m에 달하는 신라 최대의 표주박형(쌍분) 적석목곽분. 남분 금동관·북분 금관 및 비단벌레 마구 출토.',
    stratigraphy: [
      '봉분 성토층(지상 +23.0m): 표토 잔디 및 다짐 성토·냇돌 호석',
      '내부 적석부: 거대 천석(川石) 적석 및 목곽 안치부',
      '기저부(0.0~2.0m): 구지표 정지층 및 선행 소형 적석목곽분·옹관묘 문화층',
    ],
  },
  {
    id: 'GJ-007',
    name: '경주 대릉원 천마총 (155호분)',
    category: 'ROYAL_TUMULUS',
    designation: '사적 제512호 (국보 금관·천마도 출토)',
    period: '삼국(신라 마립간기 말~중고기 초, 5세기 말~6세기 초)',
    lat: 35.8384,
    lon: 129.2116,
    archetype3D: 'tumulus_single',
    footprintRadiusM: 23.5,
    heightM: 12.7,
    reconstructedHeightM: 12.7,
    buriedFeatureDepthM: [0.0, 4.0],
    summary: '직경 47m, 높이 12.7m의 전형적인 신라 적석목곽분. 자작나무 껍질에 그린 천마도 장니와 신라 금관·금제 과대 등 11,500여 점 출토.',
    stratigraphy: [
      '봉분 성토층(지상 +12.7m): 점토·사질토 교호 다짐층',
      '기저부(0.0~1.8m): 구지표 정지층 및 하안단구 역층',
    ],
  },
  {
    id: 'GJ-008',
    name: '경주 대릉원 미추왕릉 (죽장릉)',
    category: 'ROYAL_TUMULUS',
    designation: '사적 제175호',
    period: '삼국(신라 마립간기)',
    lat: 35.8358,
    lon: 129.2125,
    archetype3D: 'tumulus_single',
    footprintRadiusM: 28.0,
    heightM: 12.4,
    reconstructedHeightM: 12.4,
    buriedFeatureDepthM: [0.0, 3.8],
    summary: '김씨 최초의 왕인 미추이사금의 능으로 전해지는 대형 고분. 주변에서 상감유리옥·금제장식 보검이 출토된 지구 포함.',
    stratigraphy: [
      '봉분 성토층(지상 +12.4m)',
      '주변 평지구(0.3~2.8m): 4~6세기 밀집 적석목곽분·석곽묘 및 통일신라 생활유구 중첩층',
    ],
  },
  {
    id: 'GJ-009',
    name: '경주 노동동 봉황대 (125호분)',
    category: 'ROYAL_TUMULUS',
    designation: '사적 제512호',
    period: '삼국(신라 마립간기, 5세기)',
    lat: 35.8419,
    lon: 129.2111,
    archetype3D: 'tumulus_single',
    footprintRadiusM: 41.0,
    heightM: 22.0,
    reconstructedHeightM: 22.0,
    buriedFeatureDepthM: [0.0, 4.5],
    summary: '단일 원분으로는 신라 최대 규모(밑지름 82m, 높이 22m)의 초대형 적석목곽분. 금관총·서봉총·식리총 고분군의 중심 랜드마크.',
    stratigraphy: [
      '봉분 성토층(지상 +22.0m)',
      '인접 지하(0.5~3.5m): 금관총·서봉총·호우총과 연결되는 고밀도 왕릉급 고분 묘역층',
    ],
  },
  {
    id: 'GJ-010',
    name: '경주 쪽샘지구 고분군 (44호분·가~라지구)',
    category: 'ROYAL_TUMULUS',
    designation: '사적 제512호',
    period: '원삼국~삼국(신라 4~6세기)',
    lat: 35.8372,
    lon: 129.2178,
    archetype3D: 'tumulus_cluster',
    footprintRadiusM: 140.0,
    heightM: 8.5,
    reconstructedHeightM: 11.0,
    buriedFeatureDepthM: [0.2, 3.8],
    summary: '대릉원 동쪽에 연접한 약 38만㎡ 규모의 신라 왕족·귀족 집단 묘역. 목곽묘→적석목곽분→석곽묘 변천 과정과 철갑옷·마구 다량 확인.',
    stratigraphy: [
      'I층(0.0~0.5m): 구 근현대 가옥 철거 정지층',
      'II층(0.5~1.3m): 통일신라 방리제 도로·담장·우물·건물지 문화층 (고분 삭평 후 도시화)',
      'III층(1.3~3.8m): 4~6세기 적석목곽분·목곽묘·옹관묘 주묘역층',
    ],
  },
  {
    id: 'GJ-011',
    name: '경주 월정교 (춘양교·일정교 유적)',
    category: 'ROYAL_INFRA',
    designation: '사적 제457호',
    period: '통일신라 (경덕왕 19년, 760년)',
    lat: 35.8291,
    lon: 129.2180,
    archetype3D: 'bridge_corridor',
    footprintRadiusM: 35.0,
    heightM: 14.5,
    reconstructedHeightM: 15.6,
    buriedFeatureDepthM: [0.5, 3.0],
    summary: '월성 서남쪽 남천을 가로질러 왕궁과 남산 지역을 연결하던 길이 66.15m의 대형 누교(樓橋). 교각 초석과 문루 복원.',
    stratigraphy: [
      '하상 충적사력층(0.0~3.0m): 주형 교각 기초 석축 및 불탄 목부재·기와 퇴적층',
    ],
  },
  {
    id: 'GJ-012',
    name: '경주 사천왕사지 (낭산 서남 기슭)',
    category: 'ROYAL_TEMPLE',
    designation: '사적 제8호',
    period: '통일신라 (문무왕 19년, 679년)',
    lat: 35.8206,
    lon: 129.2398,
    archetype3D: 'twin_pagoda_temple',
    footprintRadiusM: 95.0,
    heightM: 6.5,
    reconstructedHeightM: 24.0,
    buriedFeatureDepthM: [0.3, 2.6],
    summary: '명랑법사의 문두루 비법으로 당군을 물리친 뒤 창건한 호국 사찰. 통일신라 쌍탑식 가람배치의 효시이며 녹유신장상 전(塼) 출토.',
    stratigraphy: [
      'I층(0.0~0.4m): 표토층',
      'II층(0.4~1.8m): 통일신라 쌍목탑·금당·단석 기단 및 녹유와전 문화층',
      'III층(1.8~2.6m): 낭산 사면 정지 성토층',
    ],
  },
  {
    id: 'GJ-013',
    name: '경주 선덕여왕릉 (낭산 정상부)',
    category: 'ROYAL_TUMULUS',
    designation: '사적 제182호 / 유네스코 세계유산',
    period: '삼국(신라 선덕여왕 16년, 647년)',
    lat: 35.8240,
    lon: 129.2435,
    archetype3D: 'tumulus_single',
    footprintRadiusM: 12.0,
    heightM: 6.8,
    reconstructedHeightM: 6.8,
    buriedFeatureDepthM: [0.0, 3.0],
    summary: '신라 제27대 선덕여왕의 유언에 따라 도리천(忉利天)에 해당하는 낭산 남쪽 봉우리에 조영된 원형 봉토분. 하단에 자연석 호석 배치.',
    stratigraphy: [
      '산악 능선 풍화토 기반 위 할석 호석 및 봉토 성토층',
    ],
  },
  {
    id: 'GJ-014',
    name: '경주 망덕사지 (사천왕사 남쪽)',
    category: 'ROYAL_TEMPLE',
    designation: '사적 제7호',
    period: '통일신라 (신문왕 5년, 685년)',
    lat: 35.8180,
    lon: 129.2393,
    archetype3D: 'twin_pagoda_temple',
    footprintRadiusM: 88.0,
    heightM: 4.5,
    reconstructedHeightM: 28.0,
    buriedFeatureDepthM: [0.4, 2.5],
    summary: '사천왕사와 남북으로 마주 보며 당나라 사신을 위해 건립한 쌍목탑(13층 목탑) 가람.',
    stratigraphy: [
      'I층(0.0~0.5m): 경작 표토층',
      'II층(0.5~2.2m): 통일신라 동·서 13층 목탑 초석 및 회랑·금당지 문화층',
    ],
  },
  {
    id: 'GJ-015',
    name: '경주 재매정지 (김유신 장군 재택 우물)',
    category: 'ARISTOCRAT_RESIDENCE',
    designation: '사적 제246호',
    period: '삼국~통일신라',
    lat: 35.8286,
    lon: 129.2145,
    archetype3D: 'aristocrat_mansion',
    footprintRadiusM: 42.0,
    heightM: 5.5,
    reconstructedHeightM: 8.5,
    buriedFeatureDepthM: [0.4, 3.2],
    summary: '삼국사기 김유신 열전에 기록된 김유신 가문의 저택(재매정댁) 터와 석조 우물 유구.',
    stratigraphy: [
      'I층(0.0~0.5m): 표토층',
      'II층(0.5~2.8m): 통일신라 귀족 대형 택지(방리 내 1방 규모) 담장·건물지·우물층',
    ],
  },
  {
    id: 'GJ-016',
    name: '경주 흥륜사지 (영묘사 추정지·사정동)',
    category: 'ROYAL_TEMPLE',
    designation: '사적 제15호',
    period: '삼국(신라 법흥왕 14년, 527년 이차돈 순교 창건)',
    lat: 35.8331,
    lon: 129.2063,
    archetype3D: 'stone_pagoda',
    footprintRadiusM: 90.0,
    heightM: 7.0,
    reconstructedHeightM: 22.0,
    buriedFeatureDepthM: [0.4, 3.4],
    summary: '신라 최초의 공인 사찰(또는 영묘사지 비정). 고려시대 루하비(수막새) 등 다량의 고급 기와와 금당·탑지 확인.',
    stratigraphy: [
      'I층(0.0~0.5m): 현 사찰 표토층',
      'II층(0.5~1.8m): 고려·통일신라 사찰 건물지층',
      'III층(1.8~3.4m): 6세기 신라 불교 공인기 초창 대지 조성층',
    ],
  },
  {
    id: 'GJ-017',
    name: '경주 오릉 및 나정 (박혁거세 탄강·왕릉)',
    category: 'ROYAL_TUMULUS',
    designation: '사적 제172호 / 제245호',
    period: '원삼국~삼국(신라 초기~마립간기)',
    lat: 35.8235,
    lon: 129.2038,
    archetype3D: 'tumulus_cluster',
    footprintRadiusM: 130.0,
    heightM: 10.5,
    reconstructedHeightM: 10.5,
    buriedFeatureDepthM: [0.3, 3.5],
    summary: '신라 시조 박혁거세와 알영부인 등 초기 박씨 왕들의 능역 및 시조 탄강 우물(나정) 팔각건물지.',
    stratigraphy: [
      'I층(0.0~0.4m): 송림 표토층',
      'II층(0.4~1.8m): 통일신라 팔각건물지·제사 유구층',
      'III층(1.8~3.5m): 원삼국~초기 신라 구릉 및 하안단구 문화층',
    ],
  },
  {
    id: 'GJ-018',
    name: '경주 명활산성',
    category: 'PALACE_FORTRESS',
    designation: '사적 제47호 / 유네스코 세계유산',
    period: '삼국(신라 실성이사금~자비마립간 왕궁 이궁, 5~7세기)',
    lat: 35.8355,
    lon: 129.2610,
    archetype3D: 'fortress_palace',
    footprintRadiusM: 420.0,
    heightM: 10.0,
    reconstructedHeightM: 12.0,
    buriedFeatureDepthM: [0.2, 2.8],
    summary: '경주 왕경 동쪽 방어 거점이자 자비마립간이 13년간 거처한 산성 왕궁. 명활산성 작성비(551년) 출토.',
    stratigraphy: [
      '산성 성벽 할석 축조층 및 성내 집수지·건물지 문화층(0.2~2.8m)',
    ],
  },
  {
    id: 'GJ-019',
    name: '경주 남산신성 (남산 북단 산성)',
    category: 'PALACE_FORTRESS',
    designation: '사적 제22호 / 유네스코 세계유산',
    period: '삼국(신라 진평왕 13년, 591년)',
    lat: 35.8185,
    lon: 129.2248,
    archetype3D: 'fortress_palace',
    footprintRadiusM: 380.0,
    heightM: 9.0,
    reconstructedHeightM: 11.0,
    buriedFeatureDepthM: [0.2, 2.5],
    summary: '왕경 남쪽 남산 북봉 능선을 따라 축조된 대규모 석축 산성 및 대형 무기창고(장창지). 남산신성비 10기 출토.',
    stratigraphy: [
      '능선 암반 정지층 위 석축 성벽 및 탄화미·철제 무기류 창고지 문화층',
    ],
  },
  {
    id: 'GJ-020',
    name: '경주 성동동 전랑지 (관청·동궁 부속 전각터)',
    category: 'PALACE_FORTRESS',
    designation: '사적 제88호',
    period: '통일신라 (7~8세기)',
    lat: 35.8412,
    lon: 129.2245,
    archetype3D: 'aristocrat_mansion',
    footprintRadiusM: 75.0,
    heightM: 6.5,
    reconstructedHeightM: 11.5,
    buriedFeatureDepthM: [0.4, 3.0],
    summary: '월성 북쪽 왕경 핵심 관청지구(북궁·성동동 일대)에 위치한 대규모 회랑 및 온돌 전각 유적.',
    stratigraphy: [
      'I층(0.0~0.5m): 표토층',
      'II층(0.5~2.4m): 통일신라 대형 관청 초석·전돌 배수로·회랑 유구층',
    ],
  },
]);

/**
 * Enrich all Gyeongju archaeological sites with Local 3D coordinates `(worldX, worldY, worldZ)`
 * and EPSG:5187 coordinates.
 */
export function getGyeongjuArchaeologySites3D() {
  return RAW_GYEONGJU_SITES.map((site) => {
    const loc = wgs84ToLocal3D(site.lat, site.lon, 0.0);
    const groundElevM = sampleGyeongjuElevation(loc.worldX, loc.worldZ);
    return {
      ...site,
      worldX: loc.worldX,
      worldY: groundElevM,
      worldZ: loc.worldZ,
      epsg5187X: loc.epsg5187X,
      epsg5187Y: loc.epsg5187Y,
      groundElevationM: groundElevM,
      topElevationM: groundElevM + site.heightM,
      reconstructedTopElevationM: groundElevM + site.reconstructedHeightM,
    };
  });
}

/**
 * Generate the Silla Royal Capital Gridded Street Network (`신라 왕경 방리제(坊里制) 도로망`).
 * Historically spans roughly 140m–160m per Bang (坊) block across the Hwangnyongsa–Bunhwangsa–Donggung–Seongdong-dong plain,
 * oriented approximately N 4.5° E to align with the Bukcheon alluvial fan and Hwangnyongsa–Wolseong axis.
 */
export function generateSillaBangrijeGrid3D() {
  const angleRad = (4.5 * Math.PI) / 180.0;
  const cosA = Math.cos(angleRad);
  const sinA = Math.sin(angleRad);
  const blockStepM = 160.0; // 1 Bang (~160m x 160m)
  const centerX = 620.0; // Centered near Hwangnyongsa / Donggung / Seongdong-dong
  const centerZ = -320.0;

  const roads = [];
  // North-South Avenues (남북대로, 폭 15~23m)
  for (let ix = -9; ix <= 9; ix++) {
    const u = ix * blockStepM;
    const isMainAxis = ix === 0 || ix === -4 || ix === 4;
    const pts = [];
    for (let s = -10; s <= 8; s++) {
      const v = s * blockStepM;
      const wx = centerX + u * cosA - v * sinA;
      const wz = centerZ + u * sinA + v * cosA;
      const wy = sampleGyeongjuElevation(wx, wz) + 0.65;
      pts.push([wx, wy, wz]);
    }
    roads.push({
      id: `NS-ROAD-${ix + 10}`,
      orientation: 'NS',
      isMainRoyalAvenue: isMainAxis,
      widthM: isMainAxis ? 22.0 : 12.0,
      points: pts,
    });
  }

  // East-West Streets (동서대로)
  for (let iz = -10; iz <= 8; iz++) {
    const v = iz * blockStepM;
    const isMainAxis = iz === 0 || iz === -3 || iz === 3;
    const pts = [];
    for (let s = -9; s <= 9; s++) {
      const u = s * blockStepM;
      const wx = centerX + u * cosA - v * sinA;
      const wz = centerZ + u * sinA + v * cosA;
      const wy = sampleGyeongjuElevation(wx, wz) + 0.65;
      pts.push([wx, wy, wz]);
    }
    roads.push({
      id: `EW-ROAD-${iz + 11}`,
      orientation: 'EW',
      isMainRoyalAvenue: isMainAxis,
      widthM: isMainAxis ? 20.0 : 11.0,
      points: pts,
    });
  }

  return {
    originWorldX: centerX,
    originWorldZ: centerZ,
    azimuthDeg: 4.5,
    blockPitchM: blockStepM,
    roads,
  };
}

/**
 * Generate GPU-instanced 3D building footprints for Gyeongju:
 * - Preserves exclusion zones around Wolseong, Daereungwon, Donggung, Hwangnyongsa, and rivers.
 * - Generates authentic low-rise Hanok / Historic Preservation Zone buildings (1–2 stories, tiled roof palette)
 *   in Hwangnam-dong (`황리단길`), Gyochon (`교촌마을`), Inwang-dong, and Noseo/Nodong-dong,
 *   and mid-rise urban blocks in Seongdong-dong / Dongcheon-dong / Hwangseong-dong.
 */
export function generateGyeongju3DBuildings(countTarget = 950) {
  const buildings = [];
  const sites = getGyeongjuArchaeologySites3D();

  // Deterministic pseudo-random helper
  let seed = 20261024;
  const rand = () => {
    seed = (seed * 1664525 + 1013904223) >>> 0;
    return seed / 4294967296.0;
  };

  // Urban district clusters in local 3D space (worldX, worldZ)
  const districts = [
    { name: '황남동 한옥지구(황리단길)', cx: -820, cz: 180, rx: 520, rz: 620, maxFloors: 2, isHanok: true, weight: 0.28 },
    { name: '교촌·인왕동 한옥마을', cx: -120, cz: 480, rx: 380, rz: 260, maxFloors: 1, isHanok: true, weight: 0.14 },
    { name: '성동동·노서동 도심구역', cx: -450, cz: -780, rx: 950, rz: 650, maxFloors: 5, isHanok: false, weight: 0.30 },
    { name: '동천동·황성동 시가지', cx: 350, cz: -2150, rx: 1200, rz: 680, maxFloors: 8, isHanok: false, weight: 0.18 },
    { name: '구황동·배반동 취락지구', cx: 1450, cz: 420, rx: 650, rz: 580, maxFloors: 2, isHanok: true, weight: 0.10 },
  ];

  let attempts = 0;
  while (buildings.length < countTarget && attempts < countTarget * 5) {
    attempts++;
    const r = rand();
    let acc = 0.0;
    let dist = districts[0];
    for (const d of districts) {
      acc += d.weight;
      if (r <= acc) {
        dist = d;
        break;
      }
    }

    // Box-Muller / uniform disk offset aligned to street grid
    const gx = (rand() * 2.0 - 1.0) * dist.rx;
    const gz = (rand() * 2.0 - 1.0) * dist.rz;
    const wx = Math.round((dist.cx + gx) / 18.0) * 18.0 + (rand() - 0.5) * 4.0;
    const wz = Math.round((dist.cz + gz) / 18.0) * 18.0 + (rand() - 0.5) * 4.0;

    // Check exclusion zones around protected archaeological monuments
    let insideMonument = false;
    for (const s of sites) {
      const d = Math.hypot(wx - s.worldX, wz - s.worldZ);
      if (d < Math.max(55.0, s.footprintRadiusM * 0.92)) {
        insideMonument = true;
        break;
      }
    }
    if (insideMonument) continue;

    const elev = sampleGyeongjuElevation(wx, wz);
    if (elev > 82.0 || elev < 38.2) continue;

    const floors = 1 + Math.floor(rand() * dist.maxFloors);
    const heightM = dist.isHanok ? 3.8 + floors * 2.4 : 3.4 * floors + rand() * 1.8;
    const widthM = dist.isHanok ? 9.0 + rand() * 9.0 : 12.0 + rand() * 18.0;
    const depthM = dist.isHanok ? 7.0 + rand() * 8.0 : 10.0 + rand() * 15.0;
    const rotationRad = ((4.5 + (rand() - 0.5) * 6.0) * Math.PI) / 180.0;

    buildings.push({
      id: `BLD-${buildings.length + 1}`,
      district: dist.name,
      isHanok: dist.isHanok,
      worldX: wx,
      worldY: elev,
      worldZ: wz,
      widthM,
      depthM,
      heightM,
      floors,
      rotationRad,
    });
  }

  return buildings;
}
