#include "GitExecutor.h"

#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TestGitExecutor : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();
  void testRun();
  void testExec();
  void testCommandLogged();

private:
  QString m_repoPath;
  GitExecutor *m_executor = nullptr;
  QTemporaryDir *m_tempDir = nullptr;
};

void TestGitExecutor::initTestCase() {
  m_tempDir = new QTemporaryDir();
  QVERIFY(m_tempDir->isValid());
  m_repoPath = m_tempDir->path();

  auto runGit = [&](const QStringList &args) {
    QProcess p;
    p.setWorkingDirectory(m_repoPath);
    p.start(QStringLiteral("git"),
            QStringList{QStringLiteral("-C"), m_repoPath} + args);
    QVERIFY(p.waitForStarted(5000));
    QVERIFY(p.waitForFinished(10000));
    QCOMPARE(p.exitCode(), 0);
  };

  runGit({QStringLiteral("init")});
  runGit({QStringLiteral("config"), QStringLiteral("user.email"),
          QStringLiteral("test@example.com")});
  runGit({QStringLiteral("config"), QStringLiteral("user.name"),
          QStringLiteral("Test User")});

  QFile f(m_repoPath + QStringLiteral("/test.txt"));
  QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
  f.write("hello\n");
  f.close();

  runGit({QStringLiteral("add"), QStringLiteral("test.txt")});
  runGit({QStringLiteral("commit"), QStringLiteral("-m"),
          QStringLiteral("Initial commit")});

  m_executor = new GitExecutor(this);
  QVERIFY(m_executor != nullptr);
}

void TestGitExecutor::cleanupTestCase() {
  delete m_tempDir;
  m_tempDir = nullptr;
}

void TestGitExecutor::testRun() {
  const QStringList output = m_executor->run(
      m_repoPath, {QStringLiteral("log"), QStringLiteral("--format=%s")});
  QCOMPARE(output.size(), 1);
  QCOMPARE(output.first(), QStringLiteral("Initial commit"));
}

void TestGitExecutor::testExec() {
  QString output;
  const bool ok =
      m_executor->exec(m_repoPath,
                       {QStringLiteral("rev-parse"), QStringLiteral("--short"),
                        QStringLiteral("HEAD")},
                       &output);
  QVERIFY(ok);
  QVERIFY(!output.isEmpty());
  QVERIFY(output.length() >= 7);
}

void TestGitExecutor::testCommandLogged() {
  QSignalSpy spy(m_executor, &GitExecutor::commandLogged);
  m_executor->run(m_repoPath,
                  {QStringLiteral("status"), QStringLiteral("--porcelain")});
  QCOMPARE(spy.count(), 1);
  const QList<QVariant> arguments = spy.takeFirst();
  QCOMPARE(arguments.size(), 3);
  QVERIFY(arguments.at(0).toString().contains(QStringLiteral("status")));
  QVERIFY(arguments.at(2).toInt() == 0);
}

QTEST_MAIN(TestGitExecutor)
#include "TestGitExecutor.moc"
