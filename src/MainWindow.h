#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSet>
#include <QStringList>

class QPoint;
class QPushButton;
class QLineEdit;
class QTextEdit;
class QDockWidget;
class QTreeWidget;
class QTableWidget;
class QTableWidgetItem;
class QTreeWidgetItem;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

QT_BEGIN_NAMESPACE

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

private slots:
  void showBranchContextMenu(const QPoint &pos);
  void showUnstagedContextMenu(const QPoint &pos);
  void showStagedContextMenu(const QPoint &pos);
  void onTagClicked(QTreeWidgetItem *item, int column);
  void onFileClicked(QTreeWidgetItem *item, int column);
  void onInitRepository();
  void onCommitClicked();

private:
  void loadRepository(const QString &path);
  void loadWorkingTree();
  void addFileToTree(QTreeWidget *tree, const QString &filePath,
                     const QString &status = QString());
  QString itemPath(QTreeWidget *tree, QTreeWidgetItem *item) const;
  QString formatDiff(const QStringList &lines) const;
  QStringList runGit(const QString &path, const QStringList &args,
                     int acceptedExitCode = 0) const;
  bool execGit(const QString &path, const QStringList &args,
               QString *output = nullptr) const;
  void loadStashes();
  void updateCommitButton();
  void updateFilter();
  void onCommitSelected(QTableWidgetItem *item);
  void onCommitFileClicked(QTreeWidgetItem *item, int column);
  void onStashClicked(QTreeWidgetItem *item, int column);
  void onBranchClicked(QTreeWidgetItem *item, int column);

  Ui::MainWindow *ui;
  QTreeWidget *m_repoPanel = nullptr;
  QTreeWidgetItem *m_localBranchesItem = nullptr;
  QTreeWidgetItem *m_remoteBranchesItem = nullptr;
  QTreeWidgetItem *m_tagsItem = nullptr;
  QTreeWidgetItem *m_stashesItem = nullptr;
  QString m_currentPath;
  QString m_selectedCommitSha;
  QString m_localHeadSha;
  QString m_remoteHeadSha;
  QString m_remoteBranchName;
  QSet<QString> m_unpushedShas;
  QSet<QString> m_unpulledShas;
  QTableWidget *m_commitTable = nullptr;
  QTreeWidget *m_unstagedTree = nullptr;
  QTreeWidget *m_stagedTree = nullptr;
  QTreeWidget *m_commitFilesTree = nullptr;
  QTextEdit *m_diffView = nullptr;
  QLineEdit *m_commitSubject = nullptr;
  QTextEdit *m_commitBody = nullptr;
  QPushButton *m_commitButton = nullptr;
  QPushButton *m_pushButton = nullptr;
  QPushButton *m_pullButton = nullptr;
  QLineEdit *m_filterEdit = nullptr;
  QDockWidget *m_repoDock = nullptr;
  QDockWidget *m_workTreeDock = nullptr;
};

#endif // MAINWINDOW_H
