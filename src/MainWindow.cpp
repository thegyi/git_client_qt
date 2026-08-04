#include "MainWindow.h"
#include "GitExecutor.h"
#include "ui_MainWindow.h"
#include "widgets/CommitTableWidget.h"
#include "widgets/DiffViewWidget.h"
#include "widgets/FileTreeWidget.h"
#include "widgets/RepoPanelWidget.h"

#include <QDebug>

#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QByteArray>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHeaderView>
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
#include <QSettings>
#include <QShortcut>
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
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QStatusBar>
#include <QTabWidget>
#include <QTemporaryFile>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  m_gitExecutor = new GitExecutor(this);
  m_gitRepository = new GitRepository(m_gitExecutor, this);
  m_diffPresenter = new DiffPresenter(this);
  m_commitModel = new CommitModel(this);
  m_workingTreeModel = new WorkingTreeModel(this);
  ui->setupUi(this);
  ui->menuFile->setTitle(tr("&File"));
  ui->menuEdit->setTitle(tr("&Edit"));
  ui->menuHelp->setTitle(tr("&Help"));
  setStyleSheet("QMainWindow::separator { background: #808080; width: 4px; }"
                "QMenu::item { padding: 6px 24px 6px 12px; }"
                "QMenu::item:selected { background-color: palette(highlight); "
                "color: palette(highlighted-text); }");
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
  ui->menuFile->insertMenu(ui->actionClose, m_recentMenu);

  auto *searchMenu = new QMenu(tr("&Search"), this);
  menuBar()->addMenu(searchMenu);
  auto *grepAction = searchMenu->addAction(tr("Grep"));
  grepAction->setStatusTip(tr("Search for a pattern in the repository"));
  grepAction->setShortcut(QKeySequence(QLatin1String("Ctrl+Shift+G")));
  connect(grepAction, &QAction::triggered, this, &MainWindow::onGrepRequested);

  auto *submodulesMenu = new QMenu(tr("&Submodules"), this);
  menuBar()->addMenu(submodulesMenu);
  auto *initSubmodulesAction = submodulesMenu->addAction(tr("Init"));
  initSubmodulesAction->setStatusTip(
      tr("Initialize the configured submodules"));
  auto *updateSubmodulesAction = submodulesMenu->addAction(tr("Update"));
  updateSubmodulesAction->setStatusTip(tr("Update registered submodules"));
  auto *syncSubmodulesAction = submodulesMenu->addAction(tr("Sync"));
  syncSubmodulesAction->setStatusTip(tr("Sync submodule remotes"));
  auto *addSubmoduleAction = submodulesMenu->addAction(tr("Add..."));
  addSubmoduleAction->setStatusTip(tr("Add a new submodule to the repository"));
  auto *openSubmoduleAction = submodulesMenu->addAction(tr("Open..."));
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
  }

  auto *repoSettingsAction = repositoryMenu->addAction(tr("Settings..."));
  repoSettingsAction->setStatusTip(tr("Configure repository settings"));
  repoSettingsAction->setShortcut(QKeySequence(QLatin1String("Ctrl+,")));
  connect(repoSettingsAction, &QAction::triggered, this,
          &MainWindow::showRepositorySettings);

  auto *hooksAndTemplatesAction =
      repositoryMenu->addAction(tr("Commit hooks and templates..."));
  hooksAndTemplatesAction->setStatusTip(
      tr("Edit commit message template and repository hooks"));
  hooksAndTemplatesAction->setShortcut(
      QKeySequence(QLatin1String("Ctrl+Shift+H")));
  connect(hooksAndTemplatesAction, &QAction::triggered, this,
          &MainWindow::showCommitHooksAndTemplates);

  auto *applyPatchAction = repositoryMenu->addAction(tr("Apply patch..."));
  applyPatchAction->setStatusTip(tr("Apply a patch or diff file"));
  applyPatchAction->setShortcut(QKeySequence(QLatin1String("Ctrl+Shift+P")));
  connect(applyPatchAction, &QAction::triggered, this, &MainWindow::applyPatch);

  auto *reflogAction = repositoryMenu->addAction(tr("Reflog"));
  reflogAction->setStatusTip(tr("View the reference log"));
  reflogAction->setShortcut(QKeySequence(QLatin1String("Ctrl+Shift+R")));
  connect(reflogAction, &QAction::triggered, this, &MainWindow::showReflog);

  auto *resolveConflictsAction =
      repositoryMenu->addAction(tr("Resolve conflicts..."));
  resolveConflictsAction->setStatusTip(tr("Resolve merge conflicts"));
  resolveConflictsAction->setShortcut(
      QKeySequence(QLatin1String("Ctrl+Shift+C")));
  connect(resolveConflictsAction, &QAction::triggered, this,
          [this]() { showConflictResolver(QString()); });

  auto *bisectMenu = repositoryMenu->addMenu(tr("&Bisect"));
  auto *bisectStartAction = bisectMenu->addAction(tr("Start..."));
  auto *bisectGoodAction = bisectMenu->addAction(tr("Good"));
  auto *bisectBadAction = bisectMenu->addAction(tr("Bad"));
  auto *bisectSkipAction = bisectMenu->addAction(tr("Skip"));
  auto *bisectResetAction = bisectMenu->addAction(tr("Reset"));
  connect(bisectStartAction, &QAction::triggered, this,
          &MainWindow::startBisect);
  connect(bisectGoodAction, &QAction::triggered, this, &MainWindow::bisectGood);
  connect(bisectBadAction, &QAction::triggered, this, &MainWindow::bisectBad);
  connect(bisectSkipAction, &QAction::triggered, this, &MainWindow::bisectSkip);
  connect(bisectResetAction, &QAction::triggered, this,
          &MainWindow::bisectReset);

  auto *lfsMenu = repositoryMenu->addMenu(tr("&LFS"));
  auto *lfsTrackAction = lfsMenu->addAction(tr("Track pattern..."));
  auto *lfsUntrackAction = lfsMenu->addAction(tr("Untrack pattern..."));
  auto *lfsPullAction = lfsMenu->addAction(tr("Pull objects"));
  auto *lfsPushAction = lfsMenu->addAction(tr("Push objects"));
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
  ui->menuFile->insertAction(ui->actionOpen, actionClone);
  actionClone->setStatusTip(tr("Clone a remote repository"));
  actionClone->setShortcut(QKeySequence(QLatin1String("Ctrl+Shift+N")));
  connect(actionClone, &QAction::triggered, this,
          &MainWindow::onCloneRepository);

  auto *actionInit = new QAction(tr("Initialize Repository"), this);
  ui->menuFile->insertAction(ui->actionOpen, actionInit);
  actionInit->setStatusTip(tr("Create a new Git repository"));
  actionInit->setShortcut(QKeySequence(QLatin1String("Ctrl+N")));
  connect(actionInit, &QAction::triggered, this, &MainWindow::onInitRepository);

  auto *editGitignoreAction = new QAction(tr("Edit .gitignore"), this);
  editGitignoreAction->setStatusTip(tr("Edit the repository .gitignore file"));
  editGitignoreAction->setShortcut(QKeySequence(QLatin1String("Ctrl+Shift+I")));
  connect(editGitignoreAction, &QAction::triggered, this,
          &MainWindow::editGitignore);

  auto *editGitattributesAction = new QAction(tr("Edit .gitattributes"), this);
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
  connect(m_undoButton, &QPushButton::clicked, this, [this] {
    if (m_currentPath.isEmpty())
      return;
    if (QMessageBox::question(this, tr("Undo last commit"),
                              tr("Undo the last commit and keep changes "
                                 "staged?")) == QMessageBox::Yes) {
      if (m_gitExecutor->exec(m_currentPath, {"reset", "--soft", "HEAD~1"})) {
        loadRepository(m_currentPath);
        statusBar()->showMessage(tr("Undone last commit"));
      } else {
        statusBar()->showMessage(tr("Failed to undo last commit"));
      }
    }
  });

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
  fetchFromAction->setStatusTip(tr("Fetch from a selected remote"));
  connect(fetchFromAction, &QAction::triggered, this, [this] {
    if (m_currentPath.isEmpty())
      return;
    const QStringList remotes = m_gitExecutor->run(m_currentPath, {"remote"});
    if (remotes.isEmpty()) {
      QMessageBox::warning(this, tr("No remotes"),
                           tr("There are no remotes to fetch from."));
      return;
    }
    bool ok;
    const QString remote = QInputDialog::getItem(
        this, tr("Fetch from Remote"), tr("Remote:"), remotes, 0, false, &ok);
    if (ok && !remote.isEmpty()) {
      if (m_gitExecutor->exec(m_currentPath, {"fetch", remote})) {
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
    if (m_currentPath.isEmpty())
      return;

    const QString currentBranch =
        m_gitExecutor->run(m_currentPath, {"rev-parse", "--abbrev-ref", "HEAD"})
            .value(0);
    if (currentBranch.isEmpty()) {
      QMessageBox::warning(this, tr("Push failed"),
                           tr("Could not determine the current branch."));
      return;
    }

    const QString remote =
        m_gitExecutor
            ->run(m_currentPath,
                  {"config",
                   QStringLiteral("branch.%1.remote").arg(currentBranch)})
            .value(0);
    const QString pushRemote =
        remote.isEmpty() ? QStringLiteral("origin") : remote;

    QStringList args = {QStringLiteral("push")};
    for (const QString &arg : m_pushArgs) {
      if (arg != QStringLiteral("push"))
        args << arg;
    }
    args << QStringLiteral("-u") << pushRemote << currentBranch;

    QProgressDialog progress(tr("Pushing to remote..."), tr("Cancel"), 0, 0,
                             this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    QProcess p;
    p.setWorkingDirectory(m_currentPath);

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

    p.start(QStringLiteral("git"), args);
    if (!p.waitForStarted(5000)) {
      QMessageBox::warning(this, tr("Push failed"),
                           tr("Could not start git process"));
      return;
    }

    progress.exec();

    if (canceled) {
      statusBar()->showMessage(tr("Push canceled"));
      return;
    }

    if (p.exitCode() == 0) {
      if (!remote.isEmpty())
        m_gitExecutor->exec(m_currentPath, {QStringLiteral("fetch"), remote});
      loadRepository(m_currentPath);
      statusBar()->showMessage(tr("Push finished"));
      if (!output.isEmpty()) {
        QMessageBox msg(this);
        msg.setWindowTitle(tr("Push output"));
        msg.setText(tr("Push completed successfully."));
        msg.setDetailedText(output);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.exec();
      }
    } else {
      QMessageBox msg(this);
      msg.setWindowTitle(tr("Push failed"));
      msg.setText(tr("Git push failed."));
      msg.setDetailedText(output);
      msg.setIcon(QMessageBox::Warning);
      msg.setStandardButtons(QMessageBox::Ok);
      msg.exec();
    }
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
        QMessageBox::warning(this, tr("Pull failed"), tr("No current branch"));
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
          QMessageBox::warning(this, tr("No remote"),
                               tr("There is no remote to pull from."));
          return;
        }
        pullArgs << remotes.first() << currentBranch;
      } else {
        pullArgs << remote;
      }
    }

    p.start(QStringLiteral("git"), pullArgs);
    if (!p.waitForStarted(5000)) {
      QMessageBox::warning(this, tr("Pull failed"),
                           tr("Could not start git process"));
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
      QMessageBox::warning(this, tr("Pull failed"), output);
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

  m_centralStack->addWidget(welcomeWidget);
  m_centralStack->addWidget(m_commitTable);
  m_centralStack->setCurrentIndex(0);
  setCentralWidget(m_centralStack);

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
  dock->setFeatures(QDockWidget::DockWidgetMovable);
  dock->setMaximumWidth(400);
  addDockWidget(Qt::LeftDockWidgetArea, dock);
  m_repoDock = dock;

  auto *rightDock = new QDockWidget(tr("Working Tree"), this);
  rightDock->setObjectName(QStringLiteral("workTreeDock"));
  rightDock->setFeatures(QDockWidget::DockWidgetMovable);
  auto *rightWidget = new QWidget(this);
  rightWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto *rightLayout = new QVBoxLayout(rightWidget);
  rightLayout->setContentsMargins(4, 4, 4, 4);
  rightLayout->setSpacing(4);

  auto *unstagedGroup = new QGroupBox(tr("Unstaged Files"), this);
  auto *unstagedLayout = new QVBoxLayout(unstagedGroup);
  m_unstagedTree = new FileTreeWidget(tr("Unstaged Files"), this);
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
  m_stagedTree = new FileTreeWidget(tr("Staged Files"), this);
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

  auto *messageGroup = new QGroupBox(tr("Commit Message"), this);
  auto *messageLayout = new QVBoxLayout(messageGroup);
  m_commitSubject = new QLineEdit(this);
  m_commitSubject->setPlaceholderText(tr("Short summary"));
  messageLayout->addWidget(m_commitSubject);
  m_commitBody = new QTextEdit(this);
  m_commitBody->setPlaceholderText(tr("Long description"));
  m_commitBody->setAcceptRichText(false);
  m_commitBody->setMaximumHeight(120);
  messageLayout->addWidget(m_commitBody);
  m_amendCheckBox = new QCheckBox(tr("Amend last commit"), this);
  messageLayout->addWidget(m_amendCheckBox);
  m_signCommitCheckBox = new QCheckBox(tr("Sign with GPG"), this);
  messageLayout->addWidget(m_signCommitCheckBox);
  connect(m_signCommitCheckBox, &QCheckBox::toggled, this,
          &MainWindow::toggleGpgConfig);
  m_commitButton = new QPushButton(tr("Commit"), this);
  m_commitButton->setIcon(
      QApplication::style()->standardIcon(QStyle::SP_DialogOkButton));
  m_commitButton->setEnabled(false);
  m_commitButton->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
  messageLayout->addWidget(m_commitButton);
  rightLayout->addWidget(messageGroup);

  auto *commitFilesGroup = new QGroupBox(tr("Commit Files"), this);
  auto *commitFilesLayout = new QVBoxLayout(commitFilesGroup);
  m_commitFilesTree = new FileTreeWidget(QString(), this);
  connect(m_commitFilesTree, &QTreeWidget::customContextMenuRequested, this,
          &MainWindow::showCommitFilesContextMenu);
  connect(m_commitFilesTree, &QTreeWidget::itemClicked, this,
          &MainWindow::onCommitFileClicked);
  commitFilesLayout->addWidget(m_commitFilesTree);
  showEmptyCommitFiles();
  rightLayout->addWidget(commitFilesGroup);

  connect(m_commitButton, &QPushButton::clicked, this,
          &MainWindow::onCommitClicked);
  connect(m_commitSubject, &QLineEdit::textChanged, this,
          &MainWindow::updateCommitButton);
  if (m_amendCheckBox)
    connect(m_amendCheckBox, &QCheckBox::stateChanged, this,
            &MainWindow::onAmendToggled);

  rightDock->setWidget(rightWidget);
  rightDock->setTitleBarWidget(new QWidget(rightDock));
  rightDock->setFeatures(QDockWidget::DockWidgetMovable);
  addDockWidget(Qt::RightDockWidgetArea, rightDock);
  m_workTreeDock = rightDock;

  m_diffDock = new QDockWidget(tr("Diff"), this);
  m_diffDock->setObjectName(QStringLiteral("diffDock"));
  m_diffView = new DiffViewWidget(this);
  m_diffView->setMinimumHeight(120);
  m_diffView->setFont(QFont(QStringLiteral("monospace"), 10));
  m_diffView->setFrameStyle(QFrame::NoFrame);
  m_diffView->document()->setDocumentMargin(0);
  connect(m_diffView, &QTextBrowser::anchorClicked, this,
          &MainWindow::onDiffAnchorClicked);
  showEmptyDiff();

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
  tabifyDockWidget(rightDock, m_grepDock);

  m_commandLogDock = new QDockWidget(tr("Command Log"), this);
  m_commandLogDock->setObjectName(QStringLiteral("commandLogDock"));
  m_commandLogEdit = new QTextEdit(this);
  m_commandLogEdit->setReadOnly(true);
  m_commandLogEdit->setFont(QFont(QStringLiteral("monospace"), 9));
  m_commandLogDock->setWidget(m_commandLogEdit);
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

  m_diffDock->setWidget(m_diffView);
  m_diffDock->setFeatures(QDockWidget::DockWidgetMovable);
  addDockWidget(Qt::BottomDockWidgetArea, m_diffDock);

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
  addVisibilityAction(tr("Diff"), m_diffDock);
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
      QMessageBox::warning(this, tr("Not a Git repository"),
                           tr("The selected folder is not a Git repository."));
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
  delete ui;
}

void MainWindow::restoreSettings() {
  QSettings settings("GitClientQt", "GitClientQt");
  if (settings.value("reopenLastRepo", true).toBool()) {
    const QString lastRepo = settings.value("lastRepo").toString();
    if (!lastRepo.isEmpty() && m_currentPath.isEmpty())
      m_currentPath = lastRepo;
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
  settings.setValue(base + QLatin1String("state"), saveState(0));

  if (m_commitTable) {
    QVariantList widths;
    for (int c = 0; c < m_commitTable->columnCount(); ++c)
      widths << m_commitTable->columnWidth(c);
    qDebug() << "saveDockAndColumnState" << m_currentPath << repoKey
             << "commitTable widths:" << widths;
    settings.setValue(base + QLatin1String("columnWidths/commitTable"), widths);
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

void MainWindow::restoreDockAndColumnState(bool includeGeometry) {
  QSettings settings("GitClientQt", "GitClientQt");

  if (includeGeometry) {
    const QByteArray geometry =
        settings.value("mainWindow/geometry").toByteArray();
    if (!geometry.isEmpty())
      restoreGeometry(geometry);

    bool onScreen = false;
    const QRect windowRect = QRect(pos(), size());
    for (QScreen *screen : QGuiApplication::screens()) {
      if (screen->availableGeometry().intersects(windowRect)) {
        onScreen = true;
        break;
      }
    }
    if (!onScreen) {
      const QRect screenRect =
          QGuiApplication::primaryScreen()->availableGeometry();
      setGeometry(screenRect.adjusted(100, 100, -100, -100));
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
      settings.value(base + QLatin1String("state")).toByteArray();
  if (!state.isEmpty())
    restoreState(state, 0);

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
        settings.value(base + QLatin1String("columnWidths/commitTable"))
            .toList();
    qDebug() << "restoreDockAndColumnState" << m_currentPath << repoKey
             << "commitTable widths:" << widths;
    for (int c = 0; c < widths.size() && c < m_commitTable->columnCount(); ++c)
      m_commitTable->setColumnWidth(c, widths.at(c).toInt());
  }
}

void MainWindow::showPreferences() {
  QSettings settings("GitClientQt", "GitClientQt");
  QDialog dlg(this);
  dlg.setWindowTitle(tr("Preferences"));
  dlg.setMinimumSize(550, 450);
  auto *mainLayout = new QVBoxLayout(&dlg);

  auto *tabs = new QTabWidget(&dlg);
  mainLayout->addWidget(tabs);

  auto *generalTab = new QWidget(&dlg);
  auto *generalLayout = new QFormLayout(generalTab);

  auto *pullModeCombo = new QComboBox(&dlg);
  pullModeCombo->addItem(tr("Pull (fast-forward if possible)"),
                         QStringLiteral("ffIfPossible"));
  pullModeCombo->addItem(tr("Pull (fast-forward only)"),
                         QStringLiteral("ffOnly"));
  pullModeCombo->addItem(tr("Pull (rebase)"), QStringLiteral("rebase"));
  pullModeCombo->addItem(tr("Fetch all"), QStringLiteral("fetchAll"));
  pullModeCombo->setCurrentIndex(pullModeCombo->findData(
      settings.value("pullMode", QStringLiteral("ffIfPossible"))));

  auto *reopenBox =
      new QCheckBox(tr("Reopen last repository on startup"), &dlg);
  reopenBox->setChecked(settings.value("reopenLastRepo", true).toBool());

  auto *gpgKeyEdit = new QLineEdit(&dlg);
  gpgKeyEdit->setText(settings.value("gpgSigningKey").toString());

  generalLayout->addRow(tr("Default pull mode:"), pullModeCombo);
  generalLayout->addWidget(reopenBox);
  generalLayout->addRow(tr("GPG key ID or email:"), gpgKeyEdit);
  tabs->addTab(generalTab, tr("General"));

  auto *themeTab = new QWidget(&dlg);
  auto *themeLayout = new QVBoxLayout(themeTab);

  auto *themeCombo = new QComboBox(&dlg);
  themeCombo->addItem(tr("Dark"), QStringLiteral("dark"));
  themeCombo->addItem(tr("Light"), QStringLiteral("light"));
  themeCombo->addItem(tr("Custom"), QStringLiteral("custom"));
  themeCombo->setCurrentIndex(themeCombo->findData(
      settings.value("theme/mode", QStringLiteral("dark"))));
  themeLayout->addWidget(themeCombo);

  const QVector<QPair<QString, QPalette::ColorRole>> roleList = {
      {QStringLiteral("Window"), QPalette::Window},
      {QStringLiteral("WindowText"), QPalette::WindowText},
      {QStringLiteral("Base"), QPalette::Base},
      {QStringLiteral("AlternateBase"), QPalette::AlternateBase},
      {QStringLiteral("Text"), QPalette::Text},
      {QStringLiteral("Button"), QPalette::Button},
      {QStringLiteral("ButtonText"), QPalette::ButtonText},
      {QStringLiteral("Highlight"), QPalette::Highlight},
      {QStringLiteral("HighlightedText"), QPalette::HighlightedText},
      {QStringLiteral("Link"), QPalette::Link},
      {QStringLiteral("ToolTipBase"), QPalette::ToolTipBase},
      {QStringLiteral("ToolTipText"), QPalette::ToolTipText}};

  auto *colorGrid = new QGridLayout();
  QVector<QPushButton *> colorButtons;
  for (int i = 0; i < roleList.size(); ++i) {
    auto *label = new QLabel(roleList[i].first + QLatin1Char(':'), &dlg);
    auto *btn = new QPushButton(&dlg);
    btn->setFlat(true);
    colorButtons.append(btn);
    colorGrid->addWidget(label, i, 0);
    colorGrid->addWidget(btn, i, 1);
  }
  themeLayout->addLayout(colorGrid);

  auto setButtonColor = [&](QPushButton *btn, const QColor &c) {
    btn->setProperty("color", c);
    btn->setText(c.name());
    btn->setStyleSheet(QStringLiteral("background-color: %1;").arg(c.name()));
  };

  auto loadColors = [&](const QPalette &pal) {
    for (int i = 0; i < roleList.size(); ++i) {
      const QColor c = pal.color(QPalette::Active, roleList[i].second);
      setButtonColor(colorButtons[i], c);
    }
  };

  loadColors(qApp->palette());

  connect(themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
          [&, roleList, colorButtons](int) {
            const QString mode = themeCombo->currentData().toString();
            if (mode == QLatin1String("custom"))
              return;

            QPalette pal;
            if (mode == QLatin1String("dark")) {
              pal.setColor(QPalette::Window, QColor(0x1e, 0x1e, 0x1e));
              pal.setColor(QPalette::WindowText, QColor(0xd4, 0xd4, 0xd4));
              pal.setColor(QPalette::Base, QColor(0x25, 0x25, 0x26));
              pal.setColor(QPalette::AlternateBase, QColor(0x2d, 0x2d, 0x30));
              pal.setColor(QPalette::Text, QColor(0xd4, 0xd4, 0xd4));
              pal.setColor(QPalette::Button, QColor(0x3c, 0x3c, 0x3c));
              pal.setColor(QPalette::ButtonText, QColor(0xd4, 0xd4, 0xd4));
              pal.setColor(QPalette::Highlight, QColor(0x09, 0x47, 0x71));
              pal.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
              pal.setColor(QPalette::Link, QColor(0x37, 0x94, 0xff));
              pal.setColor(QPalette::ToolTipBase, QColor(0x1e, 0x1e, 0x1e));
              pal.setColor(QPalette::ToolTipText, QColor(0xd4, 0xd4, 0xd4));
            } else { // light
              pal = QPalette();
            }
            loadColors(pal);
          });

  for (int i = 0; i < colorButtons.size(); ++i) {
    QPushButton *btn = colorButtons[i];
    connect(btn, &QPushButton::clicked, &dlg, [&, i, btn]() {
      const QColor current = btn->property("color").value<QColor>();
      const QColor c = QColorDialog::getColor(
          current, &dlg, tr("Select color for %1").arg(roleList[i].first));
      if (c.isValid()) {
        setButtonColor(btn, c);
        themeCombo->setCurrentIndex(
            themeCombo->findData(QStringLiteral("custom")));
      }
    });
  }

  tabs->addTab(themeTab, tr("Theme"));

  auto *shortcutsTab = new QWidget(&dlg);
  auto *shortcutsLayout = new QVBoxLayout(shortcutsTab);
  auto *table = new QTableWidget(&dlg);
  table->setColumnCount(3);
  table->setHorizontalHeaderLabels({tr("Menu"), tr("Action"), tr("Shortcut")});
  table->horizontalHeader()->setStretchLastSection(true);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  shortcutsLayout->addWidget(table);
  tabs->addTab(shortcutsTab, tr("Shortcuts"));

  QMap<QString, QAction *> actionMap;
  QList<QMenu *> menus;
  QSet<QMenu *> visited;
  for (QAction *top : menuBar()->actions()) {
    if (QMenu *m = top->menu())
      menus.append(m);
  }
  while (!menus.isEmpty()) {
    QMenu *menu = menus.takeFirst();
    if (visited.contains(menu))
      continue;
    visited.insert(menu);
    for (QAction *action : menu->actions()) {
      if (action->isSeparator())
        continue;
      if (QMenu *sub = action->menu()) {
        menus.append(sub);
      } else {
        QString menuName = menu->title();
        menuName.remove('&');
        QString text = action->text();
        text.remove('&');
        if (text.isEmpty())
          continue;
        QString key = menuName + QLatin1Char('/') + text;
        actionMap.insert(key, action);
      }
    }
  }

  table->setRowCount(actionMap.size());
  int row = 0;
  QMap<QString, QKeySequenceEdit *> shortcutEdits;
  for (auto it = actionMap.begin(); it != actionMap.end(); ++it) {
    const QString key = it.key();
    const int slash = key.indexOf(QLatin1Char('/'));
    const QString menuName = key.left(slash);
    const QString actionName = key.mid(slash + 1);
    table->setItem(row, 0, new QTableWidgetItem(menuName));
    table->setItem(row, 1, new QTableWidgetItem(actionName));
    auto *seqEdit = new QKeySequenceEdit(it.value()->shortcut(), &dlg);
    shortcutEdits.insert(key, seqEdit);
    table->setCellWidget(row, 2, seqEdit);
    ++row;
  }

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  mainLayout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() != QDialog::Accepted)
    return;

  const QString pullMode = pullModeCombo->currentData().toString();
  settings.setValue("pullMode", pullMode);
  settings.setValue("reopenLastRepo", reopenBox->isChecked());
  const QString themeMode = themeCombo->currentData().toString();
  settings.setValue("theme/mode", themeMode);

  QPalette newPalette;
  if (themeMode == QLatin1String("dark")) {
    newPalette.setColor(QPalette::Window, QColor(0x1e, 0x1e, 0x1e));
    newPalette.setColor(QPalette::WindowText, QColor(0xd4, 0xd4, 0xd4));
    newPalette.setColor(QPalette::Base, QColor(0x25, 0x25, 0x26));
    newPalette.setColor(QPalette::AlternateBase, QColor(0x2d, 0x2d, 0x30));
    newPalette.setColor(QPalette::Text, QColor(0xd4, 0xd4, 0xd4));
    newPalette.setColor(QPalette::Button, QColor(0x3c, 0x3c, 0x3c));
    newPalette.setColor(QPalette::ButtonText, QColor(0xd4, 0xd4, 0xd4));
    newPalette.setColor(QPalette::Highlight, QColor(0x09, 0x47, 0x71));
    newPalette.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    newPalette.setColor(QPalette::Link, QColor(0x37, 0x94, 0xff));
    newPalette.setColor(QPalette::ToolTipBase, QColor(0x1e, 0x1e, 0x1e));
    newPalette.setColor(QPalette::ToolTipText, QColor(0xd4, 0xd4, 0xd4));
  } else if (themeMode == QLatin1String("light")) {
    newPalette.setColor(QPalette::Window, QColor(0xf0, 0xf0, 0xf0));
    newPalette.setColor(QPalette::WindowText, QColor(0x00, 0x00, 0x00));
    newPalette.setColor(QPalette::Base, QColor(0xff, 0xff, 0xff));
    newPalette.setColor(QPalette::AlternateBase, QColor(0xf5, 0xf5, 0xf5));
    newPalette.setColor(QPalette::Text, QColor(0x00, 0x00, 0x00));
    newPalette.setColor(QPalette::Button, QColor(0xe0, 0xe0, 0xe0));
    newPalette.setColor(QPalette::ButtonText, QColor(0x00, 0x00, 0x00));
    newPalette.setColor(QPalette::Highlight, QColor(0x00, 0x78, 0xd7));
    newPalette.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    newPalette.setColor(QPalette::Link, QColor(0x00, 0x00, 0xff));
    newPalette.setColor(QPalette::ToolTipBase, QColor(0xff, 0xff, 0xff));
    newPalette.setColor(QPalette::ToolTipText, QColor(0x00, 0x00, 0x00));
  } else {
    for (int i = 0; i < roleList.size(); ++i) {
      const QColor c = colorButtons[i]->property("color").value<QColor>();
      if (c.isValid())
        newPalette.setColor(QPalette::Active, roleList[i].second, c);
    }
  }
  qApp->setStyle(QStyleFactory::create("Fusion"));
  qApp->setPalette(newPalette);
  qApp->setStyleSheet(
      QStringLiteral("QMenuBar { color: %1; } "
                     "QMenu::item { color: %2; }"
                     "QLineEdit { color: %3; background-color: %4; }"
                     "QTextEdit { color: %3; background-color: %4; }")
          .arg(newPalette.color(QPalette::WindowText).name(),
               newPalette.color(QPalette::Text).name(),
               newPalette.color(QPalette::Text).name(),
               newPalette.color(QPalette::Base).name()));

  for (int i = 0; i < roleList.size(); ++i) {
    const QColor c = newPalette.color(QPalette::Active, roleList[i].second);
    if (c.isValid())
      settings.setValue(QLatin1String("theme/palette/") + roleList[i].first, c);
  }

  settings.setValue("gpgSigningKey", gpgKeyEdit->text().trimmed());

  for (auto it = actionMap.begin(); it != actionMap.end(); ++it) {
    QKeySequenceEdit *seqEdit = shortcutEdits.value(it.key());
    if (!seqEdit)
      continue;
    const QKeySequence seq = seqEdit->keySequence();
    it.value()->setShortcut(seq);
    settings.setValue(QLatin1String("shortcuts/") + it.key(), seq.toString());
  }

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
  } else if (pullMode == QLatin1String("fetchAll")) {
    m_pullArgs.clear();
    m_pullArgs << "fetch" << "--all";
  } else {
    m_pullArgs.clear();
    m_pullArgs << "pull";
  }

  updateRecentRepos();
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

void MainWindow::loadRepository(const QString &path) {
  if (path.isEmpty())
    return;

  if (!m_currentPath.isEmpty() && m_currentPath != path)
    saveDockAndColumnState(false);

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
  if (!isInitialLoad)
    restoreDockAndColumnState(false);
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
  for (const QString &branch :
       m_gitExecutor->run(path, {"branch", "--format=%(refname:short)"})) {
    auto *item = new QTreeWidgetItem(m_localBranchesItem, QStringList{branch});
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
  for (const QString &tag : m_gitExecutor->run(path, {"tag", "--list"})) {
    new QTreeWidgetItem(m_tagsItem, QStringList{tag});
  }
  m_tagsItem->setExpanded(true);

  m_submodulesItem = new QTreeWidgetItem(m_repoPanel, {tr("Submodules")});
  for (const QString &line :
       m_gitExecutor->run(path, {"submodule", "status"})) {
    if (line.length() < 2)
      continue;
    const QChar status = line.at(0);
    const QString rest = line.mid(1).trimmed();
    const int shaEnd = rest.indexOf(' ');
    if (shaEnd < 0)
      continue;
    const QString subPath = rest.mid(shaEnd + 1).section('(', 0, 0).trimmed();
    QString text = subPath;
    if (status == '+')
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

  m_commitTable->clear();
  m_commitTable->setRowCount(0);
  m_commitTable->setHorizontalHeaderLabels(
      {tr("Graph"), tr("Date/Time"), tr("Date"), tr("Commit Message"),
       tr("Author"), tr("Branches"), tr("SHA")});
  m_commitTable->horizontalHeader()->setVisible(true);

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

  QProcess p;
  p.start("git",
          QStringList{"-C", path} +
              QStringList{"log", "--all", "--graph", "--source", "--date-order",
                          "--date=format:%Y-%m-%d %H:%M:%S",
                          "--pretty=format:%x1f%H%x1f%h%x1f%an%x1f%ad%x1f%ar%"
                          "x1f%s%x1f%b%x1f%S%x1e"});
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
    if (!m_remoteBranchName.isEmpty()) {
      for (const QString &sha : m_gitExecutor->run(
               path, {"log", "--format=%H", m_remoteBranchName + "..HEAD"}))
        m_unpushedShas.insert(sha);
      for (const QString &sha : m_gitExecutor->run(
               path, {"log", "--format=%H", "HEAD.." + m_remoteBranchName}))
        m_unpulledShas.insert(sha);
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

      const QString tip =
          tr("Subject: %1\n\n%2\n\nDate: %3\nAuthor: %4\nSHA: %5 (%6)")
              .arg(c.subject)
              .arg(c.body)
              .arg(c.date)
              .arg(c.author)
              .arg(c.shortSha)
              .arg(c.fullSha);

      QBrush bgBrush;
      if (m_unpushedShas.contains(c.fullSha))
        bgBrush = QBrush(QColor(225, 255, 225));
      else if (m_unpulledShas.contains(c.fullSha))
        bgBrush = QBrush(QColor(255, 240, 225));

      const int row = m_commitTable->rowCount();
      m_commitTable->insertRow(row);

      auto *graphItem = new QTableWidgetItem(c.graph);
      graphItem->setToolTip(tip);
      QFont graphFont;
      graphFont.setStyleHint(QFont::Monospace);
      graphFont.setFamily("Monospace");
      graphItem->setFont(graphFont);
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

      auto *branchItem = new QTableWidgetItem(c.branch);
      branchItem->setToolTip(tip);
      if (bgBrush != QBrush())
        branchItem->setBackground(bgBrush);
      m_commitTable->setItem(row, 5, branchItem);

      auto *shaItem = new QTableWidgetItem(c.shortSha);
      shaItem->setData(Qt::UserRole, c.fullSha);
      shaItem->setToolTip(tip);
      QFont shaFont;
      shaFont.setStyleHint(QFont::Monospace);
      shaFont.setFamily(QStringLiteral("Monospace"));
      shaItem->setFont(shaFont);
      if (bgBrush != QBrush())
        shaItem->setBackground(bgBrush);
      m_commitTable->setItem(row, 6, shaItem);
    }
  }
  if (!m_commitTableWidthInitialized) {
    m_commitTable->resizeColumnsToContents();
    m_commitTableWidthInitialized = true;
  }
  const int tableWidth =
      m_commitTable->horizontalHeader()->length() +
      QApplication::style()->pixelMetric(QStyle::PM_ScrollBarExtent) +
      2 * m_commitTable->frameWidth();
  if (isInitialLoad) {
    const int leftWidth = qBound(
        80, m_repoPanel ? m_repoPanel->sizeHint().width() + 20 : 180, 200);
    const int rightWidth = 200;
    if (m_repoDock && m_workTreeDock) {
      resizeDocks({m_repoDock, m_workTreeDock}, {leftWidth, rightWidth},
                  Qt::Horizontal);
    }
    resize(leftWidth + tableWidth + rightWidth, height());
  }
  qDebug() << "loadRepository: loaded" << m_commitTable->rowCount()
           << "commits";
  updateFilter();

  loadWorkingTree();

  const QString upstream =
      m_gitExecutor->run(path, {"rev-parse", "@{u}"}).value(0);
  m_lastRepoSignature =
      m_localHeadSha + '|' + upstream + '|' +
      m_gitExecutor->run(path, {"status", "--porcelain"}).join('\n') + '|' +
      m_gitExecutor->run(path, {"tag", "--list"}).join('\n');

  statusBar()->showMessage(tr("Loaded: %1").arg(path));
}

void MainWindow::launchGitTool(const QStringList &args, bool reload) {
  if (m_currentPath.isEmpty() || args.isEmpty())
    return;

  const QString command = args.first();
  QString configKey;
  if (command == QStringLiteral("difftool"))
    configKey = QStringLiteral("diff.tool");
  else if (command == QStringLiteral("mergetool"))
    configKey = QStringLiteral("merge.tool");
  if (!configKey.isEmpty() &&
      m_gitExecutor->run(m_currentPath, {"config", configKey}).isEmpty()) {
    statusBar()->showMessage(
        tr("No %1 configured in Repository Settings").arg(configKey));
    return;
  }

  auto *p = new QProcess(this);
  p->setWorkingDirectory(m_currentPath);
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert(QStringLiteral("GIT_EDITOR"), QStringLiteral("true"));
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

void MainWindow::showBranchContextMenu(const QPoint &pos) {
  QTreeWidgetItem *item = m_repoPanel->itemAt(pos);
  if (!item) {
    return;
  }

  const bool isLocal = (item->parent() == m_localBranchesItem);
  const bool isRemote =
      (item->parent() && item->parent()->parent() == m_remoteBranchesItem);
  if (!isLocal && !isRemote) {
    return;
  }

  const QString branchName = item->text(0);
  QMenu menu(this);
  QAction *selected = nullptr;

  if (isLocal) {
    auto *switchAction = menu.addAction(tr("&Switch to %1").arg(branchName));
    auto *pushAction = menu.addAction(tr("&Push %1 to remote").arg(branchName));
    auto *setUpstreamAction = menu.addAction(tr("Set u&pstream"));
    auto *createAction =
        menu.addAction(tr("&Create branch from %1").arg(branchName));
    auto *renameAction = menu.addAction(tr("&Rename"));
    const QString currentBranch =
        m_gitExecutor->run(m_currentPath, {"rev-parse", "--abbrev-ref", "HEAD"})
            .value(0);
    auto *mergeAction =
        (branchName != currentBranch)
            ? menu.addAction(tr("&Merge %1 into current").arg(branchName))
            : nullptr;
    auto *rebaseAction =
        menu.addAction(tr("Reb&ase %1 onto...").arg(branchName));
    auto *deleteAction =
        (branchName != currentBranch) ? menu.addAction(tr("&Delete")) : nullptr;
    selected = menu.exec(m_repoPanel->viewport()->mapToGlobal(pos));

    if (!selected) {
      return;
    }

    if (selected == switchAction) {
      if (m_gitExecutor->exec(m_currentPath, {"checkout", branchName})) {
        loadRepository(m_currentPath);
      } else {
        statusBar()->showMessage(tr("Failed to switch to %1").arg(branchName));
      }
    } else if (selected == pushAction) {
      const QStringList remotes = m_gitExecutor->run(m_currentPath, {"remote"});
      if (remotes.isEmpty()) {
        QMessageBox::warning(this, tr("No remotes"),
                             tr("There are no remotes to push to."));
        return;
      }
      bool okRemote;
      const QString remote =
          QInputDialog::getItem(this, tr("Push to Remote"), tr("Remote:"),
                                remotes, 0, false, &okRemote);
      if (!okRemote || remote.isEmpty())
        return;

      bool okDest;
      const QString destBranch = QInputDialog::getText(
          this, tr("Push to Branch"), tr("Remote branch name:"),
          QLineEdit::Normal, branchName, &okDest);
      if (!okDest || destBranch.isEmpty())
        return;

      const QString ref = (destBranch == branchName)
                              ? branchName
                              : branchName + ":" + destBranch;
      if (m_gitExecutor->exec(m_currentPath, {"push", "-u", remote, ref})) {
        loadRepository(m_currentPath);
        statusBar()->showMessage(
            tr("Pushed %1 to %2/%3").arg(branchName, remote, destBranch));
      } else {
        statusBar()->showMessage(tr("Failed to push %1").arg(branchName));
      }
    } else if (selected == setUpstreamAction) {
      const QStringList remoteBranchesRaw =
          m_gitExecutor->run(m_currentPath, {"branch", "-r"});
      QStringList remoteBranches;
      for (const QString &line : remoteBranchesRaw) {
        if (line.contains(QLatin1String(" -> ")))
          continue;
        remoteBranches.append(line.trimmed());
      }
      if (remoteBranches.isEmpty()) {
        QMessageBox::warning(this, tr("No remote branches"),
                             tr("There are no remote branches to track."));
        return;
      }
      bool ok;
      const QString upstream =
          QInputDialog::getItem(this, tr("Set Upstream"), tr("Remote branch:"),
                                remoteBranches, 0, false, &ok);
      if (!ok || upstream.isEmpty())
        return;
      if (m_gitExecutor->exec(m_currentPath, {"branch", "--set-upstream-to",
                                              upstream, branchName})) {
        loadRepository(m_currentPath);
        statusBar()->showMessage(
            tr("Upstream set for %1 to %2").arg(branchName, upstream));
      } else {
        statusBar()->showMessage(
            tr("Failed to set upstream for %1").arg(branchName));
      }
    } else if (selected == createAction) {
      bool ok;
      const QString newName =
          QInputDialog::getText(this, tr("Create Branch"), tr("Branch name:"),
                                QLineEdit::Normal, QString(), &ok);
      if (ok && !newName.isEmpty()) {
        if (m_gitExecutor->exec(m_currentPath,
                                {"branch", newName, branchName})) {
          loadRepository(m_currentPath);
        } else {
          statusBar()->showMessage(tr("Failed to create branch %1 from %2")
                                       .arg(newName, branchName));
        }
      }
    } else if (selected == renameAction) {
      bool ok;
      const QString newName =
          QInputDialog::getText(this, tr("Rename Branch"), tr("New name:"),
                                QLineEdit::Normal, branchName, &ok);
      if (ok && !newName.isEmpty() && newName != branchName) {
        if (m_gitExecutor->exec(m_currentPath,
                                {"branch", "-m", branchName, newName})) {
          loadRepository(m_currentPath);
        } else {
          statusBar()->showMessage(tr("Failed to rename %1").arg(branchName));
        }
      }
    } else if (selected == mergeAction) {
      QString output;
      if (m_gitExecutor->exec(
              m_currentPath,
              {"merge", "-m",
               tr("Merge branch %1 into %2").arg(branchName, currentBranch),
               branchName},
              &output)) {
        loadRepository(m_currentPath);
        statusBar()->showMessage(
            tr("Merged %1 into %2").arg(branchName, currentBranch));
      } else {
        QMessageBox::warning(this, tr("Merge failed"), output);
      }
    } else if (selected == rebaseAction) {
      QStringList branches = m_gitExecutor->run(
          m_currentPath, {"branch", "--format=%(refname:short)"});
      branches.removeOne(branchName);
      if (branches.isEmpty()) {
        QMessageBox::warning(this, tr("No target branch"),
                             tr("There are no other branches to rebase onto."));
        return;
      }

      bool ok;
      const QString targetBranch = QInputDialog::getItem(
          this, tr("Rebase %1").arg(branchName),
          tr("Rebase %1 onto:").arg(branchName), branches, 0, false, &ok);
      if (!ok || targetBranch.isEmpty())
        return;

      QString output;
      if (m_gitExecutor->exec(m_currentPath,
                              {"rebase", targetBranch, branchName}, &output)) {
        loadRepository(m_currentPath);
        statusBar()->showMessage(
            tr("Rebased %1 onto %2").arg(branchName, targetBranch));
      } else {
        QMessageBox::warning(this, tr("Rebase failed"), output);
      }
    } else if (selected == deleteAction) {
      if (m_gitExecutor->exec(m_currentPath, {"branch", "-d", branchName})) {
        loadRepository(m_currentPath);
      } else {
        statusBar()->showMessage(tr("Failed to delete %1").arg(branchName));
      }
    }
  } else {
    const QString fullBranchName = item->parent()->text(0) + "/" + branchName;
    auto *checkoutAction = menu.addAction(tr("Checkout as tracking branch"));
    auto *deleteRemoteAction =
        menu.addAction(tr("Delete remote branch %1").arg(fullBranchName));
    selected = menu.exec(m_repoPanel->viewport()->mapToGlobal(pos));

    if (!selected) {
      return;
    }

    if (selected == checkoutAction) {
      bool ok;
      const QString localName = QInputDialog::getText(
          this, tr("Checkout Remote Branch"), tr("Local branch name:"),
          QLineEdit::Normal, branchName, &ok);
      if (ok && !localName.isEmpty()) {
        if (m_gitExecutor->exec(
                m_currentPath, {"checkout", "-b", localName, fullBranchName})) {
          loadRepository(m_currentPath);
        } else {
          statusBar()->showMessage(
              tr("Failed to checkout %1 as %2").arg(fullBranchName, localName));
        }
      }
    } else if (selected == deleteRemoteAction) {
      const QString remote = item->parent()->text(0);
      const QString rbranch = branchName;
      if (m_gitExecutor->exec(m_currentPath,
                              {"push", remote, "--delete", rbranch})) {
        loadRepository(m_currentPath);
      } else {
        statusBar()->showMessage(
            tr("Failed to delete remote branch %1").arg(fullBranchName));
      }
    }
  }
}

void MainWindow::showStashContextMenu(const QPoint &pos) {
  QTreeWidgetItem *item = m_repoPanel->itemAt(pos);
  if (!item || !m_stashesItem)
    return;

  QMenu menu(this);
  if (item == m_stashesItem) {
    auto *createAction = menu.addAction(tr("&Create Stash"));
    if (menu.exec(m_repoPanel->viewport()->mapToGlobal(pos)) != createAction)
      return;

    bool ok;
    const QString message =
        QInputDialog::getText(this, tr("Create Stash"), tr("Message:"),
                              QLineEdit::Normal, QString(), &ok);
    if (!ok || message.isEmpty())
      return;

    if (m_gitExecutor->exec(m_currentPath, {"stash", "push", "-m", message})) {
      loadRepository(m_currentPath);
      statusBar()->showMessage(tr("Stash created"));
    } else {
      statusBar()->showMessage(tr("Failed to create stash"));
    }
    return;
  }

  if (item->parent() != m_stashesItem)
    return;

  const QString ref = item->data(0, Qt::UserRole).toString();
  if (ref.isEmpty())
    return;

  auto *popAction = menu.addAction(tr("&Pop Stash"));
  auto *applyAction = menu.addAction(tr("&Apply Stash"));
  auto *deleteAction = menu.addAction(tr("&Delete Stash"));
  auto *viewDiffAction = menu.addAction(tr("&View diff"));
  QAction *selected = menu.exec(m_repoPanel->viewport()->mapToGlobal(pos));

  if (selected == popAction) {
    if (m_gitExecutor->exec(m_currentPath, {"stash", "pop", ref})) {
      loadRepository(m_currentPath);
      statusBar()->showMessage(tr("Stash popped"));
    } else {
      statusBar()->showMessage(tr("Failed to pop stash"));
    }
  } else if (selected == applyAction) {
    if (m_gitExecutor->exec(m_currentPath, {"stash", "apply", ref})) {
      loadRepository(m_currentPath);
      statusBar()->showMessage(tr("Stash applied"));
    } else {
      statusBar()->showMessage(tr("Failed to apply stash"));
    }
  } else if (selected == deleteAction) {
    if (m_gitExecutor->exec(m_currentPath, {"stash", "drop", ref})) {
      loadStashes();
      statusBar()->showMessage(tr("Stash deleted"));
    } else {
      statusBar()->showMessage(tr("Failed to delete stash"));
    }
  } else if (selected == viewDiffAction) {
    showStashDiff(ref);
  }
}

void MainWindow::onBranchClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column)
  if (!item)
    return;

  QString branch;
  if (m_localBranchesItem && item->parent() == m_localBranchesItem) {
    branch = item->text(0);
  } else if (m_remoteBranchesItem && item->parent() &&
             item->parent()->parent() == m_remoteBranchesItem) {
    branch = item->parent()->text(0) + "/" + item->text(0);
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

void MainWindow::showTagContextMenu(const QPoint &pos) {
  QTreeWidgetItem *item = m_repoPanel->itemAt(pos);
  if (!item || !m_tagsItem)
    return;

  QMenu menu(this);
  if (item == m_tagsItem) {
    auto *createAction = menu.addAction(tr("&Create Tag at HEAD"));
    if (menu.exec(m_repoPanel->viewport()->mapToGlobal(pos)) == createAction) {
      bool ok;
      const QString tagName =
          QInputDialog::getText(this, tr("Create Tag"), tr("Tag name:"),
                                QLineEdit::Normal, QString(), &ok);
      if (ok && !tagName.isEmpty()) {
        if (tagName.contains(QLatin1Char(' '))) {
          QMessageBox::warning(this, tr("Invalid tag name"),
                               tr("Tag names cannot contain spaces."));
        } else {
          QString output;
          if (m_gitExecutor->exec(m_currentPath, {"tag", tagName}, &output)) {
            loadRepository(m_currentPath);
            statusBar()->showMessage(tr("Tag %1 created").arg(tagName));
          } else {
            QMessageBox::warning(this, tr("Create tag failed"), output);
          }
        }
      }
    }
  } else if (item->parent() == m_tagsItem) {
    const QString tagName = item->text(0);
    auto *checkoutAction = menu.addAction(tr("Ch&eckout Tag %1").arg(tagName));
    auto *pushAction = menu.addAction(tr("&Push Tag %1").arg(tagName));
    auto *deleteAction = menu.addAction(tr("&Delete Tag %1").arg(tagName));
    QAction *selected = menu.exec(m_repoPanel->viewport()->mapToGlobal(pos));

    if (selected == checkoutAction) {
      QString output;
      if (m_gitExecutor->exec(m_currentPath, {"checkout", tagName}, &output)) {
        loadRepository(m_currentPath);
        statusBar()->showMessage(tr("Checked out tag %1").arg(tagName));
      } else {
        QMessageBox::warning(this, tr("Checkout failed"), output);
      }
    } else if (selected == pushAction) {
      const QStringList remotes = m_gitExecutor->run(m_currentPath, {"remote"});
      if (remotes.isEmpty()) {
        QMessageBox::warning(this, tr("No remotes"),
                             tr("There are no remotes to push to."));
      } else {
        bool okRemote;
        const QString remote =
            QInputDialog::getItem(this, tr("Push Tag %1").arg(tagName),
                                  tr("Remote:"), remotes, 0, false, &okRemote);
        if (okRemote && !remote.isEmpty()) {
          if (m_gitExecutor->exec(m_currentPath, {"push", remote, tagName})) {
            statusBar()->showMessage(
                tr("Pushed tag %1 to %2").arg(tagName, remote));
          } else {
            statusBar()->showMessage(tr("Failed to push tag %1").arg(tagName));
          }
        }
      }
    } else if (selected == deleteAction) {
      if (m_gitExecutor->exec(m_currentPath, {"tag", "-d", tagName})) {
        loadRepository(m_currentPath);
        statusBar()->showMessage(tr("Tag %1 deleted").arg(tagName));
      } else {
        statusBar()->showMessage(tr("Failed to delete tag %1").arg(tagName));
      }
    }
  }
}

void MainWindow::onTagClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column)
  qDebug() << "onTagClicked: item=" << item << "m_tagsItem=" << m_tagsItem;
  if (!item || !m_tagsItem || item->parent() != m_tagsItem) {
    qDebug() << "onTagClicked: early return, not a tag item";
    return;
  }

  const QString tagName = item->text(0);
  qDebug() << "onTagClicked: tagName=" << tagName;
  const QString sha =
      m_gitExecutor->run(m_currentPath, {"log", "-1", tagName, "--format=%H"})
          .value(0);
  qDebug() << "onTagClicked: resolved sha=" << sha
           << "tableRows=" << m_commitTable->rowCount();
  if (sha.isEmpty()) {
    qDebug() << "onTagClicked: empty SHA, returning";
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
      qDebug() << "onTagClicked: scrolled to row=" << row;
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
  if (m_currentPath.isEmpty()) {
    return;
  }

  m_workingTreeModel->load(
      m_gitExecutor->run(m_currentPath, {"diff", "--cached", "--name-status"}),
      m_gitExecutor->run(m_currentPath, {"diff", "--name-status"}),
      m_gitExecutor->run(m_currentPath,
                         {"ls-files", "--others", "--exclude-standard"}));

  for (const FileStatus &fs : m_workingTreeModel->stagedFiles())
    m_stagedTree->addFile(fs.first, fs.second);

  for (const FileStatus &fs : m_workingTreeModel->unstagedFiles())
    m_unstagedTree->addFile(fs.first, fs.second);

  if (m_stagedTree)
    m_stagedTree->collapseAll();
  if (m_unstagedTree)
    m_unstagedTree->collapseAll();
  updateCommitButton();
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

void MainWindow::onAmendToggled(int state) {
  if (state == Qt::Checked && m_currentPath.isEmpty())
    return;

  if (state == Qt::Checked) {
    const QStringList lines =
        m_gitExecutor->run(m_currentPath, {"log", "-1", "--format=%B"});
    if (lines.isEmpty())
      return;

    if (m_commitSubject)
      m_commitSubject->setText(lines.first());
    if (m_commitBody) {
      m_commitBody->setText(lines.mid(1).join('\n').trimmed());
    }
  } else {
    if (m_commitSubject)
      m_commitSubject->clear();
    if (m_commitBody)
      m_commitBody->clear();
  }
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

void MainWindow::onCommitClicked() {
  if (m_currentPath.isEmpty()) {
    return;
  }
  const QString subject = m_commitSubject->text().trimmed();
  if (subject.isEmpty()) {
    statusBar()->showMessage(tr("Enter a commit subject"));
    return;
  }
  const QString body = m_commitBody->toPlainText().trimmed();
  QString message = subject;
  if (!body.isEmpty()) {
    message += "\n\n" + body;
  }

  QTemporaryFile tempFile;
  if (!tempFile.open()) {
    statusBar()->showMessage(tr("Failed to create commit message file"));
    return;
  }
  tempFile.write(message.toUtf8());
  tempFile.close();

  QStringList commitArgs =
      (m_amendCheckBox && m_amendCheckBox->isChecked())
          ? QStringList{"commit", "--amend", "-F", tempFile.fileName()}
          : QStringList{"commit", "-F", tempFile.fileName()};
  if (m_gitExecutor->exec(m_currentPath, commitArgs)) {
    m_commitSubject->clear();
    m_commitBody->clear();
    if (m_amendCheckBox)
      m_amendCheckBox->setChecked(false);
    loadRepository(m_currentPath);
    statusBar()->showMessage((m_amendCheckBox && m_amendCheckBox->isChecked())
                                 ? tr("Amended")
                                 : tr("Committed"));
  } else {
    statusBar()->showMessage(tr("Commit failed"));
  }
}

void MainWindow::toggleGpgConfig(bool enabled) {
  if (m_currentPath.isEmpty())
    return;

  m_gitExecutor->exec(
      m_currentPath,
      {QStringLiteral("config"), QStringLiteral("commit.gpgsign"),
       enabled ? QStringLiteral("true") : QStringLiteral("false")});

  if (!enabled)
    return;

  QString signingKey =
      m_gitExecutor
          ->run(m_currentPath,
                {QStringLiteral("config"), QStringLiteral("user.signingkey")})
          .value(0);
  if (signingKey.isEmpty()) {
    QSettings appSettings("GitClientQt", "GitClientQt");
    signingKey = appSettings.value("gpgSigningKey").toString().trimmed();
  }
  if (signingKey.isEmpty()) {
    signingKey =
        QInputDialog::getText(this, tr("GPG key"), tr("GPG key ID or email:"));
    if (signingKey.isEmpty()) {
      if (m_signCommitCheckBox) {
        m_signCommitCheckBox->blockSignals(true);
        m_signCommitCheckBox->setChecked(false);
        m_signCommitCheckBox->blockSignals(false);
      }
      m_gitExecutor->exec(m_currentPath, {QStringLiteral("config"),
                                          QStringLiteral("commit.gpgsign"),
                                          QStringLiteral("false")});
      return;
    }
    signingKey = signingKey.trimmed();
  }
  m_gitExecutor->exec(m_currentPath,
                      {QStringLiteral("config"),
                       QStringLiteral("user.signingkey"), signingKey});
}

void MainWindow::showEmptyDiff() {
  if (m_diffView)
    if (m_diffDock)
      m_diffDock->setVisible(false);
  m_diffView->showEmpty(tr("No diff"),
                        tr("Select a file or commit to view the diff."));
}

void MainWindow::showErrorDiff(const QString &message) {
  if (m_diffView)
    m_diffView->showError(message);
}

void MainWindow::showEmptyCommitFiles() {
  if (!m_commitFilesTree)
    return;
  m_commitFilesTree->clear();
  auto *placeholder = new QTreeWidgetItem(
      m_commitFilesTree,
      QStringList{tr("No commit selected — select a commit to view files")});
  placeholder->setToolTip(0, tr("Choose a commit from the table above"));
  placeholder->setFlags(Qt::NoItemFlags);
}

void MainWindow::showErrorCommitFiles(const QString &message) {
  if (!m_commitFilesTree)
    return;
  m_commitFilesTree->clear();
  auto *placeholder =
      new QTreeWidgetItem(m_commitFilesTree, QStringList{message});
  placeholder->setFlags(Qt::NoItemFlags);
}

void MainWindow::showUnstagedContextMenu(const QPoint &pos) {
  QTreeWidgetItem *item = m_unstagedTree->itemAt(pos);
  if (!item || m_currentPath.isEmpty()) {
    return;
  }

  const bool isFolder = item->childCount() > 0;
  const QString path = m_unstagedTree->itemPath(item);

  QList<QTreeWidgetItem *> leaves;
  QList<QTreeWidgetItem *> stack;
  stack.append(item);
  while (!stack.isEmpty()) {
    QTreeWidgetItem *node = stack.takeLast();
    if (node->childCount() == 0) {
      leaves.append(node);
    } else {
      for (int i = 0; i < node->childCount(); ++i) {
        stack.append(node->child(i));
      }
    }
  }

  bool hasTracked = false;
  bool hasNew = false;
  for (QTreeWidgetItem *leaf : leaves) {
    if (leaf->data(0, Qt::UserRole).toString() == "?")
      hasNew = true;
    else
      hasTracked = true;
  }

  QMenu menu(this);
  QAction *stageAction =
      menu.addAction(isFolder ? tr("&Stage folder") : tr("&Stage file"));
  stageAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
  if (!isFolder)
    stageAction->setIcon(
        QApplication::style()->standardIcon(QStyle::SP_ArrowRight));
  QAction *stashAction =
      menu.addAction(isFolder ? tr("S&tash folder") : tr("S&tash file"));
  QAction *discardAction =
      menu.addAction(isFolder ? tr("&Discard all changes in this folder")
                              : tr("&Discard all changes"));
  QAction *ignoreAction = nullptr;
  if (isFolder || hasNew) {
    ignoreAction = menu.addAction(
        isFolder ? tr("&Ignore all files in this folder") : tr("&Ignore"));
  }
  QAction *blameAction = nullptr;
  if (!isFolder) {
    blameAction = menu.addAction(tr("&Blame"));
  }
  QAction *stageHunksAction = nullptr;
  if (!isFolder && hasTracked) {
    stageHunksAction = menu.addAction(tr("Stage &hunks"));
  }
  QAction *externalDiffAction = nullptr;
  if (!isFolder && hasTracked &&
      !m_gitExecutor->run(m_currentPath, {"config", "diff.tool"}).isEmpty()) {
    externalDiffAction = menu.addAction(tr("O&pen in external diff tool"));
  }

  QAction *selected = menu.exec(m_unstagedTree->mapToGlobal(pos));
  if (!selected)
    return;
  if (selected == stageAction) {
    if (m_gitExecutor->exec(m_currentPath, {"add", path})) {
      loadWorkingTree();
    }
  } else if (selected == stashAction) {
    if (m_gitExecutor->exec(m_currentPath, {"stash", "push", "-m",
                                            "Stash " + path, "--", path})) {
      loadWorkingTree();
      loadStashes();
    }
  } else if (selected == discardAction) {
    bool ok = true;
    if (hasTracked) {
      ok &= m_gitExecutor->exec(m_currentPath, {"checkout", "--", path});
    }
    if (hasNew) {
      ok &= m_gitExecutor->exec(m_currentPath, {"clean", "-fd", "--", path});
    }
    if (ok) {
      loadWorkingTree();
      if (m_diffView)
        showEmptyDiff();
    }
  } else if (selected == ignoreAction) {
    const QString pattern = isFolder ? path + '/' : path;
    QFile gitignore(m_currentPath + "/.gitignore");
    if (gitignore.open(QIODevice::Append | QIODevice::Text)) {
      QTextStream out(&gitignore);
      out << pattern << "\n";
      gitignore.close();
    }
    loadWorkingTree();
  } else if (selected == blameAction) {
    showBlame(path);
  } else if (selected == stageHunksAction) {
    showHunkStaging(path, false);
  } else if (selected == externalDiffAction) {
    launchGitTool({QStringLiteral("difftool"), QStringLiteral("-y"),
                   QStringLiteral("--"), path});
  }
}

void MainWindow::showRemotesContextMenu(const QPoint &pos) {
  QTreeWidgetItem *item = m_repoPanel->itemAt(pos);
  if (!item || !m_remotesItem)
    return;

  QMenu menu(this);
  if (item == m_remotesItem) {
    auto *addAction = menu.addAction(tr("&Add Remote"));
    if (menu.exec(m_repoPanel->viewport()->mapToGlobal(pos)) != addAction)
      return;

    bool okName;
    const QString name =
        QInputDialog::getText(this, tr("Add Remote"), tr("Name:"),
                              QLineEdit::Normal, QString(), &okName);
    if (!okName || name.isEmpty())
      return;
    bool okUrl;
    const QString url =
        QInputDialog::getText(this, tr("Add Remote"), tr("URL:"),
                              QLineEdit::Normal, QString(), &okUrl);
    if (!okUrl || url.isEmpty())
      return;
    if (m_gitExecutor->exec(m_currentPath, {"remote", "add", name, url})) {
      loadRemotes();
      statusBar()->showMessage(tr("Remote %1 added").arg(name));
    } else {
      statusBar()->showMessage(tr("Failed to add remote %1").arg(name));
    }
  } else if (item->parent() == m_remotesItem) {
    const QString remoteName = item->data(0, Qt::UserRole).toString();
    const QString currentUrl = item->data(0, Qt::UserRole + 1).toString();
    auto *editAction = menu.addAction(tr("&Edit URL"));
    auto *renameAction = menu.addAction(tr("&Rename"));
    auto *pruneAction = menu.addAction(tr("&Prune"));
    auto *removeAction = menu.addAction(tr("&Remove Remote"));
    QAction *selected = menu.exec(m_repoPanel->viewport()->mapToGlobal(pos));

    if (selected == editAction) {
      bool ok;
      const QString newUrl =
          QInputDialog::getText(this, tr("Edit Remote URL"), tr("URL:"),
                                QLineEdit::Normal, currentUrl, &ok);
      if (ok && !newUrl.isEmpty() && newUrl != currentUrl) {
        if (m_gitExecutor->exec(m_currentPath,
                                {"remote", "set-url", remoteName, newUrl})) {
          loadRemotes();
          statusBar()->showMessage(tr("Remote %1 URL updated").arg(remoteName));
        } else {
          statusBar()->showMessage(
              tr("Failed to update remote %1").arg(remoteName));
        }
      }
    } else if (selected == renameAction) {
      bool ok;
      const QString newName =
          QInputDialog::getText(this, tr("Rename Remote"), tr("New name:"),
                                QLineEdit::Normal, remoteName, &ok);
      if (ok && !newName.isEmpty() && newName != remoteName) {
        if (m_gitExecutor->exec(m_currentPath,
                                {"remote", "rename", remoteName, newName})) {
          loadRemotes();
          statusBar()->showMessage(
              tr("Remote %1 renamed to %2").arg(remoteName, newName));
        } else {
          statusBar()->showMessage(
              tr("Failed to rename remote %1").arg(remoteName));
        }
      }
    } else if (selected == pruneAction) {
      if (m_gitExecutor->exec(m_currentPath, {"remote", "prune", remoteName})) {
        loadRepository(m_currentPath);
        statusBar()->showMessage(tr("Remote %1 pruned").arg(remoteName));
      } else {
        statusBar()->showMessage(
            tr("Failed to prune remote %1").arg(remoteName));
      }
    } else if (selected == removeAction) {
      if (QMessageBox::question(this, tr("Remove Remote"),
                                tr("Remove remote %1?").arg(remoteName)) ==
          QMessageBox::Yes) {
        if (m_gitExecutor->exec(m_currentPath,
                                {"remote", "remove", remoteName})) {
          loadRemotes();
          statusBar()->showMessage(tr("Remote %1 removed").arg(remoteName));
        } else {
          statusBar()->showMessage(
              tr("Failed to remove remote %1").arg(remoteName));
        }
      }
    }
  }
}

void MainWindow::showStagedContextMenu(const QPoint &pos) {
  QTreeWidgetItem *item = m_stagedTree->itemAt(pos);
  if (!item || m_currentPath.isEmpty()) {
    return;
  }

  const bool isFolder = item->childCount() > 0;
  QMenu menu(this);
  const QString path = m_stagedTree->itemPath(item);
  QAction *unstageAction =
      menu.addAction(isFolder ? tr("&Unstage folder") : tr("&Unstage file"));
  unstageAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_U));
  if (!isFolder)
    unstageAction->setIcon(
        QApplication::style()->standardIcon(QStyle::SP_ArrowLeft));
  QAction *unstageHunksAction = nullptr;
  if (!isFolder) {
    unstageHunksAction = menu.addAction(tr("U&nstage hunks"));
  }
  QAction *blameAction = nullptr;
  if (!isFolder) {
    blameAction = menu.addAction(tr("&Blame"));
  }
  QAction *externalDiffAction = nullptr;
  if (!isFolder &&
      !m_gitExecutor->run(m_currentPath, {"config", "diff.tool"}).isEmpty()) {
    externalDiffAction = menu.addAction(tr("O&pen in external diff tool"));
  }
  QAction *selected = menu.exec(m_stagedTree->mapToGlobal(pos));
  if (!selected)
    return;
  if (selected == unstageAction) {
    if (m_gitExecutor->exec(m_currentPath, {"reset", "HEAD", "--", path})) {
      loadWorkingTree();
    }
  } else if (selected == unstageHunksAction) {
    showHunkStaging(path, true);
  } else if (selected == blameAction) {
    showBlame(path);
  } else if (selected == externalDiffAction) {
    launchGitTool({QStringLiteral("difftool"), QStringLiteral("-y"),
                   QStringLiteral("--cached"), QStringLiteral("--"), path});
  }
}

void MainWindow::showCommitContextMenu(const QPoint &pos) {
  QTableWidgetItem *item = m_commitTable->itemAt(pos);
  if (!item || m_currentPath.isEmpty())
    return;

  const int row = item->row();
  QTableWidgetItem *shaItem = m_commitTable->item(row, 6);
  if (!shaItem)
    return;
  const QString sha = shaItem->data(Qt::UserRole).toString();
  if (sha.isEmpty())
    return;

  QMenu menu(this);
  auto *checkoutAction = menu.addAction(tr("&Checkout this Commit"));
  auto *createBranchAction =
      menu.addAction(tr("Create &branch from this commit"));
  auto *createTagAction = menu.addAction(tr("Create &Tag for this Commit"));
  auto *diffAction = menu.addAction(tr("&Diff with another commit"));
  auto *interactiveRebaseAction =
      menu.addAction(tr("&Interactive rebase from here"));
  auto *cherryPickAction = menu.addAction(tr("Cherry-&pick this commit"));
  auto *revertAction = menu.addAction(tr("&Revert this commit"));
  auto *squashAction = menu.addAction(tr("&Squash with previous"));
  auto *fixupAction = menu.addAction(tr("Fi&xup into previous"));
  auto *resetAction = menu.addAction(tr("R&eset to this commit"));
  auto *savePatchAction = menu.addAction(tr("Sa&ve as patch..."));
  QAction *selected = menu.exec(m_commitTable->viewport()->mapToGlobal(pos));

  if (selected == checkoutAction) {
    QString output;
    if (m_gitExecutor->exec(m_currentPath, {"checkout", sha}, &output)) {
      loadRepository(m_currentPath);
      statusBar()->showMessage(tr("Checked out %1").arg(sha.left(7)));
    } else {
      QMessageBox::warning(this, tr("Checkout failed"), output);
    }
    return;
  }

  if (selected == createBranchAction) {
    bool okBranch;
    const QString branchName =
        QInputDialog::getText(this, tr("Create Branch"), tr("Branch name:"),
                              QLineEdit::Normal, QString(), &okBranch);
    if (!okBranch || branchName.isEmpty())
      return;

    if (branchName.contains(QLatin1Char(' '))) {
      QMessageBox::warning(this, tr("Invalid branch name"),
                           tr("Branch names cannot contain spaces."));
      return;
    }

    if (m_gitExecutor->exec(m_currentPath, {"branch", branchName, sha})) {
      loadRepository(m_currentPath);
      statusBar()->showMessage(tr("Branch %1 created").arg(branchName));
    } else {
      statusBar()->showMessage(
          tr("Failed to create branch %1").arg(branchName));
    }
    return;
  }

  if (selected == diffAction) {
    diffWithCommit(sha);
    return;
  }

  if (selected == interactiveRebaseAction) {
    showInteractiveRebase(sha);
    return;
  }

  if (selected == cherryPickAction) {
    cherryPickCommit(sha);
    return;
  }

  if (selected == revertAction) {
    revertCommit(sha);
    return;
  }

  if (selected == squashAction || selected == fixupAction) {
    const QString base = sha + QLatin1String("~2");
    if (m_gitExecutor->run(m_currentPath, {"rev-parse", base}).isEmpty()) {
      QMessageBox::warning(
          this, tr("Cannot rebase"),
          tr("Selected commit has no previous commit to combine with."));
      return;
    }
    const QString shortSha =
        m_gitExecutor->run(m_currentPath, {"rev-parse", "--short", sha})
            .value(0);
    const QString command = (selected == squashAction)
                                ? QStringLiteral("squash")
                                : QStringLiteral("fixup");
    const QString sedCmd =
        QStringLiteral("sed -i 's/^pick %1 /%2 %1 /'").arg(shortSha, command);

    QProcess p;
    p.setWorkingDirectory(m_currentPath);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("GIT_SEQUENCE_EDITOR"), sedCmd);
    env.insert(QStringLiteral("GIT_EDITOR"), QStringLiteral("true"));
    p.setProcessEnvironment(env);
    p.start(QStringLiteral("git"),
            QStringList{QStringLiteral("-C"), m_currentPath,
                        QStringLiteral("rebase"), QStringLiteral("-i"), base});
    p.waitForFinished(-1);

    if (p.exitCode() == 0) {
      loadRepository(m_currentPath);
      statusBar()->showMessage(
          tr("%1d %2 into previous").arg(command, sha.left(7)));
    } else {
      QMessageBox::warning(this, tr("Rebase failed"),
                           QString::fromLocal8Bit(p.readAllStandardError() +
                                                  p.readAllStandardOutput()));
    }
    return;
  }

  if (selected == resetAction) {
    const QStringList modes = {tr("soft"), tr("mixed"), tr("hard")};
    bool ok;
    const QString mode =
        QInputDialog::getItem(this, tr("Reset %1").arg(sha.left(7)),
                              tr("Mode:"), modes, 1, false, &ok);
    if (ok && !mode.isEmpty()) {
      if (mode == tr("hard")) {
        if (QMessageBox::question(this, tr("Confirm Hard Reset"),
                                  tr("This will discard working tree changes. "
                                     "Continue?")) != QMessageBox::Yes)
          return;
      }
      QString output;
      if (m_gitExecutor->exec(m_currentPath,
                              {"reset", QStringLiteral("--%1").arg(mode), sha},
                              &output)) {
        loadRepository(m_currentPath);
        statusBar()->showMessage(tr("Reset %1 to %2").arg(mode, sha.left(7)));
      } else {
        QMessageBox::warning(this, tr("Reset failed"), output);
      }
    }
    return;
  }

  if (selected == savePatchAction) {
    createPatchFromCommit(sha);
    return;
  }

  if (selected != createTagAction)
    return;

  bool ok;
  const QString tagName =
      QInputDialog::getText(this, tr("Create Tag"), tr("Tag name:"),
                            QLineEdit::Normal, QString(), &ok);
  if (!ok || tagName.isEmpty())
    return;

  if (tagName.contains(QLatin1Char(' '))) {
    QMessageBox::warning(this, tr("Invalid tag name"),
                         tr("Tag names cannot contain spaces."));
    return;
  }

  QString output;
  if (m_gitExecutor->exec(m_currentPath, {"tag", tagName, sha}, &output)) {
    loadRepository(m_currentPath);
    statusBar()->showMessage(tr("Tag %1 created").arg(tagName));
  } else {
    QMessageBox::warning(this, tr("Create tag failed"), output);
  }
}

void MainWindow::diffWithCommit(const QString &fromSha) {
  if (!m_commitTable || !m_diffView || m_currentPath.isEmpty())
    return;

  QStringList items;
  QStringList shas;
  for (int row = 0; row < m_commitTable->rowCount(); ++row) {
    QTableWidgetItem *shaItem = m_commitTable->item(row, 6);
    if (!shaItem)
      continue;
    const QString sha = shaItem->data(Qt::UserRole).toString();
    if (sha.isEmpty() || sha == fromSha)
      continue;
    QTableWidgetItem *msgItem = m_commitTable->item(row, 3);
    const QString msg = msgItem ? msgItem->text() : QString();
    items.append(QStringLiteral("%1 - %2").arg(sha.left(7), msg));
    shas.append(sha);
  }

  if (items.isEmpty()) {
    statusBar()->showMessage(tr("No other commits to compare"));
    return;
  }

  bool ok = false;
  const QString selected = QInputDialog::getItem(
      this, tr("Diff with"), tr("Select commit:"), items, 0, false, &ok);
  if (!ok)
    return;

  const int index = items.indexOf(selected);
  if (index < 0 || index >= shas.size())
    return;

  const QString toSha = shas.at(index);
  const QStringList diff =
      m_gitExecutor->run(m_currentPath, {"diff", fromSha, toSha});
  if (diff.isEmpty()) {
    if (m_diffDock)
      m_diffDock->setVisible(false);
    m_diffView->showEmpty(tr("No diff"),
                          tr("No changes to show for this selection."));
  } else {
    m_diffDock->setVisible(true);
  }
  m_diffView->setHtml(m_diffPresenter->formatDiff(diff));
  statusBar()->showMessage(
      tr("Diff between %1 and %2").arg(fromSha.left(7), toSha.left(7)));
}

void MainWindow::showCommitFilesContextMenu(const QPoint &pos) {
  QTreeWidgetItem *item = m_commitFilesTree->itemAt(pos);
  if (!item || m_currentPath.isEmpty() || m_selectedCommitSha.isEmpty())
    return;

  if (item->childCount() > 0)
    return;

  const QString path = m_commitFilesTree->itemPath(item);
  QMenu menu(this);
  auto *externalDiffAction =
      !m_gitExecutor->run(m_currentPath, {"config", "diff.tool"}).isEmpty()
          ? menu.addAction(tr("&View diff in external diff tool"))
          : nullptr;
  if (externalDiffAction)
    menu.addSeparator();
  auto *blameAction = menu.addAction(tr("&Blame"));
  auto *selected = menu.exec(m_commitFilesTree->mapToGlobal(pos));

  if (selected == externalDiffAction) {
    launchGitTool({QStringLiteral("difftool"), QStringLiteral("-y"),
                   m_selectedCommitSha + QLatin1Char('^'), m_selectedCommitSha,
                   QStringLiteral("--"), path});
  } else if (selected == blameAction) {
    showBlame(path, m_selectedCommitSha);
  }
}

void MainWindow::showBlame(const QString &path, const QString &revision) {
  if (m_currentPath.isEmpty() || path.isEmpty())
    return;

  QStringList args = {QStringLiteral("blame"), QStringLiteral("--porcelain")};
  if (!revision.isEmpty())
    args.append(revision);
  args.append(QStringLiteral("--"));
  args.append(path);

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Blame %1").arg(path));
  auto *layout = new QVBoxLayout(&dlg);
  auto *table = new QTableWidget(&dlg);
  table->setColumnCount(4);
  table->setHorizontalHeaderLabels(
      {tr("SHA"), tr("Summary"), tr("Date"), tr("Source")});
  table->horizontalHeader()->setVisible(false);
  table->verticalHeader()->setVisible(false);
  table->setShowGrid(false);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->setFont(QFont(QStringLiteral("monospace"), 10));
  table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  layout->addWidget(table);

  QString currentSha;
  QString currentSummary;
  qint64 currentTime = 0;

  const QRegularExpression hunkRe(
      QStringLiteral("^(\\^?[0-9a-fA-F]{40}) (\\d+) (\\d+) (\\d+)$"));
  for (const QString &line : m_gitExecutor->run(m_currentPath, args)) {
    const QRegularExpressionMatch m = hunkRe.match(line);
    if (m.hasMatch()) {
      currentSha = m.captured(1);
      currentSha.remove(QLatin1Char('^'));
      currentSummary.clear();
      currentTime = 0;
    } else if (line.startsWith(QStringLiteral("author-time "))) {
      currentTime = line.mid(12).toLongLong();
    } else if (line.startsWith(QStringLiteral("summary "))) {
      currentSummary = line.mid(8);
    } else if (line.startsWith(QLatin1Char('\t'))) {
      const QString content = line.mid(1);
      const QString date = currentTime > 0
                               ? QDateTime::fromSecsSinceEpoch(currentTime)
                                     .toString(QStringLiteral("yyyy-MM-dd"))
                               : QString();

      const int row = table->rowCount();
      table->insertRow(row);

      auto *shaItem = new QTableWidgetItem(currentSha.left(7));
      shaItem->setBackground(QColor(0x25, 0x25, 0x26));
      shaItem->setForeground(QColor(0x9c, 0xdc, 0xfe));
      shaItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      table->setItem(row, 0, shaItem);

      auto *summaryItem = new QTableWidgetItem(currentSummary.left(60));
      summaryItem->setToolTip(currentSummary);
      summaryItem->setBackground(QColor(0x25, 0x25, 0x26));
      summaryItem->setForeground(QColor(0xdc, 0xdc, 0xaa));
      summaryItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      table->setItem(row, 1, summaryItem);

      auto *dateItem = new QTableWidgetItem(date);
      dateItem->setBackground(QColor(0x25, 0x25, 0x26));
      dateItem->setForeground(QColor(0x80, 0x80, 0x80));
      dateItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      table->setItem(row, 2, dateItem);

      auto *contentItem = new QTableWidgetItem(content);
      contentItem->setBackground(QColor(0x1e, 0x1e, 0x1e));
      contentItem->setForeground(QColor(0xd4, 0xd4, 0xd4));
      contentItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      table->setItem(row, 3, contentItem);
    }
  }

  auto *closeBtn = new QPushButton(tr("Close"), &dlg);
  connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
  layout->addWidget(closeBtn);
  dlg.resize(1000, 700);

  table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
  table->setColumnWidth(0, 70);
  table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
  table->setColumnWidth(1, 150);
  table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
  table->setColumnWidth(2, 80);
  table->horizontalHeader()->setStretchLastSection(true);

  dlg.show();
  table->resizeRowsToContents();
  dlg.exec();
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
  if (!shaItem) {
    showEmptyCommitFiles();
    return;
  }

  m_selectedCommitSha = shaItem->data(Qt::UserRole).toString();

  for (const QString &line : m_gitExecutor->run(
           m_currentPath, {"diff-tree", "--no-commit-id", "--name-status",
                           "--root", "-r", m_selectedCommitSha})) {
    const QStringList parts = line.split('\t');
    if (parts.size() < 2)
      continue;
    const QString status = parts.first();
    const QString filePath = parts.last();
    if (status.isEmpty() || filePath.isEmpty())
      continue;
    m_commitFilesTree->addFile(filePath, status);
  }

  if (m_commitFilesTree->topLevelItemCount() > 0) {
    m_commitFilesTree->collapseAll();
  } else {
    showErrorCommitFiles(tr("This commit has no file changes."));
  }
}

void MainWindow::onCommitFileClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column)
  if (!item || m_currentPath.isEmpty() || m_selectedCommitSha.isEmpty())
    return;

  const bool isFolder = item->childCount() > 0;
  if (isFolder)
    return;

  const QString path = m_commitFilesTree->itemPath(item);
  const QStringList diff = m_gitExecutor->run(
      m_currentPath, {"show", "--pretty=format:", "--no-notes",
                      m_selectedCommitSha, "--", path});
  if (m_diffView) {
    if (diff.isEmpty()) {
      if (m_diffDock)
        m_diffDock->setVisible(false);
      m_diffView->showEmpty(tr("No diff"),
                            tr("No changes to show for this selection."));
      return;
    }
    const QString html = m_diffPresenter->isLfsPointer(diff)
                             ? m_diffPresenter->lfsPointerHtml(diff)
                             : m_diffPresenter->formatDiff(diff);
    m_diffDock->setVisible(true);
    m_diffView->setHtml(html);
  }
}

void MainWindow::onInitRepository() {
  const QString path =
      QFileDialog::getExistingDirectory(this, tr("Initialize Repository"));
  if (path.isEmpty())
    return;

  if (m_gitExecutor->exec(path, {"rev-parse", "--git-dir"})) {
    QMessageBox::warning(this, tr("Already a Git repository"),
                         tr("The selected folder is already a Git "
                            "repository."));
    return;
  }

  if (m_gitExecutor->exec(path, {"init"})) {
    loadRepository(path);
    statusBar()->showMessage(tr("Repository initialized"));
  } else {
    statusBar()->showMessage(tr("Failed to initialize repository"));
  }
}

void MainWindow::onCloneRepository() {
  bool ok;
  const QString url =
      QInputDialog::getText(this, tr("Clone Repository"), tr("Remote URL:"),
                            QLineEdit::Normal, QString(), &ok);
  if (!ok || url.isEmpty())
    return;

  const QString parentDir =
      QFileDialog::getExistingDirectory(this, tr("Clone Destination"));
  if (parentDir.isEmpty())
    return;

  QString repoName = url.mid(url.lastIndexOf('/') + 1);
  if (repoName.endsWith(QLatin1String(".git")))
    repoName.chop(4);
  if (repoName.isEmpty())
    repoName = tr("repo");

  const QString localPath = parentDir + '/' + repoName;
  if (QFileInfo(localPath).exists()) {
    QMessageBox::warning(this, tr("Folder already exists"),
                         tr("The destination folder already exists."));
    return;
  }

  if (m_gitExecutor->exec(parentDir, {"clone", url, repoName})) {
    loadRepository(localPath);
    statusBar()->showMessage(tr("Cloned %1").arg(repoName));
  } else {
    statusBar()->showMessage(tr("Clone failed"));
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

void MainWindow::showStashDiff(const QString &ref) {
  if (m_currentPath.isEmpty() || ref.isEmpty())
    return;

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Stash diff"));
  auto *layout = new QVBoxLayout(&dlg);
  auto *diffView = new DiffViewWidget(&dlg);
  layout->addWidget(diffView);

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  const QStringList lines =
      m_gitExecutor->run(m_currentPath, {"stash", "show", "-p", ref});
  if (lines.isEmpty())
    diffView->showEmpty(tr("No diff"), tr("No changes in this stash."));
  else
    diffView->setHtml(m_diffPresenter->formatDiff(lines));

  dlg.resize(900, 600);
  dlg.exec();
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

void MainWindow::showWorktreeContextMenu(const QPoint &pos) {
  QTreeWidgetItem *item = m_repoPanel->itemAt(pos);
  if (!item || !m_worktreesItem)
    return;

  if (item == m_worktreesItem) {
    QMenu menu(this);
    auto *addAction = menu.addAction(tr("&Add worktree..."));
    if (menu.exec(m_repoPanel->viewport()->mapToGlobal(pos)) != addAction)
      return;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Add worktree"));
    auto *layout = new QFormLayout(&dlg);
    auto *pathEdit = new QLineEdit(&dlg);
    auto *branchEdit = new QLineEdit(&dlg);
    layout->addRow(tr("Path:"), pathEdit);
    layout->addRow(tr("Branch:"), branchEdit);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
      return;

    const QString worktreePath = pathEdit->text().trimmed();
    const QString branch = branchEdit->text().trimmed();
    if (worktreePath.isEmpty() || branch.isEmpty())
      return;

    const QString existingSha =
        m_gitExecutor->run(m_currentPath, {"rev-parse", "--verify", branch})
            .value(0);
    QStringList args = {"worktree", "add"};
    if (existingSha.isEmpty())
      args << "-b" << branch;
    args << worktreePath;
    if (!existingSha.isEmpty())
      args << branch;

    if (m_gitExecutor->exec(m_currentPath, args)) {
      loadWorktrees();
      statusBar()->showMessage(tr("Worktree added"));
    } else {
      statusBar()->showMessage(tr("Failed to add worktree"));
    }
    return;
  }

  if (item->parent() != m_worktreesItem)
    return;

  const QString path = item->data(0, Qt::UserRole).toString();
  if (path.isEmpty())
    return;

  QMenu menu(this);
  auto *openAction = menu.addAction(tr("&Open"));
  auto *removeAction = menu.addAction(tr("&Remove"));
  QAction *selected = menu.exec(m_repoPanel->viewport()->mapToGlobal(pos));

  if (selected == openAction) {
    onWorktreeClicked(item, 0);
  } else if (selected == removeAction) {
    if (QFileInfo(path).canonicalFilePath() ==
        QFileInfo(m_currentPath).canonicalFilePath()) {
      QMessageBox::warning(this, tr("Worktree"),
                           tr("Cannot remove the currently open worktree."));
      return;
    }
    if (m_gitExecutor->exec(m_currentPath, {"worktree", "remove", path})) {
      loadWorktrees();
      statusBar()->showMessage(tr("Worktree removed"));
    } else {
      statusBar()->showMessage(tr("Failed to remove worktree"));
    }
  }
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
      m_diffDock->setVisible(true);
    }
    m_diffView->setHtml(m_diffPresenter->formatDiff(diff));
  }
}

void MainWindow::onFileClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column)
  if (!item || m_currentPath.isEmpty()) {
    return;
  }

  auto *tree = qobject_cast<FileTreeWidget *>(sender());
  if (!tree) {
    return;
  }

  const QString path = tree->itemPath(item);
  const bool staged = (tree == m_stagedTree);
  const bool isFolder = item->childCount() > 0;
  const bool isNew =
      isFolder ? false : (item->data(0, Qt::UserRole).toString() == "?");

  if (isNew) {
    if (staged) {
      const QStringList diff =
          m_gitExecutor->run(m_currentPath, {"diff", "--cached", "--", path});
      if (m_diffView) {
        if (diff.isEmpty()) {
          if (m_diffDock)
            m_diffDock->setVisible(false);
          m_diffView->showEmpty(tr("No diff"),
                                tr("No changes to show for this selection."));
          return;
        }
        const QString html = m_diffPresenter->isLfsPointer(diff)
                                 ? m_diffPresenter->lfsPointerHtml(diff)
                                 : m_diffPresenter->formatDiff(diff);
        m_diffDock->setVisible(true);
        m_diffView->setHtml(html);
      }
    } else {
      const QStringList diff = m_gitExecutor->run(
          m_currentPath,
          {"diff", "--no-index", "--", QStringLiteral("/dev/null"), path}, 1);
      if (m_diffView) {
        if (diff.isEmpty()) {
          if (m_diffDock)
            m_diffDock->setVisible(false);
          m_diffView->showEmpty(tr("No diff"),
                                tr("No changes to show for this selection."));
          return;
        }
        const QString html = m_diffPresenter->isLfsPointer(diff)
                                 ? m_diffPresenter->lfsPointerHtml(diff)
                                 : m_diffPresenter->formatDiff(diff);
        m_diffDock->setVisible(true);
        m_diffView->setHtml(html);
      }
    }
    return;
  }

  const QStringList diff = m_gitExecutor->run(
      m_currentPath, staged ? QStringList{"diff", "--cached", "--", path}
                            : QStringList{"diff", "--", path});
  if (m_diffView) {
    if (diff.isEmpty()) {
      if (m_diffDock)
        m_diffDock->setVisible(false);
      m_diffView->showEmpty(tr("No diff"),
                            tr("No changes to show for this selection."));
      return;
    }
    m_currentDiffPath = path;
    m_currentDiffLines = diff;
    m_currentDiffUnstage = staged;
    m_currentDiffIsNew = false;
    const QString html = m_diffPresenter->isLfsPointer(diff)
                             ? m_diffPresenter->lfsPointerHtml(diff)
                             : m_diffPresenter->formatDiff(diff, true, staged);
    m_diffDock->setVisible(true);
    m_diffView->setHtml(html);
  }
}

void MainWindow::onDiffAnchorClicked(const QUrl &url) {
  const QString urlString = url.toString();
  if (!urlString.startsWith(QStringLiteral("git:hunk:")))
    return;

  const int hunkIndex = urlString.mid(9).toInt();
  if (m_currentPath.isEmpty() || m_currentDiffPath.isEmpty() ||
      m_currentDiffLines.isEmpty())
    return;

  const QRegularExpression hunkRe(
      QStringLiteral("^@@ -(\\d+)(?:,(\\d+))? \\+(\\d+)(?:,(\\d+))? @@"));
  QList<int> hunkStarts;
  for (int i = 0; i < m_currentDiffLines.size(); ++i) {
    if (hunkRe.match(m_currentDiffLines[i]).hasMatch())
      hunkStarts.append(i);
  }
  if (hunkIndex < 0 || hunkIndex >= hunkStarts.size())
    return;

  const int firstHunk = hunkStarts.first();
  QStringList headerLines;
  for (int i = 0; i < firstHunk; ++i)
    headerLines.append(m_currentDiffLines[i]);

  const int start = hunkStarts[hunkIndex];
  const int end = (hunkIndex + 1 < hunkStarts.size())
                      ? hunkStarts[hunkIndex + 1] - 1
                      : m_currentDiffLines.size() - 1;

  QStringList patchLines = headerLines;
  for (int i = start; i <= end; ++i)
    patchLines.append(m_currentDiffLines[i]);
  const QString patch = patchLines.join(QLatin1Char('\n')) + QLatin1Char('\n');

  QTemporaryFile tempFile;
  if (!tempFile.open())
    return;
  tempFile.write(patch.toUtf8());
  tempFile.close();

  const QStringList applyArgs =
      m_currentDiffUnstage
          ? QStringList{QStringLiteral("apply"), QStringLiteral("--cached"),
                        QStringLiteral("-R"), tempFile.fileName()}
          : QStringList{QStringLiteral("apply"), QStringLiteral("--cached"),
                        tempFile.fileName()};
  if (!m_gitExecutor->exec(m_currentPath, applyArgs))
    return;

  loadWorkingTree();

  const QStringList refreshArgs =
      m_currentDiffUnstage
          ? QStringList{QStringLiteral("diff"), QStringLiteral("--cached"),
                        QStringLiteral("--"), m_currentDiffPath}
          : QStringList{QStringLiteral("diff"), QStringLiteral("--"),
                        QStringLiteral("--"), m_currentDiffPath};
  m_currentDiffLines = m_gitExecutor->run(m_currentPath, refreshArgs);
  if (m_currentDiffLines.isEmpty() || m_diffView == nullptr) {
    showEmptyDiff();
    return;
  }
  m_diffView->setHtml(m_diffPresenter->formatDiff(m_currentDiffLines, true,
                                                  m_currentDiffUnstage));
}

void MainWindow::editGitignore() {
  if (m_currentPath.isEmpty()) {
    QMessageBox::warning(this, tr("No repository"),
                         tr("Open a repository first."));
    return;
  }

  const QString gitignorePath = m_currentPath + QStringLiteral("/.gitignore");

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Edit .gitignore"));
  dlg.setMinimumSize(500, 400);
  dlg.resize(700, 500);
  auto *layout = new QVBoxLayout(&dlg);
  auto *edit = new QTextEdit(&dlg);
  edit->setFont(QFont(QStringLiteral("monospace"), 10));
  layout->addWidget(edit);

  QString content;
  QFile file(gitignorePath);
  if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    content = QString::fromUtf8(file.readAll());
    file.close();
  }
  edit->setPlainText(content);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() == QDialog::Accepted) {
    QFile out(gitignorePath);
    if (out.open(QIODevice::WriteOnly | QIODevice::Text)) {
      out.write(edit->toPlainText().toUtf8());
      out.close();
      loadWorkingTree();
    } else {
      QMessageBox::warning(this, tr("Error"),
                           tr("Could not write .gitignore."));
    }
  }
}

void MainWindow::editGitattributes() {
  if (m_currentPath.isEmpty()) {
    QMessageBox::warning(this, tr("No repository"),
                         tr("Open a repository first."));
    return;
  }

  const QString gitattributesPath =
      m_currentPath + QStringLiteral("/.gitattributes");

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Edit .gitattributes"));
  dlg.setMinimumSize(500, 400);
  dlg.resize(700, 500);
  auto *layout = new QVBoxLayout(&dlg);
  auto *edit = new QTextEdit(&dlg);
  edit->setFont(QFont(QStringLiteral("monospace"), 10));
  layout->addWidget(edit);

  QString content;
  QFile file(gitattributesPath);
  if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    content = QString::fromUtf8(file.readAll());
    file.close();
  }
  edit->setPlainText(content);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() == QDialog::Accepted) {
    QFile out(gitattributesPath);
    if (out.open(QIODevice::WriteOnly | QIODevice::Text)) {
      out.write(edit->toPlainText().toUtf8());
      out.close();
      loadWorkingTree();
    } else {
      QMessageBox::warning(this, tr("Error"),
                           tr("Could not write .gitattributes."));
    }
  }
}

void MainWindow::showCommitHooksAndTemplates() {
  if (m_currentPath.isEmpty()) {
    QMessageBox::warning(this, tr("No repository"),
                         tr("Open a repository first."));
    return;
  }

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Commit hooks and templates"));
  dlg.setMinimumSize(700, 500);
  dlg.resize(800, 600);
  auto *mainLayout = new QVBoxLayout(&dlg);
  auto *tabs = new QTabWidget(&dlg);
  mainLayout->addWidget(tabs);

  auto *templateTab = new QWidget(tabs);
  auto *templateLayout = new QVBoxLayout(templateTab);
  auto *templatePathLayout = new QHBoxLayout();
  auto *templatePath = new QLineEdit(templateTab);
  auto *browseButton = new QPushButton(tr("Browse..."), templateTab);
  templatePathLayout->addWidget(templatePath);
  templatePathLayout->addWidget(browseButton);
  templateLayout->addLayout(templatePathLayout);
  auto *templateEdit = new QTextEdit(templateTab);
  templateEdit->setFont(QFont(QStringLiteral("monospace"), 10));
  templateLayout->addWidget(templateEdit);
  tabs->addTab(templateTab, tr("Commit template"));

  auto *hooksTab = new QWidget(tabs);
  auto *hooksLayout = new QHBoxLayout(hooksTab);
  auto *hooksList = new QListWidget(hooksTab);
  auto *hookEdit = new QTextEdit(hooksTab);
  hookEdit->setFont(QFont(QStringLiteral("monospace"), 10));
  auto *saveHookButton = new QPushButton(tr("Save hook"), hooksTab);
  auto *hookEditLayout = new QVBoxLayout();
  hookEditLayout->addWidget(hookEdit);
  hookEditLayout->addWidget(saveHookButton);
  hooksLayout->addWidget(hooksList, 1);
  hooksLayout->addLayout(hookEditLayout, 3);
  tabs->addTab(hooksTab, tr("Hooks"));

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
  mainLayout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  auto loadTemplate = [&](const QString &path) {
    templatePath->setText(path);
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
      templateEdit->setPlainText(QString::fromUtf8(f.readAll()));
    else
      templateEdit->clear();
  };

  const QString configuredTemplate =
      m_gitExecutor->run(m_currentPath, {"config", "commit.template"}).value(0);
  if (!configuredTemplate.isEmpty())
    loadTemplate(configuredTemplate);
  else
    loadTemplate(m_currentPath + QStringLiteral("/.gitmessage"));

  const QString hooksPath = m_currentPath + QStringLiteral("/.git/hooks");
  QDir hooksDir(hooksPath);
  for (const QString &entry :
       hooksDir.entryList(QDir::Files | QDir::NoDotAndDotDot))
    hooksList->addItem(entry);

  auto loadHook = [&]() {
    const QString fileName =
        hooksList->currentItem() ? hooksList->currentItem()->text() : QString();
    if (fileName.isEmpty()) {
      hookEdit->clear();
      return;
    }
    QFile f(hooksDir.absoluteFilePath(fileName));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
      hookEdit->setPlainText(QString::fromUtf8(f.readAll()));
    else
      hookEdit->clear();
  };

  connect(hooksList, &QListWidget::currentTextChanged, this,
          [&](const QString &) { loadHook(); });

  connect(browseButton, &QPushButton::clicked, this, [&]() {
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Select commit template"), m_currentPath);
    if (!file.isEmpty())
      loadTemplate(file);
  });

  connect(saveHookButton, &QPushButton::clicked, this, [&]() {
    const QString fileName =
        hooksList->currentItem() ? hooksList->currentItem()->text() : QString();
    if (fileName.isEmpty())
      return;
    QFile f(hooksDir.absoluteFilePath(fileName));
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
      f.write(hookEdit->toPlainText().toUtf8());
      f.close();
      statusBar()->showMessage(tr("Hook %1 saved").arg(fileName));
    } else {
      QMessageBox::warning(this, tr("Error"),
                           tr("Could not write hook %1.").arg(fileName));
    }
  });

  if (dlg.exec() == QDialog::Accepted) {
    const QString templatePathText = templatePath->text().trimmed();
    if (templatePathText.isEmpty()) {
      m_gitExecutor->exec(m_currentPath,
                          {"config", "--unset", "commit.template"});
      return;
    }

    QFile f(templatePathText);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
      f.write(templateEdit->toPlainText().toUtf8());
      f.close();
      m_gitExecutor->exec(m_currentPath,
                          {"config", "commit.template", templatePathText});
    } else {
      QMessageBox::warning(this, tr("Error"),
                           tr("Could not write commit template."));
    }
  }
}

void MainWindow::showSubmodulesContextMenu(const QPoint &pos) {
  QTreeWidgetItem *item = m_repoPanel->itemAt(pos);
  if (!item || !m_submodulesItem)
    return;

  QMenu menu(this);
  QAction *selected = nullptr;

  if (item == m_submodulesItem) {
    auto *addAction = menu.addAction(tr("&Add..."));
    if (m_submodulesItem->childCount() > 0) {
      auto *initAllAction = menu.addAction(tr("&Init all"));
      auto *updateAllAction = menu.addAction(tr("&Update all"));
      auto *syncAllAction = menu.addAction(tr("&Sync all"));
      selected = menu.exec(m_repoPanel->viewport()->mapToGlobal(pos));
      if (!selected)
        return;

      if (selected == initAllAction) {
        initSubmodules();
      } else if (selected == updateAllAction) {
        updateSubmodules();
      } else if (selected == syncAllAction) {
        syncSubmodules();
      } else if (selected == addAction) {
        addSubmodule();
      }
    } else {
      selected = menu.exec(m_repoPanel->viewport()->mapToGlobal(pos));
      if (selected == addAction)
        addSubmodule();
    }
    return;
  }

  if (item->parent() != m_submodulesItem)
    return;

  const QString subPath = item->data(0, Qt::UserRole).toString();
  if (subPath.isEmpty())
    return;

  auto *openAction = menu.addAction(tr("&Open"));
  auto *initAction = menu.addAction(tr("&Init"));
  auto *updateAction = menu.addAction(tr("&Update"));
  auto *syncAction = menu.addAction(tr("&Sync"));
  auto *removeAction = menu.addAction(tr("&Remove"));
  selected = menu.exec(m_repoPanel->viewport()->mapToGlobal(pos));
  if (!selected)
    return;

  if (selected == openAction) {
    loadRepository(m_currentPath + QLatin1Char('/') + subPath);
  } else if (selected == syncAction) {
    if (m_gitExecutor->exec(m_currentPath, {"submodule", "sync", subPath})) {
      loadRepository(m_currentPath);
      statusBar()->showMessage(tr("Submodule %1 synced").arg(subPath));
    } else {
      statusBar()->showMessage(tr("Failed to sync %1").arg(subPath));
    }
  } else if (selected == initAction || selected == updateAction) {
    if (m_gitExecutor->exec(m_currentPath,
                            {"submodule", "update", "--init", subPath})) {
      loadRepository(m_currentPath);
      statusBar()->showMessage(tr("Submodule %1 updated").arg(subPath));
    } else {
      statusBar()->showMessage(tr("Failed to update %1").arg(subPath));
    }
  } else if (selected == removeAction) {
    if (QMessageBox::question(this, tr("Remove Submodule"),
                              tr("Remove submodule %1?").arg(subPath),
                              QMessageBox::Yes | QMessageBox::No) ==
        QMessageBox::Yes) {
      bool ok = m_gitExecutor->exec(m_currentPath,
                                    {"submodule", "deinit", "-f", subPath});
      ok &= m_gitExecutor->exec(m_currentPath, {"rm", "-f", subPath});
      QDir(m_currentPath + QLatin1String("/.git/modules/") + subPath)
          .removeRecursively();
      if (ok) {
        loadRepository(m_currentPath);
        statusBar()->showMessage(tr("Submodule %1 removed").arg(subPath));
      } else {
        statusBar()->showMessage(tr("Failed to remove %1").arg(subPath));
      }
    }
  }
}

void MainWindow::initSubmodules() {
  if (m_currentPath.isEmpty()) {
    QMessageBox::warning(this, tr("No repository"),
                         tr("Open a repository first."));
    return;
  }

  if (m_gitExecutor->exec(m_currentPath,
                          {"submodule", "update", "--init", "--recursive"})) {
    loadWorkingTree();
    statusBar()->showMessage(tr("Submodules initialized"));
  } else {
    statusBar()->showMessage(tr("Failed to initialize submodules"));
  }
}

void MainWindow::updateSubmodules() {
  if (m_currentPath.isEmpty()) {
    QMessageBox::warning(this, tr("No repository"),
                         tr("Open a repository first."));
    return;
  }

  if (m_gitExecutor->exec(m_currentPath,
                          {"submodule", "update", "--recursive"})) {
    loadWorkingTree();
    statusBar()->showMessage(tr("Submodules updated"));
  } else {
    statusBar()->showMessage(tr("Failed to update submodules"));
  }
}

void MainWindow::syncSubmodules() {
  if (m_currentPath.isEmpty()) {
    QMessageBox::warning(this, tr("No repository"),
                         tr("Open a repository first."));
    return;
  }

  if (m_gitExecutor->exec(m_currentPath,
                          {"submodule", "sync", "--recursive"})) {
    loadWorkingTree();
    statusBar()->showMessage(tr("Submodules synced"));
  } else {
    statusBar()->showMessage(tr("Failed to sync submodules"));
  }
}

void MainWindow::addSubmodule() {
  if (m_currentPath.isEmpty()) {
    QMessageBox::warning(this, tr("No repository"),
                         tr("Open a repository first."));
    return;
  }

  bool okUrl;
  const QString url =
      QInputDialog::getText(this, tr("Add Submodule"), tr("Repository URL:"),
                            QLineEdit::Normal, QString(), &okUrl);
  if (!okUrl || url.isEmpty())
    return;

  bool okPath;
  const QString path =
      QInputDialog::getText(this, tr("Add Submodule"), tr("Local path:"),
                            QLineEdit::Normal, QString(), &okPath);
  if (!okPath || path.isEmpty())
    return;

  if (m_gitExecutor->exec(m_currentPath, {"submodule", "add", url, path})) {
    loadWorkingTree();
    statusBar()->showMessage(tr("Submodule added"));
  } else {
    statusBar()->showMessage(tr("Failed to add submodule"));
  }
}

void MainWindow::openSubmodule() {
  if (m_currentPath.isEmpty()) {
    QMessageBox::warning(this, tr("No repository"),
                         tr("Open a repository first."));
    return;
  }

  const QStringList configLines = m_gitExecutor->run(
      m_currentPath, {"config", "--file", QStringLiteral(".gitmodules"),
                      "--get-regexp", "^submodule\\..*\\.path$"});
  if (configLines.isEmpty()) {
    QMessageBox::warning(this, tr("No submodules"),
                         tr("This repository has no submodules."));
    return;
  }

  QStringList paths;
  for (const QString &line : configLines) {
    const int sep = line.indexOf(QLatin1Char(' '));
    if (sep > 0)
      paths.append(line.mid(sep + 1));
  }

  bool ok;
  const QString path = QInputDialog::getItem(
      this, tr("Open Submodule"), tr("Submodule:"), paths, 0, false, &ok);
  if (!ok || path.isEmpty())
    return;

  loadRepository(m_currentPath + QLatin1Char('/') + path);
}

void MainWindow::showRepositorySettings() {
  if (m_currentPath.isEmpty()) {
    QMessageBox::warning(this, tr("No repository"),
                         tr("Open a repository first."));
    return;
  }

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Repository Settings"));
  auto *layout = new QVBoxLayout(&dlg);
  auto *form = new QFormLayout();

  auto *nameEdit = new QLineEdit(&dlg);
  auto *emailEdit = new QLineEdit(&dlg);
  auto *branchEdit = new QLineEdit(&dlg);
  auto *autocrlfCombo = new QComboBox(&dlg);
  autocrlfCombo->setEditable(true);
  autocrlfCombo->addItems({QString(), QStringLiteral("true"),
                           QStringLiteral("false"), QStringLiteral("input")});
  auto *diffToolEdit = new QLineEdit(&dlg);
  auto *mergeToolEdit = new QLineEdit(&dlg);
  auto *lfsUrlEdit = new QLineEdit(&dlg);
  auto *lfsLocksverifyBox =
      new QCheckBox(tr("Verify LFS locks before push"), &dlg);

  nameEdit->setText(
      m_gitExecutor->run(m_currentPath, {"config", "user.name"}).value(0));
  emailEdit->setText(
      m_gitExecutor->run(m_currentPath, {"config", "user.email"}).value(0));
  branchEdit->setText(
      m_gitExecutor->run(m_currentPath, {"config", "init.defaultBranch"})
          .value(0));
  autocrlfCombo->setCurrentText(
      m_gitExecutor->run(m_currentPath, {"config", "core.autocrlf"}).value(0));
  diffToolEdit->setText(
      m_gitExecutor->run(m_currentPath, {"config", "diff.tool"}).value(0));
  mergeToolEdit->setText(
      m_gitExecutor->run(m_currentPath, {"config", "merge.tool"}).value(0));
  lfsUrlEdit->setText(
      m_gitExecutor->run(m_currentPath, {"config", "lfs.url"}).value(0));
  const QString locksverify =
      m_gitExecutor->run(m_currentPath, {"config", "--bool", "lfs.locksverify"})
          .value(0);
  lfsLocksverifyBox->setChecked(locksverify == QLatin1String("true"));

  form->addRow(tr("User name:"), nameEdit);
  form->addRow(tr("User email:"), emailEdit);
  form->addRow(tr("Default branch:"), branchEdit);
  form->addRow(tr("Auto CRLF:"), autocrlfCombo);
  form->addRow(tr("Diff tool:"), diffToolEdit);
  form->addRow(tr("Merge tool:"), mergeToolEdit);
  form->addRow(tr("LFS URL:"), lfsUrlEdit);
  form->addRow(tr("LFS locks verify:"), lfsLocksverifyBox);
  layout->addLayout(form);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() != QDialog::Accepted)
    return;

  auto setConfig = [this](const QString &key, const QString &value) {
    if (value.isEmpty()) {
      m_gitExecutor->exec(m_currentPath, {"config", "--local", "--unset", key});
    } else {
      m_gitExecutor->exec(m_currentPath, {"config", "--local", key, value});
    }
  };

  setConfig(QStringLiteral("user.name"), nameEdit->text());
  setConfig(QStringLiteral("user.email"), emailEdit->text());
  setConfig(QStringLiteral("init.defaultBranch"), branchEdit->text());
  setConfig(QStringLiteral("core.autocrlf"), autocrlfCombo->currentText());
  setConfig(QStringLiteral("diff.tool"), diffToolEdit->text());
  setConfig(QStringLiteral("merge.tool"), mergeToolEdit->text());
  setConfig(QStringLiteral("lfs.url"), lfsUrlEdit->text().trimmed());
  setConfig(QStringLiteral("lfs.locksverify"),
            lfsLocksverifyBox->isChecked() ? "true" : "false");

  statusBar()->showMessage(tr("Repository settings saved"));
}

void MainWindow::showHunkStaging(const QString &path, bool unstage) {
  if (m_currentPath.isEmpty())
    return;

  QStringList args = QStringList{"-C", m_currentPath, "diff"};
  if (unstage)
    args.append("--cached");
  args.append("--");
  args.append(path);

  QProcess p;
  p.start("git", args);
  if (!p.waitForFinished(10000) || p.exitCode() != 0)
    return;

  const QString raw = QString::fromLocal8Bit(p.readAllStandardOutput());
  if (raw.isEmpty())
    return;

  const QStringList lines = raw.split('\n');

  QList<int> hunkStarts;
  const QRegularExpression hunkRe(
      QStringLiteral("^@@ -(\\d+)(?:,(\\d+))? \\+(\\d+)(?:,(\\d+))? @@"));
  for (int i = 0; i < lines.size(); ++i) {
    if (hunkRe.match(lines[i]).hasMatch())
      hunkStarts.append(i);
  }

  if (hunkStarts.isEmpty()) {
    if (unstage)
      m_gitExecutor->exec(m_currentPath, {"reset", "HEAD", "--", path});
    else
      m_gitExecutor->exec(m_currentPath, {"add", "--", path});
    loadWorkingTree();
    return;
  }

  const int firstHunk = hunkStarts.first();
  QStringList headerLines;
  for (int i = 0; i < firstHunk; ++i)
    headerLines.append(lines[i]);

  struct Hunk {
    int start;
    int end;
  };
  QList<Hunk> hunks;
  for (int i = 0; i < hunkStarts.size(); ++i) {
    int start = hunkStarts[i];
    int end =
        (i + 1 < hunkStarts.size()) ? hunkStarts[i + 1] - 1 : lines.size() - 1;
    hunks.append({start, end});
  }

  QDialog dlg(this);
  dlg.setWindowTitle(unstage ? tr("Unstage hunks - %1").arg(path)
                             : tr("Stage hunks - %1").arg(path));
  auto *layout = new QVBoxLayout(&dlg);

  auto *list = new QListWidget(&dlg);
  list->setStyleSheet(QStringLiteral(
      "QListWidget { background-color: #1e1e1e; color: #d4d4d4; "
      "font-family: monospace; }"
      "QListWidget::item { padding: 4px; }"
      "QListWidget::item:selected { background-color: #3c3c3c; }"));
  list->setMinimumHeight(80);

  auto *preview = new QTextEdit(&dlg);
  preview->setReadOnly(true);
  preview->setFont(QFont(QStringLiteral("monospace"), 10));

  for (const Hunk &h : hunks) {
    QStringList hunkLines;
    for (int i = h.start; i <= h.end; ++i)
      hunkLines.append(lines[i]);
    auto *wi = new QListWidgetItem(lines[h.start], list);
    wi->setFlags(wi->flags() | Qt::ItemIsUserCheckable);
    wi->setCheckState(Qt::Checked);
    wi->setData(Qt::UserRole, hunkLines.join(QLatin1Char('\n')));
  }

  auto updatePreview = [this, list, preview]() {
    auto *wi = list->currentItem();
    if (!wi)
      return;
    preview->setHtml(m_diffPresenter->formatDiff(
        wi->data(Qt::UserRole).toString().split(QLatin1Char('\n'))));
  };
  connect(list, &QListWidget::currentItemChanged, this, updatePreview);
  list->setCurrentRow(0);
  if (!hunks.isEmpty()) {
    preview->setHtml(m_diffPresenter->formatDiff(
        lines.mid(hunks[0].start, hunks[0].end - hunks[0].start + 1)));
  }

  auto *splitter = new QSplitter(Qt::Vertical, &dlg);
  splitter->addWidget(list);
  splitter->addWidget(preview);
  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 1);
  layout->addWidget(splitter);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Apply | QDialogButtonBox::Cancel, &dlg);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() != QDialog::Accepted)
    return;

  QStringList patchLines = headerLines;
  for (int i = 0; i < list->count(); ++i) {
    QListWidgetItem *wi = list->item(i);
    if (wi->checkState() == Qt::Checked)
      patchLines.append(
          wi->data(Qt::UserRole).toString().split(QLatin1Char('\n')));
  }

  if (patchLines.size() == headerLines.size()) {
    QMessageBox::information(this, tr("No hunks selected"),
                             tr("Select at least one hunk to apply."));
    return;
  }

  const QString patch = patchLines.join(QLatin1Char('\n')) + QLatin1Char('\n');

  QTemporaryFile tempFile;
  if (!tempFile.open()) {
    QMessageBox::warning(this, tr("Error"), tr("Could not create patch file."));
    return;
  }
  tempFile.write(patch.toUtf8());
  tempFile.close();

  const QStringList applyArgs =
      unstage ? QStringList{"apply", "--cached", "-R", tempFile.fileName()}
              : QStringList{"apply", "--cached", tempFile.fileName()};
  if (m_gitExecutor->exec(m_currentPath, applyArgs)) {
    loadWorkingTree();
    statusBar()->showMessage(unstage ? tr("Hunks unstaged")
                                     : tr("Hunks staged"));
  } else {
    statusBar()->showMessage(unstage ? tr("Failed to unstage hunks")
                                     : tr("Failed to stage hunks"));
  }
}

void MainWindow::showInteractiveRebase(const QString &baseSha) {
  if (m_currentPath.isEmpty() || baseSha.isEmpty())
    return;

  const QStringList logLines = m_gitExecutor->run(
      m_currentPath, {"log", baseSha + QLatin1String("..HEAD"), "--reverse",
                      "--pretty=format:%H %s"});
  if (logLines.isEmpty()) {
    QMessageBox::information(
        this, tr("Nothing to rebase"),
        tr("There are no commits after the selected one."));
    return;
  }

  struct Commit {
    QString sha;
    QString message;
  };
  QList<Commit> commits;
  for (const QString &line : logLines) {
    const int sp = line.indexOf(QLatin1Char(' '));
    if (sp < 0)
      continue;
    commits.append({line.left(sp), line.mid(sp + 1)});
  }
  if (commits.isEmpty())
    return;

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Interactive rebase"));
  auto *layout = new QVBoxLayout(&dlg);
  auto *table = new QTableWidget(commits.size(), 3, &dlg);
  table->setHorizontalHeaderLabels({tr("Action"), tr("Commit"), tr("Message")});
  table->setColumnWidth(0, 100);
  table->setColumnWidth(1, 260);
  table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
  table->verticalHeader()->setVisible(false);

  const QStringList actions = {QStringLiteral("pick"), QStringLiteral("reword"),
                               QStringLiteral("squash"),
                               QStringLiteral("fixup"), QStringLiteral("drop")};
  for (int i = 0; i < commits.size(); ++i) {
    auto *combo = new QComboBox(&dlg);
    for (const QString &a : actions)
      combo->addItem(tr(a.toLatin1().constData()));
    table->setCellWidget(i, 0, combo);
    auto *shaItem =
        new QTableWidgetItem(commits[i].sha.left(7) + QLatin1Char(' ') +
                             commits[i].message.left(40));
    shaItem->setFlags(shaItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(i, 1, shaItem);
    auto *msgItem = new QTableWidgetItem(commits[i].message);
    table->setItem(i, 2, msgItem);
  }
  layout->addWidget(table);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() != QDialog::Accepted)
    return;

  QTemporaryDir tempDir;
  if (!tempDir.isValid())
    return;

  int messageCount = 0;
  QStringList todoLines;
  for (int i = 0; i < commits.size(); ++i) {
    auto *combo = qobject_cast<QComboBox *>(table->cellWidget(i, 0));
    const QString action = combo ? combo->currentText() : tr("pick");
    const QString sha = commits[i].sha;
    const QString newMsg =
        table->item(i, 2) ? table->item(i, 2)->text() : commits[i].message;

    if (action == tr("reword") || action == tr("squash")) {
      QFile f(tempDir.filePath(QStringLiteral("msg_%1.txt").arg(messageCount)));
      if (f.open(QIODevice::WriteOnly | QIODevice::Text))
        f.write(newMsg.toUtf8());
      ++messageCount;
    }
    todoLines.append(action + QLatin1Char(' ') + sha + QLatin1Char(' ') +
                     (action == tr("drop") ? QString() : newMsg));
  }

  const QString todoFile = tempDir.filePath(QStringLiteral("todo.txt"));
  {
    QFile f(todoFile);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
      return;
    f.write(todoLines.join(QLatin1Char('\n')).toUtf8());
    f.write("\n", 1);
  }

  const QString seqEditor = tempDir.filePath(QStringLiteral("seq-editor.sh"));
  {
    QFile f(seqEditor);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
      f.write(QStringLiteral("#!/bin/sh\ncp \"%1\" \"$1\"\n")
                  .arg(todoFile)
                  .toUtf8());
      f.close();
      f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }
  }

  const QString msgCounter = tempDir.filePath(QStringLiteral("counter.txt"));
  if (messageCount > 0) {
    QFile c(msgCounter);
    if (c.open(QIODevice::WriteOnly | QIODevice::Text))
      c.write("0");
  }

  const QString msgEditor = tempDir.filePath(QStringLiteral("msg-editor.sh"));
  if (messageCount > 0) {
    QFile f(msgEditor);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
      f.write(QStringLiteral("#!/bin/sh\n"
                             "COUNTER=\"%1\"\n"
                             "DIR=\"%2\"\n"
                             "IDX=$(cat \"$COUNTER\")\n"
                             "echo $((IDX+1)) > \"$COUNTER\"\n"
                             "cp \"$DIR/msg_${IDX}.txt\" \"$1\"\n")
                  .arg(msgCounter, tempDir.path())
                  .toUtf8());
      f.close();
      f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }
  }

  QProcess p;
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert(QStringLiteral("GIT_SEQUENCE_EDITOR"), seqEditor);
  if (messageCount > 0)
    env.insert(QStringLiteral("GIT_EDITOR"), msgEditor);
  p.setProcessEnvironment(env);
  p.start(QStringLiteral("git"),
          QStringList{QStringLiteral("-C"), m_currentPath,
                      QStringLiteral("rebase"), QStringLiteral("-i"), baseSha});
  if (!p.waitForStarted(5000) || !p.waitForFinished(120000)) {
    p.kill();
    p.waitForFinished(1000);
    statusBar()->showMessage(
        tr("Interactive rebase timed out or failed to start"));
    return;
  }

  const int code = p.exitCode();
  const QString output = QString::fromLocal8Bit(p.readAllStandardOutput() +
                                                p.readAllStandardError());
  if (code == 0) {
    loadRepository(m_currentPath);
    statusBar()->showMessage(tr("Interactive rebase completed"));
  } else {
    QMessageBox::warning(this, tr("Interactive rebase failed"), output);
  }
}

void MainWindow::showConflictResolver(const QString &operation) {
  if (m_currentPath.isEmpty())
    return;

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Resolve conflicts"));
  auto *layout = new QVBoxLayout(&dlg);

  layout->addWidget(new QLabel(tr("Conflicted files:"), &dlg));
  auto *list = new QListWidget(&dlg);
  list->setSelectionMode(QAbstractItemView::SingleSelection);
  layout->addWidget(list);

  auto *btnLayout = new QHBoxLayout;
  auto *oursBtn = new QPushButton(tr("Use ours"), &dlg);
  auto *theirsBtn = new QPushButton(tr("Use theirs"), &dlg);
  auto *resolvedBtn = new QPushButton(tr("Mark resolved"), &dlg);
  auto *toolBtn = new QPushButton(tr("Open in merge tool"), &dlg);
  auto *continueBtn = new QPushButton(tr("Continue"), &dlg);
  auto *abortBtn = new QPushButton(tr("Abort"), &dlg);
  btnLayout->addWidget(oursBtn);
  btnLayout->addWidget(theirsBtn);
  btnLayout->addWidget(resolvedBtn);
  btnLayout->addWidget(toolBtn);
  btnLayout->addStretch();
  btnLayout->addWidget(continueBtn);
  btnLayout->addWidget(abortBtn);
  layout->addLayout(btnLayout);

  continueBtn->setVisible(!operation.isEmpty());
  abortBtn->setVisible(!operation.isEmpty());

  auto refresh = [&]() {
    list->clear();
    for (const QString &file : m_gitExecutor->run(
             m_currentPath, {"diff", "--name-only", "--diff-filter=U"}))
      list->addItem(file);
    const bool hasSelection = list->currentItem() != nullptr;
    const bool hasMergeTool =
        !m_gitExecutor->run(m_currentPath, {"config", "merge.tool"}).isEmpty();
    oursBtn->setEnabled(hasSelection);
    theirsBtn->setEnabled(hasSelection);
    resolvedBtn->setEnabled(hasSelection);
    toolBtn->setEnabled(hasSelection && hasMergeTool);
    continueBtn->setEnabled(!operation.isEmpty());
    abortBtn->setEnabled(!operation.isEmpty());
  };

  refresh();
  connect(list, &QListWidget::itemSelectionChanged, refresh);

  connect(oursBtn, &QPushButton::clicked, this, [&]() {
    auto *item = list->currentItem();
    if (!item)
      return;
    const QString file = item->text();
    m_gitExecutor->exec(m_currentPath, {"checkout", "--ours", "--", file});
    m_gitExecutor->exec(m_currentPath, {"add", "--", file});
    refresh();
  });

  connect(theirsBtn, &QPushButton::clicked, this, [&]() {
    auto *item = list->currentItem();
    if (!item)
      return;
    const QString file = item->text();
    m_gitExecutor->exec(m_currentPath, {"checkout", "--theirs", "--", file});
    m_gitExecutor->exec(m_currentPath, {"add", "--", file});
    refresh();
  });

  connect(resolvedBtn, &QPushButton::clicked, this, [&]() {
    auto *item = list->currentItem();
    if (!item)
      return;
    const QString file = item->text();
    m_gitExecutor->exec(m_currentPath, {"add", "--", file});
    refresh();
  });

  connect(toolBtn, &QPushButton::clicked, this, [&]() {
    auto *item = list->currentItem();
    if (!item)
      return;
    const QString file = item->text();
    launchGitTool({QStringLiteral("mergetool"), QStringLiteral("-y"),
                   QStringLiteral("--"), file},
                  false);
  });

  connect(continueBtn, &QPushButton::clicked, this, [&]() {
    QProcess p;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("GIT_EDITOR"), QStringLiteral("true"));
    p.setProcessEnvironment(env);
    p.start(QStringLiteral("git"),
            QStringList{QStringLiteral("-C"), m_currentPath, operation,
                        QStringLiteral("--continue")});
    if (p.waitForStarted(5000) && p.waitForFinished(120000) &&
        p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0) {
      loadRepository(m_currentPath);
      dlg.accept();
      statusBar()->showMessage(tr("%1 completed").arg(operation));
    } else {
      QMessageBox::warning(&dlg, tr("Continue failed"),
                           QString::fromLocal8Bit(p.readAllStandardError()));
      refresh();
    }
  });

  connect(abortBtn, &QPushButton::clicked, this, [&]() {
    if (m_gitExecutor->exec(m_currentPath,
                            {operation, QStringLiteral("--abort")})) {
      loadRepository(m_currentPath);
      dlg.accept();
      statusBar()->showMessage(tr("%1 aborted").arg(operation));
    }
  });

  dlg.resize(600, 400);
  dlg.exec();
}

void MainWindow::cherryPickCommit(const QString &sha) {
  if (m_currentPath.isEmpty() || sha.isEmpty())
    return;

  QString output;
  if (m_gitExecutor->exec(m_currentPath, {"cherry-pick", sha}, &output)) {
    loadRepository(m_currentPath);
    statusBar()->showMessage(tr("Cherry-picked %1").arg(sha.left(7)));
    return;
  }

  const QStringList conflicted = m_gitExecutor->run(
      m_currentPath, {"diff", "--name-only", "--diff-filter=U"});
  if (conflicted.isEmpty()) {
    QMessageBox::warning(this, tr("Cherry-pick failed"), output);
    return;
  }

  showConflictResolver(QStringLiteral("cherry-pick"));
}

void MainWindow::revertCommit(const QString &sha) {
  if (m_currentPath.isEmpty() || sha.isEmpty())
    return;

  QString output;
  if (m_gitExecutor->exec(m_currentPath, {"revert", sha}, &output)) {
    loadRepository(m_currentPath);
    statusBar()->showMessage(tr("Reverted %1").arg(sha.left(7)));
    return;
  }

  const QStringList conflicted = m_gitExecutor->run(
      m_currentPath, {"diff", "--name-only", "--diff-filter=U"});
  if (conflicted.isEmpty()) {
    QMessageBox::warning(this, tr("Revert failed"), output);
    return;
  }

  showConflictResolver(QStringLiteral("revert"));
}

void MainWindow::startBisect() {
  if (m_currentPath.isEmpty())
    return;

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Start bisect"));
  auto *layout = new QFormLayout(&dlg);
  auto *badEdit = new QLineEdit(&dlg);
  badEdit->setText(QStringLiteral("HEAD"));
  auto *goodEdit = new QLineEdit(&dlg);
  layout->addRow(tr("Bad commit:"), badEdit);
  layout->addRow(tr("Good commit:"), goodEdit);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  layout->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() != QDialog::Accepted)
    return;

  const QString bad = badEdit->text().trimmed();
  const QString good = goodEdit->text().trimmed();
  if (bad.isEmpty() || good.isEmpty()) {
    QMessageBox::warning(this, tr("Bisect"),
                         tr("Both bad and good commits are required."));
    return;
  }

  if (m_gitExecutor->exec(m_currentPath, {"bisect", "start", bad, good})) {
    loadRepository(m_currentPath);
    statusBar()->showMessage(tr("Bisect started"));
  }
}

void MainWindow::bisectGood() {
  if (m_currentPath.isEmpty())
    return;
  if (m_gitExecutor->exec(m_currentPath, {"bisect", "good"})) {
    loadRepository(m_currentPath);
    statusBar()->showMessage(tr("Marked as good"));
  }
}

void MainWindow::bisectBad() {
  if (m_currentPath.isEmpty())
    return;
  if (m_gitExecutor->exec(m_currentPath, {"bisect", "bad"})) {
    loadRepository(m_currentPath);
    statusBar()->showMessage(tr("Marked as bad"));
  }
}

void MainWindow::bisectSkip() {
  if (m_currentPath.isEmpty())
    return;
  if (m_gitExecutor->exec(m_currentPath, {"bisect", "skip"})) {
    loadRepository(m_currentPath);
    statusBar()->showMessage(tr("Skipped"));
  }
}

void MainWindow::bisectReset() {
  if (m_currentPath.isEmpty())
    return;
  if (m_gitExecutor->exec(m_currentPath, {"bisect", "reset"})) {
    loadRepository(m_currentPath);
    statusBar()->showMessage(tr("Bisect reset"));
  }
}

void MainWindow::lfsTrack() {
  if (m_currentPath.isEmpty())
    return;

  bool ok;
  const QString pattern = QInputDialog::getText(
      this, tr("Track pattern"), tr("File pattern to track with LFS:"),
      QLineEdit::Normal, QStringLiteral("*"), &ok);
  if (!ok || pattern.isEmpty())
    return;

  if (m_gitExecutor->exec(m_currentPath, {"lfs", "track", pattern})) {
    loadWorkingTree();
    statusBar()->showMessage(tr("LFS tracking %1").arg(pattern));
  } else {
    statusBar()->showMessage(tr("Failed to track %1").arg(pattern));
  }
}

void MainWindow::lfsUntrack() {
  if (m_currentPath.isEmpty())
    return;

  bool ok;
  const QString pattern = QInputDialog::getText(
      this, tr("Untrack pattern"), tr("File pattern to untrack from LFS:"),
      QLineEdit::Normal, QString(), &ok);
  if (!ok || pattern.isEmpty())
    return;

  if (m_gitExecutor->exec(m_currentPath, {"lfs", "untrack", pattern})) {
    loadWorkingTree();
    statusBar()->showMessage(tr("LFS untracking %1").arg(pattern));
  } else {
    statusBar()->showMessage(tr("Failed to untrack %1").arg(pattern));
  }
}

void MainWindow::lfsPull() {
  if (m_currentPath.isEmpty())
    return;
  if (m_gitExecutor->exec(m_currentPath, {"lfs", "pull"})) {
    loadWorkingTree();
    statusBar()->showMessage(tr("LFS objects pulled"));
  } else {
    statusBar()->showMessage(tr("Failed to pull LFS objects"));
  }
}

void MainWindow::lfsPush() {
  if (m_currentPath.isEmpty())
    return;

  const QString currentBranch =
      m_gitExecutor->run(m_currentPath, {"rev-parse", "--abbrev-ref", "HEAD"})
          .value(0);
  const QString remote =
      m_gitExecutor
          ->run(
              m_currentPath,
              {"config", QStringLiteral("branch.%1.remote").arg(currentBranch)})
          .value(0);
  if (currentBranch.isEmpty() || remote.isEmpty()) {
    QMessageBox::warning(this, tr("LFS push"),
                         tr("No upstream configured for the current branch."));
    return;
  }

  if (m_gitExecutor->exec(m_currentPath,
                          {"lfs", "push", remote, currentBranch})) {
    statusBar()->showMessage(tr("LFS objects pushed"));
  } else {
    statusBar()->showMessage(tr("Failed to push LFS objects"));
  }
}

void MainWindow::showReflog() {
  if (m_currentPath.isEmpty()) {
    QMessageBox::warning(this, tr("No repository"),
                         tr("Open a repository first."));
    return;
  }

  const QStringList raw =
      m_gitExecutor->run(m_currentPath, {QStringLiteral("reflog")});
  if (raw.isEmpty()) {
    QMessageBox::information(this, tr("No reflog"), tr("Reflog is empty."));
    return;
  }

  struct ReflogEntry {
    QString sha;
    QString ref;
    QString message;
  };
  QList<ReflogEntry> entries;
  for (const QString &line : raw) {
    const int firstSpace = line.indexOf(QLatin1Char(' '));
    if (firstSpace < 0)
      continue;
    const QString sha = line.left(firstSpace);
    const int colon = line.indexOf(QLatin1Char(':'), firstSpace);
    if (colon < 0)
      continue;
    const QString ref =
        line.mid(firstSpace + 1, colon - firstSpace - 1).trimmed();
    const QString message = line.mid(colon + 1).trimmed();
    entries.append({sha, ref, message});
  }

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Reflog"));
  auto *layout = new QVBoxLayout(&dlg);
  auto *table = new QTableWidget(entries.size(), 4, &dlg);
  table->setHorizontalHeaderLabels(
      {tr("SHA"), tr("Ref"), tr("Message"), tr("Action")});
  table->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  table->horizontalHeader()->setStretchLastSection(false);
  table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  table->setMinimumSize(100, 100);
  table->verticalHeader()->setVisible(false);

  for (int i = 0; i < entries.size(); ++i) {
    auto *shaItem = new QTableWidgetItem(entries[i].sha.left(7));
    shaItem->setData(Qt::UserRole, entries[i].sha);
    shaItem->setFlags(shaItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(i, 0, shaItem);
    auto *refItem = new QTableWidgetItem(entries[i].ref);
    refItem->setFlags(refItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(i, 1, refItem);
    auto *msgItem = new QTableWidgetItem(entries[i].message);
    msgItem->setFlags(msgItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(i, 2, msgItem);
    auto *actionItem =
        new QTableWidgetItem(entries[i].message.split(' ').value(0));
    actionItem->setFlags(actionItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(i, 3, actionItem);
  }
  layout->addWidget(table);

  table->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(
      table, &QTableWidget::customContextMenuRequested, this,
      [this, table, &dlg](const QPoint &pos) {
        auto *item = table->itemAt(pos);
        if (!item)
          return;
        const int row = item->row();
        const QString sha = table->item(row, 0)->data(Qt::UserRole).toString();
        QMenu menu(&dlg);
        auto *checkoutAction =
            menu.addAction(tr("Checkout %1").arg(sha.left(7)));
        auto *resetAction = menu.addAction(tr("Reset to %1").arg(sha.left(7)));
        auto *branchAction =
            menu.addAction(tr("Create branch from %1").arg(sha.left(7)));
        QAction *selected = menu.exec(table->mapToGlobal(pos));
        if (!selected)
          return;
        if (selected == checkoutAction) {
          if (m_gitExecutor->exec(m_currentPath, {"checkout", sha}))
            loadRepository(m_currentPath);
        } else if (selected == resetAction) {
          if (QMessageBox::warning(
                  this, tr("Reset"),
                  tr("Reset the current branch to %1?").arg(sha.left(7)),
                  QMessageBox::Yes | QMessageBox::No,
                  QMessageBox::No) == QMessageBox::Yes) {
            if (m_gitExecutor->exec(m_currentPath, {"reset", "--hard", sha}))
              loadRepository(m_currentPath);
          }
        } else if (selected == branchAction) {
          bool ok;
          const QString name = QInputDialog::getText(
              this, tr("Create Branch"), tr("Branch name:"), QLineEdit::Normal,
              QString(), &ok);
          if (ok && !name.isEmpty()) {
            if (m_gitExecutor->exec(m_currentPath,
                                    {"checkout", "-b", name, sha}))
              loadRepository(m_currentPath);
          }
        }
      });

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  dlg.setFixedSize(900, 500);
  dlg.exec();
}

void MainWindow::applyPatch() {
  if (m_currentPath.isEmpty()) {
    QMessageBox::warning(this, tr("No repository"),
                         tr("Open a repository first."));
    return;
  }

  const QString fileName =
      QFileDialog::getOpenFileName(this, tr("Apply patch"), QDir::homePath(),
                                   tr("Patches (*.patch *.diff);;"
                                      "All files (*.*)"));
  if (fileName.isEmpty())
    return;

  if (m_gitExecutor->exec(m_currentPath, {"apply", fileName})) {
    loadRepository(m_currentPath);
    statusBar()->showMessage(tr("Patch applied"));
  } else {
    statusBar()->showMessage(tr("Failed to apply patch"));
  }
}

void MainWindow::createPatchFromCommit(const QString &sha) {
  if (m_currentPath.isEmpty() || sha.isEmpty())
    return;

  const QString defaultName = QDir::homePath() + QStringLiteral("/") +
                              sha.left(7) + QStringLiteral(".patch");
  const QString fileName = QFileDialog::getSaveFileName(
      this, tr("Save patch"), defaultName, tr("Patch files (*.patch)"));
  if (fileName.isEmpty())
    return;

  const QStringList patchLines = m_gitExecutor->run(
      m_currentPath, {"format-patch", "-1", sha, "--stdout"});
  if (patchLines.isEmpty()) {
    statusBar()->showMessage(tr("Failed to create patch"));
    return;
  }

  QFile out(fileName);
  if (out.open(QIODevice::WriteOnly | QIODevice::Text)) {
    out.write(patchLines.join(QLatin1Char('\n')).toUtf8());
    out.write("\n");
    out.close();
    statusBar()->showMessage(tr("Patch saved to %1").arg(fileName));
  } else {
    QMessageBox::warning(this, tr("Error"), tr("Could not write patch file."));
  }
}
