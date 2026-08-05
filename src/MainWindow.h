#ifndef MAINWINDOW_H
#define MAINWINDOW_H
// test
#include "CommitModel.h"
#include "DiffPresenter.h"
#include "GitExecutor.h"
#include "GitRepository.h"
#include "WorkingTreeModel.h"
#include "widgets/DiffViewWidget.h"
#include "widgets/FileTreeWidget.h"

#include <QMainWindow>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QStringList>

class QPoint;
class QDockWidget;
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
class QNetworkAccessManager;

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
  void undoLastCommit();
  void onCommitClicked();
  void onAmendToggled(int state);
  void showPreferences();
  void editGitignore();
  void editGitattributes();
  void showCommitHooksAndTemplates();
  void showRepositorySettings();
  void applyPatch();
  void createPatchFromCommit(const QString &sha);
  void initSubmodules();
  void updateSubmodules();
  void syncSubmodules();
  void addSubmodule();
  void openSubmodule();
  void showReflog();
  void showConflictResolver(const QString &operation);
  void startBisect();
  void bisectGood();
  void bisectBad();
  void bisectSkip();
  void bisectReset();
  void checkForUpdates();
  void lfsTrack();
  void lfsUntrack();
  void lfsPull();
  void lfsPush();

private:
  void loadRepository(const QString &path);
  bool repositoryStateChanged() const;
  void showHunkStaging(const QString &path, bool unstage);
  void onDiffAnchorClicked(const QUrl &link);
  void toggleGpgConfig(bool enabled);
  void restoreSettings();
  void savePullMode();
  void loadWorkingTree();
  void saveDockAndColumnState(bool includeGeometry = true);
  void restoreDockAndColumnState(bool includeGeometry = true);
  void onGrepRequested();
  void diffWithCommit(const QString &fromSha);
  void diffWithRemote();
  void onGrepResultActivated(QTreeWidgetItem *item, int column);
  void showBlame(const QString &path, const QString &revision = QString());
  void showEmptyDiff();
  void showErrorDiff(const QString &message);
  void showEmptyCommitFiles();
  void showErrorCommitFiles(const QString &message);
  void launchGitTool(const QStringList &args, bool reload = false);
  void loadStashes();
  void showStashDiff(const QString &ref);
  void loadWorktrees();
  void showWorktreeContextMenu(const QPoint &pos);
  void onWorktreeClicked(QTreeWidgetItem *item, int column);
  void loadRemotes();
  void updateCommitButton();
  void updateFilter();
  void updateFileFilter(const QString &text);
  void updateRecentRepos();
  void onCommitSelected(QTableWidgetItem *item);
  void onCommitFileClicked(QTreeWidgetItem *item, int column);
  void onStashClicked(QTreeWidgetItem *item, int column);
  void onBranchClicked(QTreeWidgetItem *item, int column);

  Ui::MainWindow *ui;
  GitExecutor *m_gitExecutor = nullptr;
  GitRepository *m_gitRepository = nullptr;
  DiffPresenter *m_diffPresenter = nullptr;
  CommitModel *m_commitModel = nullptr;
  WorkingTreeModel *m_workingTreeModel = nullptr;
  QStackedWidget *m_centralStack = nullptr;
  QTreeWidget *m_repoPanel = nullptr;
  QTreeWidgetItem *m_localBranchesItem = nullptr;
  QTreeWidgetItem *m_remoteBranchesItem = nullptr;
  QTreeWidgetItem *m_tagsItem = nullptr;
  QTreeWidgetItem *m_remotesItem = nullptr;
  QTreeWidgetItem *m_stashesItem = nullptr;
  QTreeWidgetItem *m_worktreesItem = nullptr;
  QTreeWidgetItem *m_submodulesItem = nullptr;
  QString m_currentPath;
  QString m_currentDiffPath;
  QStringList m_currentDiffLines;
  bool m_currentDiffUnstage = false;
  bool m_currentDiffIsNew = false;
  QString m_selectedCommitSha;
  QString m_localHeadSha;
  QString m_remoteHeadSha;
  QString m_remoteBranchName;
  QSet<QString> m_unpushedShas;
  QSet<QString> m_unpulledShas;
  QString m_lastRepoSignature;
  QTableWidget *m_commitTable = nullptr;
  QFileSystemWatcher *m_watcher = nullptr;
  QTimer *m_fsDebounceTimer = nullptr;
  FileTreeWidget *m_unstagedTree = nullptr;
  FileTreeWidget *m_stagedTree = nullptr;
  FileTreeWidget *m_commitFilesTree = nullptr;
  DiffViewWidget *m_diffView = nullptr;
  QLineEdit *m_commitSubject = nullptr;
  QTextEdit *m_commitBody = nullptr;
  QPushButton *m_commitButton = nullptr;
  QCheckBox *m_amendCheckBox = nullptr;
  QCheckBox *m_signCommitCheckBox = nullptr;
  QLabel *m_branchLabel = nullptr;
  QToolButton *m_pushButton = nullptr;
  QPushButton *m_undoButton = nullptr;
  QToolButton *m_pullButton = nullptr;
  QMenu *m_recentMenu = nullptr;
  QStringList m_pullArgs;
  QStringList m_pushArgs;
  bool m_commitTableWidthInitialized = false;
  QLineEdit *m_filterEdit = nullptr;
  QDockWidget *m_repoDock = nullptr;
  QDockWidget *m_workTreeDock = nullptr;
  QDockWidget *m_grepDock = nullptr;
  QDockWidget *m_commandLogDock = nullptr;
  QTextEdit *m_commandLogEdit = nullptr;
  QDockWidget *m_diffDock = nullptr;
  QLineEdit *m_grepEdit = nullptr;
  QTreeWidget *m_grepResults = nullptr;
  QNetworkAccessManager *m_networkManager = nullptr;
};

#endif // MAINWINDOW_H
