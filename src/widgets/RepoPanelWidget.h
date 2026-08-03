#ifndef REPOPANELWIDGET_H
#define REPOPANELWIDGET_H

#include <QTreeWidget>

class RepoPanelWidget : public QTreeWidget {
  Q_OBJECT

public:
  explicit RepoPanelWidget(QWidget *parent = nullptr);
};

#endif // REPOPANELWIDGET_H
