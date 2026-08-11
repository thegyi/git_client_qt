#ifndef GITEXECUTOR_H
#define GITEXECUTOR_H

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QStringList>

class QProcess;
class QString;

class GitExecutor : public QObject {
  Q_OBJECT

public:
  explicit GitExecutor(QObject *parent = nullptr);

  QStringList run(const QString &path, const QStringList &args,
                  int acceptedExitCode = 0);
  bool exec(const QString &path, const QStringList &args,
            QString *output = nullptr);
  QByteArray raw(const QString &path, const QStringList &args);
  void cancel();

signals:
  void commandLogged(const QString &command, const QString &output,
                     int exitCode);
  void commandStarted(const QString &command);
  void commandFinished(const QString &command, int exitCode, bool ok);

private:
  struct CommandResult {
    bool ok = false;
    int exitCode = -1;
    QByteArray standardOutput;
    QByteArray standardError;
    QString allOutput;
  };
  CommandResult runCommand(const QString &path, const QStringList &args,
                           int acceptedExitCode = 0);

  QPointer<QProcess> m_currentProcess;
  bool m_commandRunning = false;
};

#endif // GITEXECUTOR_H
