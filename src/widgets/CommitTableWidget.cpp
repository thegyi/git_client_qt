#include "CommitTableWidget.h"
#include "Lanes.h"
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
  setSelectionMode(QAbstractItemView::ExtendedSelection);
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
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  setAlternatingRowColors(true);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QPixmap CommitTableWidget::commitGraphPixmap(const QString &graph,
                                             QString &prevGraph,
                                             QHash<int, QColor> &columnColors,
                                             int rowHeight, const QFont &font,
                                             const QStringList &tags) {
  const QFontMetrics fm(font);
  const int charWidth = qMax(8, fm.horizontalAdvance(QLatin1Char('M')));
  const int h = qMax(16, rowHeight);
  const int graphWidth = qMax(charWidth, graph.length() * charWidth);

  QFont labelFont(font);
  if (labelFont.pointSizeF() > 0)
    labelFont.setPointSizeF(qMax(6.0, labelFont.pointSizeF() - 1.0));
  labelFont.setBold(true);
  const QFontMetrics labelFm(labelFont);
  const int labelPadding = 6;
  const int labelSpacing = 4;
  const int labelHeight = qMax(12, qMin(h - 6, labelFm.height() + 4));

  QList<int> labelWidths;
  int labelsWidth = 0;
  for (const QString &tag : tags) {
    const int tagWidth = labelFm.horizontalAdvance(tag) + labelPadding * 2;
    labelWidths.append(tagWidth);
    labelsWidth += tagWidth + labelSpacing;
  }
  if (labelsWidth > 0)
    labelsWidth += labelSpacing;

  const int w = graphWidth + labelsWidth;

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

  const int dotRadius = qMin(4, h / 6);
  const int midX = charWidth / 2;
  const int midY = h / 2;

  // Propagate branch colors from the previous row.
  const QHash<int, QColor> prevColors = columnColors;
  QHash<int, QColor> currentColors;
  for (int j = 0; j < prevGraph.length(); ++j) {
    const QChar pc = prevGraph.at(j);
    if (pc == QChar(' ') || !prevColors.contains(j))
      continue;
    if (pc == QLatin1Char('|') || pc == QLatin1Char('*') ||
        pc == QChar(0x25CF) || pc == QLatin1Char('.')) {
      currentColors[j] = prevColors[j];
    }
  }
  for (int j = 0; j < prevGraph.length(); ++j) {
    const QChar pc = prevGraph.at(j);
    if (pc == QChar(' ') || !prevColors.contains(j))
      continue;
    if (pc == QLatin1Char('\\') && !currentColors.contains(j + 1))
      currentColors[j + 1] = prevColors[j];
    if (pc == QLatin1Char('/') && !currentColors.contains(j - 1))
      currentColors[j - 1] = prevColors[j];
  }
  columnColors = currentColors;

  for (int i = 0; i < graph.length(); ++i) {
    const QChar c = graph.at(i);
    if (c == QLatin1Char(' '))
      continue;

    const int x = i * charWidth + midX;
    if (!columnColors.contains(i))
      columnColors[i] =
          branchColors[i % (sizeof(branchColors) / sizeof(branchColors[0]))];
    const QColor color = columnColors[i];
    QPen pen(color);
    pen.setWidthF(2.0);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);

    if (c == QLatin1Char('*') || c == QChar(0x25CF)) {
      const int dotTop = qMax(0, midY - dotRadius);
      const int dotBottom = qMin(h, midY + dotRadius);
      painter.drawLine(x, 0, x, dotTop);
      painter.drawLine(x, dotBottom, x, h);
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

  if (!labelWidths.isEmpty()) {
    painter.setFont(labelFont);
    const int labelY = (h - labelHeight) / 2;
    int labelX = graphWidth + labelSpacing;
    for (int i = 0; i < labelWidths.size(); ++i) {
      const QRect chip(labelX, labelY, labelWidths.at(i), labelHeight);
      painter.setPen(QPen(QColor(0xb8, 0x7a, 0x0d)));
      painter.setBrush(QColor(0xf5, 0xc7, 0x4a));
      painter.drawRoundedRect(chip, 4, 4);
      painter.setBrush(Qt::NoBrush);
      painter.setPen(QColor(0x33, 0x2b, 0x00));
      painter.drawText(chip, Qt::AlignCenter, tags.at(i));
      labelX += labelWidths.at(i) + labelSpacing;
    }
  }

  prevGraph = graph;

  return pixmap;
}

static void paintGraphLane(QPainter &p, int type, int x1, int x2, int h,
                           const QColor &color, const QBrush &back) {
  if (type == Lane::EMPTY)
    return;

  const int m = (x1 + x2) / 2;
  const int midY = h / 2;
  const int r = qMax(3, qMin(5, qMin(x2 - x1, h) / 3));
  const int d = 2 * r;

  const QPoint P_0(x2, midY);
  const QPoint P_90(m, 0);
  const QPoint P_180(x1, midY);
  const QPoint P_270(m, h);
  const QPoint P_CENTER(m, midY);
  const QRect R_CENTER(m - r, midY - r, d, d);

  QPen pen(color);
  pen.setWidthF(2.0);
  pen.setCapStyle(Qt::RoundCap);
  p.setPen(pen);

  // vertical line
  switch (type) {
  case Lane::ACTIVE:
  case Lane::NOT_ACTIVE:
  case Lane::MERGE_FORK:
  case Lane::MERGE_FORK_R:
  case Lane::MERGE_FORK_L:
  case Lane::JOIN:
  case Lane::JOIN_R:
  case Lane::JOIN_L:
    p.drawLine(P_90, P_270);
    break;
  case Lane::HEAD:
  case Lane::HEAD_R:
  case Lane::HEAD_L:
  case Lane::BRANCH:
    p.drawLine(P_CENTER, P_270);
    break;
  case Lane::TAIL:
  case Lane::TAIL_R:
  case Lane::TAIL_L:
  case Lane::INITIAL:
  case Lane::BOUNDARY:
  case Lane::BOUNDARY_C:
  case Lane::BOUNDARY_R:
  case Lane::BOUNDARY_L:
    p.drawLine(P_90, P_CENTER);
    break;
  default:
    break;
  }

  // horizontal line
  switch (type) {
  case Lane::MERGE_FORK:
  case Lane::JOIN:
  case Lane::HEAD:
  case Lane::TAIL:
  case Lane::CROSS:
  case Lane::CROSS_EMPTY:
  case Lane::BOUNDARY_C:
    p.drawLine(P_180, P_0);
    break;
  case Lane::MERGE_FORK_R:
  case Lane::JOIN_R:
  case Lane::HEAD_R:
  case Lane::TAIL_R:
  case Lane::BOUNDARY_R:
    p.drawLine(P_180, P_CENTER);
    break;
  case Lane::MERGE_FORK_L:
  case Lane::JOIN_L:
  case Lane::HEAD_L:
  case Lane::TAIL_L:
  case Lane::BOUNDARY_L:
    p.drawLine(P_CENTER, P_0);
    break;
  default:
    break;
  }

  // center symbol
  switch (type) {
  case Lane::ACTIVE:
  case Lane::INITIAL:
  case Lane::BRANCH:
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(R_CENTER);
    p.setBrush(Qt::NoBrush);
    p.setPen(pen);
    break;
  case Lane::MERGE_FORK:
  case Lane::MERGE_FORK_R:
  case Lane::MERGE_FORK_L:
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawRect(R_CENTER);
    p.setBrush(Qt::NoBrush);
    p.setPen(pen);
    break;
  case Lane::UNAPPLIED:
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 0, 0));
    p.drawRect(m - r, midY - 1, d, 2);
    p.setBrush(Qt::NoBrush);
    p.setPen(pen);
    break;
  case Lane::APPLIED:
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 160, 0));
    p.drawRect(m - r, midY - 1, d, 2);
    p.drawRect(m - 1, midY - r, 2, d);
    p.setBrush(Qt::NoBrush);
    p.setPen(pen);
    break;
  case Lane::BOUNDARY:
    p.setPen(Qt::NoPen);
    p.setBrush(back);
    p.drawEllipse(R_CENTER);
    p.setBrush(Qt::NoBrush);
    p.setPen(pen);
    break;
  case Lane::BOUNDARY_C:
  case Lane::BOUNDARY_R:
  case Lane::BOUNDARY_L:
    p.setPen(Qt::NoPen);
    p.setBrush(back);
    p.drawRect(R_CENTER);
    p.setBrush(Qt::NoBrush);
    p.setPen(pen);
    break;
  default:
    break;
  }
}

QPixmap CommitTableWidget::paintLanes(const QVector<int> &lanes, int rowHeight,
                                      const QFont &font,
                                      const QBrush &backBrush,
                                      const QStringList &tags) {
  const QFontMetrics fm(font);
  const int laneWidth = qMax(10, fm.horizontalAdvance(QLatin1Char('M')));
  const int h = qMax(16, rowHeight);
  const int graphWidth = lanes.count() * laneWidth;

  QFont labelFont(font);
  if (labelFont.pointSizeF() > 0)
    labelFont.setPointSizeF(qMax(6.0, labelFont.pointSizeF() - 1.0));
  labelFont.setBold(true);
  const QFontMetrics labelFm(labelFont);
  const int labelPadding = 6;
  const int labelSpacing = 4;
  const int labelHeight = qMax(12, qMin(h - 6, labelFm.height() + 4));

  QList<int> labelWidths;
  int labelsWidth = 0;
  for (const QString &tag : tags) {
    const int tagWidth = labelFm.horizontalAdvance(tag) + labelPadding * 2;
    labelWidths.append(tagWidth);
    labelsWidth += tagWidth + labelSpacing;
  }
  if (labelsWidth > 0)
    labelsWidth += labelSpacing;

  const int w = graphWidth + labelsWidth;

  QPixmap pixmap(w, h);
  pixmap.fill(backBrush.style() == Qt::NoBrush ? Qt::transparent
                                               : backBrush.color());

  QPainter p(&pixmap);
  p.setRenderHint(QPainter::Antialiasing);
  p.setFont(font);

  const QColor branchColors[] = {
      QColor(0xf2, 0x91, 0x11), QColor(0x4f, 0xc3, 0xf7),
      QColor(0x7e, 0xd3, 0x21), QColor(0xe9, 0x1e, 0x63),
      QColor(0x9c, 0x27, 0xb0), QColor(0x00, 0x96, 0x88),
      QColor(0x21, 0x96, 0xf3), QColor(0xff, 0x57, 0x22),
  };
  const int colorsCount = sizeof(branchColors) / sizeof(branchColors[0]);

  // The active/merge lane is the first lane that is not just a line.
  int mergeLane = 0;
  for (int i = 0; i < lanes.count(); ++i) {
    const int t = lanes[i];
    if (t == Lane::ACTIVE || t == Lane::BRANCH || t == Lane::INITIAL ||
        t == Lane::APPLIED || t == Lane::BOUNDARY || t == Lane::MERGE_FORK ||
        t == Lane::MERGE_FORK_R || t == Lane::MERGE_FORK_L || t == Lane::JOIN ||
        t == Lane::JOIN_R || t == Lane::JOIN_L) {
      mergeLane = i;
      break;
    }
  }

  for (int i = 0; i < lanes.count(); ++i) {
    const int ln = lanes[i];
    const int x1 = i * laneWidth;
    const int x2 = x1 + laneWidth;

    const int colIdx = (Lane::isHead(ln) || Lane::isTail(ln) ||
                        Lane::isJoin(ln) || ln == Lane::CROSS_EMPTY)
                           ? mergeLane
                           : i;
    const QColor color = branchColors[colIdx % colorsCount];

    if (ln == Lane::CROSS) {
      paintGraphLane(p, Lane::NOT_ACTIVE, x1, x2, h, color, backBrush);
      paintGraphLane(p, Lane::CROSS, x1, x2, h,
                     branchColors[mergeLane % colorsCount], backBrush);
    } else if (ln == Lane::CROSS_EMPTY) {
      paintGraphLane(p, Lane::CROSS_EMPTY, x1, x2, h,
                     branchColors[mergeLane % colorsCount], backBrush);
    } else {
      paintGraphLane(p, ln, x1, x2, h, color, backBrush);
    }
  }

  if (!labelWidths.isEmpty()) {
    p.setFont(labelFont);
    const int labelY = (h - labelHeight) / 2;
    int labelX = graphWidth + labelSpacing;
    for (int i = 0; i < labelWidths.size(); ++i) {
      const QRect chip(labelX, labelY, labelWidths.at(i), labelHeight);
      p.setPen(QPen(QColor(0xb8, 0x7a, 0x0d)));
      p.setBrush(QColor(0xf5, 0xc7, 0x4a));
      p.drawRoundedRect(chip, 4, 4);
      p.setBrush(Qt::NoBrush);
      p.setPen(QColor(0x33, 0x2b, 0x00));
      p.drawText(chip, Qt::AlignCenter, tags.at(i));
      labelX += labelWidths.at(i) + labelSpacing;
    }
  }

  return pixmap;
}
