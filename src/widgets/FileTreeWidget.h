#ifndef FILETREEWIDGET_H
#define FILETREEWIDGET_H

#include <QTreeWidget>

class FileTreeWidget : public QTreeWidget {
  Q_OBJECT

public:
  explicit FileTreeWidget(const QString &headerLabel,
                          QWidget *parent = nullptr);

  QString itemPath(QTreeWidgetItem *item) const;
  void addFile(const QString &filePath, const QString &status = QString());

  static QIcon statusIcon(const QString &status);

signals:
  void fileDropped(const QString &path);

protected:
  void startDrag(Qt::DropActions supportedActions) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;
};

#endif // FILETREEWIDGET_H
