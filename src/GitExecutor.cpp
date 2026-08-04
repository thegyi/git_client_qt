#include "GitExecutor.h"

#include <QProcess>
#include <QString>

GitExecutor::GitExecutor(QObject *parent) : QObject(parent) {}

QStringList GitExecutor::run(const QString &path, const QStringList &args,
                             int acceptedExitCode) {
  QProcess p;
  p.start(QStringLiteral("git"),
          QStringList{QStringLiteral("-C"), path} + args);
  if (!p.waitForFinished(10000))
    return {};
  const int code = p.exitCode();
  const QString cmd = QStringLiteral("git -C ") + path + QStringLiteral(" ") +
                      args.join(QLatin1Char(' '));
  const QString output =
      QString::fromLocal8Bit(p.readAllStandardOutput().trimmed());
  emit commandLogged(cmd, output, code);
  if (code != 0 && code != acceptedExitCode)
    return {};
  return output.split('\n', Qt::SkipEmptyParts);
}

bool GitExecutor::exec(const QString &path, const QStringList &args,
                       QString *output) {
  QProcess p;
  p.start(QStringLiteral("git"),
          QStringList{QStringLiteral("-C"), path} + args);
  if (!p.waitForStarted(5000)) {
    const QString err = tr("Failed to start git: %1").arg(p.errorString());
    if (output)
      *output = err;
    emit commandLogged(args.join(QLatin1Char(' ')), err, -1);
    return false;
  }
  if (!p.waitForFinished(30000)) {
    p.kill();
    p.waitForFinished(1000);
    const QString allOutput = QString::fromLocal8Bit(p.readAllStandardOutput() +
                                                     p.readAllStandardError());
    const QString err =
        tr("Git command timed out or was killed.\n%1").arg(allOutput);
    if (output) {
      *output = err;
    }
    emit commandLogged(args.join(QLatin1Char(' ')), err, p.exitCode());
    return false;
  }
  const QString allOutput = QString::fromLocal8Bit(p.readAllStandardOutput() +
                                                   p.readAllStandardError());
  if (output)
    *output = allOutput;
  const int code = p.exitCode();
  emit commandLogged(args.join(QLatin1Char(' ')), allOutput, code);
  return code == 0;
}
