#ifndef WORKINGTREEMODEL_H
#define WORKINGTREEMODEL_H

#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>

using FileStatus = QPair<QString, QString>;

class WorkingTreeModel : public QObject {
  Q_OBJECT

public:
  explicit WorkingTreeModel(QObject *parent = nullptr);

  void load(const QStringList &staged, const QStringList &unstaged,
            const QStringList &untracked);
  void clear();

  const QList<FileStatus> &stagedFiles() const;
  const QList<FileStatus> &unstagedFiles() const;
  const QList<FileStatus> &untrackedFiles() const;

private:
  QList<FileStatus> m_staged;
  QList<FileStatus> m_unstaged;
  QList<FileStatus> m_untracked;
};

#endif // WORKINGTREEMODEL_H
