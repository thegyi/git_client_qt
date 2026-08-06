#ifndef COMMITMODEL_H
#define COMMITMODEL_H

#include <QAbstractTableModel>
#include <QString>
#include <QStringList>

struct Commit {
  QString graph;
  QString fullSha;
  QString shortSha;
  QString author;
  QString date;
  QString relative;
  QString subject;
  QString body;
  QString branch;
  QStringList parents;
};

class CommitModel : public QAbstractTableModel {
  Q_OBJECT

public:
  explicit CommitModel(QObject *parent = nullptr);

  void loadLog(const QString &rawLog);
  void clear();

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;

  const Commit &commit(int row) const;

private:
  QList<Commit> m_commits;
};

#endif // COMMITMODEL_H
