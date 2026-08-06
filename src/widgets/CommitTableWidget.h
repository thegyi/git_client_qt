#ifndef COMMITTABLEWIDGET_H
#define COMMITTABLEWIDGET_H

#include <QColor>
#include <QHash>
#include <QPixmap>
#include <QStringList>
#include <QTableWidget>

class CommitTableWidget : public QTableWidget {
  Q_OBJECT

public:
  explicit CommitTableWidget(QWidget *parent = nullptr);

  static QPixmap commitGraphPixmap(const QString &graph, QString &prevGraph,
                                   QHash<int, QColor> &columnColors,
                                   int rowHeight, const QFont &font,
                                   const QStringList &tags = QStringList());

  // QGit-style lane graph painter. Replaces commitGraphPixmap for
  // the lane-based renderer.
  static QPixmap paintLanes(const QVector<int> &lanes, int rowHeight,
                            const QFont &font, const QBrush &backBrush,
                            const QStringList &tags = QStringList());
};

#endif // COMMITTABLEWIDGET_H
