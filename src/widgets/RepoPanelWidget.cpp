#include "RepoPanelWidget.h"

RepoPanelWidget::RepoPanelWidget(QWidget *parent) : QTreeWidget(parent) {
  setObjectName(QStringLiteral("repoPanel"));
  setHeaderHidden(true);
  setRootIsDecorated(true);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setMinimumWidth(80);
  setContextMenuPolicy(Qt::CustomContextMenu);
}
