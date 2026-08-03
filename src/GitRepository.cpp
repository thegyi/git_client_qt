#include "GitRepository.h"
#include "GitExecutor.h"

GitRepository::GitRepository(GitExecutor *executor, QObject *parent)
    : QObject(parent), m_executor(executor) {}

void GitRepository::setPath(const QString &path) { m_path = path; }

QString GitRepository::path() const { return m_path; }

bool GitRepository::isValid() const {
  if (m_path.isEmpty())
    return false;
  return !m_executor->run(m_path, {"rev-parse", "--git-dir"}).isEmpty();
}

QString GitRepository::root() const {
  return m_executor->run(m_path, {"rev-parse", "--show-toplevel"}).value(0);
}

QString GitRepository::stateSignature() const {
  const QString head = m_executor->run(m_path, {"rev-parse", "HEAD"}).value(0);
  const QString upstream =
      m_executor->run(m_path, {"rev-parse", "@{u}"}).value(0);
  const QString status =
      m_executor->run(m_path, {"status", "--porcelain"}).join('\n');
  const QString tags =
      m_executor->run(m_path, {"tag", "--list"}).join('\n');
  return head + '|' + upstream + '|' + status + '|' + tags;
}

QList<QPair<QString, QString>> GitRepository::stashes() const {
  QList<QPair<QString, QString>> result;
  for (const QString &line : m_executor->run(m_path, {"stash", "list"})) {
    const int colon = line.indexOf(':');
    if (colon < 0)
      continue;
    const QString ref = line.left(colon).trimmed();
    const QString msg = line.mid(colon + 1).trimmed();
    if (ref.isEmpty())
      continue;
    result.append({ref, msg});
  }
  return result;
}

QList<QPair<QString, QString>> GitRepository::remotes() const {
  QList<QPair<QString, QString>> result;
  for (const QString &line : m_executor->run(m_path, {"remote", "-v"})) {
    const QStringList parts = line.split('\t');
    if (parts.size() < 2)
      continue;
    const QString name = parts.at(0);
    const QString rest = parts.at(1);
    if (rest.endsWith(QLatin1String(" (push)")))
      continue;
    const QString url = rest.section(' ', 0, -2);
    result.append({name, url});
  }
  return result;
}

QStringList GitRepository::worktrees() const {
  QStringList result;
  for (const QString &line : m_executor->run(m_path, {"worktree", "list"})) {
    const QString path = line.section(' ', 0, 0).trimmed();
    if (!path.isEmpty())
      result.append(path);
  }
  return result;
}
