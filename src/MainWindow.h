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
#include <QMap>
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
class QComboBox;
class QLabel;
class QWidget;
class QDockWidget;
class QTreeWidget;
class QTableWidget;
class QTableWidgetItem;
class QTreeWidgetItem;
class QTabBar;
class QTabWidget;
class QSplitter;
class QNetworkAccessManager;
class QNetworkReply;
class SpellCheckHighlighter;
class QProcess;
class QProgressBar;
class QProgressDialog;
class GitLabApiClient;

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
  void showUntrackedContextMenu(const QPoint &pos);
  void showCommitFilesContextMenu(const QPoint &pos);
  void showCommitTableHeaderContextMenu(const QPoint &pos);
  void showCommitContextMenu(const QPoint &pos);
  void showRemotesContextMenu(const QPoint &pos);
  void showInteractiveRebase(const QString &baseSha);
  void cherryPickCommit(const QString &sha);
  void revertCommit(const QString &sha);
  void resetToCommit(const QString &sha);
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
  void showMergeDialog(const QString &branchToMerge = QString());
  void showArchiveDialog();
  void showRebaseOntoDialog();
  void startBisect();
  void bisectGood();
  void bisectBad();
  void bisectSkip();
  void bisectReset();
  void onRepositoryTabChanged(int index);
  void onRepositoryTabCloseRequested(int index);
  void checkForUpdates();
  void lfsTrack();
  void lfsUntrack();
  void lfsPull();
  void lfsPush();

private:
  enum class AmendStrategy { LocalOnly, ForceWithLease, Fixup, Cancel };

  AmendStrategy askAmendStrategy();
  void loadRepository(const QString &path, bool updateTab = true);
  void activateRepositoryTab(const QString &path);
  bool repositoryStateChanged() const;
  void showHunkStaging(const QString &path, bool unstage);
  void onDiffAnchorClicked(const QUrl &link);
  void toggleGpgConfig(bool enabled);
  void restoreSettings();
  void applyFonts();
  void savePullMode();
  void restoreSelectedFiles();
  void loadCommitMessageIntoEditor(const QString &sha);
  bool performPush(const QStringList &extraArgs);
  QString currentBranchName() const;
  bool isHeadPushed() const;
  bool isProtectedBranch(const QString &branch) const;
  bool execGitWithProgress(const QString &path, const QStringList &args,
                           const QString &label, QString *output = nullptr);
  void updateAmendWarning();
  void loadWorkingTree();
  void saveDockAndColumnState(bool includeGeometry = true);
  void saveOpenTabs();
  void restoreOpenTabs();
  bool restoreDockAndColumnState(bool includeGeometry = true);
  void onGrepRequested();
  void diffWithCommit(const QString &fromSha);
  void diffWithRemote();
  void onGrepResultActivated(QTreeWidgetItem *item, int column);
  void showBlame(const QString &path, const QString &revision = QString());
  void showFileHistory(const QString &path);
  void showEmptyDiff();
  void showErrorDiff(const QString &message);
  void openInExternalEditor(const QString &filePath) const;
  QString configuredDiffTool() const;
  QString configuredMergeTool() const;
  void showEmptyCommitFiles();
  void showErrorCommitFiles(const QString &message);
  void showImageDiff(const QString &path, bool staged, bool isNew);
  bool isImageFile(const QString &path) const;
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
  void refreshGitLabMergeRequests();
  void onGitLabMergeRequestsFinished(QNetworkReply *reply);
  void refreshGitLabPipelines();
  void onGitLabPipelinesFinished(QNetworkReply *reply);
  void onGitLabPipelineJobsFinished(QNetworkReply *reply);
  void onGitLabJobTraceFinished(QNetworkReply *reply);
  void downloadGitLabJobArtifacts();
  void onGitLabJobArtifactsFinished(QNetworkReply *reply, int jobId);

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
  FileTreeWidget *m_untrackedTree = nullptr;
  FileTreeWidget *m_commitFilesTree = nullptr;
  DiffViewWidget *m_diffView = nullptr;
  QWidget *m_diffContainer = nullptr;
  QComboBox *m_diffModeCombo = nullptr;
  QLineEdit *m_commitSubject = nullptr;
  QTextEdit *m_commitBody = nullptr;
  QComboBox *m_commitTemplateCombo = nullptr;
  QCheckBox *m_commitSpellCheckCheckBox = nullptr;
  SpellCheckHighlighter *m_spellCheckHighlighter = nullptr;
  QPushButton *m_commitButton = nullptr;
  QCheckBox *m_amendCheckBox = nullptr;
  QLabel *m_amendWarningLabel = nullptr;
  QString m_commitSubjectDraft;
  QString m_commitBodyDraft;
  QCheckBox *m_signCommitCheckBox = nullptr;
  QLabel *m_branchLabel = nullptr;
  QToolButton *m_pushButton = nullptr;
  QPushButton *m_undoButton = nullptr;
  QProgressBar *m_commandProgressBar = nullptr;
  QToolButton *m_pullButton = nullptr;
  QMenu *m_recentMenu = nullptr;
  QStringList m_pullArgs;
  QStringList m_pushArgs;
  bool m_backupAndMergePull = false;
  QLineEdit *m_filterEdit = nullptr;
  QDockWidget *m_repoDock = nullptr;
  QDockWidget *m_workTreeDock = nullptr;
  QDockWidget *m_grepDock = nullptr;
  QDockWidget *m_commandLogDock = nullptr;
  QTextEdit *m_commandLogEdit = nullptr;
  QLineEdit *m_arbitraryGitCommandEdit = nullptr;
  QPushButton *m_runGitCommandButton = nullptr;
  QDockWidget *m_diffDock = nullptr;
  bool m_commitTableResized = false;
  bool m_initialRepositoryLoaded = false;
  QLineEdit *m_grepEdit = nullptr;
  QTreeWidget *m_grepResults = nullptr;
  QNetworkAccessManager *m_networkManager = nullptr;
  QTabBar *m_repoTabBar = nullptr;
  QTabWidget *m_viewTabWidget = nullptr;
  QSplitter *m_mainSplitter = nullptr;
  QMap<QString, QString> m_repoSelectedShas;
  QMap<QString, int> m_repoHorizontalScroll;
  QMap<QString, QString> m_repoUnstagedFile;
  QMap<QString, QString> m_repoStagedFile;
  QMap<QString, QString> m_repoCommitFile;
  GitLabApiClient *m_gitLabClient = nullptr;
  QDockWidget *m_gitLabDock = nullptr;
  QTabWidget *m_gitLabTabWidget = nullptr;
  QTreeWidget *m_gitLabMRTree = nullptr;
  QPushButton *m_gitLabRefreshMRsButton = nullptr;
  QPushButton *m_gitLabRefreshPipelinesButton = nullptr;
  QPushButton *m_gitLabDownloadArtifactsButton = nullptr;
  QLabel *m_gitLabStatusLabel = nullptr;
  QTreeWidget *m_gitLabPipelineTree = nullptr;
  QTreeWidget *m_gitLabJobTree = nullptr;
  QTextEdit *m_gitLabJobLog = nullptr;
};

#endif // MAINWINDOW_H
