#include "WorkingTreeModel.h"

WorkingTreeModel::WorkingTreeModel(QObject *parent) : QObject(parent) {}

void WorkingTreeModel::load(const QStringList &staged,
                            const QStringList &unstaged,
                            const QStringList &untracked) {
  m_staged.clear();
  m_unstaged.clear();
  m_untracked.clear();

  for (const QString &line : staged) {
    const QStringList parts = line.split(QLatin1Char('\t'));
    if (parts.isEmpty())
      continue;
    m_staged.append({parts.last(), parts.first().left(1)});
  }

  for (const QString &line : unstaged) {
    const QStringList parts = line.split(QLatin1Char('\t'));
    if (parts.isEmpty())
      continue;
    m_unstaged.append({parts.last(), parts.first().left(1)});
  }

  for (const QString &filePath : untracked) {
    m_untracked.append({filePath, QStringLiteral("?")});
  }
}

void WorkingTreeModel::clear() {
  m_staged.clear();
  m_unstaged.clear();
  m_untracked.clear();
}

const QList<FileStatus> &WorkingTreeModel::stagedFiles() const {
  return m_staged;
}

const QList<FileStatus> &WorkingTreeModel::unstagedFiles() const {
  return m_unstaged;
}

const QList<FileStatus> &WorkingTreeModel::untrackedFiles() const {
  return m_untracked;
}
