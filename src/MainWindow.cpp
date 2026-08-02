#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QDebug>

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <QGroupBox>
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
  setStyleSheet("QMainWindow::separator { background: #808080; width: 4px; }");

  auto *actionInit = new QAction(tr("Initialize Repository"), this);
  ui->menuFile->insertAction(ui->actionExit, actionInit);
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
  m_pullButton = new QPushButton(tr("Pull"), this);
  m_pullButton->setIcon(
      QApplication::style()->standardIcon(QStyle::SP_ArrowDown));
  m_pullButton->setEnabled(false);
  remoteBar->addWidget(m_pushButton);
  remoteBar->addWidget(m_pullButton);
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
  connect(m_pullButton, &QPushButton::clicked, this, [this] {
    if (m_currentPath.isEmpty())
      return;
    QString output;
    if (execGit(m_currentPath, {"pull"}, &output)) {
      loadRepository(m_currentPath);
      statusBar()->showMessage(tr("Pulled"));
    } else {
      QMessageBox::warning(this, tr("Pull failed"), output);
    }
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
  connect(m_commitTable, &QTableWidget::itemClicked, this,
          &MainWindow::onCommitSelected);
  setCentralWidget(m_commitTable);

  m_repoPanel = new QTreeWidget(this);
  m_repoPanel->setObjectName(QStringLiteral("repoPanel"));
  m_repoPanel->setHeaderHidden(true);
  m_repoPanel->setRootIsDecorated(true);
  m_repoPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_repoPanel->setMinimumWidth(80);
  m_repoPanel->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_repoPanel, &QTreeWidget::customContextMenuRequested, this,
          [this](const QPoint &pos) {
            QTreeWidgetItem *item = m_repoPanel->itemAt(pos);
            if (item && m_stashesItem && item->parent() == m_stashesItem) {
              showStashContextMenu(pos);
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
  messageLayout->addWidget(m_commitSubject);
  m_commitBody = new QTextEdit(this);
  m_commitBody->setPlaceholderText(tr("Long description"));
  m_commitBody->setMaximumHeight(120);
  messageLayout->addWidget(m_commitBody);
  m_commitButton = new QPushButton(tr("Commit"), this);
  m_commitButton->setEnabled(false);
  messageLayout->addWidget(m_commitButton);
  rightLayout->addWidget(messageGroup);

  auto *commitFilesGroup = new QGroupBox(tr("Commit Files"), this);
  auto *commitFilesLayout = new QVBoxLayout(commitFilesGroup);
  m_commitFilesTree = new QTreeWidget(this);
  m_commitFilesTree->setHeaderHidden(true);
  m_commitFilesTree->setRootIsDecorated(true);
  m_commitFilesTree->setContextMenuPolicy(Qt::NoContextMenu);
  connect(m_commitFilesTree, &QTreeWidget::itemClicked, this,
          &MainWindow::onCommitFileClicked);
  commitFilesLayout->addWidget(m_commitFilesTree);
  rightLayout->addWidget(commitFilesGroup);

  connect(m_commitButton, &QPushButton::clicked, this,
          &MainWindow::onCommitClicked);
  connect(m_commitSubject, &QLineEdit::textChanged, this,
          &MainWindow::updateCommitButton);

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
    statusBar()->showMessage(tr("Repository closed"));
  });

  connect(ui->actionExit, &QAction::triggered, qApp, &QApplication::quit,
          Qt::QueuedConnection);
  connect(ui->actionAbout, &QAction::triggered, this,
          [this] { QMessageBox::about(this, tr("About"), tr("Git Client")); });
}

MainWindow::~MainWindow() { delete ui; }

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
  if (runGit(path, {"rev-parse", "--git-dir"}).isEmpty()) {
    statusBar()->showMessage(tr("Not a git repository: %1").arg(path));
    return;
  }

  const bool isInitialLoad = m_currentPath.isEmpty();
  m_currentPath = path;
  setWindowTitle(QFileInfo(path).fileName() + " - " + tr("Git Client Qt"));
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
    auto *createAction =
        menu.addAction(tr("Create branch from %1...").arg(branchName));
    auto *renameAction = menu.addAction(tr("Rename..."));
    auto *deleteAction = menu.addAction(tr("Delete"));
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
    auto *checkoutAction = menu.addAction(tr("Checkout as tracking branch..."));
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
          QLineEdit::Normal, QString(), &ok);
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
  if (!item || !m_stashesItem || item->parent() != m_stashesItem)
    return;

  const QString ref = item->data(0, Qt::UserRole).toString();
  if (ref.isEmpty())
    return;

  QMenu menu(this);
  auto *applyAction = menu.addAction(tr("Apply Stash"));
  auto *deleteAction = menu.addAction(tr("Delete Stash"));
  QAction *selected = menu.exec(m_repoPanel->viewport()->mapToGlobal(pos));

  if (selected == applyAction) {
    if (execGit(m_currentPath, {"stash", "apply", ref})) {
      loadWorkingTree();
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
    }
    parentItem = child;
  }
  QTreeWidgetItem *leaf =
      new QTreeWidgetItem(parentItem, QStringList{fileName});
  if (!status.isEmpty()) {
    leaf->setData(0, Qt::UserRole, status);
  }
}

void MainWindow::loadWorkingTree() {
  if (m_unstagedTree)
    m_unstagedTree->clear();
  if (m_stagedTree)
    m_stagedTree->clear();
  if (m_currentPath.isEmpty()) {
    return;
  }

  for (const QString &filePath :
       runGit(m_currentPath, {"diff", "--cached", "--name-only"})) {
    addFileToTree(m_stagedTree, filePath, QString());
  }

  for (const QString &filePath :
       runGit(m_currentPath, {"diff", "--name-only"})) {
    addFileToTree(m_unstagedTree, filePath, QString());
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
  m_commitButton->setEnabled(hasMessage && hasStaged);
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

  if (execGit(m_currentPath, {"commit", "-F", tempFile.fileName()})) {
    m_commitSubject->clear();
    m_commitBody->clear();
    loadRepository(m_currentPath);
    statusBar()->showMessage(tr("Committed"));
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
  if (menu.exec(m_stagedTree->mapToGlobal(pos)) == unstageAction) {
    if (execGit(m_currentPath, {"reset", "HEAD", "--", path})) {
      loadWorkingTree();
    }
  }
}

void MainWindow::onCommitSelected(QTableWidgetItem *item) {
  if (!item || !m_commitFilesTree)
    return;

  if (m_diffView)
    m_diffView->clear();

  const int row = item->row();
  QTableWidgetItem *shaItem = m_commitTable->item(row, 5);
  if (!shaItem)
    return;

  m_selectedCommitSha = shaItem->data(Qt::UserRole).toString();
  m_commitFilesTree->clear();

  for (const QString &line :
       runGit(m_currentPath, {"diff-tree", "--no-commit-id", "--name-status",
                              "-r", m_selectedCommitSha})) {
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

  const QStringList diff = runGit(m_currentPath, {"stash", "show", "-p", ref});
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
