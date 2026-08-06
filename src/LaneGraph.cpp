#include "LaneGraph.h"
#include "Lanes.h"

QVector<QVector<int>> LaneGraph::build(const QList<Commit> &commits) {
  QVector<QVector<int>> rows;
  if (commits.isEmpty())
    return rows;

  Lanes lanes;
  lanes.init(commits.first().fullSha);

  for (int r = 0; r < commits.size(); ++r) {
    const Commit &c = commits[r];

    lanes.changeActiveLane(c.fullSha);
    lanes.setBoundary(false);

    bool isDiscontinuity = false;
    if (lanes.isFork(c.fullSha, isDiscontinuity)) {
      lanes.setFork(c.fullSha);
    }

    if (c.parents.size() > 1)
      lanes.setMerge(c.parents);

    if (c.parents.isEmpty())
      lanes.setInitial();

    QVector<int> row;
    lanes.getLanes(row);
    rows.append(row);

    // Clean up for the next row. Order: afterMerge first, then afterFork,
    // then update the active lane's next expected commit.
    if (c.parents.size() > 1)
      lanes.afterMerge();

    lanes.afterFork();

    if (lanes.isBranch())
      lanes.afterBranch();

    const QString next = c.parents.isEmpty() ? QString() : c.parents.first();
    lanes.nextParent(next);
  }

  return rows;
}
