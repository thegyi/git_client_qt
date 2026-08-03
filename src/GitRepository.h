#ifndef GITREPOSITORY_H
#define GITREPOSITORY_H

#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>

class GitExecutor;

class GitRepository : public QObject {
  Q_OBJECT

public:
  explicit GitRepository(GitExecutor *executor, QObject *parent = nullptr);

  void setPath(const QString &path);
  QString path() const;

  bool isValid() const;
  QString root() const;
  QString stateSignature() const;

  QList<QPair<QString, QString>> stashes() const;
  QList<QPair<QString, QString>> remotes() const;
  QStringList worktrees() const;

private:
  GitExecutor *m_executor = nullptr;
  QString m_path;
};

#endif // GITREPOSITORY_H
