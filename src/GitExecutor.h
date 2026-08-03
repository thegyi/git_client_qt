#ifndef GITEXECUTOR_H
#define GITEXECUTOR_H

#include <QObject>
#include <QStringList>

class QString;

class GitExecutor : public QObject {
  Q_OBJECT

public:
  explicit GitExecutor(QObject *parent = nullptr);

  QStringList run(const QString &path, const QStringList &args,
                  int acceptedExitCode = 0) const;
  bool exec(const QString &path, const QStringList &args,
            QString *output = nullptr) const;
};

#endif // GITEXECUTOR_H
