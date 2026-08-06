#ifndef LANES_H
#define LANES_H

#include <QString>
#include <QStringList>
#include <QVector>

namespace Lane {

enum Type {
    EMPTY = 0,
    ACTIVE,
    NOT_ACTIVE,
    MERGE_FORK,
    MERGE_FORK_R,
    MERGE_FORK_L,
    JOIN,
    JOIN_R,
    JOIN_L,
    HEAD,
    HEAD_R,
    HEAD_L,
    BRANCH,
    TAIL,
    TAIL_R,
    TAIL_L,
    INITIAL,
    BOUNDARY,
    BOUNDARY_C,
    BOUNDARY_R,
    BOUNDARY_L,
    CROSS,
    CROSS_EMPTY,
    UNAPPLIED,
    APPLIED,
    LANE_TYPES_NUM
};

inline bool isHead(int t) {
    return t == HEAD || t == HEAD_R || t == HEAD_L;
}

inline bool isTail(int t) {
    return t == TAIL || t == TAIL_R || t == TAIL_L;
}

inline bool isJoin(int t) {
    return t == JOIN || t == JOIN_R || t == JOIN_L;
}

inline bool isBoundary(int t) {
    return t == BOUNDARY || t == BOUNDARY_C || t == BOUNDARY_R || t == BOUNDARY_L;
}

} // namespace Lane

class Lanes {
public:
    Lanes() {}
    bool isEmpty() const { return typeVec.isEmpty(); }
    void init(const QString& expectedSha);
    void clear();
    bool isFork(const QString& sha, bool& isDiscontinuity);
    void setBoundary(bool isBoundary);
    void setFork(const QString& sha);
    void setMerge(const QStringList& parents);
    void setInitial();
    void setApplied();
    void changeActiveLane(const QString& sha);
    void afterMerge();
    void afterFork();
    bool isBranch();
    void afterBranch();
    void afterApplied();
    void nextParent(const QString& sha);
    void getLanes(QVector<int>& ln) const { ln = typeVec; }

private:
    bool isNode(int t) const { return t == NODE || t == NODE_R || t == NODE_L; }
    int findNextSha(const QString& next, int pos);
    int findType(int type, int pos);
    int add(int type, const QString& next, int pos);

    int activeLane = 0;
    QVector<int> typeVec;
    QVector<QString> nextShaVec;
    bool boundary = false;
    int NODE = Lane::MERGE_FORK;
    int NODE_R = Lane::MERGE_FORK_R;
    int NODE_L = Lane::MERGE_FORK_L;
};

#endif // LANES_H
