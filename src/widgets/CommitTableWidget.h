#ifndef COMMITTABLEWIDGET_H
#define COMMITTABLEWIDGET_H

#include <QPixmap>
#include <QStringList>
#include <QTableWidget>

class CommitTableWidget : public QTableWidget {
  Q_OBJECT

public:
  explicit CommitTableWidget(QWidget *parent = nullptr);

  static QPixmap commitGraphPixmap(const QString &graph, int rowHeight,
                                   const QFont &font,
                                   const QStringList &tags = QStringList());
};

#endif // COMMITTABLEWIDGET_H
