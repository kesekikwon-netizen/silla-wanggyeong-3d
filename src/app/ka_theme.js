/**
 * @file ka_theme.js
 * @description Exact port of `C:\Users\권을\Documents\hgis\src\app\KaTheme.h`, `KaTheme.cpp`,
 * `KaThemeColors.cpp`, and `KaThemePaper.cpp` so the standalone 3D App (`src/app/`) maintains
 * 1:1 visual and token parity with the main HGIS application (`gogohak1`).
 */

export const KA_BUTTON_METRICS = Object.freeze({
  ribbonIconSize: 32,
  ribbonFontSize: 13,
  ribbonMinFontSize: 12,
  ribbonChipWidth: 40,
  ribbonMinWidth: 40,
  ribbonLabelPadding: 8,
  ribbonMaxLabelWidth: 64,
  buttonPadding: 1,
  buttonSpacing: 4,
  scaleButtonHeight: 30,
  scaleButtonMinWidth: 54,
  scaleFontSize: 13,
  layoutIconSize: 32,
  layoutButtonHeight: 64,
  panelMargin: 8,
  chipHeight: 22,
  chipRadius: 11,
});

export const KA_ICON_PALETTE = Object.freeze({
  ink: '#1D2733',
  file: '#1F6FB2',
  record: '#7A4A00',
  map: '#256B42',
  align: '#5B6875',
  output: '#B5573A',
  water: '#24526B',
  earth: '#8C6239',
  earthLight: '#B59B7C',
  rock: '#6E7378',
  vegetation: '#3F6B31',
});

export const KA_THEMES = Object.freeze({
  // 1. Stock Strata Chrome (`KaTheme.cpp: strataTokens()`)
  default: {
    key: 'default',
    label: 'Strata 기본 (네이비·블루)',
    accent: '#1F6FB2',
    accentHover: '#185E99',
    accentDeep: '#12558D',
    accentWash: '#E3EEF8',
    ink: '#1D2733',
    inkMuted: '#5B6875',
    inkDisabled: '#596874',
    surface: '#FFFFFF',
    desk: '#F4F6F8',
    altRow: '#F7F9FB',
    stripe: '#F2F6FA',
    hover: '#EEF4FA',
    selected: '#E6F0FA',
    pressed: '#E1ECF7',
    disabledSurface: '#EEF1F4',
    border: '#DCE3EA',
    borderStrong: '#B8C4CF',
    danger: '#B42318',
    dangerSurface: '#FBEDEA',
    ok: '#256B42',
    successSurface: '#EAF4EE',
    warn: '#7A4A00',
    warnSurface: '#FFF4DB',
    rail: '#0B3A63',
    railText: '#FFFFFF',
    railMuted: '#D5E6F5',
    chrome: '#FFFFFF',
    tile: '#EEF4FA',
    tileHover: '#E3EEF8',
    tileOn: '#1F6FB2',
    glyph: '#1D2733',
    glyphOn: '#FFFFFF',
    fontStack: "'Malgun Gothic', 'Apple SD Gothic Neo', sans-serif",
    titleFontStack: "'Malgun Gothic', sans-serif",
    titleWeight: 700,
    paperRadius: 3,
  },

  // 2. 새 모양 / 조용한 도판 (`KaThemePaper.cpp: applyPaperLook()`)
  paper: {
    key: 'paper',
    label: '새 모양 · 조용한 도판 (Paper & Clay)',
    accent: '#B5573A',
    accentHover: '#A44D32',
    accentDeep: '#8F4229',
    accentWash: '#F4E3DA',
    ink: '#141413',
    inkMuted: '#5E5D59',
    inkDisabled: '#696862',
    surface: '#FFFFFF',
    desk: '#F5F4ED',
    altRow: '#FAF9F5',
    stripe: '#F7F6F0',
    hover: '#F5F4ED',
    selected: '#E3DACC',
    pressed: '#DDD8CB',
    disabledSurface: '#F0EEE6',
    border: '#DEDCD1',
    borderStrong: '#C2C0B6',
    danger: '#A33B3B',
    dangerSurface: '#F3DEDA',
    ok: '#3F6B31',
    successSurface: '#E6EEDD',
    warn: '#7A5A00',
    warnSurface: '#F6ECC8',
    rail: '#F0EEE6',
    railText: '#141413',
    railMuted: '#5E5D59',
    chrome: '#FAF9F5',
    tile: '#F0EEE6',
    tileHover: '#E8E6DC',
    tileOn: '#30302E',
    glyph: '#30302E',
    glyphOn: '#FAF9F5',
    fontStack: "'Malgun Gothic', sans-serif",
    titleFontStack: "'Noto Serif KR', 'Batang', 'Malgun Gothic', serif",
    titleWeight: 500,
    paperRadius: 0,
  },

  // 3. 하늘 눈밭 (`KaThemeColors.cpp: skySnow()`)
  sky: {
    key: 'sky',
    label: '하늘 눈밭 (Sky Snow)',
    accent: '#1C6BA6',
    accentHover: '#175B8E',
    accentDeep: '#124A75',
    accentWash: '#DCEEFA',
    ink: '#122536',
    inkMuted: '#465A6C',
    inkDisabled: '#56636F',
    surface: '#FFFFFF',
    desk: '#EAF4FC',
    altRow: '#F5FAFE',
    stripe: '#EDF5FC',
    hover: '#E3F0FB',
    selected: '#CDE5F7',
    pressed: '#B9DAF3',
    disabledSurface: '#EBF0F4',
    border: '#C6DAEA',
    borderStrong: '#9AB7CE',
    danger: '#B42318',
    dangerSurface: '#FBEDEA',
    ok: '#256B42',
    successSurface: '#EAF4EE',
    warn: '#7A4A00',
    warnSurface: '#FFF4DB',
    rail: '#CDE7F8',
    railText: '#122536',
    railMuted: '#33495D',
    chrome: '#F7FBFE',
    tile: '#E0EEF8',
    tileHover: '#D0E5F4',
    tileOn: '#1C6BA6',
    glyph: '#22425B',
    glyphOn: '#FFFFFF',
    fontStack: "'Malgun Gothic', sans-serif",
    titleFontStack: "'Malgun Gothic', sans-serif",
    titleWeight: 700,
    paperRadius: 3,
  },

  // 4. 번개 노랑 (`KaThemeColors.cpp: boltYellow()`)
  bolt: {
    key: 'bolt',
    label: '번개 노랑 (Bolt Yellow)',
    accent: '#C2261F',
    accentHover: '#A61F1A',
    accentDeep: '#8A1915',
    accentWash: '#FBE2DE',
    ink: '#1F1A0A',
    inkMuted: '#54470E',
    inkDisabled: '#5E5434',
    surface: '#FFFFFF',
    desk: '#FFF7D1',
    altRow: '#FFFBE8',
    stripe: '#FFF5C7',
    hover: '#FFF1B8',
    selected: '#FFE47A',
    pressed: '#FFD84A',
    disabledSurface: '#F3EEDA',
    border: '#EAD688',
    borderStrong: '#C7AA36',
    danger: '#B42318',
    dangerSurface: '#FBEDEA',
    ok: '#256B42',
    successSurface: '#EAF4EE',
    warn: '#7A4A00',
    warnSurface: '#FFF4DB',
    rail: '#FFD84A',
    railText: '#1F1A0A',
    railMuted: '#40360A',
    chrome: '#FFFBE8',
    tile: '#FFF1B8',
    tileHover: '#FFE47A',
    tileOn: '#C2261F',
    glyph: '#3A300C',
    glyphOn: '#FFFFFF',
    fontStack: "'Malgun Gothic', sans-serif",
    titleFontStack: "'Malgun Gothic', sans-serif",
    titleWeight: 700,
    paperRadius: 3,
  },

  // 5. 벚꽃 파스텔 (`KaThemeColors.cpp: blossomPastel()`)
  blossom: {
    key: 'blossom',
    label: '벚꽃 파스텔 (Blossom Pastel)',
    accent: '#A82860',
    accentHover: '#8F2050',
    accentDeep: '#751941',
    accentWash: '#F9E0EC',
    ink: '#2B1A2B',
    inkMuted: '#664662',
    inkDisabled: '#685A6A',
    surface: '#FFFFFF',
    desk: '#FCEEF5',
    altRow: '#FEF6FA',
    stripe: '#FBEFF5',
    hover: '#F8E5F0',
    selected: '#EBDAF6',
    pressed: '#DDC8EF',
    disabledSurface: '#F3ECF1',
    border: '#EBD2E1',
    borderStrong: '#CAA3BC',
    danger: '#B42318',
    dangerSurface: '#FBEDEA',
    ok: '#256B42',
    successSurface: '#EAF4EE',
    warn: '#7A4A00',
    warnSurface: '#FFF4DB',
    rail: '#F3D5E6',
    railText: '#2B1A2B',
    railMuted: '#4F3450',
    chrome: '#FEF6FA',
    tile: '#F8E5F0',
    tileHover: '#EBDAF6',
    tileOn: '#A82860',
    glyph: '#4A2545',
    glyphOn: '#FFFFFF',
    fontStack: "'Malgun Gothic', sans-serif",
    titleFontStack: "'Malgun Gothic', sans-serif",
    titleWeight: 700,
    paperRadius: 3,
  },
});

/**
 * WCAG 2.0 Relative Luminance & Contrast Ratio (`KaTheme.cpp: contrastRatio`).
 */
export function hexToRgbNormalized(hex) {
  const clean = hex.replace('#', '');
  const num = parseInt(clean, 16);
  return [(num >> 16) / 255.0, ((num >> 8) & 0xff) / 255.0, (num & 0xff) / 255.0];
}

export function relativeLuminance(hexColor) {
  const [r, g, b] = hexToRgbNormalized(hexColor);
  const lin = (c) => (c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4));
  return 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b);
}

export function contrastRatio(hexA, hexB) {
  const l1 = relativeLuminance(hexA);
  const l2 = relativeLuminance(hexB);
  const lighter = Math.max(l1, l2);
  const darker = Math.min(l1, l2);
  return Number(((lighter + 0.05) / (darker + 0.05)).toFixed(2));
}

/**
 * Resolve `KaTheme::Tokens` for given `DisplayOptions` (`themeKey`, `highContrast`, `largeText`).
 */
export function resolveKaThemeTokens(options = {}) {
  const key = options.themeKey && KA_THEMES[options.themeKey] ? options.themeKey : 'default';
  const base = { ...KA_THEMES[key] };
  const highContrast = Boolean(options.highContrast);
  const largeText = Boolean(options.largeText);

  if (highContrast) {
    base.border = base.borderStrong;
    base.inkMuted = base.ink;
  }

  return {
    ...base,
    highContrast,
    largeText,
    bodyFontSizePx: largeText ? 15 : 13,
    denseFontSizePx: largeText ? 12 : 11,
  };
}

/**
 * Apply resolved `KaTheme` CSS custom properties to a DOM root element (`document.documentElement`).
 */
export function applyKaThemeToDocument(docRoot, options = {}) {
  const t = resolveKaThemeTokens(options);
  if (!docRoot || !docRoot.style) return t;

  const vars = {
    '--ka-accent': t.accent,
    '--ka-accent-hover': t.accentHover,
    '--ka-accent-deep': t.accentDeep,
    '--ka-accent-wash': t.accentWash,
    '--ka-ink': t.ink,
    '--ka-ink-muted': t.inkMuted,
    '--ka-ink-disabled': t.inkDisabled,
    '--ka-surface': t.surface,
    '--ka-desk': t.desk,
    '--ka-alt-row': t.altRow,
    '--ka-stripe': t.stripe,
    '--ka-hover': t.hover,
    '--ka-selected': t.selected,
    '--ka-pressed': t.pressed,
    '--ka-border': t.border,
    '--ka-border-strong': t.borderStrong,
    '--ka-danger': t.danger,
    '--ka-danger-surface': t.dangerSurface,
    '--ka-ok': t.ok,
    '--ka-success-surface': t.successSurface,
    '--ka-warn': t.warn,
    '--ka-warn-surface': t.warnSurface,
    '--ka-rail': t.rail,
    '--ka-rail-text': t.railText,
    '--ka-rail-muted': t.railMuted,
    '--ka-chrome': t.chrome,
    '--ka-tile': t.tile,
    '--ka-tile-hover': t.tileHover,
    '--ka-tile-on': t.tileOn,
    '--ka-glyph': t.glyph,
    '--ka-glyph-on': t.glyphOn,
    '--ka-font-body': t.fontStack,
    '--ka-font-title': t.titleFontStack,
    '--ka-font-size': `${t.bodyFontSizePx}px`,
    '--ka-font-dense': `${t.denseFontSizePx}px`,
    '--ka-paper-radius': `${t.paperRadius}px`,
  };

  for (const [k, v] of Object.entries(vars)) {
    docRoot.style.setProperty(k, v);
  }
  return t;
}
