#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QProcess>
#include <QRegularExpression>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTemporaryFile>
#include <QTextStream>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

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

void MainWindow::diffWithCommit(const QString &fromSha) {
  if (!m_commitTable || !m_diffView || m_currentPath.isEmpty())
    return;

  QStringList items;
  QStringList shas;
  for (int row = 0; row < m_commitTable->rowCount(); ++row) {
    QTableWidgetItem *shaItem = m_commitTable->item(row, 7);
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
    if (m_viewTabWidget)
      m_viewTabWidget->setCurrentWidget(m_diffContainer);
  }
  m_diffView->setHtml(m_diffPresenter->formatDiff(diff));
  statusBar()->showMessage(
      tr("Diff between %1 and %2").arg(fromSha.left(7), toSha.left(7)));
}

void MainWindow::onCommitFileClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column)
  if (!item || m_currentPath.isEmpty() || m_selectedCommitSha.isEmpty())
    return;

  const bool isFolder = item->childCount() > 0;
  if (isFolder)
    return;

  const QString path = m_commitFilesTree->itemPath(item);
  if (!m_currentPath.isEmpty())
    m_repoCommitFile[m_currentPath] = path;
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
    if (m_viewTabWidget)
      m_viewTabWidget->setCurrentWidget(m_diffContainer);
    m_diffView->setHtml(html);
  }
}

void MainWindow::diffWithRemote() {
  if (m_currentPath.isEmpty()) {
    statusBar()->showMessage(tr("Open a repository first."), 0);
    return;
  }

  if (m_remoteHeadSha.isEmpty()) {
    statusBar()->showMessage(tr("No upstream/remote HEAD is set."), 0);
    return;
  }

  const QStringList diff = m_gitExecutor->run(
      m_currentPath,
      {"diff", m_remoteHeadSha + QLatin1String(".."), m_localHeadSha});
  if (m_diffView) {
    if (diff.isEmpty()) {
      if (m_diffDock)
        m_diffDock->setVisible(false);
      m_diffView->showEmpty(tr("No diff"),
                            tr("Local and remote HEAD are the same."));
    } else {
      if (m_diffDock)
        if (m_viewTabWidget)
          m_viewTabWidget->setCurrentWidget(m_diffContainer);
      m_diffView->setHtml(m_diffPresenter->formatDiff(diff));
    }
  }
  statusBar()->showMessage(
      tr("Diff: %1..%2").arg(m_remoteHeadSha.left(7), m_localHeadSha.left(7)));
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

void MainWindow::onFileClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column)
  if (!item || m_currentPath.isEmpty()) {
    return;
  }

  auto *tree = qobject_cast<FileTreeWidget *>(item->treeWidget());
  if (!tree) {
    return;
  }

  const QString path = tree->itemPath(item);
  if (tree == m_unstagedTree)
    m_repoUnstagedFile[m_currentPath] = path;
  else if (tree == m_stagedTree)
    m_repoStagedFile[m_currentPath] = path;
  const bool staged = (tree == m_stagedTree);
  const bool isFolder = item->childCount() > 0;
  const bool isNew =
      isFolder ? false : (item->data(0, Qt::UserRole).toString() == "?");

  if (!isFolder && isImageFile(path)) {
    showImageDiff(path, staged, isNew);
    return;
  }

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
        if (m_viewTabWidget)
          m_viewTabWidget->setCurrentWidget(m_diffContainer);
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
        if (m_viewTabWidget)
          m_viewTabWidget->setCurrentWidget(m_diffContainer);
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
    if (m_viewTabWidget)
      m_viewTabWidget->setCurrentWidget(m_diffContainer);
    m_diffView->setHtml(html);
  }
}

bool MainWindow::isImageFile(const QString &path) const {
  static const QRegularExpression imageRe(
      QStringLiteral("\\.(png|jpg|jpeg|gif|bmp|webp)$"),
      QRegularExpression::CaseInsensitiveOption);
  return imageRe.match(path).hasMatch();
}

void MainWindow::showImageDiff(const QString &path, bool staged, bool isNew) {
  if (!m_diffView || !m_gitExecutor || m_currentPath.isEmpty())
    return;

  QImage oldImage;
  QImage newImage;

  if (staged) {
    if (!isNew)
      oldImage = QImage::fromData(
          m_gitExecutor->raw(m_currentPath, {"show", "HEAD:" + path}));
    newImage = QImage::fromData(
        m_gitExecutor->raw(m_currentPath, {"show", ":0:" + path}));
  } else {
    if (!isNew)
      oldImage = QImage::fromData(
          m_gitExecutor->raw(m_currentPath, {"show", "HEAD:" + path}));
    const QString fullPath = m_currentPath + QLatin1Char('/') + path;
    QFile f(fullPath);
    if (f.open(QIODevice::ReadOnly))
      newImage = QImage::fromData(f.readAll());
  }

  if (oldImage.isNull() && newImage.isNull()) {
    m_diffView->showEmpty(tr("No diff"), tr("Could not load image content."));
    return;
  }

  const int maxW =
      qMax(160, m_diffView->width() > 0 ? m_diffView->width() / 2 - 24 : 400);
  if (!oldImage.isNull())
    oldImage = oldImage.scaled(maxW, maxW, Qt::KeepAspectRatio,
                               Qt::SmoothTransformation);
  if (!newImage.isNull())
    newImage = newImage.scaled(maxW, maxW, Qt::KeepAspectRatio,
                               Qt::SmoothTransformation);

  m_diffView->document()->addResource(QTextDocument::ImageResource,
                                      QUrl(QStringLiteral("diff://old")),
                                      oldImage);
  m_diffView->document()->addResource(QTextDocument::ImageResource,
                                      QUrl(QStringLiteral("diff://new")),
                                      newImage);

  const QString oldCell = oldImage.isNull()
                              ? tr("(no old image)")
                              : QStringLiteral("<img src=\"diff://old\" />");
  const QString newCell = newImage.isNull()
                              ? tr("(no new image)")
                              : QStringLiteral("<img src=\"diff://new\" />");

  const QString html =
      QStringLiteral("<html>"
                     "<body style=\"background-color:#1e1e1e; color:#cccccc; "
                     "font-family:sans-serif; padding:8px;\">"
                     "<table width=\"100%\" height=\"100%\"><tr>"
                     "<td align=\"center\" valign=\"top\" width=\"50%\">%1</td>"
                     "<td align=\"center\" valign=\"top\" width=\"50%\">%2</td>"
                     "</tr></table></body></html>")
          .arg(oldCell, newCell);

  if (m_viewTabWidget)
    m_viewTabWidget->setCurrentWidget(m_diffContainer);
  m_diffView->setHtml(html);
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
