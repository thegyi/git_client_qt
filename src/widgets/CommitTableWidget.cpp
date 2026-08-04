#include "CommitTableWidget.h"

#include <QHeaderView>

CommitTableWidget::CommitTableWidget(QWidget *parent) : QTableWidget(parent) {
  setColumnCount(7);
  setHorizontalHeaderLabels({tr("Graph"), tr("Date/Time"), tr("Date"),
                             tr("Commit Message"), tr("Author"), tr("Branches"),
                             tr("SHA")});
  horizontalHeader()->setVisible(false);
  setSelectionBehavior(QAbstractItemView::SelectRows);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setEditTriggers(QAbstractItemView::NoEditTriggers);
  verticalHeader()->setVisible(false);
  horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  setShowGrid(true);
  setStyleSheet(
      QStringLiteral("QTableView { gridline-color: #555555; }"
                     "QTableView::item { border: 1px solid #555555; }"
                     "QTableView::item:alternate { border: 1px solid #555555; }"
                     "QTableView::item:selected { "
                     "background-color: palette(highlight); "
                     "color: palette(highlighted-text); }"));
  setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  setAlternatingRowColors(true);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}
