#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCursor>
#include <QDebug>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QStatusBar>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

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

  const QString branchName =
      isLocal ? item->data(0, Qt::UserRole).toString()
              : item->parent()->text(0) + QLatin1Char('/') + item->text(0);
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
        statusBar()->showMessage(tr("There are no remotes to push to."), 0);
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
      if (execGitWithProgress(
              m_currentPath, {"push", "-u", remote, ref},
              tr("Pushing %1 to %2...").arg(branchName, remote))) {
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
        statusBar()->showMessage(tr("There are no remote branches to track."),
                                 0);
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
      showMergeDialog(branchName);
    } else if (selected == rebaseAction) {
      QStringList branches = m_gitExecutor->run(
          m_currentPath, {"branch", "--format=%(refname:short)"});
      branches.removeOne(branchName);
      if (branches.isEmpty()) {
        statusBar()->showMessage(
            tr("There are no other branches to rebase onto."), 0);
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
        statusBar()->showMessage(
            output.isEmpty() ? tr("Rebase failed") : output, 0);
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
          statusBar()->showMessage(tr("Tag names cannot contain spaces."), 0);
        } else {
          QString output;
          if (m_gitExecutor->exec(m_currentPath, {"tag", tagName}, &output)) {
            loadRepository(m_currentPath);
            statusBar()->showMessage(tr("Tag %1 created").arg(tagName));
          } else {
            statusBar()->showMessage(
                output.isEmpty() ? tr("Create tag failed") : output, 0);
          }
        }
      }
    }
  } else if (item->parent() == m_tagsItem) {
    const QString tagName = item->data(0, Qt::UserRole).toString();
    auto *checkoutAction = menu.addAction(tr("Ch&eckout Tag %1").arg(tagName));
    const QStringList remotes = m_gitExecutor->run(m_currentPath, {"remote"});
    QStringList pushableRemotes;
    for (const QString &remote : remotes) {
      if (m_gitExecutor
              ->run(m_currentPath, {"ls-remote", "--tags", remote, tagName})
              .isEmpty())
        pushableRemotes.append(remote);
    }
    auto *pushAction = pushableRemotes.isEmpty()
                           ? nullptr
                           : menu.addAction(tr("&Push Tag %1").arg(tagName));
    auto *deleteAction = menu.addAction(tr("&Delete Tag %1").arg(tagName));
    QAction *selected = menu.exec(m_repoPanel->viewport()->mapToGlobal(pos));

    if (selected == checkoutAction) {
      QString output;
      if (m_gitExecutor->exec(m_currentPath, {"checkout", tagName}, &output)) {
        loadRepository(m_currentPath);
        statusBar()->showMessage(tr("Checked out tag %1").arg(tagName));
      } else {
        statusBar()->showMessage(
            output.isEmpty() ? tr("Checkout failed") : output, 0);
      }
    } else if (pushAction && selected == pushAction) {
      if (pushableRemotes.isEmpty()) {
        statusBar()->showMessage(tr("There are no remotes to push to."), 0);
      } else {
        bool okRemote;
        const QString remote = QInputDialog::getItem(
            this, tr("Push Tag %1").arg(tagName), tr("Remote:"),
            pushableRemotes, 0, false, &okRemote);
        if (okRemote && !remote.isEmpty()) {
          if (execGitWithProgress(
                  m_currentPath, {"push", remote, tagName},
                  tr("Pushing tag %1 to %2...").arg(tagName, remote))) {
            statusBar()->showMessage(
                tr("Pushed tag %1 to %2").arg(tagName, remote));
          } else {
            statusBar()->showMessage(tr("Failed to push tag %1").arg(tagName));
          }
        }
      }
    } else if (selected == deleteAction) {
      QString localOutput;
      if (m_gitExecutor->exec(m_currentPath, {"tag", "-d", tagName},
                              &localOutput)) {
        const QStringList allRemotes =
            m_gitExecutor->run(m_currentPath, {"remote"});
        QStringList remotesWithTag;
        for (const QString &remote : allRemotes) {
          if (!m_gitExecutor
                   ->run(m_currentPath,
                         {"ls-remote", "--tags", remote, tagName})
                   .isEmpty())
            remotesWithTag.append(remote);
        }
        if (!remotesWithTag.isEmpty()) {
          QString remoteToDelete;
          if (remotesWithTag.size() == 1) {
            auto reply = QMessageBox::question(
                this, tr("Delete remote tag"),
                tr("Also delete tag %1 from remote %2?")
                    .arg(tagName, remotesWithTag.first()),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (reply == QMessageBox::Yes)
              remoteToDelete = remotesWithTag.first();
          } else {
            QStringList choices = remotesWithTag;
            choices.append(tr("None (local only)"));
            bool ok;
            const QString choice = QInputDialog::getItem(
                this, tr("Delete remote tag"),
                tr("Also delete tag %1 from remote:").arg(tagName), choices, 0,
                false, &ok);
            if (ok && !choice.isEmpty() && choice != tr("None (local only)")) {
              remoteToDelete = choice;
            }
          }
          if (!remoteToDelete.isEmpty()) {
            if (!execGitWithProgress(m_currentPath,
                                     {"push", remoteToDelete,
                                      QStringLiteral(":refs/tags/") + tagName},
                                     tr("Deleting tag %1 from %2...")
                                         .arg(tagName, remoteToDelete))) {
              statusBar()->showMessage(
                  tr("Failed to delete tag %1 from remote %2.")
                      .arg(tagName, remoteToDelete),
                  0);
            }
          }
        }
        loadRepository(m_currentPath);
        statusBar()->showMessage(tr("Tag %1 deleted").arg(tagName));
      } else {
        statusBar()->showMessage(
            localOutput.isEmpty() ? tr("Delete tag failed") : localOutput, 0);
      }
    }
  }
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
  if (!isFolder && hasTracked && !configuredDiffTool().isEmpty()) {
    externalDiffAction = menu.addAction(tr("O&pen in external diff tool"));
  }

  menu.addSeparator();
  const QString fullPath = m_currentPath + QLatin1Char('/') + path;
  QAction *openEditorAction = nullptr;
  if (!isFolder)
    openEditorAction = menu.addAction(tr("Open in &External Editor"));
  auto *openFolderAction = menu.addAction(tr("Open Containing &Folder"));
  auto *revertAction = menu.addAction(tr("&Revert to HEAD"));
  QAction *rmAction = nullptr;
  QAction *mvAction = nullptr;
  if (hasTracked) {
    rmAction = menu.addAction(tr("Git &rm"));
    mvAction = menu.addAction(tr("Git &mv"));
  }
  QAction *cleanAction = nullptr;
  if (hasNew)
    cleanAction = menu.addAction(tr("Git &clean"));

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
  } else if (selected == openEditorAction) {
    openInExternalEditor(fullPath);
  } else if (selected == openFolderAction) {
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(QFileInfo(fullPath).dir().absolutePath()));
  } else if (selected == revertAction) {
    bool ok = true;
    if (hasTracked)
      ok &=
          m_gitExecutor->exec(m_currentPath, {"checkout", "HEAD", "--", path});
    if (hasNew)
      ok &= m_gitExecutor->exec(m_currentPath, {"clean", "-fd", "--", path});
    if (ok) {
      loadWorkingTree();
      if (m_diffView)
        showEmptyDiff();
    }
  } else if (selected == rmAction) {
    const QStringList rmArgs = isFolder ? QStringList{"rm", "-rf", "--", path}
                                        : QStringList{"rm", "-f", "--", path};
    if (m_gitExecutor->exec(m_currentPath, rmArgs)) {
      loadWorkingTree();
      if (m_diffView)
        showEmptyDiff();
    }
  } else if (selected == mvAction) {
    bool ok;
    const QString newPath = QInputDialog::getText(
        this, tr("Git mv"), tr("New path:"), QLineEdit::Normal, path, &ok);
    if (ok && !newPath.isEmpty() && newPath != path) {
      if (m_gitExecutor->exec(m_currentPath, {"mv", "--", path, newPath})) {
        loadWorkingTree();
        if (m_diffView)
          showEmptyDiff();
      }
    }
  } else if (selected == cleanAction) {
    if (m_gitExecutor->exec(m_currentPath, {"clean", "-fd", "--", path})) {
      loadWorkingTree();
      if (m_diffView)
        showEmptyDiff();
    }
  }
}

void MainWindow::showUntrackedContextMenu(const QPoint &pos) {
  QTreeWidgetItem *item = m_untrackedTree->itemAt(pos);
  if (!item || m_currentPath.isEmpty())
    return;

  const bool isFolder = item->childCount() > 0;
  const QString path = m_untrackedTree->itemPath(item);

  QMenu menu(this);
  QAction *stageAction =
      menu.addAction(isFolder ? tr("&Stage folder") : tr("&Stage file"));
  QAction *ignoreAction = nullptr;
  if (isFolder)
    ignoreAction = menu.addAction(tr("&Ignore all files in this folder"));
  else
    ignoreAction = menu.addAction(tr("&Ignore"));
  QAction *cleanAction = menu.addAction(tr("Git &clean"));

  menu.addSeparator();
  const QString fullPath = m_currentPath + QLatin1Char('/') + path;
  auto *openFolderAction = menu.addAction(tr("Open Containing &Folder"));

  QAction *selected = menu.exec(m_untrackedTree->mapToGlobal(pos));
  if (!selected)
    return;

  if (selected == stageAction) {
    if (m_gitExecutor->exec(m_currentPath, {"add", path})) {
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
  } else if (selected == cleanAction) {
    if (m_gitExecutor->exec(m_currentPath, {"clean", "-fd", "--", path})) {
      loadWorkingTree();
      if (m_diffView)
        showEmptyDiff();
    }
  } else if (selected == openFolderAction) {
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(QFileInfo(fullPath).dir().absolutePath()));
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
  if (!isFolder && !configuredDiffTool().isEmpty()) {
    externalDiffAction = menu.addAction(tr("O&pen in external diff tool"));
  }

  menu.addSeparator();
  const QString fullPath = m_currentPath + QLatin1Char('/') + path;
  QAction *openEditorAction = nullptr;
  if (!isFolder)
    openEditorAction = menu.addAction(tr("Open in &External Editor"));
  auto *openFolderAction = menu.addAction(tr("Open Containing &Folder"));
  auto *revertAction = menu.addAction(tr("&Revert to HEAD"));
  auto *rmAction = menu.addAction(tr("Git &rm"));
  auto *mvAction = menu.addAction(tr("Git &mv"));

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
  } else if (selected == openEditorAction) {
    openInExternalEditor(fullPath);
  } else if (selected == openFolderAction) {
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(QFileInfo(fullPath).dir().absolutePath()));
  } else if (selected == revertAction) {
    if (m_gitExecutor->exec(m_currentPath, {"checkout", "HEAD", "--", path})) {
      loadWorkingTree();
      if (m_diffView)
        showEmptyDiff();
    }
  } else if (selected == rmAction) {
    const QStringList rmArgs = isFolder ? QStringList{"rm", "-rf", "--", path}
                                        : QStringList{"rm", "-f", "--", path};
    if (m_gitExecutor->exec(m_currentPath, rmArgs)) {
      loadWorkingTree();
      if (m_diffView)
        showEmptyDiff();
    }
  } else if (selected == mvAction) {
    bool ok;
    const QString newPath = QInputDialog::getText(
        this, tr("Git mv"), tr("New path:"), QLineEdit::Normal, path, &ok);
    if (ok && !newPath.isEmpty() && newPath != path) {
      if (m_gitExecutor->exec(m_currentPath, {"mv", "--", path, newPath})) {
        loadWorkingTree();
        if (m_diffView)
          showEmptyDiff();
      }
    }
  }
}

void MainWindow::showCommitContextMenu(const QPoint &pos) {
  QTableWidgetItem *item = m_commitTable->itemAt(pos);
  if (!item || m_currentPath.isEmpty())
    return;

  // Collect selected commit SHAs (in row order)
  QList<int> selectedRows;
  const auto selectedItems = m_commitTable->selectedItems();
  QSet<int> seenRows;
  for (const auto *sel : selectedItems) {
    const int r = sel->row();
    if (!seenRows.contains(r)) {
      seenRows.insert(r);
      selectedRows.append(r);
    }
  }
  std::sort(selectedRows.begin(), selectedRows.end());

  // Multi-select context menu (2+ commits)
  if (selectedRows.size() >= 2) {
    QStringList shas;
    for (int r : selectedRows) {
      QTableWidgetItem *si = m_commitTable->item(r, 7);
      if (si) {
        const QString s = si->data(Qt::UserRole).toString();
        if (!s.isEmpty())
          shas.append(s);
      }
    }
    if (shas.size() < 2)
      return;

    QMenu menu(this);
    auto *diffRangeAction =
        menu.addAction(tr("Compare &range (%1 commits)").arg(shas.size()));
    auto *cherryPickAllAction =
        menu.addAction(tr("Cherry-&pick %1 commits").arg(shas.size()));
    menu.addSeparator();
    auto *copyMenu = menu.addMenu(tr("Copy"));
    auto *copyShasAction = copyMenu->addAction(tr("All SHAs"));

    QAction *selected = menu.exec(QCursor::pos());
    if (!selected)
      return;

    if (selected == diffRangeAction) {
      // Diff from oldest^ to newest
      const QString oldest = shas.last();  // bottom of table = oldest
      const QString newest = shas.first(); // top of table = newest
      const QStringList diffLines =
          m_gitExecutor->run(m_currentPath, {"diff", oldest + "^", newest});
      if (diffLines.isEmpty()) {
        statusBar()->showMessage(tr("No differences in selected range."));
      } else {
        m_currentDiffLines = diffLines;
        m_currentDiffPath =
            tr("Range: %1..%2").arg(oldest.left(7), newest.left(7));
        if (m_viewTabWidget)
          m_viewTabWidget->setCurrentWidget(m_diffContainer);
        m_diffView->setHtml(m_diffPresenter->formatDiff(diffLines));
        if (m_diffDock)
          m_diffDock->setVisible(true);
        statusBar()->showMessage(
            tr("Diff: %1..%2").arg(oldest.left(7), newest.left(7)));
      }
      return;
    }

    if (selected == cherryPickAllAction) {
      // Cherry-pick in chronological order (oldest first = bottom to top)
      int successCount = 0;
      for (int i = shas.size() - 1; i >= 0; --i) {
        QString output;
        if (m_gitExecutor->exec(m_currentPath, {"cherry-pick", shas[i]},
                                &output)) {
          ++successCount;
        } else {
          const QStringList conflicted = m_gitExecutor->run(
              m_currentPath, {"diff", "--name-only", "--diff-filter=U"});
          if (!conflicted.isEmpty()) {
            loadRepository(m_currentPath);
            statusBar()->showMessage(
                tr("Cherry-picked %1/%2 commits — conflicts on %3")
                    .arg(successCount)
                    .arg(shas.size())
                    .arg(shas[i].left(7)));
            showConflictResolver(QStringLiteral("cherry-pick"));
            return;
          } else {
            statusBar()->showMessage(
                output.isEmpty()
                    ? tr("Cherry-pick failed at %1").arg(shas[i].left(7))
                    : output,
                0);
            return;
          }
        }
      }
      loadRepository(m_currentPath);
      statusBar()->showMessage(
          tr("Cherry-picked %1 commits successfully").arg(successCount));
      return;
    }

    if (selected == copyShasAction) {
      QApplication::clipboard()->setText(shas.join('\n'));
      return;
    }
    return;
  }

  const int row = item->row();
  QTableWidgetItem *shaItem = m_commitTable->item(row, 7);
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

  menu.addSeparator();
  auto *copyMenu = menu.addMenu(tr("Copy"));
  auto *copyFullShaAction = copyMenu->addAction(tr("Full SHA"));
  auto *copyShortShaAction = copyMenu->addAction(tr("Short SHA"));
  auto *copySubjectAction = copyMenu->addAction(tr("Subject"));
  auto *copyAuthorAction = copyMenu->addAction(tr("Author"));

  auto *undoLastCommitAction =
      sha == m_localHeadSha ? menu.addAction(tr("&Undo last commit (soft)"))
                            : nullptr;

  qDebug() << "showCommitContextMenu pos=" << pos
           << "tableGlobal=" << m_commitTable->mapToGlobal(pos)
           << "viewportGlobal=" << m_commitTable->viewport()->mapToGlobal(pos)
           << "cursorGlobal=" << QCursor::pos() << "sha=" << sha;
  QAction *selected = menu.exec(QCursor::pos());
  qDebug() << "showCommitContextMenu selected=" << selected;
  if (selected) {
    qDebug() << "  selected text=" << selected->text();
  }

  if (selected == copyFullShaAction) {
    QApplication::clipboard()->setText(sha);
    return;
  }
  if (selected == copyShortShaAction) {
    QApplication::clipboard()->setText(shaItem->text());
    return;
  }
  if (selected == copySubjectAction) {
    QApplication::clipboard()->setText(
        shaItem->data(Qt::UserRole + 1).toString());
    return;
  }
  if (selected == copyAuthorAction) {
    QApplication::clipboard()->setText(
        shaItem->data(Qt::UserRole + 2).toString());
    return;
  }

  if (selected == checkoutAction) {
    QString output;
    if (m_gitExecutor->exec(m_currentPath, {"checkout", sha}, &output)) {
      loadRepository(m_currentPath);
      statusBar()->showMessage(tr("Checked out %1").arg(sha.left(7)));
    } else {
      statusBar()->showMessage(
          output.isEmpty() ? tr("Checkout failed") : output, 0);
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
      statusBar()->showMessage(tr("Branch names cannot contain spaces."), 0);
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
      statusBar()->showMessage(
          tr("Selected commit has no previous commit to combine with."), 0);
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
      statusBar()->showMessage(
          QString::fromLocal8Bit(p.readAllStandardError() +
                                 p.readAllStandardOutput()),
          0);
    }
    return;
  }

  if (selected == resetAction) {
    resetToCommit(sha);
    return;
  }

  if (selected == savePatchAction) {
    createPatchFromCommit(sha);
    return;
  }

  if (undoLastCommitAction && selected == undoLastCommitAction) {
    undoLastCommit();
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
    statusBar()->showMessage(tr("Tag names cannot contain spaces."), 0);
    return;
  }

  QString output;
  if (m_gitExecutor->exec(m_currentPath, {"tag", tagName, sha}, &output)) {
    loadRepository(m_currentPath);
    statusBar()->showMessage(tr("Tag %1 created").arg(tagName));
  } else {
    statusBar()->showMessage(
        output.isEmpty() ? tr("Create tag failed") : output, 0);
  }
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
      !configuredDiffTool().isEmpty()
          ? menu.addAction(tr("&View diff in external diff tool"))
          : nullptr;
  if (externalDiffAction)
    menu.addSeparator();
  auto *blameAction = menu.addAction(tr("&Blame"));
  auto *historyAction = menu.addAction(tr("File &History"));
  auto *checkoutFileAction = menu.addAction(tr("Checkout this &version"));
  menu.addSeparator();
  const QString fullPath = m_currentPath + QLatin1Char('/') + path;
  auto *openEditorAction = menu.addAction(tr("Open in &External Editor"));
  auto *openFolderAction = menu.addAction(tr("Open Containing &Folder"));
  auto *selected = menu.exec(m_commitFilesTree->mapToGlobal(pos));

  if (externalDiffAction && selected == externalDiffAction) {
    launchGitTool({QStringLiteral("difftool"), QStringLiteral("-y"),
                   m_selectedCommitSha + QLatin1Char('^'), m_selectedCommitSha,
                   QStringLiteral("--"), path});
  } else if (selected == blameAction) {
    showBlame(path, m_selectedCommitSha);
  } else if (selected == historyAction) {
    showFileHistory(path);
  } else if (selected == openEditorAction) {
    openInExternalEditor(fullPath);
  } else if (selected == checkoutFileAction) {
    if (m_gitExecutor->exec(m_currentPath,
                            {"checkout", m_selectedCommitSha, "--", path})) {
      loadWorkingTree();
      statusBar()->showMessage(
          tr("Checked out %1 from %2").arg(path, m_selectedCommitSha.left(7)));
    }
  } else if (selected == openFolderAction) {
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(QFileInfo(fullPath).dir().absolutePath()));
  }
}

void MainWindow::showCommitTableHeaderContextMenu(const QPoint &pos) {
  if (!m_commitTable)
    return;

  QMenu menu(this);
  for (int c = 0; c < m_commitTable->columnCount(); ++c) {
    QTableWidgetItem *header = m_commitTable->horizontalHeaderItem(c);
    const QString text = header ? header->text() : QString::number(c);
    QAction *action = menu.addAction(text);
    action->setCheckable(true);
    action->setChecked(!m_commitTable->isColumnHidden(c));
    connect(action, &QAction::triggered, this, [this, c](bool checked) {
      m_commitTable->setColumnHidden(c, !checked);
    });
  }
  menu.exec(m_commitTable->horizontalHeader()->viewport()->mapToGlobal(pos));
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
      statusBar()->showMessage(tr("Cannot remove the currently open worktree."),
                               0);
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
