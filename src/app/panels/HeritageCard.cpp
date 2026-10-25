#include "HeritageCard.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "KaTheme.h"

HeritageCard::HeritageCard(QWidget* parent) : QFrame(parent) {
  setObjectName(QStringLiteral("kaFeatureCard"));
  auto* lay = new QVBoxLayout(this);
  lay->setContentsMargins(12, 10, 12, 10);
  lay->setSpacing(6);
  auto* head = new QHBoxLayout();
  name_ = new QLabel(this);
  name_->setObjectName(QStringLiteral("kaFeatureCardName"));
  name_->setWordWrap(true);
  name_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  kind_ = new QLabel(this);
  kind_->setObjectName(QStringLiteral("kaFeatureCardKind"));
  kind_->setAlignment(Qt::AlignRight | Qt::AlignTop);
  head->addWidget(name_, 1);
  head->addWidget(kind_, 0, Qt::AlignTop);
  lay->addLayout(head);

  auto caption = [this](const QString& text) {
    auto* l = new QLabel(text, this);
    l->setObjectName(QStringLiteral("kaFeatureCardSub"));
    return l;
  };
  auto form = [this] {
    auto* f = new QFormLayout();
    f->setContentsMargins(0, 0, 0, 0);
    f->setHorizontalSpacing(10);
    f->setVerticalSpacing(3);
    f->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    f->setRowWrapPolicy(QFormLayout::WrapLongRows);
    return f;
  };
  recordCaption_ = caption(QStringLiteral("원 기록"));
  lay->addWidget(recordCaption_);
  record_ = form();
  lay->addLayout(record_);
  computedCaption_ = caption(QStringLiteral("3D 계산 (조사구역 기준)"));
  lay->addWidget(computedCaption_);
  computed_ = form();
  lay->addLayout(computed_);
  foot_ = new QLabel(this);
  foot_->setObjectName(QStringLiteral("kaFeatureCardSub"));
  foot_->setWordWrap(true);
  lay->addWidget(foot_);
  clearRecord();
}

void HeritageCard::fill(QFormLayout* form, const Rows& rows) {
  while (form->rowCount() > 0) form->removeRow(0);
  const QColor muted = KaTheme::tokens().inkMuted;
  for (const auto& [k, v] : rows) {
    auto* key = new QLabel(k, this);
    key->setStyleSheet(QStringLiteral("color:%1;").arg(muted.name()));
    auto* val = new QLabel(v, this);
    val->setWordWrap(true);
    val->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(key, val);
  }
}

void HeritageCard::setRecord(const QString& title, const QString& kind, const Rows& record, const Rows& computed,
                             const QString& footnote) {
  name_->setText(title.isEmpty() ? QStringLiteral("(이름 없음)") : title);
  kind_->setText(kind);
  fill(record_, record);
  fill(computed_, computed);
  recordCaption_->setVisible(!record.isEmpty());
  computedCaption_->setVisible(!computed.isEmpty());
  foot_->setText(footnote);
  foot_->setVisible(!footnote.isEmpty());
  show();
}

void HeritageCard::clearRecord() {
  fill(record_, {});
  fill(computed_, {});
  hide();
}
