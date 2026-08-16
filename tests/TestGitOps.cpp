#include "DiffPresenter.h"
#include "GitExecutor.h"
#include "GitRepository.h"

#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TestGitOps : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();

  // GitExecutor advanced
  void testExecFailure();
  void testExecNonExistentRepo();
  void testRunEmptyResult();
  void testExecMultiLineOutput();
  void testExecAcceptedExitCode();
  void testRaw();
  void testCancel();

  // GitRepository
  void testBranches();
  void testStashCreateAndList();
  void testStateSignatureChanges();
  void testInvalidRepo();
  void testPath();
  void testRemotes();
  void testWorktrees();

  // DiffPresenter
  void testSideBySideMode();
  void testUnifiedModeDefault();
  void testEmptyDiff();
  void testFormatCurrent();
  void testLfsPointerHtml();

private:
  bool runGit(const QStringList &args);
  QString m_repoPath;
  GitExecutor *m_executor = nullptr;
  QTemporaryDir *m_tempDir = nullptr;
};

bool TestGitOps::runGit(const QStringList &args) {
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

void TestGitOps::initTestCase() {
  m_tempDir = new QTemporaryDir();
  QVERIFY(m_tempDir->isValid());
  m_repoPath = m_tempDir->path();

  QVERIFY(runGit({QStringLiteral("init")}));
  QVERIFY(runGit({QStringLiteral("config"), QStringLiteral("user.email"),
                  QStringLiteral("test@example.com")}));
  QVERIFY(runGit({QStringLiteral("config"), QStringLiteral("user.name"),
                  QStringLiteral("Test User")}));

  QFile f(m_repoPath + QStringLiteral("/file.txt"));
  QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
  f.write("line1\nline2\n");
  f.close();

  QVERIFY(runGit({QStringLiteral("add"), QStringLiteral(".")}));
  QVERIFY(runGit({QStringLiteral("commit"), QStringLiteral("-m"),
                  QStringLiteral("Initial commit")}));

  m_executor = new GitExecutor(this);
}

void TestGitOps::cleanupTestCase() {
  delete m_tempDir;
  m_tempDir = nullptr;
}

// --- GitExecutor advanced ---

void TestGitOps::testExecFailure() {
  QString output;
  // Trying to checkout a non-existent branch should fail
  const bool ok = m_executor->exec(
      m_repoPath,
      {QStringLiteral("checkout"), QStringLiteral("nonexistent-branch-xyz")},
      &output);
  QVERIFY(!ok);
  QVERIFY(!output.isEmpty());
}

void TestGitOps::testExecNonExistentRepo() {
  QString output;
  const bool ok =
      m_executor->exec(QStringLiteral("/tmp/nonexistent_repo_12345"),
                       {QStringLiteral("status")}, &output);
  QVERIFY(!ok);
}

void TestGitOps::testRunEmptyResult() {
  // No tags in fresh repo
  const QStringList tags = m_executor->run(
      m_repoPath, {QStringLiteral("tag"), QStringLiteral("--list")});
  QVERIFY(tags.isEmpty());
}

void TestGitOps::testExecMultiLineOutput() {
  // Create a second commit
  QFile f(m_repoPath + QStringLiteral("/file2.txt"));
  QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
  f.write("content\n");
  f.close();
  QVERIFY(runGit({QStringLiteral("add"), QStringLiteral(".")}));
  QVERIFY(runGit({QStringLiteral("commit"), QStringLiteral("-m"),
                  QStringLiteral("Second commit")}));

  const QStringList log = m_executor->run(
      m_repoPath, {QStringLiteral("log"), QStringLiteral("--format=%s")});
  QVERIFY(log.size() >= 2);
  QCOMPARE(log.first(), QStringLiteral("Second commit"));
  QCOMPARE(log.last(), QStringLiteral("Initial commit"));
}

void TestGitOps::testExecAcceptedExitCode() {
  // `git diff --no-index --exit-code` returns 1 when files differ, but
  // the accepted exit code still makes `run` return the diff output
  QFile f1(m_repoPath + QStringLiteral("/f1.txt"));
  QVERIFY(f1.open(QIODevice::WriteOnly | QIODevice::Text));
  f1.write("a\n");
  f1.close();

  QFile f2(m_repoPath + QStringLiteral("/f2.txt"));
  QVERIFY(f2.open(QIODevice::WriteOnly | QIODevice::Text));
  f2.write("b\n");
  f2.close();

  const QStringList output =
      m_executor->run(m_repoPath,
                      {QStringLiteral("diff"), QStringLiteral("--exit-code"),
                       QStringLiteral("--no-index"), QStringLiteral("f1.txt"),
                       QStringLiteral("f2.txt")},
                      1);
  QVERIFY(!output.isEmpty());
}

void TestGitOps::testRaw() {
  const QByteArray out = m_executor->raw(
      m_repoPath, {QStringLiteral("log"), QStringLiteral("--format=%s")});
  QVERIFY(!out.isEmpty());
  QVERIFY(out.contains("Initial commit"));
}

void TestGitOps::testCancel() {
  // Cancelling a not-running process should not crash
  m_executor->cancel();
  QVERIFY(true);
}

// --- GitRepository ---

void TestGitOps::testBranches() {
  GitRepository repo(m_executor);
  repo.setPath(m_repoPath);
  QVERIFY(repo.isValid());

  // Create a branch
  QVERIFY(runGit({QStringLiteral("branch"), QStringLiteral("feature-x")}));

  // Verify via executor
  const QStringList branches = m_executor->run(
      m_repoPath,
      {QStringLiteral("branch"), QStringLiteral("--format=%(refname:short)")});
  QVERIFY(branches.contains(QStringLiteral("feature-x")));
}

void TestGitOps::testStashCreateAndList() {
  GitRepository repo(m_executor);
  repo.setPath(m_repoPath);

  // Modify a file to create something to stash
  QFile f(m_repoPath + QStringLiteral("/file.txt"));
  QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
  f.write("modified content\n");
  f.close();

  QVERIFY(runGit({QStringLiteral("stash"), QStringLiteral("push"),
                  QStringLiteral("-m"), QStringLiteral("test stash")}));

  const auto stashes = repo.stashes();
  QVERIFY(!stashes.isEmpty());
  QVERIFY(stashes.first().second.contains(QStringLiteral("test stash")));

  // Clean up stash
  QVERIFY(runGit({QStringLiteral("stash"), QStringLiteral("drop")}));
}

void TestGitOps::testStateSignatureChanges() {
  GitRepository repo(m_executor);
  repo.setPath(m_repoPath);

  const QString sig1 = repo.stateSignature();
  QVERIFY(!sig1.isEmpty());

  // Create a new commit
  QFile f(m_repoPath + QStringLiteral("/sig_test.txt"));
  QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
  f.write("data\n");
  f.close();
  QVERIFY(runGit({QStringLiteral("add"), QStringLiteral(".")}));
  QVERIFY(runGit({QStringLiteral("commit"), QStringLiteral("-m"),
                  QStringLiteral("sig change")}));

  const QString sig2 = repo.stateSignature();
  QVERIFY(sig1 != sig2);
}

void TestGitOps::testInvalidRepo() {
  GitRepository repo(m_executor);
  repo.setPath(QStringLiteral("/tmp"));
  QVERIFY(!repo.isValid());
  QVERIFY(repo.root().isEmpty());
}

void TestGitOps::testPath() {
  GitRepository repo(m_executor);
  repo.setPath(m_repoPath);
  QCOMPARE(repo.path(), m_repoPath);
}

void TestGitOps::testRemotes() {
  GitRepository repo(m_executor);
  repo.setPath(m_repoPath);

  // Add a remote
  QVERIFY(runGit({QStringLiteral("remote"), QStringLiteral("add"),
                  QStringLiteral("origin"),
                  QStringLiteral("https://example.com/repo.git")}));

  const auto remotes = repo.remotes();
  QCOMPARE(remotes.size(), 1);
  QCOMPARE(remotes.first().first, QStringLiteral("origin"));
  QCOMPARE(remotes.first().second,
           QStringLiteral("https://example.com/repo.git"));
}

void TestGitOps::testWorktrees() {
  GitRepository repo(m_executor);
  repo.setPath(m_repoPath);

  const auto worktrees = repo.worktrees();
  QVERIFY(!worktrees.isEmpty());
  QVERIFY(worktrees.first().contains(m_repoPath));
}

// --- DiffPresenter ---

void TestGitOps::testSideBySideMode() {
  DiffPresenter presenter;
  presenter.setMode(DiffPresenter::DiffMode::SideBySide);
  QCOMPARE(presenter.mode(), DiffPresenter::DiffMode::SideBySide);

  const QStringList diff = {QStringLiteral("diff --git a/file.txt b/file.txt"),
                            QStringLiteral("index 123..456 100644"),
                            QStringLiteral("--- a/file.txt"),
                            QStringLiteral("+++ b/file.txt"),
                            QStringLiteral("@@ -1,2 +1,2 @@"),
                            QStringLiteral("-old line"),
                            QStringLiteral("+new line"),
                            QStringLiteral(" context")};

  const QString html = presenter.formatDiff(diff);
  QVERIFY(!html.isEmpty());
  // Side-by-side should produce a table
  QVERIFY(html.contains(QStringLiteral("<table")) ||
          html.contains(QStringLiteral("<td")));
}

void TestGitOps::testUnifiedModeDefault() {
  DiffPresenter presenter;
  QCOMPARE(presenter.mode(), DiffPresenter::DiffMode::Unified);

  const QStringList diff = {QStringLiteral("diff --git a/file.txt b/file.txt"),
                            QStringLiteral("--- a/file.txt"),
                            QStringLiteral("+++ b/file.txt"),
                            QStringLiteral("@@ -1 +1 @@"),
                            QStringLiteral("-removed"),
                            QStringLiteral("+added")};

  const QString html = presenter.formatDiff(diff);
  QVERIFY(!html.isEmpty());
  QVERIFY(html.contains(QStringLiteral("removed")));
  QVERIFY(html.contains(QStringLiteral("added")));
}

void TestGitOps::testEmptyDiff() {
  DiffPresenter presenter;
  const QString html = presenter.formatDiff({});
  // Should handle gracefully (empty or minimal HTML)
  QVERIFY(html.isEmpty() || html.contains(QStringLiteral("<")));
}

void TestGitOps::testFormatCurrent() {
  DiffPresenter presenter;
  // Before any formatDiff call, hasCurrent should be false
  QVERIFY(!presenter.hasCurrent());

  const QStringList diff = {QStringLiteral("diff --git a/f.txt b/f.txt"),
                            QStringLiteral("--- a/f.txt"),
                            QStringLiteral("+++ b/f.txt"),
                            QStringLiteral("@@ -1 +1 @@"),
                            QStringLiteral("-x"),
                            QStringLiteral("+y")};

  presenter.formatDiff(diff);
  QVERIFY(presenter.hasCurrent());
  const QString current = presenter.formatCurrent();
  QVERIFY(!current.isEmpty());
}

void TestGitOps::testLfsPointerHtml() {
  DiffPresenter presenter;
  const QStringList lfs = {
      QStringLiteral("version https://git-lfs.github.com/spec/v1"),
      QStringLiteral("oid sha256:abc123"), QStringLiteral("size 1024")};

  QVERIFY(presenter.isLfsPointer(lfs));
  const QString html = presenter.lfsPointerHtml(lfs);
  QVERIFY(!html.isEmpty());
  QVERIFY(html.contains(QStringLiteral("LFS")) ||
          html.contains(QStringLiteral("lfs")));
}

QTEST_MAIN(TestGitOps)
#include "TestGitOps.moc"
