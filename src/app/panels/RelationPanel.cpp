#include "RelationPanel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMap>
#include <QSignalBlocker>
#include <QSplitter>
#include <QLabel>
#include <QTableWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "KaTheme.h"

namespace {

enum Col { Name, Kind, Distance, Bearing, Height, Period, Tier, ColCount };

QTableWidgetItem* cell(const QString& text, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
  auto* it = new QTableWidgetItem(text);
  it->setTextAlignment(align);
  it->setFlags(it->flags() & ~Qt::ItemIsEditable);
  return it;
}

QTableWidgetItem* number(double v, int decimals, const QString& unit) {
  auto* it = cell(QStringLiteral("%1 %2").arg(v, 0, 'f', decimals).arg(unit), Qt::AlignRight | Qt::AlignVCenter);
  it->setData(Qt::UserRole, v);  // 정렬은 숫자로
  return it;
}

// OSM 원래 태그(historic=yes 등)는 사람이 읽는 말로
QString kindText(const strata3d::NearbyItem& it) {
  if (it.category.contains(QLatin1Char('='))) return it.source == QStringLiteral("OSM") ? QStringLiteral("OSM 유적") : QStringLiteral("유적");
  return it.category;
}

// 관계 표 분류: 국가유산청 지정·등록 유산은 기본, 조사·분포 자료(수백 건)는 따로 고른다
QString rowGroup(const strata3d::RelationRow& r) {
  const QString& c = r.item.category;
  for (const char16_t* g : {u"발굴유적위치도", u"지표유적위치도", u"문화유적분포지도"})
    if (c == QString::fromUtf16(g)) return c;
  return QStringLiteral("지정·등록 유산");
}

}  // namespace

RelationPanel::RelationPanel(QWidget* parent) : QWidget(parent) {
  auto* lay = new QVBoxLayout(this);
  lay->setContentsMargins(8, 6, 8, 6);
  lay->setSpacing(4);
  title_ = new QLabel(this);
  title_->setWordWrap(true);
  // 구역 줄·노트는 줄바꿈 라벨로 두면 도크가 낮을 때 높이 계산이 어긋나 표 위에 겹친다 → 스크롤되는 읽기 전용 상자
  auto plainBox = [this](int maxH) {
    auto* b = new QTextBrowser(this);
    b->setFrameShape(QFrame::NoFrame);
    b->setOpenLinks(false);
    b->setMaximumHeight(maxH);
    b->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    b->viewport()->setAutoFillBackground(false);
    return b;
  };
  // 아래 도크는 넓고 낮다 → 구역 판정(왼쪽)과 표(오른쪽)를 나란히 둔다. 구역 6줄이 한눈에 보이는 것이 먼저다.
  zones_ = plainBox(QWIDGETSIZE_MAX);
  zones_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  table_ = new QTableWidget(0, ColCount, this);
  table_->setHorizontalHeaderLabels({QStringLiteral("유산"), QStringLiteral("종목"), QStringLiteral("거리"), QStringLiteral("방위"),
                                     QStringLiteral("높이차"), QStringLiteral("시대"), QStringLiteral("위치 등급")});
  table_->verticalHeader()->setVisible(false);
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->setSelectionMode(QAbstractItemView::SingleSelection);
  table_->setAlternatingRowColors(true);
  table_->setSortingEnabled(false);
  table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(Name, QHeaderView::Stretch);
  notes_ = plainBox(KaTheme::scaledUiPx(40));  // 노트는 두 줄만 보이고 나머지는 스크롤(구역 판정이 자리를 먼저 받는다)
  notes_->setStyleSheet(QStringLiteral("color:%1; font-size:%2px").arg(KaTheme::tokens().inkMuted.name()).arg(KaTheme::scaledUiPx(11)));
  filter_ = new QComboBox(this);
  filter_->setMinimumWidth(KaTheme::scaledUiPx(200));
  filter_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  connect(filter_, &QComboBox::currentIndexChanged, this, [this] { fillRows(); });
  auto* head = new QHBoxLayout;
  head->addWidget(title_, 1);
  head->addWidget(filter_);
  auto* left = new QWidget(this);
  auto* leftLay = new QVBoxLayout(left);
  leftLay->setContentsMargins(0, 0, 0, 0);
  leftLay->addWidget(zones_, 1);
  leftLay->addWidget(notes_);
  auto* split = new QSplitter(Qt::Horizontal, this);
  split->addWidget(left);
  split->addWidget(table_);
  split->setStretchFactor(0, 2);
  split->setStretchFactor(1, 3);
  split->setChildrenCollapsible(false);
  lay->addLayout(head);
  lay->addWidget(split, 1);
  // 도크가 이보다 낮아지지 않게(구역 6줄 + 노트 두 줄). 안쪽은 좌우로 나눠 두었으므로 눌리지 않는다
  setMinimumHeight(KaTheme::scaledUiPx(300));
  connect(table_, &QTableWidget::itemSelectionChanged, this, [this] {
    const auto rows = table_->selectionModel()->selectedRows();
    if (!rows.isEmpty()) emit heritageChosen(table_->item(rows.front().row(), Name)->text());
  });
  clear(QStringLiteral("조사를 열면 조사구역과 주변 유산의 관계를 계산합니다."));
}

void RelationPanel::setBusy(const QString& message) {
  title_->setText(message);
}

void RelationPanel::clear(const QString& message) {
  last_ = {};
  filter_->clear();
  table_->setRowCount(0);
  title_->setText(message);
  zones_->clear();
  notes_->clear();
}

int RelationPanel::rowCount() const {
  return table_->rowCount();
}

void RelationPanel::setTable(const strata3d::RelationTable& t, const QString& siteLabel) {
  using strata3d::tierLabel;
  using strata3d::tierLetter;
  title_->setText(QStringLiteral("기준: %1 · 반경 %2 m 안 %3곳 · 조사구역 표고 %4 m")
                      .arg(siteLabel)
                      .arg(t.radiusM, 0, 'f', 0)
                      .arg(t.rows.size())
                      .arg(t.siteTerrain.elevationM, 0, 'f', 1));
  // 구역: 분류마다 한 줄. 겹치면 「안」, 아니면 가장 가까운 거리
  QStringList zoneLines;
  for (const QString& layer : strata3d::zoneLayerNames()) {
    std::vector<const strata3d::ZoneHit*> hits;
    for (const auto& z : t.zones)
      if (z.layer == layer) hits.push_back(&z);
    if (hits.empty()) continue;
    // 겹치는 구역(조각)이 여럿이면 이름만 모아 앞의 셋을 보인다. 현상변경허용기준은 조각이 수천 개라 한 줄로 묶는다.
    int overlapping = 0;
    QStringList names, zones;
    for (const auto* z : hits) {
      if (!z->overlaps) break;  // 엔진이 겹침을 앞에 둔다
      ++overlapping;
      if (!z->name.isEmpty() && !names.contains(z->name)) names << z->name;
      if (!z->zone.isEmpty() && !zones.contains(z->zone)) zones << z->zone;
    }
    const auto* first = hits.front();
    QString state, who;
    if (overlapping > 0 && !zones.isEmpty()) {
      // 현상변경허용기준: 같은 구역 도형이 유산마다 등록되므로 「구역 이름들 · 유산 n건」으로 보인다
      zones.sort();
      state = QStringLiteral("<b>안</b> — %1 · 기준 유산 %2건").arg(zones.join(QStringLiteral(", ")).toHtmlEscaped()).arg(names.size());
    } else if (overlapping > 0) {
      state = QStringLiteral("<b>안</b> (겹치는 조각 %1)").arg(overlapping);
      QStringList shown = names.mid(0, 3);
      for (QString& n : shown) n = n.toHtmlEscaped();
      if (!shown.isEmpty()) who = QStringLiteral(" — %1%2").arg(shown.join(QStringLiteral(", ")),
                                                              names.size() > 3 ? QStringLiteral(" 외 %1").arg(names.size() - 3) : QString());
    } else {
      state = QStringLiteral("밖, 가장 가까운 곳 %1 m").arg(first->distanceM, 0, 'f', 0);
      if (!first->name.isEmpty()) who = QStringLiteral(" — %1").arg(first->name.toHtmlEscaped());
    }
    zoneLines << QStringLiteral("%1: %2%3 · 반경 %4 m 안 %5조각").arg(layer, state, who).arg(t.radiusM, 0, 'f', 0).arg(hits.size());
  }
  zones_->setHtml(zoneLines.isEmpty() ? QStringLiteral("구역 자료 없음(Strata 에서 주변유적 자료를 받으면 보호구역·현상변경허용기준·유존지역을 판정합니다)")
                                      : zoneLines.join(QStringLiteral("<br>")));
  last_ = t;
  // 분류 목록과 건수. 고른 분류는 유지한다(조사를 다시 계산해도)
  const QString keep = filter_->currentData().toString();
  QMap<QString, int> counts;
  for (const auto& r : t.rows) ++counts[rowGroup(r)];
  const QSignalBlocker block(filter_);
  filter_->clear();
  const QString designated = QStringLiteral("지정·등록 유산");
  for (const QString& g : {designated, QStringLiteral("발굴유적위치도"), QStringLiteral("지표유적위치도"), QStringLiteral("문화유적분포지도")})
    if (counts.contains(g) || g == designated) filter_->addItem(QStringLiteral("%1 %2곳").arg(g).arg(counts.value(g)), g);
  filter_->addItem(QStringLiteral("전체 %1곳").arg(t.rows.size()), QStringLiteral("*"));
  const int idx = filter_->findData(keep.isEmpty() ? designated : keep);
  filter_->setCurrentIndex(idx < 0 ? 0 : idx);
  fillRows();
  notes_->setPlainText(t.notes.join(QLatin1Char('\n')));
}

void RelationPanel::fillRows() {
  using strata3d::tierLabel;
  using strata3d::tierLetter;
  const QString want = filter_->currentData().toString();
  std::vector<const strata3d::RelationRow*> rows;
  for (const auto& r : last_.rows)
    if (want == QLatin1String("*") || rowGroup(r) == want) rows.push_back(&r);
  table_->setRowCount(int(rows.size()));
  for (int i = 0; i < int(rows.size()); ++i) {
    const auto& r = *rows[size_t(i)];
    table_->setItem(i, Name, cell(r.item.name));
    table_->setItem(i, Kind, cell(r.pieces > 1 ? QStringLiteral("%1 · %2조각").arg(kindText(r.item)).arg(r.pieces) : kindText(r.item)));
    table_->setItem(i, Distance, number(r.item.distanceM, 0, QStringLiteral("m")));
    table_->setItem(i, Bearing, cell(QStringLiteral("%1 %2°").arg(r.item.bearingName).arg(r.item.bearingDeg, 0, 'f', 0)));
    table_->setItem(i, Height, number(r.item.deltaElevM, 1, QStringLiteral("m")));
    table_->setItem(i, Period, cell(r.period.isEmpty() ? QStringLiteral("자료 없음") : r.period));
    table_->setItem(i, Tier, cell(QStringLiteral("%1 %2").arg(tierLetter(r.positionTier)).arg(tierLabel(r.positionTier))));
  }
}
