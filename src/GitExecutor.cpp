#include "GitExecutor.h"

#include <QProcess>
#include <QString>

GitExecutor::GitExecutor(QObject *parent) : QObject(parent) {}

QStringList GitExecutor::run(const QString &path, const QStringList &args,
                             int acceptedExitCode) const {
  QProcess p;
  p.start(QStringLiteral("git"), QStringList{QStringLiteral("-C"), path} + args);
  if (!p.waitForFinished(10000))
    return {};
  const int code = p.exitCode();
  if (code != 0 && code != acceptedExitCode)
    return {};
  return QString::fromLocal8Bit(p.readAllStandardOutput().trimmed())
      .split('\n', Qt::SkipEmptyParts);
}

bool GitExecutor::exec(const QString &path, const QStringList &args,
                       QString *output) const {
  QProcess p;
  p.start(QStringLiteral("git"), QStringList{QStringLiteral("-C"), path} + args);
  if (!p.waitForStarted(5000)) {
    if (output)
      *output = tr("Failed to start git: %1").arg(p.errorString());
    return false;
  }
  if (!p.waitForFinished(30000)) {
    p.kill();
    p.waitForFinished(1000);
    if (output) {
      *output = tr("Git command timed out or was killed.\n%1")
                    .arg(QString::fromLocal8Bit(p.readAllStandardOutput() +
                                                p.readAllStandardError()));
    }
    return false;
  }
  if (output)
    *output = QString::fromLocal8Bit(p.readAllStandardOutput() +
                                     p.readAllStandardError());
  return p.exitCode() == 0;
}
