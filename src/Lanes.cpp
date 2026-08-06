#include "Lanes.h"

void Lanes::init(const QString& expectedSha) {
    clear();
    activeLane = 0;
    setBoundary(false);
    add(Lane::BRANCH, expectedSha, activeLane);
}

void Lanes::clear() {
    typeVec.clear();
    nextShaVec.clear();
}

void Lanes::setBoundary(bool b) {
    NODE   = b ? Lane::BOUNDARY_C : Lane::MERGE_FORK;
    NODE_R = b ? Lane::BOUNDARY_R : Lane::MERGE_FORK_R;
    NODE_L = b ? Lane::BOUNDARY_L : Lane::MERGE_FORK_L;
    boundary = b;

    if (boundary)
        typeVec[activeLane] = Lane::BOUNDARY;
}

bool Lanes::isFork(const QString& sha, bool& isDiscontinuity) {

    int pos = findNextSha(sha, 0);
    isDiscontinuity = (activeLane != pos);
    if (pos == -1) // new branch case
        return false;

    return (findNextSha(sha, pos + 1) != -1);
}

void Lanes::setFork(const QString& sha) {

    int rangeStart, rangeEnd, idx;
    rangeStart = rangeEnd = idx = findNextSha(sha, 0);

    while (idx != -1) {
        rangeEnd = idx;
        typeVec[idx] = Lane::TAIL;
        idx = findNextSha(sha, idx + 1);
    }
    typeVec[activeLane] = NODE;

    int& startT = typeVec[rangeStart];
    int& endT = typeVec[rangeEnd];

    if (startT == NODE)
        startT = NODE_L;

    if (endT == NODE)
        endT = NODE_R;

    if (startT == Lane::TAIL)
        startT = Lane::TAIL_L;

    if (endT == Lane::TAIL)
        endT = Lane::TAIL_R;

    for (int i = rangeStart + 1; i < rangeEnd; i++) {

        int& t = typeVec[i];

        if (t == Lane::NOT_ACTIVE)
            t = Lane::CROSS;

        else if (t == Lane::EMPTY)
            t = Lane::CROSS_EMPTY;
    }
}

void Lanes::setMerge(const QStringList& parents) {
    // setFork() must be called before setMerge()

    if (boundary)
        return; // handle as a simple active line

    int& t = typeVec[activeLane];
    bool wasFork   = (t == NODE);
    bool wasFork_L = (t == NODE_L);
    bool wasFork_R = (t == NODE_R);
    bool startJoinWasACross = false, endJoinWasACross = false;

    t = NODE;

    int rangeStart = activeLane, rangeEnd = activeLane;
    QStringList::const_iterator it(parents.constBegin());
    for (++it; it != parents.constEnd(); ++it) { // skip first parent

        int idx = findNextSha(*it, 0);
        if (idx != -1) {

            if (idx > rangeEnd) {

                rangeEnd = idx;
                endJoinWasACross = typeVec[idx] == Lane::CROSS;
            }

            if (idx < rangeStart) {

                rangeStart = idx;
                startJoinWasACross = typeVec[idx] == Lane::CROSS;
            }

            typeVec[idx] = Lane::JOIN;
        } else
            rangeEnd = add(Lane::HEAD, *it, rangeEnd + 1);
    }
    int& startT = typeVec[rangeStart];
    int& endT = typeVec[rangeEnd];

    if (startT == NODE && !wasFork && !wasFork_R)
        startT = NODE_L;

    if (endT == NODE && !wasFork && !wasFork_L)
        endT = NODE_R;

    if (startT == Lane::JOIN && !startJoinWasACross)
        startT = Lane::JOIN_L;

    if (endT == Lane::JOIN && !endJoinWasACross)
        endT = Lane::JOIN_R;

    if (startT == Lane::HEAD)
        startT = Lane::HEAD_L;

    if (endT == Lane::HEAD)
        endT = Lane::HEAD_R;

    for (int i = rangeStart + 1; i < rangeEnd; i++) {

        int& t = typeVec[i];

        if (t == Lane::NOT_ACTIVE)
            t = Lane::CROSS;

        else if (t == Lane::EMPTY)
            t = Lane::CROSS_EMPTY;

        else if (t == Lane::TAIL_R || t == Lane::TAIL_L)
            t = Lane::TAIL;
    }
}

void Lanes::setInitial() {

    int& t = typeVec[activeLane];
    if (!isNode(t) && t != Lane::APPLIED)
        t = (boundary ? Lane::BOUNDARY : Lane::INITIAL);
}

void Lanes::setApplied() {

    // applied patches are not merges, nor forks
    typeVec[activeLane] = Lane::APPLIED; // TODO test with boundaries
}

void Lanes::changeActiveLane(const QString& sha) {

    int& t = typeVec[activeLane];
    if (t == Lane::INITIAL || Lane::isBoundary(t))
        t = Lane::EMPTY;
    else
        t = Lane::NOT_ACTIVE;

    int idx = findNextSha(sha, 0); // find first sha
    if (idx != -1)
        typeVec[idx] = Lane::ACTIVE; // called before setBoundary()
    else
        idx = add(Lane::BRANCH, sha, activeLane); // new branch

    activeLane = idx;
}

void Lanes::afterMerge() {

    if (boundary)
        return; // will be reset by changeActiveLane()

    for (int i = 0; i < typeVec.count(); i++) {

        int& t = typeVec[i];

        if (Lane::isHead(t) || Lane::isJoin(t) || t == Lane::CROSS)
            t = Lane::NOT_ACTIVE;

        else if (t == Lane::CROSS_EMPTY)
            t = Lane::EMPTY;

        else if (isNode(t))
            t = Lane::ACTIVE;
    }
}

void Lanes::afterFork() {

    for (int i = 0; i < typeVec.count(); i++) {

        int& t = typeVec[i];

        if (t == Lane::CROSS)
            t = Lane::NOT_ACTIVE;

        else if (Lane::isTail(t) || t == Lane::CROSS_EMPTY)
            t = Lane::EMPTY;

        if (!boundary && isNode(t))
            t = Lane::ACTIVE; // boundary will be reset by changeActiveLane()
    }
    while (typeVec.last() == Lane::EMPTY) {
        typeVec.pop_back();
        nextShaVec.pop_back();
    }
}

bool Lanes::isBranch() {

    return (typeVec[activeLane] == Lane::BRANCH);
}

void Lanes::afterBranch() {

    typeVec[activeLane] = Lane::ACTIVE; // TODO test with boundaries
}

void Lanes::afterApplied() {

    typeVec[activeLane] = Lane::ACTIVE; // TODO test with boundaries
}

void Lanes::nextParent(const QString& sha) {

    nextShaVec[activeLane] = (boundary ? QString() : sha);
}

int Lanes::findNextSha(const QString& next, int pos) {

    for (int i = pos; i < nextShaVec.count(); i++)
        if (nextShaVec[i] == next)
            return i;
    return -1;
}

int Lanes::findType(int type, int pos) {

    for (int i = pos; i < typeVec.count(); i++)
        if (typeVec[i] == type)
            return i;
    return -1;
}

int Lanes::add(int type, const QString& next, int pos) {

    // first check empty lanes starting from pos
    if (pos < (int)typeVec.count()) {
        pos = findType(Lane::EMPTY, pos);
        if (pos != -1) {
            typeVec[pos] = type;
            nextShaVec[pos] = next;
            return pos;
        }
    }
    // if all lanes are occupied add a new lane
    typeVec.append(type);
    nextShaVec.append(next);
    return typeVec.count() - 1;
}
