#include "StrataLayerDock.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

#include <qgslayertree.h>
#include <qgslayertreeview.h>
#include <qgsmaplayer.h>
#include <qgsproject.h>

#include "KaLayerInformation.h"
#include "KaLayerListChrome.h"
#include "KaTheme.h"

StrataLayerDock::StrataLayerDock(QgsProject* project, QWidget* parent) : QFrame(parent), project_(project) {
  setObjectName(QStringLiteral("layersCard"));
  auto* lay = new QVBoxLayout(this);
  lay->setContentsMargins(6, 6, 6, 6);
  lay->setSpacing(6);

  // 제목줄: Strata 와 같은 「레이어」 + 글자 단추(전체 끄기/켜기)
  auto* capRow = new QHBoxLayout();
  capRow->setContentsMargins(0, 0, 0, 0);
  capRow->setSpacing(6);
  auto* cap = new QLabel(QStringLiteral("레이어"), this);
  cap->setObjectName(QStringLiteral("cardCaption"));
  capRow->addWidget(cap);
  capRow->addStretch(1);
  checkAll_ = new QToolButton(this);
  checkAll_->setObjectName(QStringLiteral("layerCheckAllBtn"));
  checkAll_->setFocusPolicy(Qt::NoFocus);
  checkAll_->setToolTip(QStringLiteral("레이어 체크를 한 번에 모두 끄거나 켭니다. 하나라도 켜져 있으면 전부 끕니다."));
  KaTheme::followPaperLook(checkAll_);
  connect(checkAll_, &QToolButton::clicked, this, &StrataLayerDock::toggleAll);
  capRow->addWidget(checkAll_);
  lay->addLayout(capRow);

  auto* inner = new QFrame(this);
  inner->setObjectName(QStringLiteral("layersInner"));
  auto* innerLay = new QVBoxLayout(inner);
  innerLay->setContentsMargins(4, 4, 4, 4);

  model_ = new KaLayerInformationModel(project_, false, this);
  // 3D 에서는 순서·이름을 바꾸지 않는다(조사 프로젝트는 Strata 가 쓰는 파일). 켜고 끄기만 한다.
  model_->setFlag(QgsLayerTreeModel::AllowNodeReorder, false);
  model_->setFlag(QgsLayerTreeModel::AllowNodeChangeVisibility, true);
  model_->setFlag(QgsLayerTreeModel::AllowNodeRename, false);
  model_->setAutoCollapseLegendNodes(1);
  view_ = new KaLayerInformationView(inner);
  view_->setObjectName(QStringLiteral("layerTree"));
  view_->setModel(model_);
  KaLayerInformationModel::configureView(view_);
  view_->setSelectionMode(QAbstractItemView::SingleSelection);
  view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  innerLay->addWidget(new KaLayerListChrome(view_, inner));
  innerLay->addWidget(view_, 1);
  innerLay->addWidget(new KaLayerInformationPanel(model_, view_, inner));
  empty_ = new QLabel(QStringLiteral("레이어가 없습니다.\n「열기」로 Strata 조사를 여세요."), inner);
  empty_->setObjectName(QStringLiteral("emptyState"));
  empty_->setAlignment(Qt::AlignCenter);
  empty_->setWordWrap(true);
  innerLay->addWidget(empty_, 1);
  lay->addWidget(inner, 1);

  if (QgsLayerTree* root = project_->layerTreeRoot()) {
    // visibilityChanged 는 트리 안 어느 노드가 바뀌어도 뿌리까지 올라온다(Strata 와 같은 방식)
    connect(root, &QgsLayerTreeNode::visibilityChanged, this, [this](QgsLayerTreeNode*) {
      refreshChrome();
      emit layersVisibilityChanged();
    });
  }
  connect(project_, &QgsProject::layersAdded, this, [this](const QList<QgsMapLayer*>&) { refreshChrome(); });
  connect(project_, &QgsProject::layersRemoved, this, [this](const QStringList&) { refreshChrome(); });
  connect(project_, &QgsProject::cleared, this, [this] { refreshChrome(); });
  connect(view_, &QgsLayerTreeView::currentLayerChanged, this, [this](QgsMapLayer* l) {
    emit currentLayerChanged(l ? l->name() : QString());
  });
  refreshChrome();
}

int StrataLayerDock::layerCount() const {
  QgsLayerTree* root = project_ ? project_->layerTreeRoot() : nullptr;
  return root ? int(root->findLayers().size()) : 0;
}

QSet<QString> StrataLayerDock::hiddenLayerNames() const {
  QSet<QString> out;
  QgsLayerTree* root = project_ ? project_->layerTreeRoot() : nullptr;
  if (!root) return out;
  for (QgsLayerTreeLayer* n : root->findLayers())
    if (n && !n->isVisible() && n->layer()) out.insert(n->layer()->name());
  return out;
}

QSet<QString> StrataLayerDock::hiddenLayerKeys() const {
  QSet<QString> out;
  QgsLayerTree* root = project_ ? project_->layerTreeRoot() : nullptr;
  if (!root) return out;
  for (QgsLayerTreeLayer* n : root->findLayers()) {
    if (!n || n->isVisible() || !n->layer()) continue;
    const QString key = n->layer()->customProperty(QStringLiteral("ka_hgis/layer_key")).toString();
    if (!key.isEmpty()) out.insert(key);
  }
  return out;
}

void StrataLayerDock::toggleAll() {
  QgsLayerTree* root = project_ ? project_->layerTreeRoot() : nullptr;
  if (!root) return;
  const QList<QgsLayerTreeLayer*> layers = root->findLayers();
  if (layers.isEmpty()) return;
  bool anyOn = false;
  for (QgsLayerTreeLayer* n : layers)
    if (n && n->isVisible()) { anyOn = true; break; }
  // 하나라도 켜져 있으면 전부 끈다. 전부 꺼져 있을 때만 전부 켠다(Strata 와 같은 규칙).
  for (QgsLayerTreeNode* n : root->children())
    if (n) n->setItemVisibilityCheckedRecursive(!anyOn);
  refreshChrome();
}

void StrataLayerDock::refreshChrome() {
  const int n = layerCount();
  empty_->setVisible(n == 0);
  view_->setVisible(n > 0);
  checkAll_->setEnabled(n > 0);
  bool anyOn = false;
  if (QgsLayerTree* root = project_ ? project_->layerTreeRoot() : nullptr)
    for (QgsLayerTreeLayer* l : root->findLayers())
      if (l && l->isVisible()) { anyOn = true; break; }
  checkAll_->setText(anyOn ? QStringLiteral("전체 끄기") : QStringLiteral("전체 켜기"));
}
