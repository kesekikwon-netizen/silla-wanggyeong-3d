#include "RibbonSpec.h"

namespace ribbon {

namespace {
Button b(const char* id, const char* icon, const QString& label, const QString& tip, bool checkable = false, bool menu = false) {
  return Button{QString::fromLatin1(id), QString::fromLatin1(icon), label, tip, checkable, menu};
}
}  // namespace

const QList<Group>& groups() {
  static const QList<Group> table = [] {
    QList<Group> g;
    // 조사: Strata 와 같은 네 단추. 3D 시점·켠 레이어는 조사 옆 파일(.s3d.json)에 둔다.
    g.append({QStringLiteral("survey"), QStringLiteral("조사"), QStringLiteral("survey"), {
        b("new", "new", QStringLiteral("새 조사"), QStringLiteral("현장 조사 파일(.gpkg)을 새로 만듭니다 (Ctrl+N). 조사구역은 「그리기」로 지형 위에 그립니다")),
        b("open", "open", QStringLiteral("열기"), QStringLiteral("Strata 에서 저장한 조사(.gpkg)를 엽니다 (Ctrl+O)")),
        b("save", "save", QStringLiteral("저장"), QStringLiteral("그린 도형·기록을 조사 파일에(Strata 와 같은 저장), 3D 시점은 조사 옆 파일에 저장합니다 (Ctrl+S)")),
        b("save_as", "save_as", QStringLiteral("다른 이름"), QStringLiteral("3D 시점·보기 설정을 다른 파일로 저장합니다")),
    }});
    // 기록: 아이콘은 Strata 그대로, 3D 지형 위를 눌러 그린다.
    g.append({QStringLiteral("record"), QStringLiteral("기록"), QStringLiteral("record"), {
        b("select", "select", QStringLiteral("선택"), QStringLiteral("3D 화면에서 유적·구역을 눌러 고릅니다"), true),
        b("measure", "measure", QStringLiteral("측거"), QStringLiteral("지형 위 두 점의 평면 거리·지표 거리·높이차를 잽니다"), true),
        b("draw_poly", "draw_poly", QStringLiteral("그리기"), QStringLiteral("조사구역·유구·유물을 지형 위에 그려 조사 레이어에 넣습니다 (Ctrl+D)"), false, true),
        b("trench_grid", "trench_grid", QStringLiteral("시굴격자"), QStringLiteral("지형 위 두 점으로 시굴 격자를 놓습니다"), true),
        b("buffer", "buffer", QStringLiteral("버퍼"), QStringLiteral("조사구역 둘레 500 m · 1000 m 안의 주변유적")),
    }});
    // 자료 받기: Strata 받기 흐름 그대로(유산·지적·수치지형).
    g.append({QStringLiteral("fetch"), QStringLiteral("자료 받기"), QStringLiteral("fetch"), {
        b("cadastral", "cadastral", QStringLiteral("지적"), QStringLiteral("조사구역 둘레의 지적도를 받습니다")),
        b("topo_download", "topo_download", QStringLiteral("수치지형"), QStringLiteral("국토지리정보원 수치지형도를 받습니다")),
        b("heritage", "heritage", QStringLiteral("유산"), QStringLiteral("국가유산청 주변유적을 받습니다")),
    }});
    // 지형 위에 올리기: Strata 「배경 지도」 아이콘 그대로, 2D 배경이 아니라 3D 지형에 입히는 층.
    g.append({QStringLiteral("drape"), QStringLiteral("지형 위에 올리기"), QStringLiteral("basemap"), {
        b("satellite", "satellite", QStringLiteral("위성"), QStringLiteral("VWorld 위성 영상을 지형에 입힙니다(저장하지 않음)"), true),
        b("contour", "contour", QStringLiteral("지형"), QStringLiteral("음영 지형(점토)으로 봅니다"), true),
        b("dem", "dem", QStringLiteral("DEM"), QStringLiteral("표고 색띠·경사로 봅니다"), false, true),
        b("survey_contour", "survey_contour", QStringLiteral("등고선"), QStringLiteral("수치지형도 등고선을 3D 선으로 올립니다"), true),
        b("soil", "soil", QStringLiteral("토양"), QStringLiteral("토양도를 지형에 반투명으로 입힙니다"), true),
        b("paleo", "paleo", QStringLiteral("고지형"), QStringLiteral("고지형도를 지형에 반투명으로 입힙니다"), true),
        b("geology", "geology", QStringLiteral("지질"), QStringLiteral("지질도를 지형에 반투명으로 입힙니다"), true),
        b("river", "river", QStringLiteral("수계"), QStringLiteral("수계도를 지형에 올립니다"), true),
        b("old_map", "old_map", QStringLiteral("옛 지도"), QStringLiteral("옛 지도를 지형에 입힙니다"), true),
    }});
    // 3D 보기: 3D 전용.
    g.append({QStringLiteral("view"), QStringLiteral("3D 보기"), QString(), {
        b("view3d", "view3d", QStringLiteral("시점"), QStringLiteral("위·조감·조사구역 시점"), false, true),
        b("zscale", "zscale", QStringLiteral("높이"), QStringLiteral("높이 배율(×1 기본). 바꾸면 화면에 늘 표시됩니다"), false, true),
        b("labels", "labels", QStringLiteral("이름표"), QStringLiteral("유적 이름표 켜기·크기 (Ctrl+= / Ctrl+- / Ctrl+0)"), false, true),
        b("tiers", "tiers", QStringLiteral("등급"), QStringLiteral("자료 등급별 켜기: A 실측 · B 근거 · D 도식"), false, true),
        b("subsurface", "subsurface", QStringLiteral("땅속"), QStringLiteral("지표를 반투명하게 해 땅속 기록을 봅니다"), true),
    }});
    // 3D 분석: 3D 전용. 결과는 아래 칸.
    g.append({QStringLiteral("analysis"), QStringLiteral("3D 분석"), QString(), {
        b("zones", "zones", QStringLiteral("구역"), QStringLiteral("조사구역과 보호구역·현상변경허용기준·유존지역·허가구역의 겹침과 거리")),
        b("relations", "relations", QStringLiteral("관계 표"), QStringLiteral("조사구역과 주변 유산의 거리·방위·높이차·시대")),
        b("section", "section", QStringLiteral("단면"), QStringLiteral("지형 위 두 점의 지표 단면"), true),
        b("depth", "depth", QStringLiteral("깊이"), QStringLiteral("기록된 유구 윗면·바닥 표고")),
        b("inventory", "inventory", QStringLiteral("현황표"), QStringLiteral("주변유적 현황표(Strata 엑셀 17열과 같은 칸)")),
    }});
    // 내보내기: Strata 틀 그대로. 「GeoTIFF」 자리는 「이미지」(3D 그림 한 장은 지도 좌표에 맞지 않는다).
    g.append({QStringLiteral("out"), QStringLiteral("내보내기"), QStringLiteral("out"), {
        b("pdf", "pdf", QStringLiteral("도면"), QStringLiteral("입체 입지도 도면(PNG·PDF)을 만듭니다 (Ctrl+L)")),
        b("print", "print", QStringLiteral("인쇄"), QStringLiteral("도면을 인쇄합니다 (Ctrl+P)")),
        b("image", "geotiff", QStringLiteral("이미지"), QStringLiteral("지금 3D 화면을 그림 파일로 저장합니다")),
        b("export_convert", "export_convert", QStringLiteral("검수·제출"), QStringLiteral("검수·제출(5179 묶음)은 Strata 에서 합니다")),
    }});
    // 기타: Strata 「더보기」 그대로(맞추기·OSM 배경·Google 위성은 뺀다).
    g.append({QStringLiteral("more"), QStringLiteral("기타"), QStringLiteral("more"), {
        b("more", "more", QStringLiteral("더보기"), QStringLiteral("불러오기 · 화면 · 계정과 키 · 상태와 정보"), false, true),
    }});
    return g;
  }();
  return table;
}

const Button* find(const QString& buttonId) {
  for (const Group& g : groups())
    for (const Button& b : g.buttons)
      if (b.id == buttonId) return &b;
  return nullptr;
}

}  // namespace ribbon
