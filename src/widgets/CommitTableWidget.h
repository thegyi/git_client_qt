#ifndef COMMITTABLEWIDGET_H
#define COMMITTABLEWIDGET_H

#include <QPixmap>
#include <QTableWidget>

class CommitTableWidget : public QTableWidget {
  Q_OBJECT

public:
  explicit CommitTableWidget(QWidget *parent = nullptr);

  static QPixmap commitGraphPixmap(const QString &graph, int rowHeight,
                                   const QFont &font);
};

#endif // COMMITTABLEWIDGET_H
