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
                  int acceptedExitCode = 0);
  bool exec(const QString &path, const QStringList &args,
            QString *output = nullptr);

signals:
  void commandLogged(const QString &command, const QString &output,
                     int exitCode);
};

#endif // GITEXECUTOR_H
