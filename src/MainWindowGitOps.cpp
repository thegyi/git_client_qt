#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QCursor>
#include <QDebug>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTemporaryFile>
#include <QTextEdit>
#include <QTextStream>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVBoxLayout>
#include <QVersionNumber>

bool MainWindow::performPush(const QStringList &extraArgs) {
  if (m_currentPath.isEmpty())
    return false;

  const QString currentBranch = currentBranchName();
  if (currentBranch.isEmpty()) {
    statusBar()->showMessage(tr("Could not determine the current branch."), 0);
    return false;
  }

  const QString remote =
      m_gitExecutor
          ->run(
              m_currentPath,
              {"config", QStringLiteral("branch.%1.remote").arg(currentBranch)})
          .value(0);
  const QString pushRemote =
      remote.isEmpty() ? QStringLiteral("origin") : remote;

  QStringList args = {QStringLiteral("push")};
  args << extraArgs;
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
    statusBar()->showMessage(tr("Could not start git process"), 0);
    return false;
  }

  progress.exec();

  if (canceled) {
    statusBar()->showMessage(tr("Push canceled"));
    return false;
  }

  if (p.exitCode() == 0) {
    if (!remote.isEmpty())
      m_gitExecutor->exec(m_currentPath, {QStringLiteral("fetch"), remote});
    loadRepository(m_currentPath);
    statusBar()->showMessage(tr("Push finished"));
    if (!output.isEmpty()) {
      statusBar()->showMessage(
          tr("Push completed successfully: %1").arg(output), 0);
    }
    return true;
  }

  statusBar()->showMessage(output.isEmpty() ? tr("Git push failed.") : output,
                           0);
  return false;
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

  const bool amending = m_amendCheckBox && m_amendCheckBox->isChecked();

  // Amending an unpushed commit is a purely local operation, so it needs no
  // confirmation. Amending a pushed commit rewrites published history, so the
  // user has to choose how to deal with the divergence up front rather than
  // discovering it when the next push is rejected.
  AmendStrategy strategy = AmendStrategy::LocalOnly;
  if (amending && isHeadPushed()) {
    strategy = askAmendStrategy();
    if (strategy == AmendStrategy::Cancel)
      return;
  }

  if (strategy == AmendStrategy::Fixup) {
    if (!m_gitExecutor->exec(m_currentPath, {QStringLiteral("commit"),
                                             QStringLiteral("--fixup=HEAD")})) {
      statusBar()->showMessage(tr("Creating fixup commit failed"));
      return;
    }
    if (m_amendCheckBox)
      m_amendCheckBox->setChecked(false);
    m_commitSubject->clear();
    m_commitBody->clear();
    m_commitSubjectDraft.clear();
    m_commitBodyDraft.clear();
    loadRepository(m_currentPath);
    statusBar()->showMessage(
        tr("Created fixup commit. Squash it later with rebase --autosquash."));
    return;
  }

  QTemporaryFile tempFile;
  if (!tempFile.open()) {
    statusBar()->showMessage(tr("Failed to create commit message file"));
    return;
  }
  tempFile.write(message.toUtf8());
  tempFile.close();

  const QStringList commitArgs =
      amending ? QStringList{"commit", "--amend", "-F", tempFile.fileName()}
               : QStringList{"commit", "-F", tempFile.fileName()};
  if (!m_gitExecutor->exec(m_currentPath, commitArgs)) {
    statusBar()->showMessage(tr("Commit failed"));
    return;
  }

  m_commitSubject->clear();
  m_commitBody->clear();
  m_commitSubjectDraft.clear();
  m_commitBodyDraft.clear();
  if (m_amendCheckBox)
    m_amendCheckBox->setChecked(false);
  loadRepository(m_currentPath);
  statusBar()->showMessage(amending ? tr("Amended") : tr("Committed"));

  // A one-shot lease push: never mutate the sticky push mode, so later pushes
  // stay on whatever the user actually selected in the Push dropdown.
  if (strategy == AmendStrategy::ForceWithLease)
    performPush({QStringLiteral("--force-with-lease")});
}

MainWindow::AmendStrategy MainWindow::askAmendStrategy() {
  const QString branch = currentBranchName();
  const bool blocked = isProtectedBranch(branch);

  QMessageBox box(this);
  box.setIcon(QMessageBox::Warning);
  box.setWindowTitle(tr("Amend a pushed commit"));
  box.setText(tr("The last commit has already been pushed."));
  if (blocked) {
    box.setInformativeText(
        tr("Amending rewrites published history. Branch '%1' is protected, so "
           "force pushing is not offered. Create a fixup commit instead, or "
           "amend locally and resolve the divergence yourself.")
            .arg(branch));
  } else {
    box.setInformativeText(
        tr("Amending rewrites published history and the remote will reject a "
           "normal push. Choose how to proceed."));
  }

  QPushButton *leaseButton = nullptr;
  if (!blocked) {
    leaseButton = box.addButton(tr("Amend and force push with lease"),
                                QMessageBox::AcceptRole);
  }
  // A fixup commit needs staged content; with nothing staged the amend can
  // only be a message edit, so the option would always fail.
  const bool hasStaged = m_stagedTree && m_stagedTree->topLevelItemCount() > 0;
  QPushButton *fixupButton = nullptr;
  if (hasStaged) {
    fixupButton = box.addButton(tr("Create fixup commit instead"),
                                QMessageBox::ActionRole);
  }
  auto *localButton =
      box.addButton(tr("Amend locally only"), QMessageBox::ActionRole);
  auto *cancelButton = box.addButton(QMessageBox::Cancel);
  box.setDefaultButton(blocked ? (fixupButton ? fixupButton : localButton)
                               : leaseButton);

  box.exec();

  QAbstractButton *clicked = box.clickedButton();
  if (clicked == cancelButton || clicked == nullptr)
    return AmendStrategy::Cancel;
  if (clicked == fixupButton)
    return AmendStrategy::Fixup;
  if (clicked == localButton)
    return AmendStrategy::LocalOnly;
  return AmendStrategy::ForceWithLease;
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

void MainWindow::undoLastCommit() {
  if (m_currentPath.isEmpty()) {
    statusBar()->showMessage(tr("Open a repository first."), 0);
    return;
  }

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
}

void MainWindow::checkForUpdates() {
  if (!m_networkManager)
    m_networkManager = new QNetworkAccessManager(this);

  const QString url =
      QStringLiteral("https://api.github.com/repos/thegyi/git_client_qt/"
                     "releases/latest");
  QNetworkRequest request{QUrl(url)};
  request.setHeader(QNetworkRequest::UserAgentHeader,
                    QStringLiteral("GitClientQt"));
  request.setRawHeader(QByteArrayLiteral("Accept"),
                       QByteArrayLiteral("application/vnd.github+json"));

  QNetworkReply *reply = m_networkManager->get(request);
  statusBar()->showMessage(tr("Checking for updates..."));

  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    statusBar()->clearMessage();

    if (reply->error() != QNetworkReply::NoError) {
      statusBar()->showMessage(reply->errorString().isEmpty()
                                   ? tr("Update check failed")
                                   : reply->errorString(),
                               0);
      reply->deleteLater();
      return;
    }

    const QJsonObject release =
        QJsonDocument::fromJson(reply->readAll()).object();
    const QString tag = release.value(QStringLiteral("tag_name")).toString();
    const QString htmlUrl =
        release.value(QStringLiteral("html_url")).toString();
    const QString latestVersion =
        (tag.startsWith(QLatin1Char('v')) || tag.startsWith(QLatin1Char('V')))
            ? tag.mid(1)
            : tag;
    reply->deleteLater();

    if (latestVersion.isEmpty()) {
      statusBar()->showMessage(
          tr("Could not parse the latest release version."), 0);
      return;
    }

    const QVersionNumber current =
        QVersionNumber::fromString(QStringLiteral(APP_VERSION));
    const QVersionNumber latest = QVersionNumber::fromString(latestVersion);

    if (latest > current) {
      if (QMessageBox::information(
              this, tr("Update available"),
              tr("Version %1 is available.\n\nCurrent version: %2")
                  .arg(tag, QStringLiteral(APP_VERSION)),
              QMessageBox::Open | QMessageBox::Close,
              QMessageBox::Close) == QMessageBox::Open) {
        QDesktopServices::openUrl(QUrl(htmlUrl));
      }
    } else if (latest == current) {
      statusBar()->showMessage(tr("You are running the latest version (%1).")
                                   .arg(QStringLiteral(APP_VERSION)),
                               0);
    } else {
      statusBar()->showMessage(
          tr("You are running a newer or development version (%1).")
              .arg(QStringLiteral(APP_VERSION)),
          0);
    }
  });
}

void MainWindow::onInitRepository() {
  const QString path =
      QFileDialog::getExistingDirectory(this, tr("Initialize Repository"));
  if (path.isEmpty())
    return;

  if (m_gitExecutor->exec(path, {"rev-parse", "--git-dir"})) {
    statusBar()->showMessage(
        tr("The selected folder is already a Git repository."), 0);
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
    statusBar()->showMessage(tr("The destination folder already exists."), 0);
    return;
  }

  if (execGitWithProgress(parentDir, {"clone", url, repoName},
                          tr("Cloning %1...").arg(repoName))) {
    loadRepository(localPath);
    statusBar()->showMessage(tr("Cloned %1").arg(repoName));
  } else {
    statusBar()->showMessage(tr("Clone failed"));
  }
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
    statusBar()->showMessage(
        output.isEmpty() ? tr("Cherry-pick failed") : output, 0);
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
    statusBar()->showMessage(output.isEmpty() ? tr("Revert failed") : output,
                             0);
    return;
  }

  showConflictResolver(QStringLiteral("revert"));
}

void MainWindow::resetToCommit(const QString &sha) {
  if (m_currentPath.isEmpty() || sha.isEmpty())
    return;

  const QStringList logLines = m_gitExecutor->run(
      m_currentPath, {"log", "-1", "--pretty=format:%h %s", sha});
  const QString commitLabel =
      logLines.isEmpty() ? sha.left(7) : logLines.first();

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Reset to %1").arg(sha.left(7)));
  auto *layout = new QVBoxLayout(&dlg);

  auto *infoLabel =
      new QLabel(tr("Reset the current branch to:\n%1").arg(commitLabel), &dlg);
  infoLabel->setWordWrap(true);
  layout->addWidget(infoLabel);

  layout->addSpacing(8);

  auto *modeGroup = new QGroupBox(tr("Reset mode"), &dlg);
  auto *modeLayout = new QVBoxLayout(modeGroup);

  auto *softRadio = new QRadioButton(tr("Soft"), modeGroup);
  auto *softDesc =
      new QLabel(tr("Keep all changes staged (index and working tree "
                    "unchanged)."),
                 modeGroup);
  softDesc->setWordWrap(true);
  softDesc->setContentsMargins(20, 0, 0, 4);
  softDesc->setStyleSheet(QStringLiteral("color: gray; font-size: 11px;"));

  auto *mixedRadio = new QRadioButton(tr("Mixed (default)"), modeGroup);
  auto *mixedDesc = new QLabel(
      tr("Unstage changes but keep them in the working tree."), modeGroup);
  mixedDesc->setWordWrap(true);
  mixedDesc->setContentsMargins(20, 0, 0, 4);
  mixedDesc->setStyleSheet(QStringLiteral("color: gray; font-size: 11px;"));

  auto *hardRadio = new QRadioButton(tr("Hard"), modeGroup);
  auto *hardDesc = new QLabel(
      tr("Discard all changes — index and working tree are reset."), modeGroup);
  hardDesc->setWordWrap(true);
  hardDesc->setContentsMargins(20, 0, 0, 4);
  hardDesc->setStyleSheet(QStringLiteral("color: gray; font-size: 11px;"));

  auto *hardWarning = new QLabel(
      tr("⚠ This will permanently discard uncommitted changes!"), modeGroup);
  hardWarning->setWordWrap(true);
  hardWarning->setContentsMargins(20, 0, 0, 0);
  hardWarning->setStyleSheet(
      QStringLiteral("color: red; font-weight: bold; font-size: 11px;"));
  hardWarning->setVisible(false);

  modeLayout->addWidget(softRadio);
  modeLayout->addWidget(softDesc);
  modeLayout->addWidget(mixedRadio);
  modeLayout->addWidget(mixedDesc);
  modeLayout->addWidget(hardRadio);
  modeLayout->addWidget(hardDesc);
  modeLayout->addWidget(hardWarning);
  layout->addWidget(modeGroup);

  mixedRadio->setChecked(true);

  connect(hardRadio, &QRadioButton::toggled, hardWarning, &QLabel::setVisible);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  buttons->button(QDialogButtonBox::Ok)->setText(tr("Reset"));
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() != QDialog::Accepted)
    return;

  QString mode;
  if (softRadio->isChecked())
    mode = QStringLiteral("soft");
  else if (hardRadio->isChecked())
    mode = QStringLiteral("hard");
  else
    mode = QStringLiteral("mixed");

  QString output;
  if (m_gitExecutor->exec(m_currentPath,
                          {"reset", QStringLiteral("--%1").arg(mode), sha},
                          &output)) {
    loadRepository(m_currentPath);
    statusBar()->showMessage(tr("Reset --%1 to %2").arg(mode, sha.left(7)));
  } else {
    statusBar()->showMessage(output.isEmpty() ? tr("Reset failed") : output, 0);
  }
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
    statusBar()->showMessage(tr("Both bad and good commits are required."), 0);
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
  if (execGitWithProgress(m_currentPath, {"lfs", "pull"},
                          tr("Pulling LFS objects..."))) {
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
    statusBar()->showMessage(
        tr("No upstream configured for the current branch."), 0);
    return;
  }

  if (execGitWithProgress(m_currentPath, {"lfs", "push", remote, currentBranch},
                          tr("Pushing LFS objects to %1...").arg(remote))) {
    statusBar()->showMessage(tr("LFS objects pushed"));
  } else {
    statusBar()->showMessage(tr("Failed to push LFS objects"));
  }
}

void MainWindow::initSubmodules() {
  if (m_currentPath.isEmpty()) {
    statusBar()->showMessage(tr("Open a repository first."), 0);
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
    statusBar()->showMessage(tr("Open a repository first."), 0);
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
    statusBar()->showMessage(tr("Open a repository first."), 0);
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
    statusBar()->showMessage(tr("Open a repository first."), 0);
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
    statusBar()->showMessage(tr("Open a repository first."), 0);
    return;
  }

  const QStringList configLines = m_gitExecutor->run(
      m_currentPath, {"config", "--file", QStringLiteral(".gitmodules"),
                      "--get-regexp", "^submodule\\..*\\.path$"});
  if (configLines.isEmpty()) {
    statusBar()->showMessage(tr("This repository has no submodules."), 0);
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

void MainWindow::applyPatch() {
  if (m_currentPath.isEmpty()) {
    statusBar()->showMessage(tr("Open a repository first."), 0);
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
    statusBar()->showMessage(tr("Could not write patch file."), 0);
  }
}

bool MainWindow::execGitWithProgress(const QString &path,
                                     const QStringList &args,
                                     const QString &label, QString *output) {
  QProgressDialog progress(label, tr("Cancel"), 0, 0, this);
  progress.setWindowModality(Qt::WindowModal);
  progress.setMinimumDuration(0);

  QProcess p;
  p.setWorkingDirectory(path);
  QString allOutput;
  bool canceled = false;

  connect(&p, &QProcess::readyReadStandardOutput, this, [&]() {
    allOutput += QString::fromLocal8Bit(p.readAllStandardOutput());
  });
  connect(&p, &QProcess::readyReadStandardError, this, [&]() {
    allOutput += QString::fromLocal8Bit(p.readAllStandardError());
  });
  connect(&p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          &progress, &QProgressDialog::close);
  connect(&p, &QProcess::errorOccurred, &progress, &QProgressDialog::close);
  connect(&progress, &QProgressDialog::canceled, &p, &QProcess::kill);
  connect(&progress, &QProgressDialog::canceled, this,
          [&]() { canceled = true; });

  p.start(QStringLiteral("git"), args);
  if (!p.waitForStarted(5000)) {
    if (output)
      *output = tr("Could not start git process");
    return false;
  }

  progress.exec();

  if (canceled) {
    if (output)
      *output = tr("Canceled");
    return false;
  }

  if (output)
    *output = allOutput;
  return p.exitCode() == 0;
}
