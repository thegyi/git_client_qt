#include "CommitModel.h"

CommitModel::CommitModel(QObject *parent) : QAbstractTableModel(parent) {}

void CommitModel::loadLog(const QString &rawLog) {
  beginResetModel();
  m_commits.clear();

  for (const QString &record : rawLog.split(QChar(0x1e), Qt::SkipEmptyParts)) {
    const QStringList fields = record.split(QChar(0x1f), Qt::KeepEmptyParts);
    if (fields.size() < 9)
      continue;

    Commit c;
    c.graph = fields.at(0);
    c.fullSha = fields.at(1);
    c.shortSha = fields.at(2);
    c.author = fields.at(3);
    c.date = fields.at(4);
    c.relative = fields.at(5);
    c.subject = fields.at(6);
    c.subject.remove(QStringLiteral("[skip ci]"), Qt::CaseInsensitive);
    c.subject = c.subject.trimmed();
    c.body = fields.at(7).trimmed();
    c.branch = fields.at(8);
    if (c.branch.startsWith(QStringLiteral("refs/heads/")))
      c.branch = c.branch.mid(11);
    else if (c.branch.startsWith(QStringLiteral("refs/remotes/")))
      c.branch = c.branch.mid(13);
    else if (c.branch.startsWith(QStringLiteral("refs/tags/")))
      c.branch = c.branch.mid(10);

    m_commits.append(c);
  }

  endResetModel();
}

void CommitModel::clear() {
  beginResetModel();
  m_commits.clear();
  endResetModel();
}

int CommitModel::rowCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return m_commits.size();
}

int CommitModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return 7;
}

QVariant CommitModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= m_commits.size())
    return QVariant();

  const Commit &c = m_commits.at(index.row());
  if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
    switch (index.column()) {
    case 0:
      return c.graph;
    case 1:
      return c.date;
    case 2:
      return c.relative;
    case 3:
      return c.subject;
    case 4:
      return c.author;
    case 5:
      return c.branch;
    case 6:
      return c.shortSha;
    default:
      return QVariant();
    }
  }
  if (role == Qt::UserRole && index.column() == 6)
    return c.fullSha;

  return QVariant();
}

QVariant CommitModel::headerData(int section, Qt::Orientation orientation,
                                 int role) const {
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    return QVariant();

  switch (section) {
  case 0:
    return tr("Graph");
  case 1:
    return tr("Date/Time");
  case 2:
    return tr("Date");
  case 3:
    return tr("Commit Message");
  case 4:
    return tr("Author");
  case 5:
    return tr("Branches");
  case 6:
    return tr("SHA");
  default:
    return QVariant();
  }
}

const Commit &CommitModel::commit(int row) const { return m_commits.at(row); }
