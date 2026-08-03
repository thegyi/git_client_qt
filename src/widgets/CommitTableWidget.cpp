#include "CommitTableWidget.h"

#include <QHeaderView>

CommitTableWidget::CommitTableWidget(QWidget *parent) : QTableWidget(parent) {
  setColumnCount(7);
  setHorizontalHeaderLabels({tr("Graph"), tr("Date/Time"), tr("Date"),
                             tr("Commit Message"), tr("Author"),
                             tr("Branches"), tr("SHA")});
  horizontalHeader()->setVisible(false);
  setSelectionBehavior(QAbstractItemView::SelectRows);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setEditTriggers(QAbstractItemView::NoEditTriggers);
  verticalHeader()->setVisible(false);
  horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  setShowGrid(true);
  setStyleSheet(QStringLiteral("QTableView { gridline-color: #555555; }"));
  setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  setAlternatingRowColors(true);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}
