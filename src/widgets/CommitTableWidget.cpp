#include "CommitTableWidget.h"
#include "Theme.h"

#include <QFontMetrics>
#include <QFrame>
#include <QHeaderView>
#include <QPainter>
#include <QPen>

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
  setFont(Theme::monospaceFont());
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

QPixmap CommitTableWidget::commitGraphPixmap(const QString &graph,
                                             int rowHeight, const QFont &font) {
  const QFontMetrics fm(font);
  const int charWidth = qMax(8, fm.horizontalAdvance(QLatin1Char('M')));
  const int h = qMax(16, rowHeight);
  const int w = qMax(charWidth, graph.length() * charWidth);

  QPixmap pixmap(w, h);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setFont(font);

  const QColor branchColors[] = {
      QColor(0xf2, 0x91, 0x11), QColor(0x4f, 0xc3, 0xf7),
      QColor(0x7e, 0xd3, 0x21), QColor(0xe9, 0x1e, 0x63),
      QColor(0x9c, 0x27, 0xb0), QColor(0x00, 0x96, 0x88),
      QColor(0x21, 0x96, 0xf3), QColor(0xff, 0x57, 0x22),
  };

  const int dotRadius = qMin(5, h / 4);
  const int midX = charWidth / 2;
  const int midY = h / 2;

  for (int i = 0; i < graph.length(); ++i) {
    const QChar c = graph.at(i);
    if (c == QLatin1Char(' '))
      continue;

    const int x = i * charWidth + midX;
    const QColor color =
        branchColors[i % (sizeof(branchColors) / sizeof(branchColors[0]))];
    QPen pen(color);
    pen.setWidthF(2.0);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);

    if (c == QLatin1Char('*') || c == QChar(0x25CF)) {
      painter.setBrush(color);
      painter.drawEllipse(QPoint(x, midY), dotRadius, dotRadius);
      painter.setBrush(Qt::NoBrush);
    } else if (c == QLatin1Char('|')) {
      painter.drawLine(x, 0, x, h);
    } else if (c == QLatin1Char('/')) {
      painter.drawLine(x - charWidth / 2, h, x + charWidth / 2, 0);
    } else if (c == QLatin1Char('\\')) {
      painter.drawLine(x - charWidth / 2, 0, x + charWidth / 2, h);
    } else if (c == QLatin1Char('_') || c == QLatin1Char('-')) {
      painter.drawLine(x - charWidth / 2, midY, x + charWidth / 2, midY);
    } else if (c == QLatin1Char('.')) {
      painter.drawPoint(x, midY);
    } else {
      painter.drawText(x - charWidth / 2, 0, charWidth, h, Qt::AlignCenter, c);
    }
  }

  return pixmap;
}
