#ifndef MAINWINDOW_H
#define MAINWINDOW_H
// test
#include <QMainWindow>
#include <QSet>
#include <QStringList>

class QPoint;
class QIcon;
class QMenu;
class QFileSystemWatcher;
class QPushButton;
class QTimer;
class QToolButton;
class QLineEdit;
class QTextEdit;
class QCheckBox;
class QLabel;
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
  void showStashContextMenu(const QPoint &pos);
  void showTagContextMenu(const QPoint &pos);
  void showUnstagedContextMenu(const QPoint &pos);
  void showStagedContextMenu(const QPoint &pos);
  void showCommitFilesContextMenu(const QPoint &pos);
  void showCommitContextMenu(const QPoint &pos);
  void showRemotesContextMenu(const QPoint &pos);
  void showInteractiveRebase(const QString &baseSha);
  void cherryPickCommit(const QString &sha);
  void revertCommit(const QString &sha);
  void showSubmodulesContextMenu(const QPoint &pos);
  void onTagClicked(QTreeWidgetItem *item, int column);
  void onFileClicked(QTreeWidgetItem *item, int column);
  void onCloneRepository();
  void onInitRepository();
  void onCommitClicked();
  void onAmendToggled(int state);
  void showPreferences();
  void editGitignore();
  void showRepositorySettings();
  void initSubmodules();
  void updateSubmodules();
  void addSubmodule();
  void openSubmodule();
  void showReflog();

private:
  void loadRepository(const QString &path);
  void showHunkStaging(const QString &path, bool unstage);
  void restoreSettings();
  void savePullMode();
  void loadWorkingTree();
  void onGrepRequested();
  void diffWithCommit(const QString &fromSha);
  void onGrepResultActivated(QTreeWidgetItem *item, int column);
  void showBlame(const QString &path, const QString &revision = QString());
  void addFileToTree(QTreeWidget *tree, const QString &filePath,
                     const QString &status = QString());
  QIcon statusIcon(const QString &status) const;
  QString itemPath(QTreeWidget *tree, QTreeWidgetItem *item) const;
  QString formatDiff(const QStringList &lines) const;
  QStringList runGit(const QString &path, const QStringList &args,
                     int acceptedExitCode = 0) const;
  bool execGit(const QString &path, const QStringList &args,
               QString *output = nullptr) const;
  void loadStashes();
  void loadRemotes();
  void updateCommitButton();
  void updateFilter();
  void updateRecentRepos();
  void onCommitSelected(QTableWidgetItem *item);
  void onCommitFileClicked(QTreeWidgetItem *item, int column);
  void onStashClicked(QTreeWidgetItem *item, int column);
  void onBranchClicked(QTreeWidgetItem *item, int column);

  Ui::MainWindow *ui;
  QTreeWidget *m_repoPanel = nullptr;
  QTreeWidgetItem *m_localBranchesItem = nullptr;
  QTreeWidgetItem *m_remoteBranchesItem = nullptr;
  QTreeWidgetItem *m_tagsItem = nullptr;
  QTreeWidgetItem *m_remotesItem = nullptr;
  QTreeWidgetItem *m_stashesItem = nullptr;
  QTreeWidgetItem *m_submodulesItem = nullptr;
  QString m_currentPath;
  QString m_selectedCommitSha;
  QString m_localHeadSha;
  QString m_remoteHeadSha;
  QString m_remoteBranchName;
  QSet<QString> m_unpushedShas;
  QSet<QString> m_unpulledShas;
  QTableWidget *m_commitTable = nullptr;
  QFileSystemWatcher *m_watcher = nullptr;
  QTimer *m_fsDebounceTimer = nullptr;
  QTreeWidget *m_unstagedTree = nullptr;
  QTreeWidget *m_stagedTree = nullptr;
  QTreeWidget *m_commitFilesTree = nullptr;
  QTextEdit *m_diffView = nullptr;
  QLineEdit *m_commitSubject = nullptr;
  QTextEdit *m_commitBody = nullptr;
  QPushButton *m_commitButton = nullptr;
  QCheckBox *m_amendCheckBox = nullptr;
  QLabel *m_branchLabel = nullptr;
  QPushButton *m_pushButton = nullptr;
  QToolButton *m_pullButton = nullptr;
  QMenu *m_recentMenu = nullptr;
  QStringList m_pullArgs;
  QLineEdit *m_filterEdit = nullptr;
  QDockWidget *m_repoDock = nullptr;
  QDockWidget *m_workTreeDock = nullptr;
  QDockWidget *m_grepDock = nullptr;
  QLineEdit *m_grepEdit = nullptr;
  QTreeWidget *m_grepResults = nullptr;
};

#endif // MAINWINDOW_H
