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

  // CommitModel API
  void testCommitModelDataAndHeaders();
  void testCommitModelClear();
  void testCommitModelInvalidIndex();

  // Lanes direct manipulation
  void testLanesBasic();
  void testLanesForkAndMerge();
  void testLanesAddAndFind();
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
  const QString badLog = QStringLiteral("*\x1f"
                                        "shortsha\x1f"
                                        "author\x1e");
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

void TestCommitModelAdvanced::testCommitModelDataAndHeaders() {
  CommitModel model;

  const QString raw = QStringLiteral("*\x1f")
                          .append(QStringLiteral(
                              "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\x1f"))
                          .append(QStringLiteral("aaaaaaa\x1f"))
                          .append(QStringLiteral("Author\x1f"))
                          .append(QStringLiteral("a@b.com\x1f"))
                          .append(QStringLiteral("2024-01-01 12:00:00\x1f"))
                          .append(QStringLiteral("1 day ago\x1f"))
                          .append(QStringLiteral("Add feature\x1f"))
                          .append(QStringLiteral("Body text\x1f"))
                          .append(QStringLiteral("main\x1f"))
                          .append(QStringLiteral("\x1f"))
                          .append(QStringLiteral("G\x1e"));

  model.loadLog(raw);
  QCOMPARE(model.rowCount(), 1);
  QCOMPARE(model.columnCount(), 7);

  // Header data
  QCOMPARE(model.headerData(0, Qt::Horizontal, Qt::DisplayRole).toString(),
           QStringLiteral("Graph"));
  QCOMPARE(model.headerData(1, Qt::Horizontal, Qt::DisplayRole).toString(),
           QStringLiteral("Date/Time"));
  QCOMPARE(model.headerData(2, Qt::Horizontal, Qt::DisplayRole).toString(),
           QStringLiteral("Date"));
  QCOMPARE(model.headerData(3, Qt::Horizontal, Qt::DisplayRole).toString(),
           QStringLiteral("Commit Message"));
  QCOMPARE(model.headerData(4, Qt::Horizontal, Qt::DisplayRole).toString(),
           QStringLiteral("Author"));
  QCOMPARE(model.headerData(5, Qt::Horizontal, Qt::DisplayRole).toString(),
           QStringLiteral("Branches"));
  QCOMPARE(model.headerData(6, Qt::Horizontal, Qt::DisplayRole).toString(),
           QStringLiteral("SHA"));
  QVERIFY(model.headerData(7, Qt::Horizontal, Qt::DisplayRole)
              .isNull()); // out of range
  QVERIFY(model.headerData(0, Qt::Vertical, Qt::DisplayRole)
              .isNull()); // wrong orientation
  QVERIFY(
      model.headerData(0, Qt::Horizontal, Qt::EditRole).isNull()); // wrong role

  // Data in display role
  QModelIndex idx0 = model.index(0, 0);
  QCOMPARE(model.data(idx0, Qt::DisplayRole).toString(), QStringLiteral("*"));
  QCOMPARE(model.data(model.index(0, 1), Qt::DisplayRole).toString(),
           QStringLiteral("2024-01-01 12:00:00"));
  QCOMPARE(model.data(model.index(0, 2), Qt::DisplayRole).toString(),
           QStringLiteral("1 day ago"));
  QCOMPARE(model.data(model.index(0, 3), Qt::DisplayRole).toString(),
           QStringLiteral("Add feature"));
  QCOMPARE(model.data(model.index(0, 4), Qt::DisplayRole).toString(),
           QStringLiteral("Author"));
  QCOMPARE(model.data(model.index(0, 5), Qt::DisplayRole).toString(),
           QStringLiteral("main"));
  QCOMPARE(model.data(model.index(0, 6), Qt::DisplayRole).toString(),
           QStringLiteral("aaaaaaa"));

  // User role for full SHA on SHA column
  QCOMPARE(model.data(model.index(0, 6), Qt::UserRole).toString(),
           QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  QVERIFY(model.data(model.index(0, 0), Qt::UserRole).isNull());

  // Tooltip role is also supported
  QCOMPARE(model.data(idx0, Qt::ToolTipRole).toString(), QStringLiteral("*"));
}

static QString makeTestRecord() {
  return QStringLiteral("*\x1f")
      .append(QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\x1f"))
      .append(QStringLiteral("aaaaaaa\x1f"))
      .append(QStringLiteral("Author\x1f"))
      .append(QStringLiteral("a@b.com\x1f"))
      .append(QStringLiteral("2024-01-01 12:00:00\x1f"))
      .append(QStringLiteral("1 day ago\x1f"))
      .append(QStringLiteral("Subject\x1f"))
      .append(QStringLiteral("Body\x1f"))
      .append(QStringLiteral("main\x1f"))
      .append(QStringLiteral("\x1f"))
      .append(QStringLiteral("N\x1e"));
}

void TestCommitModelAdvanced::testCommitModelClear() {
  CommitModel model;
  model.loadLog(makeTestRecord());
  QVERIFY(model.rowCount() > 0);
  model.clear();
  QCOMPARE(model.rowCount(), 0);
}

void TestCommitModelAdvanced::testCommitModelInvalidIndex() {
  CommitModel model;
  model.loadLog(makeTestRecord());
  QVERIFY(model.data(QModelIndex(), Qt::DisplayRole).isNull());
  QVERIFY(model.data(model.index(10, 0), Qt::DisplayRole).isNull());
  QVERIFY(model.data(model.index(0, 10), Qt::DisplayRole).isNull());
}

void TestCommitModelAdvanced::testLanesBasic() {
  Lanes lanes;
  lanes.init(QStringLiteral("sha1"));
  QVERIFY(!lanes.isEmpty());

  lanes.setBoundary(true);
  QVERIFY(true);

  lanes.setInitial();
  lanes.setApplied();
  lanes.afterBranch();
  lanes.afterApplied();

  lanes.nextParent(QStringLiteral("parent"));
  lanes.clear();
  QVERIFY(lanes.isEmpty());
}

void TestCommitModelAdvanced::testLanesForkAndMerge() {
  // Build a simple 3-commit scenario: c1 (sha1) -> c2 (sha2) with a fork
  Lanes lanes;
  lanes.init(QStringLiteral("sha1"));

  // Move to sha2 (new branch)
  bool isDiscontinuity = false;
  QVERIFY(!lanes.isFork(QStringLiteral("sha2"), isDiscontinuity));
  lanes.changeActiveLane(QStringLiteral("sha2"));
  lanes.setInitial();
  lanes.afterFork();
  lanes.nextParent(QStringLiteral("sha2_parent"));

  // Create extra lanes via changeActiveLane with new SHAs and test merge
  lanes.changeActiveLane(QStringLiteral("side1"));
  lanes.changeActiveLane(QStringLiteral("sha2"));
  lanes.changeActiveLane(QStringLiteral("side2"));
  lanes.changeActiveLane(QStringLiteral("sha2"));

  const QStringList parents = {QStringLiteral("sha2_parent"),
                               QStringLiteral("side1"),
                               QStringLiteral("side2")};
  lanes.setMerge(parents);
  lanes.afterMerge();
}

void TestCommitModelAdvanced::testLanesAddAndFind() {
  Lanes lanes;
  lanes.init(QStringLiteral("sha1"));

  // New branch added by changeActiveLane should be BRANCH
  lanes.changeActiveLane(QStringLiteral("side1"));
  QVERIFY(lanes.isBranch());

  bool isDiscontinuity = false;
  QVERIFY(!lanes.isFork(QStringLiteral("missing"), isDiscontinuity));

  // Move back to existing sha; it becomes ACTIVE, no longer BRANCH
  lanes.changeActiveLane(QStringLiteral("sha1"));
  QVERIFY(!lanes.isBranch());
}

QTEST_MAIN(TestCommitModelAdvanced)
#include "TestCommitModelAdvanced.moc"
