#include "CommitTableWidget.h"

#include <QFrame>
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
  horizontalHeader()->setHighlightSections(false);
  setShowGrid(false);
  setFrameShape(QFrame::NoFrame);
  setWordWrap(false);
  verticalHeader()->setDefaultSectionSize(26);
  setStyleSheet(QStringLiteral(
      "QTableView { border: none; outline: none; }"
      "QTableView::item { border: none; padding: 4px 8px; }"
      "QTableView::item:hover { background-color: rgba(127, 127, 127, 40); }"
      "QTableView::item:selected { background-color: palette(highlight); "
      "color: palette(highlighted-text); }"));
  setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  setAlternatingRowColors(true);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}
