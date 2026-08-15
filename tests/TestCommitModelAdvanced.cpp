#include "CommitModel.h"
#include "LaneGraph.h"
#include "Lanes.h"

#include <QTest>

class TestCommitModelAdvanced : public QObject {
  Q_OBJECT

private slots:
  void testEmptyLog();
  void testMalformedRecord();
  void testMultipleCommits();
  void testSkipCiStripped();
  void testBranchRefNormalization();
  void testParentsParsed();
  void testGpgStatusVariants();
  void testLaneGraphEmpty();
  void testLaneGraphLinear();
  void testLaneGraphBranchMerge();
};

void TestCommitModelAdvanced::testEmptyLog() {
  CommitModel model;
  model.loadLog(QString());
  QCOMPARE(model.rowCount(), 0);

  model.loadLog(QStringLiteral(""));
  QCOMPARE(model.rowCount(), 0);

  model.loadLog(QStringLiteral("\x1e\x1e\x1e"));
  QCOMPARE(model.rowCount(), 0);
}

void TestCommitModelAdvanced::testMalformedRecord() {
  CommitModel model;
  // Too few fields — should be skipped
  const QString badLog = QStringLiteral("*\x1fshortsha\x1fauthor\x1e");
  model.loadLog(badLog);
  QCOMPARE(model.rowCount(), 0);
}

void TestCommitModelAdvanced::testMultipleCommits() {
  CommitModel model;

  auto makeRecord = [](const QString &sha, const QString &subject) {
    return QStringLiteral("*\x1f")
        .append(sha + QStringLiteral("\x1f"))
        .append(sha.left(7) + QStringLiteral("\x1f"))
        .append(QStringLiteral("Author\x1f"))
        .append(QStringLiteral("a@b.com\x1f"))
        .append(QStringLiteral("2024-01-01 12:00:00\x1f"))
        .append(QStringLiteral("1 day ago\x1f"))
        .append(subject + QStringLiteral("\x1f"))
        .append(QStringLiteral("\x1f"))
        .append(QStringLiteral("main\x1f"))
        .append(QStringLiteral("\x1f"))
        .append(QStringLiteral("N\x1e"));
  };

  const QString sha1 =
      QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  const QString sha2 =
      QStringLiteral("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  const QString sha3 =
      QStringLiteral("cccccccccccccccccccccccccccccccccccccccc");

  const QString raw = makeRecord(sha1, "First") + makeRecord(sha2, "Second") +
                      makeRecord(sha3, "Third");

  model.loadLog(raw);
  QCOMPARE(model.rowCount(), 3);
  QCOMPARE(model.commit(0).subject, QStringLiteral("First"));
  QCOMPARE(model.commit(1).subject, QStringLiteral("Second"));
  QCOMPARE(model.commit(2).subject, QStringLiteral("Third"));
  QCOMPARE(model.commit(0).fullSha, sha1);
  QCOMPARE(model.commit(2).fullSha, sha3);
}

void TestCommitModelAdvanced::testSkipCiStripped() {
  CommitModel model;

  const QString raw =
      QStringLiteral("*\x1f")
          .append(
              QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\x1f"))
          .append(QStringLiteral("aaaaaaa\x1f"))
          .append(QStringLiteral("Author\x1f"))
          .append(QStringLiteral("a@b.com\x1f"))
          .append(QStringLiteral("2024-01-01 12:00:00\x1f"))
          .append(QStringLiteral("1 day ago\x1f"))
          .append(QStringLiteral("Fix build [skip ci] stuff\x1f"))
          .append(QStringLiteral("\x1f"))
          .append(QStringLiteral("main\x1f"))
          .append(QStringLiteral("\x1f"))
          .append(QStringLiteral("N\x1e"));

  model.loadLog(raw);
  QCOMPARE(model.rowCount(), 1);
  QCOMPARE(model.commit(0).subject, QStringLiteral("Fix build  stuff"));
}

void TestCommitModelAdvanced::testBranchRefNormalization() {
  CommitModel model;

  auto makeWithBranch = [](const QString &branch) {
    return QStringLiteral("*\x1f")
        .append(QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\x1f"))
        .append(QStringLiteral("aaaaaaa\x1f"))
        .append(QStringLiteral("Author\x1f"))
        .append(QStringLiteral("a@b.com\x1f"))
        .append(QStringLiteral("2024-01-01 12:00:00\x1f"))
        .append(QStringLiteral("1 day ago\x1f"))
        .append(QStringLiteral("msg\x1f"))
        .append(QStringLiteral("\x1f"))
        .append(branch + QStringLiteral("\x1f"))
        .append(QStringLiteral("\x1f"))
        .append(QStringLiteral("N\x1e"));
  };

  // refs/heads/ prefix stripped
  model.loadLog(makeWithBranch(QStringLiteral("refs/heads/feature")));
  QCOMPARE(model.commit(0).branch, QStringLiteral("feature"));

  // refs/remotes/ prefix stripped
  model.loadLog(makeWithBranch(QStringLiteral("refs/remotes/origin/main")));
  QCOMPARE(model.commit(0).branch, QStringLiteral("origin/main"));

  // refs/tags/ prefix stripped
  model.loadLog(makeWithBranch(QStringLiteral("refs/tags/v1.0")));
  QCOMPARE(model.commit(0).branch, QStringLiteral("v1.0"));

  // No prefix — left as-is
  model.loadLog(makeWithBranch(QStringLiteral("main")));
  QCOMPARE(model.commit(0).branch, QStringLiteral("main"));
}

void TestCommitModelAdvanced::testParentsParsed() {
  CommitModel model;

  const QString raw = QStringLiteral("*\x1f")
                          .append(QStringLiteral(
                              "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\x1f"))
                          .append(QStringLiteral("aaaaaaa\x1f"))
                          .append(QStringLiteral("Author\x1f"))
                          .append(QStringLiteral("a@b.com\x1f"))
                          .append(QStringLiteral("2024-01-01 12:00:00\x1f"))
                          .append(QStringLiteral("1 day ago\x1f"))
                          .append(QStringLiteral("Merge commit\x1f"))
                          .append(QStringLiteral("\x1f"))
                          .append(QStringLiteral("main\x1f"))
                          .append(QStringLiteral("parent1 parent2\x1f"))
                          .append(QStringLiteral("G\x1e"));

  model.loadLog(raw);
  QCOMPARE(model.rowCount(), 1);
  QCOMPARE(model.commit(0).parents.size(), 2);
  QCOMPARE(model.commit(0).parents.at(0), QStringLiteral("parent1"));
  QCOMPARE(model.commit(0).parents.at(1), QStringLiteral("parent2"));
}

void TestCommitModelAdvanced::testGpgStatusVariants() {
  CommitModel model;

  auto makeWithGpg = [](const QString &gpg) {
    return QStringLiteral("*\x1f")
        .append(QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\x1f"))
        .append(QStringLiteral("aaaaaaa\x1f"))
        .append(QStringLiteral("Author\x1f"))
        .append(QStringLiteral("a@b.com\x1f"))
        .append(QStringLiteral("2024-01-01 12:00:00\x1f"))
        .append(QStringLiteral("1 day ago\x1f"))
        .append(QStringLiteral("msg\x1f"))
        .append(QStringLiteral("\x1f"))
        .append(QStringLiteral("main\x1f"))
        .append(QStringLiteral("\x1f"))
        .append(gpg + QStringLiteral("\x1e"));
  };

  model.loadLog(makeWithGpg(QStringLiteral("G")));
  QCOMPARE(model.commit(0).gpgStatus, QStringLiteral("G"));

  model.loadLog(makeWithGpg(QStringLiteral("B")));
  QCOMPARE(model.commit(0).gpgStatus, QStringLiteral("B"));

  model.loadLog(makeWithGpg(QStringLiteral("U")));
  QCOMPARE(model.commit(0).gpgStatus, QStringLiteral("U"));

  model.loadLog(makeWithGpg(QStringLiteral("N")));
  QCOMPARE(model.commit(0).gpgStatus, QStringLiteral("N"));

  model.loadLog(makeWithGpg(QString()));
  QCOMPARE(model.commit(0).gpgStatus, QString());
}

void TestCommitModelAdvanced::testLaneGraphEmpty() {
  QList<Commit> empty;
  const QVector<QVector<int>> result = LaneGraph::build(empty);
  QVERIFY(result.isEmpty());
}

void TestCommitModelAdvanced::testLaneGraphLinear() {
  // 3 linear commits: A -> B -> C
  QList<Commit> commits;
  Commit a;
  a.fullSha = QStringLiteral("aaa");
  a.parents = QStringList{QStringLiteral("bbb")};
  Commit b;
  b.fullSha = QStringLiteral("bbb");
  b.parents = QStringList{QStringLiteral("ccc")};
  Commit c;
  c.fullSha = QStringLiteral("ccc");
  c.parents = QStringList{};

  commits << a << b << c;
  const QVector<QVector<int>> lanes = LaneGraph::build(commits);

  QCOMPARE(lanes.size(), 3);
  // Each row should have at least one lane
  for (const auto &row : lanes)
    QVERIFY(!row.isEmpty());
  // The active lane should be at position 0 for all rows in a linear graph
  QVERIFY(lanes[0][0] != Lane::EMPTY);
  QVERIFY(lanes[1][0] != Lane::EMPTY);
  QVERIFY(lanes[2][0] != Lane::EMPTY);
}

void TestCommitModelAdvanced::testLaneGraphBranchMerge() {
  // Merge commit M with parents A and B, followed by A and B
  // M -> A, B
  // A -> base
  // B -> base
  QList<Commit> commits;
  Commit m;
  m.fullSha = QStringLiteral("mmm");
  m.parents = QStringList{QStringLiteral("aaa"), QStringLiteral("bbb")};
  Commit a;
  a.fullSha = QStringLiteral("aaa");
  a.parents = QStringList{QStringLiteral("base")};
  Commit b;
  b.fullSha = QStringLiteral("bbb");
  b.parents = QStringList{QStringLiteral("base")};

  commits << m << a << b;
  const QVector<QVector<int>> lanes = LaneGraph::build(commits);

  QCOMPARE(lanes.size(), 3);
  // Merge commit should produce multiple lanes
  QVERIFY(lanes[0].size() >= 1);
  // After the merge, there should be at least 2 lanes for the two parents
  QVERIFY(lanes[1].size() >= 2);
}

QTEST_MAIN(TestCommitModelAdvanced)
#include "TestCommitModelAdvanced.moc"
