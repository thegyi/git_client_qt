#ifndef COMMITTABLEWIDGET_H
#define COMMITTABLEWIDGET_H

#include <QTableWidget>

class CommitTableWidget : public QTableWidget {
  Q_OBJECT

public:
  explicit CommitTableWidget(QWidget *parent = nullptr);
};

#endif // COMMITTABLEWIDGET_H
