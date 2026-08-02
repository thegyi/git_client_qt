#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QDebug>

#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeySequence>
#include <QLineEdit>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QShortcut>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <QDateTime>
#include <QDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QTemporaryFile>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);
  setStyleSheet("QMainWindow::separator { background: #808080; width: 4px; }"
                "QMenu::item { padding: 6px 24px 6px 12px; }");

  m_recentMenu = new QMenu(tr("Recent Repositories"), this);
  ui->menuFile->insertMenu(ui->actionClose, m_recentMenu);

  auto *searchMenu = new QMenu(tr("Search"), this);
  menuBar()->addMenu(searchMenu);
  auto *grepAction = searchMenu->addAction(tr("Grep"));
  grepAction->setShortcut(QKeySequence(QLatin1String("Ctrl+Shift+G")));
  connect(grepAction, &QAction::triggered, this, &MainWindow::onGrepRequested);

  ui->actionOpen->setShortcut(QKeySequence::Open);
  ui->actionClose->setShortcut(QKeySequence::Close);
  ui->actionExit->setShortcut(QKeySequence::Quit);
  ui->actionPreferences->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));

  m_branchLabel = new QLabel(this);
  statusBar()->addPermanentWidget(m_branchLabel);

  auto *actionClone = new QAction(tr("Clone Repository"), this);
  ui->menuFile->insertAction(ui->actionOpen, actionClone);
  actionClone->setShortcut(QKeySequence(QLatin1String("Ctrl+Shift+N")));
  connect(actionClone, &QAction::triggered, this,
          &MainWindow::onCloneRepository);

  auto *actionInit = new QAction(tr("Initialize Repository"), this);
  ui->menuFile->insertAction(ui->actionExit, actionInit);
  actionInit->setShortcut(QKeySequence(QLatin1String("Ctrl+N")));
  connect(actionInit, &QAction::triggered, this, &MainWindow::onInitRepository);

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
  m_pushButton = new QPushButton(tr("Push"), this);
  m_pushButton->setIcon(
      QApplication::style()->standardIcon(QStyle::SP_ArrowUp));
  m_pushButton->setEnabled(false);
  m_pushButton->setShortcut(QKeySequence(QLatin1String("Ctrl+Shift+P")));
  m_pullButton = new QToolButton(this);
  m_pullButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
  m_pullButton->setPopupMode(QToolButton::MenuButtonPopup);
  m_pullButton->setEnabled(false);
  m_pullButton->setShortcut(QKeySequence(QLatin1String("Ctrl+Shift+L")));
  remoteBar->addWidget(m_pushButton);
  remoteBar->addWidget(m_pullButton);

  m_pullButton->setText(tr("Pull"));

  auto *pullMenu = new QMenu(this);
  auto *ffIfPossible =
      pullMenu->addAction(tr("Pull (fast-forward if possible)"));
  auto *ffOnly = pullMenu->addAction(tr("Pull (fast-forward only)"));
  auto *rebase = pullMenu->addAction(tr("Pull (rebase)"));
  auto *fetchAll = pullMenu->addAction(tr("Fetch all"));
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
  m_pullArgs.clear();
  m_pullArgs << "pull";

  connect(m_pushButton, &QPushButton::clicked, this, [this] {
    if (m_currentPath.isEmpty())
      return;
    QString output;
    if (execGit(m_currentPath, {"push"}, &output)) {
      statusBar()->showMessage(tr("Pushed"));
    } else {
      QMessageBox::warning(this, tr("Push failed"), output);
    }
  });

  connect(m_pullButton, &QToolButton::clicked, this, [this] {
    if (m_currentPath.isEmpty())
      return;
    QString output;
    if (execGit(m_currentPath, m_pullArgs, &output)) {
      loadRepository(m_currentPath);
      statusBar()->showMessage(m_pullArgs.first() == QLatin1String("fetch")
                                   ? tr("Fetched")
                                   : tr("Pulled"));
    } else {
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

  m_commitTable = new QTableWidget(this);
  m_commitTable->setColumnCount(6);
  m_commitTable->setHorizontalHeaderLabels({tr("Graph"), tr("Date"),
                                            tr("Commit Message"), tr("Author"),
                                            tr("Branches"), tr("SHA")});
  m_commitTable->horizontalHeader()->setVisible(false);
  m_commitTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_commitTable->setSelectionMode(QAbstractItemView::SingleSelection);
  m_commitTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_commitTable->verticalHeader()->setVisible(false);
  m_commitTable->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  m_commitTable->setShowGrid(false);
  m_commitTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_commitTable->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  connect(m_commitTable, &QTableWidget::cellClicked, this,
          [this](int row, int column) {
            Q_UNUSED(column)
            onCommitSelected(m_commitTable->item(row, 0));
          });
  m_commitTable->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_commitTable, &QTableWidget::customContextMenuRequested, this,
          &MainWindow::showCommitContextMenu);
  setCentralWidget(m_commitTable);

  m_repoPanel = new QTreeWidget(this);
  m_repoPanel->setObjectName(QStringLiteral("repoPanel"));
  m_repoPanel->setHeaderHidden(true);
  m_repoPanel->setRootIsDecorated(true);
  m_repoPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_repoPanel->setMinimumWidth(80);
  m_repoPanel->setContextMenuPolicy(Qt::CustomContextMenu);
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
        } else {
          showBranchContextMenu(pos);
        }
      });
  connect(m_repoPanel, &QTreeWidget::itemClicked, this,
          &MainWindow::onTagClicked);
  connect(m_repoPanel, &QTreeWidget::itemClicked, this,
          &MainWindow::onStashClicked);
  connect(m_repoPanel, &QTreeWidget::itemClicked, this,
          &MainWindow::onBranchClicked);

  auto *dock = new QDockWidget(QStringLiteral("Repository"), this);
  dock->setTitleBarWidget(new QWidget(dock));
  dock->setWidget(m_repoPanel);
  dock->setFeatures(QDockWidget::DockWidgetMovable);
  dock->setMaximumWidth(400);
  addDockWidget(Qt::LeftDockWidgetArea, dock);
  m_repoDock = dock;

  auto *rightDock = new QDockWidget(tr("Working Tree"), this);
  rightDock->setFeatures(QDockWidget::DockWidgetMovable);
  auto *rightWidget = new QWidget(this);
  rightWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto *rightLayout = new QVBoxLayout(rightWidget);
  rightLayout->setContentsMargins(4, 4, 4, 4);
  rightLayout->setSpacing(4);

  auto *unstagedGroup = new QGroupBox(tr("Unstaged Files"), this);
  auto *unstagedLayout = new QVBoxLayout(unstagedGroup);
  m_unstagedTree = new QTreeWidget(this);
  m_unstagedTree->setHeaderLabels(QStringList{tr("Unstaged Files")});
  m_unstagedTree->setRootIsDecorated(true);
  m_unstagedTree->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_unstagedTree, &QTreeWidget::customContextMenuRequested, this,
          &MainWindow::showUnstagedContextMenu);
  connect(m_unstagedTree, &QTreeWidget::itemClicked, this,
          &MainWindow::onFileClicked);
  unstagedLayout->addWidget(m_unstagedTree);
  rightLayout->addWidget(unstagedGroup);

  auto *stagedGroup = new QGroupBox(tr("Staged Files"), this);
  auto *stagedLayout = new QVBoxLayout(stagedGroup);
  m_stagedTree = new QTreeWidget(this);
  m_stagedTree->setHeaderLabels(QStringList{tr("Staged Files")});
  m_stagedTree->setRootIsDecorated(true);
  m_stagedTree->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_stagedTree, &QTreeWidget::customContextMenuRequested, this,
          &MainWindow::showStagedContextMenu);
  connect(m_stagedTree, &QTreeWidget::itemClicked, this,
          &MainWindow::onFileClicked);
  stagedLayout->addWidget(m_stagedTree);
  rightLayout->addWidget(stagedGroup);

  auto *messageGroup = new QGroupBox(tr("Commit Message"), this);
  auto *messageLayout = new QVBoxLayout(messageGroup);
  m_commitSubject = new QLineEdit(this);
  m_commitSubject->setPlaceholderText(tr("Short summary"));
  m_commitSubject->setStyleSheet(QStringLiteral(
      "QLineEdit { color: #000000; background-color: #ffffff; }"));
  messageLayout->addWidget(m_commitSubject);
  m_commitBody = new QTextEdit(this);
  m_commitBody->setPlaceholderText(tr("Long description"));
  m_commitBody->setAcceptRichText(false);
  m_commitBody->setStyleSheet(QStringLiteral(
      "QTextEdit { color: #000000; background-color: #ffffff; }"));
  m_commitBody->setMaximumHeight(120);
  messageLayout->addWidget(m_commitBody);
  m_amendCheckBox = new QCheckBox(tr("Amend last commit"), this);
  messageLayout->addWidget(m_amendCheckBox);
  m_commitButton = new QPushButton(tr("Commit"), this);
  m_commitButton->setEnabled(false);
  m_commitButton->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
  messageLayout->addWidget(m_commitButton);
  rightLayout->addWidget(messageGroup);

  auto *commitFilesGroup = new QGroupBox(tr("Commit Files"), this);
  auto *commitFilesLayout = new QVBoxLayout(commitFilesGroup);
  m_commitFilesTree = new QTreeWidget(this);
  m_commitFilesTree->setHeaderHidden(true);
  m_commitFilesTree->setRootIsDecorated(true);
  m_commitFilesTree->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_commitFilesTree, &QTreeWidget::customContextMenuRequested, this,
          &MainWindow::showCommitFilesContextMenu);
  connect(m_commitFilesTree, &QTreeWidget::itemClicked, this,
          &MainWindow::onCommitFileClicked);
  commitFilesLayout->addWidget(m_commitFilesTree);
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

  auto *diffDock = new QDockWidget(tr("Diff"), this);
  m_diffView = new QTextEdit(this);
  m_diffView->setReadOnly(true);
  m_diffView->setMinimumHeight(120);
  m_diffView->setFont(QFont(QStringLiteral("monospace"), 10));
  m_diffView->setFrameStyle(QFrame::NoFrame);
  m_diffView->document()->setDocumentMargin(0);

  m_grepDock = new QDockWidget(tr("Grep"), this);
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
  diffDock->setWidget(m_diffView);
  diffDock->setFeatures(QDockWidget::DockWidgetMovable |
                        QDockWidget::DockWidgetFloatable);
  addDockWidget(Qt::BottomDockWidgetArea, diffDock);

  connect(ui->actionOpen, &QAction::triggered, this, [this] {
    const QString path =
        QFileDialog::getExistingDirectory(this, tr("Open Repository"));
    if (path.isEmpty())
      return;
    if (execGit(path, {"rev-parse", "--git-dir"})) {
      loadRepository(path);
    } else {
      QMessageBox::warning(this, tr("Not a Git repository"),
                           tr("The selected folder is not a Git repository."));
    }
  });

  connect(ui->actionClose, &QAction::triggered, this, [this] {
    m_currentPath.clear();
    m_repoPanel->clear();
    m_localBranchesItem = nullptr;
    m_remoteBranchesItem = nullptr;
    m_tagsItem = nullptr;
    m_stashesItem = nullptr;
    m_localHeadSha.clear();
    m_remoteHeadSha.clear();
    m_remoteBranchName.clear();
    m_unpushedShas.clear();
    m_unpulledShas.clear();
    m_commitTable->clear();
    m_commitTable->setRowCount(0);
    m_commitTable->horizontalHeader()->setVisible(false);
    if (m_unstagedTree)
      m_unstagedTree->clear();
    if (m_stagedTree)
      m_stagedTree->clear();
    if (m_diffView)
      m_diffView->clear();
    if (m_commitSubject)
      m_commitSubject->clear();
    if (m_commitBody)
      m_commitBody->clear();
    if (m_commitFilesTree)
      m_commitFilesTree->clear();
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
}

MainWindow::~MainWindow() { delete ui; }

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

void MainWindow::showPreferences() {
  QSettings settings("GitClientQt", "GitClientQt");
  QDialog dlg(this);
  dlg.setWindowTitle(tr("Preferences"));
  auto *layout = new QFormLayout(&dlg);

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

  layout->addRow(tr("Default pull mode:"), pullModeCombo);
  layout->addWidget(reopenBox);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  layout->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() != QDialog::Accepted)
    return;

  const QString pullMode = pullModeCombo->currentData().toString();
  settings.setValue("pullMode", pullMode);
  settings.setValue("reopenLastRepo", reopenBox->isChecked());

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

QStringList MainWindow::runGit(const QString &path, const QStringList &args,
                               int acceptedExitCode) const {
  QProcess p;
  p.start("git", QStringList{"-C", path} + args);
  if (!p.waitForFinished(10000)) {
    return {};
  }
  const int code = p.exitCode();
  if (code != 0 && code != acceptedExitCode) {
    return {};
  }
  return QString::fromLocal8Bit(p.readAllStandardOutput().trimmed())
      .split('\n', Qt::SkipEmptyParts);
}

void MainWindow::loadRepository(const QString &path) {
  if (path.isEmpty())
    return;
  if (runGit(path, {"rev-parse", "--git-dir"}).isEmpty()) {
    statusBar()->showMessage(tr("Not a git repository: %1").arg(path));
    return;
  }

  const bool isInitialLoad = m_currentPath.isEmpty();
  const QString repoRoot =
      runGit(path, {"rev-parse", "--show-toplevel"}).value(0);
  if (repoRoot.isEmpty()) {
    statusBar()->showMessage(
        tr("Could not determine repository root for %1").arg(path));
    return;
  }
  m_currentPath = repoRoot;
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
  if (m_pullButton)
    m_pullButton->setEnabled(true);
  m_repoPanel->clear();

  const QString currentBranch =
      runGit(path, {"rev-parse", "--abbrev-ref", "HEAD"}).value(0);

  m_localBranchesItem =
      new QTreeWidgetItem(m_repoPanel, {tr("Local Branches")});
  for (const QString &branch :
       runGit(path, {"branch", "--format=%(refname:short)"})) {
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
  for (const QString &branch :
       runGit(path, {"branch", "-r", "--format=%(refname:short)"})) {
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
  for (const QString &tag : runGit(path, {"tag", "--list"})) {
    new QTreeWidgetItem(m_tagsItem, QStringList{tag});
  }
  m_tagsItem->setExpanded(true);

  auto *submodules = new QTreeWidgetItem(m_repoPanel, {tr("Submodules")});
  for (const QString &line : runGit(path, {"submodule", "status"})) {
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
    new QTreeWidgetItem(submodules, QStringList{text});
  }
  submodules->setExpanded(true);

  m_stashesItem = new QTreeWidgetItem(m_repoPanel, {tr("Stashes")});
  loadStashes();
  m_stashesItem->setExpanded(true);

  m_commitTable->clear();
  m_commitTable->setRowCount(0);
  m_commitTable->setHorizontalHeaderLabels({tr("Graph"), tr("Date"),
                                            tr("Commit Message"), tr("Author"),
                                            tr("Branches"), tr("SHA")});
  m_commitTable->horizontalHeader()->setVisible(true);

  QMap<QString, int> wipCounts;
  for (const QString &line : runGit(path, {"status", "--porcelain"})) {
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

    auto *wipDateItem = new QTableWidgetItem();
    wipDateItem->setFlags(wipDateItem->flags() & ~Qt::ItemIsSelectable);
    m_commitTable->setItem(0, 1, wipDateItem);

    auto *wipWidget = new QWidget(m_commitTable);
    auto *wipLayout = new QHBoxLayout(wipWidget);
    wipLayout->setContentsMargins(4, 2, 4, 2);
    wipLayout->setSpacing(6);
    const QStringList order = {"M", "A", "D", "R", "C", "T", "?"};
    for (const QString &status : order) {
      if (!wipCounts.contains(status))
        continue;
      auto *iconLabel = new QLabel(wipWidget);
      iconLabel->setPixmap(statusIcon(status).pixmap(12, 12));
      wipLayout->addWidget(iconLabel);
      wipLayout->addWidget(
          new QLabel(QString::number(wipCounts.value(status)), wipWidget));
    }
    wipLayout->addStretch();
    m_commitTable->setCellWidget(0, 2, wipWidget);

    auto *wipAuthorItem = new QTableWidgetItem();
    wipAuthorItem->setFlags(wipAuthorItem->flags() & ~Qt::ItemIsSelectable);
    m_commitTable->setItem(0, 3, wipAuthorItem);

    auto *wipBranchesItem = new QTableWidgetItem();
    wipBranchesItem->setFlags(wipBranchesItem->flags() & ~Qt::ItemIsSelectable);
    m_commitTable->setItem(0, 4, wipBranchesItem);
  }

  QProcess p;
  p.start("git",
          QStringList{"-C", path} +
              QStringList{"log", "--all", "--graph", "--source", "--date-order",
                          "--date=format:%Y-%m-%d %H:%M:%S",
                          "--pretty=format:%x1f%H%x1f%h%x1f%an%x1f%ad%x1f%s%"
                          "x1f%b%x1f%S%x1e"});
  if (p.waitForFinished(10000) && p.exitCode() == 0) {
    const QString output =
        QString::fromLocal8Bit(p.readAllStandardOutput().trimmed());

    m_localHeadSha = runGit(path, {"rev-parse", "HEAD"}).value(0);
    m_remoteBranchName =
        runGit(path, {"rev-parse", "--abbrev-ref", "@{u}"}).value(0);
    m_remoteHeadSha =
        !m_remoteBranchName.isEmpty()
            ? runGit(path, {"rev-parse", m_remoteBranchName}).value(0)
            : QString();
    m_unpushedShas.clear();
    m_unpulledShas.clear();
    if (!m_remoteBranchName.isEmpty()) {
      for (const QString &sha :
           runGit(path, {"log", "--format=%H", m_remoteBranchName + "..HEAD"}))
        m_unpushedShas.insert(sha);
      for (const QString &sha :
           runGit(path, {"log", "--format=%H", "HEAD.." + m_remoteBranchName}))
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

    for (const QString &record :
         output.split(QChar(0x1e), Qt::SkipEmptyParts)) {
      const QStringList fields =
          record.trimmed().split(QChar(0x1f), Qt::KeepEmptyParts);
      if (fields.size() < 8) {
        continue;
      }

      const QString graph = fields.at(0);
      const QString fullSha = fields.at(1);
      const QString shortSha = fields.at(2);
      const QString author = fields.at(3);
      const QString date = fields.at(4);
      QString subject = fields.at(5);
      subject.remove("[skip ci]", Qt::CaseInsensitive);
      subject = subject.trimmed();
      const QString body = fields.at(6).trimmed();

      QString branchName = fields.at(7);
      if (branchName.startsWith("refs/heads/")) {
        branchName = branchName.mid(11);
      } else if (branchName.startsWith("refs/remotes/")) {
        branchName = branchName.mid(13);
      } else if (branchName.startsWith("refs/tags/")) {
        branchName = branchName.mid(10);
      }

      QString markers;
      if (fullSha == m_localHeadSha)
        markers += " [HEAD]";
      if (!m_remoteBranchName.isEmpty() && fullSha == m_remoteHeadSha)
        markers += " [" + m_remoteBranchName + "]";
      if (m_unpushedShas.contains(fullSha))
        markers += " ↑";
      if (m_unpulledShas.contains(fullSha))
        markers += " ↓";

      QString preview = subject;
      if (preview.length() > 60) {
        preview = preview.left(60) + "...";
      }
      preview += markers;

      const QString tip =
          tr("Subject: %1\n\n%2\n\nDate: %3\nAuthor: %4\nSHA: %5 (%6)")
              .arg(subject)
              .arg(body)
              .arg(date)
              .arg(author)
              .arg(shortSha)
              .arg(fullSha);

      QBrush bgBrush;
      if (m_unpushedShas.contains(fullSha))
        bgBrush = QBrush(QColor(225, 255, 225));
      else if (m_unpulledShas.contains(fullSha))
        bgBrush = QBrush(QColor(255, 240, 225));

      const int row = m_commitTable->rowCount();
      m_commitTable->insertRow(row);

      auto *graphItem = new QTableWidgetItem(graph);
      graphItem->setToolTip(tip);
      QFont graphFont;
      graphFont.setStyleHint(QFont::Monospace);
      graphFont.setFamily("Monospace");
      graphItem->setFont(graphFont);
      if (bgBrush != QBrush())
        graphItem->setBackground(bgBrush);
      m_commitTable->setItem(row, 0, graphItem);

      auto *dateItem = new QTableWidgetItem(date);
      dateItem->setToolTip(tip);
      if (bgBrush != QBrush())
        dateItem->setBackground(bgBrush);
      m_commitTable->setItem(row, 1, dateItem);

      auto *msgItem = new QTableWidgetItem(preview);
      msgItem->setToolTip(tip);
      if (bgBrush != QBrush())
        msgItem->setBackground(bgBrush);
      m_commitTable->setItem(row, 2, msgItem);

      auto *authorItem = new QTableWidgetItem(author);
      authorItem->setToolTip(tip);
      if (bgBrush != QBrush())
        authorItem->setBackground(bgBrush);
      m_commitTable->setItem(row, 3, authorItem);

      auto *branchItem = new QTableWidgetItem(branchName);
      branchItem->setToolTip(tip);
      if (bgBrush != QBrush())
        branchItem->setBackground(bgBrush);
      m_commitTable->setItem(row, 4, branchItem);

      auto *shaItem = new QTableWidgetItem(shortSha);
      shaItem->setData(Qt::UserRole, fullSha);
      shaItem->setToolTip(tip);
      if (bgBrush != QBrush())
        shaItem->setBackground(bgBrush);
      m_commitTable->setItem(row, 5, shaItem);
    }
  }
  m_commitTable->resizeColumnsToContents();
  const int tableWidth =
      m_commitTable->horizontalHeader()->length() +
      QApplication::style()->pixelMetric(QStyle::PM_ScrollBarExtent) +
      2 * m_commitTable->frameWidth();
  m_commitTable->setFixedWidth(tableWidth);
  const int leftWidth =
      qBound(80, m_repoPanel ? m_repoPanel->sizeHint().width() + 20 : 180, 200);
  const int rightWidth = 200;
  if (m_repoDock && m_workTreeDock) {
    resizeDocks({m_repoDock, m_workTreeDock}, {leftWidth, rightWidth},
                Qt::Horizontal);
  }
  if (isInitialLoad)
    resize(leftWidth + tableWidth + rightWidth, height());
  qDebug() << "loadRepository: loaded" << m_commitTable->rowCount()
           << "commits";
  updateFilter();

  loadWorkingTree();

  statusBar()->showMessage(tr("Loaded: %1").arg(path));
}

bool MainWindow::execGit(const QString &path, const QStringList &args,
                         QString *output) const {
  QProcess p;
  p.start("git", QStringList{"-C", path} + args);
  if (!p.waitForStarted(5000)) {
    if (output)
      *output = tr("Failed to start git: %1").arg(p.errorString());
    return false;
  }
  if (!p.waitForFinished(30000)) {
    p.kill();
    p.waitForFinished(1000);
    if (output) {
      *output = tr("Git command timed out or was killed.\n%1")
                    .arg(QString::fromLocal8Bit(p.readAllStandardOutput() +
                                                p.readAllStandardError()));
    }
    return false;
  }
  if (output)
    *output = QString::fromLocal8Bit(p.readAllStandardOutput() +
                                     p.readAllStandardError());
  return p.exitCode() == 0;
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
    auto *switchAction = menu.addAction(tr("Switch to %1").arg(branchName));
    auto *pushAction = menu.addAction(tr("Push %1 to remote").arg(branchName));
    auto *setUpstreamAction = menu.addAction(tr("Set upstream"));
    auto *createAction =
        menu.addAction(tr("Create branch from %1").arg(branchName));
    auto *renameAction = menu.addAction(tr("Rename"));
    const QString currentBranch =
        runGit(m_currentPath, {"rev-parse", "--abbrev-ref", "HEAD"}).value(0);
    auto *deleteAction =
        (branchName != currentBranch) ? menu.addAction(tr("Delete")) : nullptr;
    selected = menu.exec(m_repoPanel->viewport()->mapToGlobal(pos));

    if (!selected) {
      return;
    }

    if (selected == switchAction) {
      if (execGit(m_currentPath, {"checkout", branchName})) {
        loadRepository(m_currentPath);
      } else {
        statusBar()->showMessage(tr("Failed to switch to %1").arg(branchName));
      }
    } else if (selected == pushAction) {
      const QStringList remotes = runGit(m_currentPath, {"remote"});
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
      if (execGit(m_currentPath, {"push", "-u", remote, ref})) {
        loadRepository(m_currentPath);
        statusBar()->showMessage(
            tr("Pushed %1 to %2/%3").arg(branchName, remote, destBranch));
      } else {
        statusBar()->showMessage(tr("Failed to push %1").arg(branchName));
      }
    } else if (selected == setUpstreamAction) {
      const QStringList remoteBranchesRaw =
          runGit(m_currentPath, {"branch", "-r"});
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
      if (execGit(m_currentPath,
                  {"branch", "--set-upstream-to", upstream, branchName})) {
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
        if (execGit(m_currentPath, {"branch", newName, branchName})) {
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
        if (execGit(m_currentPath, {"branch", "-m", branchName, newName})) {
          loadRepository(m_currentPath);
        } else {
          statusBar()->showMessage(tr("Failed to rename %1").arg(branchName));
        }
      }
    } else if (selected == deleteAction) {
      if (execGit(m_currentPath, {"branch", "-d", branchName})) {
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
        if (execGit(m_currentPath,
                    {"checkout", "-b", localName, fullBranchName})) {
          loadRepository(m_currentPath);
        } else {
          statusBar()->showMessage(
              tr("Failed to checkout %1 as %2").arg(fullBranchName, localName));
        }
      }
    } else if (selected == deleteRemoteAction) {
      const QString remote = item->parent()->text(0);
      const QString rbranch = branchName;
      if (execGit(m_currentPath, {"push", remote, "--delete", rbranch})) {
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
    auto *createAction = menu.addAction(tr("Create Stash"));
    if (menu.exec(m_repoPanel->viewport()->mapToGlobal(pos)) != createAction)
      return;

    bool ok;
    const QString message =
        QInputDialog::getText(this, tr("Create Stash"), tr("Message:"),
                              QLineEdit::Normal, QString(), &ok);
    if (!ok || message.isEmpty())
      return;

    if (execGit(m_currentPath, {"stash", "push", "-m", message})) {
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

  auto *popAction = menu.addAction(tr("Pop Stash"));
  auto *applyAction = menu.addAction(tr("Apply Stash"));
  auto *deleteAction = menu.addAction(tr("Delete Stash"));
  QAction *selected = menu.exec(m_repoPanel->viewport()->mapToGlobal(pos));

  if (selected == popAction) {
    if (execGit(m_currentPath, {"stash", "pop", ref})) {
      loadRepository(m_currentPath);
      statusBar()->showMessage(tr("Stash popped"));
    } else {
      statusBar()->showMessage(tr("Failed to pop stash"));
    }
  } else if (selected == applyAction) {
    if (execGit(m_currentPath, {"stash", "apply", ref})) {
      loadRepository(m_currentPath);
      statusBar()->showMessage(tr("Stash applied"));
    } else {
      statusBar()->showMessage(tr("Failed to apply stash"));
    }
  } else if (selected == deleteAction) {
    if (execGit(m_currentPath, {"stash", "drop", ref})) {
      loadStashes();
      statusBar()->showMessage(tr("Stash deleted"));
    } else {
      statusBar()->showMessage(tr("Failed to delete stash"));
    }
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

  const QString sha = runGit(m_currentPath, {"rev-parse", branch}).value(0);
  if (sha.isEmpty())
    return;

  for (int row = 0; row < m_commitTable->rowCount(); ++row) {
    QTableWidgetItem *shaItem = m_commitTable->item(row, 5);
    if (shaItem && shaItem->data(Qt::UserRole).toString() == sha) {
      m_commitTable->selectRow(row);
      QTableWidgetItem *msgItem = m_commitTable->item(row, 2);
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
    auto *createAction = menu.addAction(tr("Create Tag at HEAD"));
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
          if (execGit(m_currentPath, {"tag", tagName}, &output)) {
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
    auto *checkoutAction = menu.addAction(tr("Checkout Tag %1").arg(tagName));
    auto *deleteAction = menu.addAction(tr("Delete Tag %1").arg(tagName));
    QAction *selected = menu.exec(m_repoPanel->viewport()->mapToGlobal(pos));

    if (selected == checkoutAction) {
      QString output;
      if (execGit(m_currentPath, {"checkout", tagName}, &output)) {
        loadRepository(m_currentPath);
        statusBar()->showMessage(tr("Checked out tag %1").arg(tagName));
      } else {
        QMessageBox::warning(this, tr("Checkout failed"), output);
      }
    } else if (selected == deleteAction) {
      if (execGit(m_currentPath, {"tag", "-d", tagName})) {
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
      runGit(m_currentPath, {"log", "-1", tagName, "--format=%H"}).value(0);
  qDebug() << "onTagClicked: resolved sha=" << sha
           << "tableRows=" << m_commitTable->rowCount();
  if (sha.isEmpty()) {
    qDebug() << "onTagClicked: empty SHA, returning";
    return;
  }

  for (int row = 0; row < m_commitTable->rowCount(); ++row) {
    QTableWidgetItem *shaItem = m_commitTable->item(row, 5);
    if (shaItem && shaItem->data(Qt::UserRole).toString() == sha) {
      m_commitTable->selectRow(row);
      QTableWidgetItem *msgItem = m_commitTable->item(row, 2);
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
       runGit(m_currentPath, {"grep", "-n", "-I", pattern})) {
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

void MainWindow::addFileToTree(QTreeWidget *tree, const QString &filePath,
                               const QString &status) {
  const QStringList parts = filePath.split('/');
  const QString fileName = parts.last();
  if (fileName.isEmpty()) {
    return;
  }
  QTreeWidgetItem *parentItem = tree->invisibleRootItem();
  for (int i = 0; i < parts.size() - 1; ++i) {
    const QString &dirName = parts.at(i);
    QTreeWidgetItem *child = nullptr;
    for (int c = 0; c < parentItem->childCount(); ++c) {
      if (parentItem->child(c)->text(0) == dirName) {
        child = parentItem->child(c);
        break;
      }
    }
    if (!child) {
      child = new QTreeWidgetItem(parentItem, QStringList{dirName});
      child->setIcon(0, statusIcon(QString()));
    }
    parentItem = child;
  }
  QTreeWidgetItem *leaf =
      new QTreeWidgetItem(parentItem, QStringList{fileName});
  leaf->setIcon(0, statusIcon(status));
  if (!status.isEmpty()) {
    leaf->setData(0, Qt::UserRole, status);
  }
}

QIcon MainWindow::statusIcon(const QString &status) const {
  if (status.isEmpty())
    return QApplication::style()->standardIcon(QStyle::SP_DirIcon);

  QPixmap pixmap(16, 16);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);

  if (status == "?" || status == "A") {
    QPen pen(QColor(40, 167, 69));
    pen.setWidth(2);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.drawLine(8, 4, 8, 12);
    painter.drawLine(4, 8, 12, 8);
  } else if (status == "M") {
    QPen pen(QColor(0, 123, 255));
    pen.setWidth(2);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.drawLine(5, 13, 11, 5);
    QPolygonF tip;
    tip << QPointF(11, 5) << QPointF(12, 4) << QPointF(13, 5) << QPointF(12, 6);
    painter.setBrush(QColor(0, 123, 255));
    painter.setPen(Qt::NoPen);
    painter.drawPolygon(tip);
    QPolygonF eraser;
    eraser << QPointF(5, 13) << QPointF(4, 14) << QPointF(5, 15)
           << QPointF(6, 14);
    painter.drawPolygon(eraser);
  } else if (status == "D") {
    QPen pen(QColor(220, 53, 69));
    pen.setWidth(2);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.drawLine(4, 8, 12, 8);
  } else {
    painter.setBrush(QColor(160, 160, 160));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(pixmap.rect().adjusted(2, 2, -2, -2));
  }

  return QIcon(pixmap);
}

void MainWindow::loadWorkingTree() {
  if (m_unstagedTree)
    m_unstagedTree->clear();
  if (m_stagedTree)
    m_stagedTree->clear();
  if (m_currentPath.isEmpty()) {
    return;
  }

  for (const QString &line :
       runGit(m_currentPath, {"diff", "--cached", "--name-status"})) {
    const QStringList parts = line.split('\t');
    if (parts.isEmpty())
      continue;
    addFileToTree(m_stagedTree, parts.last(), parts.first().left(1));
  }

  for (const QString &line : runGit(m_currentPath, {"diff", "--name-status"})) {
    const QStringList parts = line.split('\t');
    if (parts.isEmpty())
      continue;
    addFileToTree(m_unstagedTree, parts.last(), parts.first().left(1));
  }

  for (const QString &filePath :
       runGit(m_currentPath, {"ls-files", "--others", "--exclude-standard"})) {
    addFileToTree(m_unstagedTree, filePath, QStringLiteral("?"));
  }

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
        runGit(m_currentPath, {"log", "-1", "--format=%B"});
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
  const bool empty = text.isEmpty();
  for (int row = 0; row < m_commitTable->rowCount(); ++row) {
    bool match = empty;
    if (!empty) {
      for (int col : {1, 2, 3}) {
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

  const QStringList commitArgs =
      (m_amendCheckBox && m_amendCheckBox->isChecked())
          ? QStringList{"commit", "--amend", "-F", tempFile.fileName()}
          : QStringList{"commit", "-F", tempFile.fileName()};
  if (execGit(m_currentPath, commitArgs)) {
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

QString MainWindow::itemPath(QTreeWidget *tree, QTreeWidgetItem *item) const {
  QStringList parts;
  QTreeWidgetItem *current = item;
  while (current && current != tree->invisibleRootItem()) {
    parts.prepend(current->text(0));
    current = current->parent();
  }
  return parts.join('/');
}

QString MainWindow::formatDiff(const QStringList &lines) const {
  QString html = QStringLiteral(
      "<html>"
      "<body style=\"background-color:#1e1e1e\">"
      "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\">");

  int oldLine = -1;
  int newLine = -1;
  QRegularExpression hunkRe(
      QStringLiteral("@@ -(\\d+)(?:,\\d+)? [+](\\d+)(?:,\\d+)? @@"));

  for (const QString &line : lines) {
    if (line.startsWith(QStringLiteral("@@"))) {
      const QRegularExpressionMatch m = hunkRe.match(line);
      if (m.hasMatch()) {
        oldLine = m.captured(1).toInt();
        newLine = m.captured(2).toInt();
      }
      html +=
          QStringLiteral("<tr><td colspan=\"2\" "
                         "style=\"background-color:#3c3c3c; color:#aaaaaa;\">"
                         "<pre style=\"margin:0\">") +
          line.toHtmlEscaped() + QStringLiteral("</pre></td></tr>");
      continue;
    }

    if (line.startsWith(QStringLiteral("diff --git")) ||
        line.startsWith(QStringLiteral("index ")) ||
        line.startsWith(QStringLiteral("--- ")) ||
        line.startsWith(QStringLiteral("+++ "))) {
      html +=
          QStringLiteral("<tr><td colspan=\"2\" "
                         "style=\"background-color:#3c3c3c; color:#aaaaaa;\">"
                         "<pre style=\"margin:0\">") +
          line.toHtmlEscaped() + QStringLiteral("</pre></td></tr>");
      continue;
    }

    QString bg;
    QString fg;
    QString lineNum = QStringLiteral("&nbsp;");
    const QString content = line.toHtmlEscaped();

    if (line.startsWith('+') && !line.startsWith(QStringLiteral("+++ "))) {
      bg = QStringLiteral("#1e4d2b");
      fg = QStringLiteral("#d4edda");
      lineNum = QString::number(newLine++);
    } else if (line.startsWith('-') &&
               !line.startsWith(QStringLiteral("--- "))) {
      bg = QStringLiteral("#4d1e1e");
      fg = QStringLiteral("#f8d7da");
      lineNum = QString::number(oldLine++);
    } else {
      bg = QStringLiteral("#2b2b2b");
      fg = QStringLiteral("#cccccc");
      if (newLine >= 0)
        lineNum = QString::number(newLine++);
      else if (oldLine >= 0)
        lineNum = QString::number(oldLine++);
    }

    html += QStringLiteral("<tr>"
                           "<td align=\"right\" "
                           "style=\"width:45px; background-color:#2b2b2b; "
                           "color:#888888;\">"
                           "<pre style=\"margin:0\">") +
            lineNum +
            QStringLiteral("</pre></td>"
                           "<td style=\"background-color:") +
            bg + QStringLiteral("; color:") + fg +
            QStringLiteral("; padding-left:4px;\">"
                           "<pre style=\"margin:0\">") +
            content + QStringLiteral("</pre></td></tr>");
  }

  html += QStringLiteral("</table></body></html>");
  return html;
}

void MainWindow::showUnstagedContextMenu(const QPoint &pos) {
  QTreeWidgetItem *item = m_unstagedTree->itemAt(pos);
  if (!item || m_currentPath.isEmpty()) {
    return;
  }

  const bool isFolder = item->childCount() > 0;
  const QString path = itemPath(m_unstagedTree, item);

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
      menu.addAction(isFolder ? tr("Stage folder") : tr("Stage file"));
  QAction *stashAction =
      menu.addAction(isFolder ? tr("Stash folder") : tr("Stash file"));
  QAction *discardAction =
      menu.addAction(isFolder ? tr("Discard all changes in this folder")
                              : tr("Discard all changes"));
  QAction *ignoreAction = nullptr;
  if (isFolder || hasNew) {
    ignoreAction = menu.addAction(
        isFolder ? tr("Ignore all files in this folder") : tr("Ignore"));
  }
  QAction *blameAction = nullptr;
  if (!isFolder) {
    blameAction = menu.addAction(tr("Blame"));
  }

  QAction *selected = menu.exec(m_unstagedTree->mapToGlobal(pos));
  if (selected == stageAction) {
    if (execGit(m_currentPath, {"add", path})) {
      loadWorkingTree();
    }
  } else if (selected == stashAction) {
    if (execGit(m_currentPath,
                {"stash", "push", "-m", "Stash " + path, "--", path})) {
      loadWorkingTree();
      loadStashes();
    }
  } else if (selected == discardAction) {
    bool ok = true;
    if (hasTracked) {
      ok &= execGit(m_currentPath, {"checkout", "--", path});
    }
    if (hasNew) {
      ok &= execGit(m_currentPath, {"clean", "-fd", "--", path});
    }
    if (ok) {
      loadWorkingTree();
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
  }
}

void MainWindow::showRemotesContextMenu(const QPoint &pos) {
  QTreeWidgetItem *item = m_repoPanel->itemAt(pos);
  if (!item || !m_remotesItem)
    return;

  QMenu menu(this);
  if (item == m_remotesItem) {
    auto *addAction = menu.addAction(tr("Add Remote"));
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
    if (execGit(m_currentPath, {"remote", "add", name, url})) {
      loadRemotes();
      statusBar()->showMessage(tr("Remote %1 added").arg(name));
    } else {
      statusBar()->showMessage(tr("Failed to add remote %1").arg(name));
    }
  } else if (item->parent() == m_remotesItem) {
    const QString remoteName = item->data(0, Qt::UserRole).toString();
    const QString currentUrl = item->data(0, Qt::UserRole + 1).toString();
    auto *editAction = menu.addAction(tr("Edit URL"));
    auto *removeAction = menu.addAction(tr("Remove Remote"));
    QAction *selected = menu.exec(m_repoPanel->viewport()->mapToGlobal(pos));

    if (selected == editAction) {
      bool ok;
      const QString newUrl =
          QInputDialog::getText(this, tr("Edit Remote URL"), tr("URL:"),
                                QLineEdit::Normal, currentUrl, &ok);
      if (ok && !newUrl.isEmpty() && newUrl != currentUrl) {
        if (execGit(m_currentPath, {"remote", "set-url", remoteName, newUrl})) {
          loadRemotes();
          statusBar()->showMessage(tr("Remote %1 URL updated").arg(remoteName));
        } else {
          statusBar()->showMessage(
              tr("Failed to update remote %1").arg(remoteName));
        }
      }
    } else if (selected == removeAction) {
      if (QMessageBox::question(this, tr("Remove Remote"),
                                tr("Remove remote %1?").arg(remoteName)) ==
          QMessageBox::Yes) {
        if (execGit(m_currentPath, {"remote", "remove", remoteName})) {
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
  const QString path = itemPath(m_stagedTree, item);
  QAction *unstageAction =
      menu.addAction(isFolder ? tr("Unstage folder") : tr("Unstage file"));
  QAction *blameAction = nullptr;
  if (!isFolder) {
    blameAction = menu.addAction(tr("Blame"));
  }
  QAction *selected = menu.exec(m_stagedTree->mapToGlobal(pos));
  if (selected == unstageAction) {
    if (execGit(m_currentPath, {"reset", "HEAD", "--", path})) {
      loadWorkingTree();
    }
  } else if (selected == blameAction) {
    showBlame(path);
  }
}

void MainWindow::showCommitContextMenu(const QPoint &pos) {
  QTableWidgetItem *item = m_commitTable->itemAt(pos);
  if (!item || m_currentPath.isEmpty())
    return;

  const int row = item->row();
  QTableWidgetItem *shaItem = m_commitTable->item(row, 5);
  if (!shaItem)
    return;
  const QString sha = shaItem->data(Qt::UserRole).toString();
  if (sha.isEmpty())
    return;

  QMenu menu(this);
  auto *checkoutAction = menu.addAction(tr("Checkout this Commit"));
  auto *createBranchAction =
      menu.addAction(tr("Create branch from this commit"));
  auto *createTagAction = menu.addAction(tr("Create Tag for this Commit"));
  auto *diffAction = menu.addAction(tr("Diff with another commit"));
  QAction *selected = menu.exec(m_commitTable->viewport()->mapToGlobal(pos));

  if (selected == checkoutAction) {
    QString output;
    if (execGit(m_currentPath, {"checkout", sha}, &output)) {
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

    if (execGit(m_currentPath, {"branch", branchName, sha})) {
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
  if (execGit(m_currentPath, {"tag", tagName, sha}, &output)) {
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
    QTableWidgetItem *shaItem = m_commitTable->item(row, 5);
    if (!shaItem)
      continue;
    const QString sha = shaItem->data(Qt::UserRole).toString();
    if (sha.isEmpty() || sha == fromSha)
      continue;
    QTableWidgetItem *msgItem = m_commitTable->item(row, 2);
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
  const QStringList diff = runGit(m_currentPath, {"diff", fromSha, toSha});
  m_diffView->setHtml(diff.isEmpty() ? tr("No diff") : formatDiff(diff));
  statusBar()->showMessage(
      tr("Diff between %1 and %2").arg(fromSha.left(7), toSha.left(7)));
}

void MainWindow::showCommitFilesContextMenu(const QPoint &pos) {
  QTreeWidgetItem *item = m_commitFilesTree->itemAt(pos);
  if (!item || m_currentPath.isEmpty() || m_selectedCommitSha.isEmpty())
    return;

  if (item->childCount() > 0)
    return;

  const QString path = itemPath(m_commitFilesTree, item);
  QMenu menu(this);
  auto *blameAction = menu.addAction(tr("Blame"));
  if (menu.exec(m_commitFilesTree->mapToGlobal(pos)) == blameAction)
    showBlame(path, m_selectedCommitSha);
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
  auto *view = new QTextEdit(&dlg);
  view->setReadOnly(true);
  view->setFont(QFont(QStringLiteral("monospace"), 10));
  layout->addWidget(view);

  QString currentSha;
  QString currentAuthor;
  QString currentSummary;
  qint64 currentTime = 0;
  int currentLine = 0;

  QString html = QStringLiteral(
      "<html>"
      "<body style=\"background-color:#1e1e1e; color:#d4d4d4;\" >"
      "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"2\">");

  const QRegularExpression hunkRe(
      QStringLiteral("^(\\^?[0-9a-fA-F]{40}) (\\d+) (\\d+) (\\d+)$"));
  for (const QString &line : runGit(m_currentPath, args)) {
    const QRegularExpressionMatch m = hunkRe.match(line);
    if (m.hasMatch()) {
      currentSha = m.captured(1);
      currentSha.remove(QLatin1Char('^'));
      currentLine = m.captured(3).toInt();
      currentAuthor.clear();
      currentSummary.clear();
      currentTime = 0;
    } else if (line.startsWith(QStringLiteral("author "))) {
      currentAuthor = line.mid(7);
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
      html +=
          QStringLiteral(
              "<tr>"
              "<td style=\"white-space:nowrap; background-color:#252526; "
              "color:#9cdcfe; padding-right:12px;\">%1</td>"
              "<td style=\"white-space:nowrap; background-color:#252526; "
              "color:#dcdcaa; padding-right:12px;\">%2</td>"
              "<td style=\"white-space:nowrap; background-color:#252526; "
              "color:#808080; padding-right:12px;\">%3</td>"
              "<td style=\"white-space:nowrap; background-color:#1e1e1e; "
              "color:#d4d4d4;\"><pre style=\"margin:0; font-family:monospace; "
              "white-space:pre;\">%4</pre></td>"
              "</tr>")
              .arg(currentSha.left(7).toHtmlEscaped(),
                   currentSummary.toHtmlEscaped(), date,
                   content.toHtmlEscaped());
      ++currentLine;
    }
  }

  html += QStringLiteral("</table></body></html>");
  view->setHtml(html);

  auto *closeBtn = new QPushButton(tr("Close"), &dlg);
  connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
  layout->addWidget(closeBtn);
  dlg.resize(1000, 700);
  dlg.exec();
}

void MainWindow::onCommitSelected(QTableWidgetItem *item) {
  if (!item || !m_commitFilesTree)
    return;

  if (m_diffView)
    m_diffView->clear();

  m_commitFilesTree->clear();
  m_selectedCommitSha.clear();

  const int row = item->row();
  QTableWidgetItem *shaItem = m_commitTable->item(row, 5);
  if (!shaItem)
    return;

  m_selectedCommitSha = shaItem->data(Qt::UserRole).toString();

  for (const QString &line :
       runGit(m_currentPath, {"diff-tree", "--no-commit-id", "--name-status",
                              "--root", "-r", m_selectedCommitSha})) {
    const QStringList parts = line.split('\t');
    if (parts.size() < 2)
      continue;
    const QString status = parts.first();
    const QString filePath = parts.last();
    if (status.isEmpty() || filePath.isEmpty())
      continue;
    addFileToTree(m_commitFilesTree, filePath, status);
  }

  if (m_commitFilesTree->topLevelItemCount() > 0)
    m_commitFilesTree->collapseAll();
}

void MainWindow::onCommitFileClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column)
  if (!item || m_currentPath.isEmpty() || m_selectedCommitSha.isEmpty())
    return;

  const bool isFolder = item->childCount() > 0;
  if (isFolder)
    return;

  const QString path = itemPath(m_commitFilesTree, item);
  const QStringList diff =
      runGit(m_currentPath, {"show", "--pretty=format:", "--no-notes",
                             m_selectedCommitSha, "--", path});
  if (m_diffView) {
    m_diffView->setHtml(diff.isEmpty() ? tr("No diff") : formatDiff(diff));
  }
}

void MainWindow::onInitRepository() {
  const QString path =
      QFileDialog::getExistingDirectory(this, tr("Initialize Repository"));
  if (path.isEmpty())
    return;

  if (execGit(path, {"rev-parse", "--git-dir"})) {
    QMessageBox::warning(this, tr("Already a Git repository"),
                         tr("The selected folder is already a Git "
                            "repository."));
    return;
  }

  if (execGit(path, {"init"})) {
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

  if (execGit(parentDir, {"clone", url, repoName})) {
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

  for (const QString &line : runGit(m_currentPath, {"remote", "-v"})) {
    const QStringList parts = line.split('\t');
    if (parts.size() < 2)
      continue;
    const QString name = parts.at(0);
    const QString rest = parts.at(1);
    if (rest.endsWith(" (push)"))
      continue;
    const QString url = rest.section(' ', 0, -2);
    QTreeWidgetItem *child = new QTreeWidgetItem(m_remotesItem, {name});
    child->setToolTip(0, url);
    child->setData(0, Qt::UserRole, name);
    child->setData(0, Qt::UserRole + 1, url);
    child->setText(0, QString("%1   %2").arg(name, url));
  }
}

void MainWindow::loadStashes() {
  if (!m_stashesItem || m_currentPath.isEmpty())
    return;

  while (m_stashesItem->childCount() > 0)
    delete m_stashesItem->takeChild(0);

  for (const QString &line : runGit(m_currentPath, {"stash", "list"})) {
    const int colon = line.indexOf(':');
    if (colon < 0)
      continue;
    const QString ref = line.left(colon).trimmed();
    const QString msg = line.mid(colon + 1).trimmed();
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
         runGit(m_currentPath,
                {"diff", "--name-status", ref + QLatin1Char('^'), ref})) {
      const QStringList parts = line.split('\t');
      if (parts.size() < 2)
        continue;
      addFileToTree(m_commitFilesTree, parts.last(), parts.first().left(1));
    }
    if (m_commitFilesTree->topLevelItemCount() > 0)
      m_commitFilesTree->collapseAll();
  }

  const QStringList diff =
      runGit(m_currentPath, {"show", "--pretty=format:", "--no-notes", ref});
  if (m_diffView) {
    m_diffView->setHtml(diff.isEmpty() ? tr("No diff") : formatDiff(diff));
  }
}

void MainWindow::onFileClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column)
  if (!item || m_currentPath.isEmpty()) {
    return;
  }

  QTreeWidget *tree = qobject_cast<QTreeWidget *>(sender());
  if (!tree) {
    return;
  }

  const QString path = itemPath(tree, item);
  const bool staged = (tree == m_stagedTree);
  const bool isFolder = item->childCount() > 0;
  const bool isNew =
      isFolder ? false : (item->data(0, Qt::UserRole).toString() == "?");

  if (isNew) {
    if (staged) {
      const QStringList diff =
          runGit(m_currentPath, {"diff", "--cached", "--", path});
      if (m_diffView) {
        m_diffView->setHtml(diff.isEmpty() ? tr("No diff") : formatDiff(diff));
      }
    } else {
      const QStringList diff = runGit(
          m_currentPath,
          {"diff", "--no-index", "--", QStringLiteral("/dev/null"), path}, 1);
      if (m_diffView) {
        m_diffView->setHtml(diff.isEmpty() ? tr("No diff") : formatDiff(diff));
      }
    }
    return;
  }

  const QStringList diff =
      runGit(m_currentPath, staged ? QStringList{"diff", "--cached", "--", path}
                                   : QStringList{"diff", "--", path});
  if (m_diffView) {
    m_diffView->setHtml(diff.isEmpty() ? tr("No diff") : formatDiff(diff));
  }
}
