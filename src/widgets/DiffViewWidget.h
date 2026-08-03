#ifndef DIFFVIEWWIDGET_H
#define DIFFVIEWWIDGET_H

#include <QString>
#include <QTextEdit>

class DiffViewWidget : public QTextEdit {
  Q_OBJECT

public:
  explicit DiffViewWidget(QWidget *parent = nullptr);

  void setDiffHtml(const QString &html);
  void showEmpty(const QString &title, const QString &message);
  void showError(const QString &message);
};

#endif // DIFFVIEWWIDGET_H
