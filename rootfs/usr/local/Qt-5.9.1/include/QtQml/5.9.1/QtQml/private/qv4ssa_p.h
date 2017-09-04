/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QV4SSA_P_H
#define QV4SSA_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qv4jsir_p.h"
#include "qv4isel_util_p.h"
#include <private/qv4util_p.h>
#include <QtCore/QSharedPointer>

QT_BEGIN_NAMESPACE
class QTextStream;
class QQmlEnginePrivate;

namespace QV4 {
namespace IR {

struct LifeTimeIntervalRange {
    int start;
    int end;

    LifeTimeIntervalRange(int start = -1, int end = -1)
        : start(start)
        , end(end)
    {}

    bool covers(int position) const { return start <= position && position <= end; }
};
} // IR namespace
} // QV4 namespace

Q_DECLARE_TYPEINFO(QV4::IR::LifeTimeIntervalRange, Q_PRIMITIVE_TYPE);

namespace QV4 {
namespace IR {

class Q_AUTOTEST_EXPORT LifeTimeInterval {
public:
    typedef QVarLengthArray<LifeTimeIntervalRange, 4> Ranges;

private:
    Temp _temp;
    Ranges _ranges;
    int _end;
    int _reg;
    unsigned _isFixedInterval : 1;
    unsigned _isSplitFromInterval : 1;

public:
    enum { InvalidPosition = -1 };
    enum { InvalidRegister = -1 };

    explicit LifeTimeInterval(int rangeCapacity = 4)
        : _end(InvalidPosition)
        , _reg(InvalidRegister)
        , _isFixedInterval(0)
        , _isSplitFromInterval(0)
    { _ranges.reserve(rangeCapacity); }

    bool isValid() const { return _end != InvalidRegister; }

    void setTemp(const Temp &temp) { this->_temp = temp; }
    Temp temp() const { return _temp; }
    bool isFP() const { return _temp.type == IR::DoubleType; }

    void setFrom(int from);
    void addRange(int from, int to);
    const Ranges &ranges() const { return _ranges; }

    int start() const { return _ranges.first().start; }
    int end() const { return _end; }
    bool covers(int position) const
    {
        for (int i = 0, ei = _ranges.size(); i != ei; ++i) {
            if (_ranges.at(i).covers(position))
                return true;
        }
        return false;
    }

    int reg() const { return _reg; }
    void setReg(int reg) { Q_ASSERT(!_isFixedInterval); _reg = reg; }

    bool isFixedInterval() const { return _isFixedInterval; }
    void setFixedInterval(bool isFixedInterval) { _isFixedInterval = isFixedInterval; }

    LifeTimeInterval split(int atPosition, int newStart);
    bool isSplitFromInterval() const { return _isSplitFromInterval; }
    void setSplitFromInterval(bool isSplitFromInterval) { _isSplitFromInterval = isSplitFromInterval; }

    void dump(QTextStream &out) const;
    static bool lessThan(const LifeTimeInterval *r1, const LifeTimeInterval *r2);
    static bool lessThanForTemp(const LifeTimeInterval *r1, const LifeTimeInterval *r2);

    void validate() const {
#if !defined(QT_NO_DEBUG)
        // Validate the new range
        if (_end != InvalidPosition) {
            Q_ASSERT(!_ranges.isEmpty());
            for (const LifeTimeIntervalRange &range : qAsConst(_ranges)) {
                Q_ASSERT(range.start >= 0);
                Q_ASSERT(range.end >= 0);
                Q_ASSERT(range.start <= range.end);
            }
        }
#endif
    }
};

inline bool LifeTimeInterval::lessThan(const LifeTimeInterval *r1, const LifeTimeInterval *r2)
{
    if (r1->_ranges.first().start == r2->_ranges.first().start) {
        if (r1->isSplitFromInterval() == r2->isSplitFromInterval())
            return r1->_ranges.last().end < r2->_ranges.last().end;
        else
            return r1->isSplitFromInterval();
    } else
        return r1->_ranges.first().start < r2->_ranges.first().start;
}

class LifeTimeIntervals
{
    Q_DISABLE_COPY(LifeTimeIntervals)

    LifeTimeIntervals(IR::Function *function);
    void renumber(IR::Function *function);

public:
    typedef QSharedPointer<LifeTimeIntervals> Ptr;
    static Ptr create(IR::Function *function)
    { return Ptr(new LifeTimeIntervals(function)); }

    ~LifeTimeIntervals();

    // takes ownership of the pointer
    void add(LifeTimeInterval *interval)
    { _intervals.append(interval); }

    // After calling Optimizer::lifeTimeIntervals() the result will have all intervals in descending order of start position.
    QVector<LifeTimeInterval *> intervals() const
    { return _intervals; }

    int size() const
    { return _intervals.size(); }

    int positionForStatement(Stmt *stmt) const
    {
        Q_ASSERT(stmt->id() >= 0);
        if (static_cast<unsigned>(stmt->id()) < _positionForStatement.size())
            return _positionForStatement[stmt->id()];

        return Stmt::InvalidId;
    }

    int startPosition(BasicBlock *bb) const
    {
        Q_ASSERT(bb->index() >= 0);
        Q_ASSERT(static_cast<unsigned>(bb->index()) < _basicBlockPosition.size());

        return _basicBlockPosition.at(bb->index()).start;
    }

    int endPosition(BasicBlock *bb) const
    {
        Q_ASSERT(bb->index() >= 0);
        Q_ASSERT(static_cast<unsigned>(bb->index()) < _basicBlockPosition.size());

        return _basicBlockPosition.at(bb->index()).end;
    }

    int lastPosition() const
    {
        return _lastPosition;
    }

private:
    struct BasicBlockPositions {
        int start;
        int end;

        BasicBlockPositions()
            : start(IR::Stmt::InvalidId)
            , end(IR::Stmt::InvalidId)
        {}
    };

    std::vector<BasicBlockPositions> _basicBlockPosition;
    std::vector<int> _positionForStatement;
    QVector<LifeTimeInterval *> _intervals;
    int _lastPosition;
};

class Q_QML_PRIVATE_EXPORT Optimizer
{
    Q_DISABLE_COPY(Optimizer)

public:
    Optimizer(Function *function);

    void run(QQmlEnginePrivate *qmlEngine, bool doTypeInference = true, bool peelLoops = true);
    void convertOutOfSSA();

    bool isInSSA() const
    { return inSSA; }

    QHash<BasicBlock *, BasicBlock *> loopStartEndBlocks() const { return startEndLoops; }

    LifeTimeIntervals::Ptr lifeTimeIntervals() const;

    BitVector calculateOptionalJumps();

    static void showMeTheCode(Function *function, const char *marker);

private:
    Function *function;
    bool inSSA;
    QHash<BasicBlock *, BasicBlock *> startEndLoops;
};

class Q_QML_AUTOTEST_EXPORT MoveMapping
{
#ifdef V4_AUTOTEST
public:
#endif
    struct Move {
        Expr *from;
        Temp *to;
        bool needsSwap;

        Move(Expr *from, Temp *to, bool needsSwap = false)
            : from(from), to(to), needsSwap(needsSwap)
        {}

        bool operator==(const Move &other) const
        { return from == other.from && to == other.to; }
    };
    typedef QList<Move> Moves;

    Moves _moves;

    static Moves sourceUsages(Expr *e, const Moves &moves);

public:
    void add(Expr *from, Temp *to);
    void order();
    QList<IR::Move *> insertMoves(BasicBlock *bb, Function *function, bool atEnd) const;

    void dump() const;

private:
    int findLeaf() const;
};

/*
 * stack slot allocation:
 *
 * foreach bb do
 *   foreach stmt do
 *     if the current statement is not a phi-node:
 *       purge ranges that end before the current statement
 *       check for life ranges to activate, and if they don't have a stackslot associated then allocate one
 *       renumber temps to stack
 *     for phi nodes: check if all temps (src+dst) are assigned stack slots and marked as allocated
 *     if it's a jump:
 *       foreach phi node in the successor:
 *         allocate slots for each temp (both sources and targets) if they don't have one allocated already
 *         insert moves before the jump
 */
class AllocateStackSlots: protected ConvertTemps
{
    IR::LifeTimeIntervals::Ptr _intervals;
    QVector<IR::LifeTimeInterval *> _unhandled;
    QVector<IR::LifeTimeInterval *> _live;
    QBitArray _slotIsInUse;
    IR::Function *_function;

    int defPosition(IR::Stmt *s) const
    {
        return usePosition(s) + 1;
    }

    int usePosition(IR::Stmt *s) const
    {
        return _intervals->positionForStatement(s);
    }

public:
    AllocateStackSlots(const IR::LifeTimeIntervals::Ptr &intervals)
        : _intervals(intervals)
        , _slotIsInUse(intervals->size(), false)
        , _function(0)
    {
        _live.reserve(8);
        _unhandled = _intervals->intervals();
    }

    void forFunction(IR::Function *function)
    {
        IR::Optimizer::showMeTheCode(function, "Before stack slot allocation");
        _function = function;
        toStackSlots(function);
    }

protected:
    int allocateFreeSlot() override
    {
        for (int i = 0, ei = _slotIsInUse.size(); i != ei; ++i) {
            if (!_slotIsInUse[i]) {
                if (_nextUnusedStackSlot <= i) {
                    Q_ASSERT(_nextUnusedStackSlot == i);
                    _nextUnusedStackSlot = i + 1;
                }
                _slotIsInUse[i] = true;
                return i;
            }
        }

        Q_UNREACHABLE();
        return -1;
    }

    void process(IR::Stmt *s) override
    {
//        qDebug("L%d statement %d:", _currentBasicBlock->index, s->id);

        if (IR::Phi *phi = s->asPhi()) {
            visitPhi(phi);
        } else {
            // purge ranges no longer alive:
            for (int i = 0; i < _live.size(); ) {
                const IR::LifeTimeInterval *lti = _live.at(i);
                if (lti->end() < usePosition(s)) {
//                    qDebug() << "\t - moving temp" << lti->temp().index << "to handled, freeing slot" << _stackSlotForTemp[lti->temp().index];
                    _live.remove(i);
                    Q_ASSERT(_slotIsInUse[_stackSlotForTemp[lti->temp().index]]);
                    _slotIsInUse[_stackSlotForTemp[lti->temp().index]] = false;
                    continue;
                } else {
                    ++i;
                }
            }

            // active new ranges:
            while (!_unhandled.isEmpty()) {
                IR::LifeTimeInterval *lti = _unhandled.last();
                if (lti->start() > defPosition(s))
                    break; // we're done
                Q_ASSERT(!_stackSlotForTemp.contains(lti->temp().index));
                _stackSlotForTemp[lti->temp().index] = allocateFreeSlot();
//                qDebug() << "\t - activating temp" << lti->temp().index << "on slot" << _stackSlotForTemp[lti->temp().index];
                _live.append(lti);
                _unhandled.removeLast();
            }

            visit(s);
        }

        if (IR::Jump *jump = s->asJump()) {
            IR::MoveMapping moves;
            for (IR::Stmt *succStmt : jump->target->statements()) {
                if (IR::Phi *phi = succStmt->asPhi()) {
                    forceActivation(*phi->targetTemp);
                    for (int i = 0, ei = phi->incoming.size(); i != ei; ++i) {
                        IR::Expr *e = phi->incoming[i];
                        if (IR::Temp *t = e->asTemp()) {
                            forceActivation(*t);
                        }
                        if (jump->target->in[i] == _currentBasicBlock)
                            moves.add(phi->incoming[i], phi->targetTemp);
                    }
                } else {
                    break;
                }
            }
            moves.order();
            const QList<IR::Move *> newMoves = moves.insertMoves(_currentBasicBlock, _function, true);
            for (IR::Move *move : newMoves)
                visit(move);
        }
    }

    void forceActivation(const IR::Temp &t)
    {
        if (_stackSlotForTemp.contains(t.index))
            return;

        int i = _unhandled.size() - 1;
        for (; i >= 0; --i) {
            IR::LifeTimeInterval *lti = _unhandled[i];
            if (lti->temp() == t) {
                _live.append(lti);
                _unhandled.remove(i);
                break;
            }
        }
        Q_ASSERT(i >= 0); // check that we always found the entry

        _stackSlotForTemp[t.index] = allocateFreeSlot();
//        qDebug() << "\t - force activating temp" << t.index << "on slot" << _stackSlotForTemp[t.index];
    }

    void visitPhi(IR::Phi *phi) override
    {
        Q_UNUSED(phi);
#if !defined(QT_NO_DEBUG)
        Q_ASSERT(_stackSlotForTemp.contains(phi->targetTemp->index));
        Q_ASSERT(_slotIsInUse[_stackSlotForTemp[phi->targetTemp->index]]);
        for (IR::Expr *e : phi->incoming) {
            if (IR::Temp *t = e->asTemp())
                Q_ASSERT(_stackSlotForTemp.contains(t->index));
        }
#endif // defined(QT_NO_DEBUG)
    }
};

} // IR namespace
} // QV4 namespace


Q_DECLARE_TYPEINFO(QV4::IR::LifeTimeInterval, Q_MOVABLE_TYPE);

QT_END_NAMESPACE

#endif // QV4SSA_P_H
