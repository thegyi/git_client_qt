#ifndef LANEGRAPH_H
#define LANEGRAPH_H

#include "CommitModel.h"

#include <QVector>

class LaneGraph {
public:
  // Process a top-down (newest first) list of commits and return
  // the qgit-style lane type vectors, one per commit.
  static QVector<QVector<int>> build(const QList<Commit> &commits);
};

#endif // LANEGRAPH_H
