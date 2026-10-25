#include "BangriDockPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "KaTheme.h"

namespace {

QString colorHex(const QColor& c) { return c.name(); }

}  // namespace

BangriDockPanel::BangriDockPanel(QWidget* parent) : QWidget(parent) {
  presets_ = strata3d::BangriGridEngine::builtInPresets();
  heritageCatalog_ = strata3d::BangriTerrainValidator::gyeongjuHeritageCatalog();
  if (!presets_.empty()) {
    baseParams_ = presets_.front().defaultParams;
  }

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(6, 6, 6, 6);
  root->setSpacing(5);

  // 1. 일반인을 위한 원클릭 경주 핵심 유적 3D 카메라 바로가기 버튼 그룹
  auto* tourBox = new QGroupBox(QStringLiteral("① 보고 싶은 경주 유적 권역 바로가기 (실제 위치로 3D 카메라 이동)"), this);
  auto* tourGrid = new QGridLayout(tourBox);
  tourGrid->setContentsMargins(6, 5, 6, 5);
  tourGrid->setSpacing(4);

  auto* btnHwang = new QPushButton(QStringLiteral("황룡사 9층목탑 · 분황사 모전석탑"), tourBox);
  auto* btnPalace = new QPushButton(QStringLiteral("동궁과 월지 · 월성(왕궁·해자·석빙고)"), tourBox);
  auto* btnTombs = new QPushButton(QStringLiteral("대릉원(황남대총·천마총) · 봉황대 · 금관총"), tourBox);
  auto* btnStreet = new QPushButton(QStringLiteral("국보 첨성대 · 계림 · 내물왕릉 · 월정교"), tourBox);
  auto* btnOverview = new QPushButton(QStringLiteral("경주 신라 왕경 핵심 유적 전체 한눈에 보기"), tourBox);

  tourGrid->addWidget(btnHwang, 0, 0);
  tourGrid->addWidget(btnPalace, 0, 1);
  tourGrid->addWidget(btnTombs, 1, 0);
  tourGrid->addWidget(btnStreet, 1, 1);
  tourGrid->addWidget(btnOverview, 2, 0, 1, 2);
  root->addWidget(tourBox);

  connect(btnHwang, &QPushButton::clicked, this, [this] {
    emit cameraTourRequested(int(CameraTourTarget::HwangnyongsaAndGrid));
  });
  connect(btnPalace, &QPushButton::clicked, this, [this] {
    emit cameraTourRequested(int(CameraTourTarget::PalaceAndWolji));
  });
  connect(btnTombs, &QPushButton::clicked, this, [this] {
    emit cameraTourRequested(int(CameraTourTarget::RoyalTombsDaereung));
  });
  connect(btnStreet, &QPushButton::clicked, this, [this] {
    emit cameraTourRequested(int(CameraTourTarget::StreetCloseUp));
  });
  connect(btnOverview, &QPushButton::clicked, this, [this] {
    emit cameraTourRequested(int(CameraTourTarget::CapitalOverview));
  });

  // 2. 경주 전체 유적 블렌더 3D 모델 개별 켜기/끄기 목록 (사용자가 하나의 유적씩 켜고 끌 수 있음)
  auto* heritageBox = new QGroupBox(
      QStringLiteral("② 경주 유적 3D 모델(도식) 켜기/끄기 — 총 %1곳 · 체크: 표시 · 클릭: 그곳으로 이동")
          .arg(heritageCatalog_.size()),
      this);
  auto* heritageLayout = new QVBoxLayout(heritageBox);
  heritageLayout->setContentsMargins(6, 5, 6, 5);
  heritageLayout->setSpacing(4);

  auto* heritageBtnRow = new QHBoxLayout();
  heritageBtnRow->setSpacing(4);
  auto* btnAllOn = new QPushButton(QStringLiteral("전체 켜기 (%1곳)").arg(heritageCatalog_.size()), heritageBox);
  auto* btnAllOff = new QPushButton(QStringLiteral("전체 끄기"), heritageBox);
  auto* btnSoloSite = new QPushButton(QStringLiteral("선택 유적 1곳만 보기"), heritageBox);
  heritageCountLabel_ = new QLabel(
      QStringLiteral("표시 중: %1/%2").arg(heritageCatalog_.size()).arg(heritageCatalog_.size()), heritageBox);
  heritageBtnRow->addWidget(btnAllOn);
  heritageBtnRow->addWidget(btnAllOff);
  heritageBtnRow->addWidget(btnSoloSite);
  heritageBtnRow->addWidget(heritageCountLabel_);
  heritageLayout->addLayout(heritageBtnRow);

  auto* searchEdit = new QLineEdit(heritageBox);
  searchEdit->setPlaceholderText(QStringLiteral("🔍 경주 전체 유적 이름·분류 검색 (예: 불국사, 석굴암, 감은사, 석탑, 왕릉, 국보, 남산)..."));
  searchEdit->setClearButtonEnabled(true);
  heritageLayout->addWidget(searchEdit);

  heritageList_ = new QListWidget(heritageBox);
  heritageList_->setMinimumHeight(130);
  heritageList_->setMaximumHeight(165);
  heritageList_->setSelectionMode(QAbstractItemView::SingleSelection);
  for (size_t i = 0; i < heritageCatalog_.size(); ++i) {
    const auto& site = heritageCatalog_[i];
    auto* item = new QListWidgetItem(
        QStringLiteral("%1. %2").arg(i + 1, 3, 10, QLatin1Char('0')).arg(site.name),
        heritageList_);
    item->setData(Qt::UserRole, site.id);
    item->setData(Qt::UserRole + 1, site.category + QStringLiteral(" ") + site.subtitle);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    item->setCheckState(Qt::Checked);
    item->setToolTip(QStringLiteral("%1\n· %2\n· 3D 모델(도식, 유형 틀): 삼각형 %3개 (정점 %4개)\n· 체크박스를 끄면 이 유적만 화면에서 숨겨집니다.")
                         .arg(site.name, site.subtitle)
                         .arg(site.blenderTriangleCount)
                         .arg(site.blenderVertexCount));
  }
  heritageLayout->addWidget(heritageList_);
  root->addWidget(heritageBox);

  connect(searchEdit, &QLineEdit::textChanged, this, [this](const QString& q) {
    const QString query = q.trimmed();
    for (int i = 0; i < heritageList_->count(); ++i) {
      QListWidgetItem* it = heritageList_->item(i);
      const bool match = query.isEmpty() ||
                         it->text().contains(query, Qt::CaseInsensitive) ||
                         it->data(Qt::UserRole + 1).toString().contains(query, Qt::CaseInsensitive);
      it->setHidden(!match);
    }
  });

  auto updateHeritageCountLabel = [this] {
    int checked = 0;
    for (int i = 0; i < heritageList_->count(); ++i) {
      if (heritageList_->item(i)->checkState() == Qt::Checked) ++checked;
    }
    if (heritageCountLabel_) {
      heritageCountLabel_->setText(QStringLiteral("표시 중: %1/%2").arg(checked).arg(heritageList_->count()));
    }
  };

  connect(heritageList_, &QListWidget::itemChanged, this, [this, updateHeritageCountLabel](QListWidgetItem*) {
    if (updatingUi_) return;
    updateHeritageCountLabel();
    emit heritageSiteSelectionChanged();
  });
  connect(heritageList_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
    if (!item) return;
    emit heritageSiteFocusRequested(item->data(Qt::UserRole).toString());
  });
  connect(btnAllOn, &QPushButton::clicked, this, [this, updateHeritageCountLabel] {
    setAllHeritageSitesEnabled(true, true);
    updateHeritageCountLabel();
  });
  connect(btnAllOff, &QPushButton::clicked, this, [this, updateHeritageCountLabel] {
    setAllHeritageSitesEnabled(false, true);
    updateHeritageCountLabel();
  });
  connect(btnSoloSite, &QPushButton::clicked, this, [this, updateHeritageCountLabel] {
    QListWidgetItem* cur = heritageList_->currentItem();
    if (!cur && heritageList_->count() > 0) cur = heritageList_->item(0);
    if (!cur) return;
    const QString targetId = cur->data(Qt::UserRole).toString();
    updatingUi_ = true;
    for (int i = 0; i < heritageList_->count(); ++i) {
      QListWidgetItem* it = heritageList_->item(i);
      it->setCheckState(it->data(Qt::UserRole).toString() == targetId ? Qt::Checked : Qt::Unchecked);
    }
    updatingUi_ = false;
    updateHeritageCountLabel();
    emit heritageSiteSelectionChanged();
    emit heritageSiteFocusRequested(targetId);
  });

  // 3. 사용자가 맵에 직접 3D 선을 긋고 3D 격자를 만드는 도구 (그린 뒤 3D 지형과 같이 움직임)
  auto* drawBox = new QGroupBox(QStringLiteral("③ 3D 맵에 선 긋기 && 격자 만들기 (그린 뒤 3D 지형과 함께 회전·이동)"), this);
  auto* drawLayout = new QVBoxLayout(drawBox);
  drawLayout->setContentsMargins(6, 5, 6, 5);
  drawLayout->setSpacing(4);

  auto* drawBtnRow1 = new QHBoxLayout();
  drawBtnRow1->setSpacing(4);
  drawLineBtn_ = new QPushButton(QStringLiteral("✏️ 맵에 3D 선 긋기"), drawBox);
  drawLineBtn_->setCheckable(true);
  drawLineBtn_->setToolTip(QStringLiteral("켜고 3D 지형 위 두 점을 클릭하면 지형 굴곡을 따라 입체 선과 거리·방위각이 그려집니다."));

  drawGridBtn_ = new QPushButton(QStringLiteral("📐 맵 클릭으로 3D 격자 만들기"), drawBox);
  drawGridBtn_->setCheckable(true);
  drawGridBtn_->setToolTip(QStringLiteral("켜고 3D 지형 위 두 점을 클릭하면 두 점 사이에 3D 입체 격자가 생성됩니다."));

  instantGridBtn_ = new QPushButton(QStringLiteral("➕ 현위치 격자 생성"), drawBox);
  instantGridBtn_->setToolTip(QStringLiteral("현재 바라보는 3D 화면 중심에 설정한 칸 크기·칸수로 즉시 3D 격자를 올립니다."));

  drawBtnRow1->addWidget(drawLineBtn_, 1);
  drawBtnRow1->addWidget(drawGridBtn_, 1);
  drawBtnRow1->addWidget(instantGridBtn_);
  drawLayout->addLayout(drawBtnRow1);

  auto* drawGridCfg = new QGridLayout();
  drawGridCfg->setContentsMargins(0, 0, 0, 0);
  drawGridCfg->setSpacing(4);

  userGridWidthSpin_ = new QDoubleSpinBox(drawBox);
  userGridWidthSpin_->setRange(10.0, 1000.0);
  userGridWidthSpin_->setValue(140.0);
  userGridWidthSpin_->setDecimals(0);
  userGridWidthSpin_->setSingleStep(10.0);
  userGridWidthSpin_->setPrefix(QStringLiteral("가로간격 "));
  userGridWidthSpin_->setSuffix(QStringLiteral("m"));

  userGridHeightSpin_ = new QDoubleSpinBox(drawBox);
  userGridHeightSpin_->setRange(10.0, 1000.0);
  userGridHeightSpin_->setValue(140.0);
  userGridHeightSpin_->setDecimals(0);
  userGridHeightSpin_->setSingleStep(10.0);
  userGridHeightSpin_->setPrefix(QStringLiteral("세로간격 "));
  userGridHeightSpin_->setSuffix(QStringLiteral("m"));

  userGridColsSpin_ = new QSpinBox(drawBox);
  userGridColsSpin_->setRange(1, 50);
  userGridColsSpin_->setValue(6);
  userGridColsSpin_->setPrefix(QStringLiteral("가로 "));
  userGridColsSpin_->setSuffix(QStringLiteral("칸"));

  userGridRowsSpin_ = new QSpinBox(drawBox);
  userGridRowsSpin_->setRange(1, 50);
  userGridRowsSpin_->setValue(6);
  userGridRowsSpin_->setPrefix(QStringLiteral("세로 "));
  userGridRowsSpin_->setSuffix(QStringLiteral("칸"));

  userGridAzimuthSpin_ = new QDoubleSpinBox(drawBox);
  userGridAzimuthSpin_->setRange(-90.0, 90.0);
  userGridAzimuthSpin_->setValue(0.0);
  userGridAzimuthSpin_->setDecimals(1);
  userGridAzimuthSpin_->setSingleStep(1.0);
  userGridAzimuthSpin_->setPrefix(QStringLiteral("각도 "));
  userGridAzimuthSpin_->setSuffix(QStringLiteral("°"));

  userDrawLineWidthSpin_ = new QDoubleSpinBox(drawBox);
  userDrawLineWidthSpin_->setRange(1.0, 25.0);
  userDrawLineWidthSpin_->setValue(3.5);
  userDrawLineWidthSpin_->setDecimals(1);
  userDrawLineWidthSpin_->setSingleStep(0.5);
  userDrawLineWidthSpin_->setPrefix(QStringLiteral("굵기 "));
  userDrawLineWidthSpin_->setSuffix(QStringLiteral("m"));

  userGridSubdivCombo_ = new QComboBox(drawBox);
  userGridSubdivCombo_->addItem(QStringLiteral("기본 격자 (보조칸 없음)"), 1);
  userGridSubdivCombo_->addItem(QStringLiteral("十자 4분할 보조격자"), 2);
  userGridSubdivCombo_->addItem(QStringLiteral("井자 9분할 보조격자"), 3);

  userDrawColorCombo_ = new QComboBox(drawBox);
  userDrawColorCombo_->addItem(QStringLiteral("색상: 황금 노랑"), 0);
  userDrawColorCombo_->addItem(QStringLiteral("색상: 하늘 청록"), 1);
  userDrawColorCombo_->addItem(QStringLiteral("색상: 주홍 빨강"), 2);
  userDrawColorCombo_->addItem(QStringLiteral("색상: 형광 연두"), 3);
  userDrawColorCombo_->addItem(QStringLiteral("색상: 보라 자주"), 4);
  userDrawColorCombo_->addItem(QStringLiteral("색상: 순백색"), 5);

  userGridAutoAlignCheck_ = new QCheckBox(QStringLiteral("클릭한 두 점 방향으로 격자 각도 자동 회전"), drawBox);
  userGridAutoAlignCheck_->setChecked(false);

  undoDrawBtn_ = new QPushButton(QStringLiteral("↩ 마지막 취소"), drawBox);
  clearDrawBtn_ = new QPushButton(QStringLiteral("🗑 선·격자 모두 지우기"), drawBox);

  drawGridCfg->addWidget(userGridWidthSpin_, 0, 0);
  drawGridCfg->addWidget(userGridHeightSpin_, 0, 1);
  drawGridCfg->addWidget(userGridColsSpin_, 0, 2);
  drawGridCfg->addWidget(userGridRowsSpin_, 0, 3);

  drawGridCfg->addWidget(userGridAzimuthSpin_, 1, 0);
  drawGridCfg->addWidget(userDrawLineWidthSpin_, 1, 1);
  drawGridCfg->addWidget(userGridSubdivCombo_, 1, 2);
  drawGridCfg->addWidget(userDrawColorCombo_, 1, 3);

  drawGridCfg->addWidget(userGridAutoAlignCheck_, 2, 0, 1, 2);
  drawGridCfg->addWidget(undoDrawBtn_, 2, 2);
  drawGridCfg->addWidget(clearDrawBtn_, 2, 3);
  drawLayout->addLayout(drawGridCfg);

  userDrawStatusLabel_ = new QLabel(
      QStringLiteral("안내: 「✏️ 맵에 3D 선 긋기」 또는 「📐 맵 클릭으로 3D 격자 만들기」를 켜고 지형 위 두 점을 클릭하세요."),
      drawBox);
  userDrawStatusLabel_->setWordWrap(true);
  drawLayout->addWidget(userDrawStatusLabel_);
  root->addWidget(drawBox);

  connect(drawLineBtn_, &QPushButton::toggled, this, [this](bool on) {
    if (updatingUi_) return;
    if (on && drawGridBtn_->isChecked()) {
      updatingUi_ = true;
      drawGridBtn_->setChecked(false);
      updatingUi_ = false;
    }
    emit userDrawModeRequested(on ? 2 : 0);
  });
  connect(drawGridBtn_, &QPushButton::toggled, this, [this](bool on) {
    if (updatingUi_) return;
    if (on && drawLineBtn_->isChecked()) {
      updatingUi_ = true;
      drawLineBtn_->setChecked(false);
      updatingUi_ = false;
    }
    emit userDrawModeRequested(on ? 3 : 0);
  });
  connect(instantGridBtn_, &QPushButton::clicked, this, &BangriDockPanel::addUserGridAtCameraRequested);
  connect(undoDrawBtn_, &QPushButton::clicked, this, &BangriDockPanel::undoUserDrawRequested);
  connect(clearDrawBtn_, &QPushButton::clicked, this, &BangriDockPanel::clearUserDrawRequested);

  auto emitUserStyleEdited = [this] {
    if (updatingUi_) return;
    emit userDrawStyleEdited();
  };
  connect(userGridWidthSpin_, &QDoubleSpinBox::valueChanged, this, emitUserStyleEdited);
  connect(userGridHeightSpin_, &QDoubleSpinBox::valueChanged, this, emitUserStyleEdited);
  connect(userGridColsSpin_, &QSpinBox::valueChanged, this, emitUserStyleEdited);
  connect(userGridRowsSpin_, &QSpinBox::valueChanged, this, emitUserStyleEdited);
  connect(userGridAzimuthSpin_, &QDoubleSpinBox::valueChanged, this, emitUserStyleEdited);
  connect(userDrawLineWidthSpin_, &QDoubleSpinBox::valueChanged, this, emitUserStyleEdited);
  connect(userGridSubdivCombo_, &QComboBox::currentIndexChanged, this, emitUserStyleEdited);
  connect(userDrawColorCombo_, &QComboBox::currentIndexChanged, this, emitUserStyleEdited);

  // 4. 직관적 화면 표시 스위치 & 글씨/창 크기 자유 조절 그룹
  auto* modeBox = new QGroupBox(QStringLiteral("④ 전체 화면 표시 스위치 && 글씨·창 크기 자유 조절"), this);
  auto* modeLayout = new QVBoxLayout(modeBox);
  modeLayout->setContentsMargins(6, 5, 6, 5);
  modeLayout->setSpacing(3);

  auto* checkGrid = new QGridLayout();
  checkGrid->setContentsMargins(0, 0, 0, 0);
  checkGrid->setSpacing(3);

  arch3dCheck_ = new QCheckBox(QStringLiteral("경주 유적 3D 모델(도식) 표시"), modeBox);
  arch3dCheck_->setChecked(true);
  moundCheck_ = new QCheckBox(QStringLiteral("대릉원·봉황대·오릉 왕릉 3D 봉분 표시"), modeBox);
  moundCheck_->setChecked(true);
  labelsCheck_ = new QCheckBox(QStringLiteral("3D 화면 위 유적·측정 이름표 표시"), modeBox);
  labelsCheck_->setChecked(true);
  topBannerCheck_ = new QCheckBox(QStringLiteral("좌측 상단 요약 안내창 표시"), modeBox);
  topBannerCheck_->setChecked(true);
  bareEarthCheck_ = new QCheckBox(QStringLiteral("현대 아파트·상가 숨기기"), modeBox);
  bareEarthCheck_->setChecked(true);
  gridEnabledCheck_ = new QCheckBox(QStringLiteral("(선택) 신라 방리 대로망 겹쳐 보기"), modeBox);
  gridEnabledCheck_->setChecked(false);

  checkGrid->addWidget(arch3dCheck_, 0, 0);
  checkGrid->addWidget(moundCheck_, 0, 1);
  checkGrid->addWidget(labelsCheck_, 1, 0);
  checkGrid->addWidget(topBannerCheck_, 1, 1);
  checkGrid->addWidget(bareEarthCheck_, 2, 0);
  checkGrid->addWidget(gridEnabledCheck_, 2, 1);
  modeLayout->addLayout(checkGrid);

  // 글씨 크기 & 안내창/패널 크기 조절 슬라이더 + [- / +] 버튼
  auto* sizeGrid = new QGridLayout();
  sizeGrid->setContentsMargins(0, 2, 0, 0);
  sizeGrid->setSpacing(4);

  auto* fontMinusBtn = new QPushButton(QStringLiteral("글씨 -"), modeBox);
  auto* fontPlusBtn = new QPushButton(QStringLiteral("글씨 +"), modeBox);
  fontSlider_ = new QSlider(Qt::Horizontal, modeBox);
  fontSlider_->setRange(50, 200);
  fontSlider_->setValue(85);
  fontPctLabel_ = new QLabel(QStringLiteral("85%"), modeBox);
  fontPctLabel_->setMinimumWidth(36);

  auto* boxMinusBtn = new QPushButton(QStringLiteral("창 -"), modeBox);
  auto* boxPlusBtn = new QPushButton(QStringLiteral("창 +"), modeBox);
  boxSlider_ = new QSlider(Qt::Horizontal, modeBox);
  boxSlider_->setRange(50, 200);
  boxSlider_->setValue(85);
  boxPctLabel_ = new QLabel(QStringLiteral("85%"), modeBox);
  boxPctLabel_->setMinimumWidth(36);

  sizeGrid->addWidget(new QLabel(QStringLiteral("글씨 크기:"), modeBox), 0, 0);
  sizeGrid->addWidget(fontMinusBtn, 0, 1);
  sizeGrid->addWidget(fontSlider_, 0, 2);
  sizeGrid->addWidget(fontPlusBtn, 0, 3);
  sizeGrid->addWidget(fontPctLabel_, 0, 4);

  sizeGrid->addWidget(new QLabel(QStringLiteral("창·이름표:"), modeBox), 1, 0);
  sizeGrid->addWidget(boxMinusBtn, 1, 1);
  sizeGrid->addWidget(boxSlider_, 1, 2);
  sizeGrid->addWidget(boxPlusBtn, 1, 3);
  sizeGrid->addWidget(boxPctLabel_, 1, 4);
  modeLayout->addLayout(sizeGrid);

  root->addWidget(modeBox);

  auto onScaleSlidersEdited = [this] {
    if (updatingUi_) return;
    fontPctLabel_->setText(QStringLiteral("%1%").arg(fontSlider_->value()));
    boxPctLabel_->setText(QStringLiteral("%1%").arg(boxSlider_->value()));
    emit uiAndHudScaleChanged(hudFontScale(), hudBoxScale());
  };
  connect(fontSlider_, &QSlider::valueChanged, this, onScaleSlidersEdited);
  connect(boxSlider_, &QSlider::valueChanged, this, onScaleSlidersEdited);
  connect(fontMinusBtn, &QPushButton::clicked, this, [this] { fontSlider_->setValue(fontSlider_->value() - 10); });
  connect(fontPlusBtn, &QPushButton::clicked, this, [this] { fontSlider_->setValue(fontSlider_->value() + 10); });
  connect(boxMinusBtn, &QPushButton::clicked, this, [this] { boxSlider_->setValue(boxSlider_->value() - 10); });
  connect(boxPlusBtn, &QPushButton::clicked, this, [this] { boxSlider_->setValue(boxSlider_->value() + 10); });

  // 5. 도시 선택 및 도면 저장
  auto* cityBox = new QGroupBox(QStringLiteral("⑤ 신라 수도 경주 && 전국 9주 5소경 계획도시 선택"), this);
  auto* cityLayout = new QVBoxLayout(cityBox);
  cityLayout->setContentsMargins(6, 5, 6, 5);
  cityLayout->setSpacing(4);

  presetCombo_ = new QComboBox(cityBox);
  for (const auto& p : presets_) {
    presetCombo_->addItem(QStringLiteral("[%1] %2 — %3").arg(p.categoryLabel, p.name, p.modernRegion), p.id);
  }
  cityLayout->addWidget(presetCombo_);

  auto* topBtns = new QHBoxLayout();
  topBtns->setSpacing(4);
  autoFitBtn_ = new QPushButton(QStringLiteral("실제 유적 자동 정렬"), cityBox);
  applyTrenchBtn_ = new QPushButton(QStringLiteral("추천 발굴지(T1) 단면"), cityBox);
  exportBtn_ = new QPushButton(QStringLiteral("5패널 종합 도면 저장…"), cityBox);
  topBtns->addWidget(autoFitBtn_, 1);
  topBtns->addWidget(applyTrenchBtn_, 1);
  topBtns->addWidget(exportBtn_, 1);
  cityLayout->addLayout(topBtns);
  root->addWidget(cityBox);

  // 6. 일반인 눈높이 해설 + 학술 검증 요약 창
  summaryBrowser_ = new QTextBrowser(this);
  summaryBrowser_->setOpenExternalLinks(true);
  summaryBrowser_->setMinimumWidth(420);
  summaryBrowser_->setMinimumHeight(130);
  root->addWidget(summaryBrowser_, 1);

  // 5. 상세 치수 조절 (기본 접힘 상태로 두어 해설 창이 넓게 보이도록 하고, 버튼 클릭 시 펼쳐짐)
  auto* toggleParamBtn = new QPushButton(QStringLiteral("⚙️ 전문가용 상세 규격 조절 (블록 크기 · 도로 폭 · 척도) 펼치기 ▼"), this);
  toggleParamBtn->setCheckable(true);
  toggleParamBtn->setChecked(false);
  root->addWidget(toggleParamBtn);

  auto* paramBox = new QGroupBox(QStringLiteral("④ 상세 도시 규격 조절 (블록 크기 · 도로 폭 · 고대 척도)"), this);
  paramBox->setVisible(false);
  connect(toggleParamBtn, &QPushButton::toggled, paramBox, [toggleParamBtn, paramBox](bool on) {
    paramBox->setVisible(on);
    toggleParamBtn->setText(on ? QStringLiteral("⚙️ 전문가용 상세 규격 조절 닫기 ▲")
                               : QStringLiteral("⚙️ 전문가용 상세 규격 조절 (블록 크기 · 도로 폭 · 척도) 펼치기 ▼"));
  });
  auto* form = new QFormLayout(paramBox);
  form->setContentsMargins(8, 5, 8, 5);
  form->setSpacing(3);

  scaleCombo_ = new QComboBox(paramBox);
  scaleCombo_->addItem(QStringLiteral("고구려척 (1척 ≈ 35.6 cm)"), int(strata3d::BangriScaleUnit::GoguryeoChi));
  scaleCombo_->addItem(QStringLiteral("당대척 (1척 ≈ 29.6 cm · 1보=5척)"), int(strata3d::BangriScaleUnit::TangLargeChi));
  scaleCombo_->addItem(QStringLiteral("미터 (1.0 m)"), int(strata3d::BangriScaleUnit::Meter));
  form->addRow(QStringLiteral("고대 척도"), scaleCombo_);

  auto* blockRow = new QWidget(paramBox);
  auto* blockLayout = new QHBoxLayout(blockRow);
  blockLayout->setContentsMargins(0, 0, 0, 0);
  blockLayout->setSpacing(4);
  blockWidthSpin_ = new QDoubleSpinBox(blockRow);
  blockWidthSpin_->setRange(60.0, 320.0);
  blockWidthSpin_->setDecimals(1);
  blockWidthSpin_->setSingleStep(1.0);
  blockWidthSpin_->setSuffix(QStringLiteral(" m"));
  blockHeightSpin_ = new QDoubleSpinBox(blockRow);
  blockHeightSpin_->setRange(60.0, 320.0);
  blockHeightSpin_->setDecimals(1);
  blockHeightSpin_->setSingleStep(1.0);
  blockHeightSpin_->setSuffix(QStringLiteral(" m"));
  blockLayout->addWidget(new QLabel(QStringLiteral("가로"), blockRow));
  blockLayout->addWidget(blockWidthSpin_, 1);
  blockLayout->addWidget(new QLabel(QStringLiteral("세로"), blockRow));
  blockLayout->addWidget(blockHeightSpin_, 1);
  form->addRow(QStringLiteral("1블록(방) 크기"), blockRow);

  blockChiLabel_ = new QLabel(paramBox);
  form->addRow(QStringLiteral("척도 환산"), blockChiLabel_);

  subdivCombo_ = new QComboBox(paramBox);
  subdivCombo_->addItem(QStringLiteral("통블록 (내부 골목 분할 없음)"), int(strata3d::BangriSubdivisionMode::None));
  subdivCombo_->addItem(QStringLiteral("十자형 4칸 분할 (동서1·남북1 골목)"), int(strata3d::BangriSubdivisionMode::Cross4));
  subdivCombo_->addItem(QStringLiteral("井자형 9칸 분할 (동서2·남북2 골목 · 왕경 표준)"), int(strata3d::BangriSubdivisionMode::Well9));
  form->addRow(QStringLiteral("블록 내부 분할"), subdivCombo_);

  auto* roadRow = new QWidget(paramBox);
  auto* roadLayout = new QHBoxLayout(roadRow);
  roadLayout->setContentsMargins(0, 0, 0, 0);
  roadLayout->setSpacing(4);
  mainRoadSpin_ = new QDoubleSpinBox(roadRow);
  mainRoadSpin_->setRange(8.0, 35.0);
  mainRoadSpin_->setDecimals(1);
  mainRoadSpin_->setSingleStep(0.5);
  mainRoadSpin_->setPrefix(QStringLiteral("큰길 "));
  mainRoadSpin_->setSuffix(QStringLiteral("m"));

  interRoadSpin_ = new QDoubleSpinBox(roadRow);
  interRoadSpin_->setRange(4.0, 22.0);
  interRoadSpin_->setDecimals(1);
  interRoadSpin_->setSingleStep(0.5);
  interRoadSpin_->setPrefix(QStringLiteral("중간길 "));
  interRoadSpin_->setSuffix(QStringLiteral("m"));

  alleySpin_ = new QDoubleSpinBox(roadRow);
  alleySpin_->setRange(2.5, 12.0);
  alleySpin_->setDecimals(1);
  alleySpin_->setSingleStep(0.25);
  alleySpin_->setPrefix(QStringLiteral("골목 "));
  alleySpin_->setSuffix(QStringLiteral("m"));

  roadLayout->addWidget(mainRoadSpin_, 1);
  roadLayout->addWidget(interRoadSpin_, 1);
  roadLayout->addWidget(alleySpin_, 1);
  form->addRow(QStringLiteral("도로 폭"), roadRow);

  auto* ditchExtentRow = new QWidget(paramBox);
  auto* ditchExtentLayout = new QHBoxLayout(ditchExtentRow);
  ditchExtentLayout->setContentsMargins(0, 0, 0, 0);
  ditchExtentLayout->setSpacing(4);
  azimuthSpin_ = new QDoubleSpinBox(ditchExtentRow);
  azimuthSpin_->setRange(-30.0, 30.0);
  azimuthSpin_->setDecimals(1);
  azimuthSpin_->setSingleStep(0.5);
  azimuthSpin_->setPrefix(QStringLiteral("각도 "));
  azimuthSpin_->setSuffix(QStringLiteral("°"));

  ditchWidthSpin_ = new QDoubleSpinBox(ditchExtentRow);
  ditchWidthSpin_->setRange(0.4, 3.5);
  ditchWidthSpin_->setDecimals(1);
  ditchWidthSpin_->setSingleStep(0.1);
  ditchWidthSpin_->setPrefix(QStringLiteral("배수로 "));
  ditchWidthSpin_->setSuffix(QStringLiteral("m"));

  colsSpin_ = new QSpinBox(ditchExtentRow);
  colsSpin_->setRange(2, 16);
  colsSpin_->setPrefix(QStringLiteral("가로 "));
  colsSpin_->setSuffix(QStringLiteral("블록"));

  rowsSpin_ = new QSpinBox(ditchExtentRow);
  rowsSpin_->setRange(2, 16);
  rowsSpin_->setPrefix(QStringLiteral("세로 "));
  rowsSpin_->setSuffix(QStringLiteral("블록"));

  ditchExtentLayout->addWidget(azimuthSpin_, 1);
  ditchExtentLayout->addWidget(ditchWidthSpin_, 1);
  ditchExtentLayout->addWidget(colsSpin_, 1);
  ditchExtentLayout->addWidget(rowsSpin_, 1);
  form->addRow(QStringLiteral("배수로 · 범위"), ditchExtentRow);

  root->addWidget(paramBox);

  populateFromParams(baseParams_);

  // 시그널 연결
  auto emitTerrainMode = [this] {
    if (updatingUi_) return;
    emit terrainModeChanged(gridEnabledCheck_->isChecked(), bareEarthCheck_->isChecked(), moundCheck_->isChecked());
  };
  connect(arch3dCheck_, &QCheckBox::toggled, this, emitTerrainMode);
  connect(labelsCheck_, &QCheckBox::toggled, this, emitTerrainMode);
  connect(topBannerCheck_, &QCheckBox::toggled, this, emitTerrainMode);
  connect(gridEnabledCheck_, &QCheckBox::toggled, this, emitTerrainMode);
  connect(bareEarthCheck_, &QCheckBox::toggled, this, emitTerrainMode);
  connect(moundCheck_, &QCheckBox::toggled, this, emitTerrainMode);

  connect(presetCombo_, &QComboBox::currentIndexChanged, this, [this](int idx) {
    if (updatingUi_ || idx < 0 || idx >= int(presets_.size())) return;
    const QString id = presetCombo_->itemData(idx).toString();
    baseParams_ = presets_[size_t(idx)].defaultParams;
    populateFromParams(baseParams_);
    emit presetSelected(id);
  });

  auto onParamWidgetEdited = [this] {
    if (updatingUi_) return;
    syncChiLabels();
    baseParams_ = collectParams();
    emit paramsChanged(baseParams_);
  };
  connect(scaleCombo_, &QComboBox::currentIndexChanged, this, onParamWidgetEdited);
  connect(blockWidthSpin_, &QDoubleSpinBox::valueChanged, this, onParamWidgetEdited);
  connect(blockHeightSpin_, &QDoubleSpinBox::valueChanged, this, onParamWidgetEdited);
  connect(azimuthSpin_, &QDoubleSpinBox::valueChanged, this, onParamWidgetEdited);
  connect(subdivCombo_, &QComboBox::currentIndexChanged, this, onParamWidgetEdited);
  connect(mainRoadSpin_, &QDoubleSpinBox::valueChanged, this, onParamWidgetEdited);
  connect(interRoadSpin_, &QDoubleSpinBox::valueChanged, this, onParamWidgetEdited);
  connect(alleySpin_, &QDoubleSpinBox::valueChanged, this, onParamWidgetEdited);
  connect(ditchWidthSpin_, &QDoubleSpinBox::valueChanged, this, onParamWidgetEdited);
  connect(colsSpin_, &QSpinBox::valueChanged, this, onParamWidgetEdited);
  connect(rowsSpin_, &QSpinBox::valueChanged, this, onParamWidgetEdited);

  connect(autoFitBtn_, &QPushButton::clicked, this, &BangriDockPanel::autoFitRequested);
  connect(exportBtn_, &QPushButton::clicked, this, &BangriDockPanel::exportBangriSheetRequested);
  connect(applyTrenchBtn_, &QPushButton::clicked, this, [this] {
    if (hasRecTrench_) {
      emit applyRecommendedTrenchRequested(recTrenchA_.x, recTrenchA_.y, recTrenchB_.x, recTrenchB_.y);
    }
  });
}

bool BangriDockPanel::gridOverlayEnabled() const {
  return gridEnabledCheck_ && gridEnabledCheck_->isChecked();
}

bool BangriDockPanel::architecture3dEnabled() const {
  return arch3dCheck_ && arch3dCheck_->isChecked();
}

void BangriDockPanel::setCalloutLabelsEnabled(bool on) {
  if (labelsCheck_) labelsCheck_->setChecked(on);  // toggled → emitTerrainMode 로 화면도 바뀐다
}

bool BangriDockPanel::calloutLabelsEnabled() const {
  return labelsCheck_ && labelsCheck_->isChecked();
}

bool BangriDockPanel::topBannerEnabled() const {
  return topBannerCheck_ && topBannerCheck_->isChecked();
}

float BangriDockPanel::hudFontScale() const {
  return fontSlider_ ? float(fontSlider_->value()) / 100.0f : 0.85f;
}

float BangriDockPanel::hudBoxScale() const {
  return boxSlider_ ? float(boxSlider_->value()) / 100.0f : 0.85f;
}

void BangriDockPanel::setHudScales(float fontScale, float boxScale, bool emitSignal) {
  updatingUi_ = true;
  if (fontSlider_) {
    fontSlider_->setValue(std::clamp(int(std::lround(fontScale * 100.0f)), 50, 200));
    if (fontPctLabel_) fontPctLabel_->setText(QStringLiteral("%1%").arg(fontSlider_->value()));
  }
  if (boxSlider_) {
    boxSlider_->setValue(std::clamp(int(std::lround(boxScale * 100.0f)), 50, 200));
    if (boxPctLabel_) boxPctLabel_->setText(QStringLiteral("%1%").arg(boxSlider_->value()));
  }
  updatingUi_ = false;
  if (emitSignal) emit uiAndHudScaleChanged(hudFontScale(), hudBoxScale());
}

bool BangriDockPanel::bareEarthMode() const {
  return bareEarthCheck_ && bareEarthCheck_->isChecked();
}

bool BangriDockPanel::reconstructMounds() const {
  return moundCheck_ && moundCheck_->isChecked();
}

QString BangriDockPanel::currentPresetId() const {
  return presetCombo_ ? presetCombo_->currentData().toString() : baseParams_.presetId;
}

strata3d::BangriParams BangriDockPanel::currentParams() const {
  return collectParams();
}

std::vector<QString> BangriDockPanel::enabledHeritageSiteIds() const {
  std::vector<QString> out;
  if (!heritageList_) return out;
  out.reserve(size_t(heritageList_->count()));
  for (int i = 0; i < heritageList_->count(); ++i) {
    QListWidgetItem* item = heritageList_->item(i);
    if (item && item->checkState() == Qt::Checked) {
      out.push_back(item->data(Qt::UserRole).toString());
    }
  }
  return out;
}

void BangriDockPanel::setHeritageSiteEnabled(const QString& siteId, bool enabled, bool emitSignal) {
  if (!heritageList_) return;
  updatingUi_ = true;
  int checkedCount = 0;
  for (int i = 0; i < heritageList_->count(); ++i) {
    QListWidgetItem* item = heritageList_->item(i);
    if (!item) continue;
    if (item->data(Qt::UserRole).toString().compare(siteId, Qt::CaseInsensitive) == 0) {
      item->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    }
    if (item->checkState() == Qt::Checked) ++checkedCount;
  }
  if (heritageCountLabel_) {
    heritageCountLabel_->setText(QStringLiteral("표시 중: %1/%2").arg(checkedCount).arg(heritageList_->count()));
  }
  updatingUi_ = false;
  if (emitSignal) emit heritageSiteSelectionChanged();
}

void BangriDockPanel::setAllHeritageSitesEnabled(bool enabled, bool emitSignal) {
  if (!heritageList_) return;
  updatingUi_ = true;
  for (int i = 0; i < heritageList_->count(); ++i) {
    if (QListWidgetItem* item = heritageList_->item(i)) {
      item->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    }
  }
  if (heritageCountLabel_) {
    heritageCountLabel_->setText(QStringLiteral("표시 중: %1/%2")
                                     .arg(enabled ? heritageList_->count() : 0)
                                     .arg(heritageList_->count()));
  }
  updatingUi_ = false;
  if (emitSignal) emit heritageSiteSelectionChanged();
}

double BangriDockPanel::userGridCellWidthM() const {
  return userGridWidthSpin_ ? userGridWidthSpin_->value() : 140.0;
}

double BangriDockPanel::userGridCellHeightM() const {
  return userGridHeightSpin_ ? userGridHeightSpin_->value() : 140.0;
}

int BangriDockPanel::userGridCols() const {
  return userGridColsSpin_ ? userGridColsSpin_->value() : 6;
}

int BangriDockPanel::userGridRows() const {
  return userGridRowsSpin_ ? userGridRowsSpin_->value() : 6;
}

double BangriDockPanel::userGridAzimuthDeg() const {
  return userGridAzimuthSpin_ ? userGridAzimuthSpin_->value() : 0.0;
}

bool BangriDockPanel::userGridAutoAlign() const {
  return userGridAutoAlignCheck_ && userGridAutoAlignCheck_->isChecked();
}

float BangriDockPanel::userDrawLineWidthM() const {
  return userDrawLineWidthSpin_ ? float(userDrawLineWidthSpin_->value()) : 3.5f;
}

int BangriDockPanel::userGridSubdivisions() const {
  return userGridSubdivCombo_ ? userGridSubdivCombo_->currentData().toInt() : 1;
}

std::array<float, 4> BangriDockPanel::userDrawColor() const {
  const int idx = userDrawColorCombo_ ? userDrawColorCombo_->currentData().toInt() : 0;
  switch (idx) {
    case 0: return {1.00f, 0.84f, 0.16f, 1.0f}; // 황금 노랑
    case 1: return {0.14f, 0.88f, 1.00f, 1.0f}; // 하늘 청록
    case 2: return {1.00f, 0.28f, 0.20f, 1.0f}; // 주홍 빨강
    case 3: return {0.25f, 0.94f, 0.34f, 1.0f}; // 형광 연두
    case 4: return {0.90f, 0.36f, 1.00f, 1.0f}; // 보라 자주
    default: return {0.98f, 0.98f, 0.98f, 1.0f}; // 순백색
  }
}

void BangriDockPanel::syncDrawToolButtons(int activeDrawMode) {
  updatingUi_ = true;
  if (drawLineBtn_) drawLineBtn_->setChecked(activeDrawMode == 2);
  if (drawGridBtn_) drawGridBtn_->setChecked(activeDrawMode == 3);
  updatingUi_ = false;
}

void BangriDockPanel::setUserDrawStatusText(const QString& text) {
  if (userDrawStatusLabel_) userDrawStatusLabel_->setText(text);
}

void BangriDockPanel::setPresetById(const QString& presetId, bool emitSignals) {
  for (int i = 0; i < presetCombo_->count(); ++i) {
    if (presetCombo_->itemData(i).toString().compare(presetId, Qt::CaseInsensitive) == 0) {
      updatingUi_ = true;
      presetCombo_->setCurrentIndex(i);
      baseParams_ = presets_[size_t(i)].defaultParams;
      populateFromParams(baseParams_);
      updatingUi_ = false;
      if (emitSignals) emit presetSelected(baseParams_.presetId);
      return;
    }
  }
}

void BangriDockPanel::setParams(const strata3d::BangriParams& params, bool emitSignals) {
  baseParams_ = params;
  populateFromParams(baseParams_);
  if (emitSignals) emit paramsChanged(baseParams_);
}

void BangriDockPanel::setBareEarthAndMounds(bool bareEarth, bool mounds, bool emitSignals) {
  updatingUi_ = true;
  if (bareEarthCheck_) bareEarthCheck_->setChecked(bareEarth);
  if (moundCheck_) moundCheck_->setChecked(mounds);
  updatingUi_ = false;
  if (emitSignals) {
    emit terrainModeChanged(gridOverlayEnabled(), bareEarthMode(), reconstructMounds());
  }
}

void BangriDockPanel::syncChiLabels() {
  const auto unit = strata3d::BangriScaleUnit(scaleCombo_->currentData().toInt());
  const double wChi = strata3d::BangriGridEngine::metersToScaleUnit(blockWidthSpin_->value(), unit);
  const double hChi = strata3d::BangriGridEngine::metersToScaleUnit(blockHeightSpin_->value(), unit);
  const double areaSoccer = (blockWidthSpin_->value() * blockHeightSpin_->value()) / 7140.0;
  if (unit == strata3d::BangriScaleUnit::GoguryeoChi) {
    blockChiLabel_->setText(QStringLiteral("고구려척 %1×%2척 (축구장 약 %3개 면적)")
                                .arg(wChi, 0, 'f', 0)
                                .arg(hChi, 0, 'f', 0)
                                .arg(areaSoccer, 0, 'f', 1));
  } else if (unit == strata3d::BangriScaleUnit::TangLargeChi) {
    blockChiLabel_->setText(QStringLiteral("당척 %1×%2척 (축구장 약 %3개 면적)")
                                .arg(wChi, 0, 'f', 0)
                                .arg(hChi, 0, 'f', 0)
                                .arg(areaSoccer, 0, 'f', 1));
  } else {
    blockChiLabel_->setText(QStringLiteral("축구장 약 %1개 면적 (고구려척 %2×%3척)")
                                .arg(areaSoccer, 0, 'f', 1)
                                .arg(blockWidthSpin_->value() / 0.356, 0, 'f', 0)
                                .arg(blockHeightSpin_->value() / 0.356, 0, 'f', 0));
  }
}

void BangriDockPanel::populateFromParams(const strata3d::BangriParams& p) {
  updatingUi_ = true;
  for (int i = 0; i < scaleCombo_->count(); ++i) {
    if (scaleCombo_->itemData(i).toInt() == int(p.scaleUnit)) {
      scaleCombo_->setCurrentIndex(i);
      break;
    }
  }
  blockWidthSpin_->setValue(p.blockWidthM);
  blockHeightSpin_->setValue(p.blockHeightM);
  azimuthSpin_->setValue(p.azimuthDeg);
  for (int i = 0; i < subdivCombo_->count(); ++i) {
    if (subdivCombo_->itemData(i).toInt() == int(p.subdivisionMode)) {
      subdivCombo_->setCurrentIndex(i);
      break;
    }
  }
  mainRoadSpin_->setValue(p.mainRoadWidthM);
  interRoadSpin_->setValue(p.interBlockRoadWidthM);
  alleySpin_->setValue(p.innerAlleyWidthM);
  ditchWidthSpin_->setValue(p.ditchWidthM);
  colsSpin_->setValue(p.extentCols);
  rowsSpin_->setValue(p.extentRows);
  syncChiLabels();
  updatingUi_ = false;
}

strata3d::BangriParams BangriDockPanel::collectParams() const {
  strata3d::BangriParams p = baseParams_;
  p.scaleUnit = strata3d::BangriScaleUnit(scaleCombo_->currentData().toInt());
  p.blockWidthM = blockWidthSpin_->value();
  p.blockHeightM = blockHeightSpin_->value();
  p.azimuthDeg = azimuthSpin_->value();
  p.subdivisionMode = strata3d::BangriSubdivisionMode(subdivCombo_->currentData().toInt());
  p.mainRoadWidthM = mainRoadSpin_->value();
  p.interBlockRoadWidthM = interRoadSpin_->value();
  p.innerAlleyWidthM = alleySpin_->value();
  p.ditchWidthM = ditchWidthSpin_->value();
  p.extentCols = colsSpin_->value();
  p.extentRows = rowsSpin_->value();
  return p;
}

void BangriDockPanel::updateVerificationSummary(const BangriSheetData& d) {
  if (!d.surveyEval.recommendedTrenches.empty()) {
    const auto& t1 = d.surveyEval.recommendedTrenches.front();
    recTrenchA_ = t1.startPt;
    recTrenchB_ = t1.endPt;
    hasRecTrench_ = true;
    applyTrenchBtn_->setEnabled(true);
  } else {
    hasRecTrench_ = false;
    applyTrenchBtn_->setEnabled(false);
  }

  const KaTheme::Tokens& tok = KaTheme::tokens();
  const double fontPt = std::clamp(10.0 * double(hudFontScale()), 7.5, 18.0);

  QString h = QStringLiteral("<body style='color:%1; font-size:%2pt; line-height:1.45;'>")
                  .arg(colorHex(tok.ink))
                  .arg(fontPt, 0, 'f', 1);
  h += QStringLiteral("<h3 style='margin-bottom:4px;'>%1 (%2)</h3>")
           .arg(d.preset.name.toHtmlEscaped(), d.preset.modernRegion.toHtmlEscaped());

  h += QStringLiteral(
           "<p style='margin-top:0px;'><b>■ 경주 신라 핵심 유적 3D 모델 — 등급 D 도식</b><br>"
           "위치는 국가유산청 좌표에 맞췄지만, 건물·탑의 모양은 유형 틀로 만든 도식입니다. 실측이나 보고서 치수로 세운 복원이 아니며 화면에서 회색으로 보입니다.</p>"
           "<ul>"
           "<li><b>황룡사 대가람</b>: 심초석 부근(35.83718°N, 129.23313°E)에 세운 도식 <b>황룡사 9층목탑(높이 80m)</b>과 중금당·동금당·서금당·강당·중문·남문·외곽 회랑</li>"
           "<li><b>분황사 및 구황동 원지</b>: 안산암 벽돌 <b>분황사 모전석탑</b>과 네 귀퉁이 돌사자, 보광전, 구황동 신라 왕경 원지(연못·섬)</li>"
           "<li><b>동궁과 월지 (안압지)</b>: 실제 굴곡진 호안 석축 연못과 <b>삼신산 3개 섬(대도·중도·소도)</b>, 서안 <b>임해전 제1·2·3건물</b></li>"
           "<li><b>월성 (반월성) & 석빙고</b>: 실제 <b>초승달 모양 성곽</b>과 북편 계단식 <b>월성 해자</b>, 왕궁 정전군, <b>석빙고(보물)</b></li>"
           "<li><b>첨성대·월정교·계림·향교</b>: 27단 곡선 원통형 <b>국보 첨성대</b>, 남천 위 주형교각과 남북 중층 문루를 갖춘 <b>월정교</b>, <b>계림·내물왕릉·경주향교·최부자댁</b></li>"
           "<li><b>신라 왕릉군 (%1기)</b>: <b>황남대총 남북 쌍분(높이 23m)</b>, <b>천마총</b>, <b>미추왕릉</b>, <b>봉황대</b>, <b>금관총</b>, <b>오릉</b> 실제 위치 3D 봉분</li>"
           "</ul>")
           .arg(int(d.mounds.size()));

  h += QStringLiteral("<p style='color:%1;'>%2</p>")
           .arg(colorHex(tok.inkMuted), d.preset.historicalSummary.toHtmlEscaped());

  h += QStringLiteral("<h4>■ 실제 발굴 유적과의 일치도 & 추천 발굴 지점</h4>");
  h += QStringLiteral("<ul>"
                      "<li><b>실제 발굴된 신라 도로 유적(%1곳)과의 오차(RMSE)</b>: 평균 <b>%2 m</b> 이내로 정확히 일치</li>"
                      "<li><b>1,300년 뒤 오늘날 도로·골목길로 남은 비율(화석화율)</b>: <b>%3%</b> (%4 m 구간이 지금도 도로·집터 경계로 사용 중)</li>"
                      "<li><b>추천 발굴 트렌치(T1)</b>: 도로와 양옆 돌배수로·담장을 한 번에 확인하도록 길과 <b>90° 수직</b>으로 설계 (폭 2m × 길이 15m)</li>"
                      "</ul>")
           .arg(int(d.anchors.size()))
           .arg(d.anchorRmseM, 0, 'f', 2)
           .arg(d.fossilization.fossilizationScorePct, 0, 'f', 1)
           .arg(d.fossilization.matchedLengthM, 0, 'f', 0);

  h += QStringLiteral("<h4>■ 고증에 사용된 DBpia 학술논문 및 발굴보고서 (전체 200편 중 핵심)</h4>");
  if (!d.citations.empty()) {
    h += QStringLiteral("<ol>");
    for (const auto& c : d.citations) {
      h += QStringLiteral("<li><b>%1</b> (%2, %3, %4)<br>")
               .arg(c.title.toHtmlEscaped(),
                    c.authors.toHtmlEscaped(),
                    c.journal.toHtmlEscaped(),
                    c.year.toHtmlEscaped());
      if (!c.oneLineSummary.isEmpty()) {
        h += QStringLiteral("<span style='color:%1;'>→ %2</span>")
                 .arg(colorHex(tok.inkMuted), c.oneLineSummary.left(150).toHtmlEscaped());
      }
      h += QStringLiteral("</li>");
    }
    h += QStringLiteral("</ol>");
  }
  h += QStringLiteral("</body>");
  summaryBrowser_->setHtml(h);
}
