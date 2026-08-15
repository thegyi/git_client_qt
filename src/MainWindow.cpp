#include "MainWindow.h"
#include "GitExecutor.h"
#include "LaneGraph.h"
#include "Theme.h"
#include "ui_MainWindow.h"
#include "widgets/CommitTableWidget.h"
#include "widgets/DiffViewWidget.h"
#include "widgets/FileTreeWidget.h"
#include "widgets/RepoPanelWidget.h"
#include "widgets/SpellCheckHighlighter.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QByteArray>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QCryptographicHash>
#include <QCursor>
#include <QDebug>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFont>
#include <QFontComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHeaderView>
#include <QImage>
#include <QInputDialog>
#include <QKeySequence>
#include <QKeySequenceEdit>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QPalette>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScreen>
#include <QScrollBar>
#include <QSet>
#include <QSettings>
#include <QShortcut>
#include <QSpinBox>
#include <QSplitter>
#include <QStyle>
#include <QStyleFactory>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVector>

#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryFile>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QVersionNumber>

namespace {
QIcon themedIcon(const QString &theme, QStyle::StandardPixmap fallback) {
  return QIcon::fromTheme(theme, QApplication::style()->standardIcon(fallback));
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  m_gitExecutor = new GitExecutor(this);
  m_gitRepository = new GitRepository(m_gitExecutor, this);
  m_diffPresenter = new DiffPresenter(this);
  m_commitModel = new CommitModel(this);
  m_workingTreeModel = new WorkingTreeModel(this);
  ui->setupUi(this);
  setMinimumSize(400, 300);
  statusBar()->setObjectName(QStringLiteral("statusBar"));
  statusBar()->setSizeGripEnabled(true);
  statusBar()->show();
  ui->menuFile->setTitle(tr("&File"));
  ui->menuEdit->setTitle(tr("&Edit"));
  ui->menuHelp->setTitle(tr("&Help"));

  if (ui->actionOpen)
    ui->actionOpen->setIcon(
        themedIcon("document-open", QStyle::SP_DirOpenIcon));
  if (ui->actionClose)
    ui->actionClose->setIcon(
        themedIcon("window-close", QStyle::SP_DialogCloseButton));
  if (ui->actionExit)
    ui->actionExit->setIcon(
        themedIcon("application-exit", QStyle::SP_DialogCloseButton));
  if (ui->actionPreferences)
    ui->actionPreferences->setIcon(
        themedIcon("preferences-system", QStyle::SP_ComputerIcon));
  if (ui->actionAbout)
    ui->actionAbout->setIcon(
        themedIcon("help-about", QStyle::SP_MessageBoxInformation));
  setWindowIcon(themedIcon("git-client-qt", QStyle::SP_DirHomeIcon));
  setDockNestingEnabled(true);

  m_watcher = new QFileSystemWatcher(this);
  m_fsDebounceTimer = new QTimer(this);
  m_fsDebounceTimer->setSingleShot(true);
  m_fsDebounceTimer->setInterval(500);
  connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
          [this](const QString &) { m_fsDebounceTimer->start(); });
  connect(m_watcher, &QFileSystemWatcher::fileChanged, this,
          [this](const QString &) { m_fsDebounceTimer->start(); });
  connect(m_fsDebounceTimer, &QTimer::timeout, this, [this]() {
    if (!m_currentPath.isEmpty() && repositoryStateChanged())
      loadRepository(m_currentPath);
  });

  m_recentMenu = new QMenu(tr("Recent Repositories"), this);
  m_recentMenu->setIcon(
      themedIcon("document-open-recent", QStyle::SP_DirLinkIcon));
  ui->menuFile->insertMenu(ui->actionClose, m_recentMenu);

  auto *searchMenu = new QMenu(tr("&Search"), this);
  menuBar()->addMenu(searchMenu);
  auto *grepAction = searchMenu->addAction(tr("Grep"));
  grepAction->setIcon(
      themedIcon("edit-find", QStyle::SP_FileDialogContentsView));
  grepAction->setStatusTip(tr("Search for a pattern in the repository"));
  grepAction->setShortcut(QKeySequence(QLatin1String("Ctrl+Shift+G")));
  connect(grepAction, &QAction::triggered, this, &MainWindow::onGrepRequested);

  auto *submodulesMenu = new QMenu(tr("&Submodules"), this);
  menuBar()->addMenu(submodulesMenu);
  auto *initSubmodulesAction = submodulesMenu->addAction(tr("Init"));
  initSubmodulesAction->setIcon(
      themedIcon("folder-new", QStyle::SP_FileDialogNewFolder));
  initSubmodulesAction->setStatusTip(
      tr("Initialize the configured submodules"));
  auto *updateSubmodulesAction = submodulesMenu->addAction(tr("Update"));
  updateSubmodulesAction->setIcon(
      themedIcon("view-refresh", QStyle::SP_BrowserReload));
  updateSubmodulesAction->setStatusTip(tr("Update registered submodules"));
  auto *syncSubmodulesAction = submodulesMenu->addAction(tr("Sync"));
  syncSubmodulesAction->setIcon(
      themedIcon("emblem-synchronizing", QStyle::SP_BrowserReload));
  syncSubmodulesAction->setStatusTip(tr("Sync submodule remotes"));
  auto *addSubmoduleAction = submodulesMenu->addAction(tr("Add..."));
  addSubmoduleAction->setIcon(
      themedIcon("list-add", QStyle::SP_FileDialogNewFolder));
  addSubmoduleAction->setStatusTip(tr("Add a new submodule to the repository"));
  auto *openSubmoduleAction = submodulesMenu->addAction(tr("Open..."));
  openSubmoduleAction->setIcon(
      themedIcon("document-open", QStyle::SP_DirOpenIcon));
  openSubmoduleAction->setStatusTip(tr("Open the selected submodule"));
  connect(initSubmodulesAction, &QAction::triggered, this,
          &MainWindow::initSubmodules);
  connect(updateSubmodulesAction, &QAction::triggered, this,
          &MainWindow::updateSubmodules);
  connect(syncSubmodulesAction, &QAction::triggered, this,
          &MainWindow::syncSubmodules);
  connect(addSubmoduleAction, &QAction::triggered, this,
          &MainWindow::addSubmodule);
  connect(openSubmoduleAction, &QAction::triggered, this,
          &MainWindow::openSubmodule);

  auto *repositoryMenu = new QMenu(tr("&Repository"), this);
  menuBar()->addMenu(repositoryMenu);

  if (ui->menuHelp) {
    menuBar()->removeAction(ui->menuHelp->menuAction());
    menuBar()->addAction(ui->menuHelp->menuAction());

    auto *checkForUpdatesAction =
        ui->menuHelp->addAction(tr("&Check for Updates..."));
    checkForUpdatesAction->setIcon(
        themedIcon("system-software-update", QStyle::SP_BrowserReload));
    checkForUpdatesAction->setStatusTip(
        tr("Check GitHub for the latest release"));
    connect(checkForUpdatesAction, &QAction::triggered, this,
            &MainWindow::checkForUpdates);
  }

  auto *repoSettingsAction = repositoryMenu->addAction(tr("Settings..."));
  repoSettingsAction->setIcon(
      themedIcon("preferences-system", QStyle::SP_ComputerIcon));
  repoSettingsAction->setStatusTip(tr("Configure repository settings"));
  repoSettingsAction->setShortcut(QKeySequence(QLatin1String("Ctrl+,")));
  connect(repoSettingsAction, &QAction::triggered, this,
          &MainWindow::showRepositorySettings);

  auto *hooksAndTemplatesAction =
      repositoryMenu->addAction(tr("Commit hooks and templates..."));
  hooksAndTemplatesAction->setIcon(
      themedIcon("text-x-script", QStyle::SP_FileIcon));
  hooksAndTemplatesAction->setStatusTip(
      tr("Edit commit message template and repository hooks"));
  hooksAndTemplatesAction->setShortcut(
      QKeySequence(QLatin1String("Ctrl+Shift+H")));
  connect(hooksAndTemplatesAction, &QAction::triggered, this,
          &MainWindow::showCommitHooksAndTemplates);

  auto *applyPatchAction = repositoryMenu->addAction(tr("Apply patch..."));
  applyPatchAction->setIcon(
      themedIcon("document-import", QStyle::SP_FileDialogStart));
  applyPatchAction->setStatusTip(tr("Apply a patch or diff file"));
  applyPatchAction->setShortcut(QKeySequence(QLatin1String("Ctrl+Shift+P")));
  connect(applyPatchAction, &QAction::triggered, this, &MainWindow::applyPatch);

  auto *reflogAction = repositoryMenu->addAction(tr("Reflog"));
  reflogAction->setIcon(
      themedIcon("view-list-text", QStyle::SP_FileDialogListView));
  reflogAction->setStatusTip(tr("View the reference log"));
  reflogAction->setShortcut(QKeySequence(QLatin1String("Ctrl+Shift+R")));
  connect(reflogAction, &QAction::triggered, this, &MainWindow::showReflog);

  auto *diffWithRemoteAction =
      repositoryMenu->addAction(tr("Diff local vs remote"));
  diffWithRemoteAction->setIcon(
      themedIcon("view-split-left-right", QStyle::SP_FileDialogContentsView));
  diffWithRemoteAction->setStatusTip(
      tr("Show diff between local and remote HEAD"));
  connect(diffWithRemoteAction, &QAction::triggered, this,
          &MainWindow::diffWithRemote);

  auto *undoLastCommitAction =
      repositoryMenu->addAction(tr("Undo last commit"));
  undoLastCommitAction->setIcon(themedIcon("edit-undo", QStyle::SP_ArrowBack));
  undoLastCommitAction->setStatusTip(
      tr("Soft-reset the most recent commit and keep changes staged"));
  connect(undoLastCommitAction, &QAction::triggered, this,
          &MainWindow::undoLastCommit);

  auto *mergeAction = repositoryMenu->addAction(tr("Merge branch..."));
  mergeAction->setIcon(themedIcon("merge", QStyle::SP_ArrowRight));
  mergeAction->setStatusTip(tr("Merge another branch into the current branch"));
  mergeAction->setShortcut(QKeySequence(QLatin1String("Ctrl+M")));
  connect(mergeAction, &QAction::triggered, this,
          [this]() { showMergeDialog(); });

  auto *resolveConflictsAction =
      repositoryMenu->addAction(tr("Resolve conflicts..."));
  resolveConflictsAction->setIcon(
      themedIcon("dialog-warning", QStyle::SP_MessageBoxWarning));
  resolveConflictsAction->setStatusTip(tr("Resolve merge conflicts"));
  resolveConflictsAction->setShortcut(
      QKeySequence(QLatin1String("Ctrl+Shift+C")));
  connect(resolveConflictsAction, &QAction::triggered, this,
          [this]() { showConflictResolver(QString()); });

  auto *bisectMenu = repositoryMenu->addMenu(tr("&Bisect"));
  bisectMenu->setIcon(
      themedIcon("edit-find-replace", QStyle::SP_FileDialogContentsView));
  auto *bisectStartAction = bisectMenu->addAction(tr("Start..."));
  bisectStartAction->setIcon(
      themedIcon("media-playback-start", QStyle::SP_MediaPlay));
  auto *bisectGoodAction = bisectMenu->addAction(tr("Good"));
  bisectGoodAction->setIcon(
      themedIcon("dialog-ok", QStyle::SP_DialogApplyButton));
  auto *bisectBadAction = bisectMenu->addAction(tr("Bad"));
  bisectBadAction->setIcon(
      themedIcon("process-stop", QStyle::SP_MessageBoxCritical));
  auto *bisectSkipAction = bisectMenu->addAction(tr("Skip"));
  bisectSkipAction->setIcon(
      themedIcon("media-skip-forward", QStyle::SP_MediaSkipForward));
  auto *bisectResetAction = bisectMenu->addAction(tr("Reset"));
  bisectResetAction->setIcon(
      themedIcon("edit-undo", QStyle::SP_DialogResetButton));
  connect(bisectStartAction, &QAction::triggered, this,
          &MainWindow::startBisect);
  connect(bisectGoodAction, &QAction::triggered, this, &MainWindow::bisectGood);
  connect(bisectBadAction, &QAction::triggered, this, &MainWindow::bisectBad);
  connect(bisectSkipAction, &QAction::triggered, this, &MainWindow::bisectSkip);
  connect(bisectResetAction, &QAction::triggered, this,
          &MainWindow::bisectReset);

  auto *lfsMenu = repositoryMenu->addMenu(tr("&LFS"));
  lfsMenu->setIcon(themedIcon("package-x-generic", QStyle::SP_DriveHDIcon));
  auto *lfsTrackAction = lfsMenu->addAction(tr("Track pattern..."));
  lfsTrackAction->setIcon(
      themedIcon("list-add", QStyle::SP_FileDialogNewFolder));
  auto *lfsUntrackAction = lfsMenu->addAction(tr("Untrack pattern..."));
  lfsUntrackAction->setIcon(
      themedIcon("list-remove", QStyle::SP_DialogDiscardButton));
  auto *lfsPullAction = lfsMenu->addAction(tr("Pull objects"));
  lfsPullAction->setIcon(themedIcon("go-down", QStyle::SP_ArrowDown));
  auto *lfsPushAction = lfsMenu->addAction(tr("Push objects"));
  lfsPushAction->setIcon(themedIcon("go-up", QStyle::SP_ArrowUp));
  connect(lfsTrackAction, &QAction::triggered, this, &MainWindow::lfsTrack);
  connect(lfsUntrackAction, &QAction::triggered, this, &MainWindow::lfsUntrack);
  connect(lfsPullAction, &QAction::triggered, this, &MainWindow::lfsPull);
  connect(lfsPushAction, &QAction::triggered, this, &MainWindow::lfsPush);

  ui->actionOpen->setShortcut(QKeySequence::Open);
  ui->actionOpen->setStatusTip(tr("Open an existing Git repository"));
  ui->actionClose->setShortcut(QKeySequence::Close);
  ui->actionClose->setStatusTip(tr("Close the current repository"));
  ui->actionExit->setShortcut(QKeySequence::Quit);
  ui->actionExit->setStatusTip(tr("Exit the application"));
  ui->actionPreferences->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
  ui->actionPreferences->setStatusTip(tr("Open application preferences"));

  m_branchLabel = new QLabel(this);
  statusBar()->addPermanentWidget(m_branchLabel);

  auto *actionClone = new QAction(tr("Clone Repository"), this);
  actionClone->setIcon(themedIcon("folder-download", QStyle::SP_ArrowDown));
  ui->menuFile->insertAction(ui->actionOpen, actionClone);
  actionClone->setStatusTip(tr("Clone a remote repository"));
  actionClone->setShortcut(QKeySequence(QLatin1String("Ctrl+Shift+N")));
  connect(actionClone, &QAction::triggered, this,
          &MainWindow::onCloneRepository);

  auto *actionInit = new QAction(tr("Initialize Repository"), this);
  actionInit->setIcon(themedIcon("folder-new", QStyle::SP_FileDialogNewFolder));
  ui->menuFile->insertAction(ui->actionOpen, actionInit);
  actionInit->setStatusTip(tr("Create a new Git repository"));
  actionInit->setShortcut(QKeySequence(QLatin1String("Ctrl+N")));
  connect(actionInit, &QAction::triggered, this, &MainWindow::onInitRepository);

  auto *editGitignoreAction = new QAction(tr("Edit .gitignore"), this);
  editGitignoreAction->setIcon(
      themedIcon("text-x-generic", QStyle::SP_FileIcon));
  editGitignoreAction->setStatusTip(tr("Edit the repository .gitignore file"));
  editGitignoreAction->setShortcut(QKeySequence(QLatin1String("Ctrl+Shift+I")));
  connect(editGitignoreAction, &QAction::triggered, this,
          &MainWindow::editGitignore);

  auto *editGitattributesAction = new QAction(tr("Edit .gitattributes"), this);
  editGitattributesAction->setIcon(
      themedIcon("text-x-generic", QStyle::SP_FileIcon));
  editGitattributesAction->setStatusTip(
      tr("Edit the repository .gitattributes file"));
  editGitattributesAction->setShortcut(
      QKeySequence(QLatin1String("Ctrl+Shift+A")));
  connect(editGitattributesAction, &QAction::triggered, this,
          &MainWindow::editGitattributes);

  ui->menuEdit->insertAction(ui->actionPreferences, editGitattributesAction);
  ui->menuEdit->insertAction(editGitattributesAction, editGitignoreAction);
  ui->menuEdit->insertSeparator(ui->actionPreferences);

  auto *filterBar = addToolBar(tr("Filter"));
  filterBar->setMovable(false);
  m_filterEdit = new QLineEdit(this);
  m_filterEdit->setPlaceholderText(tr("Filter by author, branch or message"));
  m_filterEdit->setClearButtonEnabled(true);
  filterBar->addWidget(m_filterEdit);
  connect(m_filterEdit, &QLineEdit::textChanged, this,
          &MainWindow::updateFilter);

  auto *remoteBar = addToolBar(tr("Remote"));
  remoteBar->setMovable(false);
  m_pushButton = new QToolButton(this);
  m_pushButton->setText(tr("Push"));
  m_pushButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  m_pushButton->setPopupMode(QToolButton::MenuButtonPopup);
  m_pushButton->setIcon(
      QApplication::style()->standardIcon(QStyle::SP_ArrowUp));
  m_pushButton->setEnabled(false);
  m_pushButton->setToolTip(tr("Push changes to the remote repository"));
  m_pushButton->setStatusTip(
      tr("Push the current branch to the configured upstream remote"));
  m_pushButton->setShortcut(QKeySequence(QLatin1String("Ctrl+Shift+P")));
  m_undoButton = new QPushButton(tr("Undo"), this);
  m_undoButton->setIcon(
      QApplication::style()->standardIcon(QStyle::SP_ArrowBack));
  m_undoButton->setEnabled(false);
  m_undoButton->setToolTip(tr("Undo the last commit"));
  m_undoButton->setStatusTip(
      tr("Reset the last commit and keep the changes staged"));
  m_undoButton->setShortcut(QKeySequence(QLatin1String("Ctrl+Shift+Z")));
  m_pullButton = new QToolButton(this);
  m_pullButton->setText(tr("Pull"));
  m_pullButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  m_pullButton->setPopupMode(QToolButton::MenuButtonPopup);
  m_pullButton->setIcon(
      QApplication::style()->standardIcon(QStyle::SP_ArrowDown));
  m_pullButton->setEnabled(false);
  m_pullButton->setToolTip(tr("Pull changes from the remote"));
  m_pullButton->setStatusTip(tr("Fetch and integrate the remote branch"));
  m_pullButton->setShortcut(QKeySequence(QLatin1String("Ctrl+Shift+L")));
  remoteBar->addWidget(m_pushButton);
  remoteBar->addWidget(m_undoButton);
  remoteBar->addWidget(m_pullButton);
  connect(m_undoButton, &QPushButton::clicked, this,
          &MainWindow::undoLastCommit);

  m_pullButton->setText(tr("Pull"));

  auto *pushMenu = new QMenu(this);
  auto *pushNormal = pushMenu->addAction(tr("Push"));
  pushNormal->setStatusTip(tr("Push the current branch to its remote"));
  auto *pushForce = pushMenu->addAction(tr("Force push"));
  pushForce->setStatusTip(
      tr("Force push and overwrite the remote branch history"));
  auto *pushLease = pushMenu->addAction(tr("Push with lease"));
  pushLease->setStatusTip(
      tr("Push only if the remote ref is at the expected state"));
  auto *pushGroup = new QActionGroup(this);
  pushGroup->setExclusive(true);
  for (auto *action : {pushNormal, pushForce, pushLease}) {
    action->setCheckable(true);
    pushGroup->addAction(action);
  }
  pushNormal->setData(QStringLiteral("push"));
  pushForce->setData(QStringLiteral("force"));
  pushLease->setData(QStringLiteral("lease"));
  pushNormal->setChecked(true);
  m_pushButton->setMenu(pushMenu);

  auto *pullMenu = new QMenu(this);
  auto *ffIfPossible =
      pullMenu->addAction(tr("Pull (fast-forward if possible)"));
  ffIfPossible->setStatusTip(
      tr("Pull using fast-forward when possible, otherwise merge"));
  auto *ffOnly = pullMenu->addAction(tr("Pull (fast-forward only)"));
  ffOnly->setStatusTip(tr("Pull only if it can be fast-forwarded"));
  auto *rebase = pullMenu->addAction(tr("Pull (rebase)"));
  rebase->setStatusTip(tr("Fetch and rebase the current branch"));
  auto *fetchAll = pullMenu->addAction(tr("Fetch all"));
  fetchAll->setStatusTip(tr("Fetch all remotes"));
  fetchAll->setShortcut(QKeySequence(QLatin1String("Ctrl+Shift+F")));
  auto *actionGroup = new QActionGroup(this);
  actionGroup->setExclusive(true);
  for (auto *action : {ffIfPossible, ffOnly, rebase, fetchAll}) {
    action->setCheckable(true);
    actionGroup->addAction(action);
  }
  ffIfPossible->setData(QStringLiteral("ffIfPossible"));
  ffOnly->setData(QStringLiteral("ffOnly"));
  rebase->setData(QStringLiteral("rebase"));
  fetchAll->setData(QStringLiteral("fetchAll"));
  ffIfPossible->setChecked(true);
  m_pullButton->setMenu(pullMenu);

  pullMenu->addSeparator();
  auto *fetchFromAction = pullMenu->addAction(tr("Fetch from..."));
  fetchFromAction->setIcon(themedIcon("folder-download", QStyle::SP_ArrowDown));
  fetchFromAction->setStatusTip(tr("Fetch from a selected remote"));
  connect(fetchFromAction, &QAction::triggered, this, [this] {
    if (m_currentPath.isEmpty())
      return;
    const QStringList remotes = m_gitExecutor->run(m_currentPath, {"remote"});
    if (remotes.isEmpty()) {
      statusBar()->showMessage(tr("There are no remotes to fetch from."), 0);
      return;
    }
    bool ok;
    const QString remote = QInputDialog::getItem(
        this, tr("Fetch from Remote"), tr("Remote:"), remotes, 0, false, &ok);
    if (ok && !remote.isEmpty()) {
      if (execGitWithProgress(m_currentPath, {"fetch", remote},
                              tr("Fetching from %1...").arg(remote))) {
        loadRepository(m_currentPath);
        statusBar()->showMessage(tr("Fetched from %1").arg(remote));
      } else {
        statusBar()->showMessage(tr("Failed to fetch from %1").arg(remote));
      }
    }
  });

  m_pullArgs.clear();
  m_pullArgs << "pull";

  m_pushArgs.clear();
  m_pushArgs << "push";

  connect(pushGroup, &QActionGroup::triggered, this, [this](QAction *action) {
    if (!action)
      return;
    const QString mode = action->data().toString();
    m_pushArgs.clear();
    if (mode == QStringLiteral("push")) {
      m_pushArgs << QStringLiteral("push");
      m_pushButton->setText(tr("Push"));
    } else if (mode == QStringLiteral("force")) {
      m_pushArgs << QStringLiteral("push") << QStringLiteral("-f");
      m_pushButton->setText(tr("Force push"));
    } else if (mode == QStringLiteral("lease")) {
      m_pushArgs << QStringLiteral("push")
                 << QStringLiteral("--force-with-lease");
      m_pushButton->setText(tr("Push with lease"));
    }
  });

  connect(m_pushButton, &QToolButton::clicked, this, [this] {
    QStringList extraArgs;
    for (const QString &arg : m_pushArgs) {
      if (arg != QStringLiteral("push"))
        extraArgs << arg;
    }
    performPush(extraArgs);
  });

  connect(m_pullButton, &QToolButton::clicked, this, [this] {
    if (m_currentPath.isEmpty())
      return;

    QProgressDialog progress(tr("Pulling from remote..."), tr("Cancel"), 0, 0,
                             this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    QProcess p;
    p.setWorkingDirectory(m_currentPath);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("GIT_EDITOR"), QStringLiteral("true"));
    p.setProcessEnvironment(env);

    QString output;
    bool canceled = false;

    connect(&p, &QProcess::readyReadStandardOutput, this, [&]() {
      output += QString::fromLocal8Bit(p.readAllStandardOutput());
    });
    connect(&p, &QProcess::readyReadStandardError, this, [&]() {
      output += QString::fromLocal8Bit(p.readAllStandardError());
    });
    connect(&p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            &progress, &QProgressDialog::close);
    connect(&p, &QProcess::errorOccurred, &progress, &QProgressDialog::close);
    connect(&progress, &QProgressDialog::canceled, &p, &QProcess::kill);
    connect(&progress, &QProgressDialog::canceled, this,
            [&]() { canceled = true; });

    QStringList pullArgs = m_pullArgs;
    if (m_pullArgs.first() != QLatin1String("fetch")) {
      const QString currentBranch =
          m_gitExecutor
              ->run(m_currentPath, {"rev-parse", "--abbrev-ref", "HEAD"})
              .value(0);
      if (currentBranch.isEmpty()) {
        statusBar()->showMessage(tr("No current branch"), 0);
        return;
      }
      const QString remote =
          m_gitExecutor
              ->run(m_currentPath,
                    {"config",
                     QStringLiteral("branch.%1.remote").arg(currentBranch)})
              .value(0);
      QString merge =
          m_gitExecutor
              ->run(m_currentPath,
                    {"config",
                     QStringLiteral("branch.%1.merge").arg(currentBranch)})
              .value(0);
      if (merge.startsWith(QLatin1String("refs/heads/")))
        merge.remove(0, 11);
      if (remote.isEmpty() || merge.isEmpty()) {
        const QStringList remotes =
            m_gitExecutor->run(m_currentPath, {"remote"});
        if (remotes.isEmpty()) {
          statusBar()->showMessage(tr("There is no remote to pull from."), 0);
          return;
        }
        pullArgs << remotes.first() << currentBranch;
      } else {
        pullArgs << remote;
      }
    }

    p.start(QStringLiteral("git"), pullArgs);
    if (!p.waitForStarted(5000)) {
      statusBar()->showMessage(tr("Could not start git process"), 0);
      return;
    }

    progress.exec();

    if (canceled) {
      statusBar()->showMessage(tr("Pull canceled"));
      return;
    }
    if (p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0) {
      QTimer::singleShot(100, this,
                         [this]() { loadRepository(m_currentPath); });
      statusBar()->showMessage(m_pullArgs.first() == QLatin1String("fetch")
                                   ? tr("Fetched")
                                   : tr("Pulled"));
    } else {
      if (output.isEmpty())
        output = p.errorString();
      statusBar()->showMessage(output.isEmpty() ? tr("Pull failed") : output,
                               0);
    }
  });

  connect(ffIfPossible, &QAction::triggered, this, [this] {
    m_pullArgs.clear();
    m_pullArgs << "pull";
    savePullMode();
  });
  connect(ffOnly, &QAction::triggered, this, [this] {
    m_pullArgs.clear();
    m_pullArgs << "pull" << "--ff-only";
    savePullMode();
  });
  connect(rebase, &QAction::triggered, this, [this] {
    m_pullArgs.clear();
    m_pullArgs << "pull" << "--rebase";
    savePullMode();
  });
  connect(fetchAll, &QAction::triggered, this, [this] {
    m_pullArgs.clear();
    m_pullArgs << "fetch" << "--all";
    savePullMode();
  });

  m_commitTable = new CommitTableWidget(this);
  connect(m_commitTable, &QTableWidget::cellClicked, this,
          [this](int row, int column) {
            Q_UNUSED(column)
            onCommitSelected(m_commitTable->item(row, 0));
          });
  m_commitTable->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_commitTable, &QTableWidget::customContextMenuRequested, this,
          &MainWindow::showCommitContextMenu);
  m_commitTable->horizontalHeader()->setContextMenuPolicy(
      Qt::CustomContextMenu);
  connect(m_commitTable->horizontalHeader(),
          &QHeaderView::customContextMenuRequested, this,
          &MainWindow::showCommitTableHeaderContextMenu);

  m_diffView = new DiffViewWidget(this);
  m_diffView->setMinimumHeight(120);
  m_diffView->setFont(Theme::monospaceFont());
  m_diffView->setFrameStyle(QFrame::NoFrame);
  m_diffView->document()->setDocumentMargin(0);
  connect(m_diffView, &QTextBrowser::anchorClicked, this,
          &MainWindow::onDiffAnchorClicked);
  showEmptyDiff();

  m_diffContainer = new QWidget(this);
  auto *diffContainerLayout = new QVBoxLayout(m_diffContainer);
  diffContainerLayout->setContentsMargins(0, 0, 0, 0);
  diffContainerLayout->setSpacing(0);
  auto *diffModeLayout = new QHBoxLayout();
  diffModeLayout->setContentsMargins(4, 4, 4, 4);
  diffModeLayout->setSpacing(4);
  diffModeLayout->addWidget(new QLabel(tr("Diff mode:"), this));
  m_diffModeCombo = new QComboBox(this);
  m_diffModeCombo->addItem(tr("Unified"));
  m_diffModeCombo->addItem(tr("Side-by-side"));
  m_diffModeCombo->setCurrentIndex(
      m_diffPresenter->mode() == DiffPresenter::DiffMode::Unified ? 0 : 1);
  diffModeLayout->addWidget(m_diffModeCombo);
  diffModeLayout->addStretch();
  diffContainerLayout->addLayout(diffModeLayout);
  diffContainerLayout->addWidget(m_diffView, 1);
  connect(m_diffModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int index) {
            if (!m_diffPresenter || !m_diffView)
              return;
            m_diffPresenter->setMode(index == 0
                                         ? DiffPresenter::DiffMode::Unified
                                         : DiffPresenter::DiffMode::SideBySide);
            if (m_diffPresenter->hasCurrent())
              m_diffView->setHtml(m_diffPresenter->formatCurrent());
          });

  m_centralStack = new QStackedWidget(this);
  auto *welcomeWidget = new QWidget(m_centralStack);
  auto *welcomeLayout = new QVBoxLayout(welcomeWidget);
  welcomeLayout->setAlignment(Qt::AlignCenter);

  auto *welcomeIcon = new QLabel(welcomeWidget);
  welcomeIcon->setText(tr("Git Client Qt"));
  QFont iconFont = welcomeIcon->font();
  iconFont.setPointSize(18);
  iconFont.setBold(true);
  welcomeIcon->setFont(iconFont);
  welcomeIcon->setAlignment(Qt::AlignCenter);
  welcomeLayout->addWidget(welcomeIcon);

  auto *welcomeLabel = new QLabel(tr("No repository open"), welcomeWidget);
  welcomeLabel->setAlignment(Qt::AlignCenter);
  welcomeLayout->addWidget(welcomeLabel);
  welcomeLayout->addSpacing(24);

  auto *openButton = new QPushButton(tr("Open Repository"), welcomeWidget);
  auto *cloneButton = new QPushButton(tr("Clone Repository"), welcomeWidget);
  auto *initButton =
      new QPushButton(tr("Initialize Repository"), welcomeWidget);
  welcomeLayout->addWidget(openButton);
  welcomeLayout->addWidget(cloneButton);
  welcomeLayout->addWidget(initButton);

  connect(openButton, &QPushButton::clicked, ui->actionOpen, &QAction::trigger);
  connect(cloneButton, &QPushButton::clicked, this,
          &MainWindow::onCloneRepository);
  connect(initButton, &QPushButton::clicked, this,
          &MainWindow::onInitRepository);

  m_viewTabWidget = new QTabWidget(this);
  m_viewTabWidget->addTab(m_commitTable, tr("History"));
  m_viewTabWidget->addTab(m_diffContainer, tr("Diff"));

  m_centralStack->addWidget(welcomeWidget);
  m_centralStack->addWidget(m_viewTabWidget);
  m_centralStack->setCurrentIndex(0);

  m_repoTabBar = new QTabBar(this);
  m_repoTabBar->setTabsClosable(true);
  m_repoTabBar->setVisible(false);
  connect(m_repoTabBar, &QTabBar::currentChanged, this,
          &MainWindow::onRepositoryTabChanged);
  connect(m_repoTabBar, &QTabBar::tabCloseRequested, this,
          &MainWindow::onRepositoryTabCloseRequested);

  auto *centralContainer = new QWidget(this);
  auto *centralLayout = new QVBoxLayout(centralContainer);
  centralLayout->setContentsMargins(0, 0, 0, 0);
  centralLayout->setSpacing(0);
  centralLayout->addWidget(m_repoTabBar);
  centralLayout->addWidget(m_centralStack);

  m_mainSplitter = new QSplitter(Qt::Horizontal, this);
  m_mainSplitter->setHandleWidth(2);
  m_mainSplitter->setChildrenCollapsible(true);
  m_mainSplitter->addWidget(centralContainer);

  m_repoPanel = new RepoPanelWidget(this);
  connect(
      m_repoPanel, &QTreeWidget::customContextMenuRequested, this,
      [this](const QPoint &pos) {
        QTreeWidgetItem *item = m_repoPanel->itemAt(pos);
        if (item && m_stashesItem &&
            (item->parent() == m_stashesItem || item == m_stashesItem)) {
          showStashContextMenu(pos);
        } else if (item && m_tagsItem &&
                   (item == m_tagsItem || item->parent() == m_tagsItem)) {
          showTagContextMenu(pos);
        } else if (item && m_remotesItem &&
                   (item == m_remotesItem || item->parent() == m_remotesItem)) {
          showRemotesContextMenu(pos);
        } else if (item && m_submodulesItem &&
                   (item == m_submodulesItem ||
                    item->parent() == m_submodulesItem)) {
          showSubmodulesContextMenu(pos);
        } else if (item && m_worktreesItem &&
                   (item == m_worktreesItem ||
                    item->parent() == m_worktreesItem)) {
          showWorktreeContextMenu(pos);
        } else {
          showBranchContextMenu(pos);
        }
      });
  connect(m_repoPanel, &QTreeWidget::itemClicked, this,
          &MainWindow::onTagClicked);
  connect(m_repoPanel, &QTreeWidget::itemClicked, this,
          &MainWindow::onStashClicked);
  connect(m_repoPanel, &QTreeWidget::itemClicked, this,
          &MainWindow::onWorktreeClicked);
  connect(m_repoPanel, &QTreeWidget::itemClicked, this,
          &MainWindow::onBranchClicked);

  auto *dock = new QDockWidget(QStringLiteral("Repository"), this);
  dock->setObjectName(QStringLiteral("repoDock"));
  dock->setTitleBarWidget(new QWidget(dock));
  dock->setWidget(m_repoPanel);
  dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
  dock->setMaximumWidth(400);
  m_repoDock = dock;
  m_mainSplitter->insertWidget(0, dock);

  auto *rightDock = new QDockWidget(tr("Working Tree"), this);
  rightDock->setObjectName(QStringLiteral("workTreeDock"));
  rightDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
  auto *rightWidget = new QWidget(this);
  rightWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto *rightLayout = new QVBoxLayout(rightWidget);
  rightLayout->setContentsMargins(4, 4, 4, 4);
  rightLayout->setSpacing(4);

  auto *unstagedGroup = new QGroupBox(tr("Unstaged Files"), this);
  auto *unstagedLayout = new QVBoxLayout(unstagedGroup);
  unstagedGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_unstagedTree = new FileTreeWidget(QString(), this);
  m_unstagedTree->setMinimumHeight(120);
  connect(m_unstagedTree, &QTreeWidget::customContextMenuRequested, this,
          &MainWindow::showUnstagedContextMenu);
  connect(m_unstagedTree, &QTreeWidget::itemClicked, this,
          &MainWindow::onFileClicked);
  connect(
      m_unstagedTree, &FileTreeWidget::fileDropped, this,
      [this](const QString &path) {
        if (m_currentPath.isEmpty())
          return;
        if (m_gitExecutor->exec(m_currentPath, {QStringLiteral("reset"),
                                                QStringLiteral("HEAD"),
                                                QStringLiteral("--"), path})) {
          loadWorkingTree();
        }
      });

  unstagedLayout->addWidget(m_unstagedTree);
  rightLayout->addWidget(unstagedGroup);

  auto *stagedGroup = new QGroupBox(tr("Staged Files"), this);
  auto *stagedLayout = new QVBoxLayout(stagedGroup);
  stagedGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_stagedTree = new FileTreeWidget(QString(), this);
  m_stagedTree->setMinimumHeight(120);
  connect(m_stagedTree, &QTreeWidget::customContextMenuRequested, this,
          &MainWindow::showStagedContextMenu);
  connect(m_stagedTree, &QTreeWidget::itemClicked, this,
          &MainWindow::onFileClicked);
  connect(
      m_stagedTree, &FileTreeWidget::fileDropped, this,
      [this](const QString &path) {
        if (m_currentPath.isEmpty())
          return;
        if (m_gitExecutor->exec(m_currentPath, {QStringLiteral("add"), path})) {
          loadWorkingTree();
        }
      });

  stagedLayout->addWidget(m_stagedTree);
  rightLayout->addWidget(stagedGroup);

  auto *untrackedGroup = new QGroupBox(tr("Untracked Files"), this);
  auto *untrackedLayout = new QVBoxLayout(untrackedGroup);
  untrackedGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_untrackedTree = new FileTreeWidget(QString(), this);
  m_untrackedTree->setMinimumHeight(120);
  connect(m_untrackedTree, &QTreeWidget::customContextMenuRequested, this,
          &MainWindow::showUntrackedContextMenu);
  untrackedLayout->addWidget(m_untrackedTree);
  rightLayout->addWidget(untrackedGroup);

  auto *messageGroup = new QGroupBox(tr("Commit Message"), this);
  auto *messageLayout = new QVBoxLayout(messageGroup);
  auto *templateLayout = new QHBoxLayout();
  templateLayout->addWidget(new QLabel(tr("Template:"), this));
  m_commitTemplateCombo = new QComboBox(this);
  m_commitTemplateCombo->addItem(tr("Select..."));
  m_commitTemplateCombo->addItems(
      {QStringLiteral("feat: "), QStringLiteral("fix: "),
       QStringLiteral("docs: "), QStringLiteral("style: "),
       QStringLiteral("refactor: "), QStringLiteral("test: "),
       QStringLiteral("chore: ")});
  m_commitTemplateCombo->setSizePolicy(QSizePolicy::Expanding,
                                       QSizePolicy::Fixed);
  templateLayout->addWidget(m_commitTemplateCombo);
  m_commitSpellCheckCheckBox = new QCheckBox(tr("Spell-check"), this);
  templateLayout->addWidget(m_commitSpellCheckCheckBox);
  messageLayout->addLayout(templateLayout);
  m_commitSubject = new QLineEdit(this);
  m_commitSubject->setPlaceholderText(tr("Short summary"));
  connect(m_commitSubject, &QLineEdit::textChanged, this,
          &MainWindow::updateCommitButton);
  messageLayout->addWidget(m_commitSubject);
  m_commitBody = new QTextEdit(this);
  m_commitBody->setPlaceholderText(tr("Long description"));
  m_commitBody->setAcceptRichText(false);
  m_commitBody->setMaximumHeight(120);
  messageLayout->addWidget(m_commitBody);
  m_amendCheckBox = new QCheckBox(tr("Amend last commit"), this);
  messageLayout->addWidget(m_amendCheckBox);
  m_amendWarningLabel = new QLabel(this);
  m_amendWarningLabel->setWordWrap(true);
  m_amendWarningLabel->setVisible(false);
  m_amendWarningLabel->setStyleSheet(
      QStringLiteral("color: #d98b26; font-size: 9pt;"));
  messageLayout->addWidget(m_amendWarningLabel);
  m_signCommitCheckBox = new QCheckBox(tr("Sign with GPG"), this);
  messageLayout->addWidget(m_signCommitCheckBox);
  connect(m_signCommitCheckBox, &QCheckBox::toggled, this,
          &MainWindow::toggleGpgConfig);
  m_commitButton = new QPushButton(tr("Commit"), this);
  m_commitButton->setIcon(
      QApplication::style()->standardIcon(QStyle::SP_DialogOkButton));
  m_commitButton->setEnabled(false);
  m_commitButton->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
  connect(m_commitButton, &QPushButton::clicked, this,
          &MainWindow::onCommitClicked);
  messageLayout->addWidget(m_commitButton);
  rightLayout->addWidget(messageGroup);

  m_spellCheckHighlighter = new SpellCheckHighlighter(m_commitBody->document());
  QSettings settings(QLatin1String("GitClientQt"),
                     QLatin1String("GitClientQt"));
  const QStringList dictPaths = {
      settings.value(QLatin1String("commitMessage/dictionaryPath")).toString(),
      QStringLiteral("/usr/share/dict/words"),
      QStringLiteral("/usr/share/dict/american-english")};
  QString foundDict;
  for (const QString &p : dictPaths) {
    if (!p.isEmpty() && QFileInfo::exists(p)) {
      foundDict = p;
      break;
    }
  }
  m_spellCheckHighlighter->setDictionary(foundDict);
  const bool spellCheckEnabled =
      settings.value(QLatin1String("commitMessage/spellCheckEnabled"), true)
          .toBool();
  m_commitSpellCheckCheckBox->setChecked(spellCheckEnabled);
  m_commitSpellCheckCheckBox->setEnabled(!foundDict.isEmpty());
  m_spellCheckHighlighter->setEnabled(spellCheckEnabled &&
                                      !foundDict.isEmpty());
  connect(m_commitSpellCheckCheckBox, &QCheckBox::toggled, this,
          [this](bool checked) {
            if (m_spellCheckHighlighter)
              m_spellCheckHighlighter->setEnabled(checked);
            QSettings(QLatin1String("GitClientQt"),
                      QLatin1String("GitClientQt"))
                .setValue(QLatin1String("commitMessage/spellCheckEnabled"),
                          checked);
          });
  connect(m_commitTemplateCombo, QOverload<int>::of(&QComboBox::activated),
          this, [this](int index) {
            if (index <= 0 || !m_commitTemplateCombo || !m_commitSubject)
              return;
            m_commitSubject->insert(m_commitTemplateCombo->itemText(index));
            m_commitTemplateCombo->setCurrentIndex(0);
          });

  auto *commitFilesGroup = new QGroupBox(tr("Commit Files"), this);
  commitFilesGroup->setSizePolicy(QSizePolicy::Expanding,
                                  QSizePolicy::Expanding);
  auto *commitFilesLayout = new QVBoxLayout(commitFilesGroup);
  m_commitFilesTree = new FileTreeWidget(QString(), this);
  m_commitFilesTree->setMinimumHeight(150);
  connect(m_commitFilesTree, &QTreeWidget::customContextMenuRequested, this,
          &MainWindow::showCommitFilesContextMenu);
  connect(m_commitFilesTree, &QTreeWidget::itemClicked, this,
          &MainWindow::onCommitFileClicked);
  commitFilesLayout->addWidget(m_commitFilesTree);
  showEmptyCommitFiles();
  rightLayout->addWidget(commitFilesGroup);

  rightLayout->setStretchFactor(unstagedGroup, 2);
  rightLayout->setStretchFactor(stagedGroup, 2);
  rightLayout->setStretchFactor(commitFilesGroup, 2);

  auto *rightScroll = new QScrollArea(this);
  rightScroll->setWidgetResizable(true);
  rightScroll->setFrameShape(QFrame::NoFrame);
  rightScroll->setWidget(rightWidget);
  rightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  rightDock->setWidget(rightScroll);
  rightDock->setTitleBarWidget(new QWidget(rightDock));
  rightDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
  rightDock->setMinimumWidth(320);
  m_workTreeDock = rightDock;
  m_mainSplitter->addWidget(rightDock);

  auto *mainContainer = new QWidget(this);
  auto *mainContainerLayout = new QVBoxLayout(mainContainer);
  mainContainerLayout->setContentsMargins(8, 10, 8, 8);
  mainContainerLayout->setSpacing(0);
  mainContainerLayout->addWidget(m_mainSplitter);
  setCentralWidget(mainContainer);

  m_grepDock = new QDockWidget(tr("Grep"), this);
  m_grepDock->setObjectName(QStringLiteral("grepDock"));
  m_grepDock->setMinimumHeight(200);
  auto *grepWidget = new QWidget(this);
  auto *grepLayout = new QVBoxLayout(grepWidget);
  grepLayout->setContentsMargins(4, 4, 4, 4);
  grepLayout->setSpacing(4);

  auto *inputLayout = new QHBoxLayout();
  inputLayout->setSpacing(4);
  m_grepEdit = new QLineEdit(this);
  m_grepEdit->setPlaceholderText(tr("Search pattern"));
  m_grepEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  inputLayout->addWidget(m_grepEdit);
  auto *grepButton = new QPushButton(tr("Search"), this);
  grepButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  connect(grepButton, &QPushButton::clicked, this,
          &MainWindow::onGrepRequested);
  connect(m_grepEdit, &QLineEdit::returnPressed, this,
          &MainWindow::onGrepRequested);
  inputLayout->addWidget(grepButton);
  grepLayout->addLayout(inputLayout);

  m_grepResults = new QTreeWidget(this);
  m_grepResults->setMinimumHeight(120);
  m_grepResults->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_grepResults->setColumnCount(3);
  m_grepResults->setHeaderLabels(
      QStringList{tr("File"), tr("Line"), tr("Match")});
  m_grepResults->setRootIsDecorated(false);
  m_grepResults->setContextMenuPolicy(Qt::NoContextMenu);
  connect(m_grepResults, &QTreeWidget::itemActivated, this,
          &MainWindow::onGrepResultActivated);
  grepLayout->addWidget(m_grepResults);
  grepLayout->setStretch(0, 0);
  grepLayout->setStretch(1, 1);
  m_grepDock->setWidget(grepWidget);
  m_grepDock->setFeatures(QDockWidget::DockWidgetMovable |
                          QDockWidget::DockWidgetFloatable);
  m_grepDock->setVisible(false);
  addDockWidget(Qt::RightDockWidgetArea, m_grepDock);

  m_commandLogDock = new QDockWidget(tr("Command Log"), this);
  m_commandLogDock->setObjectName(QStringLiteral("commandLogDock"));
  auto *commandLogWidget = new QWidget(this);
  auto *commandLogLayout = new QVBoxLayout(commandLogWidget);
  commandLogLayout->setContentsMargins(4, 4, 4, 4);
  commandLogLayout->setSpacing(4);
  m_commandLogEdit = new QTextEdit(this);
  m_commandLogEdit->setReadOnly(true);
  m_commandLogEdit->setFont(Theme::monospaceFont());
  commandLogLayout->addWidget(m_commandLogEdit);
  auto *runnerLayout = new QHBoxLayout();
  runnerLayout->setSpacing(4);
  runnerLayout->addWidget(new QLabel(tr("git"), this));
  m_arbitraryGitCommandEdit = new QLineEdit(this);
  m_arbitraryGitCommandEdit->setPlaceholderText(tr("status --short"));
  runnerLayout->addWidget(m_arbitraryGitCommandEdit);
  m_runGitCommandButton = new QPushButton(tr("Run"), this);
  runnerLayout->addWidget(m_runGitCommandButton);
  commandLogLayout->addLayout(runnerLayout);
  m_commandLogDock->setWidget(commandLogWidget);
  m_commandLogDock->setFeatures(QDockWidget::DockWidgetMovable |
                                QDockWidget::DockWidgetFloatable |
                                QDockWidget::DockWidgetClosable);
  m_commandLogDock->setVisible(false);
  addDockWidget(Qt::BottomDockWidgetArea, m_commandLogDock);

  connect(m_gitExecutor, &GitExecutor::commandLogged, this,
          [this](const QString &command, const QString &output, int exitCode) {
            if (!m_commandLogEdit)
              return;
            m_commandLogEdit->append(
                tr("> %1 (exit %2)").arg(command).arg(exitCode));
            if (!output.isEmpty())
              m_commandLogEdit->append(output);
            m_commandLogEdit->append(QString());
          });

  auto runArbitraryGitCommand = [this] {
    if (!m_arbitraryGitCommandEdit || m_currentPath.isEmpty())
      return;
    QString text = m_arbitraryGitCommandEdit->text().trimmed();
    if (text.isEmpty())
      return;
    QStringList tokens = text.split(QRegularExpression(QStringLiteral("\\s+")),
                                    Qt::SkipEmptyParts);
    if (!tokens.isEmpty() && tokens.first() == QStringLiteral("git"))
      tokens.removeFirst();
    if (tokens.isEmpty())
      return;
    m_gitExecutor->exec(m_currentPath, tokens);
    m_arbitraryGitCommandEdit->clear();
    if (m_commandLogDock)
      m_commandLogDock->setVisible(true);
  };
  connect(m_runGitCommandButton, &QPushButton::clicked, this,
          runArbitraryGitCommand);
  connect(m_arbitraryGitCommandEdit, &QLineEdit::returnPressed, this,
          runArbitraryGitCommand);

  auto *viewMenu = new QMenu(tr("&View"), this);
  menuBar()->insertMenu(ui->menuHelp->menuAction(), viewMenu);

  auto addVisibilityAction = [&](const QString &title, QDockWidget *dock) {
    auto *action = viewMenu->addAction(title);
    action->setCheckable(true);
    action->setChecked(dock->isVisible());
    connect(action, &QAction::toggled, dock, &QDockWidget::setVisible);
    connect(dock, &QDockWidget::visibilityChanged, action,
            &QAction::setChecked);
  };

  addVisibilityAction(tr("Repository panel"), m_repoDock);
  addVisibilityAction(tr("Working tree"), m_workTreeDock);
  addVisibilityAction(tr("Grep"), m_grepDock);
  addVisibilityAction(tr("Command log"), m_commandLogDock);

  connect(ui->actionOpen, &QAction::triggered, this, [this] {
    const QString path =
        QFileDialog::getExistingDirectory(this, tr("Open Repository"));
    if (path.isEmpty())
      return;
    if (m_gitExecutor->exec(path, {"rev-parse", "--git-dir"})) {
      loadRepository(path);
    } else {
      statusBar()->showMessage(
          tr("The selected folder is not a Git repository."), 0);
    }
  });

  connect(ui->actionClose, &QAction::triggered, this, [this] {
    if (!m_currentPath.isEmpty())
      saveDockAndColumnState(false);
    m_currentPath.clear();
    m_repoPanel->clear();
    m_localBranchesItem = nullptr;
    m_remoteBranchesItem = nullptr;
    m_tagsItem = nullptr;
    m_stashesItem = nullptr;
    m_worktreesItem = nullptr;
    m_submodulesItem = nullptr;
    m_localHeadSha.clear();
    m_remoteHeadSha.clear();
    m_remoteBranchName.clear();
    m_unpushedShas.clear();
    m_unpulledShas.clear();
    m_commitTable->clear();
    m_commitTable->setRowCount(0);
    m_commitTable->horizontalHeader()->setVisible(false);
    if (m_centralStack)
      m_centralStack->setCurrentIndex(0);
    if (m_unstagedTree)
      m_unstagedTree->clear();
    if (m_stagedTree)
      m_stagedTree->clear();
    if (m_diffView)
      showEmptyDiff();
    if (m_commitBody)
      m_commitBody->clear();
    if (m_commitFilesTree)
      showEmptyCommitFiles();
    m_selectedCommitSha.clear();
    if (m_pushButton)
      m_pushButton->setEnabled(false);
    if (m_pullButton)
      m_pullButton->setEnabled(false);
    setWindowTitle(tr("Git Client Qt"));
    if (m_branchLabel)
      m_branchLabel->clear();
    statusBar()->showMessage(tr("Repository closed"));
  });

  connect(ui->actionExit, &QAction::triggered, qApp, &QApplication::quit,
          Qt::QueuedConnection);
  connect(ui->actionAbout, &QAction::triggered, this,
          [this] { QMessageBox::about(this, tr("About"), tr("Git Client")); });
  connect(ui->actionPreferences, &QAction::triggered, this,
          &MainWindow::showPreferences);

  restoreSettings();
  applyFonts();
  loadRepository(m_currentPath);

  auto *reloadShortcut = new QShortcut(QKeySequence(Qt::Key_F5), this);
  connect(reloadShortcut, &QShortcut::activated, this, [this] {
    if (!m_currentPath.isEmpty())
      loadRepository(m_currentPath);
  });

  restoreDockAndColumnState();
}

MainWindow::~MainWindow() {
  saveDockAndColumnState();
  saveOpenTabs();
  delete ui;
}

void MainWindow::restoreSettings() {
  QSettings settings("GitClientQt", "GitClientQt");
  if (settings.value("reopenLastRepo", true).toBool()) {
    if (!settings.value("openRepos").toStringList().isEmpty() &&
        m_currentPath.isEmpty())
      restoreOpenTabs();
    else if (!settings.value("lastRepo").toString().isEmpty() &&
             m_currentPath.isEmpty())
      m_currentPath = settings.value("lastRepo").toString();
  }

  const QString pullMode =
      settings.value("pullMode", QStringLiteral("ffIfPossible")).toString();
  if (m_pullButton && m_pullButton->menu()) {
    for (QAction *action : m_pullButton->menu()->actions()) {
      if (action->data().toString() == pullMode) {
        action->setChecked(true);
        break;
      }
    }
  }

  if (pullMode == QLatin1String("ffOnly")) {
    m_pullArgs.clear();
    m_pullArgs << "pull" << "--ff-only";
  } else if (pullMode == QLatin1String("rebase")) {
    m_pullArgs.clear();
    m_pullArgs << "pull" << "--rebase";
  } else {
    m_pullArgs.clear();
    m_pullArgs << "pull";
  }

  for (QAction *action : findChildren<QAction *>()) {
    QMenu *menu = qobject_cast<QMenu *>(action->parent());
    if (!menu)
      continue;
    QString menuName = menu->title();
    menuName.remove('&');
    QString text = action->text();
    text.remove('&');
    if (text.isEmpty())
      continue;
    const QString key =
        QLatin1String("shortcuts/") + menuName + QLatin1Char('/') + text;
    if (settings.contains(key))
      action->setShortcut(QKeySequence(settings.value(key).toString()));
  }
}

void MainWindow::savePullMode() {
  QSettings settings("GitClientQt", "GitClientQt");
  QString mode = QStringLiteral("ffIfPossible");
  if (m_pullArgs.first() == QLatin1String("fetch"))
    mode = QStringLiteral("fetchAll");
  else if (m_pullArgs.contains(QLatin1String("--rebase")))
    mode = QStringLiteral("rebase");
  else if (m_pullArgs.contains(QLatin1String("--ff-only")))
    mode = QStringLiteral("ffOnly");
  settings.setValue("pullMode", mode);
}

QString MainWindow::currentBranchName() const {
  if (m_currentPath.isEmpty())
    return QString();
  return m_gitExecutor
      ->run(m_currentPath, {"rev-parse", "--abbrev-ref", "HEAD"})
      .value(0);
}

bool MainWindow::isHeadPushed() const {
  if (m_remoteBranchName.isEmpty() || m_localHeadSha.isEmpty())
    return false;
  return !m_unpushedShas.contains(m_localHeadSha);
}

bool MainWindow::isProtectedBranch(const QString &branch) const {
  if (branch.isEmpty())
    return false;
  const QSettings settings(QStringLiteral("GitClientQt"),
                           QStringLiteral("GitClientQt"));
  const QStringList protectedBranches =
      settings
          .value(QStringLiteral("protectedBranches"),
                 QStringList{QStringLiteral("main"), QStringLiteral("master"),
                             QStringLiteral("develop")})
          .toStringList();
  for (const QString &candidate : protectedBranches) {
    if (branch.compare(candidate.trimmed(), Qt::CaseInsensitive) == 0)
      return true;
  }
  return false;
}

void MainWindow::updateAmendWarning() {
  if (!m_amendWarningLabel)
    return;

  const bool amending = m_amendCheckBox && m_amendCheckBox->isChecked();
  if (!amending || !isHeadPushed()) {
    m_amendWarningLabel->setVisible(false);
    return;
  }

  const QString branch = currentBranchName();
  if (isProtectedBranch(branch)) {
    m_amendWarningLabel->setText(
        tr("This commit is already pushed to the protected branch '%1'. "
           "Amending rewrites published history and force pushing is blocked.")
            .arg(branch));
  } else {
    m_amendWarningLabel->setText(
        tr("This commit is already pushed. Amending rewrites published "
           "history and requires a force push."));
  }
  m_amendWarningLabel->setVisible(true);
}

void MainWindow::applyFonts() {
  const QFont menu = Theme::menuFont();
  if (QMenuBar *bar = menuBar()) {
    bar->setFont(menu);
    for (QAction *action : bar->actions()) {
      if (QMenu *subMenu = action->menu())
        subMenu->setFont(menu);
    }
  }

  const QFont mono = Theme::monospaceFont();
  if (m_diffPresenter)
    m_diffPresenter->setMonospaceFont(mono.family(), mono.pointSize());
  if (m_diffView)
    m_diffView->setFont(mono);
  if (m_commandLogEdit)
    m_commandLogEdit->setFont(mono);
  if (m_commitTable) {
    for (int row = 0; row < m_commitTable->rowCount(); ++row) {
      if (QTableWidgetItem *item = m_commitTable->item(row, 6))
        item->setFont(mono);
    }
  }
}

void MainWindow::saveDockAndColumnState(bool includeGeometry) {
  QSettings settings("GitClientQt", "GitClientQt");
  if (includeGeometry)
    settings.setValue("mainWindow/geometry", saveGeometry());

  const QString repoKey =
      m_currentPath.isEmpty()
          ? QStringLiteral("default")
          : QCryptographicHash::hash(m_currentPath.toUtf8(),
                                     QCryptographicHash::Sha1)
                .toHex();
  const QString base =
      QLatin1String("dockLayouts/") + repoKey + QLatin1Char('/');
  const QByteArray state = saveState(0);
  settings.setValue(QLatin1String("mainWindow/dockState"), state);
  if (m_mainSplitter)
    settings.setValue(QLatin1String("mainWindow/mainSplitterState"),
                      m_mainSplitter->saveState());

  if (m_commitTable) {
    QVariantList widths;
    QVariantList visibility;
    for (int c = 0; c < m_commitTable->columnCount(); ++c) {
      widths << m_commitTable->columnWidth(c);
      visibility << !m_commitTable->isColumnHidden(c);
    }
    settings.setValue(
        QLatin1String("dockLayouts/global/columnWidths/commitTable"), widths);
    settings.setValue(
        QLatin1String("dockLayouts/global/columnVisibility/commitTable"),
        visibility);
  }
  if (m_unstagedTree)
    settings.setValue(base + QLatin1String("headers/unstagedTree"),
                      m_unstagedTree->header()->saveState());
  if (m_stagedTree)
    settings.setValue(base + QLatin1String("headers/stagedTree"),
                      m_stagedTree->header()->saveState());
  if (m_commitFilesTree)
    settings.setValue(base + QLatin1String("headers/commitFilesTree"),
                      m_commitFilesTree->header()->saveState());
  if (m_grepResults)
    settings.setValue(base + QLatin1String("headers/grepResults"),
                      m_grepResults->header()->saveState());
}

bool MainWindow::restoreDockAndColumnState(bool includeGeometry) {
  bool restoredCommitTableWidths = false;
  QSettings settings("GitClientQt", "GitClientQt");

  if (includeGeometry) {
    const QByteArray geometry =
        settings.value("mainWindow/geometry").toByteArray();
    if (!geometry.isEmpty())
      restoreGeometry(geometry);

    QScreen *screen = QGuiApplication::screenAt(pos());
    if (!screen)
      screen = QGuiApplication::primaryScreen();
    const QRect windowRect = QRect(pos(), size());
    const QRect available = screen ? screen->availableGeometry() : QRect();
    if (available.isValid() && !available.contains(windowRect)) {
      const int w = qMax(640, qMin(1280, available.width() - 200));
      const int h = qMax(480, qMin(800, available.height() - 200));
      const int x = available.x() + (available.width() - w) / 2;
      const int y = available.y() + (available.height() - h) / 2;
      setGeometry(x, y, w, h);
    }
  }

  const QString repoKey =
      m_currentPath.isEmpty()
          ? QStringLiteral("default")
          : QCryptographicHash::hash(m_currentPath.toUtf8(),
                                     QCryptographicHash::Sha1)
                .toHex();
  const QString base =
      QLatin1String("dockLayouts/") + repoKey + QLatin1Char('/');

  const QByteArray state =
      settings.value(QLatin1String("mainWindow/dockState")).toByteArray();
  if (!state.isEmpty())
    restoreState(state, 0);

  if (m_mainSplitter) {
    const QByteArray splitterState =
        settings.value(QLatin1String("mainWindow/mainSplitterState"))
            .toByteArray();
    if (!splitterState.isEmpty())
      m_mainSplitter->restoreState(splitterState);
  }

  if (m_diffDock && m_diffDock->isFloating())
    m_diffDock->setFloating(false);

  if (m_repoDock)
    m_repoDock->setVisible(true);
  if (m_workTreeDock)
    m_workTreeDock->setVisible(true);
  if (m_diffDock)
    m_diffDock->setVisible(false);
  if (m_grepDock)
    m_grepDock->setVisible(false);
  if (m_commandLogDock)
    m_commandLogDock->setVisible(false);

  const QHash<QTreeWidget *, QString> trees = {
      {m_unstagedTree, QLatin1String("headers/unstagedTree")},
      {m_stagedTree, QLatin1String("headers/stagedTree")},
      {m_commitFilesTree, QLatin1String("headers/commitFilesTree")},
      {m_grepResults, QLatin1String("headers/grepResults")}};
  for (auto it = trees.cbegin(); it != trees.cend(); ++it) {
    QTreeWidget *tree = it.key();
    if (tree) {
      const QByteArray headerState =
          settings.value(base + it.value()).toByteArray();
      if (!headerState.isEmpty())
        tree->header()->restoreState(headerState);
    }
  }

  if (m_commitTable) {
    const QVariantList widths =
        settings
            .value(QLatin1String("dockLayouts/global/columnWidths/commitTable"))
            .toList();
    for (int c = 0; c < widths.size() && c < m_commitTable->columnCount(); ++c)
      m_commitTable->setColumnWidth(c, widths.at(c).toInt());
    const QVariantList visibility =
        settings
            .value(QLatin1String(
                "dockLayouts/global/columnVisibility/commitTable"))
            .toList();
    for (int c = 0; c < visibility.size() && c < m_commitTable->columnCount();
         ++c)
      m_commitTable->setColumnHidden(c, !visibility.at(c).toBool());
    restoredCommitTableWidths = !widths.isEmpty();
  }

  return restoredCommitTableWidths;
}

void MainWindow::updateRecentRepos() {
  if (!m_recentMenu)
    return;
  m_recentMenu->clear();
  QSettings settings("GitClientQt", "GitClientQt");
  const QStringList recent = settings.value("recentRepos").toStringList();
  if (recent.isEmpty()) {
    auto *none = m_recentMenu->addAction(tr("No recent repositories"));
    none->setEnabled(false);
    return;
  }
  for (const QString &repo : recent) {
    if (repo.isEmpty())
      continue;
    auto *action = m_recentMenu->addAction(QFileInfo(repo).fileName());
    action->setToolTip(repo);
    connect(action, &QAction::triggered, this,
            [this, repo] { loadRepository(repo); });
  }
  m_recentMenu->addSeparator();
  auto *clearAction = m_recentMenu->addAction(tr("Clear Recent Repositories"));
  connect(clearAction, &QAction::triggered, this, [this] {
    QSettings settings("GitClientQt", "GitClientQt");
    settings.remove("recentRepos");
    updateRecentRepos();
  });
}

bool MainWindow::repositoryStateChanged() const {
  if (m_currentPath.isEmpty())
    return true;
  return m_gitRepository->stateSignature() != m_lastRepoSignature;
}

void MainWindow::loadRepository(const QString &path, bool updateTab) {
  if (path.isEmpty())
    return;

  const bool switchingRepo = !m_currentPath.isEmpty() && m_currentPath != path;
  if (switchingRepo) {
    saveDockAndColumnState(false);
    showEmptyDiff();
    m_currentDiffPath.clear();
    m_currentDiffLines.clear();
    m_selectedCommitSha.clear();
    showEmptyCommitFiles();
  }

  m_gitRepository->setPath(path);
  if (!m_gitRepository->isValid()) {
    statusBar()->showMessage(tr("Not a git repository: %1").arg(path));
    return;
  }

  const bool isInitialLoad = m_currentPath.isEmpty();
  const QString repoRoot = m_gitRepository->root();
  if (repoRoot.isEmpty()) {
    statusBar()->showMessage(
        tr("Could not determine repository root for %1").arg(path));
    return;
  }
  m_currentPath = repoRoot;
  m_gitRepository->setPath(m_currentPath);
  if (updateTab)
    activateRepositoryTab(m_currentPath);
  if (m_centralStack)
    m_centralStack->setCurrentIndex(1);
  if (m_watcher) {
    m_watcher->removePaths(m_watcher->directories());
    m_watcher->removePaths(m_watcher->files());
    const QStringList watchPaths = {
        m_currentPath,
        m_currentPath + QLatin1String("/.git"),
        m_currentPath + QLatin1String("/.git/HEAD"),
        m_currentPath + QLatin1String("/.git/index"),
        m_currentPath + QLatin1String("/.git/packed-refs"),
        m_currentPath + QLatin1String("/.git/refs"),
        m_currentPath + QLatin1String("/.git/refs/heads"),
        m_currentPath + QLatin1String("/.git/refs/remotes"),
        m_currentPath + QLatin1String("/.git/refs/tags"),
    };
    for (const QString &p : watchPaths) {
      if (QFileInfo::exists(p))
        m_watcher->addPath(p);
    }
  }
  QSettings settings("GitClientQt", "GitClientQt");
  settings.setValue("lastRepo", m_currentPath);

  QStringList recent = settings.value("recentRepos").toStringList();
  recent.removeAll(m_currentPath);
  recent.prepend(m_currentPath);
  while (recent.size() > 10)
    recent.removeLast();
  settings.setValue("recentRepos", recent);
  updateRecentRepos();
  setWindowTitle(QFileInfo(m_currentPath).fileName() + " - " +
                 tr("Git Client Qt"));
  if (m_pushButton)
    m_pushButton->setEnabled(true);
  if (m_undoButton)
    m_undoButton->setEnabled(true);
  if (m_pullButton)
    m_pullButton->setEnabled(true);
  if (m_signCommitCheckBox) {
    const QString gpgSign =
        m_gitExecutor->run(path, {"config", "--bool", "commit.gpgsign"})
            .value(0);
    m_signCommitCheckBox->blockSignals(true);
    m_signCommitCheckBox->setChecked(gpgSign == QLatin1String("true"));
    m_signCommitCheckBox->blockSignals(false);
  }
  m_repoPanel->clear();

  const QString currentBranch =
      m_gitExecutor->run(path, {"rev-parse", "--abbrev-ref", "HEAD"}).value(0);

  m_localBranchesItem =
      new QTreeWidgetItem(m_repoPanel, {tr("Local Branches")});
  for (const QString &line : m_gitExecutor->run(
           path, {QStringLiteral("for-each-ref"),
                  QStringLiteral("--format=%(refname:short)|%(upstream:track)"),
                  QStringLiteral("refs/heads")})) {
    const QStringList fields = line.split(QLatin1Char('|'));
    const QString branch = fields.value(0);
    const QString track = fields.value(1).trimmed();
    int ahead = 0;
    int behind = 0;
    if (!track.isEmpty()) {
      static const QRegularExpression aheadRe(
          QStringLiteral("ahead\\s+(\\d+)"));
      static const QRegularExpression behindRe(
          QStringLiteral("behind\\s+(\\d+)"));
      const QRegularExpressionMatch aheadMatch = aheadRe.match(track);
      const QRegularExpressionMatch behindMatch = behindRe.match(track);
      if (aheadMatch.hasMatch())
        ahead = aheadMatch.captured(1).toInt();
      if (behindMatch.hasMatch())
        behind = behindMatch.captured(1).toInt();
    }
    QString display = branch;
    if (ahead > 0 || behind > 0)
      display += QStringLiteral(" [+%1 -%2]").arg(ahead).arg(behind);
    auto *item = new QTreeWidgetItem(m_localBranchesItem, QStringList{display});
    item->setData(0, Qt::UserRole, branch);
    if (branch == currentBranch) {
      QFont font = item->font(0);
      font.setBold(true);
      item->setFont(0, font);
    }
  }
  m_localBranchesItem->setExpanded(true);

  m_remoteBranchesItem =
      new QTreeWidgetItem(m_repoPanel, {tr("Remote Branches")});
  QMap<QString, QTreeWidgetItem *> remoteGroups;
  for (const QString &branch : m_gitExecutor->run(
           path, {"branch", "-r", "--format=%(refname:short)"})) {
    const QString remote = branch.section('/', 0, 0);
    const QString branchName = branch.section('/', 1);
    if (branchName.isEmpty() || branchName == "HEAD") {
      continue;
    }
    QTreeWidgetItem *remoteItem = remoteGroups.value(remote);
    if (!remoteItem) {
      remoteItem =
          new QTreeWidgetItem(m_remoteBranchesItem, QStringList{remote});
      remoteGroups.insert(remote, remoteItem);
    }
    new QTreeWidgetItem(remoteItem, QStringList{branchName});
  }
  for (auto it = remoteGroups.begin(); it != remoteGroups.end(); ++it) {
    it.value()->setExpanded(true);
  }
  m_remoteBranchesItem->setExpanded(true);

  m_remotesItem = new QTreeWidgetItem(m_repoPanel, {tr("Remotes")});
  loadRemotes();
  m_remotesItem->setExpanded(true);

  m_tagsItem = new QTreeWidgetItem(m_repoPanel, {tr("Tags")});
  const QStringList remotes = m_gitExecutor->run(path, {"remote"});
  QSet<QString> remoteTags;
  bool canCheckLocal = remotes.isEmpty();
  for (const QString &remote : remotes) {
    QString output;
    if (m_gitExecutor->exec(path, {"ls-remote", "--tags", remote}, &output)) {
      canCheckLocal = true;
      for (const QString &line : output.split('\n', Qt::SkipEmptyParts)) {
        const QString ref = line.section(QLatin1Char('\t'), 1, 1);
        if (ref.startsWith(QLatin1String("refs/tags/"))) {
          QString tag = ref.mid(10);
          if (tag.endsWith(QLatin1String("^{}")))
            tag.chop(3);
          remoteTags.insert(tag);
        }
      }
    }
  }
  for (const QString &tag : m_gitExecutor->run(path, {"tag", "--list"})) {
    auto *tagItem = new QTreeWidgetItem(m_tagsItem, QStringList{tag});
    if (canCheckLocal && !remoteTags.contains(tag)) {
      tagItem->setText(0, tag + tr(" (local)"));
      tagItem->setForeground(0, QColor(160, 160, 160));
    }
    tagItem->setData(0, Qt::UserRole, tag);
  }
  m_tagsItem->setExpanded(true);

  m_submodulesItem = new QTreeWidgetItem(m_repoPanel, {tr("Submodules")});
  for (const QString &line :
       m_gitExecutor->run(path, {"submodule", "status", "--recursive"})) {
    if (line.length() < 2)
      continue;
    const QChar status = line.at(0);
    const QString rest = line.mid(1).trimmed();
    const int shaEnd = rest.indexOf(' ');
    if (shaEnd < 0)
      continue;
    const QString subPath = rest.mid(shaEnd + 1).section('(', 0, 0).trimmed();
    QString text = subPath;
    if (status == '+' || status == 'U')
      text += tr(" (needs update)");
    else if (status == '-')
      text += tr(" (not initialized)");
    auto *subItem = new QTreeWidgetItem(m_submodulesItem, QStringList{text});
    subItem->setData(0, Qt::UserRole, subPath);
  }
  m_submodulesItem->setExpanded(true);

  m_stashesItem = new QTreeWidgetItem(m_repoPanel, {tr("Stashes")});
  loadStashes();
  m_stashesItem->setExpanded(true);

  m_worktreesItem = new QTreeWidgetItem(m_repoPanel, {tr("Worktrees")});
  loadWorktrees();
  m_worktreesItem->setExpanded(true);

  QVector<bool> savedHidden(m_commitTable->columnCount(), false);
  for (int c = 0; c < m_commitTable->columnCount(); ++c)
    savedHidden[c] = m_commitTable->isColumnHidden(c);

  m_commitTable->setRowCount(0);
  m_commitTable->setHorizontalHeaderLabels(
      {tr("Graph"), tr("Date/Time"), tr("Date"), tr("Commit Message"),
       tr("Author"), tr("Branches"), tr("SHA")});
  m_commitTable->horizontalHeader()->setVisible(true);
  m_commitTable->horizontalHeader()->viewport()->update();

  for (int c = 0; c < m_commitTable->columnCount() && c < savedHidden.size();
       ++c)
    m_commitTable->setColumnHidden(c, savedHidden[c]);

  QMap<QString, int> wipCounts;
  for (const QString &line :
       m_gitExecutor->run(path, {"status", "--porcelain"})) {
    if (line.length() < 2)
      continue;
    const QChar indexStatus = line.at(0);
    const QChar workTreeStatus = line.at(1);
    if (indexStatus == ' ' && workTreeStatus == ' ')
      continue;
    const QString status = (workTreeStatus != ' ') ? QString(workTreeStatus)
                                                   : QString(indexStatus);
    ++wipCounts[status];
  }

  if (!wipCounts.isEmpty()) {
    m_commitTable->insertRow(0);

    auto *wipGraphItem = new QTableWidgetItem(QStringLiteral("●"));
    wipGraphItem->setTextAlignment(Qt::AlignCenter);
    wipGraphItem->setForeground(QBrush(QColor(120, 120, 120)));
    wipGraphItem->setFlags(wipGraphItem->flags() & ~Qt::ItemIsSelectable);
    m_commitTable->setItem(0, 0, wipGraphItem);

    auto *wipDateTimeItem = new QTableWidgetItem();
    wipDateTimeItem->setFlags(wipDateTimeItem->flags() & ~Qt::ItemIsSelectable);
    m_commitTable->setItem(0, 1, wipDateTimeItem);

    auto *wipRelativeItem = new QTableWidgetItem();
    wipRelativeItem->setFlags(wipRelativeItem->flags() & ~Qt::ItemIsSelectable);
    m_commitTable->setItem(0, 2, wipRelativeItem);

    auto *wipWidget = new QWidget(m_commitTable);
    auto *wipLayout = new QHBoxLayout(wipWidget);
    wipLayout->setContentsMargins(4, 2, 4, 2);
    wipLayout->setSpacing(6);
    const QStringList order = {"M", "A", "D", "R", "C", "T", "?"};
    for (const QString &status : order) {
      if (!wipCounts.contains(status))
        continue;
      auto *iconLabel = new QLabel(wipWidget);
      iconLabel->setPixmap(FileTreeWidget::statusIcon(status).pixmap(12, 12));
      wipLayout->addWidget(iconLabel);
      wipLayout->addWidget(
          new QLabel(QString::number(wipCounts.value(status)), wipWidget));
    }
    wipLayout->addStretch();
    m_commitTable->setCellWidget(0, 3, wipWidget);

    auto *wipAuthorItem = new QTableWidgetItem();
    wipAuthorItem->setFlags(wipAuthorItem->flags() & ~Qt::ItemIsSelectable);
    m_commitTable->setItem(0, 4, wipAuthorItem);

    auto *wipBranchesItem = new QTableWidgetItem();
    wipBranchesItem->setFlags(wipBranchesItem->flags() & ~Qt::ItemIsSelectable);
    m_commitTable->setItem(0, 5, wipBranchesItem);
  }

  int graphColumnWidth = 0;

  QProcess p;
  p.start("git",
          QStringList{"-C", path} +
              QStringList{"log", "--branches", "--tags", "--remotes", "--graph",
                          "--source", "--date-order",
                          "--date=format:%Y-%m-%d %H:%M:%S",
                          "--pretty=format:%x1f%H%x1f%h%x1f%an%x1f%ad%x1f%ar%"
                          "x1f%s%x1f%b%x1f%S%x1f%P%x1e"});
  if (p.waitForFinished(10000) && p.exitCode() == 0) {
    const QString output =
        QString::fromLocal8Bit(p.readAllStandardOutput().trimmed());

    m_localHeadSha = m_gitExecutor->run(path, {"rev-parse", "HEAD"}).value(0);
    m_remoteBranchName =
        m_gitExecutor->run(path, {"rev-parse", "--abbrev-ref", "@{u}"})
            .value(0);
    if (m_remoteBranchName.isEmpty()) {
      const QStringList candidates = {QStringLiteral("origin/") + currentBranch,
                                      QStringLiteral("origin/HEAD")};
      for (const QString &c : candidates) {
        if (!m_gitExecutor->run(path, {"rev-parse", c}).isEmpty()) {
          m_remoteBranchName = c;
          break;
        }
      }
    }
    m_remoteHeadSha =
        !m_remoteBranchName.isEmpty()
            ? m_gitExecutor->run(path, {"rev-parse", m_remoteBranchName})
                  .value(0)
            : QString();
    m_unpushedShas.clear();
    m_unpulledShas.clear();
    QSet<QString> localShas;
    QSet<QString> remoteShas;
    QSet<QString> branchSet;
    QSet<QString> tagSet;
    if (!m_remoteBranchName.isEmpty()) {
      for (const QString &sha : m_gitExecutor->run(
               path, {"log", "--format=%H", m_remoteBranchName + "..HEAD"}))
        m_unpushedShas.insert(sha);
      for (const QString &sha : m_gitExecutor->run(
               path, {"log", "--format=%H", "HEAD.." + m_remoteBranchName}))
        m_unpulledShas.insert(sha);
      for (const QString &sha :
           m_gitExecutor->run(path, {"log", "--format=%H", m_remoteHeadSha}))
        remoteShas.insert(sha);
    }
    for (const QString &sha :
         m_gitExecutor->run(path, {"log", "--format=%H", "HEAD"}))
      localShas.insert(sha);
    for (const QString &ref :
         m_gitExecutor->run(path, {"for-each-ref", "--format=%(refname:short)",
                                   QStringLiteral("refs/heads/"),
                                   QStringLiteral("refs/remotes/")}))
      branchSet.insert(ref);
    for (const QString &ref :
         m_gitExecutor->run(path, {"for-each-ref", "--format=%(refname:short)",
                                   QStringLiteral("refs/tags/")}))
      tagSet.insert(ref);

    QHash<QString, QStringList> tagsBySha;
    for (const QString &line : m_gitExecutor->run(
             path, {"for-each-ref",
                    "--format=%(objectname) %(*objectname) %(refname:short)",
                    QStringLiteral("refs/tags/")})) {
      const QStringList parts =
          line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
      if (parts.size() >= 3)
        tagsBySha[parts.at(1)].append(parts.at(2));
      else if (parts.size() == 2)
        tagsBySha[parts.at(0)].append(parts.at(1));
    }

    if (m_branchLabel) {
      QString branchText = currentBranch;
      if (!m_remoteBranchName.isEmpty()) {
        branchText += QString(" ↑%1 ↓%2")
                          .arg(m_unpushedShas.size())
                          .arg(m_unpulledShas.size());
      }
      m_branchLabel->setText(branchText);
    }

    m_commitModel->loadLog(output);

    QString prevGraph;
    QHash<int, QColor> graphColumnColors;

    QList<Commit> allCommits;
    allCommits.reserve(m_commitModel->rowCount());
    for (int i = 0; i < m_commitModel->rowCount(); ++i)
      allCommits.append(m_commitModel->commit(i));
    const QVector<QVector<int>> laneRows = LaneGraph::build(allCommits);

    for (int r = 0; r < m_commitModel->rowCount(); ++r) {
      const Commit &c = m_commitModel->commit(r);

      QString markers;
      if (c.fullSha == m_localHeadSha)
        markers += " [HEAD]";
      if (!m_remoteBranchName.isEmpty() && c.fullSha == m_remoteHeadSha)
        markers += " [" + m_remoteBranchName + "]";
      if (m_unpushedShas.contains(c.fullSha))
        markers += " ↑";
      if (m_unpulledShas.contains(c.fullSha))
        markers += " ↓";

      QString preview = c.subject;
      if (preview.length() > 60) {
        preview = preview.left(60) + "...";
      }
      preview += markers;

      QString branchText;
      if (branchSet.contains(c.branch) && !tagSet.contains(c.branch))
        branchText = c.branch;
      else if (localShas.contains(c.fullSha))
        branchText = currentBranch;
      else if (!m_remoteHeadSha.isEmpty() && remoteShas.contains(c.fullSha))
        branchText = m_remoteBranchName;

      const QString tip = tr("Subject: %1\n\n%2\n\nDate: %3\nAuthor: "
                             "%4\nBranch: %5\nSHA: %6 (%7)")
                              .arg(c.subject)
                              .arg(c.body)
                              .arg(c.date)
                              .arg(c.author)
                              .arg(branchText)
                              .arg(c.shortSha)
                              .arg(c.fullSha);

      QBrush bgBrush;
      if (m_unpushedShas.contains(c.fullSha))
        bgBrush = QBrush(QColor(225, 255, 225));
      else if (m_unpulledShas.contains(c.fullSha))
        bgBrush = QBrush(QColor(255, 240, 225));

      const int row = m_commitTable->rowCount();
      m_commitTable->insertRow(row);

      const QFont graphFont = Theme::monospaceFont();
      const int rowHeight =
          m_commitTable->verticalHeader()->defaultSectionSize();
      QPixmap graphPixmap;
      if (r < laneRows.size() && !laneRows.at(r).isEmpty()) {
        graphPixmap =
            CommitTableWidget::paintLanes(laneRows.at(r), rowHeight, graphFont,
                                          bgBrush, tagsBySha.value(c.fullSha));
      } else {
        graphPixmap = CommitTableWidget::commitGraphPixmap(
            c.graph, prevGraph, graphColumnColors, rowHeight, graphFont,
            tagsBySha.value(c.fullSha));
      }
      auto *graphItem = new QTableWidgetItem;
      graphItem->setData(Qt::DecorationRole, graphPixmap);
      graphItem->setToolTip(tip);
      graphColumnWidth = qMax(graphColumnWidth, graphPixmap.width());
      if (bgBrush != QBrush())
        graphItem->setBackground(bgBrush);
      m_commitTable->setItem(row, 0, graphItem);

      auto *dateTimeItem = new QTableWidgetItem(c.date);
      dateTimeItem->setToolTip(tip);
      if (bgBrush != QBrush())
        dateTimeItem->setBackground(bgBrush);
      m_commitTable->setItem(row, 1, dateTimeItem);

      auto *relativeDateItem = new QTableWidgetItem(c.relative);
      relativeDateItem->setToolTip(tip);
      if (bgBrush != QBrush())
        relativeDateItem->setBackground(bgBrush);
      m_commitTable->setItem(row, 2, relativeDateItem);

      auto *msgItem = new QTableWidgetItem(preview);
      msgItem->setToolTip(tip);
      if (bgBrush != QBrush())
        msgItem->setBackground(bgBrush);
      m_commitTable->setItem(row, 3, msgItem);

      auto *authorItem = new QTableWidgetItem(c.author);
      authorItem->setToolTip(tip);
      if (bgBrush != QBrush())
        authorItem->setBackground(bgBrush);
      m_commitTable->setItem(row, 4, authorItem);

      auto *branchItem = new QTableWidgetItem(branchText);
      branchItem->setToolTip(tip);
      if (bgBrush != QBrush())
        branchItem->setBackground(bgBrush);
      m_commitTable->setItem(row, 5, branchItem);

      auto *shaItem = new QTableWidgetItem(c.shortSha);
      shaItem->setData(Qt::UserRole, c.fullSha);
      shaItem->setData(Qt::UserRole + 1, c.subject);
      shaItem->setData(Qt::UserRole + 2, c.author);
      shaItem->setToolTip(tip);
      shaItem->setFont(Theme::monospaceFont());
      if (bgBrush != QBrush())
        shaItem->setBackground(bgBrush);
      m_commitTable->setItem(row, 6, shaItem);
    }
  }
  const bool restoredCommitTableWidths =
      switchingRepo ? restoreDockAndColumnState(false) : false;
  if (!restoredCommitTableWidths) {
    m_commitTable->resizeColumnsToContents();
    m_commitTable->setColumnWidth(0, qMax(100, graphColumnWidth + 16));
  }

  if (!m_currentPath.isEmpty() && m_initialRepositoryLoaded)
    saveDockAndColumnState(false);
  m_initialRepositoryLoaded = true;

  int tableWidth =
      m_commitTable->horizontalHeader()->length() +
      QApplication::style()->pixelMetric(QStyle::PM_ScrollBarExtent) +
      2 * m_commitTable->frameWidth();
  if (isInitialLoad) {
    const int leftWidth = qBound(
        80, m_repoPanel ? m_repoPanel->sizeHint().width() + 20 : 180, 200);
    const int rightWidth = 320;
    QScreen *screen = QGuiApplication::primaryScreen();
    const int screenWidth = screen ? screen->availableGeometry().width() : 1280;
    const int maxTableWidth =
        qMax(400, screenWidth - leftWidth - rightWidth - 80);
    tableWidth = qMin(tableWidth, maxTableWidth);
    if (m_mainSplitter)
      m_mainSplitter->setSizes({leftWidth, tableWidth, rightWidth});
    resize(leftWidth + tableWidth + rightWidth, height());
  }
  updateFilter();
  updateAmendWarning();

  if (m_commitTable) {
    if (m_repoSelectedShas.contains(m_currentPath)) {
      const QString savedSha = m_repoSelectedShas.value(m_currentPath);
      if (savedSha.isEmpty()) {
        if (m_commitTable->rowCount() > 0) {
          const int row = 0;
          if (QTableWidgetItem *item = m_commitTable->item(row, 0)) {
            m_commitTable->selectRow(row);
            m_commitTable->scrollToItem(item, QAbstractItemView::EnsureVisible);
            onCommitSelected(item);
          }
        }
      } else {
        for (int row = 0; row < m_commitTable->rowCount(); ++row) {
          if (QTableWidgetItem *shaItem = m_commitTable->item(row, 6)) {
            if (shaItem->data(Qt::UserRole).toString() == savedSha) {
              m_commitTable->selectRow(row);
              m_commitTable->scrollToItem(shaItem,
                                          QAbstractItemView::EnsureVisible);
              if (QTableWidgetItem *item = m_commitTable->item(row, 0))
                onCommitSelected(item);
              break;
            }
          }
        }
      }
    }

    const int savedHScroll = m_repoHorizontalScroll.value(m_currentPath, 0);
    if (m_commitTable->horizontalScrollBar()) {
      QTimer::singleShot(0, this, [this, savedHScroll]() {
        if (m_commitTable && m_commitTable->horizontalScrollBar())
          m_commitTable->horizontalScrollBar()->setValue(savedHScroll);
      });
    }
  }

  loadWorkingTree();

  const QString upstream =
      m_gitExecutor->run(path, {"rev-parse", "@{u}"}).value(0);
  m_lastRepoSignature =
      m_localHeadSha + '|' + upstream + '|' +
      m_gitExecutor->run(path, {"status", "--porcelain"}).join('\n') + '|' +
      m_gitExecutor->run(path, {"tag", "--list"}).join('\n');

  statusBar()->showMessage(tr("Loaded: %1").arg(path));
}

void MainWindow::openInExternalEditor(const QString &filePath) const {
  QSettings settings("GitClientQt", "GitClientQt");
  const QString editor =
      settings.value(QStringLiteral("external/editor")).toString().trimmed();
  if (editor.isEmpty()) {
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    return;
  }

  QStringList tokens = editor.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  if (tokens.isEmpty())
    return;
  const QString program = tokens.takeFirst();
  tokens.append(filePath);
  if (!QProcess::startDetached(program, tokens))
    const_cast<MainWindow *>(this)->statusBar()->showMessage(
        tr("Failed to start external editor"));
}

QString MainWindow::configuredDiffTool() const {
  QSettings settings("GitClientQt", "GitClientQt");
  QString userTool =
      settings.value(QStringLiteral("external/diffTool")).toString().trimmed();
  if (!userTool.isEmpty())
    return userTool;
  if (m_currentPath.isEmpty())
    return QString();
  return m_gitExecutor->run(m_currentPath, {"config", "diff.tool"}).value(0);
}

QString MainWindow::configuredMergeTool() const {
  QSettings settings("GitClientQt", "GitClientQt");
  QString userTool =
      settings.value(QStringLiteral("external/mergeTool")).toString().trimmed();
  if (!userTool.isEmpty())
    return userTool;
  if (m_currentPath.isEmpty())
    return QString();
  return m_gitExecutor->run(m_currentPath, {"config", "merge.tool"}).value(0);
}

void MainWindow::launchGitTool(const QStringList &args, bool reload) {
  if (m_currentPath.isEmpty() || args.isEmpty())
    return;

  const QString command = args.first();
  QString toolName;
  QString envName;
  if (command == QStringLiteral("difftool")) {
    toolName = configuredDiffTool();
    envName = QStringLiteral("GIT_DIFF_TOOL");
  } else if (command == QStringLiteral("mergetool")) {
    toolName = configuredMergeTool();
    envName = QStringLiteral("GIT_MERGE_TOOL");
  }
  if (!envName.isEmpty() && toolName.isEmpty()) {
    statusBar()->showMessage(tr("No %1 tool configured").arg(command));
    return;
  }

  auto *p = new QProcess(this);
  p->setWorkingDirectory(m_currentPath);
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert(QStringLiteral("GIT_EDITOR"), QStringLiteral("true"));
  if (!envName.isEmpty())
    env.insert(envName, toolName);
  p->setProcessEnvironment(env);
  connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          this, [this, p, reload](int, QProcess::ExitStatus) {
            p->deleteLater();
            if (reload)
              loadRepository(m_currentPath);
          });
  connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError) {
    statusBar()->showMessage(tr("Failed to launch external tool"));
    p->deleteLater();
  });
  p->start(QStringLiteral("git"),
           QStringList{QStringLiteral("-C"), m_currentPath} + args);
  if (p->state() == QProcess::NotRunning)
    statusBar()->showMessage(tr("Failed to start external tool"));
}

void MainWindow::onBranchClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column)
  if (!item)
    return;

  QString branch;
  if (m_localBranchesItem && item->parent() == m_localBranchesItem) {
    branch = item->data(0, Qt::UserRole).toString();
  } else if (m_remoteBranchesItem && item->parent() &&
             item->parent()->parent() == m_remoteBranchesItem) {
    branch = item->parent()->text(0) + QLatin1Char('/') + item->text(0);
  } else {
    return;
  }

  const QString sha =
      m_gitExecutor->run(m_currentPath, {"rev-parse", branch}).value(0);
  if (sha.isEmpty())
    return;

  for (int row = 0; row < m_commitTable->rowCount(); ++row) {
    QTableWidgetItem *shaItem = m_commitTable->item(row, 6);
    if (shaItem && shaItem->data(Qt::UserRole).toString() == sha) {
      m_commitTable->selectRow(row);
      QTableWidgetItem *msgItem = m_commitTable->item(row, 3);
      if (msgItem) {
        m_commitTable->setCurrentItem(msgItem);
        m_commitTable->scrollToItem(msgItem, QAbstractItemView::EnsureVisible);
      }
      break;
    }
  }
}

void MainWindow::onTagClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column)
  if (!item || !m_tagsItem || item->parent() != m_tagsItem) {
    return;
  }

  const QString tagName = item->data(0, Qt::UserRole).toString();
  const QString sha =
      m_gitExecutor->run(m_currentPath, {"log", "-1", tagName, "--format=%H"})
          .value(0);
  if (sha.isEmpty()) {
    return;
  }

  for (int row = 0; row < m_commitTable->rowCount(); ++row) {
    QTableWidgetItem *shaItem = m_commitTable->item(row, 6);
    if (shaItem && shaItem->data(Qt::UserRole).toString() == sha) {
      m_commitTable->selectRow(row);
      QTableWidgetItem *msgItem = m_commitTable->item(row, 3);
      if (msgItem) {
        m_commitTable->setCurrentItem(msgItem);
        m_commitTable->scrollToItem(msgItem, QAbstractItemView::EnsureVisible);
      }
      break;
    }
  }
}
void MainWindow::onGrepRequested() {
  if (!m_grepEdit || !m_grepResults || !m_grepDock)
    return;

  m_grepDock->setVisible(true);
  m_grepDock->raise();
  m_grepEdit->setFocus();

  const QString pattern = m_grepEdit->text();
  if (pattern.isEmpty() || m_currentPath.isEmpty())
    return;

  m_grepResults->clear();
  for (const QString &line :
       m_gitExecutor->run(m_currentPath, {"grep", "-n", "-I", pattern})) {
    const int firstColon = line.indexOf(':');
    if (firstColon < 0)
      continue;
    const int secondColon = line.indexOf(':', firstColon + 1);
    if (secondColon < 0)
      continue;
    const QString filePath = m_currentPath + '/' + line.left(firstColon);
    const QString lineNo =
        line.mid(firstColon + 1, secondColon - firstColon - 1);
    const QString match = line.mid(secondColon + 1);
    auto *item = new QTreeWidgetItem(
        m_grepResults, QStringList{line.left(firstColon), lineNo, match});
    item->setData(0, Qt::UserRole, filePath);
  }
}

void MainWindow::onGrepResultActivated(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column)
  if (!item)
    return;
  const QString filePath = item->data(0, Qt::UserRole).toString();
  if (!filePath.isEmpty())
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}

void MainWindow::loadWorkingTree() {
  if (m_unstagedTree)
    m_unstagedTree->clear();
  if (m_stagedTree)
    m_stagedTree->clear();
  if (m_untrackedTree)
    m_untrackedTree->clear();
  if (m_currentPath.isEmpty()) {
    return;
  }

  m_workingTreeModel->load(
      m_gitExecutor->run(m_currentPath, {"diff", "--cached", "--name-status"}),
      m_gitExecutor->run(m_currentPath, {"diff", "--name-status"}),
      m_gitExecutor->run(m_currentPath,
                         {"ls-files", "--others", "--exclude-standard"}));

  auto parseNumstat = [](const QStringList &lines,
                         QHash<QString, int> &addedMap,
                         QHash<QString, int> &removedMap) {
    for (const QString &line : lines) {
      const QStringList parts = line.split('\t');
      if (parts.size() < 3)
        continue;
      bool okAdd = false, okRem = false;
      int added = 0, removed = 0;
      if (parts[0] == QStringLiteral("-")) {
        added = 0;
        okAdd = true;
      } else {
        added = parts[0].toInt(&okAdd);
      }
      if (parts[1] == QStringLiteral("-")) {
        removed = 0;
        okRem = true;
      } else {
        removed = parts[1].toInt(&okRem);
      }
      if (!okAdd || !okRem)
        continue;
      addedMap[parts[2]] = added;
      removedMap[parts[2]] = removed;
    }
  };

  QHash<QString, int> stagedAdded;
  QHash<QString, int> stagedRemoved;
  QHash<QString, int> unstagedAdded;
  QHash<QString, int> unstagedRemoved;

  parseNumstat(
      m_gitExecutor->run(m_currentPath, {"diff", "--cached", "--numstat"}),
      stagedAdded, stagedRemoved);
  parseNumstat(m_gitExecutor->run(m_currentPath, {"diff", "--numstat"}),
               unstagedAdded, unstagedRemoved);

  for (const FileStatus &fs : m_workingTreeModel->stagedFiles())
    m_stagedTree->addFile(fs.first, fs.second, stagedAdded.value(fs.first, -1),
                          stagedRemoved.value(fs.first, -1));

  for (const FileStatus &fs : m_workingTreeModel->unstagedFiles()) {
    int added = unstagedAdded.value(fs.first, -1);
    int removed = unstagedRemoved.value(fs.first, -1);
    m_unstagedTree->addFile(fs.first, fs.second, added, removed);
  }

  for (const FileStatus &fs : m_workingTreeModel->untrackedFiles()) {
    int added = 0;
    const QString fullPath = m_currentPath + QLatin1Char('/') + fs.first;
    QFile f(fullPath);
    if (f.open(QIODevice::ReadOnly)) {
      char buffer[4096];
      qint64 n;
      while ((n = f.read(buffer, sizeof(buffer))) > 0) {
        for (qint64 i = 0; i < n; ++i) {
          if (buffer[i] == '\n')
            ++added;
        }
      }
    }
    m_untrackedTree->addFile(fs.first, fs.second, added, 0);
  }

  if (m_stagedTree)
    m_stagedTree->collapseAll();
  if (m_unstagedTree)
    m_unstagedTree->collapseAll();
  if (m_untrackedTree)
    m_untrackedTree->collapseAll();
  updateCommitButton();
  restoreSelectedFiles();
}

void MainWindow::restoreSelectedFiles() {
  if (m_currentPath.isEmpty())
    return;

  if (m_unstagedTree) {
    const QString path = m_repoUnstagedFile.value(m_currentPath);
    if (!path.isEmpty()) {
      if (QTreeWidgetItem *item = m_unstagedTree->itemForPath(path)) {
        m_unstagedTree->setCurrentItem(item);
        onFileClicked(item, 0);
      }
    }
  }

  if (m_stagedTree) {
    const QString path = m_repoStagedFile.value(m_currentPath);
    if (!path.isEmpty()) {
      if (QTreeWidgetItem *item = m_stagedTree->itemForPath(path)) {
        m_stagedTree->setCurrentItem(item);
        onFileClicked(item, 0);
      }
    }
  }

  if (m_commitFilesTree) {
    const QString path = m_repoCommitFile.value(m_currentPath);
    if (!path.isEmpty()) {
      if (QTreeWidgetItem *item = m_commitFilesTree->itemForPath(path)) {
        m_commitFilesTree->setCurrentItem(item);
        onCommitFileClicked(item, 0);
      }
    }
  }
}

void MainWindow::updateCommitButton() {
  if (!m_commitButton)
    return;
  const bool hasMessage =
      m_commitSubject && !m_commitSubject->text().trimmed().isEmpty();
  const bool hasStaged = m_stagedTree && m_stagedTree->topLevelItemCount() > 0;
  const bool amend = m_amendCheckBox && m_amendCheckBox->isChecked();
  m_commitButton->setEnabled(hasMessage && (hasStaged || amend));
}

void MainWindow::loadCommitMessageIntoEditor(const QString &sha) {
  if (!m_commitSubject || !m_commitBody || m_currentPath.isEmpty())
    return;
  const QStringList args = sha.isEmpty()
                               ? QStringList{"log", "-1", "--format=%B"}
                               : QStringList{"log", "-1", sha, "--format=%B"};
  const QStringList lines = m_gitExecutor->run(m_currentPath, args);
  if (lines.isEmpty())
    return;
  m_commitSubject->setText(lines.first());
  m_commitBody->setText(lines.mid(1).join('\n').trimmed());
}

void MainWindow::onAmendToggled(int state) {
  if (state == Qt::Checked && m_currentPath.isEmpty())
    return;

  if (state == Qt::Checked) {
    m_commitSubjectDraft =
        m_commitSubject ? m_commitSubject->text() : QString();
    m_commitBodyDraft = m_commitBody ? m_commitBody->toPlainText() : QString();
    loadCommitMessageIntoEditor(m_selectedCommitSha);
  } else {
    if (m_commitSubject)
      m_commitSubject->setText(m_commitSubjectDraft);
    if (m_commitBody)
      m_commitBody->setPlainText(m_commitBodyDraft);
    m_commitSubjectDraft.clear();
    m_commitBodyDraft.clear();
  }
  updateAmendWarning();
  updateCommitButton();
}

void MainWindow::updateFilter() {
  if (!m_filterEdit || !m_commitTable)
    return;
  const QString text = m_filterEdit->text().trimmed();
  if (text.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive)) {
    QTimer::singleShot(200, this, [this, text] { updateFileFilter(text); });
    return;
  }
  const bool empty = text.isEmpty();
  for (int row = 0; row < m_commitTable->rowCount(); ++row) {
    bool match = empty;
    if (!empty) {
      for (int col = 0; col < m_commitTable->columnCount(); ++col) {
        QTableWidgetItem *item = m_commitTable->item(row, col);
        if (item && item->text().contains(text, Qt::CaseInsensitive)) {
          match = true;
          break;
        }
      }
    }
    m_commitTable->setRowHidden(row, !match);
  }
}

void MainWindow::updateFileFilter(const QString &text) {
  if (!m_filterEdit || !m_commitTable)
    return;
  if (m_filterEdit->text().trimmed() != text)
    return;

  const QString pattern = text.mid(5).trimmed();
  const QStringList shas =
      m_currentPath.isEmpty()
          ? QStringList()
          : m_gitExecutor->run(m_currentPath,
                               {QStringLiteral("log"), QStringLiteral("--all"),
                                QStringLiteral("--format=%H"),
                                QStringLiteral("--"), pattern});
  const QSet<QString> shaSet = QSet<QString>(shas.cbegin(), shas.cend());
  for (int row = 0; row < m_commitTable->rowCount(); ++row) {
    QTableWidgetItem *item = m_commitTable->item(row, 6);
    const QString sha = item ? item->data(Qt::UserRole).toString() : QString();
    m_commitTable->setRowHidden(row, !shaSet.contains(sha));
  }
}

void MainWindow::onCommitSelected(QTableWidgetItem *item) {
  if (!item || !m_commitFilesTree)
    return;

  if (m_diffView)
    showEmptyDiff();

  m_commitFilesTree->clear();
  m_selectedCommitSha.clear();

  const int row = item->row();
  QTableWidgetItem *shaItem = m_commitTable->item(row, 6);
  const QString sha =
      shaItem ? shaItem->data(Qt::UserRole).toString() : QString();

  if (m_amendCheckBox) {
    m_amendCheckBox->setEnabled(sha.isEmpty());
    if (!sha.isEmpty())
      m_amendCheckBox->setChecked(false);
  }
  if (m_signCommitCheckBox) {
    m_signCommitCheckBox->setEnabled(sha.isEmpty());
    if (!sha.isEmpty())
      m_signCommitCheckBox->setChecked(false);
  }

  if (sha.isEmpty()) {
    m_selectedCommitSha.clear();
    if (!m_currentPath.isEmpty())
      m_repoSelectedShas[m_currentPath] = m_selectedCommitSha;
    showEmptyCommitFiles();
    if (m_commitSubject)
      m_commitSubject->clear();
    if (m_commitBody)
      m_commitBody->clear();
    return;
  }

  m_selectedCommitSha = sha;
  if (!m_currentPath.isEmpty())
    m_repoSelectedShas[m_currentPath] = m_selectedCommitSha;
  loadCommitMessageIntoEditor(m_selectedCommitSha);

  QHash<QString, int> addedMap;
  QHash<QString, int> removedMap;
  for (const QString &line : m_gitExecutor->run(
           m_currentPath, {"diff-tree", "--no-commit-id", "--numstat", "--root",
                           "-m", "-r", m_selectedCommitSha})) {
    const QStringList parts = line.split('\t');
    if (parts.size() < 3)
      continue;
    bool okAdd = false, okRem = false;
    int added = 0, removed = 0;
    if (parts[0] == QStringLiteral("-")) {
      added = 0;
      okAdd = true;
    } else {
      added = parts[0].toInt(&okAdd);
    }
    if (parts[1] == QStringLiteral("-")) {
      removed = 0;
      okRem = true;
    } else {
      removed = parts[1].toInt(&okRem);
    }
    if (!okAdd || !okRem)
      continue;
    if (!addedMap.contains(parts[2])) {
      addedMap[parts[2]] = added;
      removedMap[parts[2]] = removed;
    }
  }

  QSet<QString> shownFiles;
  for (const QString &line : m_gitExecutor->run(
           m_currentPath, {"diff-tree", "--no-commit-id", "--name-status",
                           "--root", "-m", "-r", m_selectedCommitSha})) {
    const QStringList parts = line.split('\t');
    if (parts.size() < 2)
      continue;
    const QString status = parts.first();
    const QString filePath = parts.last();
    if (status.isEmpty() || filePath.isEmpty() || shownFiles.contains(filePath))
      continue;
    shownFiles.insert(filePath);
    m_commitFilesTree->addFile(filePath, status, addedMap.value(filePath, 0),
                               removedMap.value(filePath, 0));
  }

  if (m_commitFilesTree->topLevelItemCount() > 0) {
    m_commitFilesTree->collapseAll();
  } else {
    showErrorCommitFiles(tr("This commit has no file changes."));
  }
}

void MainWindow::loadRemotes() {
  if (!m_remotesItem || m_currentPath.isEmpty())
    return;
  for (QTreeWidgetItem *child : m_remotesItem->takeChildren())
    delete child;

  for (const auto &remote : m_gitRepository->remotes()) {
    const QString name = remote.first;
    const QString url = remote.second;
    QTreeWidgetItem *child = new QTreeWidgetItem(m_remotesItem, {name});
    child->setToolTip(0, url);
    child->setData(0, Qt::UserRole, name);
    child->setData(0, Qt::UserRole + 1, url);
    child->setText(0, QString("%1   %2").arg(name, url));
  }
}

void MainWindow::loadWorktrees() {
  if (!m_worktreesItem || m_currentPath.isEmpty())
    return;
  while (m_worktreesItem->childCount() > 0)
    delete m_worktreesItem->takeChild(0);

  for (const QString &path : m_gitRepository->worktrees()) {
    auto *item = new QTreeWidgetItem(m_worktreesItem, {path});
    item->setData(0, Qt::UserRole, path);
    item->setToolTip(0, path);
  }
}

void MainWindow::onWorktreeClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column)
  if (!item || !m_worktreesItem || item->parent() != m_worktreesItem)
    return;

  const QString path = item->data(0, Qt::UserRole).toString();
  if (path.isEmpty())
    return;

  if (QFileInfo(path).canonicalFilePath() ==
      QFileInfo(m_currentPath).canonicalFilePath()) {
    return;
  }

  loadRepository(path);
}

void MainWindow::loadStashes() {
  if (!m_stashesItem || m_currentPath.isEmpty())
    return;

  while (m_stashesItem->childCount() > 0)
    delete m_stashesItem->takeChild(0);

  for (const auto &stash : m_gitRepository->stashes()) {
    const QString ref = stash.first;
    const QString msg = stash.second;
    if (ref.isEmpty())
      continue;
    auto *stashItem = new QTreeWidgetItem(m_stashesItem, QStringList{msg});
    stashItem->setData(0, Qt::UserRole, ref);
  }
}

void MainWindow::onStashClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column)
  if (!item || !m_stashesItem || item->parent() != m_stashesItem)
    return;

  const QString ref = item->data(0, Qt::UserRole).toString();
  if (ref.isEmpty())
    return;

  m_selectedCommitSha = ref;

  if (m_commitFilesTree) {
    m_commitFilesTree->clear();
    for (const QString &line :
         m_gitExecutor->run(m_currentPath, {"diff", "--name-status",
                                            ref + QLatin1Char('^'), ref})) {
      const QStringList parts = line.split('\t');
      if (parts.size() < 2)
        continue;
      m_commitFilesTree->addFile(parts.last(), parts.first().left(1));
    }
    if (m_commitFilesTree->topLevelItemCount() > 0) {
      m_commitFilesTree->collapseAll();
    } else {
      showErrorCommitFiles(tr("This stash has no file changes."));
    }
  }

  const QStringList diff = m_gitExecutor->run(
      m_currentPath, {"show", "--pretty=format:", "--no-notes", ref});
  if (m_diffView) {
    if (diff.isEmpty()) {
      if (m_diffDock)
        m_diffDock->setVisible(false);
      m_diffView->showEmpty(tr("No diff"),
                            tr("No changes to show for this selection."));
    } else {
      if (m_viewTabWidget)
        m_viewTabWidget->setCurrentWidget(m_diffContainer);
    }
    m_diffView->setHtml(m_diffPresenter->formatDiff(diff));
  }
}

void MainWindow::activateRepositoryTab(const QString &path) {
  if (!m_repoTabBar || path.isEmpty())
    return;

  const bool oldBlock = m_repoTabBar->blockSignals(true);
  int index = -1;
  for (int i = 0; i < m_repoTabBar->count(); ++i) {
    if (m_repoTabBar->tabData(i).toString() == path) {
      index = i;
      break;
    }
  }

  if (index == -1) {
    index = m_repoTabBar->addTab(QFileInfo(path).fileName());
    m_repoTabBar->setTabData(index, path);
    m_repoTabBar->setTabToolTip(index, path);
  }

  m_repoTabBar->setCurrentIndex(index);
  m_repoTabBar->setVisible(true);
  m_repoTabBar->blockSignals(oldBlock);
}

void MainWindow::onRepositoryTabChanged(int index) {
  if (index < 0) {
    m_currentPath.clear();
    if (m_centralStack)
      m_centralStack->setCurrentIndex(0);
    if (m_repoTabBar)
      m_repoTabBar->setVisible(false);
    setWindowTitle(tr("Git Client Qt"));
    if (m_pushButton)
      m_pushButton->setEnabled(false);
    if (m_undoButton)
      m_undoButton->setEnabled(false);
    if (m_pullButton)
      m_pullButton->setEnabled(false);
    return;
  }

  const QString path = m_repoTabBar->tabData(index).toString();
  if (path == m_currentPath)
    return;
  if (!m_currentPath.isEmpty())
    m_repoSelectedShas[m_currentPath] = m_selectedCommitSha;
  if (!m_currentPath.isEmpty() && m_commitTable &&
      m_commitTable->horizontalScrollBar())
    m_repoHorizontalScroll[m_currentPath] =
        m_commitTable->horizontalScrollBar()->value();
  loadRepository(path, false);
}

void MainWindow::onRepositoryTabCloseRequested(int index) {
  if (m_repoTabBar)
    m_repoTabBar->removeTab(index);
}

void MainWindow::saveOpenTabs() {
  QSettings settings("GitClientQt", "GitClientQt");
  QStringList openPaths;
  for (int i = 0; i < m_repoTabBar->count(); ++i)
    openPaths << m_repoTabBar->tabData(i).toString();
  settings.setValue(QLatin1String("openRepos"), openPaths);
  settings.setValue(QLatin1String("activeRepo"), m_currentPath);
}

void MainWindow::restoreOpenTabs() {
  QSettings settings("GitClientQt", "GitClientQt");
  const QStringList openRepos =
      settings.value(QLatin1String("openRepos")).toStringList();
  if (openRepos.isEmpty())
    return;

  const QString activeRepo =
      settings.value(QLatin1String("activeRepo")).toString();
  if (m_currentPath.isEmpty())
    m_currentPath = activeRepo.isEmpty() ? openRepos.first() : activeRepo;

  if (m_repoTabBar) {
    const bool oldBlock = m_repoTabBar->blockSignals(true);
    for (const QString &path : openRepos) {
      const int index = m_repoTabBar->addTab(QFileInfo(path).fileName());
      m_repoTabBar->setTabData(index, path);
      m_repoTabBar->setTabToolTip(index, path);
    }
    m_repoTabBar->setVisible(m_repoTabBar->count() > 0);
    m_repoTabBar->blockSignals(oldBlock);
  }
}
