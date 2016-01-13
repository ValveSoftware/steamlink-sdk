/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

// When building with debug code, the macro below will enable debug helpers when using libc++.
// For example, the std::vector<T>::operator[] will use _LIBCPP_ASSERT to check if the index is
// within the array bounds. Note that this only works reliably with OSX 10.9 or later.
//#define _LIBCPP_DEBUG2 2

#include "qv4ssa_p.h"
#include "qv4isel_util_p.h"
#include "qv4util_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QStringList>
#include <QtCore/QSet>
#include <QtCore/QLinkedList>
#include <QtCore/QStack>
#include <qv4runtime_p.h>
#include <cmath>
#include <iostream>
#include <cassert>
#include <algorithm>

#undef SHOW_SSA
#undef DEBUG_MOVEMAPPING

QT_USE_NAMESPACE

using namespace QV4;
using namespace IR;

namespace {

#ifdef QT_NO_DEBUG
enum { DoVerification = 0 };
#else
enum { DoVerification = 1 };
#endif

Q_GLOBAL_STATIC_WITH_ARGS(QTextStream, qout, (stderr, QIODevice::WriteOnly));
#define qout *qout()

void showMeTheCode(IR::Function *function)
{
    static bool showCode = !qgetenv("QV4_SHOW_IR").isNull();
    if (showCode) {
        IRPrinter(&qout).print(function);
        qout << endl;
    }
}

class ProcessedBlocks
{
    QBitArray processed;

public:
    ProcessedBlocks(IR::Function *function)
    {
        processed = QBitArray(function->basicBlockCount(), false);
    }

    bool alreadyProcessed(BasicBlock *bb) const
    {
        Q_ASSERT(bb);

        return processed.at(bb->index());
    }

    void markAsProcessed(BasicBlock *bb)
    {
        processed.setBit(bb->index());
    }
};

class BasicBlockSet
{
    typedef std::vector<int> Numbers;
    typedef std::vector<bool> Flags;

    Numbers *blockNumbers;
    Flags *blockFlags;
    IR::Function *function;
    enum { MaxVectorCapacity = 8 };

public:
    class const_iterator
    {
        const BasicBlockSet &set;
        // ### These two members could go into a union, but clang won't compile (https://codereview.qt-project.org/#change,74259)
        Numbers::const_iterator numberIt;
        size_t flagIt;

        friend class BasicBlockSet;
        const_iterator(const BasicBlockSet &set, bool end)
            : set(set)
        {
            if (end || !set.function) {
                if (set.blockNumbers)
                    numberIt = set.blockNumbers->end();
                else
                    flagIt = set.blockFlags->size();
            } else {
                if (set.blockNumbers)
                    numberIt = set.blockNumbers->begin();
                else
                    findNextWithFlags(0);
            }
        }

        void findNextWithFlags(size_t start)
        {
            flagIt = std::distance(set.blockFlags->begin(),
                                   std::find(set.blockFlags->begin() + start,
                                             set.blockFlags->end(),
                                             true));

            // The ++operator of std::vector<bool>::iterator in libc++ has a bug when using it on an
            // iterator pointing to the last element. It will not be set to ::end(), but beyond
            // that. (It will be set to the first multiple of the native word size that is bigger
            // than size().)
            //
            // See http://llvm.org/bugs/show_bug.cgi?id=19663
            //
            // As we use the size to for our end() iterator, take the minimum of the size and the
            // distance for the flagIt:
            flagIt = qMin(flagIt, set.blockFlags->size());

            Q_ASSERT(flagIt <= set.blockFlags->size());
        }

    public:
        BasicBlock *operator*() const
        {

            if (set.blockNumbers) {
                return set.function->basicBlock(*numberIt);
            } else {
                Q_ASSERT(flagIt <= static_cast<size_t>(set.function->basicBlockCount()));
                return set.function->basicBlock(static_cast<int>(flagIt));
            }
        }

        bool operator==(const const_iterator &other) const
        {
            if (&set != &other.set)
                return false;
            if (set.blockNumbers)
                return numberIt == other.numberIt;
            else
                return flagIt == other.flagIt;
        }

        bool operator!=(const const_iterator &other) const
        { return !(*this == other); }

        const_iterator &operator++()
        {
            if (set.blockNumbers)
                ++numberIt;
            else
                findNextWithFlags(flagIt + 1);

            return *this;
        }
    };

    friend class const_iterator;

public:
    BasicBlockSet(IR::Function *f = 0): blockNumbers(0), blockFlags(0), function(0)
    {
        if (f)
            init(f);
    }

#ifdef Q_COMPILER_RVALUE_REFS
    BasicBlockSet(BasicBlockSet &&other): blockNumbers(0), blockFlags(0)
    {
        std::swap(blockNumbers, other.blockNumbers);
        std::swap(blockFlags, other.blockFlags);
        std::swap(function, other.function);
    }
#endif // Q_COMPILER_RVALUE_REFS

    BasicBlockSet(const BasicBlockSet &other)
        : blockNumbers(0)
        , blockFlags(0)
        , function(other.function)
    {
        if (other.blockFlags)
            blockFlags = new Flags(*other.blockFlags);
        else if (other.blockNumbers)
            blockNumbers = new Numbers(*other.blockNumbers);
    }

    BasicBlockSet &operator=(const BasicBlockSet &other)
    {
        if (blockFlags) {
            delete blockFlags;
            blockFlags = 0;
        } else {
            delete blockNumbers;
            blockNumbers = 0;
        }
        function = other.function;
        if (other.blockFlags)
            blockFlags = new Flags(*other.blockFlags);
        else if (other.blockNumbers)
            blockNumbers = new Numbers(*other.blockNumbers);
        return *this;
    }

    ~BasicBlockSet() { delete blockNumbers; delete blockFlags; }

    void init(IR::Function *f)
    {
        Q_ASSERT(!function);
        Q_ASSERT(f);
        function = f;
        blockNumbers = new Numbers;
        blockNumbers->reserve(MaxVectorCapacity);
    }

    bool empty() const
    {
        return begin() == end();
    }

    void insert(BasicBlock *bb)
    {
        Q_ASSERT(function);

        if (blockFlags) {
            (*blockFlags)[bb->index()] = true;
            return;
        }

        for (std::vector<int>::const_iterator i = blockNumbers->begin(), ei = blockNumbers->end();
             i != ei; ++i)
            if (*i == bb->index())
                return;

        if (blockNumbers->size() == MaxVectorCapacity) {
            blockFlags = new Flags(function->basicBlockCount(), false);
            for (std::vector<int>::const_iterator i = blockNumbers->begin(), ei = blockNumbers->end();
                 i != ei; ++i)
                blockFlags->operator[](*i) = true;
            delete blockNumbers;
            blockNumbers = 0;
            blockFlags->operator[](bb->index()) = true;
        } else {
            blockNumbers->push_back(bb->index());
        }
    }

    void remove(BasicBlock *bb)
    {
        Q_ASSERT(function);

        if (blockFlags) {
            (*blockFlags)[bb->index()] = false;
            return;
        }

        for (std::vector<int>::iterator i = blockNumbers->begin(), ei = blockNumbers->end(); i != ei; ++i) {
            if (*i == bb->index()) {
                blockNumbers->erase(i);
                return;
            }
        }
    }

    const_iterator begin() const { return const_iterator(*this, false); }
    const_iterator end() const { return const_iterator(*this, true); }

    void collectValues(std::vector<BasicBlock *> &bbs) const
    {
        Q_ASSERT(function);

        for (const_iterator it = begin(), eit = end(); it != eit; ++it)
            bbs.push_back(*it);
    }

    bool contains(BasicBlock *bb) const
    {
        Q_ASSERT(function);

        if (blockFlags)
            return (*blockFlags)[bb->index()];

        for (std::vector<int>::const_iterator i = blockNumbers->begin(), ei = blockNumbers->end(); i != ei; ++i) {
            if (*i == bb->index())
                return true;
        }

        return false;
    }
};

class DominatorTree
{
    enum {
        DebugDominatorFrontiers = 0,
        DebugImmediateDominators = 0,

        DebugCodeCanUseLotsOfCpu = 0
    };

    typedef int BasicBlockIndex;
    enum { InvalidBasicBlockIndex = -1 };

    struct Data
    {
        int N;
        std::vector<int> dfnum; // BasicBlock index -> dfnum
        std::vector<int> vertex;
        std::vector<BasicBlockIndex> parent; // BasicBlock index -> parent BasicBlock index
        std::vector<BasicBlockIndex> ancestor; // BasicBlock index -> ancestor BasicBlock index
        std::vector<BasicBlockIndex> best; // BasicBlock index -> best BasicBlock index
        std::vector<BasicBlockIndex> semi; // BasicBlock index -> semi dominator BasicBlock index
        std::vector<BasicBlockIndex> samedom; // BasicBlock index -> same dominator BasicBlock index

        Data(): N(0) {}
    };

    IR::Function *function;
    QScopedPointer<Data> d;
    std::vector<BasicBlockIndex> idom; // BasicBlock index -> immediate dominator BasicBlock index
    std::vector<BasicBlockSet> DF; // BasicBlock index -> dominator frontier

    struct DFSTodo {
        BasicBlockIndex node, parent;

        DFSTodo()
            : node(InvalidBasicBlockIndex)
            , parent(InvalidBasicBlockIndex)
        {}

        DFSTodo(BasicBlockIndex node, BasicBlockIndex parent)
            : node(node)
            , parent(parent)
        {}
    };

    void DFS(BasicBlockIndex node) {
        std::vector<DFSTodo> worklist;
        worklist.reserve(d->vertex.capacity() / 2);
        DFSTodo todo(node, InvalidBasicBlockIndex);

        while (true) {
            BasicBlockIndex n = todo.node;

            if (d->dfnum[n] == 0) {
                d->dfnum[n] = d->N;
                d->vertex[d->N] = n;
                d->parent[n] = todo.parent;
                ++d->N;
                const QVector<BasicBlock *> &out = function->basicBlock(n)->out;
                for (int i = out.size() - 1; i > 0; --i)
                    worklist.push_back(DFSTodo(out[i]->index(), n));

                if (out.size() > 0) {
                    todo.node = out.first()->index();
                    todo.parent = n;
                    continue;
                }
            }

            if (worklist.empty())
                break;

            todo = worklist.back();
            worklist.pop_back();
        }

#if defined(SHOW_SSA)
        for (int i = 0; i < nodes.size(); ++i)
            qDebug("\tL%d: dfnum = %d, parent = %d", i, dfnum[i], parent[i]);
#endif // SHOW_SSA
    }

    BasicBlockIndex ancestorWithLowestSemi(BasicBlockIndex v, std::vector<BasicBlockIndex> &worklist) {
        worklist.clear();
        for (BasicBlockIndex it = v; it != InvalidBasicBlockIndex; it = d->ancestor[it])
            worklist.push_back(it);

        if (worklist.size() < 2)
            return d->best[v];

        BasicBlockIndex b = InvalidBasicBlockIndex;
        BasicBlockIndex last = worklist.back();
        Q_ASSERT(worklist.size() <= INT_MAX);
        for (int it = static_cast<int>(worklist.size()) - 2; it >= 0; --it) {
            BasicBlockIndex bbIt = worklist[it];
            d->ancestor[bbIt] = last;
            BasicBlockIndex &best_it = d->best[bbIt];
            if (b != InvalidBasicBlockIndex && d->dfnum[d->semi[b]] < d->dfnum[d->semi[best_it]])
                best_it = b;
            else
                b = best_it;
        }
        return b;
    }

    void link(BasicBlockIndex p, BasicBlockIndex n) {
        d->ancestor[n] = p;
        d->best[n] = n;
    }

    void calculateIDoms() {
        Q_ASSERT(function->basicBlock(0)->in.isEmpty());

        const int bbCount = function->basicBlockCount();
        d->vertex = std::vector<int>(bbCount, InvalidBasicBlockIndex);
        d->parent = std::vector<int>(bbCount, InvalidBasicBlockIndex);
        d->dfnum = std::vector<int>(bbCount, 0);
        d->semi = std::vector<BasicBlockIndex>(bbCount, InvalidBasicBlockIndex);
        d->ancestor = std::vector<BasicBlockIndex>(bbCount, InvalidBasicBlockIndex);
        idom = std::vector<BasicBlockIndex>(bbCount, InvalidBasicBlockIndex);
        d->samedom = std::vector<BasicBlockIndex>(bbCount, InvalidBasicBlockIndex);
        d->best = std::vector<BasicBlockIndex>(bbCount, InvalidBasicBlockIndex);

        QHash<BasicBlockIndex, std::vector<BasicBlockIndex> > bucket;
        bucket.reserve(bbCount);

        DFS(function->basicBlock(0)->index());
        Q_ASSERT(d->N == function->liveBasicBlocksCount());

        std::vector<BasicBlockIndex> worklist;
        worklist.reserve(d->vertex.capacity() / 2);

        for (int i = d->N - 1; i > 0; --i) {
            BasicBlockIndex n = d->vertex[i];
            BasicBlockIndex p = d->parent[n];
            BasicBlockIndex s = p;

            foreach (BasicBlock *v, function->basicBlock(n)->in) {
                BasicBlockIndex ss = InvalidBasicBlockIndex;
                if (d->dfnum[v->index()] <= d->dfnum[n])
                    ss = v->index();
                else
                    ss = d->semi[ancestorWithLowestSemi(v->index(), worklist)];
                if (d->dfnum[ss] < d->dfnum[s])
                    s = ss;
            }
            d->semi[n] = s;
            bucket[s].push_back(n);
            link(p, n);
            if (bucket.contains(p)) {
                foreach (BasicBlockIndex v, bucket[p]) {
                    BasicBlockIndex y = ancestorWithLowestSemi(v, worklist);
                    BasicBlockIndex semi_v = d->semi[v];
                    if (d->semi[y] == semi_v)
                        idom[v] = semi_v;
                    else
                        d->samedom[v] = y;
                }
                bucket.remove(p);
            }
        }

        for (int i = 1; i < d->N; ++i) {
            BasicBlockIndex n = d->vertex[i];
            Q_ASSERT(n != InvalidBasicBlockIndex);
            Q_ASSERT(!bucket.contains(n));
            Q_ASSERT(d->ancestor[n] != InvalidBasicBlockIndex
                        && ((d->semi[n] != InvalidBasicBlockIndex
                                && d->dfnum[d->ancestor[n]] <= d->dfnum[d->semi[n]]) || d->semi[n] == n));
            BasicBlockIndex sdn = d->samedom[n];
            if (sdn != InvalidBasicBlockIndex)
                idom[n] = idom[sdn];
        }

        dumpImmediateDominators();
    }

    struct NodeProgress {
        std::vector<BasicBlockIndex> children;
        std::vector<BasicBlockIndex> todo;
    };

public:
    DominatorTree(IR::Function *function)
        : function(function)
        , d(new Data)
    {
        calculateIDoms();
        d.reset();
    }

    void computeDF() {
        DF.resize(function->basicBlockCount());

        // compute children of each node in the dominator tree
        std::vector<std::vector<BasicBlockIndex> > children; // BasicBlock index -> children
        children.resize(function->basicBlockCount());
        foreach (BasicBlock *n, function->basicBlocks()) {
            if (n->isRemoved())
                continue;
            const BasicBlockIndex nodeIndex = n->index();
            Q_ASSERT(function->basicBlock(nodeIndex) == n);
            const BasicBlockIndex nodeDominator = idom[nodeIndex];
            if (nodeDominator == InvalidBasicBlockIndex)
                continue; // there is no dominator to add this node to as a child (e.g. the start node)
            children[nodeDominator].push_back(nodeIndex);
        }

        // Fill the worklist and initialize the node status for each basic-block
        std::vector<NodeProgress> nodeStatus;
        nodeStatus.resize(function->basicBlockCount());
        std::vector<BasicBlockIndex> worklist;
        worklist.reserve(function->basicBlockCount());
        foreach (BasicBlock *bb, function->basicBlocks()) {
            if (bb->isRemoved())
                continue;
            BasicBlockIndex nodeIndex = bb->index();
            worklist.push_back(nodeIndex);
            NodeProgress &np = nodeStatus[nodeIndex];
            np.children = children[nodeIndex];
            np.todo = children[nodeIndex];
        }

        std::vector<bool> DF_done(function->basicBlockCount(), false);

        while (!worklist.empty()) {
            BasicBlockIndex node = worklist.back();

            if (DF_done[node]) {
                worklist.pop_back();
                continue;
            }

            NodeProgress &np = nodeStatus[node];
            std::vector<BasicBlockIndex>::iterator it = np.todo.begin();
            while (it != np.todo.end()) {
                if (DF_done[*it]) {
                    it = np.todo.erase(it);
                } else {
                    worklist.push_back(*it);
                    break;
                }
            }

            if (np.todo.empty()) {
                BasicBlockSet &S = DF[node];
                S.init(function);
                foreach (BasicBlock *y, function->basicBlock(node)->out)
                    if (idom[y->index()] != node)
                        S.insert(y);
                foreach (BasicBlockIndex child, np.children) {
                    const BasicBlockSet &ws = DF[child];
                    for (BasicBlockSet::const_iterator it = ws.begin(), eit = ws.end(); it != eit; ++it) {
                        BasicBlock *w = *it;
                        const BasicBlockIndex wIndex = w->index();
                        if (node == wIndex || !dominates(node, w->index()))
                            S.insert(w);
                    }
                }
                DF_done[node] = true;
                worklist.pop_back();
            }
        }

        if (DebugDominatorFrontiers) {
            qout << "Dominator Frontiers:" << endl;
            foreach (BasicBlock *n, function->basicBlocks()) {
                if (n->isRemoved())
                    continue;

                qout << "\tDF[" << n->index() << "]: {";
                const BasicBlockSet &SList = DF[n->index()];
                for (BasicBlockSet::const_iterator i = SList.begin(), ei = SList.end(); i != ei; ++i) {
                    if (i != SList.begin())
                        qout << ", ";
                    qout << (*i)->index();
                }
                qout << "}" << endl;
            }
        }

        if (DebugDominatorFrontiers && DebugCodeCanUseLotsOfCpu) {
            foreach (BasicBlock *n, function->basicBlocks()) {
                if (n->isRemoved())
                    continue;
                const BasicBlockSet &fBlocks = DF[n->index()];
                for (BasicBlockSet::const_iterator it = fBlocks.begin(), eit = fBlocks.end(); it != eit; ++it) {
                    BasicBlock *fBlock = *it;
                    Q_ASSERT(!dominates(n, fBlock) || fBlock == n);
                    bool hasDominatedSucc = false;
                    foreach (BasicBlock *succ, fBlock->in) {
                        if (dominates(n, succ)) {
                            hasDominatedSucc = true;
                            break;
                        }
                    }
                    if (!hasDominatedSucc) {
                        qout << fBlock << " in DF[" << n->index() << "] has no dominated predecessors" << endl;
                    }
                    Q_ASSERT(hasDominatedSucc);
                }
            }
        }
    }

    const BasicBlockSet &dominatorFrontier(BasicBlock *n) const {
        return DF[n->index()];
    }

    BasicBlock *immediateDominator(BasicBlock *bb) const {
        const BasicBlockIndex idx = idom[bb->index()];
        if (idx == -1)
            return 0;
        return function->basicBlock(idx);
    }

    void dumpImmediateDominators() const
    {
        if (DebugImmediateDominators) {
            qout << "Immediate dominators:" << endl;
            foreach (BasicBlock *to, function->basicBlocks()) {
                if (to->isRemoved())
                    continue;

                qout << '\t';
                BasicBlockIndex from = idom.at(to->index());
                if (from != InvalidBasicBlockIndex)
                    qout << from;
                else
                    qout << "(none)";
                qout << " -> " << to->index() << endl;
            }
        }
    }

    void setImmediateDominator(BasicBlock *bb, BasicBlock *newDominator)
    {
        Q_ASSERT(bb->index() >= 0);
        Q_ASSERT(!newDominator || newDominator->index() >= 0);

        if (static_cast<std::vector<BasicBlockIndex>::size_type>(bb->index()) >= idom.size()) {
            // This is a new block, probably introduced by edge splitting. So, we'll have to grow
            // the array before inserting the immediate dominator.
            idom.resize(function->basicBlockCount(), InvalidBasicBlockIndex);
        }

        const BasicBlockIndex newIdx = newDominator ? newDominator->index() : InvalidBasicBlockIndex;
        if (DebugImmediateDominators)
            qDebug() << "Setting idom of" << bb->index() << "from" << idom[bb->index()] << "to" << newIdx;
        idom[bb->index()] = newIdx;
    }

    void collectSiblings(BasicBlock *node, BasicBlockSet &siblings)
    {
        siblings.insert(node);
        const BasicBlockIndex dominator = idom[node->index()];
        if (dominator == InvalidBasicBlockIndex)
            return;
        for (size_t i = 0, ei = idom.size(); i != ei; ++i) {
            if (idom[i] == dominator) {
                BasicBlock *bb = function->basicBlock(i);
                if (!bb->isRemoved())
                    siblings.insert(bb);
            }
        }
    }

    void recalculateIDoms(const BasicBlockSet &nodes, BasicBlock *limit = 0)
    {
        const BasicBlockIndex limitIndex = limit ? limit->index() : InvalidBasicBlockIndex;
        BasicBlockSet todo(nodes), postponed(function);
        while (!todo.empty())
            recalculateIDom(*todo.begin(), todo, postponed, limitIndex);
    }

    bool dominates(BasicBlock *dominator, BasicBlock *dominated) const {
        return dominates(dominator->index(), dominated->index());
    }

    struct Cmp {
        std::vector<int> *nodeDepths;
        Cmp(std::vector<int> *nodeDepths)
            : nodeDepths(nodeDepths)
            { Q_ASSERT(nodeDepths); }
        bool operator()(BasicBlock *one, BasicBlock *two) const
            {
                if (one->isRemoved())
                    return false;
                if (two->isRemoved())
                    return true;
                return nodeDepths->at(one->index()) > nodeDepths->at(two->index());
            }
    };

    // Calculate a depth-first iteration order on the nodes of the dominator tree.
    //
    // The order of the nodes in the vector is not the same as one where a recursive depth-first
    // iteration is done on a tree. Rather, the nodes are (reverse) sorted on tree depth.
    // So for the:
    //    1 dominates 2
    //    2 dominates 3
    //    3 dominates 4
    //    2 dominates 5
    // the order will be:
    //    4, 3, 5, 2, 1
    // or:
    //    4, 5, 3, 2, 1
    // So the order of nodes on the same depth is undefined, but it will be after the nodes
    // they dominate, and before the nodes that dominate them.
    //
    // The reason for this order is that a proper DFS pre-/post-order would require inverting
    // the idom vector by either building a real tree datastructure or by searching the idoms
    // for siblings and children. Both have a higher time complexity than sorting by depth.
    QVector<BasicBlock *> calculateDFNodeIterOrder() const
    {
        std::vector<int> depths = calculateNodeDepths();
        QVector<BasicBlock *> order = function->basicBlocks();
        std::sort(order.begin(), order.end(), Cmp(&depths));
        for (int i = 0; i < order.size(); ) {
            if (order[i]->isRemoved())
                order.remove(i);
            else
                ++i;
        }
        return order;
    }

private:
    bool dominates(BasicBlockIndex dominator, BasicBlockIndex dominated) const {
        // dominator can be Invalid when the dominated block has no dominator (i.e. the start node)
        Q_ASSERT(dominated != InvalidBasicBlockIndex);

        if (dominator == dominated)
            return false;

        for (BasicBlockIndex it = idom[dominated]; it != InvalidBasicBlockIndex; it = idom[it]) {
            if (it == dominator)
                return true;
        }

        return false;
    }

    // Algorithm:
    //  - for each node:
    //    - get the depth of a node. If it's unknown (-1):
    //      - get the depth of the immediate dominator.
    //      - if that's unknown too, calculate it by calling calculateNodeDepth
    //      - set the current node's depth to that of immediate dominator + 1
    std::vector<int> calculateNodeDepths() const
    {
        std::vector<int> nodeDepths(function->basicBlockCount(), -1);
        nodeDepths[0] = 0;
        foreach (BasicBlock *bb, function->basicBlocks()) {
            if (bb->isRemoved())
                continue;

            int &bbDepth = nodeDepths[bb->index()];
            if (bbDepth == -1) {
                const int immDom = idom[bb->index()];
                int immDomDepth = nodeDepths[immDom];
                if (immDomDepth == -1)
                    immDomDepth = calculateNodeDepth(immDom, nodeDepths);
                bbDepth = immDomDepth + 1;
            }
        }
        return nodeDepths;
    }

    // Algorithm:
    //   - search for the first dominator of a node that has a known depth. As all nodes are
    //     reachable from the start node, and that node's depth is 0, this is finite.
    //   - while doing that search, put all unknown nodes in the worklist
    //   - pop all nodes from the worklist, and set their depth to the previous' (== dominating)
    //     node's depth + 1
    // This way every node's depth is calculated once, and the complexity is O(n).
    int calculateNodeDepth(int nodeIdx, std::vector<int> &nodeDepths) const
    {
        std::vector<int> worklist;
        worklist.reserve(8);
        int depth = -1;

        do {
            worklist.push_back(nodeIdx);
            nodeIdx = idom[nodeIdx];
            depth = nodeDepths[nodeIdx];
        } while (depth == -1);

        for (std::vector<int>::const_reverse_iterator it = worklist.rbegin(), eit = worklist.rend(); it != eit; ++it)
            nodeDepths[*it] = ++depth;

        return depth;
    }

    // The immediate-dominator recalculation is used when edges are removed from the CFG. See
    // [Ramalingam] for a description. Note that instead of calculating the priority, a recursive
    // algorithm is used: when recalculating the immediate dominator of a node by looking for the
    // least-common ancestor, and a node is hit that also needs recalculation, a recursive call
    // is done to calculate that nodes immediate dominator first.
    //
    // Note that this simplified algorithm cannot cope with back-edges. It only works for
    // non-looping edges (which is our use-case).
    void recalculateIDom(BasicBlock *node, BasicBlockSet &todo, BasicBlockSet &postponed, BasicBlockIndex limit) {
        Q_ASSERT(!postponed.contains(node));
        Q_ASSERT(todo.contains(node));
        todo.remove(node);

        if (node->in.size() == 1) {
            // Special case: if the node has only one incoming edge, then that is the immediate
            // dominator.
            setImmediateDominator(node, node->in.first());
            return;
        }

        std::vector<BasicBlockIndex> prefix;
        prefix.reserve(32);

        for (int i = 0, ei = node->in.size(); i != ei; ++i) {
            BasicBlock *in = node->in.at(i);
            if (node == in) // back-edge to self
                continue;
            if (dominates(node->index(), in->index())) // a known back-edge
                continue;

            if (prefix.empty()) {
                calculatePrefix(node, in, prefix, todo, postponed, limit);

                if (!prefix.empty()) {
                    std::reverse(prefix.begin(), prefix.end());
                    Q_ASSERT(!prefix.empty());
                    Q_ASSERT(prefix.front() == limit || limit == InvalidBasicBlockIndex);
                }
            } else {
                std::vector<BasicBlockIndex> anotherPrefix;
                anotherPrefix.reserve(prefix.size());
                calculatePrefix(node, in, anotherPrefix, todo, postponed, limit);

                if (!anotherPrefix.empty())
                    commonPrefix(prefix, anotherPrefix);
            }
        }

        Q_ASSERT(!prefix.empty());
        idom[node->index()] = prefix.back();
    }

    void calculatePrefix(BasicBlock *node, BasicBlock *in, std::vector<BasicBlockIndex> &prefix, BasicBlockSet &todo, BasicBlockSet &postponed, BasicBlockIndex limit)
    {
        for (BasicBlockIndex it = in->index(); it != InvalidBasicBlockIndex; it = idom[it]) {
            prefix.push_back(it);
            if (it == limit)
                return;
            BasicBlock *n = function->basicBlock(it);
            if (postponed.contains(n)) { // possible back-edge, bail out.
                prefix.clear();
                return;
            }
            if (todo.contains(n)) {
                postponed.insert(node);
                recalculateIDom(n, todo, postponed, limit);
                postponed.remove(node);
            }
        }
    }

    // Calculate the LCA (Least Common Ancestor) by finding the longest common prefix between two
    // dominator chains. Note that "anotherPrefix" has the node's immediate dominator first, while
    // "bestPrefix" has it last (meaning: is in reverse order). The reason for this is that removing
    // nodes from "bestPrefix" is cheaper because it's done at the end of the vector, while
    // reversing all "anotherPrefix" nodes would take unnecessary time.
    static void commonPrefix(std::vector<BasicBlockIndex> &bestPrefix, const std::vector<BasicBlockIndex> &anotherPrefix)
    {
        const size_t anotherSize = anotherPrefix.size();
        size_t minLen = qMin(bestPrefix.size(), anotherPrefix.size());
        while (minLen != 0) {
            --minLen;
            if (bestPrefix[minLen] == anotherPrefix[anotherSize - minLen - 1]) {
                ++minLen;
                break;
            }
        }
        if (minLen != bestPrefix.size())
            bestPrefix.erase(bestPrefix.begin() + minLen, bestPrefix.end());
    }
};

class VariableCollector: public StmtVisitor, ExprVisitor {
    std::vector<Temp> _allTemps;
    std::vector<BasicBlockSet> _defsites;
    std::vector<std::vector<int> > A_orig;
    std::vector<bool> nonLocals;
    std::vector<bool> killed;

    BasicBlock *currentBB;
    bool isCollectable(Temp *t) const
    {
        Q_UNUSED(t);
        Q_ASSERT(t->kind != Temp::PhysicalRegister && t->kind != Temp::StackSlot);
        return true;
    }

    void addDefInCurrentBlock(Temp *t)
    {
        std::vector<int> &temps = A_orig[currentBB->index()];
        if (std::find(temps.begin(), temps.end(), t->index) == temps.end())
            temps.push_back(t->index);
    }

    void addTemp(Temp *t)
    {
        if (_allTemps[t->index].kind == Temp::Invalid)
            _allTemps[t->index] = *t;
    }

public:
    VariableCollector(IR::Function *function)
    {
        _allTemps.resize(function->tempCount);
        _defsites.resize(function->tempCount);
        for (int i = 0; i < function->tempCount; ++i)
            _defsites[i].init(function);
        nonLocals.resize(function->tempCount);
        A_orig.resize(function->basicBlockCount());
        for (int i = 0, ei = A_orig.size(); i != ei; ++i)
            A_orig[i].reserve(8);

#if defined(SHOW_SSA)
        qout << "Variables collected:" << endl;
#endif // SHOW_SSA

        foreach (BasicBlock *bb, function->basicBlocks()) {
            if (bb->isRemoved())
                continue;

            currentBB = bb;
            killed.assign(function->tempCount, false);
            foreach (Stmt *s, bb->statements())
                s->accept(this);
        }

#if defined(SHOW_SSA)
        qout << "Non-locals:" << endl;
        foreach (const Temp &nonLocal, nonLocals) {
            qout << "\t";
            nonLocal.dump(qout);
            qout << endl;
        }

        qout << "end collected variables." << endl;
#endif // SHOW_SSA
    }

    const std::vector<Temp> &allTemps() const
    { return _allTemps; }

    void collectDefSites(const Temp &n, std::vector<BasicBlock *> &bbs) const {
        Q_ASSERT(!n.isInvalid());
        Q_ASSERT(n.index < _defsites.size());
        _defsites[n.index].collectValues(bbs);
    }

    const std::vector<int> &inBlock(BasicBlock *n) const
    {
        return A_orig.at(n->index());
    }

    bool isNonLocal(const Temp &var) const
    {
        Q_ASSERT(!var.isInvalid());
        Q_ASSERT(var.index < nonLocals.size());
        return nonLocals[var.index];
    }

protected:
    virtual void visitPhi(Phi *) {}
    virtual void visitConvert(Convert *e) { e->expr->accept(this); }

    virtual void visitConst(Const *) {}
    virtual void visitString(IR::String *) {}
    virtual void visitRegExp(IR::RegExp *) {}
    virtual void visitName(Name *) {}
    virtual void visitArgLocal(ArgLocal *) {}
    virtual void visitClosure(Closure *) {}
    virtual void visitUnop(Unop *e) { e->expr->accept(this); }
    virtual void visitBinop(Binop *e) { e->left->accept(this); e->right->accept(this); }
    virtual void visitSubscript(Subscript *e) { e->base->accept(this); e->index->accept(this); }
    virtual void visitMember(Member *e) { e->base->accept(this); }
    virtual void visitExp(Exp *s) { s->expr->accept(this); }
    virtual void visitJump(Jump *) {}
    virtual void visitCJump(CJump *s) { s->cond->accept(this); }
    virtual void visitRet(Ret *s) { s->expr->accept(this); }

    virtual void visitCall(Call *e) {
        e->base->accept(this);
        for (ExprList *it = e->args; it; it = it->next)
            it->expr->accept(this);
    }

    virtual void visitNew(New *e) {
        e->base->accept(this);
        for (ExprList *it = e->args; it; it = it->next)
            it->expr->accept(this);
    }

    virtual void visitMove(Move *s) {
        s->source->accept(this);

        if (Temp *t = s->target->asTemp()) {
            addTemp(t);

            if (isCollectable(t)) {
#if defined(SHOW_SSA)
                qout << '\t';
                t->dump(qout);
                qout << " -> L" << currentBB->index << endl;
#endif // SHOW_SSA

                _defsites[t->index].insert(currentBB);
                addDefInCurrentBlock(t);

                // For semi-pruned SSA:
                killed[t->index] = true;
            }
        } else {
            s->target->accept(this);
        }
    }

    virtual void visitTemp(Temp *t)
    {
        addTemp(t);

        if (isCollectable(t))
            if (!killed[t->index])
                nonLocals[t->index] = true;
    }
};

struct UntypedTemp {
    Temp temp;
    UntypedTemp() {}
    UntypedTemp(const Temp &t): temp(t) {}
};
inline bool operator==(const UntypedTemp &t1, const UntypedTemp &t2) Q_DECL_NOTHROW
{ return t1.temp.index == t2.temp.index && t1.temp.kind == t2.temp.kind; }
inline bool operator!=(const UntypedTemp &t1, const UntypedTemp &t2) Q_DECL_NOTHROW
{ return !(t1 == t2); }

class DefUses
{
public:
    struct DefUse {
        DefUse()
            : defStmt(0)
            , blockOfStatement(0)
        { uses.reserve(8); }
        Temp temp;
        Stmt *defStmt;
        BasicBlock *blockOfStatement;
        QVector<Stmt *> uses;

        bool isValid() const
        { return temp.kind != Temp::Invalid; }

        void clear()
        { defStmt = 0; blockOfStatement = 0; uses.clear(); }
    };

private:
    std::vector<DefUse> _defUses;
    class Temps: public QVector<Temp> {
    public:
        Temps() { reserve(4); }
    };
    std::vector<Temps> _usesPerStatement;

    void ensure(Temp *newTemp)
    {
        if (_defUses.size() <= newTemp->index) {
            _defUses.reserve(newTemp->index + _defUses.size() / 3 + 1);
            _defUses.resize(newTemp->index + 1);
        }
    }

    void ensure(Stmt *s)
    {
        Q_ASSERT(s->id() >= 0);
        if (static_cast<unsigned>(s->id()) >= _usesPerStatement.size()) {
            _usesPerStatement.reserve(s->id() + _usesPerStatement.size() / 3 + 1);
            _usesPerStatement.resize(s->id() + 1);
        }
    }

    void addUseForStatement(Stmt *s, const Temp &var)
    {
        ensure(s);
        _usesPerStatement[s->id()].push_back(var);
    }

public:
    DefUses(IR::Function *function)
    {
        _usesPerStatement.resize(function->statementCount());
        _defUses.resize(function->tempCount);
    }

    void cleanup()
    {
        for (size_t i = 0, ei = _defUses.size(); i != ei; ++i) {
            DefUse &defUse = _defUses[i];
            if (defUse.isValid() && !defUse.defStmt)
                defUse.clear();
        }
    }

    unsigned statementCount() const
    { return _usesPerStatement.size(); }

    unsigned tempCount() const
    { return _defUses.size(); }

    const Temp &temp(int idx) const
    { return _defUses[idx].temp; }

    void addDef(Temp *newTemp, Stmt *defStmt, BasicBlock *defBlock)
    {
        ensure(newTemp);
        DefUse &defUse = _defUses[newTemp->index];
        Q_ASSERT(!defUse.isValid());
        defUse.temp = *newTemp;
        defUse.defStmt = defStmt;
        defUse.blockOfStatement = defBlock;
    }

    QList<UntypedTemp> defsUntyped() const
    {
        QList<UntypedTemp> res;
        foreach (const DefUse &du, _defUses)
            if (du.isValid())
                res.append(UntypedTemp(du.temp));
        return res;
    }

    std::vector<const Temp *> defs() const {
        std::vector<const Temp *> res;
        res.reserve(_defUses.size());
        for (unsigned i = 0, ei = _defUses.size(); i != ei; ++i) {
            const DefUse &du = _defUses.at(i);
            if (du.isValid())
                res.push_back(&du.temp);
        }
        return res;
    }

    void removeDef(const Temp &variable) {
        Q_ASSERT(static_cast<unsigned>(variable.index) < _defUses.size());
        _defUses[variable.index].clear();
    }

    void addUses(const Temp &variable, const QVector<Stmt *> &newUses)
    {
        Q_ASSERT(static_cast<unsigned>(variable.index) < _defUses.size());
        QVector<Stmt *> &uses = _defUses[variable.index].uses;
        foreach (Stmt *stmt, newUses)
            if (std::find(uses.begin(), uses.end(), stmt) == uses.end())
                uses.push_back(stmt);
    }

    void addUse(const Temp &variable, Stmt *newUse)
    {
        if (_defUses.size() <= variable.index) {
            _defUses.resize(variable.index + 1);
            DefUse &du = _defUses[variable.index];
            du.temp = variable;
            du.uses.push_back(newUse);
            addUseForStatement(newUse, variable);
            return;
        }

        QVector<Stmt *> &uses = _defUses[variable.index].uses;
        if (std::find(uses.begin(), uses.end(), newUse) == uses.end())
            uses.push_back(newUse);
        addUseForStatement(newUse, variable);
    }

    int useCount(const Temp &variable) const
    {
        Q_ASSERT(static_cast<unsigned>(variable.index) < _defUses.size());
        return _defUses[variable.index].uses.size();
    }

    Stmt *defStmt(const Temp &variable) const
    {
        Q_ASSERT(static_cast<unsigned>(variable.index) < _defUses.size());
        return _defUses[variable.index].defStmt;
    }

    BasicBlock *defStmtBlock(const Temp &variable) const
    {
        Q_ASSERT(static_cast<unsigned>(variable.index) < _defUses.size());
        return _defUses[variable.index].blockOfStatement;
    }

    void removeUse(Stmt *usingStmt, const Temp &var)
    {
        Q_ASSERT(static_cast<unsigned>(var.index) < _defUses.size());
        QVector<Stmt *> &uses = _defUses[var.index].uses;
        uses.erase(std::remove(uses.begin(), uses.end(), usingStmt), uses.end());
    }

    void registerNewStatement(Stmt *s)
    {
        ensure(s);
    }

    const QVector<Temp> &usedVars(Stmt *s) const
    {
        Q_ASSERT(s->id() >= 0);
        Q_ASSERT(static_cast<unsigned>(s->id()) < _usesPerStatement.size());
        return _usesPerStatement[s->id()];
    }

    const QVector<Stmt *> &uses(const Temp &var) const
    {
        return _defUses[var.index].uses;
    }

    QVector<Stmt*> removeDefUses(Stmt *s)
    {
        QVector<Stmt*> defStmts;
        foreach (const Temp &usedVar, usedVars(s)) {
            if (Stmt *ds = defStmt(usedVar))
                defStmts += ds;
            removeUse(s, usedVar);
        }
        if (Move *m = s->asMove()) {
            if (Temp *t = m->target->asTemp())
                removeDef(*t);
        } else if (Phi *p = s->asPhi()) {
            removeDef(*p->targetTemp);
        }

        return defStmts;
    }

    void dump() const
    {
        qout << "Defines and uses:" << endl;
        foreach (const DefUse &du, _defUses) {
            if (!du.isValid())
                continue;
            qout << '%' << du.temp.index;
            qout << " -> defined in block " << du.blockOfStatement->index()
                 << ", statement: " << du.defStmt->id()
                 << endl;
            qout << "     uses:";
            foreach (Stmt *s, du.uses)
                qout << ' ' << s->id();
            qout << endl;
        }
        qout << "Uses per statement:" << endl;
        for (unsigned i = 0, ei = _usesPerStatement.size(); i != ei; ++i) {
            qout << "    " << i << ":";
            foreach (const Temp &t, _usesPerStatement[i])
                qout << ' ' << t.index;
            qout << endl;
        }
    }
};

void insertPhiNode(const Temp &a, BasicBlock *y, IR::Function *f) {
#if defined(SHOW_SSA)
    qout << "-> inserted phi node for variable ";
    a.dump(qout);
    qout << " in block " << y->index << endl;
#endif

    Phi *phiNode = f->NewStmt<Phi>();
    phiNode->d = new Stmt::Data;
    phiNode->targetTemp = f->New<Temp>();
    phiNode->targetTemp->init(a.kind, a.index);
    y->prependStatement(phiNode);

    phiNode->d->incoming.resize(y->in.size());
    for (int i = 0, ei = y->in.size(); i < ei; ++i) {
        Temp *t = f->New<Temp>();
        t->init(a.kind, a.index);
        phiNode->d->incoming[i] = t;
    }
}

// High-level (recursive) algorithm:
//   Mapping: old temp number -> new temp number
//
//   Start:
//     Rename(start-node)
//
//   Rename(node, mapping):
//     for each statement S in block n
//       if S not in a phi-function
//         for each use of some variable x in S
//           y = mapping[x]
//           replace the use of x with y in S
//       for each definition of some variable a in S                        [1]
//         a_new = generate new/unique temp
//         mapping[a] = a_new
//         replace definition of a with definition of a_new in S
//     for each successor Y of block n
//       Suppose n is the j-th predecessor of Y
//       for each phi function in Y
//         suppose the j-th operand of the phi-function is a
//         i = mapping[a]
//         replace the j-th operand with a_i
//     for each child X of n                                                [2]
//       Rename(X)
//     for each newly generated temp from step [1] restore the old value    [3]
//
// This algorithm can run out of CPU stack space when there are lots of basic-blocks, like in a
// switch statement with 8000 cases that all fall-through. The iterativer version below uses a
// work-item stack, where step [1] from the algorithm above also pushes an "undo mapping change",
// and step [2] pushes a "rename(X)" action. This eliminates step [3].
//
// Iterative version:
//   Mapping: old temp number -> new temp number
//
//   The stack can hold two kinds of actions:
//     "Rename basic block n"
//     "Restore count for temp"
//
//   Start:
//     counter = 0
//     push "Rename start node" onto the stack
//     while the stack is not empty:
//       take the last item, and process it
//
//   Rename(n) =
//     for each statement S in block n
//       if S not in a phi-function
//         for each use of some variable x in S
//           y = mapping[x]
//           replace the use of x with y in S
//       for each definition of some variable a in S
//         old = mapping[a]
//         push Undo(a, old)
//         counter = counter + 1
//         new = counter;
//         mapping[a] = new
//         replace definition of a with definition of a_new in S
//     for each successor Y of block n
//       Suppose n is the j-th predecessor of Y
//       for each phi function in Y
//         suppose the j-th operand of the phi-function is a
//         i = mapping[a]
//         replace the j-th operand with a_i
//     for each child X of n
//       push Rename(X)
//
//   Undo(t, c) =
//     mapping[t] = c
class VariableRenamer: public StmtVisitor, public ExprVisitor
{
    Q_DISABLE_COPY(VariableRenamer)

    IR::Function *function;
    DefUses &defUses;
    unsigned tempCount;

    typedef std::vector<int> Mapping; // maps from existing/old temp number to the new and unique temp number.
    enum { Absent = -1 };
    Mapping vregMapping;
    ProcessedBlocks processed;

    BasicBlock *currentBB;
    Stmt *currentStmt;

    struct TodoAction {
        enum { RestoreVReg, Rename } action;
        union {
            struct {
                unsigned temp;
                int previous;
            } restoreData;
            struct {
                BasicBlock *basicBlock;
            } renameData;
        };

        bool isValid() const { return action != Rename || renameData.basicBlock != 0; }

        TodoAction()
        {
            action = Rename;
            renameData.basicBlock = 0;
        }

        TodoAction(const Temp &t, int prev)
        {
            Q_ASSERT(t.kind == Temp::VirtualRegister);

            action = RestoreVReg;
            restoreData.temp = t.index;
            restoreData.previous = prev;
        }

        TodoAction(BasicBlock *bb)
        {
            Q_ASSERT(bb);

            action = Rename;
            renameData.basicBlock = bb;
        }
    };

    QVector<TodoAction> todo;

public:
    VariableRenamer(IR::Function *f, DefUses &defUses)
        : function(f)
        , defUses(defUses)
        , tempCount(0)
        , processed(f)
    {
        vregMapping.assign(f->tempCount, Absent);
        todo.reserve(f->basicBlockCount());
    }

    void run() {
        todo.append(TodoAction(function->basicBlock(0)));

        while (!todo.isEmpty()) {
            TodoAction todoAction = todo.back();
            Q_ASSERT(todoAction.isValid());
            todo.pop_back();

            switch (todoAction.action) {
            case TodoAction::Rename:
                rename(todoAction.renameData.basicBlock);
                break;
            case TodoAction::RestoreVReg:
                restore(vregMapping, todoAction.restoreData.temp, todoAction.restoreData.previous);
                break;
            default:
                Q_UNREACHABLE();
            }
        }

        function->tempCount = tempCount;
    }

private:
    static inline void restore(Mapping &mapping, int temp, int previous)
    {
        mapping[temp] = previous;
    }

    void rename(BasicBlock *bb)
    {
        while (bb && !processed.alreadyProcessed(bb)) {
            renameStatementsAndPhis(bb);
            processed.markAsProcessed(bb);

            BasicBlock *next = 0;
            foreach (BasicBlock *out, bb->out) {
                if (processed.alreadyProcessed(out))
                    continue;
                if (!next)
                    next = out;
                else
                    todo.append(TodoAction(out));
            }
            bb = next;
        }
    }

    void renameStatementsAndPhis(BasicBlock *bb)
    {
        currentBB = bb;

        foreach (Stmt *s, bb->statements()) {
            currentStmt = s;
            s->accept(this);
        }

        foreach (BasicBlock *Y, bb->out) {
            const int j = Y->in.indexOf(bb);
            Q_ASSERT(j >= 0 && j < Y->in.size());
            foreach (Stmt *s, Y->statements()) {
                if (Phi *phi = s->asPhi()) {
                    Temp *t = phi->d->incoming[j]->asTemp();
                    unsigned newTmp = currentNumber(*t);
//                    qDebug()<<"I: replacing phi use"<<a<<"with"<<newTmp<<"in L"<<Y->index;
                    t->index = newTmp;
                    t->kind = Temp::VirtualRegister;
                    defUses.addUse(*t, phi);
                } else {
                    break;
                }
            }
        }
    }

    unsigned currentNumber(const Temp &t)
    {
        int nr = Absent;
        switch (t.kind) {
        case Temp::VirtualRegister:
            nr = vregMapping[t.index];
            break;
        default:
            Q_UNREACHABLE();
            nr = Absent;
            break;
        }
        if (nr == Absent) {
            // Special case: we didn't prune the Phi nodes yet, so for proper temps (virtual
            // registers) the SSA algorithm might insert superfluous Phis that have uses without
            // definition. E.g.: if a temporary got introduced in the "then" clause, it "could"
            // reach the "end-if" block, so there will be a phi node for that temp. A later pass
            // will clean this up by looking for uses-without-defines in phi nodes. So, what we do
            // is to generate a new unique number, and leave it dangling.
            nr = nextFreeTemp(t);
        }

        return nr;
    }

    unsigned nextFreeTemp(const Temp &t)
    {
        unsigned newIndex = tempCount++;
        Q_ASSERT(newIndex <= INT_MAX);
        int oldIndex = Absent;

        switch (t.kind) {
        case Temp::VirtualRegister:
            oldIndex = vregMapping[t.index];
            vregMapping[t.index] = newIndex;
            break;
        default:
            Q_UNREACHABLE();
        }

        todo.append(TodoAction(t, oldIndex));

        return newIndex;
    }

protected:
    virtual void visitTemp(Temp *e) { // only called for uses, not defs
//        qDebug()<<"I: replacing use of"<<e->index<<"with"<<stack[e->index].top();
        e->index = currentNumber(*e);
        e->kind = Temp::VirtualRegister;
        defUses.addUse(*e, currentStmt);
    }

    virtual void visitMove(Move *s) {
        // uses:
        s->source->accept(this);

        // defs:
        if (Temp *t = s->target->asTemp())
            renameTemp(t);
        else
            s->target->accept(this);
    }

    void renameTemp(Temp *t) { // only called for defs, not uses
        const int newIdx = nextFreeTemp(*t);
//        qDebug()<<"I: replacing def of"<<a<<"with"<<newIdx;
        t->kind = Temp::VirtualRegister;
        t->index = newIdx;
        defUses.addDef(t, currentStmt, currentBB);
    }

    virtual void visitConvert(Convert *e) { e->expr->accept(this); }
    virtual void visitPhi(Phi *s) { renameTemp(s->targetTemp); }

    virtual void visitExp(Exp *s) { s->expr->accept(this); }

    virtual void visitJump(Jump *) {}
    virtual void visitCJump(CJump *s) { s->cond->accept(this); }
    virtual void visitRet(Ret *s) { s->expr->accept(this); }

    virtual void visitConst(Const *) {}
    virtual void visitString(IR::String *) {}
    virtual void visitRegExp(IR::RegExp *) {}
    virtual void visitName(Name *) {}
    virtual void visitArgLocal(ArgLocal *) {}
    virtual void visitClosure(Closure *) {}
    virtual void visitUnop(Unop *e) { e->expr->accept(this); }
    virtual void visitBinop(Binop *e) { e->left->accept(this); e->right->accept(this); }
    virtual void visitCall(Call *e) {
        e->base->accept(this);
        for (ExprList *it = e->args; it; it = it->next)
            it->expr->accept(this);
    }

    virtual void visitNew(New *e) {
        e->base->accept(this);
        for (ExprList *it = e->args; it; it = it->next)
            it->expr->accept(this);
    }

    virtual void visitSubscript(Subscript *e) {
        e->base->accept(this);
        e->index->accept(this);
    }

    virtual void visitMember(Member *e) {
        e->base->accept(this);
    }
};

// This function converts the IR to semi-pruned SSA form. For details about SSA and the algorightm,
// see [Appel]. For the changes needed for semi-pruned SSA form, and for its advantages, see [Briggs].
void convertToSSA(IR::Function *function, const DominatorTree &df, DefUses &defUses)
{
#if defined(SHOW_SSA)
    qout << "Converting function ";
    if (function->name)
        qout << *function->name;
    else
        qout << "<no name>";
    qout << " to SSA..." << endl;
#endif // SHOW_SSA

    // Collect all applicable variables:
    VariableCollector variables(function);

    // Prepare for phi node insertion:
    std::vector<std::vector<bool> > A_phi;
    A_phi.resize(function->basicBlockCount());
    for (int i = 0, ei = A_phi.size(); i != ei; ++i)
        A_phi[i].assign(function->tempCount, false);

    std::vector<BasicBlock *> W;
    W.reserve(8);

    // Place phi functions:
    foreach (const Temp &a, variables.allTemps()) {
        if (a.isInvalid())
            continue;
        if (!variables.isNonLocal(a))
            continue; // for semi-pruned SSA

        W.clear();
        variables.collectDefSites(a, W);
        while (!W.empty()) {
            BasicBlock *n = W.back();
            W.pop_back();
            const BasicBlockSet &dominatorFrontierForN = df.dominatorFrontier(n);
            for (BasicBlockSet::const_iterator it = dominatorFrontierForN.begin(), eit = dominatorFrontierForN.end();
                 it != eit; ++it) {
                BasicBlock *y = *it;
                if (!A_phi.at(y->index()).at(a.index)) {
                    insertPhiNode(a, y, function);
                    A_phi[y->index()].at(a.index) = true;
                    const std::vector<int> &varsInBlockY = variables.inBlock(y);
                    if (std::find(varsInBlockY.begin(), varsInBlockY.end(), a.index) == varsInBlockY.end())
                        W.push_back(y);
                }
            }
        }
    }

    // Rename variables:
    VariableRenamer(function, defUses).run();
}

/// Calculate if a phi node result is used only by other phi nodes, and if those uses are
/// in turn also used by other phi nodes.
bool hasPhiOnlyUses(Phi *phi, const DefUses &defUses, QBitArray &collectedPhis)
{
    collectedPhis.setBit(phi->id());

    foreach (Stmt *use, defUses.uses(*phi->targetTemp)) {
        Phi *dependentPhi = use->asPhi();
        if (!dependentPhi)
            return false; // there is a use by a non-phi node

        if (collectedPhis.at(dependentPhi->id()))
            continue; // we already found this node

        if (!hasPhiOnlyUses(dependentPhi, defUses, collectedPhis))
            return false;
    }

    return true;
}

void cleanupPhis(DefUses &defUses)
{
    QBitArray toRemove(defUses.statementCount());
    QBitArray collectedPhis(defUses.statementCount());
    std::vector<Phi *> allPhis;
    allPhis.reserve(32);

    foreach (const Temp *def, defUses.defs()) {
        Stmt *defStmt = defUses.defStmt(*def);
        if (!defStmt)
            continue;

        Phi *phi = defStmt->asPhi();
        if (!phi)
            continue;
        allPhis.push_back(phi);
        if (toRemove.at(phi->id()))
            continue;

        collectedPhis.fill(false);
        if (hasPhiOnlyUses(phi, defUses, collectedPhis))
            toRemove |= collectedPhis;
    }

    foreach (Phi *phi, allPhis) {
        if (!toRemove.at(phi->id()))
            continue;

        const Temp &targetVar = *phi->targetTemp;
        defUses.defStmtBlock(targetVar)->removeStatement(phi);

        foreach (const Temp &usedVar, defUses.usedVars(phi))
            defUses.removeUse(phi, usedVar);
        defUses.removeDef(targetVar);
    }

    defUses.cleanup();
}

class StatementWorklist
{
    IR::Function *theFunction;
    std::vector<Stmt *> stmts;
    std::vector<bool> worklist;
    unsigned worklistSize;
    std::vector<int> replaced;
    std::vector<bool> removed;

    Q_DISABLE_COPY(StatementWorklist)

public:
    StatementWorklist(IR::Function *function)
        : theFunction(function)
        , stmts(function->statementCount(), static_cast<Stmt *>(0))
        , worklist(function->statementCount(), false)
        , worklistSize(0)
        , replaced(function->statementCount(), Stmt::InvalidId)
        , removed(function->statementCount())
    {
        grow();

        foreach (BasicBlock *bb, function->basicBlocks()) {
            if (bb->isRemoved())
                continue;

            foreach (Stmt *s, bb->statements()) {
                if (!s)
                    continue;

                stmts[s->id()] = s;
                worklist[s->id()] = true;
                ++worklistSize;
            }
        }
    }

    void reset()
    {
        worklist.assign(worklist.size(), false);
        worklistSize = 0;

        foreach (Stmt *s, stmts) {
            if (!s)
                continue;

            worklist[s->id()] = true;
            ++worklistSize;
        }

        replaced.assign(replaced.size(), Stmt::InvalidId);
        removed.assign(removed.size(), false);
    }

    void remove(Stmt *stmt)
    {
        replaced[stmt->id()] = Stmt::InvalidId;
        removed[stmt->id()] = true;
        std::vector<bool>::reference inWorklist = worklist[stmt->id()];
        if (inWorklist) {
            inWorklist = false;
            Q_ASSERT(worklistSize > 0);
            --worklistSize;
        }
    }

    void replace(Stmt *oldStmt, Stmt *newStmt)
    {
        Q_ASSERT(oldStmt);
        Q_ASSERT(replaced[oldStmt->id()] == Stmt::InvalidId);
        Q_ASSERT(removed[oldStmt->id()] == false);

        Q_ASSERT(newStmt);
        registerNewStatement(newStmt);
        Q_ASSERT(replaced[newStmt->id()] == Stmt::InvalidId);
        Q_ASSERT(removed[newStmt->id()] == false);

        replaced[oldStmt->id()] = newStmt->id();
        worklist[oldStmt->id()] = false;
    }

    void applyToFunction()
    {
        foreach (BasicBlock *bb, theFunction->basicBlocks()) {
            if (bb->isRemoved())
                continue;

            for (int i = 0; i < bb->statementCount();) {
                Stmt *stmt = bb->statements().at(i);

                int id = stmt->id();
                Q_ASSERT(id != Stmt::InvalidId);
                Q_ASSERT(static_cast<unsigned>(stmt->id()) < stmts.size());

                for (int replacementId = replaced[id]; replacementId != Stmt::InvalidId; replacementId = replaced[replacementId])
                    id = replacementId;
                Q_ASSERT(id != Stmt::InvalidId);
                Q_ASSERT(static_cast<unsigned>(stmt->id()) < stmts.size());

                if (removed[id]) {
                    bb->removeStatement(i);
                } else {
                    if (id != stmt->id())
                        bb->replaceStatement(i, stmts[id]);

                    ++i;
                }
            }
        }

        replaced.assign(replaced.size(), Stmt::InvalidId);
        removed.assign(removed.size(), false);
    }

    StatementWorklist &operator+=(const QVector<Stmt *> &stmts)
    {
        foreach (Stmt *s, stmts)
            this->operator+=(s);

        return *this;
    }

    StatementWorklist &operator+=(Stmt *s)
    {
        if (!s)
            return *this;

        Q_ASSERT(s->id() >= 0);
        Q_ASSERT(static_cast<unsigned>(s->id()) < worklist.size());

        if (!worklist[s->id()]) {
            worklist[s->id()] = true;
            ++worklistSize;
        }

        return *this;
    }

    StatementWorklist &operator-=(Stmt *s)
    {
        Q_ASSERT(s->id() >= 0);
        Q_ASSERT(static_cast<unsigned>(s->id()) < worklist.size());

        std::vector<bool>::reference inWorklist = worklist[s->id()];
        if (inWorklist) {
            inWorklist = false;
            Q_ASSERT(worklistSize > 0);
            --worklistSize;
        }

        return *this;
    }

    bool isEmpty() const
    {
        return worklistSize == 0;
    }

    Stmt *takeNext(Stmt *last)
    {
        if (isEmpty())
            return 0;

        const int startAt = last ? last->id() + 1 : 0;
        Q_ASSERT(startAt >= 0);
        Q_ASSERT(static_cast<unsigned>(startAt) <= worklist.size());

        Q_ASSERT(worklist.size() == stmts.size());

        // Do not compare the result of find with the end iterator, because some libc++ versions
        // have a bug where the result of the ++operator is past-the-end of the vector, but unequal
        // to end().
        size_t pos = std::find(worklist.begin() + startAt, worklist.end(), true) - worklist.begin();
        if (pos >= worklist.size())
            pos = std::find(worklist.begin(), worklist.begin() + startAt, true) - worklist.begin();

        worklist[pos] = false;
        Q_ASSERT(worklistSize > 0);
        --worklistSize;
        Stmt *s = stmts.at(pos);
        Q_ASSERT(s);
        return s;
    }

    IR::Function *function() const
    {
        return theFunction;
    }

    void registerNewStatement(Stmt *s)
    {
        Q_ASSERT(s->id() >= 0);
        if (static_cast<unsigned>(s->id()) >= stmts.size()) {
            if (static_cast<unsigned>(s->id()) >= stmts.capacity())
                grow();

            int newSize = s->id() + 1;
            stmts.resize(newSize, 0);
            worklist.resize(newSize, false);
            replaced.resize(newSize, Stmt::InvalidId);
            removed.resize(newSize, false);
        }

        stmts[s->id()] = s;
    }

private:
    void grow()
    {
        size_t newCapacity = ((stmts.capacity() + 1) * 3) / 2;
        stmts.reserve(newCapacity);
        worklist.reserve(newCapacity);
        replaced.reserve(newCapacity);
        removed.reserve(newCapacity);
    }
};

class EliminateDeadCode: public ExprVisitor {
    DefUses &_defUses;
    StatementWorklist &_worklist;
    bool _sideEffect;
    QVector<Temp *> _collectedTemps;

public:
    EliminateDeadCode(DefUses &defUses, StatementWorklist &worklist)
        : _defUses(defUses)
        , _worklist(worklist)
    {
        _collectedTemps.reserve(8);
    }

    void run(Expr *&expr, Stmt *stmt) {
        if (!checkForSideEffects(expr)) {
            expr = 0;
            foreach (Temp *t, _collectedTemps) {
                _defUses.removeUse(stmt, *t);
                _worklist += _defUses.defStmt(*t);
            }
        }
    }

private:
    bool checkForSideEffects(Expr *expr)
    {
        bool sideEffect = false;
        qSwap(_sideEffect, sideEffect);
        expr->accept(this);
        qSwap(_sideEffect, sideEffect);
        return sideEffect;
    }

    void markAsSideEffect()
    {
        _sideEffect = true;
        _collectedTemps.clear();
    }

protected:
    virtual void visitConst(Const *) {}
    virtual void visitString(IR::String *) {}
    virtual void visitRegExp(IR::RegExp *) {}

    virtual void visitName(Name *e)
    {
        if (e->freeOfSideEffects)
            return;
        // TODO: maybe we can distinguish between built-ins of which we know that they do not have
        // a side-effect.
        if (e->builtin == Name::builtin_invalid || (e->id && *e->id != QStringLiteral("this")))
            markAsSideEffect();
    }

    virtual void visitTemp(Temp *e)
    {
        _collectedTemps.append(e);
    }

    virtual void visitArgLocal(ArgLocal *) {}

    virtual void visitClosure(Closure *)
    {
        markAsSideEffect();
    }

    virtual void visitConvert(Convert *e) {
        e->expr->accept(this);

        switch (e->expr->type) {
        case QObjectType:
        case StringType:
        case VarType:
            markAsSideEffect();
            break;
        default:
            break;
        }
    }

    virtual void visitUnop(Unop *e) {
        e->expr->accept(this);

        switch (e->op) {
        case OpUPlus:
        case OpUMinus:
        case OpNot:
        case OpIncrement:
        case OpDecrement:
            if (e->expr->type == VarType || e->expr->type == StringType || e->expr->type == QObjectType)
                markAsSideEffect();
            break;

        default:
            break;
        }
    }

    virtual void visitBinop(Binop *e) {
        // TODO: prune parts that don't have a side-effect. For example, in:
        //   function f(x) { +x+1; return 0; }
        // we can prune the binop and leave the unop/conversion.
        _sideEffect = checkForSideEffects(e->left);
        _sideEffect |= checkForSideEffects(e->right);

        if (e->left->type == VarType || e->left->type == StringType || e->left->type == QObjectType
                || e->right->type == VarType || e->right->type == StringType || e->right->type == QObjectType)
            markAsSideEffect();
    }

    virtual void visitSubscript(Subscript *e) {
        e->base->accept(this);
        e->index->accept(this);
        markAsSideEffect();
    }

    virtual void visitMember(Member *e) {
        e->base->accept(this);
        if (e->freeOfSideEffects)
            return;
        markAsSideEffect();
    }

    virtual void visitCall(Call *e) {
        e->base->accept(this);
        for (ExprList *args = e->args; args; args = args->next)
            args->expr->accept(this);
        markAsSideEffect(); // TODO: there are built-in functions that have no side effect.
    }

    virtual void visitNew(New *e) {
        e->base->accept(this);
        for (ExprList *args = e->args; args; args = args->next)
            args->expr->accept(this);
        markAsSideEffect(); // TODO: there are built-in types that have no side effect.
    }
};

struct DiscoveredType {
    int type;
    MemberExpressionResolver memberResolver;

    DiscoveredType() : type(UnknownType) {}
    DiscoveredType(Type t) : type(t) { Q_ASSERT(type != QObjectType); }
    explicit DiscoveredType(int t) : type(t) { Q_ASSERT(type != QObjectType); }
    explicit DiscoveredType(MemberExpressionResolver memberResolver) : type(QObjectType), memberResolver(memberResolver) {}

    bool test(Type t) const { return type & t; }
    bool isNumber() const { return (type & NumberType) && !(type & ~NumberType); }

    bool operator!=(Type other) const { return type != other; }
    bool operator==(Type other) const { return type == other; }
    bool operator==(const DiscoveredType &other) const { return type == other.type; }
    bool operator!=(const DiscoveredType &other) const { return type != other.type; }
};

class PropagateTempTypes: public StmtVisitor, ExprVisitor
{
    const DefUses &defUses;
    UntypedTemp theTemp;
    DiscoveredType newType;

public:
    PropagateTempTypes(const DefUses &defUses)
        : defUses(defUses)
    {}

    void run(const UntypedTemp &temp, const DiscoveredType &type)
    {
        newType = type;
        theTemp = temp;
        if (Stmt *defStmt = defUses.defStmt(temp.temp))
            defStmt->accept(this);
        foreach (Stmt *use, defUses.uses(temp.temp))
            use->accept(this);
    }

protected:
    virtual void visitConst(Const *) {}
    virtual void visitString(IR::String *) {}
    virtual void visitRegExp(IR::RegExp *) {}
    virtual void visitName(Name *) {}
    virtual void visitTemp(Temp *e) {
        if (theTemp == UntypedTemp(*e)) {
            e->type = static_cast<Type>(newType.type);
            e->memberResolver = newType.memberResolver;
        }
    }
    virtual void visitArgLocal(ArgLocal *) {}
    virtual void visitClosure(Closure *) {}
    virtual void visitConvert(Convert *e) { e->expr->accept(this); }
    virtual void visitUnop(Unop *e) { e->expr->accept(this); }
    virtual void visitBinop(Binop *e) { e->left->accept(this); e->right->accept(this); }

    virtual void visitCall(Call *e) {
        e->base->accept(this);
        for (ExprList *it = e->args; it; it = it->next)
            it->expr->accept(this);
    }
    virtual void visitNew(New *e) {
        e->base->accept(this);
        for (ExprList *it = e->args; it; it = it->next)
            it->expr->accept(this);
    }
    virtual void visitSubscript(Subscript *e) {
        e->base->accept(this);
        e->index->accept(this);
    }

    virtual void visitMember(Member *e) {
        e->base->accept(this);
    }

    virtual void visitExp(Exp *s) {s->expr->accept(this);}
    virtual void visitMove(Move *s) {
        s->source->accept(this);
        s->target->accept(this);
    }

    virtual void visitJump(Jump *) {}
    virtual void visitCJump(CJump *s) { s->cond->accept(this); }
    virtual void visitRet(Ret *s) { s->expr->accept(this); }
    virtual void visitPhi(Phi *s) {
        s->targetTemp->accept(this);
        foreach (Expr *e, s->d->incoming)
            e->accept(this);
    }
};

class TypeInference: public StmtVisitor, public ExprVisitor
{
    enum { DebugTypeInference = 0 };

    QQmlEnginePrivate *qmlEngine;
    const DefUses &_defUses;
    typedef std::vector<DiscoveredType> TempTypes;
    TempTypes _tempTypes;
    StatementWorklist *_worklist;
    struct TypingResult {
        DiscoveredType type;
        bool fullyTyped;

        TypingResult(const DiscoveredType &type = DiscoveredType()) {
#if defined(__GNUC__) && __GNUC__ == 4 && __GNUC_MINOR__ == 6
            // avoid optimization bug in gcc 4.6.3 armhf
            ((int volatile &) this->type.type) = type.type;
#endif
            this->type = type;
            fullyTyped = type.type != UnknownType;
        }
        explicit TypingResult(MemberExpressionResolver memberResolver)
            : type(memberResolver)
            , fullyTyped(true)
        {}
    };
    TypingResult _ty;

public:
    TypeInference(QQmlEnginePrivate *qmlEngine, const DefUses &defUses)
        : qmlEngine(qmlEngine)
        , _defUses(defUses)
        , _tempTypes(_defUses.tempCount())
        , _worklist(0)
        , _ty(UnknownType)
    {}

    void run(StatementWorklist &w) {
        _worklist = &w;

        Stmt *s = 0;
        while ((s = _worklist->takeNext(s))) {
            if (s->asJump())
                continue;

            if (DebugTypeInference) {
                qout<<"Typing stmt ";
                IRPrinter(&qout).print(s);
                qout<<endl;
            }

            if (!run(s)) {
                *_worklist += s;
                if (DebugTypeInference) {
                    qout<<"Pushing back stmt: ";
                    IRPrinter(&qout).print(s);
                    qout<<endl;
                }
            } else {
                if (DebugTypeInference) {
                    qout<<"Finished: ";
                    IRPrinter(&qout).print(s);
                    qout<<endl;
                }
            }
        }

        PropagateTempTypes propagator(_defUses);
        for (unsigned i = 0, ei = _tempTypes.size(); i != ei; ++i) {
            const Temp &temp = _defUses.temp(i);
            if (temp.kind == Temp::Invalid)
                continue;
            const DiscoveredType &tempType = _tempTypes[i];
            if (tempType.type == UnknownType)
                continue;
            propagator.run(temp, tempType);
        }

        _worklist = 0;
    }

private:
    bool run(Stmt *s) {
        TypingResult ty;
        std::swap(_ty, ty);
        s->accept(this);
        std::swap(_ty, ty);
        return ty.fullyTyped;
    }

    TypingResult run(Expr *e) {
        TypingResult ty;
        std::swap(_ty, ty);
        e->accept(this);
        std::swap(_ty, ty);

        if (ty.type != UnknownType)
            setType(e, ty.type);
        return ty;
    }

    void setType(Expr *e, DiscoveredType ty) {
        if (Temp *t = e->asTemp()) {
            if (DebugTypeInference)
                qout << "Setting type for temp " << t->index
                     << " to " << typeName(Type(ty.type)) << " (" << ty.type << ")"
                     << endl;

            DiscoveredType &it = _tempTypes[t->index];
            if (it != ty) {
                it = ty;

                if (DebugTypeInference) {
                    foreach (Stmt *s, _defUses.uses(*t)) {
                        qout << "Pushing back dependent stmt: ";
                        IRPrinter(&qout).print(s);
                        qout<<endl;
                    }
                }

                *_worklist += _defUses.uses(*t);
            }
        } else {
            e->type = (Type) ty.type;
        }
    }

protected:
    virtual void visitConst(Const *e) {
        if (e->type & NumberType) {
            if (canConvertToSignedInteger(e->value))
                _ty = TypingResult(SInt32Type);
            else if (canConvertToUnsignedInteger(e->value))
                _ty = TypingResult(UInt32Type);
            else
                _ty = TypingResult(e->type);
        } else
            _ty = TypingResult(e->type);
    }
    virtual void visitString(IR::String *) { _ty = TypingResult(StringType); }
    virtual void visitRegExp(IR::RegExp *) { _ty = TypingResult(VarType); }
    virtual void visitName(Name *) { _ty = TypingResult(VarType); }
    virtual void visitTemp(Temp *e) {
        if (e->memberResolver.isValid())
            _ty = TypingResult(e->memberResolver);
        else
            _ty = TypingResult(_tempTypes[e->index]);
        setType(e, _ty.type);
    }
    virtual void visitArgLocal(ArgLocal *e) {
        _ty = TypingResult(VarType);
        setType(e, _ty.type);
    }

    virtual void visitClosure(Closure *) { _ty = TypingResult(VarType); }
    virtual void visitConvert(Convert *e) {
        _ty = TypingResult(e->type);
    }

    virtual void visitUnop(Unop *e) {
        _ty = run(e->expr);
        switch (e->op) {
        case OpUPlus: _ty.type = DoubleType; return;
        case OpUMinus: _ty.type = DoubleType; return;
        case OpCompl: _ty.type = SInt32Type; return;
        case OpNot: _ty.type = BoolType; return;

        case OpIncrement:
        case OpDecrement:
            Q_ASSERT(!"Inplace operators should have been removed!");
        default:
            Q_UNIMPLEMENTED();
            Q_UNREACHABLE();
        }
    }

    virtual void visitBinop(Binop *e) {
        TypingResult leftTy = run(e->left);
        TypingResult rightTy = run(e->right);
        _ty.fullyTyped = leftTy.fullyTyped && rightTy.fullyTyped;

        switch (e->op) {
        case OpAdd:
            if (leftTy.type.test(VarType) || leftTy.type.test(QObjectType) || rightTy.type.test(VarType) || rightTy.type.test(QObjectType))
                _ty.type = VarType;
            else if (leftTy.type.test(StringType) || rightTy.type.test(StringType))
                _ty.type = StringType;
            else if (leftTy.type != UnknownType && rightTy.type != UnknownType)
                _ty.type = DoubleType;
            else
                _ty.type = UnknownType;
            break;
        case OpSub:
            _ty.type = DoubleType;
            break;

        case OpMul:
        case OpDiv:
        case OpMod:
            _ty.type = DoubleType;
            break;

        case OpBitAnd:
        case OpBitOr:
        case OpBitXor:
        case OpLShift:
        case OpRShift:
            _ty.type = SInt32Type;
            break;
        case OpURShift:
            _ty.type = UInt32Type;
            break;

        case OpGt:
        case OpLt:
        case OpGe:
        case OpLe:
        case OpEqual:
        case OpNotEqual:
        case OpStrictEqual:
        case OpStrictNotEqual:
        case OpAnd:
        case OpOr:
        case OpInstanceof:
        case OpIn:
            _ty.type = BoolType;
            break;

        default:
            Q_UNIMPLEMENTED();
            Q_UNREACHABLE();
        }
    }

    virtual void visitCall(Call *e) {
        _ty = run(e->base);
        for (ExprList *it = e->args; it; it = it->next)
            _ty.fullyTyped &= run(it->expr).fullyTyped;
        _ty.type = VarType;
    }
    virtual void visitNew(New *e) {
        _ty = run(e->base);
        for (ExprList *it = e->args; it; it = it->next)
            _ty.fullyTyped &= run(it->expr).fullyTyped;
        _ty.type = VarType;
    }
    virtual void visitSubscript(Subscript *e) {
        _ty.fullyTyped = run(e->base).fullyTyped && run(e->index).fullyTyped;
        _ty.type = VarType;
    }

    virtual void visitMember(Member *e) {
        _ty = run(e->base);

        if (_ty.fullyTyped && _ty.type.memberResolver.isValid()) {
            MemberExpressionResolver &resolver = _ty.type.memberResolver;
            _ty.type.type = resolver.resolveMember(qmlEngine, &resolver, e);
        } else
            _ty.type = VarType;
    }

    virtual void visitExp(Exp *s) { _ty = run(s->expr); }
    virtual void visitMove(Move *s) {
        TypingResult sourceTy = run(s->source);
        if (Temp *t = s->target->asTemp()) {
            setType(t, sourceTy.type);
            _ty = sourceTy;
            return;
        }

        _ty = run(s->target);
        _ty.fullyTyped &= sourceTy.fullyTyped;
    }

    virtual void visitJump(Jump *) { _ty = TypingResult(MissingType); }
    virtual void visitCJump(CJump *s) { _ty = run(s->cond); }
    virtual void visitRet(Ret *s) { _ty = run(s->expr); }
    virtual void visitPhi(Phi *s) {
        _ty = run(s->d->incoming[0]);
        for (int i = 1, ei = s->d->incoming.size(); i != ei; ++i) {
            TypingResult ty = run(s->d->incoming[i]);
            if (!ty.fullyTyped && _ty.fullyTyped) {
                // When one of the temps not fully typed, we already know that we cannot completely type this node.
                // So, pick the type we calculated upto this point, and wait until the unknown one will be typed.
                // At that point, this statement will be re-scheduled, and then we can fully type this node.
                _ty.fullyTyped = false;
                break;
            }
            _ty.type.type |= ty.type.type;
            _ty.fullyTyped &= ty.fullyTyped;
            if (_ty.type.test(QObjectType))
                _ty.type.memberResolver.clear(); // ### TODO: find common ancestor meta-object
        }

        switch (_ty.type.type) {
        case UnknownType:
        case UndefinedType:
        case NullType:
        case BoolType:
        case SInt32Type:
        case UInt32Type:
        case DoubleType:
        case StringType:
        case QObjectType:
        case VarType:
            // The type is not a combination of two or more types, so we're done.
            break;

        default:
            // There are multiple types involved, so:
            if (_ty.type.isNumber())
                // The type is any combination of double/int32/uint32, but nothing else. So we can
                // type it as double.
                _ty.type = DoubleType;
            else
                // There just is no single type that can hold this combination, so:
                _ty.type = VarType;
        }

        setType(s->targetTemp, _ty.type);
    }
};

class ReverseInference
{
    const DefUses &_defUses;

public:
    ReverseInference(const DefUses &defUses)
        : _defUses(defUses)
    {}

    void run(IR::Function *f)
    {
        Q_UNUSED(f);

        QVector<UntypedTemp> knownOk;
        QList<UntypedTemp> candidates = _defUses.defsUntyped();
        while (!candidates.isEmpty()) {
            UntypedTemp temp = candidates.last();
            candidates.removeLast();

            if (knownOk.contains(temp))
                continue;

            if (!isUsedAsInt32(temp, knownOk))
                continue;

            Stmt *s = _defUses.defStmt(temp.temp);
            Move *m = s->asMove();
            if (!m)
                continue;
            Temp *target = m->target->asTemp();
            if (!target || temp != UntypedTemp(*target) || target->type == SInt32Type)
                continue;
            if (Temp *t = m->source->asTemp()) {
                candidates.append(*t);
            } else if (m->source->asConvert()) {
                break;
            } else if (Binop *b = m->source->asBinop()) {
                bool iterateOnOperands = true;

                switch (b->op) {
                case OpSub:
                case OpMul:
                case OpAdd:
                    if (b->left->type == SInt32Type && b->right->type == SInt32Type) {
                        iterateOnOperands = false;
                        break;
                    } else {
                        continue;
                    }
                case OpBitAnd:
                case OpBitOr:
                case OpBitXor:
                case OpLShift:
                case OpRShift:
                case OpURShift:
                    break;
                default:
                    continue;
                }

                if (iterateOnOperands) {
                    if (Temp *lt = b->left->asTemp())
                        candidates.append(*lt);
                    if (Temp *rt = b->right->asTemp())
                        candidates.append(*rt);
                }
            } else if (Unop *u = m->source->asUnop()) {
                if (u->op == OpCompl || u->op == OpUPlus) {
                    if (Temp *t = u->expr->asTemp())
                        candidates.append(*t);
                }
            } else {
                continue;
            }

            knownOk.append(temp);
        }

        PropagateTempTypes propagator(_defUses);
        foreach (const UntypedTemp &t, knownOk) {
            propagator.run(t, SInt32Type);
            if (Stmt *defStmt = _defUses.defStmt(t.temp)) {
                if (Move *m = defStmt->asMove()) {
                    if (Convert *c = m->source->asConvert()) {
                        c->type = SInt32Type;
                    } else if (Unop *u = m->source->asUnop()) {
                        if (u->op != OpUMinus)
                            u->type = SInt32Type;
                    } else if (Binop *b = m->source->asBinop()) {
                        b->type = SInt32Type;
                    }
                }
            }
        }
    }

private:
    bool isUsedAsInt32(const UntypedTemp &t, const QVector<UntypedTemp> &knownOk) const
    {
        const QVector<Stmt *> &uses = _defUses.uses(t.temp);
        if (uses.isEmpty())
            return false;

        foreach (Stmt *use, uses) {
            if (Move *m = use->asMove()) {
                Temp *targetTemp = m->target->asTemp();

                if (m->source->asTemp()) {
                    if (!targetTemp || !knownOk.contains(*targetTemp))
                        return false;
                } else if (m->source->asConvert()) {
                    continue;
                } else if (Binop *b = m->source->asBinop()) {
                    switch (b->op) {
                    case OpAdd:
                    case OpSub:
                    case OpMul:
                        if (!targetTemp || !knownOk.contains(*targetTemp))
                            return false;
                    case OpBitAnd:
                    case OpBitOr:
                    case OpBitXor:
                    case OpRShift:
                    case OpLShift:
                    case OpURShift:
                        continue;
                    default:
                        return false;
                    }
                } else if (Unop *u = m->source->asUnop()) {
                    if (u->op == OpUPlus) {
                        if (!targetTemp || !knownOk.contains(*targetTemp))
                            return false;
                    } else if (u->op != OpCompl) {
                        return false;
                    }
                } else {
                    return false;
                }
            } else
                return false;
        }

        return true;
    }
};

void convertConst(Const *c, Type targetType)
{
    switch (targetType) {
    case DoubleType:
        break;
    case SInt32Type:
        c->value = QV4::Primitive::toInt32(c->value);
        break;
    case UInt32Type:
        c->value = QV4::Primitive::toUInt32(c->value);
        break;
    case BoolType:
        c->value = !(c->value == 0 || std::isnan(c->value));
        break;
    case NullType:
    case UndefinedType:
        c->value = qSNaN();
        c->type = targetType;
    default:
        Q_UNIMPLEMENTED();
        Q_ASSERT(!"Unimplemented!");
        break;
    }
    c->type = targetType;
}

class TypePropagation: public StmtVisitor, public ExprVisitor {
    DefUses &_defUses;
    Type _ty;
    IR::Function *_f;

    bool run(Expr *&e, Type requestedType = UnknownType, bool insertConversion = true) {
        qSwap(_ty, requestedType);
        e->accept(this);
        qSwap(_ty, requestedType);

        if (requestedType != UnknownType) {
            if (e->type != requestedType) {
                if (requestedType & NumberType || requestedType == BoolType) {
#ifdef SHOW_SSA
                    QTextStream os(stdout, QIODevice::WriteOnly);
                    os << "adding conversion from " << typeName(e->type)
                       << " to " << typeName(requestedType) << " for expression ";
                    e->dump(os);
                    os << " in statement ";
                    _currStmt->dump(os);
                    os << endl;
#endif
                    if (insertConversion)
                        addConversion(e, requestedType);
                    return true;
                }
            }
        }

        return false;
    }

    struct Conversion {
        Expr **expr;
        Type targetType;
        Stmt *stmt;

        Conversion(Expr **expr = 0, Type targetType = UnknownType, Stmt *stmt = 0)
            : expr(expr)
            , targetType(targetType)
            , stmt(stmt)
        {}
    };

    Stmt *_currStmt;
    QVector<Conversion> _conversions;

    void addConversion(Expr *&expr, Type targetType) {
        _conversions.append(Conversion(&expr, targetType, _currStmt));
    }

public:
    TypePropagation(DefUses &defUses) : _defUses(defUses), _ty(UnknownType) {}

    void run(IR::Function *f, StatementWorklist &worklist) {
        _f = f;
        foreach (BasicBlock *bb, f->basicBlocks()) {
            if (bb->isRemoved())
                continue;
            _conversions.clear();

            foreach (Stmt *s, bb->statements()) {
                _currStmt = s;
                s->accept(this);
            }

            foreach (const Conversion &conversion, _conversions) {
                IR::Move *move = conversion.stmt->asMove();

                // Note: isel only supports move into member when source is a temp, so convert
                // is not a supported source.
                if (move && move->source->asTemp() && !move->target->asMember()) {
                    *conversion.expr = bb->CONVERT(*conversion.expr, conversion.targetType);
                } else if (Const *c = (*conversion.expr)->asConst()) {
                    convertConst(c, conversion.targetType);
                } else if (ArgLocal *al = (*conversion.expr)->asArgLocal()) {
                    Temp *target = bb->TEMP(bb->newTemp());
                    target->type = conversion.targetType;
                    Expr *convert = bb->CONVERT(al, conversion.targetType);
                    Move *convCall = f->NewStmt<Move>();
                    worklist.registerNewStatement(convCall);
                    convCall->init(target, convert);
                    _defUses.addDef(target, convCall, bb);

                    Temp *source = bb->TEMP(target->index);
                    source->type = conversion.targetType;
                    _defUses.addUse(*source, conversion.stmt);

                    if (conversion.stmt->asPhi()) {
                        // Only temps can be used as arguments to phi nodes, so this is a sanity check...:
                        Q_UNREACHABLE();
                    } else {
                        bb->insertStatementBefore(conversion.stmt, convCall);
                    }

                    *conversion.expr = source;
                } else if (Temp *t = (*conversion.expr)->asTemp()) {
                    Temp *target = bb->TEMP(bb->newTemp());
                    target->type = conversion.targetType;
                    Expr *convert = bb->CONVERT(t, conversion.targetType);
                    Move *convCall = f->NewStmt<Move>();
                    worklist.registerNewStatement(convCall);
                    convCall->init(target, convert);
                    _defUses.addDef(target, convCall, bb);
                    _defUses.addUse(*t, convCall);

                    Temp *source = bb->TEMP(target->index);
                    source->type = conversion.targetType;
                    _defUses.removeUse(conversion.stmt, *t);
                    _defUses.addUse(*source, conversion.stmt);

                    if (Phi *phi = conversion.stmt->asPhi()) {
                        int idx = phi->d->incoming.indexOf(t);
                        Q_ASSERT(idx != -1);
                        bb->in[idx]->insertStatementBeforeTerminator(convCall);
                    } else {
                        bb->insertStatementBefore(conversion.stmt, convCall);
                    }

                    *conversion.expr = source;
                } else if (Unop *u = (*conversion.expr)->asUnop()) {
                    // convert:
                    //   int32{%2} = double{-double{%1}};
                    // to:
                    //   double{%3} = double{-double{%1}};
                    //   int32{%2} = int32{convert(double{%3})};
                    Temp *tmp = bb->TEMP(bb->newTemp());
                    tmp->type = u->type;
                    Move *extraMove = f->NewStmt<Move>();
                    worklist.registerNewStatement(extraMove);
                    extraMove->init(tmp, u);
                    _defUses.addDef(tmp, extraMove, bb);

                    if (Temp *unopOperand = u->expr->asTemp()) {
                        _defUses.addUse(*unopOperand, extraMove);
                        _defUses.removeUse(move, *unopOperand);
                    }

                    bb->insertStatementBefore(conversion.stmt, extraMove);

                    *conversion.expr = bb->CONVERT(CloneExpr::cloneTemp(tmp, f), conversion.targetType);
                    _defUses.addUse(*tmp, move);
                } else {
                    Q_UNREACHABLE();
                }
            }
        }
    }

protected:
    virtual void visitConst(Const *c) {
        if (_ty & NumberType && c->type & NumberType) {
            if (_ty == SInt32Type)
                c->value = QV4::Primitive::toInt32(c->value);
            else if (_ty == UInt32Type)
                c->value = QV4::Primitive::toUInt32(c->value);
            c->type = _ty;
        }
    }

    virtual void visitString(IR::String *) {}
    virtual void visitRegExp(IR::RegExp *) {}
    virtual void visitName(Name *) {}
    virtual void visitTemp(Temp *) {}
    virtual void visitArgLocal(ArgLocal *) {}
    virtual void visitClosure(Closure *) {}
    virtual void visitConvert(Convert *e) { run(e->expr, e->type); }
    virtual void visitUnop(Unop *e) { run(e->expr, e->type); }
    virtual void visitBinop(Binop *e) {
        // FIXME: This routine needs more tuning!
        switch (e->op) {
        case OpAdd:
        case OpSub:
        case OpMul:
        case OpDiv:
        case OpMod:
        case OpBitAnd:
        case OpBitOr:
        case OpBitXor:
            run(e->left, e->type);
            run(e->right, e->type);
            break;

        case OpLShift:
        case OpRShift:
        case OpURShift:
            run(e->left, SInt32Type);
            run(e->right, SInt32Type);
            break;

        case OpGt:
        case OpLt:
        case OpGe:
        case OpLe:
        case OpEqual:
        case OpNotEqual:
            if (e->left->type == DoubleType) {
                run(e->right, DoubleType);
            } else if (e->right->type == DoubleType) {
                run(e->left, DoubleType);
            } else {
                run(e->left, e->left->type);
                run(e->right, e->right->type);
            }
            break;

        case OpStrictEqual:
        case OpStrictNotEqual:
        case OpInstanceof:
        case OpIn:
            run(e->left, e->left->type);
            run(e->right, e->right->type);
            break;

        default:
            Q_UNIMPLEMENTED();
            Q_UNREACHABLE();
        }
    }
    virtual void visitCall(Call *e) {
        run(e->base);
        for (ExprList *it = e->args; it; it = it->next)
            run(it->expr);
    }
    virtual void visitNew(New *e) {
        run(e->base);
        for (ExprList *it = e->args; it; it = it->next)
            run(it->expr);
    }
    virtual void visitSubscript(Subscript *e) { run(e->base); run(e->index); }
    virtual void visitMember(Member *e) { run(e->base); }
    virtual void visitExp(Exp *s) { run(s->expr); }
    virtual void visitMove(Move *s) {
        if (s->source->asConvert())
            return; // this statement got inserted for a phi-node type conversion

        run(s->target);

        if (Unop *u = s->source->asUnop()) {
            if (u->op == OpUPlus) {
                if (run(u->expr, s->target->type, false)) {
                    Convert *convert = _f->New<Convert>();
                    convert->init(u->expr, s->target->type);
                    s->source = convert;
                } else {
                    s->source = u->expr;
                }

                return;
            }
        }

        const Member *targetMember = s->target->asMember();
        const bool inhibitConversion = targetMember && targetMember->inhibitTypeConversionOnWrite;

        run(s->source, s->target->type, !inhibitConversion);
    }
    virtual void visitJump(Jump *) {}
    virtual void visitCJump(CJump *s) {
        run(s->cond, BoolType);
    }
    virtual void visitRet(Ret *s) { run(s->expr); }
    virtual void visitPhi(Phi *s) {
        Type ty = s->targetTemp->type;
        for (int i = 0, ei = s->d->incoming.size(); i != ei; ++i)
            run(s->d->incoming[i], ty);
    }
};

void splitCriticalEdges(IR::Function *f, DominatorTree &df, StatementWorklist &worklist, DefUses &defUses)
{
    foreach (BasicBlock *toBB, f->basicBlocks()) {
        if (toBB->isRemoved())
            continue;
        if (toBB->in.size() < 2)
            continue;

        for (int inIdx = 0, eInIdx = toBB->in.size(); inIdx != eInIdx; ++inIdx) {
            BasicBlock *fromBB = toBB->in[inIdx];
            if (fromBB->out.size() < 2)
                continue;

            // We found a critical edge.
            // create the basic block:
            BasicBlock *newBB = f->newBasicBlock(toBB->catchBlock);
            Jump *s = f->NewStmt<Jump>();
            worklist.registerNewStatement(s);
            defUses.registerNewStatement(s);
            s->init(toBB);
            newBB->appendStatement(s);

            // rewire the old outgoing edge
            int outIdx = fromBB->out.indexOf(toBB);
            fromBB->out[outIdx] = newBB;
            newBB->in.append(fromBB);

            // rewire the old incoming edge
            toBB->in[inIdx] = newBB;
            newBB->out.append(toBB);

            // add newBB to the correct loop group
            if (toBB->isGroupStart()) {
                BasicBlock *container;
                for (container = fromBB->containingGroup(); container; container = container->containingGroup())
                     if (container == toBB)
                         break;
                if (container == toBB) // if we were already inside the toBB loop
                    newBB->setContainingGroup(toBB);
                else
                    newBB->setContainingGroup(toBB->containingGroup());
            } else {
                newBB->setContainingGroup(toBB->containingGroup());
            }

            // patch the terminator
            Stmt *terminator = fromBB->terminator();
            if (Jump *j = terminator->asJump()) {
                Q_ASSERT(outIdx == 0);
                j->target = newBB;
            } else if (CJump *j = terminator->asCJump()) {
                if (outIdx == 0)
                    j->iftrue = newBB;
                else if (outIdx == 1)
                    j->iffalse = newBB;
                else
                    Q_ASSERT(!"Invalid out edge index for CJUMP!");
            } else if (terminator->asRet()) {
                Q_ASSERT(!"A block with a RET at the end cannot have outgoing edges.");
            } else {
                Q_ASSERT(!"Unknown terminator!");
            }

//            qDebug() << "splitting edge" << fromBB->index() << "->" << toBB->index()
//                     << "by inserting block" << newBB->index();

            // Set the immediate dominator of the new block to inBB
            df.setImmediateDominator(newBB, fromBB);

            bool toNeedsNewIdom = true;
            foreach (BasicBlock *bb, toBB->in) {
                if (bb != newBB && !df.dominates(toBB, bb)) {
                    toNeedsNewIdom = false;
                    break;
                }
            }
            if (toNeedsNewIdom)
                df.setImmediateDominator(toBB, newBB);
        }
    }
}

// Detect all (sub-)loops in a function.
//
// Doing loop detection on the CFG is better than relying on the statement information in
// order to mark loops. Although JavaScript only has natural loops, it can still be the case
// that something is not a loop even though a loop-like-statement is in the source. For
// example:
//    while (true) {
//      if (i > 0)
//        break;
//      else
//        break;
//    }
//
// Algorithm:
//  - do a DFS on the dominator tree, where for each node:
//    - collect all back-edges
//    - if there are back-edges, the node is a loop-header for a new loop, so:
//      - walk the CFG is reverse-direction, and for every node:
//        - if the node already belongs to a loop, we've found a nested loop:
//          - get the loop-header for the (outermost) nested loop
//          - add that loop-header to the current loop
//          - continue by walking all incoming edges that do not yet belong to the current loop
//        - if the node does not belong to a loop yet, add it to the current loop, and
//          go on with all incoming edges
//
// Loop-header detection by checking for back-edges is very straight forward: a back-edge is
// an incoming edge where the other node is dominated by the current node. Meaning: all
// execution paths that reach that other node have to go through the current node, that other
// node ends with a (conditional) jump back to the loop header.
//
// The exact order of the DFS on the dominator tree is not important. The only property has to
// be that a node is only visited when all the nodes it dominates have been visited before.
// The reason for the DFS is that for nested loops, the inner loop's loop-header is dominated
// by the outer loop's header. So, by visiting depth-first, sub-loops are identified before
// their containing loops, which makes nested-loop identification free. An added benefit is
// that the nodes for those sub-loops are only processed once.
//
// Note: independent loops that share the same header are merged together. For example, in
// the code snippet below, there are 2 back-edges into the loop-header, but only one single
// loop will be detected.
//    while (a) {
//      if (b)
//        continue;
//      else
//        continue;
//    }
class LoopDetection
{
    enum { DebugLoopDetection = 0 };

    Q_DISABLE_COPY(LoopDetection)

public:
    struct LoopInfo
    {
        BasicBlock *loopHeader;
        QVector<BasicBlock *> loopBody;
        QVector<LoopInfo *> nestedLoops;
        LoopInfo *parentLoop;

        LoopInfo(BasicBlock *loopHeader = 0)
            : loopHeader(loopHeader)
            , parentLoop(0)
        {}

        bool isValid() const
        { return loopHeader != 0; }

        void addNestedLoop(LoopInfo *nested)
        {
            Q_ASSERT(nested);
            Q_ASSERT(!nestedLoops.contains(nested));
            Q_ASSERT(nested->parentLoop == 0);
            nested->parentLoop = this;
            nestedLoops.append(nested);
        }
    };

public:
    LoopDetection(const DominatorTree &dt)
        : dt(dt)
    {}

    ~LoopDetection()
    {
        qDeleteAll(loopInfos);
    }

    void run(IR::Function *function)
    {
        std::vector<BasicBlock *> backedges;
        backedges.reserve(4);

        foreach (BasicBlock *bb, dt.calculateDFNodeIterOrder()) {
            Q_ASSERT(!bb->isRemoved());

            backedges.clear();

            foreach (BasicBlock *in, bb->in)
                if (dt.dominates(bb, in))
                    backedges.push_back(in);

            if (!backedges.empty()) {
                subLoop(bb, backedges);
            }
        }

        createLoopInfos(function);
        dumpLoopInfo();
    }

    void dumpLoopInfo() const
    {
        if (!DebugLoopDetection)
            return;

        foreach (LoopInfo *info, loopInfos) {
            qDebug() << "Loop header:" << info->loopHeader->index()
                     << "for loop" << quint64(info);
            foreach (BasicBlock *bb, info->loopBody)
                qDebug() << "    " << bb->index();
            foreach (LoopInfo *nested, info->nestedLoops)
                qDebug() << "    sub loop:" << quint64(nested);
            qDebug() << "     parent loop:" << quint64(info->parentLoop);
        }
    }

    QVector<LoopInfo *> allLoops() const
    { return loopInfos; }

    // returns all loop headers for loops that have no nested loops.
    QVector<LoopInfo *> innermostLoops() const
    {
        QVector<LoopInfo *> inner(loopInfos);

        for (int i = 0; i < inner.size(); ) {
            if (inner.at(i)->nestedLoops.isEmpty())
                ++i;
            else
                inner.remove(i);
        }

        return inner;
    }

private:
    void subLoop(BasicBlock *loopHead, const std::vector<BasicBlock *> &backedges)
    {
        loopHead->markAsGroupStart();

        std::vector<BasicBlock *> worklist;
        worklist.reserve(backedges.size() + 8);
        worklist.insert(worklist.end(), backedges.begin(), backedges.end());
        while (!worklist.empty()) {
            BasicBlock *predIt = worklist.back();
            worklist.pop_back();

            BasicBlock *subloop = predIt->containingGroup();
            if (subloop) {
                // This is a discovered block. Find its outermost discovered loop.
                while (BasicBlock *parentLoop = subloop->containingGroup())
                  subloop = parentLoop;

                // If it is already discovered to be a subloop of this loop, continue.
                if (subloop == loopHead)
                    continue;

                // Yay, it's a subloop of this loop.
                subloop->setContainingGroup(loopHead);
                predIt = subloop;

                // Add all predecessors of the subloop header to the worklist, as long as
                // those predecessors are not in the current subloop. It might be the case
                // that they are in other loops, which we will then add as a subloop to the
                // current loop.
                foreach (BasicBlock *predIn, predIt->in)
                    if (predIn->containingGroup() != subloop)
                        worklist.push_back(predIn);
            } else {
                if (predIt == loopHead)
                    continue;

                // This is an undiscovered block. Map it to the current loop.
                predIt->setContainingGroup(loopHead);

                // Add all incoming edges to the worklist.
                foreach (BasicBlock *bb, predIt->in)
                    worklist.push_back(bb);
            }
        }
    }

private:
    const DominatorTree &dt;
    QVector<LoopInfo *> loopInfos;

    void createLoopInfos(IR::Function *function)
    {
        foreach (BasicBlock *bb, function->basicBlocks()) {
            if (bb->isRemoved())
                continue;
            if (BasicBlock *loopHeader = bb->containingGroup())
                findLoop(loopHeader)->loopBody.append(bb);
        }

        foreach (LoopInfo *info, loopInfos) {
            if (BasicBlock *containingLoopHeader = info->loopHeader->containingGroup())
                findLoop(containingLoopHeader)->addNestedLoop(info);
        }
    }

    LoopInfo *findLoop(BasicBlock *loopHeader)
    {
        foreach (LoopInfo *info, loopInfos) {
            if (info->loopHeader == loopHeader)
                return info;
        }

        LoopInfo *info = new LoopInfo;
        info->loopHeader = loopHeader;
        loopInfos.append(info);
        return info;
    }
};

// High-level algorithm:
//  0. start with the first node (the start node) of a function
//  1. emit the node
//  2. add all outgoing edges that are not yet emitted to the postponed stack
//  3. When the postponed stack is empty, pop a stack from the loop stack. If that is empty too,
//     we're done.
//  4. pop a node from the postponed stack, and check if it can be scheduled:
//     a. if all incoming edges are scheduled, go to 4.
//     b. if an incoming edge is unscheduled, but it's a back-edge (an edge in a loop that jumps
//        back to the start of the loop), ignore it
//     c. if there is any unscheduled edge that is not a back-edge, ignore this node, and go to 4.
//  5. if this node is the start of a loop, push the postponed stack on the loop stack.
//  6. go back to 1.
//
// The postponing action in step 2 will put the node into its containing group. The case where this
// is important is when a (labeled) continue or a (labeled) break statement occur in a loop: the
// outgoing edge points to a node that is not part of the current loop (and possibly not of the
// parent loop).
//
// Linear scan register allocation benefits greatly from short life-time intervals with few holes
// (see for example section 4 (Lifetime Analysis) of [Wimmer1]). This algorithm makes sure that the
// blocks of a group are scheduled together, with no non-loop blocks in between. This applies
// recursively for nested loops. It also schedules groups of if-then-else-endif blocks together for
// the same reason.
class BlockScheduler
{
    IR::Function *function;
    const DominatorTree &dominatorTree;

    struct WorkForGroup
    {
        BasicBlock *group;
        QStack<BasicBlock *> postponed;

        WorkForGroup(BasicBlock *group = 0): group(group) {}
    };
    WorkForGroup currentGroup;
    QStack<WorkForGroup> postponedGroups;
    QVector<BasicBlock *> sequence;
    ProcessedBlocks emitted;
    QHash<BasicBlock *, BasicBlock *> loopsStartEnd;

    bool checkCandidate(BasicBlock *candidate)
    {
        Q_ASSERT(candidate->containingGroup() == currentGroup.group);

        foreach (BasicBlock *in, candidate->in) {
            if (emitted.alreadyProcessed(in))
                continue;

            if (dominatorTree.dominates(candidate, in))
                // this is a loop, where there in -> candidate edge is the jump back to the top of the loop.
                continue;

            return false; // an incoming edge that is not yet emitted, and is not a back-edge
        }

        if (candidate->isGroupStart()) {
            // postpone everything, and schedule the loop first.
            postponedGroups.push(currentGroup);
            currentGroup = WorkForGroup(candidate);
        }

        return true;
    }

    BasicBlock *pickNext()
    {
        while (true) {
            while (currentGroup.postponed.isEmpty()) {
                if (postponedGroups.isEmpty())
                    return 0;
                if (currentGroup.group) // record the first and the last node of a group
                    loopsStartEnd.insert(currentGroup.group, sequence.last());
                currentGroup = postponedGroups.pop();
            }

            BasicBlock *next = currentGroup.postponed.pop();
            if (checkCandidate(next))
                return next;
        }

        Q_UNREACHABLE();
        return 0;
    }

    void emitBlock(BasicBlock *bb)
    {
        Q_ASSERT(!bb->isRemoved());
        if (emitted.alreadyProcessed(bb))
            return;

        sequence.append(bb);
        emitted.markAsProcessed(bb);
    }

    void schedule(BasicBlock *functionEntryPoint)
    {
        BasicBlock *next = functionEntryPoint;

        while (next) {
            emitBlock(next);
            for (int i = next->out.size(); i != 0; ) {
                // postpone all outgoing edges, if they were not already processed
                --i;
                BasicBlock *out = next->out[i];
                if (!emitted.alreadyProcessed(out))
                    postpone(out);
            }
            next = pickNext();
        }
    }

    void postpone(BasicBlock *bb)
    {
        if (currentGroup.group == bb->containingGroup()) {
            currentGroup.postponed.append(bb);
            return;
        }

        for (int i = postponedGroups.size(); i != 0; ) {
            --i;
            WorkForGroup &g = postponedGroups[i];
            if (g.group == bb->containingGroup()) {
                g.postponed.append(bb);
                return;
            }
        }

        Q_UNREACHABLE();
    }

public:
    BlockScheduler(IR::Function *function, const DominatorTree &dominatorTree)
        : function(function)
        , dominatorTree(dominatorTree)
        , sequence(0)
        , emitted(function)
    {}

    QHash<BasicBlock *, BasicBlock *> go()
    {
        showMeTheCode(function);
        schedule(function->basicBlock(0));

#if defined(SHOW_SSA)
        qDebug() << "Block sequence:";
        foreach (BasicBlock *bb, sequence)
            qDebug("\tL%d", bb->index());
#endif // SHOW_SSA

        Q_ASSERT(function->liveBasicBlocksCount() == sequence.size());
        function->setScheduledBlocks(sequence);
        function->renumberBasicBlocks();
        return loopsStartEnd;
    }
};

#ifndef QT_NO_DEBUG
void checkCriticalEdges(QVector<BasicBlock *> basicBlocks) {
    foreach (BasicBlock *bb, basicBlocks) {
        if (bb && bb->out.size() > 1) {
            foreach (BasicBlock *bb2, bb->out) {
                if (bb2 && bb2->in.size() > 1) {
                    qout << "found critical edge between block "
                         << bb->index() << " and block " << bb2->index();
                    Q_ASSERT(false);
                }
            }
        }
    }
}
#endif

void cleanupBasicBlocks(IR::Function *function)
{
    showMeTheCode(function);

    // Algorithm: this is the iterative version of a depth-first search for all blocks that are
    // reachable through outgoing edges, starting with the start block and all exception handler
    // blocks.
    QBitArray reachableBlocks(function->basicBlockCount());
    QVector<BasicBlock *> postponed;
    postponed.reserve(16);
    for (int i = 0, ei = function->basicBlockCount(); i != ei; ++i) {
        BasicBlock *bb = function->basicBlock(i);
        if (i == 0 || bb->isExceptionHandler())
            postponed.append(bb);
    }

    while (!postponed.isEmpty()) {
        BasicBlock *bb = postponed.back();
        postponed.pop_back();
        if (bb->isRemoved()) // this block was removed before, we don't need to clean it up.
            continue;

        reachableBlocks.setBit(bb->index());

        foreach (BasicBlock *outBB, bb->out) {
            if (!reachableBlocks.at(outBB->index()))
                postponed.append(outBB);
        }
    }

    foreach (BasicBlock *bb, function->basicBlocks()) {
        if (bb->isRemoved()) // the block has already been removed, so ignore it
            continue;
        if (reachableBlocks.at(bb->index())) // the block is reachable, so ignore it
            continue;

        foreach (BasicBlock *outBB, bb->out) {
            if (outBB->isRemoved() || !reachableBlocks.at(outBB->index()))
                continue; // We do not need to unlink from blocks that are scheduled to be removed.

            int idx = outBB->in.indexOf(bb);
            if (idx != -1) {
                outBB->in.remove(idx);
                foreach (Stmt *s, outBB->statements()) {
                    if (Phi *phi = s->asPhi())
                        phi->d->incoming.remove(idx);
                    else
                        break;
                }
            }
        }

        function->removeBasicBlock(bb);
    }

    showMeTheCode(function);
}

inline Const *isConstPhi(Phi *phi)
{
    if (Const *c = phi->d->incoming[0]->asConst()) {
        for (int i = 1, ei = phi->d->incoming.size(); i != ei; ++i) {
            if (Const *cc = phi->d->incoming[i]->asConst()) {
                if (c->value != cc->value)
                    return 0;
                if (!(c->type == cc->type || (c->type & NumberType && cc->type & NumberType)))
                    return 0;
                if (int(c->value) == 0 && int(cc->value) == 0)
                    if (isNegative(c->value) != isNegative(cc->value))
                        return 0;
            } else {
                return 0;
            }
        }
        return c;
    }
    return 0;
}

static Expr *clone(Expr *e, IR::Function *function) {
    if (Temp *t = e->asTemp()) {
        return CloneExpr::cloneTemp(t, function);
    } else if (Const *c = e->asConst()) {
        return CloneExpr::cloneConst(c, function);
    } else if (Name *n = e->asName()) {
        return CloneExpr::cloneName(n, function);
    } else {
        Q_UNREACHABLE();
        return e;
    }
}

class ExprReplacer: public StmtVisitor, public ExprVisitor
{
    DefUses &_defUses;
    IR::Function* _function;
    Temp *_toReplace;
    Expr *_replacement;

public:
    ExprReplacer(DefUses &defUses, IR::Function *function)
        : _defUses(defUses)
        , _function(function)
        , _toReplace(0)
        , _replacement(0)
    {}

    void operator()(Temp *toReplace, Expr *replacement, StatementWorklist &W, QVector<Stmt *> *newUses = 0)
    {
        Q_ASSERT(replacement->asTemp() || replacement->asConst() || replacement->asName());

//        qout << "Replacing ";toReplace->dump(qout);qout<<" by ";replacement->dump(qout);qout<<endl;

        qSwap(_toReplace, toReplace);
        qSwap(_replacement, replacement);

        const QVector<Stmt *> &uses = _defUses.uses(*_toReplace);
        if (newUses)
            newUses->reserve(uses.size());

//        qout << "        " << uses.size() << " uses:"<<endl;
        foreach (Stmt *use, uses) {
//            qout<<"        ";use->dump(qout);qout<<"\n";
            use->accept(this);
//            qout<<"     -> ";use->dump(qout);qout<<"\n";
            W += use;
            if (newUses)
                newUses->push_back(use);
        }

        qSwap(_replacement, replacement);
        qSwap(_toReplace, toReplace);
    }

protected:
    virtual void visitConst(Const *) {}
    virtual void visitString(IR::String *) {}
    virtual void visitRegExp(IR::RegExp *) {}
    virtual void visitName(Name *) {}
    virtual void visitTemp(Temp *) {}
    virtual void visitArgLocal(ArgLocal *) {}
    virtual void visitClosure(Closure *) {}
    virtual void visitConvert(Convert *e) { check(e->expr); }
    virtual void visitUnop(Unop *e) { check(e->expr); }
    virtual void visitBinop(Binop *e) { check(e->left); check(e->right); }
    virtual void visitCall(Call *e) {
        check(e->base);
        for (ExprList *it = e->args; it; it = it->next)
            check(it->expr);
    }
    virtual void visitNew(New *e) {
        check(e->base);
        for (ExprList *it = e->args; it; it = it->next)
            check(it->expr);
    }
    virtual void visitSubscript(Subscript *e) { check(e->base); check(e->index); }
    virtual void visitMember(Member *e) { check(e->base); }
    virtual void visitExp(Exp *s) { check(s->expr); }
    virtual void visitMove(Move *s) { check(s->target); check(s->source); }
    virtual void visitJump(Jump *) {}
    virtual void visitCJump(CJump *s) { check(s->cond); }
    virtual void visitRet(Ret *s) { check(s->expr); }
    virtual void visitPhi(Phi *s) {
        for (int i = 0, ei = s->d->incoming.size(); i != ei; ++i)
            check(s->d->incoming[i]);
    }

private:
    void check(Expr *&e) {
        if (equals(e, _toReplace))
            e = clone(_replacement, _function);
        else
            e->accept(this);
    }

    // This only calculates equality for everything needed by constant propagation
    bool equals(Expr *e1, Expr *e2) const {
        if (e1 == e2)
            return true;

        if (Const *c1 = e1->asConst()) {
            if (Const *c2 = e2->asConst())
                return c1->value == c2->value && (c1->type == c2->type || (c1->type & NumberType && c2->type & NumberType));
        } else if (Temp *t1 = e1->asTemp()) {
            if (Temp *t2 = e2->asTemp())
                return *t1 == *t2;
        } else if (Name *n1 = e1->asName()) {
            if (Name *n2 = e2->asName()) {
                if (n1->id) {
                    if (n2->id)
                        return *n1->id == *n2->id;
                } else {
                    return n1->builtin == n2->builtin;
                }
            }
        }

        if (e1->type == IR::NullType && e2->type == IR::NullType)
            return true;
        if (e1->type == IR::UndefinedType && e2->type == IR::UndefinedType)
            return true;

        return false;
    }
};

namespace {
/// This function removes the basic-block from the function's list, unlinks any uses and/or defs,
/// and removes unreachable staements from the worklist, so that optimiseSSA won't consider them
/// anymore.
void unlink(BasicBlock *from, BasicBlock *to, IR::Function *func, DefUses &defUses,
            StatementWorklist &W, DominatorTree &dt)
{
    struct Util {
        static void removeIncomingEdge(BasicBlock *from, BasicBlock *to, DefUses &defUses, StatementWorklist &W)
        {
            int idx = to->in.indexOf(from);
            if (idx == -1)
                return;

            to->in.remove(idx);
            foreach (Stmt *outStmt, to->statements()) {
                if (!outStmt)
                    continue;
                if (Phi *phi = outStmt->asPhi()) {
                    if (Temp *t = phi->d->incoming[idx]->asTemp()) {
                        defUses.removeUse(phi, *t);
                        W += defUses.defStmt(*t);
                    }
                    phi->d->incoming.remove(idx);
                    W += phi;
                } else {
                    break;
                }
            }
        }

        static bool isReachable(BasicBlock *bb, const DominatorTree &dt)
        {
            foreach (BasicBlock *in, bb->in) {
                if (in->isRemoved())
                    continue;
                if (dt.dominates(bb, in)) // a back-edge, not interesting
                    continue;
                return true;
            }

            return false;
        }
    };

    // don't purge blocks that are entry points for catch statements. They might not be directly
    // connected, but are required anyway
    if (to->isExceptionHandler())
        return;

    // First, unlink the edge
    from->out.removeOne(to);
    Util::removeIncomingEdge(from, to, defUses, W);

    BasicBlockSet siblings;
    siblings.init(func);

    // Check if the target is still reachable...
    if (Util::isReachable(to, dt)) { // yes, recalculate the immediate dominator, and we're done.
        dt.collectSiblings(to, siblings);
    } else {
        // The target is unreachable, so purge it:
        QVector<BasicBlock *> toPurge;
        toPurge.reserve(8);
        toPurge.append(to);
        while (!toPurge.isEmpty()) {
            BasicBlock *bb = toPurge.first();
            toPurge.removeFirst();

            if (bb->isRemoved())
                continue;

            // unlink all incoming edges
            foreach (BasicBlock *in, bb->in) {
                int idx = in->out.indexOf(bb);
                if (idx != -1)
                    in->out.remove(idx);
            }

            // unlink all outgoing edges, including "arguments" to phi statements
            foreach (BasicBlock *out, bb->out) {
                if (out->isRemoved())
                    continue;

                Util::removeIncomingEdge(bb, out, defUses, W);

                if (Util::isReachable(out, dt)) {
                    dt.collectSiblings(out, siblings);
                } else {
                    // if a successor has no incoming edges after unlinking the current basic block,
                    // then it is unreachable, and can be purged too
                    toPurge.append(out);
                }
            }

            // unlink all defs/uses from the statements in the basic block
            foreach (Stmt *s, bb->statements()) {
                if (!s)
                    continue;

                W += defUses.removeDefUses(s);
                W -= s;
            }

            siblings.remove(bb);
            dt.setImmediateDominator(bb, 0);
            func->removeBasicBlock(bb);
        }
    }

    dt.recalculateIDoms(siblings);
}

bool tryOptimizingComparison(Expr *&expr)
{
    Binop *b = expr->asBinop();
    if (!b)
        return false;
    Const *leftConst = b->left->asConst();
    if (!leftConst || leftConst->type == StringType || leftConst->type == VarType || leftConst->type == QObjectType)
        return false;
    Const *rightConst = b->right->asConst();
    if (!rightConst || rightConst->type == StringType || rightConst->type == VarType || rightConst->type == QObjectType)
        return false;

    QV4::Primitive l = convertToValue(leftConst);
    QV4::Primitive r = convertToValue(rightConst);

    switch (b->op) {
    case OpGt:
        leftConst->value = Runtime::compareGreaterThan(&l, &r);
        leftConst->type = BoolType;
        expr = leftConst;
        return true;
    case OpLt:
        leftConst->value = Runtime::compareLessThan(&l, &r);
        leftConst->type = BoolType;
        expr = leftConst;
        return true;
    case OpGe:
        leftConst->value = Runtime::compareGreaterEqual(&l, &r);
        leftConst->type = BoolType;
        expr = leftConst;
        return true;
    case OpLe:
        leftConst->value = Runtime::compareLessEqual(&l, &r);
        leftConst->type = BoolType;
        expr = leftConst;
        return true;
    case OpStrictEqual:
        leftConst->value = Runtime::compareStrictEqual(&l, &r);
        leftConst->type = BoolType;
        expr = leftConst;
        return true;
    case OpEqual:
        leftConst->value = Runtime::compareEqual(&l, &r);
        leftConst->type = BoolType;
        expr = leftConst;
        return true;
    case OpStrictNotEqual:
        leftConst->value = Runtime::compareStrictNotEqual(&l, &r);
        leftConst->type = BoolType;
        expr = leftConst;
        return true;
    case OpNotEqual:
        leftConst->value = Runtime::compareNotEqual(&l, &r);
        leftConst->type = BoolType;
        expr = leftConst;
        return true;
    default:
        break;
    }

    return false;
}

void cfg2dot(IR::Function *f, const QVector<LoopDetection::LoopInfo *> &loops = QVector<LoopDetection::LoopInfo *>())
{
    static bool showCode = !qgetenv("QV4_SHOW_IR").isNull();
    if (!showCode)
        return;

    struct Util {
        static void genLoop(LoopDetection::LoopInfo *loop)
        {
            qout << "  subgraph \"cluster" << quint64(loop) << "\" {\n";
            qout << "    L" << loop->loopHeader->index() << ";\n";
            foreach (BasicBlock *bb, loop->loopBody)
                qout << "    L" << bb->index() << ";\n";
            foreach (LoopDetection::LoopInfo *nested, loop->nestedLoops)
                genLoop(nested);
            qout << "  }\n";
        }
    };

    QString name;
    if (f->name) name = *f->name;
    else name = QString::fromLatin1("%1").arg((unsigned long long)f);
    qout << "digraph \"" << name << "\" { ordering=out;\n";

    foreach (LoopDetection::LoopInfo *l, loops) {
        if (l->parentLoop == 0)
            Util::genLoop(l);
    }

    foreach (BasicBlock *bb, f->basicBlocks()) {
        if (bb->isRemoved())
            continue;

        int idx = bb->index();
        qout << "  L" << idx << " [label=\"L" << idx << "\"";
        if (idx == 0 || bb->terminator()->asRet())
            qout << ", shape=doublecircle";
        else
            qout << ", shape=circle";
        qout << "];\n";
        foreach (BasicBlock *out, bb->out)
            qout << "  L" << idx << " -> L" << out->index() << "\n";
    }

    qout << "}\n" << flush;
}

} // anonymous namespace

void optimizeSSA(StatementWorklist &W, DefUses &defUses, DominatorTree &df)
{
    IR::Function *function = W.function();
    ExprReplacer replaceUses(defUses, function);

    Stmt *s = 0;
    while (!W.isEmpty()) {
        s = W.takeNext(s);
        Q_ASSERT(s);

        if (Phi *phi = s->asPhi()) {
            // dead code elimination:
            if (defUses.useCount(*phi->targetTemp) == 0) {
                W += defUses.removeDefUses(phi);
                W.remove(s);
                continue;
            }

            // constant propagation:
            if (Const *c = isConstPhi(phi)) {
                replaceUses(phi->targetTemp, c, W);
                defUses.removeDef(*phi->targetTemp);
                W.remove(s);
                continue;
            }

            // copy propagation:
            if (phi->d->incoming.size() == 1) {
                Temp *t = phi->targetTemp;
                Expr *e = phi->d->incoming.first();

                QVector<Stmt *> newT2Uses;
                replaceUses(t, e, W, &newT2Uses);
                if (Temp *t2 = e->asTemp()) {
                    defUses.removeUse(s, *t2);
                    defUses.addUses(*t2, newT2Uses);
                    W += defUses.defStmt(*t2);
                }
                defUses.removeDef(*t);
                W.remove(s);
                continue;
            }
        } else  if (Move *m = s->asMove()) {
            if (Convert *convert = m->source->asConvert()) {
                if (Const *sourceConst = convert->expr->asConst()) {
                    convertConst(sourceConst, convert->type);
                    m->source = sourceConst;
                    W += m;
                    continue;
                } else if (Temp *sourceTemp = convert->expr->asTemp()) {
                    if (sourceTemp->type == convert->type) {
                        m->source = sourceTemp;
                        W += m;
                        continue;
                    }
                }
            }

            if (Temp *targetTemp = m->target->asTemp()) {
                // dead code elimination:
                if (defUses.useCount(*targetTemp) == 0) {
                    EliminateDeadCode(defUses, W).run(m->source, s);
                    if (!m->source)
                        W.remove(s);
                    continue;
                }

                // constant propagation:
                if (Const *sourceConst = m->source->asConst()) {
                    Q_ASSERT(sourceConst->type != UnknownType);
                    replaceUses(targetTemp, sourceConst, W);
                    defUses.removeDef(*targetTemp);
                    W.remove(s);
                    continue;
                }
                if (Member *member = m->source->asMember()) {
                    if (member->kind == Member::MemberOfEnum) {
                        Const *c = function->New<Const>();
                        const int enumValue = member->attachedPropertiesIdOrEnumValue;
                        c->init(SInt32Type, enumValue);
                        replaceUses(targetTemp, c, W);
                        defUses.removeDef(*targetTemp);
                        W.remove(s);
                        defUses.removeUse(s, *member->base->asTemp());
                        continue;
                    } else if (member->attachedPropertiesIdOrEnumValue != 0 && member->property && member->base->asTemp()) {
                        // Attached properties have no dependency on their base. Isel doesn't
                        // need it and we can eliminate the temp used to initialize it.
                        defUses.removeUse(s, *member->base->asTemp());
                        Const *c = function->New<Const>();
                        c->init(SInt32Type, 0);
                        member->base = c;
                        continue;
                    }
                }

                // copy propagation:
                if (Temp *sourceTemp = m->source->asTemp()) {
                    QVector<Stmt *> newT2Uses;
                    replaceUses(targetTemp, sourceTemp, W, &newT2Uses);
                    defUses.removeUse(s, *sourceTemp);
                    defUses.addUses(*sourceTemp, newT2Uses);
                    defUses.removeDef(*targetTemp);
                    W.remove(s);
                    continue;
                }

                if (Unop *unop = m->source->asUnop()) {
                    // Constant unary expression evaluation:
                    if (Const *constOperand = unop->expr->asConst()) {
                        if (constOperand->type & NumberType || constOperand->type == BoolType) {
                            // TODO: implement unop propagation for other constant types
                            bool doneSomething = false;
                            switch (unop->op) {
                            case OpNot:
                                constOperand->value = !constOperand->value;
                                constOperand->type = BoolType;
                                doneSomething = true;
                                break;
                            case OpUMinus:
                                if (int(constOperand->value) == 0 && int(constOperand->value) == constOperand->value) {
                                    if (isNegative(constOperand->value))
                                        constOperand->value = 0;
                                    else
                                        constOperand->value = -1 / Q_INFINITY;
                                    constOperand->type = DoubleType;
                                    doneSomething = true;
                                    break;
                                }

                                constOperand->value = -constOperand->value;
                                doneSomething = true;
                                break;
                            case OpUPlus:
                                if (unop->type != UnknownType)
                                    constOperand->type = unop->type;
                                doneSomething = true;
                                break;
                            case OpCompl:
                                constOperand->value = ~QV4::Primitive::toInt32(constOperand->value);
                                constOperand->type = SInt32Type;
                                doneSomething = true;
                                break;
                            case OpIncrement:
                                constOperand->value = constOperand->value + 1;
                                doneSomething = true;
                                break;
                            case OpDecrement:
                                constOperand->value = constOperand->value - 1;
                                doneSomething = true;
                                break;
                            default:
                                break;
                            };

                            if (doneSomething) {
                                m->source = constOperand;
                                W += m;
                            }
                        }
                    }
                    // TODO: if the result of a unary not operation is only used in a cjump,
                    //       then inline it.

                    continue;
                }

                if (Binop *binop = m->source->asBinop()) {
                    Const *leftConst = binop->left->asConst();
                    Const *rightConst = binop->right->asConst();

                    { // Typical casts to int32:
                        Expr *casted = 0;
                        switch (binop->op) {
                        case OpBitAnd:
                            if (leftConst && !rightConst && QV4::Primitive::toUInt32(leftConst->value) == 0xffffffff)
                                casted = binop->right;
                            else if (!leftConst && rightConst && QV4::Primitive::toUInt32(rightConst->value) == 0xffffffff)
                                casted = binop->left;
                            break;
                        case OpBitOr:
                            if (leftConst && !rightConst && QV4::Primitive::toInt32(leftConst->value) == 0)
                                casted = binop->right;
                            else if (!leftConst && rightConst && QV4::Primitive::toUInt32(rightConst->value) == 0)
                                casted = binop->left;
                            break;
                        default:
                            break;
                        }
                        if (casted && casted->type == SInt32Type) {
                            m->source = casted;
                            W += m;
                            continue;
                        }
                    }
                    if (rightConst) {
                        switch (binop->op) {
                        case OpLShift:
                        case OpRShift:
                            if (double v = QV4::Primitive::toInt32(rightConst->value) & 0x1f) {
                                // mask right hand side of shift operations
                                rightConst->value = v;
                                rightConst->type = SInt32Type;
                            } else {
                                // shifting a value over 0 bits is a move:
                                if (rightConst->value == 0) {
                                    m->source = binop->left;
                                    W += m;
                                }
                            }

                            break;
                        default:
                            break;
                        }
                    }

                    // TODO: More constant binary expression evaluation
                    // TODO: If the result of the move is only used in one single cjump, then
                    //       inline the binop into the cjump.
                    if (!leftConst || leftConst->type == StringType || leftConst->type == VarType || leftConst->type == QObjectType)
                        continue;
                    if (!rightConst || rightConst->type == StringType || rightConst->type == VarType || rightConst->type == QObjectType)
                        continue;

                    QV4::Primitive lc = convertToValue(leftConst);
                    QV4::Primitive rc = convertToValue(rightConst);
                    double l = lc.toNumber();
                    double r = rc.toNumber();

                    switch (binop->op) {
                    case OpMul:
                        leftConst->value = l * r;
                        leftConst->type = DoubleType;
                        m->source = leftConst;
                        W += m;
                        break;
                    case OpAdd:
                        leftConst->value = l + r;
                        leftConst->type = DoubleType;
                        m->source = leftConst;
                        W += m;
                        break;
                    case OpSub:
                        leftConst->value = l - r;
                        leftConst->type = DoubleType;
                        m->source = leftConst;
                        W += m;
                        break;
                    case OpDiv:
                        leftConst->value = l / r;
                        leftConst->type = DoubleType;
                        m->source = leftConst;
                        W += m;
                        break;
                    case OpMod:
                        leftConst->value = std::fmod(l, r);
                        leftConst->type = DoubleType;
                        m->source = leftConst;
                        W += m;
                        break;
                    default:
                        if (tryOptimizingComparison(m->source))
                            W += m;
                        break;
                    }

                    continue;
                }
            } // TODO: var{#0} = double{%10} where %10 is defined once and used once. E.g.: function(t){t = t % 2; return t; }

        } else if (CJump *cjump = s->asCJump()) {
            if (Const *constantCondition = cjump->cond->asConst()) {
                // Note: this assumes that there are no critical edges! Meaning, we can safely purge
                //       any basic blocks that are found to be unreachable.
                Jump *jump = function->NewStmt<Jump>();
                W.registerNewStatement(jump);
                if (convertToValue(constantCondition).toBoolean()) {
                    jump->target = cjump->iftrue;
                    unlink(cjump->parent, cjump->iffalse, function, defUses, W, df);
                } else {
                    jump->target = cjump->iffalse;
                    unlink(cjump->parent, cjump->iftrue, function, defUses, W, df);
                }
                W.replace(s, jump);

                continue;
            } else if (cjump->cond->asBinop()) {
                if (tryOptimizingComparison(cjump->cond))
                    W += cjump;
                continue;
            }
            // TODO: Constant unary expression evaluation
            // TODO: if the expression is an unary not operation, lift the expression, and switch
            //       the then/else blocks.
        }
    }

    W.applyToFunction();
}

//### TODO: use DefUses from the optimizer, because it already has all this information
class InputOutputCollector: protected StmtVisitor, protected ExprVisitor {
    void setOutput(Temp *out)
    {
        Q_ASSERT(!output);
        output = out;
    }

public:
    std::vector<Temp *> inputs;
    Temp *output;

    InputOutputCollector()
    { inputs.reserve(4); }

    void collect(Stmt *s) {
        inputs.resize(0);
        output = 0;
        s->accept(this);
    }

protected:
    virtual void visitConst(Const *) {}
    virtual void visitString(IR::String *) {}
    virtual void visitRegExp(IR::RegExp *) {}
    virtual void visitName(Name *) {}
    virtual void visitTemp(Temp *e) {
        inputs.push_back(e);
    }
    virtual void visitArgLocal(ArgLocal *) {}
    virtual void visitClosure(Closure *) {}
    virtual void visitConvert(Convert *e) { e->expr->accept(this); }
    virtual void visitUnop(Unop *e) { e->expr->accept(this); }
    virtual void visitBinop(Binop *e) { e->left->accept(this); e->right->accept(this); }
    virtual void visitCall(Call *e) {
        e->base->accept(this);
        for (ExprList *it = e->args; it; it = it->next)
            it->expr->accept(this);
    }
    virtual void visitNew(New *e) {
        e->base->accept(this);
        for (ExprList *it = e->args; it; it = it->next)
            it->expr->accept(this);
    }
    virtual void visitSubscript(Subscript *e) { e->base->accept(this); e->index->accept(this); }
    virtual void visitMember(Member *e) { e->base->accept(this); }
    virtual void visitExp(Exp *s) { s->expr->accept(this); }
    virtual void visitMove(Move *s) {
        s->source->accept(this);
        if (Temp *t = s->target->asTemp()) {
            setOutput(t);
        } else {
            s->target->accept(this);
        }
    }
    virtual void visitJump(Jump *) {}
    virtual void visitCJump(CJump *s) { s->cond->accept(this); }
    virtual void visitRet(Ret *s) { s->expr->accept(this); }
    virtual void visitPhi(Phi *) {
        // Handled separately
    }
};

/*
 * The algorithm is described in:
 *
 *   Linear Scan Register Allocation on SSA Form
 *   Christian Wimmer & Michael Franz, CGO'10, April 24-28, 2010
 *
 * See LifeTimeIntervals::renumber for details on the numbering.
 */
class LifeRanges {
    typedef QSet<Temp> LiveRegs;

    std::vector<LiveRegs> _liveIn;
    std::vector<LifeTimeInterval *> _intervals;
    LifeTimeIntervals::Ptr _sortedIntervals;

    LifeTimeInterval &interval(const Temp *temp)
    {
        LifeTimeInterval *&lti = _intervals[temp->index];
        if (Q_UNLIKELY(!lti)) {
            lti = new LifeTimeInterval;
            lti->setTemp(*temp);
        }
        return *lti;
    }

    int defPosition(IR::Stmt *s) const
    {
        return usePosition(s) + 1;
    }

    int usePosition(IR::Stmt *s) const
    {
        return _sortedIntervals->positionForStatement(s);
    }

    int start(IR::BasicBlock *bb) const
    {
        return _sortedIntervals->startPosition(bb);
    }

    int end(IR::BasicBlock *bb) const
    {
        return _sortedIntervals->endPosition(bb);
    }

public:
    LifeRanges(IR::Function *function, const QHash<BasicBlock *, BasicBlock *> &startEndLoops)
        : _intervals(function->tempCount)
    {
        _sortedIntervals = LifeTimeIntervals::create(function);
        _liveIn.resize(function->basicBlockCount());

        for (int i = function->basicBlockCount() - 1; i >= 0; --i) {
            BasicBlock *bb = function->basicBlock(i);
            buildIntervals(bb, startEndLoops.value(bb, 0));
        }

        _intervals.clear();
    }

    LifeTimeIntervals::Ptr intervals() const { return _sortedIntervals; }

    void dump() const
    {
        qout << "Life ranges:" << endl;
        qout << "Intervals:" << endl;
        foreach (const LifeTimeInterval *range, _sortedIntervals->intervals()) {
            range->dump(qout);
            qout << endl;
        }

        IRPrinter printer(&qout);
        for (int i = 0, ei = _liveIn.size(); i != ei; ++i) {
            qout << "L" << i <<" live-in: ";
            QList<Temp> live = QList<Temp>::fromSet(_liveIn.at(i));
            if (live.isEmpty())
                qout << "(none)";
            std::sort(live.begin(), live.end());
            for (int i = 0; i < live.size(); ++i) {
                if (i > 0) qout << ", ";
                printer.print(&live[i]);
            }
            qout << endl;
        }
    }

private:
    void buildIntervals(BasicBlock *bb, BasicBlock *loopEnd)
    {
        LiveRegs live;
        foreach (BasicBlock *successor, bb->out) {
            live.unite(_liveIn[successor->index()]);
            const int bbIndex = successor->in.indexOf(bb);
            Q_ASSERT(bbIndex >= 0);

            foreach (Stmt *s, successor->statements()) {
                if (Phi *phi = s->asPhi()) {
                    if (Temp *t = phi->d->incoming[bbIndex]->asTemp())
                        live.insert(*t);
                } else {
                    break;
                }
            }
        }

        QVector<Stmt *> statements = bb->statements();

        foreach (const Temp &opd, live)
            interval(&opd).addRange(start(bb), end(bb));

        InputOutputCollector collector;
        for (int i = statements.size() - 1; i >= 0; --i) {
            Stmt *s = statements.at(i);
            if (Phi *phi = s->asPhi()) {
                LiveRegs::iterator it = live.find(*phi->targetTemp);
                if (it == live.end()) {
                    // a phi node target that is only defined, but never used
                    interval(phi->targetTemp).setFrom(start(bb));
                } else {
                    live.erase(it);
                }
                _sortedIntervals->add(&interval(phi->targetTemp));
                continue;
            }
            collector.collect(s);
            //### TODO: use DefUses from the optimizer, because it already has all this information
            if (Temp *opd = collector.output) {
                LifeTimeInterval &lti = interval(opd);
                lti.setFrom(defPosition(s));
                live.remove(lti.temp());
                _sortedIntervals->add(&lti);
            }
            //### TODO: use DefUses from the optimizer, because it already has all this information
            for (unsigned i = 0, ei = collector.inputs.size(); i != ei; ++i) {
                Temp *opd = collector.inputs[i];
                interval(opd).addRange(start(bb), usePosition(s));
                live.insert(*opd);
            }
        }

        if (loopEnd) { // Meaning: bb is a loop header, because loopEnd is set to non-null.
            foreach (const Temp &opd, live)
                interval(&opd).addRange(start(bb), usePosition(loopEnd->terminator()));
        }

        _liveIn[bb->index()] = live;
    }
};

void removeUnreachleBlocks(IR::Function *function)
{
    QVector<BasicBlock *> newSchedule;
    newSchedule.reserve(function->basicBlockCount());
    foreach (BasicBlock *bb, function->basicBlocks())
        if (!bb->isRemoved())
            newSchedule.append(bb);
    function->setScheduledBlocks(newSchedule);
    function->renumberBasicBlocks();
}

class ConvertArgLocals: protected StmtVisitor, protected ExprVisitor
{
public:
    ConvertArgLocals(IR::Function *function)
        : function(function)
        , convertArgs(!function->usesArgumentsObject)
    {
        tempForFormal.resize(function->formals.size(), -1);
        tempForLocal.resize(function->locals.size(), -1);
    }

    void toTemps()
    {
        if (function->variablesCanEscape())
            return;

        QVector<Stmt *> extraMoves;
        if (convertArgs) {
            const int formalCount = function->formals.size();
            extraMoves.reserve(formalCount + function->basicBlock(0)->statementCount());
            extraMoves.resize(formalCount);

            for (int i = 0; i != formalCount; ++i) {
                const int newTemp = function->tempCount++;
                tempForFormal[i] = newTemp;

                ArgLocal *source = function->New<ArgLocal>();
                source->init(ArgLocal::Formal, i, 0);

                Temp *target = function->New<Temp>();
                target->init(Temp::VirtualRegister, newTemp);

                Move *m = function->NewStmt<Move>();
                m->init(target, source);
                extraMoves[i] = m;
            }
        }

        foreach (BasicBlock *bb, function->basicBlocks())
            if (!bb->isRemoved())
                foreach (Stmt *s, bb->statements())
                    s->accept(this);

        if (convertArgs && function->formals.size() > 0)
            function->basicBlock(0)->prependStatements(extraMoves);

        function->locals.clear();
    }

protected:
    virtual void visitConst(Const *) {}
    virtual void visitString(IR::String *) {}
    virtual void visitRegExp(IR::RegExp *) {}
    virtual void visitName(Name *) {}
    virtual void visitTemp(Temp *) {}
    virtual void visitArgLocal(ArgLocal *) {}
    virtual void visitClosure(Closure *) {}
    virtual void visitConvert(Convert *e) { check(e->expr); }
    virtual void visitUnop(Unop *e) { check(e->expr); }
    virtual void visitBinop(Binop *e) { check(e->left); check(e->right); }
    virtual void visitCall(Call *e) {
        check(e->base);
        for (ExprList *it = e->args; it; it = it->next)
            check(it->expr);
    }
    virtual void visitNew(New *e) {
        check(e->base);
        for (ExprList *it = e->args; it; it = it->next)
            check(it->expr);
    }
    virtual void visitSubscript(Subscript *e) { check(e->base); check(e->index); }
    virtual void visitMember(Member *e) { check(e->base); }
    virtual void visitExp(Exp *s) { check(s->expr); }
    virtual void visitMove(Move *s) { check(s->target); check(s->source); }
    virtual void visitJump(Jump *) {}
    virtual void visitCJump(CJump *s) { check(s->cond); }
    virtual void visitRet(Ret *s) { check(s->expr); }
    virtual void visitPhi(Phi *) {
        Q_UNREACHABLE();
    }

private:
    void check(Expr *&e) {
        if (ArgLocal *al = e->asArgLocal()) {
            if (al->kind == ArgLocal::Local) {
                Temp *t = function->New<Temp>();
                t->init(Temp::VirtualRegister, fetchTempForLocal(al->index));
                e = t;
            } else if (convertArgs && al->kind == ArgLocal::Formal) {
                Temp *t = function->New<Temp>();
                t->init(Temp::VirtualRegister, fetchTempForFormal(al->index));
                e = t;
            }
        } else {
            e->accept(this);
        }
    }

    int fetchTempForLocal(int local)
    {
        int &ref = tempForLocal[local];
        if (ref == -1)
            ref = function->tempCount++;
        return ref;
    }

    int fetchTempForFormal(int formal)
    {
        return tempForFormal[formal];
    }

    IR::Function *function;
    bool convertArgs;
    std::vector<int> tempForFormal;
    std::vector<int> tempForLocal;
};

class CloneBasicBlock: protected IR::StmtVisitor, protected CloneExpr
{
public:
    BasicBlock *operator()(IR::BasicBlock *originalBlock)
    {
        block = new BasicBlock(originalBlock->function, 0);

        foreach (Stmt *s, originalBlock->statements()) {
            s->accept(this);
            clonedStmt->location = s->location;
        }

        return block;
    }

protected:
    virtual void visitExp(Exp *stmt)
    { clonedStmt = block->EXP(clone(stmt->expr)); }

    virtual void visitMove(Move *stmt)
    { clonedStmt = block->MOVE(clone(stmt->target), clone(stmt->source)); }

    virtual void visitJump(Jump *stmt)
    { clonedStmt = block->JUMP(stmt->target); }

    virtual void visitCJump(CJump *stmt)
    { clonedStmt = block->CJUMP(clone(stmt->cond), stmt->iftrue, stmt->iffalse); }

    virtual void visitRet(Ret *stmt)
    { clonedStmt = block->RET(clone(stmt->expr)); }

    virtual void visitPhi(Phi *stmt)
    {
        Phi *phi = block->function->NewStmt<Phi>();
        clonedStmt = phi;

        phi->targetTemp = clone(stmt->targetTemp);
        phi->d = new Stmt::Data;
        foreach (Expr *in, stmt->d->incoming)
            phi->d->incoming.append(clone(in));
        block->appendStatement(phi);
    }

private:
    IR::Stmt *clonedStmt;
};

// Loop-peeling is done by unfolding the loop once. The "original" loop basic blocks stay where they
// are, and a copy of the loop is placed after it. Special care is taken while copying the loop body:
// by having the copies of the basic-blocks point to the same nodes as the "original" basic blocks,
// updating the immediate dominators is easy: if the edge of a copied basic-block B points to a
// block C that has also been copied, set the immediate dominator of B to the corresponding
// immediate dominator of C. Finally, for any node outside the loop that gets a new edge attached,
// the immediate dominator has to be re-calculated.
class LoopPeeling
{
    DominatorTree &dt;

public:
    LoopPeeling(DominatorTree &dt)
        : dt(dt)
    {}

    void run(const QVector<LoopDetection::LoopInfo *> &loops)
    {
        foreach (LoopDetection::LoopInfo *loopInfo, loops)
            peelLoop(loopInfo);
    }

private:
    // All copies have their outgoing edges pointing to the same successor block as the originals.
    // For each copied block, check where the outgoing edges point to. If it's a block inside the
    // (original) loop, rewire it to the corresponding copy. Otherwise, which is when it points
    // out of the loop, leave it alone.
    // As an extra, collect all edges that point out of the copied loop, because the targets need
    // to have their immediate dominator rechecked.
    void rewire(BasicBlock *newLoopBlock, const QVector<BasicBlock *> &from, const QVector<BasicBlock *> &to, QVector<BasicBlock *> &loopExits)
    {
        for (int i = 0, ei = newLoopBlock->out.size(); i != ei; ++i) {
            BasicBlock *&out = newLoopBlock->out[i];
            const int idx = from.indexOf(out);
            if (idx == -1) {
                if (!loopExits.contains(out))
                    loopExits.append(out);
            } else {
                out->in.removeOne(newLoopBlock);
                BasicBlock *newTo = to.at(idx);
                newTo->in.append(newLoopBlock);
                out = newTo;

                Stmt *terminator = newLoopBlock->terminator();
                if (Jump *jump = terminator->asJump()) {
                    Q_ASSERT(i == 0);
                    jump->target = newTo;
                } else if (CJump *cjump = terminator->asCJump()) {
                    Q_ASSERT(i == 0 || i == 1);
                    if (i == 0)
                        cjump->iftrue = newTo;
                    else
                        cjump->iffalse = newTo;
                }
            }
        }
    }

    void peelLoop(LoopDetection::LoopInfo *loop)
    {
        CloneBasicBlock clone;

        LoopDetection::LoopInfo unpeeled(*loop);
        unpeeled.loopHeader = clone(unpeeled.loopHeader);
        unpeeled.loopHeader->setContainingGroup(loop->loopHeader->containingGroup());
        unpeeled.loopHeader->markAsGroupStart(true);
        for (int i = 0, ei = unpeeled.loopBody.size(); i != ei; ++i) {
            BasicBlock *&bodyBlock = unpeeled.loopBody[i];
            bodyBlock = clone(bodyBlock);
            bodyBlock->setContainingGroup(unpeeled.loopHeader);
            Q_ASSERT(bodyBlock->statementCount() == loop->loopBody[i]->statementCount());
        }

        // The cloned blocks will have no incoming edges, but they do have outgoing ones (copying
        // the terminators will automatically insert that edge). The blocks where the originals
        // pointed to will have an extra incoming edge from the copied blocks.

        foreach (BasicBlock *in, loop->loopHeader->in) {
            if (unpeeled.loopHeader != in // this can happen for really tight loops (where there are no body blocks). This is a back-edge in that case.
                    && !unpeeled.loopBody.contains(in) // if the edge is not coming from within the copied set, leave it alone
                    && !dt.dominates(loop->loopHeader, in)) // an edge coming from within the loop (so a back-edge): this is handled when rewiring all outgoing edges
                continue;

            unpeeled.loopHeader->in.append(in);
            loop->loopHeader->in.removeOne(in);

            Stmt *terminator = in->terminator();
            if (Jump *jump = terminator->asJump()) {
                jump->target = unpeeled.loopHeader;
                in->out[0] = unpeeled.loopHeader;
            } else if (CJump *cjump = terminator->asCJump()) {
                if (cjump->iftrue == loop->loopHeader) {
                    cjump->iftrue = unpeeled.loopHeader;
                    Q_ASSERT(in->out[0] == loop->loopHeader);
                    in->out[0] = unpeeled.loopHeader;
                } else if (cjump->iffalse == loop->loopHeader) {
                    cjump->iffalse = unpeeled.loopHeader;
                    Q_ASSERT(in->out[1] == loop->loopHeader);
                    in->out[1] = unpeeled.loopHeader;
                } else {
                    Q_UNREACHABLE();
                }
            }
        }

        QVector<BasicBlock *> loopExits;
        loopExits.reserve(8);
        loopExits.append(unpeeled.loopHeader);

        IR::Function *f = unpeeled.loopHeader->function;
        rewire(unpeeled.loopHeader, loop->loopBody, unpeeled.loopBody, loopExits);
        f->addBasicBlock(unpeeled.loopHeader);
        for (int i = 0, ei = unpeeled.loopBody.size(); i != ei; ++i) {
            BasicBlock *bodyBlock = unpeeled.loopBody.at(i);
            rewire(bodyBlock, loop->loopBody, unpeeled.loopBody, loopExits);
            f->addBasicBlock(bodyBlock);
        }

        // The original loop is now peeled off, and won't jump back to the loop header. Meaning, it
        // is not a loop anymore, so unmark it.
        loop->loopHeader->markAsGroupStart(false);
        foreach (BasicBlock *bb, loop->loopBody)
            bb->setContainingGroup(loop->loopHeader->containingGroup());

        // calculate the idoms in a separate loop, because addBasicBlock in the previous loop will
        // set the block index, which in turn is used by the dominator tree.
        for (int i = 0, ei = unpeeled.loopBody.size(); i != ei; ++i) {
            BasicBlock *bodyBlock = unpeeled.loopBody.at(i);
            BasicBlock *idom = dt.immediateDominator(loop->loopBody.at(i));
            const int idx = loop->loopBody.indexOf(idom);
            if (idom == loop->loopHeader)
                idom = unpeeled.loopHeader;
            else if (idx != -1)
                idom = unpeeled.loopBody.at(idx);
            Q_ASSERT(idom);
            dt.setImmediateDominator(bodyBlock, idom);
        }

        BasicBlockSet siblings(f);
        foreach (BasicBlock *bb, loopExits)
            dt.collectSiblings(bb, siblings);

        dt.recalculateIDoms(siblings, loop->loopHeader);
    }
};

static void verifyCFG(IR::Function *function)
{
    if (!DoVerification)
        return;

    foreach (BasicBlock *bb, function->basicBlocks()) {
        if (bb->isRemoved()) {
            Q_ASSERT(bb->in.isEmpty());
            Q_ASSERT(bb->out.isEmpty());
            continue;
        }

        Q_ASSERT(function->basicBlock(bb->index()) == bb);

        // Check the terminators:
        if (Jump *jump = bb->terminator()->asJump()) {
            Q_UNUSED(jump);
            Q_ASSERT(jump->target);
            Q_ASSERT(!jump->target->isRemoved());
            Q_ASSERT(bb->out.size() == 1);
            Q_ASSERT(bb->out.first() == jump->target);
        } else if (CJump *cjump = bb->terminator()->asCJump()) {
            Q_UNUSED(cjump);
            Q_ASSERT(bb->out.size() == 2);
            Q_ASSERT(cjump->iftrue);
            Q_ASSERT(!cjump->iftrue->isRemoved());
            Q_ASSERT(cjump->iftrue == bb->out[0]);
            Q_ASSERT(cjump->iffalse);
            Q_ASSERT(!cjump->iffalse->isRemoved());
            Q_ASSERT(cjump->iffalse == bb->out[1]);
        } else if (bb->terminator()->asRet()) {
            Q_ASSERT(bb->out.size() == 0);
        } else {
            Q_UNREACHABLE();
        }

        // Check the outgoing edges:
        foreach (BasicBlock *out, bb->out) {
            Q_UNUSED(out);
            Q_ASSERT(!out->isRemoved());
            Q_ASSERT(out->in.contains(bb));
        }

        // Check the incoming edges:
        foreach (BasicBlock *in, bb->in) {
            Q_UNUSED(in);
            Q_ASSERT(!in->isRemoved());
            Q_ASSERT(in->out.contains(bb));
        }
    }
}

static void verifyImmediateDominators(const DominatorTree &dt, IR::Function *function)
{
    if (!DoVerification)
        return;

    cfg2dot(function);
    dt.dumpImmediateDominators();
    DominatorTree referenceTree(function);

    foreach (BasicBlock *bb, function->basicBlocks()) {
        if (bb->isRemoved())
            continue;

        BasicBlock *idom = dt.immediateDominator(bb);
        BasicBlock *referenceIdom = referenceTree.immediateDominator(bb);
        Q_UNUSED(idom);
        Q_UNUSED(referenceIdom);
        Q_ASSERT(idom == referenceIdom);
    }
}

static void verifyNoPointerSharing(IR::Function *function)
{
    if (!DoVerification)
        return;

    class : public StmtVisitor, public ExprVisitor {
    public:
        void operator()(IR::Function *f)
        {
            foreach (BasicBlock *bb, f->basicBlocks()) {
                if (bb->isRemoved())
                    continue;

                foreach (Stmt *s, bb->statements())
                    s->accept(this);
            }
        }

    protected:
        virtual void visitExp(Exp *s) { check(s); s->expr->accept(this); }
        virtual void visitMove(Move *s) { check(s); s->target->accept(this); s->source->accept(this); }
        virtual void visitJump(Jump *s) { check(s); }
        virtual void visitCJump(CJump *s) { check(s); s->cond->accept(this); }
        virtual void visitRet(Ret *s) { check(s); s->expr->accept(this); }
        virtual void visitPhi(Phi *s)
        {
            check(s);
            s->targetTemp->accept(this);
            foreach (Expr *e, s->d->incoming)
                e->accept(this);
        }

        virtual void visitConst(Const *e) { check(e); }
        virtual void visitString(IR::String *e) { check(e); }
        virtual void visitRegExp(IR::RegExp *e) { check(e); }
        virtual void visitName(Name *e) { check(e); }
        virtual void visitTemp(Temp *e) { check(e); }
        virtual void visitArgLocal(ArgLocal *e) { check(e); }
        virtual void visitClosure(Closure *e) { check(e); }
        virtual void visitConvert(Convert *e) { check(e); e->expr->accept(this); }
        virtual void visitUnop(Unop *e) { check(e); e->expr->accept(this); }
        virtual void visitBinop(Binop *e) { check(e); e->left->accept(this); e->right->accept(this); }
        virtual void visitCall(Call *e) { check(e); e->base->accept(this); check(e->args); }
        virtual void visitNew(New *e) { check(e); e->base->accept(this); check(e->args); }
        virtual void visitSubscript(Subscript *e) { check(e); e->base->accept(this); e->index->accept(this); }
        virtual void visitMember(Member *e) { check(e); e->base->accept(this); }

        void check(ExprList *l)
        {
            for (ExprList *it = l; it; it = it->next)
                check(it->expr);
        }

    private:
        void check(Stmt *s)
        {
            Q_ASSERT(!stmts.contains(s));
            stmts.insert(s);
        }

        void check(Expr *e)
        {
            Q_ASSERT(!exprs.contains(e));
            exprs.insert(e);
        }

        QSet<Stmt *> stmts;
        QSet<Expr *> exprs;
    } V;
    V(function);
}

} // anonymous namespace

void LifeTimeInterval::setFrom(int from) {
    Q_ASSERT(from > 0);

    if (_ranges.isEmpty()) { // this is the case where there is no use, only a define
        _ranges.push_front(Range(from, from));
        if (_end == InvalidPosition)
            _end = from;
    } else {
        _ranges.first().start = from;
    }
}

void LifeTimeInterval::addRange(int from, int to) {
    Q_ASSERT(from > 0);
    Q_ASSERT(to > 0);
    Q_ASSERT(to >= from);

    if (_ranges.isEmpty()) {
        _ranges.push_front(Range(from, to));
        _end = to;
        return;
    }

    Range *p = &_ranges.first();
    if (to + 1 >= p->start && p->end + 1 >= from) {
        p->start = qMin(p->start, from);
        p->end = qMax(p->end, to);
        while (_ranges.count() > 1) {
            Range *p1 = p + 1;
            if (p->end + 1 < p1->start || p1->end + 1 < p->start)
                break;
            p1->start = qMin(p->start, p1->start);
            p1->end = qMax(p->end, p1->end);
            _ranges.pop_front();
            p = &_ranges.first();
        }
    } else {
        if (to < p->start) {
            _ranges.push_front(Range(from, to));
        } else {
            Q_ASSERT(from > _ranges.last().end);
            _ranges.push_back(Range(from, to));
        }
    }

    _end = _ranges.last().end;
}

LifeTimeInterval LifeTimeInterval::split(int atPosition, int newStart)
{
    Q_ASSERT(atPosition < newStart || newStart == InvalidPosition);

    if (_ranges.isEmpty() || atPosition < _ranges.first().start)
        return LifeTimeInterval();

    LifeTimeInterval newInterval = *this;
    newInterval.setSplitFromInterval(true);

    // search where to split the interval
    for (int i = 0, ei = _ranges.size(); i < ei; ++i) {
        if (_ranges.at(i).start <= atPosition) {
            if (_ranges.at(i).end >= atPosition) {
                // split happens in the middle of a range. Keep this range in both the old and the
                // new interval, and correct the end/start later
                _ranges.resize(i + 1);
                newInterval._ranges.remove(0, i);
                break;
            }
        } else {
            // split happens between two ranges.
            _ranges.resize(i);
            newInterval._ranges.remove(0, i);
            break;
        }
    }

    if (newInterval._ranges.first().end == atPosition)
        newInterval._ranges.removeFirst();

    if (newStart == InvalidPosition) {
        // the temp stays inactive for the rest of its lifetime
        newInterval = LifeTimeInterval();
    } else {
        // find the first range where the temp will get active again:
        while (!newInterval._ranges.isEmpty()) {
            const Range &range = newInterval._ranges.first();
            if (range.start > newStart) {
                // The split position is before the start of the range. Either we managed to skip
                // over the correct range, or we got an invalid split request. Either way, this
                // Should Never Happen <TM>.
                Q_ASSERT(range.start > newStart);
                return LifeTimeInterval();
            } else if (range.start <= newStart && range.end >= newStart) {
                // yay, we found the range that should be the new first range in the new interval!
                break;
            } else {
                // the temp stays inactive for this interval, so remove it.
                newInterval._ranges.removeFirst();
            }
        }
        Q_ASSERT(!newInterval._ranges.isEmpty());
        newInterval._ranges.first().start = newStart;
        _end = newStart;
    }

    // if we're in the middle of a range, set the end to the split position
    if (_ranges.last().end > atPosition)
        _ranges.last().end = atPosition;

    validate();
    newInterval.validate();

    return newInterval;
}

void LifeTimeInterval::dump(QTextStream &out) const {
    IRPrinter(&out).print(const_cast<Temp *>(&_temp));
    out << ": ends at " << _end << " with ranges ";
    if (_ranges.isEmpty())
        out << "(none)";
    for (int i = 0; i < _ranges.size(); ++i) {
        if (i > 0) out << ", ";
        out << _ranges[i].start << " - " << _ranges[i].end;
    }
    if (_reg != InvalidRegister)
        out << " (register " << _reg << ")";
}

bool LifeTimeInterval::lessThan(const LifeTimeInterval *r1, const LifeTimeInterval *r2) {
    if (r1->_ranges.first().start == r2->_ranges.first().start) {
        if (r1->isSplitFromInterval() == r2->isSplitFromInterval())
            return r1->_ranges.last().end < r2->_ranges.last().end;
        else
            return r1->isSplitFromInterval();
    } else
        return r1->_ranges.first().start < r2->_ranges.first().start;
}

bool LifeTimeInterval::lessThanForTemp(const LifeTimeInterval *r1, const LifeTimeInterval *r2)
{
    return r1->temp() < r2->temp();
}

LifeTimeIntervals::LifeTimeIntervals(IR::Function *function)
    : _basicBlockPosition(function->basicBlockCount())
    , _positionForStatement(function->statementCount(), IR::Stmt::InvalidId)
    , _lastPosition(0)
{
    _intervals.reserve(function->tempCount + 32); // we reserve a bit more space for intervals, because the register allocator will add intervals with fixed ranges for each register.
    renumber(function);
}

// Renumbering works as follows:
//  - phi statements are not numbered
//  - statement numbers start at 0 (zero) and increment get an even number (lastPosition + 2)
//  - basic blocks start at firstStatementNumber - 1, or rephrased: lastPosition + 1
//  - basic blocks end at the number of the last statement
// And during life-time calculation the next rule is used:
//  - any temporary starts its life-time at definingStatementPosition + 1
//
// This numbering simulates half-open intervals. For example:
//   0: %1 = 1
//   2: %2 = 2
//   4: %3 = %1 + %2
//   6: print(%3)
// Here the half-open life-time intervals would be:
//   %1: (0-4]
//   %2: (2-4]
//   %3: (4-6]
// Instead, we use the even statement positions for uses of temporaries, and the odd positions for
// their definitions:
//   %1: [1-4]
//   %2: [3-4]
//   %3: [5-6]
// This has the nice advantage that placing %3 (for example) is really easy: the start will
// never overlap with the end of the uses of the operands used in the defining statement.
//
// The reason to start a basic-block at firstStatementPosition - 1 is to have correct start
// positions for target temporaries of phi-nodes. Those temporaries will now start before the
// first statement. This also means that any moves that get generated when transforming out of SSA
// form, will not interfere with (read: overlap) any defining statements in the preceding
// basic-block.
void LifeTimeIntervals::renumber(IR::Function *function)
{
    foreach (BasicBlock *bb, function->basicBlocks()) {
        if (bb->isRemoved())
            continue;

        _basicBlockPosition[bb->index()].start = _lastPosition + 1;

        foreach (Stmt *s, bb->statements()) {
            if (s->asPhi())
                continue;

            _lastPosition += 2;
            _positionForStatement[s->id()] = _lastPosition;
        }

        _basicBlockPosition[bb->index()].end = _lastPosition;
    }
}

LifeTimeIntervals::~LifeTimeIntervals()
{
    qDeleteAll(_intervals);
}

Optimizer::Optimizer(IR::Function *function)
    : function(function)
    , inSSA(false)
{}

void Optimizer::run(QQmlEnginePrivate *qmlEngine, bool doTypeInference, bool peelLoops)
{
#if defined(SHOW_SSA)
    qout << "##### NOW IN FUNCTION " << (function->name ? qPrintable(*function->name) : "anonymous!")
         << " with " << function->basicBlocks.size() << " basic blocks." << endl << flush;
#endif

//    showMeTheCode(function);

    cleanupBasicBlocks(function);

    function->removeSharedExpressions();

//    showMeTheCode(function);

    static bool doSSA = qgetenv("QV4_NO_SSA").isEmpty();

    if (!function->hasTry && !function->hasWith && !function->module->debugMode && doSSA) {
//        qout << "SSA for " << (function->name ? qPrintable(*function->name) : "<anonymous>") << endl;

        ConvertArgLocals(function).toTemps();
        showMeTheCode(function);

        // Calculate the dominator tree:
        DominatorTree df(function);

        {
            // This is in a separate scope, because loop-peeling doesn't (yet) update the LoopInfo
            // calculated by the LoopDetection. So by putting it in a separate scope, it is not
            // available after peeling.

            LoopDetection loopDetection(df);
            loopDetection.run(function);
            showMeTheCode(function);
//            cfg2dot(function, loopDetection.allLoops());

            if (peelLoops) {
                QVector<LoopDetection::LoopInfo *> innerLoops = loopDetection.innermostLoops();
                LoopPeeling(df).run(innerLoops);

//                cfg2dot(function, loopDetection.allLoops());
                showMeTheCode(function);
                if (!innerLoops.isEmpty())
                    verifyImmediateDominators(df, function);
            }
        }

        verifyCFG(function);
        verifyNoPointerSharing(function);

        df.computeDF();

        verifyCFG(function);
        verifyImmediateDominators(df, function);

        DefUses defUses(function);

//        qout << "Converting to SSA..." << endl;
        convertToSSA(function, df, defUses);
//        showMeTheCode(function);
//        defUses.dump();

//        qout << "Cleaning up phi nodes..." << endl;
        cleanupPhis(defUses);
        showMeTheCode(function);

        StatementWorklist worklist(function);

        if (doTypeInference) {
//            qout << "Running type inference..." << endl;
            TypeInference(qmlEngine, defUses).run(worklist);
            showMeTheCode(function);

//            qout << "Doing reverse inference..." << endl;
            ReverseInference(defUses).run(function);
//            showMeTheCode(function);

//            qout << "Doing type propagation..." << endl;
            TypePropagation(defUses).run(function, worklist);
//            showMeTheCode(function);
            verifyNoPointerSharing(function);
        }

        static bool doOpt = qgetenv("QV4_NO_OPT").isEmpty();
        if (doOpt) {
//            qout << "Running SSA optimization..." << endl;
            worklist.reset();
            optimizeSSA(worklist, defUses, df);
            showMeTheCode(function);

            verifyImmediateDominators(df, function);
            verifyCFG(function);
        }

        verifyNoPointerSharing(function);

        // Basic-block cycles that are unreachable (i.e. for loops in a then-part where the
        // condition is calculated to be always false) are not yet removed. This will choke the
        // block scheduling, so remove those now.
//        qout << "Cleaning up unreachable basic blocks..." << endl;
        cleanupBasicBlocks(function);
//        showMeTheCode(function);

        // Transform the CFG into edge-split SSA.
//        qout << "Starting edge splitting..." << endl;
        splitCriticalEdges(function, df, worklist, defUses);
//        showMeTheCode(function);

        verifyImmediateDominators(df, function);
        verifyCFG(function);

//        qout << "Doing block scheduling..." << endl;
//        df.dumpImmediateDominators();
        startEndLoops = BlockScheduler(function, df).go();
        showMeTheCode(function);
//        cfg2dot(function);

#ifndef QT_NO_DEBUG
        checkCriticalEdges(function->basicBlocks());
#endif

//        qout << "Finished SSA." << endl;
        inSSA = true;
    } else {
        removeUnreachleBlocks(function);
        inSSA = false;
    }
}

void Optimizer::convertOutOfSSA() {
    if (!inSSA)
        return;

    // There should be no critical edges at this point.

    foreach (BasicBlock *bb, function->basicBlocks()) {
        MoveMapping moves;

        foreach (BasicBlock *successor, bb->out) {
            const int inIdx = successor->in.indexOf(bb);
            Q_ASSERT(inIdx >= 0);
            foreach (Stmt *s, successor->statements()) {
                if (Phi *phi = s->asPhi()) {
                    moves.add(clone(phi->d->incoming[inIdx], function),
                              clone(phi->targetTemp, function)->asTemp());
                } else {
                    break;
                }
            }
        }

    #if defined(DEBUG_MOVEMAPPING)
        QTextStream os(stdout, QIODevice::WriteOnly);
        os << "Move mapping for function ";
        if (function->name)
            os << *function->name;
        else
            os << (void *) function;
        os << " on basic-block L" << bb->index << ":" << endl;
        moves.dump();
    #endif // DEBUG_MOVEMAPPING

        moves.order();

        moves.insertMoves(bb, function, true);
    }

    foreach (BasicBlock *bb, function->basicBlocks()) {
        while (!bb->isEmpty()) {
            if (bb->statements().first()->asPhi()) {
                bb->removeStatement(0);
            } else {
                break;
            }
        }
    }
}

LifeTimeIntervals::Ptr Optimizer::lifeTimeIntervals() const
{
    Q_ASSERT(isInSSA());

    LifeRanges lifeRanges(function, startEndLoops);
//    lifeRanges.dump();
//    showMeTheCode(function);
    return lifeRanges.intervals();
}

QSet<Jump *> Optimizer::calculateOptionalJumps()
{
    QSet<Jump *> optional;
    QSet<BasicBlock *> reachableWithoutJump;

    const int maxSize = function->basicBlockCount();
    optional.reserve(maxSize);
    reachableWithoutJump.reserve(maxSize);

    for (int i = maxSize - 1; i >= 0; --i) {
        BasicBlock *bb = function->basicBlock(i);
        if (bb->isRemoved())
            continue;

        if (Jump *jump = bb->statements().last()->asJump()) {
            if (reachableWithoutJump.contains(jump->target)) {
                if (bb->statements().size() > 1)
                    reachableWithoutJump.clear();
                optional.insert(jump);
                reachableWithoutJump.insert(bb);
                continue;
            }
        }

        reachableWithoutJump.clear();
        reachableWithoutJump.insert(bb);
    }

#if 0
    QTextStream out(stdout, QIODevice::WriteOnly);
    out << "Jumps to ignore:" << endl;
    foreach (Jump *j, removed) {
        out << "\t" << j->id << ": ";
        j->dump(out, Stmt::MIR);
        out << endl;
    }
#endif

    return optional;
}

void Optimizer::showMeTheCode(IR::Function *function)
{
    ::showMeTheCode(function);
}

static inline bool overlappingStorage(const Temp &t1, const Temp &t2)
{
    // This is the same as the operator==, but for one detail: memory locations are not sensitive
    // to types, and neither are general-purpose registers.

    if (t1.index != t2.index)
        return false; // different position, where-ever that may (physically) be.
    if (t1.kind != t2.kind)
        return false; // formal/local/(physical-)register/stack do never overlap
    if (t1.kind != Temp::PhysicalRegister) // Other than registers, ...
        return t1.kind == t2.kind; // ... everything else overlaps: any memory location can hold everything.

    // So now the index is the same, and we know that both stored in a register. If both are
    // floating-point registers, they are the same. Or, if both are non-floating-point registers,
    // generally called general-purpose registers, they are also the same.
    return (t1.type == DoubleType && t2.type == DoubleType)
            || (t1.type != DoubleType && t2.type != DoubleType);
}

MoveMapping::Moves MoveMapping::sourceUsages(Expr *e, const Moves &moves)
{
    Moves usages;

    if (Temp *t = e->asTemp()) {
        for (int i = 0, ei = moves.size(); i != ei; ++i) {
            const Move &move = moves[i];
            if (Temp *from = move.from->asTemp())
                if (overlappingStorage(*from, *t))
                    usages.append(move);
        }
    }

    return usages;
}

void MoveMapping::add(Expr *from, Temp *to) {
    if (Temp *t = from->asTemp()) {
        if (overlappingStorage(*t, *to)) {
            // assignments like fp1 = fp1 or var{&1} = double{&1} can safely be skipped.
#if defined(DEBUG_MOVEMAPPING)
            QTextStream os(stderr, QIODevice::WriteOnly);
            os << "Skipping ";
            to->dump(os);
            os << " <- ";
            from->dump(os);
            os << endl;
#endif // DEBUG_MOVEMAPPING
            return;
        }
    }

    Move m(from, to);
    if (_moves.contains(m))
        return;
    _moves.append(m);
}

void MoveMapping::order()
{
    QList<Move> todo = _moves;
    QList<Move> output, swaps;
    output.reserve(_moves.size());
    QList<Move> delayed;
    delayed.reserve(_moves.size());

    while (!todo.isEmpty()) {
        const Move m = todo.first();
        todo.removeFirst();
        schedule(m, todo, delayed, output, swaps);
    }

    output += swaps;

    Q_ASSERT(todo.isEmpty());
    Q_ASSERT(delayed.isEmpty());
    qSwap(_moves, output);
}

QList<IR::Move *> MoveMapping::insertMoves(BasicBlock *bb, IR::Function *function, bool atEnd) const
{
    QList<IR::Move *> newMoves;
    newMoves.reserve(_moves.size());

    int insertionPoint = atEnd ? bb->statements().size() - 1 : 0;
    foreach (const Move &m, _moves) {
        IR::Move *move = function->NewStmt<IR::Move>();
        move->init(clone(m.to, function), clone(m.from, function));
        move->swap = m.needsSwap;
        bb->insertStatementBefore(insertionPoint++, move);
        newMoves.append(move);
    }

    return newMoves;
}

void MoveMapping::dump() const
{
#if defined(DEBUG_MOVEMAPPING)
    QTextStream os(stdout, QIODevice::WriteOnly);
    os << "Move mapping has " << _moves.size() << " moves..." << endl;
    foreach (const Move &m, _moves) {
        os << "\t";
        m.to->dump(os);
        if (m.needsSwap)
            os << " <-> ";
        else
            os << " <-- ";
        m.from->dump(os);
        os << endl;
    }
#endif // DEBUG_MOVEMAPPING
}

MoveMapping::Action MoveMapping::schedule(const Move &m, QList<Move> &todo, QList<Move> &delayed,
                                          QList<Move> &output, QList<Move> &swaps) const
{
    Moves usages = sourceUsages(m.to, todo) + sourceUsages(m.to, delayed);
    foreach (const Move &dependency, usages) {
        if (!output.contains(dependency)) {
            if (delayed.contains(dependency)) {
                // We have a cycle! Break it by swapping instead of assigning.
#if defined(DEBUG_MOVEMAPPING)
                delayed+=m;
                QTextStream out(stderr, QIODevice::WriteOnly);
                out<<"we have a cycle! temps:" << endl;
                foreach (const Move &m, delayed) {
                    out<<"\t";
                    m.to->dump(out);
                    out<<" <- ";
                    m.from->dump(out);
                    out<<endl;
                }
                delayed.removeOne(m);
#endif // DEBUG_MOVEMAPPING
                return NeedsSwap;
            } else {
                delayed.append(m);
                todo.removeOne(dependency);
                Action action = schedule(dependency, todo, delayed, output, swaps);
                delayed.removeOne(m);
                Move mm(m);
                if (action == NeedsSwap) {
                    mm.needsSwap = true;
                    swaps.append(mm);
                } else {
                    output.append(mm);
                }
                return action;
            }
        }
    }

    output.append(m);
    return NormalMove;
}

// References:
//  [Wimmer1] C. Wimmer and M. Franz. Linear Scan Register Allocation on SSA Form. In Proceedings of
//            CGO’10, ACM Press, 2010
//  [Wimmer2] C. Wimmer and H. Mossenbock. Optimized Interval Splitting in a Linear Scan Register
//            Allocator. In Proceedings of the ACM/USENIX International Conference on Virtual
//            Execution Environments, pages 132–141. ACM Press, 2005.
//  [Briggs]  P. Briggs, K.D. Cooper, T.J. Harvey, and L.T. Simpson. Practical Improvements to the
//            Construction and Destruction of Static Single Assignment Form.
//  [Appel]   A.W. Appel. Modern Compiler Implementation in Java. Second edition, Cambridge
//            University Press.
//  [Ramalingam] G. Ramalingam and T. Reps. An Incremental Algorithm for Maintaining the Dominator
//               Tree of a Reducible Flowgraph.
