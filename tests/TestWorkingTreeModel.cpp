#include "WorkingTreeModel.h"

#include <QTest>

class TestWorkingTreeModel : public QObject {
  Q_OBJECT

private slots:
  void testLoad();
  void testClear();
};

void TestWorkingTreeModel::testLoad() {
  WorkingTreeModel model;

  const QStringList staged = {QStringLiteral("M\tfile1.txt"),
                              QStringLiteral("A\tfile2.txt")};
  const QStringList unstaged = {QStringLiteral("M\tfile1.txt"),
                                QStringLiteral("D\tfile3.txt")};
  const QStringList untracked = {QStringLiteral("new.txt")};

  model.load(staged, unstaged, untracked);

  const auto &stagedFiles = model.stagedFiles();
  QCOMPARE(stagedFiles.size(), 2);
  QCOMPARE(stagedFiles.at(0).first, QStringLiteral("file1.txt"));
  QCOMPARE(stagedFiles.at(0).second, QStringLiteral("M"));
  QCOMPARE(stagedFiles.at(1).first, QStringLiteral("file2.txt"));
  QCOMPARE(stagedFiles.at(1).second, QStringLiteral("A"));

  const auto &unstagedFiles = model.unstagedFiles();
  QCOMPARE(unstagedFiles.size(), 3);
  QCOMPARE(unstagedFiles.at(0).first, QStringLiteral("file1.txt"));
  QCOMPARE(unstagedFiles.at(0).second, QStringLiteral("M"));
  QCOMPARE(unstagedFiles.at(1).first, QStringLiteral("file3.txt"));
  QCOMPARE(unstagedFiles.at(1).second, QStringLiteral("D"));
  QCOMPARE(unstagedFiles.at(2).first, QStringLiteral("new.txt"));
  QCOMPARE(unstagedFiles.at(2).second, QStringLiteral("?"));
}

void TestWorkingTreeModel::testClear() {
  WorkingTreeModel model;
  model.load({QStringLiteral("M\tfile.txt")}, {}, {});
  QVERIFY(!model.stagedFiles().isEmpty());

  model.clear();
  QVERIFY(model.stagedFiles().isEmpty());
  QVERIFY(model.unstagedFiles().isEmpty());
}

QTEST_MAIN(TestWorkingTreeModel)
#include "TestWorkingTreeModel.moc"
