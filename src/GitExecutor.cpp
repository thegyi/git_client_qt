#include "GitExecutor.h"

#include <QEventLoop>
#include <QProcess>
#include <QString>

GitExecutor::GitExecutor(QObject *parent)
    : QObject(parent), m_commandRunning(false) {}

GitExecutor::CommandResult GitExecutor::runCommand(const QString &path,
                                                   const QStringList &args,
                                                   int acceptedExitCode) {
  if (m_commandRunning) {
    CommandResult res;
    res.allOutput = tr("Git command already running");
    res.exitCode = -1;
    emit commandLogged(args.join(QLatin1Char(' ')), res.allOutput, -1);
    return res;
  }
  m_commandRunning = true;

  CommandResult res;
  QProcess p;
  m_currentProcess = &p;
  const QString command = QStringLiteral("git -C ") + path +
                          QStringLiteral(" ") + args.join(QLatin1Char(' '));

  p.start(QStringLiteral("git"),
          QStringList{QStringLiteral("-C"), path} + args);

  emit commandStarted(command);

  if (!p.waitForStarted(5000)) {
    res.allOutput = tr("Failed to start git: %1").arg(p.errorString());
    res.exitCode = -1;
    m_currentProcess = nullptr;
    m_commandRunning = false;
    emit commandLogged(args.join(QLatin1Char(' ')), res.allOutput, -1);
    emit commandFinished(command, res.exitCode, false);
    return res;
  }

  QEventLoop loop;
  connect(&p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          &loop, &QEventLoop::quit);
  connect(&p, &QProcess::errorOccurred, &loop, &QEventLoop::quit);

  loop.exec();

  if (p.state() != QProcess::NotRunning) {
    p.kill();
    p.waitForFinished(1000);
  }

  m_currentProcess = nullptr;

  res.exitCode = p.exitCode();
  res.standardOutput = p.readAllStandardOutput();
  res.standardError = p.readAllStandardError();
  res.allOutput =
      QString::fromLocal8Bit(res.standardOutput + res.standardError).trimmed();
  res.ok = (p.exitStatus() == QProcess::NormalExit) &&
           (res.exitCode == 0 || res.exitCode == acceptedExitCode);
  m_commandRunning = false;

  emit commandLogged(args.join(QLatin1Char(' ')), res.allOutput, res.exitCode);
  emit commandFinished(command, res.exitCode, res.ok);
  return res;
}

QStringList GitExecutor::run(const QString &path, const QStringList &args,
                             int acceptedExitCode) {
  const CommandResult res = runCommand(path, args, acceptedExitCode);
  if (!res.ok)
    return {};
  return res.allOutput.split('\n', Qt::SkipEmptyParts);
}

bool GitExecutor::exec(const QString &path, const QStringList &args,
                       QString *output) {
  const CommandResult res = runCommand(path, args, 0);
  if (output)
    *output = res.allOutput;
  return res.ok;
}

QByteArray GitExecutor::raw(const QString &path, const QStringList &args) {
  const CommandResult res = runCommand(path, args, 0);
  return res.ok ? res.standardOutput : QByteArray();
}

void GitExecutor::cancel() {
  if (m_currentProcess)
    m_currentProcess->kill();
}
