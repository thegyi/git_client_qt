#include "GitRepository.h"
#include "CommitModel.h"
#include "DiffPresenter.h"
#include "GitExecutor.h"

#include <QAbstractTableModel>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

class TestGitModels : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();
  void testGitRepository();
  void testCommitModel();
  void testDiffPresenter();

private:
  QString m_repoPath;
  GitExecutor *m_executor = nullptr;
  QTemporaryDir *m_tempDir = nullptr;

  bool runGit(const QStringList &args);
};

bool TestGitModels::runGit(const QStringList &args) {
  QProcess p;
  p.setWorkingDirectory(m_repoPath);
  p.start(QStringLiteral("git"),
          QStringList{QStringLiteral("-C"), m_repoPath} + args);
  if (!p.waitForStarted(5000))
    return false;
  if (!p.waitForFinished(10000))
    return false;
  return p.exitCode() == 0;
}

void TestGitModels::initTestCase() {
  m_tempDir = new QTemporaryDir();
  QVERIFY(m_tempDir->isValid());
  m_repoPath = m_tempDir->path();

  QVERIFY(runGit({QStringLiteral("init")}));
  QVERIFY(runGit({QStringLiteral("config"), QStringLiteral("user.email"),
                  QStringLiteral("test@example.com")}));
  QVERIFY(runGit({QStringLiteral("config"), QStringLiteral("user.name"),
                  QStringLiteral("Test User")}));

  QFile f(m_repoPath + QStringLiteral("/test.txt"));
  QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
  f.write("hello\n");
  f.close();

  QVERIFY(runGit({QStringLiteral("add"), QStringLiteral("test.txt")}));
  QVERIFY(runGit({QStringLiteral("commit"), QStringLiteral("-m"),
                  QStringLiteral("Initial commit")}));

  m_executor = new GitExecutor(this);
}

void TestGitModels::cleanupTestCase() {
  delete m_tempDir;
  m_tempDir = nullptr;
}

void TestGitModels::testGitRepository() {
  GitRepository repo(m_executor);
  repo.setPath(m_repoPath);

  QVERIFY(repo.isValid());
  QCOMPARE(repo.root(), m_repoPath);
  QVERIFY(!repo.path().isEmpty());

  QVERIFY(repo.stashes().isEmpty());
  QVERIFY(repo.remotes().isEmpty());
  QVERIFY(!repo.worktrees().isEmpty());

  const QString state = repo.stateSignature();
  QVERIFY(!state.isEmpty());
  QVERIFY(state.contains('|'));
}

void TestGitModels::testCommitModel() {
  CommitModel model;

  const QString rawLog =
      QStringLiteral("*\x1f")
          .append(QStringLiteral("abc123def456789012345678901234567890abcd\x1f"))
          .append(QStringLiteral("abc1234\x1f"))
          .append(QStringLiteral("Test Author\x1f"))
          .append(QStringLiteral("2024-01-01 12:00:00\x1f"))
          .append(QStringLiteral("2 hours ago\x1f"))
          .append(QStringLiteral("Add feature\x1f"))
          .append(QStringLiteral("Detailed body\x1f"))
          .append(QStringLiteral("main\x1e"));

  model.loadLog(rawLog);
  QCOMPARE(model.rowCount(), 1);
  QCOMPARE(model.columnCount(), 7);

  const Commit &c = model.commit(0);
  QCOMPARE(c.fullSha, QStringLiteral("abc123def456789012345678901234567890abcd"));
  QCOMPARE(c.shortSha, QStringLiteral("abc1234"));
  QCOMPARE(c.author, QStringLiteral("Test Author"));
  QCOMPARE(c.subject, QStringLiteral("Add feature"));
  QCOMPARE(c.body, QStringLiteral("Detailed body"));
  QCOMPARE(c.branch, QStringLiteral("main"));

  QCOMPARE(model.headerData(0, Qt::Horizontal, Qt::DisplayRole).toString(),
           QStringLiteral("Graph"));
  QCOMPARE(model.headerData(3, Qt::Horizontal, Qt::DisplayRole).toString(),
           QStringLiteral("Commit Message"));
}

void TestGitModels::testDiffPresenter() {
  DiffPresenter presenter;

  const QStringList diff = {
      QStringLiteral("diff --git a/file.txt b/file.txt"),
      QStringLiteral("index 123..456 100644"),
      QStringLiteral("--- a/file.txt"),
      QStringLiteral("+++ b/file.txt"),
      QStringLiteral("@@ -1,2 +1,2 @@"),
      QStringLiteral(" old"),
      QStringLiteral("+new")};

  const QString htmlWithLinks = presenter.formatDiff(diff, true, false);
  QVERIFY(htmlWithLinks.contains(QStringLiteral("git:hunk:0")));
  QVERIFY(htmlWithLinks.contains(QStringLiteral("stage hunk")));

  const QString htmlNoLinks = presenter.formatDiff(diff, false, false);
  QVERIFY(!htmlNoLinks.contains(QStringLiteral("git:hunk:0")));

  const QStringList lfsPointer = {
      QStringLiteral("version https://git-lfs.github.com/spec/v1"),
      QStringLiteral("oid sha256:1234"),
      QStringLiteral("size 42")};
  QVERIFY(presenter.isLfsPointer(lfsPointer));

  const QStringList notLfs = {QStringLiteral("some file content")};
  QVERIFY(!presenter.isLfsPointer(notLfs));
}

QTEST_MAIN(TestGitModels)
#include "TestGitModels.moc"
