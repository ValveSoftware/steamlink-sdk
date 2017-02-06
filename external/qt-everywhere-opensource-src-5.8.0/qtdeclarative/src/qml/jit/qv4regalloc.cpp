/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the V4VM module of the Qt Toolkit.
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

#include <QtCore/QBuffer>
#include <QtCore/QDebug>
#include "qv4regalloc_p.h"
#include "qv4alloca_p.h"
#include <private/qv4value_p.h>

#include <algorithm>
#if defined(Q_CC_MINGW)
#  include <malloc.h>
#endif

namespace {
enum { DebugRegAlloc = 0 };

struct Use {
    enum RegisterFlag { MustHaveRegister = 0, CouldHaveRegister = 1 };
    unsigned flag : 1;
    unsigned pos  : 31;

    Use(): flag(MustHaveRegister), pos(0) {}
    Use(int position, RegisterFlag flag): flag(flag), pos(position)
    { Q_ASSERT(position >= 0); }

    bool mustHaveRegister() const { return flag == MustHaveRegister; }
};
}

QT_BEGIN_NAMESPACE

Q_DECLARE_TYPEINFO(Use, Q_MOVABLE_TYPE);

using namespace QV4::IR;

namespace QV4 {
namespace JIT {

namespace {
class IRPrinterWithPositions: public IRPrinter
{
    LifeTimeIntervals::Ptr intervals;
    const int positionSize;

public:
    IRPrinterWithPositions(QTextStream *out, const LifeTimeIntervals::Ptr &intervals)
        : IRPrinter(out)
        , intervals(intervals)
        , positionSize(QString::number(intervals->lastPosition()).size())
    {}

protected:
    void addStmtNr(Stmt *s) Q_DECL_OVERRIDE Q_DECL_FINAL
    {
        addJustifiedNr(intervals->positionForStatement(s));
    }
};

class IRPrinterWithRegisters: public IRPrinterWithPositions
{
    const RegisterInformation &_registerInformation;
    QHash<int, const RegisterInfo *> _infoForRegularRegister;
    QHash<int, const RegisterInfo *> _infoForFPRegister;

public:
    IRPrinterWithRegisters(QTextStream *out, const LifeTimeIntervals::Ptr &intervals,
                           const RegisterInformation &registerInformation)
        : IRPrinterWithPositions(out, intervals)
        , _registerInformation(registerInformation)
    {
        for (int i = 0, ei = _registerInformation.size(); i != ei; ++i)
            if (_registerInformation.at(i).isRegularRegister())
                _infoForRegularRegister.insert(_registerInformation.at(i).reg<int>(),
                                               &_registerInformation.at(i));
            else
                _infoForFPRegister.insert(_registerInformation.at(i).reg<int>(),
                                          &_registerInformation.at(i));
    }

protected:
    void visitTemp(Temp *e) Q_DECL_OVERRIDE Q_DECL_FINAL
    {
        switch (e->kind) {
        case Temp::PhysicalRegister: {
            const RegisterInfo *ri = e->type == DoubleType ? _infoForFPRegister.value(e->index, 0)
                                                           : _infoForRegularRegister.value(e->index, 0);
            if (ri) {
                *out << ri->prettyName();
                break;
            }
        }
        default:
            IRPrinterWithPositions::visitTemp(e);
        }
    }
};
}

class RegAllocInfo: public IRDecoder
{
public:
    typedef QVarLengthArray<Temp, 4> Hints;

private:
    struct Def {
        unsigned valid : 1;
        unsigned canHaveReg : 1;
        unsigned isPhiTarget : 1;

        Def(): valid(0), canHaveReg(0), isPhiTarget(0) {}
        Def(bool canHaveReg, bool isPhiTarget)
            : valid(1), canHaveReg(canHaveReg), isPhiTarget(isPhiTarget)
        {
        }

        bool isValid() const { return valid != 0; }
    };

    IR::LifeTimeIntervals::Ptr _lifeTimeIntervals;
    BasicBlock *_currentBB;
    Stmt *_currentStmt;
    std::vector<Def> _defs;
    std::vector<std::vector<Use> > _uses;
    std::vector<int> _calls;
    std::vector<Hints> _hints;

    int usePosition(Stmt *s) const
    {
        int usePos = _lifeTimeIntervals->positionForStatement(s);
        if (usePos == Stmt::InvalidId) // phi-node operand, so:
            usePos = _lifeTimeIntervals->startPosition(_currentBB);
        return usePos;
    }

public:
    RegAllocInfo(): _currentBB(0), _currentStmt(0) {}

    void collect(IR::Function *function, const IR::LifeTimeIntervals::Ptr &lifeTimeIntervals)
    {
        _lifeTimeIntervals = lifeTimeIntervals;
        _defs.resize(function->tempCount);
        _uses.resize(function->tempCount);
        _calls.reserve(function->statementCount() / 3);
        _hints.resize(function->tempCount);

        for (BasicBlock *bb : function->basicBlocks()) {
            _currentBB = bb;
            for (Stmt *s : bb->statements()) {
                _currentStmt = s;
                visit(s);
            }
        }
    }

    const std::vector<Use> &uses(const Temp &t) const
    {
        return _uses.at(t.index);
    }

    bool canHaveRegister(const Temp &t) const {
        Q_ASSERT(_defs[t.index].isValid());
        return _defs[t.index].canHaveReg;
    }
    bool isPhiTarget(const Temp &t) const {
        Q_ASSERT(_defs[t.index].isValid());
        return _defs[t.index].isPhiTarget;
    }

    const std::vector<int> &calls() const { return _calls; }
    const Hints &hints(const Temp &t) const { return _hints[t.index]; }
    void addHint(const Temp &t, int physicalRegister)
    { addHint(t, Temp::PhysicalRegister, physicalRegister); }

    void addHint(const Temp &t, Temp::Kind kind, int hintedIndex)
    {
        Hints &hints = _hints[t.index];
        for (Hints::iterator i = hints.begin(), ei = hints.end(); i != ei; ++i)
            if (i->index == hintedIndex)
                return;

        Temp hint;
        hint.init(kind, hintedIndex);
        hints.append(hint);
    }

    void dump() const
    {
        if (!DebugRegAlloc)
            return;

        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        QTextStream qout(&buf);
        IRPrinterWithPositions printer(&qout, _lifeTimeIntervals);

        qout << "RegAllocInfo:" << endl << "Defs/uses:" << endl;
        for (unsigned t = 0; t < _defs.size(); ++t) {
            const std::vector<Use> &uses = _uses[t];
            if (uses.empty())
                continue;
            qout << "%" << t <<": "
                 << " ("
                 << (_defs[t].canHaveReg ? "can" : "can NOT")
                 << " have a register, and "
                 << (_defs[t].isPhiTarget ? "is" : "is NOT")
                 << " defined by a phi node), uses at: ";
            for (unsigned i = 0; i < uses.size(); ++i) {
                if (i > 0) qout << ", ";
                qout << uses[i].pos;
                if (uses[i].mustHaveRegister()) qout << "(R)"; else qout << "(S)";
            }
            qout << endl;
        }

        qout << "Calls at: ";
        for (unsigned i = 0; i < _calls.size(); ++i) {
            if (i > 0) qout << ", ";
            qout << _calls[i];
        }
        qout << endl;

        qout << "Hints:" << endl;
        for (unsigned t = 0; t < _hints.size(); ++t) {
            if (_uses[t].empty())
                continue;
            qout << "\t%" << t << ": ";
            const Hints &hints = _hints[t];
            for (int i = 0; i < hints.size(); ++i) {
                if (i > 0) qout << ", ";
                printer.print(hints[i]);
            }
            qout << endl;
        }
        qDebug("%s", buf.data().constData());
    }

protected: // IRDecoder
    virtual void callBuiltinInvalid(IR::Name *, IR::ExprList *, IR::Expr *) {}
    virtual void callBuiltinTypeofQmlContextProperty(IR::Expr *, IR::Member::MemberKind, int, IR::Expr *) {}
    virtual void callBuiltinTypeofMember(IR::Expr *, const QString &, IR::Expr *) {}
    virtual void callBuiltinTypeofSubscript(IR::Expr *, IR::Expr *, IR::Expr *) {}
    virtual void callBuiltinTypeofName(const QString &, IR::Expr *) {}
    virtual void callBuiltinTypeofValue(IR::Expr *, IR::Expr *) {}
    virtual void callBuiltinDeleteMember(IR::Expr *, const QString &, IR::Expr *) {}
    virtual void callBuiltinDeleteSubscript(IR::Expr *, IR::Expr *, IR::Expr *) {}
    virtual void callBuiltinDeleteName(const QString &, IR::Expr *) {}
    virtual void callBuiltinDeleteValue(IR::Expr *) {}
    virtual void callBuiltinThrow(IR::Expr *) {}
    virtual void callBuiltinReThrow() {}
    virtual void callBuiltinUnwindException(IR::Expr *) {}
    virtual void callBuiltinPushCatchScope(const QString &) {};
    virtual void callBuiltinForeachIteratorObject(IR::Expr *, IR::Expr *) {}
    virtual void callBuiltinForeachNextProperty(IR::Temp *, IR::Temp *) {}
    virtual void callBuiltinForeachNextPropertyname(IR::Expr *, IR::Expr *) {}
    virtual void callBuiltinPushWithScope(IR::Expr *) {}
    virtual void callBuiltinPopScope() {}
    virtual void callBuiltinDeclareVar(bool , const QString &) {}
    virtual void callBuiltinDefineArray(IR::Expr *, IR::ExprList *) {}
    virtual void callBuiltinDefineObjectLiteral(IR::Expr *, int, IR::ExprList *, IR::ExprList *, bool) {}
    virtual void callBuiltinSetupArgumentObject(IR::Expr *) {}
    virtual void callBuiltinConvertThisToObject() {}

    virtual void callValue(IR::Expr *value, IR::ExprList *args, IR::Expr *result)
    {
        addDef(result);
        if (IR::Temp *tempValue = value->asTemp())
            addUses(tempValue, Use::CouldHaveRegister);
        addUses(args, Use::CouldHaveRegister);
        addCall();
    }

    virtual void callQmlContextProperty(IR::Expr *base, IR::Member::MemberKind /*kind*/, int propertyIndex, IR::ExprList *args, IR::Expr *result)
    {
        Q_UNUSED(propertyIndex)

        addDef(result);
        addUses(base->asTemp(), Use::CouldHaveRegister);
        addUses(args, Use::CouldHaveRegister);
        addCall();
    }

    virtual void callProperty(IR::Expr *base, const QString &name, IR::ExprList *args,
                              IR::Expr *result)
    {
        Q_UNUSED(name)

        addDef(result);
        addUses(base->asTemp(), Use::CouldHaveRegister);
        addUses(args, Use::CouldHaveRegister);
        addCall();
    }

    virtual void callSubscript(IR::Expr *base, IR::Expr *index, IR::ExprList *args,
                               IR::Expr *result)
    {
        addDef(result);
        addUses(base->asTemp(), Use::CouldHaveRegister);
        addUses(index->asTemp(), Use::CouldHaveRegister);
        addUses(args, Use::CouldHaveRegister);
        addCall();
    }

    virtual void convertType(IR::Expr *source, IR::Expr *target)
    {
        addDef(target);

        bool needsCall = true;
        Use::RegisterFlag sourceReg = Use::CouldHaveRegister;

        switch (target->type) {
        case DoubleType:
            switch (source->type) {
            case UInt32Type:
            case SInt32Type:
            case NullType:
            case UndefinedType:
            case BoolType:
                needsCall = false;
                break;
            default:
                break;
            }
            break;
        case BoolType:
            switch (source->type) {
            case UInt32Type:
                sourceReg = Use::MustHaveRegister;
                needsCall = false;
                break;
            case DoubleType:
            case UndefinedType:
            case NullType:
            case SInt32Type:
                needsCall = false;
                break;
            default:
                break;
            }
            break;
        case SInt32Type:
            switch (source->type) {
            case UInt32Type:
            case NullType:
            case UndefinedType:
            case BoolType:
                needsCall = false;
            default:
                break;
            }
            break;
        case UInt32Type:
            switch (source->type) {
            case SInt32Type:
            case NullType:
            case UndefinedType:
            case BoolType:
                needsCall = false;
            default:
                break;
            }
            break;
        default:
            break;
        }

        Temp *sourceTemp = source->asTemp();
        if (sourceTemp)
            addUses(sourceTemp, sourceReg);

        if (needsCall)
            addCall();
        else if (target->asTemp())
            addHint(target->asTemp(), sourceTemp);
    }

    virtual void constructActivationProperty(IR::Name *, IR::ExprList *args, IR::Expr *result)
    {
        addDef(result);
        addUses(args, Use::CouldHaveRegister);
        addCall();
    }

    virtual void constructProperty(IR::Expr *base, const QString &, IR::ExprList *args, IR::Expr *result)
    {
        addDef(result);
        addUses(base, Use::CouldHaveRegister);
        addUses(args, Use::CouldHaveRegister);
        addCall();
    }

    virtual void constructValue(IR::Expr *value, IR::ExprList *args, IR::Expr *result)
    {
        addDef(result);
        addUses(value, Use::CouldHaveRegister);
        addUses(args, Use::CouldHaveRegister);
        addCall();
    }

    virtual void loadThisObject(IR::Expr *temp)
    {
        addDef(temp);
    }

    virtual void loadQmlContext(IR::Expr *temp)
    {
        addDef(temp);
        addCall();
    }

    virtual void loadQmlImportedScripts(IR::Expr *temp)
    {
        addDef(temp);
        addCall();
    }

    virtual void loadQmlSingleton(const QString &/*name*/, Expr *temp)
    {
        Q_UNUSED(temp);

        addDef(temp);
        addCall();
    }

    virtual void loadConst(IR::Const *sourceConst, Expr *targetTemp)
    {
        Q_UNUSED(sourceConst);

        addDef(targetTemp);
    }

    virtual void loadString(const QString &str, Expr *targetTemp)
    {
        Q_UNUSED(str);

        addDef(targetTemp);
    }

    virtual void loadRegexp(IR::RegExp *sourceRegexp, Expr *targetTemp)
    {
        Q_UNUSED(sourceRegexp);

        addDef(targetTemp);
        addCall();
    }

    virtual void getActivationProperty(const IR::Name *, Expr *temp)
    {
        addDef(temp);
        addCall();
    }

    virtual void setActivationProperty(IR::Expr *source, const QString &)
    {
        addUses(source->asTemp(), Use::CouldHaveRegister);
        addCall();
    }

    virtual void initClosure(IR::Closure *closure, Expr *target)
    {
        Q_UNUSED(closure);

        addDef(target);
        addCall();
    }

    virtual void getProperty(IR::Expr *base, const QString &, Expr *target)
    {
        addDef(target);
        addUses(base->asTemp(), Use::CouldHaveRegister);
        addCall();
    }

    virtual void setProperty(IR::Expr *source, IR::Expr *targetBase, const QString &)
    {
        addUses(source->asTemp(), Use::CouldHaveRegister);
        addUses(targetBase->asTemp(), Use::CouldHaveRegister);
        addCall();
    }

    virtual void setQmlContextProperty(IR::Expr *source, IR::Expr *targetBase, IR::Member::MemberKind /*kind*/, int /*propertyIndex*/)
    {
        addUses(source->asTemp(), Use::CouldHaveRegister);
        addUses(targetBase->asTemp(), Use::CouldHaveRegister);
        addCall();
    }

    virtual void setQObjectProperty(IR::Expr *source, IR::Expr *targetBase, int /*propertyIndex*/)
    {
        addUses(source->asTemp(), Use::CouldHaveRegister);
        addUses(targetBase->asTemp(), Use::CouldHaveRegister);
        addCall();
    }

    virtual void getQmlContextProperty(IR::Expr *base, IR::Member::MemberKind /*kind*/, int /*index*/, bool /*captureRequired*/, IR::Expr *target)
    {
        addDef(target);
        addUses(base->asTemp(), Use::CouldHaveRegister);
        addCall();
    }

    virtual void getQObjectProperty(IR::Expr *base, int /*propertyIndex*/, bool /*captureRequired*/, bool /*isSingleton*/, int /*attachedPropertiesId*/, IR::Expr *target)
    {
        addDef(target);
        addUses(base->asTemp(), Use::CouldHaveRegister);
        addCall();
    }

    virtual void getElement(IR::Expr *base, IR::Expr *index, Expr *target)
    {
        addDef(target);
        addUses(base->asTemp(), Use::CouldHaveRegister);
        addUses(index->asTemp(), Use::CouldHaveRegister);
        addCall();
    }

    virtual void setElement(IR::Expr *source, IR::Expr *targetBase, IR::Expr *targetIndex)
    {
        addUses(source->asTemp(), Use::CouldHaveRegister);
        addUses(targetBase->asTemp(), Use::CouldHaveRegister);
        addUses(targetIndex->asTemp(), Use::CouldHaveRegister);
        addCall();
    }

    virtual void copyValue(Expr *source, Expr *target)
    {
        addDef(target);
        Temp *sourceTemp = source->asTemp();
        if (!sourceTemp)
            return;
        addUses(sourceTemp, Use::CouldHaveRegister);
        Temp *targetTemp = target->asTemp();
        if (targetTemp)
            addHint(targetTemp, sourceTemp);
    }

    virtual void swapValues(Expr *, Expr *)
    {
        // Inserted by the register allocator, so it cannot occur here.
        Q_UNREACHABLE();
    }

    virtual void unop(AluOp oper, Expr *source, Expr *target)
    {
        addDef(target);

        bool needsCall = true;
        if (oper == OpNot && source->type == IR::BoolType && target->type == IR::BoolType)
            needsCall = false;

#if 0 // TODO: change masm to generate code
        switch (oper) {
        case OpIfTrue:
        case OpNot:
        case OpUMinus:
        case OpUPlus:
        case OpCompl:
            needsCall = sourceTemp->type & ~NumberType && sourceTemp->type != BoolType;
            break;

        case OpIncrement:
        case OpDecrement:
        default:
            Q_UNREACHABLE();
        }
#endif

        IR::Temp *sourceTemp = source->asTemp();
        if (needsCall) {
            if (sourceTemp)
                addUses(sourceTemp, Use::CouldHaveRegister);
            addCall();
        } else {
            if (sourceTemp)
                addUses(sourceTemp, Use::MustHaveRegister);
        }
    }

    virtual void binop(AluOp oper, Expr *leftSource, Expr *rightSource, Expr *target)
    {
        bool needsCall = true;

        if (oper == OpStrictEqual || oper == OpStrictNotEqual) {
            bool noCall = leftSource->type == NullType || rightSource->type == NullType
                    || leftSource->type == UndefinedType || rightSource->type == UndefinedType
                    || leftSource->type == BoolType || rightSource->type == BoolType;
            needsCall = !noCall;
        } else if (leftSource->type == DoubleType && rightSource->type == DoubleType) {
            if (oper == OpMul || oper == OpAdd || oper == OpDiv || oper == OpSub
                    || (oper >= OpGt && oper <= OpStrictNotEqual)) {
                needsCall = false;
            }
        } else if (oper == OpBitAnd || oper == OpBitOr || oper == OpBitXor || oper == OpLShift || oper == OpRShift || oper == OpURShift) {
            needsCall = false;
        } else if (oper == OpAdd || oper == OpMul || oper == OpSub
                   || (oper >= OpGt && oper <= OpStrictNotEqual)) {
            if (leftSource->type == SInt32Type && rightSource->type == SInt32Type)
                needsCall = false;
        }

        addDef(target);

        if (needsCall) {
            addUses(leftSource->asTemp(), Use::CouldHaveRegister);
            addUses(rightSource->asTemp(), Use::CouldHaveRegister);
            addCall();
        } else {
            addUses(leftSource->asTemp(), Use::MustHaveRegister);
            addHint(target, leftSource->asTemp());
            addHint(target, rightSource->asTemp());

#if CPU(X86) || CPU(X86_64)
            switch (oper) {
                // The rhs operand can be a memory address
            case OpAdd:
            case OpSub:
            case OpMul:
            case OpDiv:
#if CPU(X86_64)
                if (leftSource->type == DoubleType || rightSource->type == DoubleType) {
                    // well, on 64bit the doubles are mangled, so they must first be loaded in a register and demangled, so...:
                    addUses(rightSource->asTemp(), Use::MustHaveRegister);
                    break;
                }
#endif
            case OpBitAnd:
            case OpBitOr:
            case OpBitXor:
                addUses(rightSource->asTemp(), Use::CouldHaveRegister);
                break;

            default:
                addUses(rightSource->asTemp(), Use::MustHaveRegister);
                break;
            }
#else
            addUses(rightSource->asTemp(), Use::MustHaveRegister);
#endif
        }
    }

    virtual void visitJump(IR::Jump *) {}
    virtual void visitCJump(IR::CJump *s)
    {
        if (Temp *t = s->cond->asTemp()) {
#if 0 // TODO: change masm to generate code
            addUses(t, Use::MustHaveRegister);
#else
            addUses(t, Use::CouldHaveRegister);
            addCall();
#endif
        } else if (Binop *b = s->cond->asBinop()) {
            binop(b->op, b->left, b->right, 0);
        } else if (s->cond->asConst()) {
            // TODO: SSA optimization for constant condition evaluation should remove this.
            // See also visitCJump() in masm.
            addCall();
        } else {
            Q_UNREACHABLE();
        }
    }

    virtual void visitRet(IR::Ret *s)
    { addUses(s->expr->asTemp(), Use::CouldHaveRegister); }

    virtual void visitPhi(IR::Phi *s)
    {
        addDef(s->targetTemp, true);
        for (int i = 0, ei = s->incoming.size(); i < ei; ++i) {
            Expr *e = s->incoming.at(i);
            if (Temp *t = e->asTemp()) {
                // The actual use of an incoming value in a phi node is right before the terminator
                // of the other side of the incoming edge.
                const int usePos = _lifeTimeIntervals->positionForStatement(_currentBB->in.at(i)->terminator()) - 1;
                addUses(t, Use::CouldHaveRegister, usePos);
                addHint(s->targetTemp, t);
                addHint(t, s->targetTemp);
            }
        }
    }

protected:
    virtual void callBuiltin(IR::Call *c, IR::Expr *result)
    {
        addDef(result);
        addUses(c->base, Use::CouldHaveRegister);
        addUses(c->args, Use::CouldHaveRegister);
        addCall();
    }

private:
    void addDef(Expr *e, bool isPhiTarget = false)
    {
        if (!e)
            return;
        Temp *t = e->asTemp();
        if (!t)
            return;
        if (!t || t->kind != Temp::VirtualRegister)
            return;
        Q_ASSERT(!_defs[t->index].isValid());
        bool canHaveReg = true;
        switch (t->type) {
        case QObjectType:
        case VarType:
        case StringType:
        case UndefinedType:
        case NullType:
            canHaveReg = false;
            break;
        default:
            break;
        }

        _defs[t->index] = Def(canHaveReg, isPhiTarget);
    }

    void addUses(Expr *e, Use::RegisterFlag flag)
    {
        const int usePos = usePosition(_currentStmt);
        addUses(e, flag, usePos);
    }

    void addUses(Expr *e, Use::RegisterFlag flag, int usePos)
    {
        Q_ASSERT(usePos > 0);
        if (!e)
            return;
        Temp *t = e->asTemp();
        if (!t)
            return;
        if (t && t->kind == Temp::VirtualRegister)
            _uses[t->index].push_back(Use(usePos, flag));
    }

    void addUses(ExprList *l, Use::RegisterFlag flag)
    {
        for (ExprList *it = l; it; it = it->next)
            addUses(it->expr, flag);
    }

    void addCall()
    {
        _calls.push_back(usePosition(_currentStmt));
    }

    void addHint(Expr *hinted, Temp *hint1, Temp *hint2 = 0)
    {
        if (hinted)
            if (Temp *hintedTemp = hinted->asTemp())
                addHint(hintedTemp, hint1, hint2);
    }

    void addHint(Temp *hinted, Temp *hint1, Temp *hint2 = 0)
    {
        if (!hinted || hinted->kind != Temp::VirtualRegister)
            return;
        if (hint1 && hint1->kind == Temp::VirtualRegister && hinted->type == hint1->type)
            addHint(*hinted, Temp::VirtualRegister, hint1->index);
        if (hint2 && hint2->kind == Temp::VirtualRegister && hinted->type == hint2->type)
            addHint(*hinted, Temp::VirtualRegister, hint2->index);
    }
};

} // JIT namespace
} // QV4 namespace
QT_END_NAMESPACE

QT_USE_NAMESPACE

using namespace QT_PREPEND_NAMESPACE(QV4::JIT);
using namespace QT_PREPEND_NAMESPACE(QV4::IR);
using namespace QT_PREPEND_NAMESPACE(QV4);

namespace {
class ResolutionPhase
{
    Q_DISABLE_COPY(ResolutionPhase)

    LifeTimeIntervals::Ptr _intervals;
    QVector<LifeTimeInterval *> _unprocessedReverseOrder;
    IR::Function *_function;
    const std::vector<int> &_assignedSpillSlots;
    std::vector<const LifeTimeInterval *> _liveIntervals;
    const QVector<const RegisterInfo *> &_intRegs;
    const QVector<const RegisterInfo *> &_fpRegs;

    Stmt *_currentStmt;
    std::vector<Move *> _loads;
    std::vector<Move *> _stores;

    std::vector<std::vector<const LifeTimeInterval *> > _liveAtStart;
    std::vector<std::vector<const LifeTimeInterval *> > _liveAtEnd;

public:
    ResolutionPhase(QVector<LifeTimeInterval *> &&unprocessedReversedOrder,
                    const LifeTimeIntervals::Ptr &intervals,
                    IR::Function *function,
                    const std::vector<int> &assignedSpillSlots,
                    const QVector<const RegisterInfo *> &intRegs,
                    const QVector<const RegisterInfo *> &fpRegs)
        : _intervals(intervals)
        , _unprocessedReverseOrder(unprocessedReversedOrder)
        , _function(function)
        , _assignedSpillSlots(assignedSpillSlots)
        , _intRegs(intRegs)
        , _fpRegs(fpRegs)
        , _currentStmt(0)
    {
        _liveAtStart.resize(function->basicBlockCount());
        _liveAtEnd.resize(function->basicBlockCount());
    }

    void run() {
        renumber();
        if (DebugRegAlloc) {
            QBuffer buf;
            buf.open(QIODevice::WriteOnly);
            QTextStream qout(&buf);
            IRPrinterWithPositions(&qout, _intervals).print(_function);
            qDebug("%s", buf.data().constData());
        }
        resolve();
    }

private:
    int defPosition(Stmt *s) const
    {
        return usePosition(s) + 1;
    }

    int usePosition(Stmt *s) const
    {
        return _intervals->positionForStatement(s);
    }

    void renumber()
    {
        QVector<Stmt *> newStatements;

        for (BasicBlock *bb : _function->basicBlocks()) {
            _currentStmt = 0;

            QVector<Stmt *> statements = bb->statements();
            newStatements.reserve(bb->statements().size() + 7);
            newStatements.erase(newStatements.begin(), newStatements.end());

            cleanOldIntervals(_intervals->startPosition(bb));
            addNewIntervals(_intervals->startPosition(bb));
            _liveAtStart[bb->index()] = _liveIntervals;

            for (int i = 0, ei = statements.size(); i != ei; ++i) {
                _currentStmt = statements.at(i);
                _loads.clear();
                _stores.clear();
                if (_currentStmt->asTerminator())
                    addNewIntervals(usePosition(_currentStmt));
                else
                    addNewIntervals(defPosition(_currentStmt));
                visit(_currentStmt);
                for (Move *load : _loads)
                    newStatements.append(load);
                if (_currentStmt->asPhi())
                    newStatements.prepend(_currentStmt);
                else
                    newStatements.append(_currentStmt);
                for (Move *store : _stores)
                    newStatements.append(store);
            }

            cleanOldIntervals(_intervals->endPosition(bb));
            _liveAtEnd[bb->index()] = _liveIntervals;

            if (DebugRegAlloc) {
                QBuffer buf;
                buf.open(QIODevice::WriteOnly);
                QTextStream os(&buf);
                os << "Intervals live at the start of L" << bb->index() << ":" << endl;
                if (_liveAtStart[bb->index()].empty())
                    os << "\t(none)" << endl;
                for (const LifeTimeInterval *i : _liveAtStart.at(bb->index())) {
                    os << "\t";
                    i->dump(os);
                    os << endl;
                }
                os << "Intervals live at the end of L" << bb->index() << ":" << endl;
                if (_liveAtEnd[bb->index()].empty())
                    os << "\t(none)" << endl;
                for (const LifeTimeInterval *i : _liveAtEnd.at(bb->index())) {
                    os << "\t";
                    i->dump(os);
                    os << endl;
                }
                qDebug("%s", buf.data().constData());
            }

            bb->setStatements(newStatements);
        }

    }

    const LifeTimeInterval *findLiveInterval(Temp *t) const
    {
        for (const LifeTimeInterval *lti : _liveIntervals) {
            if (lti->temp() == *t)
                return lti;
        }

        return nullptr;
    }

    void maybeGenerateSpill(Temp *t)
    {
        const LifeTimeInterval *i = findLiveInterval(t);
        if (i->reg() == LifeTimeInterval::InvalidRegister)
            return;

        const RegisterInfo *pReg = platformRegister(*i);
        Q_ASSERT(pReg);
        int spillSlot = _assignedSpillSlots[i->temp().index];
        if (spillSlot != RegisterAllocator::InvalidSpillSlot)
            _stores.push_back(generateSpill(spillSlot, i->temp().type, pReg->reg<int>()));
    }

    void addNewIntervals(int position)
    {
        if (position == Stmt::InvalidId)
            return;

        while (!_unprocessedReverseOrder.isEmpty()) {
            const LifeTimeInterval *i = _unprocessedReverseOrder.constLast();
            if (i->start() > position)
                break;

            Q_ASSERT(!i->isFixedInterval());
            auto it = _liveIntervals.begin();
            for (; it != _liveIntervals.end(); ++it) {
                if ((*it)->temp() == i->temp()) {
                    *it = i;
                    break;
                }
            }
            if (it == _liveIntervals.end())
                _liveIntervals.push_back(i);
//            qDebug() << "-- Activating interval for temp" << i->temp().index;

            _unprocessedReverseOrder.removeLast();
        }
    }

    void cleanOldIntervals(int position)
    {
        for (size_t it = 0; it != _liveIntervals.size(); ) {
            const LifeTimeInterval *lti = _liveIntervals.at(it);
            if (lti->end() < position || lti->isFixedInterval())
                _liveIntervals.erase(_liveIntervals.begin() + it);
            else
                ++it;
        }
    }

    void resolve()
    {
        for (BasicBlock *bb : _function->basicBlocks()) {
            for (BasicBlock *bbOut : bb->out)
                resolveEdge(bb, bbOut);
        }
    }

    Phi *findDefPhi(const Temp &t, BasicBlock *bb) const
    {
        for (Stmt *s : bb->statements()) {
            Phi *phi = s->asPhi();
            if (!phi)
                return 0;

            if (*phi->targetTemp == t)
                return phi;
        }

        Q_UNREACHABLE();
    }

    void resolveEdge(BasicBlock *predecessor, BasicBlock *successor)
    {
        if (DebugRegAlloc) {
            qDebug() << "Resolving edge" << predecessor->index() << "->" << successor->index();
            QBuffer buf;
            buf.open(QIODevice::WriteOnly);
            QTextStream qout(&buf);
            IRPrinterWithPositions printer(&qout, _intervals);
            printer.print(predecessor);
            printer.print(successor);
            qDebug("%s", buf.data().constData());
        }

        MoveMapping mapping;

        const int predecessorEnd = _intervals->endPosition(predecessor);
        Q_ASSERT(predecessorEnd > 0);

        int successorStart = _intervals->startPosition(successor);
        Q_ASSERT(successorStart > 0);

        for (const LifeTimeInterval *it : _liveAtStart.at(successor->index())) {
            bool isPhiTarget = false;
            Expr *moveFrom = 0;

            if (it->start() == successorStart) {
                if (Phi *phi = findDefPhi(it->temp(), successor)) {
                    isPhiTarget = true;
                    Expr *opd = phi->incoming[successor->in.indexOf(predecessor)];
                    if (opd->asConst()) {
                        moveFrom = opd;
                    } else {
                        Temp *t = opd->asTemp();
                        Q_ASSERT(t);

                        for (const LifeTimeInterval *it2 : _liveAtEnd.at(predecessor->index())) {
                            if (it2->temp() == *t
                                    && it2->reg() != LifeTimeInterval::InvalidRegister
                                    && it2->covers(predecessorEnd)) {
                                moveFrom = createPhysicalRegister(it2, t->type);
                                break;
                            }
                        }
                        if (!moveFrom)
                            moveFrom = createTemp(Temp::StackSlot,
                                                  _assignedSpillSlots[t->index],
                                    t->type);
                    }
                }
            } else {
                for (const LifeTimeInterval *predIt : _liveAtEnd.at(predecessor->index())) {
                    if (predIt->temp() == it->temp()) {
                        if (predIt->reg() != LifeTimeInterval::InvalidRegister
                                && predIt->covers(predecessorEnd)) {
                            moveFrom = createPhysicalRegister(predIt, predIt->temp().type);
                        } else {
                            int spillSlot = _assignedSpillSlots[predIt->temp().index];
                            if (spillSlot != -1)
                                moveFrom = createTemp(Temp::StackSlot, spillSlot, predIt->temp().type);
                        }
                        break;
                    }
                }
            }
            if (!moveFrom) {
#if !defined(QT_NO_DEBUG) && 0
                bool lifeTimeHole = false;
                if (it->ranges().first().start <= successorStart && it->ranges().last().end >= successorStart)
                    lifeTimeHole = !it->covers(successorStart);

                Q_ASSERT(!_info->isPhiTarget(it->temp()) || it->isSplitFromInterval() || lifeTimeHole);
                if (_info->def(it->temp()) != successorStart && !it->isSplitFromInterval()) {
                    const int successorEnd = successor->terminator()->id();
                    const int idx = successor->in.indexOf(predecessor);
                    foreach (const Use &use, _info->uses(it->temp())) {
                        if (use.pos == static_cast<unsigned>(successorStart)) {
                            // only check the current edge, not all other possible ones. This is
                            // important for phi nodes: they have uses that are only valid when
                            // coming in over a specific edge.
                            foreach (Stmt *s, successor->statements()) {
                                if (Phi *phi = s->asPhi()) {
                                    Q_ASSERT(it->temp().index != phi->targetTemp->index);
                                    Q_ASSERT(phi->d->incoming[idx]->asTemp() == 0
                                             || it->temp().index != phi->d->incoming[idx]->asTemp()->index);
                                } else {
                                    // TODO: check that the first non-phi statement does not use
                                    // the temp.
                                    break;
                                }
                            }
                        } else {
                            Q_ASSERT(use.pos < static_cast<unsigned>(successorStart) ||
                                     use.pos > static_cast<unsigned>(successorEnd));
                        }
                    }
                }
#endif

                continue;
            }

            Temp *moveTo;
            if (it->reg() == LifeTimeInterval::InvalidRegister || !it->covers(successorStart)) {
                if (!isPhiTarget) // if it->temp() is a phi target, skip it.
                    continue;
                const int spillSlot = _assignedSpillSlots[it->temp().index];
                if (spillSlot == RegisterAllocator::InvalidSpillSlot)
                    continue; // it has a life-time hole here.
                moveTo = createTemp(Temp::StackSlot, spillSlot, it->temp().type);
            } else {
                moveTo = createPhysicalRegister(it, it->temp().type);
            }

            // add move to mapping
            mapping.add(moveFrom, moveTo);
        }

        mapping.order();
        if (DebugRegAlloc)
            mapping.dump();

        bool insertIntoPredecessor = successor->in.size() > 1;
        mapping.insertMoves(insertIntoPredecessor ? predecessor : successor, _function,
                            insertIntoPredecessor);

        if (DebugRegAlloc) {
            qDebug() << ".. done, result:";
            QBuffer buf;
            buf.open(QIODevice::WriteOnly);
            QTextStream qout(&buf);
            IRPrinterWithPositions printer(&qout, _intervals);
            printer.print(predecessor);
            printer.print(successor);
            qDebug("%s", buf.data().constData());
        }
    }

    Temp *createTemp(Temp::Kind kind, int index, Type type) const
    {
        Q_ASSERT(index >= 0);
        Temp *t = _function->New<Temp>();
        t->init(kind, index);
        t->type = type;
        return t;
    }

    Temp *createPhysicalRegister(const LifeTimeInterval *i, Type type) const
    {
        const RegisterInfo *ri = platformRegister(*i);
        Q_ASSERT(ri);
        return createTemp(Temp::PhysicalRegister, ri->reg<int>(), type);
    }

    const RegisterInfo *platformRegister(const LifeTimeInterval &i) const
    {
        if (i.isFP())
            return _fpRegs.value(i.reg(), 0);
        else
            return _intRegs.value(i.reg(), 0);
    }

    Move *generateSpill(int spillSlot, Type type, int pReg) const
    {
        Q_ASSERT(spillSlot >= 0);

        Move *store = _function->NewStmt<Move>();
        store->init(createTemp(Temp::StackSlot, spillSlot, type),
                    createTemp(Temp::PhysicalRegister, pReg, type));
        return store;
    }

    Move *generateUnspill(const Temp &t, int pReg) const
    {
        Q_ASSERT(pReg >= 0);
        int spillSlot = _assignedSpillSlots[t.index];
        Q_ASSERT(spillSlot != -1);
        Move *load = _function->NewStmt<Move>();
        load->init(createTemp(Temp::PhysicalRegister, pReg, t.type),
                   createTemp(Temp::StackSlot, spillSlot, t.type));
        return load;
    }

private:
    void visit(Expr *e)
    {
        switch (e->exprKind) {
        case Expr::TempExpr:
            visitTemp(e->asTemp());
            break;
        default:
            EXPR_VISIT_ALL_KINDS(e);
            break;
        }
    }

    void visitTemp(Temp *t)
    {
        if (t->kind != Temp::VirtualRegister)
            return;

        const LifeTimeInterval *i = findLiveInterval(t);
        Q_ASSERT(i->isValid());

        if (_currentStmt != 0 && i->start() == usePosition(_currentStmt)) {
            Q_ASSERT(i->isSplitFromInterval());
            const RegisterInfo *pReg = platformRegister(*i);
            Q_ASSERT(pReg);
            _loads.push_back(generateUnspill(i->temp(), pReg->reg<int>()));
        }

        if (i->reg() != LifeTimeInterval::InvalidRegister &&
                (i->covers(defPosition(_currentStmt)) ||
                 i->covers(usePosition(_currentStmt)))) {
            const RegisterInfo *pReg = platformRegister(*i);
            Q_ASSERT(pReg);
            t->kind = Temp::PhysicalRegister;
            t->index = pReg->reg<unsigned>();
        } else {
            int stackSlot = _assignedSpillSlots[t->index];
            Q_ASSERT(stackSlot >= 0);
            t->kind = Temp::StackSlot;
            t->index = stackSlot;
        }
    }

    void visit(Stmt *s)
    {
        switch (s->stmtKind) {
        case Stmt::MoveStmt: {
            auto m = s->asMove();
            if (Temp *t = m->target->asTemp())
                maybeGenerateSpill(t);

            visit(m->source);
            visit(m->target);
        } break;
        case Stmt::PhiStmt: {
            auto p = s->asPhi();
            maybeGenerateSpill(p->targetTemp);
        } break;
        default:
            STMT_VISIT_ALL_KINDS(s);
            break;
        }
    }
};
} // anonymous namespace

RegisterAllocator::RegisterAllocator(const QV4::JIT::RegisterInformation &registerInformation)
    : _registerInformation(registerInformation)
{
    for (int i = 0, ei = registerInformation.size(); i != ei; ++i) {
        const RegisterInfo &regInfo = registerInformation.at(i);
        if (regInfo.useForRegAlloc()) {
            if (regInfo.isRegularRegister())
                _normalRegisters.append(&regInfo);
            else
                _fpRegisters.append(&regInfo);
        }
    }
    Q_ASSERT(_normalRegisters.size() >= 2);
    Q_ASSERT(_fpRegisters.size() >= 2);
    _active.reserve((_normalRegisters.size() + _fpRegisters.size()) * 2);
    _inactive.reserve(_active.size());

    _regularRegsInUse.resize(_normalRegisters.size());
    _fpRegsInUse.resize(_fpRegisters.size());
}

RegisterAllocator::~RegisterAllocator()
{
}

void RegisterAllocator::run(IR::Function *function, const Optimizer &opt)
{
    _lastAssignedRegister.assign(function->tempCount, LifeTimeInterval::InvalidRegister);
    _assignedSpillSlots.assign(function->tempCount, InvalidSpillSlot);
    _activeSpillSlots.resize(function->tempCount);

    if (DebugRegAlloc)
        qDebug() << "*** Running regalloc for function" << (function->name ? qPrintable(*function->name) : "NO NAME") << "***";

    _lifeTimeIntervals = opt.lifeTimeIntervals();

    _unhandled = _lifeTimeIntervals->intervals();
    _handled.reserve(_unhandled.size());

    _info.reset(new RegAllocInfo);
    _info->collect(function, _lifeTimeIntervals);

    if (DebugRegAlloc) {
        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        QTextStream qout(&buf);
        qout << "Ranges:" << endl;
        QVector<LifeTimeInterval *> intervals = _unhandled;
        std::reverse(intervals.begin(), intervals.end());
        for (const LifeTimeInterval *r : qAsConst(intervals)) {
            r->dump(qout);
            qout << endl;
        }
        qDebug("%s", buf.data().constData());
        _info->dump();

        qDebug() << "*** Before register allocation:";
        buf.setData(QByteArray());
        IRPrinterWithPositions(&qout, _lifeTimeIntervals).print(function);
        qDebug("%s", buf.data().constData());
    }
    prepareRanges();

    linearScan();

    if (DebugRegAlloc)
        dump(function);

    // sort the ranges in reverse order, so the ResolutionPhase can take from the end (and thereby
    // prevent the copy overhead that taking from the beginning would give).
    std::sort(_handled.begin(), _handled.end(),
              [](const LifeTimeInterval *r1, const LifeTimeInterval *r2) -> bool {
        return LifeTimeInterval::lessThan(r2, r1);
    });
    ResolutionPhase(std::move(_handled), _lifeTimeIntervals, function, _assignedSpillSlots, _normalRegisters, _fpRegisters).run();

    function->tempCount = *std::max_element(_assignedSpillSlots.begin(), _assignedSpillSlots.end()) + 1;

    if (DebugRegAlloc)
        qDebug() << "*** Finished regalloc , result:";

    static const bool showCode = qEnvironmentVariableIsSet("QV4_SHOW_IR");
    if (showCode) {
        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        QTextStream qout(&buf);
        IRPrinterWithRegisters(&qout, _lifeTimeIntervals, _registerInformation).print(function);
        qDebug("%s", buf.data().constData());
    }
}

RegisterInformation RegisterAllocator::usedRegisters() const
{
    RegisterInformation regInfo;

    for (int i = 0, ei = _normalRegisters.size(); i != ei; ++i) {
        if (_regularRegsInUse.testBit(i))
            regInfo.append(*_normalRegisters.at(i));
    }

    for (int i = 0, ei = _fpRegisters.size(); i != ei; ++i) {
        if (_fpRegsInUse.testBit(i))
            regInfo.append(*_fpRegisters.at(i));
    }

    return regInfo;
}

void RegisterAllocator::markInUse(int reg, bool isFPReg)
{
    if (isFPReg)
        _fpRegsInUse.setBit(reg);
    else
        _regularRegsInUse.setBit(reg);
}

static inline LifeTimeInterval createFixedInterval(int rangeCount)
{
    LifeTimeInterval i(rangeCount);
    i.setReg(0);

    Temp t;
    t.init(Temp::PhysicalRegister, 0);
    t.type = IR::SInt32Type;
    i.setTemp(t);

    return i;
}

LifeTimeInterval *RegisterAllocator::cloneFixedInterval(int reg, bool isFP, const LifeTimeInterval &original)
{
    LifeTimeInterval *lti = new LifeTimeInterval(original);
    _lifeTimeIntervals->add(lti);
    lti->setReg(reg);
    lti->setFixedInterval(true);

    Temp t;
    t.init(Temp::PhysicalRegister, reg);
    t.type = isFP ? IR::DoubleType : IR::SInt32Type;
    lti->setTemp(t);

    return lti;
}

// Creates the intervals with fixed ranges. See [Wimmer2]. Note that this only applies to callee-
// saved registers.
void RegisterAllocator::prepareRanges()
{
    LifeTimeInterval ltiWithCalls = createFixedInterval(int(_info->calls().size()));
    for (int callPosition : _info->calls())
        ltiWithCalls.addRange(callPosition, callPosition);

    const int regCount = _normalRegisters.size();
    _fixedRegisterRanges.resize(regCount);
    for (int reg = 0; reg < regCount; ++reg) {
        if (_normalRegisters.at(reg)->isCallerSaved()) {
            LifeTimeInterval *lti = cloneFixedInterval(reg, false, ltiWithCalls);
            if (lti->isValid()) {
                _fixedRegisterRanges[reg] = lti;
                _active.append(lti);
            }
        }
    }

    const int fpRegCount = _fpRegisters.size();
    _fixedFPRegisterRanges.resize(fpRegCount);
    for (int fpReg = 0; fpReg < fpRegCount; ++fpReg) {
        if (_fpRegisters.at(fpReg)->isCallerSaved()) {
            LifeTimeInterval *lti = cloneFixedInterval(fpReg, true, ltiWithCalls);
            if (lti->isValid()) {
                _fixedFPRegisterRanges[fpReg] = lti;
                _active.append(lti);
            }
        }
    }
}

void RegisterAllocator::linearScan()
{
    while (!_unhandled.isEmpty()) {
        LifeTimeInterval *current = _unhandled.back();
        _unhandled.pop_back();
        const int position = current->start();

        // check for intervals in active that are handled or inactive
        for (int i = 0; i < _active.size(); ) {
            LifeTimeInterval *it = _active.at(i);
            if (it->end() < position) {
                if (!it->isFixedInterval())
                    _handled += it;
                _active.remove(i);
            } else if (!it->covers(position)) {
                _inactive += it;
                _active.remove(i);
            } else {
                ++i;
            }
        }

        // check for intervals in inactive that are handled or active
        for (int i = 0; i < _inactive.size(); ) {
            LifeTimeInterval *it = _inactive.at(i);
            if (it->end() < position) {
                if (!it->isFixedInterval())
                    _handled += it;
                _inactive.remove(i);
            } else if (it->covers(position)) {
                if (it->reg() != LifeTimeInterval::InvalidRegister) {
                    _active += it;
                    _inactive.remove(i);
                } else {
                    // although this interval is now active, it has no register allocated (always
                    // spilled), so leave it in inactive.
                    ++i;
                }
            } else {
                ++i;
            }
        }

        Q_ASSERT(!current->isFixedInterval());

#ifdef DEBUG_REGALLOC
        qDebug() << "** Position" << position;
#endif // DEBUG_REGALLOC

        if (_info->canHaveRegister(current->temp())) {
            tryAllocateFreeReg(*current);
            if (current->reg() == LifeTimeInterval::InvalidRegister)
                allocateBlockedReg(*current);
            if (current->reg() != LifeTimeInterval::InvalidRegister)
                _active += current;
        } else {
            assignSpillSlot(current->temp(), current->start(), current->end());
            _inactive += current;
            if (DebugRegAlloc)
                qDebug() << "*** allocating stack slot" << _assignedSpillSlots[current->temp().index]
                         << "for %" << current->temp().index << "as it cannot be loaded in a register";
        }
    }

    for (LifeTimeInterval *r : qAsConst(_active))
        if (!r->isFixedInterval())
            _handled.append(r);
    _active.clear();
    for (LifeTimeInterval *r : qAsConst(_inactive))
        if (!r->isFixedInterval())
            _handled.append(r);
    _inactive.clear();
}

static inline int indexOfRangeCoveringPosition(const LifeTimeInterval::Ranges &ranges, int position)
{
    for (int i = 0, ei = ranges.size(); i != ei; ++i) {
        if (position <= ranges[i].end)
            return i;
    }
    return -1;
}

static inline int intersectionPosition(const LifeTimeInterval::Range &one, const LifeTimeInterval::Range &two)
{
    if (one.covers(two.start))
        return two.start;
    if (two.covers(one.start))
        return one.start;
    return -1;
}

static inline bool isFP(const Temp &t)
{ return t.type == DoubleType; }

static inline bool candidateIsBetterFit(int bestSizeSoFar, int idealSize, int candidateSize)
{
    // If the candidateSize is larger than the current we take it only if the current size does not
    // yet fit for the whole interval.
    if (bestSizeSoFar < candidateSize && bestSizeSoFar < idealSize)
        return true;

    // If the candidateSize is smaller we only take it if it still fits the whole interval.
    if (bestSizeSoFar > candidateSize && candidateSize >= idealSize)
        return true;

    // Other wise: no luck.
    return false;
}

// Out of all available registers (with their next-uses), choose the one that fits the requested
// duration best. This can return a register that is not free for the whole interval, but that's
// fine: we just have to split the current interval.
static void longestAvailableReg(int *nextUses, int nextUseCount, int &reg, int &freeUntilPos_reg, int lastUse)
{
    reg = LifeTimeInterval::InvalidRegister;
    freeUntilPos_reg = 0;

    for (int candidate = 0, candidateEnd = nextUseCount; candidate != candidateEnd; ++candidate) {
        int fp = nextUses[candidate];
        if (candidateIsBetterFit(freeUntilPos_reg, lastUse, fp)) {
            reg = candidate;
            freeUntilPos_reg = fp;
        }
    }
}

#define CALLOC_ON_STACK(ty, ptr, sz, val) \
    Q_ASSERT(sz > 0); \
    ty *ptr = reinterpret_cast<ty *>(alloca(sizeof(ty) * (sz))); \
    for (ty *it = ptr, *eit = ptr + (sz); it != eit; ++it) \
        *it = val;

// Try to allocate a register that's currently free.
void RegisterAllocator::tryAllocateFreeReg(LifeTimeInterval &current)
{
    Q_ASSERT(!current.isFixedInterval());
    Q_ASSERT(current.reg() == LifeTimeInterval::InvalidRegister);

    const bool needsFPReg = isFP(current.temp());
    const int freeUntilPosCount = needsFPReg ? _fpRegisters.size() : _normalRegisters.size();
    CALLOC_ON_STACK(int, freeUntilPos, freeUntilPosCount, INT_MAX);

    for (Intervals::const_iterator i = _active.constBegin(), ei = _active.constEnd(); i != ei; ++i) {
        const LifeTimeInterval *it = *i;
        if (it->isFP() == needsFPReg)
            freeUntilPos[it->reg()] = 0; // mark register as unavailable
    }

    for (Intervals::const_iterator i = _inactive.constBegin(), ei = _inactive.constEnd(); i != ei; ++i) {
        const LifeTimeInterval *it = *i;
        if (it->isFP() != needsFPReg)
            continue; // different register type, so not applicable.
        if (it->reg() == LifeTimeInterval::InvalidRegister)
            continue; // this range does not block a register from being used, as it has no register assigned

        if (current.isSplitFromInterval() || it->isFixedInterval()) {
            const int intersectionPos = nextIntersection(current, *it);
            if (intersectionPos != -1)
                freeUntilPos[it->reg()] = qMin(freeUntilPos[it->reg()], intersectionPos);
        }
    }

    int reg = LifeTimeInterval::InvalidRegister;
    int freeUntilPos_reg = 0;

    const RegAllocInfo::Hints &hints = _info->hints(current.temp());
    for (RegAllocInfo::Hints::const_iterator i = hints.begin(), ei = hints.end(); i != ei; ++i) {
        const Temp &hint = *i;
        int candidate;
        if (hint.kind == Temp::PhysicalRegister)
            candidate = hint.index;
        else
            candidate = _lastAssignedRegister[hint.index];

        const int end = current.end();
        if (candidate == LifeTimeInterval::InvalidRegister)
            continue; // the candidate has no register assigned, so it cannot be (re-)used
        if (current.isFP() != isFP(hint))
            continue;  // different register type, so not applicable.

        const int fp = freeUntilPos[candidate];
        if (candidateIsBetterFit(freeUntilPos_reg, end, fp)) {
            reg = candidate;
            freeUntilPos_reg = fp;
        }
    }

    // None of the hinted registers could fit the interval, so try all registers next.
    if (reg == LifeTimeInterval::InvalidRegister)
        longestAvailableReg(freeUntilPos, freeUntilPosCount, reg, freeUntilPos_reg, current.end());

    if (freeUntilPos_reg == 0) {
        // no register available without spilling
        if (DebugRegAlloc)
            qDebug("*** no register available for %u", current.temp().index);
        return;
    } else if (current.end() < freeUntilPos_reg) {
        // register available for the whole interval
        if (DebugRegAlloc)
            qDebug() << "*** allocating register" << reg << "for the whole interval of %" << current.temp().index;
        current.setReg(reg);
        _lastAssignedRegister[current.temp().index] = reg;
        markInUse(reg, needsFPReg);
    } else {
        // register available for the first part of the interval

        // TODO: this is slightly inefficient in the following case:
        //   %1 = something
        //   some_call(%1)
        //   %2 = %1 + 1
        // Now %1 will get a register assigned, and will be spilled to the stack immediately. It
        // would be better to check if there are actually uses in the range before the split.

        current.setReg(reg);
        _lastAssignedRegister[current.temp().index] = reg;
        if (DebugRegAlloc)
            qDebug() << "*** allocating register" << reg << "for the first part of interval of %" << current.temp().index;
        split(current, freeUntilPos_reg, true);
        markInUse(reg, needsFPReg);
    }
}

// This gets called when all registers are currently in use.
void RegisterAllocator::allocateBlockedReg(LifeTimeInterval &current)
{
    Q_ASSERT(!current.isFixedInterval());
    Q_ASSERT(current.reg() == LifeTimeInterval::InvalidRegister);
    const int position = current.start();

    const bool isPhiTarget = _info->isPhiTarget(current.temp());
    if (isPhiTarget && !current.isSplitFromInterval()) {
        // Special case: storing to a phi-node's target will result in a single move. So, if we
        // would spill another interval to the stack (that's 1 store), and then do the move for the
        // phi target (at least 1 move or a load), that would result in 2 instructions. Instead, we
        // force the phi-node's target to go to the stack immediately, which is always a single
        // store.
        split(current, position + 1, true);
        _inactive.append(&current);
        return;
    }

    const bool needsFPReg = isFP(current.temp());
    const int nextUsePosCount = needsFPReg ? _fpRegisters.size() : _normalRegisters.size();
    CALLOC_ON_STACK(int, nextUsePos, nextUsePosCount, INT_MAX);
    QVector<LifeTimeInterval *> nextUseRangeForReg(nextUsePosCount, 0);

    for (Intervals::const_iterator i = _active.constBegin(), ei = _active.constEnd(); i != ei; ++i) {
        LifeTimeInterval &it = **i;
        if (it.isFP() != needsFPReg)
            continue; // different register type, so not applicable.

        const int nu = it.isFixedInterval() ? 0 : nextUse(it.temp(), current.start());
        if (nu == position) {
            nextUsePos[it.reg()] = 0;
        } else if (nu != -1 && nu < nextUsePos[it.reg()]) {
            nextUsePos[it.reg()] = nu;
            nextUseRangeForReg[it.reg()] = &it;
        } else if (nu == -1 && nextUsePos[it.reg()] == INT_MAX) {
            // in a loop, the range can be active, but the result might only be used before the
            // current position (e.g. the induction variable being used in the phi node in the loop
            // header). So, we can use this register, but we need to remember to split the interval
            // in order to have the edge-resolving generate a load at the edge going back to the
            // loop header.
            nextUseRangeForReg[it.reg()] = &it;
        }
    }

    for (Intervals::const_iterator i = _inactive.constBegin(), ei = _inactive.constEnd(); i != ei; ++i) {
        LifeTimeInterval &it = **i;
        if (it.isFP() != needsFPReg)
            continue; // different register type, so not applicable.
        if (it.reg() == LifeTimeInterval::InvalidRegister)
            continue; // this range does not block a register from being used, as it has no register assigned

        if (current.isSplitFromInterval() || it.isFixedInterval()) {
            if (nextIntersection(current, it) != -1) {
                const int nu = nextUse(it.temp(), current.start());
                if (nu != -1 && nu < nextUsePos[it.reg()]) {
                    nextUsePos[it.reg()] = nu;
                    nextUseRangeForReg[it.reg()] = &it;
                }
            }
        }
    }

    int reg, nextUsePos_reg;
    longestAvailableReg(nextUsePos, nextUsePosCount, reg, nextUsePos_reg, current.end());

    Q_ASSERT(current.start() <= nextUsePos_reg);

    // spill interval that currently block reg
    if (DebugRegAlloc) {
        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        QTextStream out(&buf);
        out << "*** spilling intervals that block reg " <<reg<< " for interval ";
        current.dump(out);
        qDebug("%s", buf.data().constData());
    }
    current.setReg(reg);
    _lastAssignedRegister[current.temp().index] = reg;
    LifeTimeInterval *nextUse = nextUseRangeForReg[reg];
    Q_ASSERT(nextUse);
    Q_ASSERT(!nextUse->isFixedInterval());

    split(*nextUse, position, /*skipOptionalRegisterUses =*/ true);

    // We might have chosen a register that is used by a range that has a hole in its life time.
    // If that's the case, check if the current interval completely fits in the hole. Or rephrased:
    // check if the current interval will use the register after that hole ends (so that range, and
    // if so, split that interval so that it gets a new register assigned when it needs one.
    splitInactiveAtEndOfLifetimeHole(reg, needsFPReg, position);

    // make sure that current does not intersect with the fixed interval for reg
    const LifeTimeInterval *fixedRegRange = needsFPReg ? _fixedFPRegisterRanges.at(reg)
                                                       : _fixedRegisterRanges.at(reg);
    if (fixedRegRange) {
        int ni = nextIntersection(current, *fixedRegRange);
        if (ni != -1) {
            if (DebugRegAlloc) {
                qDebug("***-- current range intersects with a fixed reg use at %d, so splitting it.", ni);
            }
            // current does overlap with a fixed interval, so split current before that intersection.
            split(current, ni, true);
        }
    }
}

int RegisterAllocator::nextIntersection(const LifeTimeInterval &current,
                                        const LifeTimeInterval &another) const
{
    const LifeTimeInterval::Ranges &currentRanges = current.ranges();
    int currentIt = 0;

    const LifeTimeInterval::Ranges &anotherRanges = another.ranges();
    const int anotherItStart = indexOfRangeCoveringPosition(anotherRanges, current.start());
    if (anotherItStart == -1)
        return -1;

    for (int currentEnd = currentRanges.size(); currentIt < currentEnd; ++currentIt) {
        const LifeTimeInterval::Range currentRange = currentRanges.at(currentIt);
        for (int anotherIt = anotherItStart, anotherEnd = anotherRanges.size(); anotherIt < anotherEnd; ++anotherIt) {
            const LifeTimeInterval::Range anotherRange = anotherRanges.at(anotherIt);
            if (anotherRange.start > currentRange.end)
                break;
            int intersectPos = intersectionPosition(currentRange, anotherRange);
            if (intersectPos != -1)
                return intersectPos;
        }
    }

    return -1;
}

/// Find the first use after the start position for the given temp.
///
/// This is only called when all registers are in use, and when one of them has to be spilled to the
/// stack. So, uses where a register is optional can be ignored.
int RegisterAllocator::nextUse(const Temp &t, int startPosition) const
{
    typedef std::vector<Use>::const_iterator ConstIt;

    const std::vector<Use> &usePositions = _info->uses(t);
    const ConstIt cend = usePositions.end();
    for (ConstIt it = usePositions.begin(); it != cend; ++it) {
        if (it->mustHaveRegister()) {
            const int usePos = it->pos;
            if (usePos >= startPosition)
                return usePos;
        }
    }

    return -1;
}

static inline void insertReverseSorted(QVector<LifeTimeInterval *> &intervals, LifeTimeInterval *newInterval)
{
    newInterval->validate();
    for (int i = intervals.size(); i > 0;) {
        if (LifeTimeInterval::lessThan(newInterval, intervals.at(--i))) {
            intervals.insert(i + 1, newInterval);
            return;
        }
    }
    intervals.insert(0, newInterval);
}

void RegisterAllocator::split(LifeTimeInterval &current, int beforePosition,
                              bool skipOptionalRegisterUses)
{ // TODO: check if we can always skip the optional register uses
    Q_ASSERT(!current.isFixedInterval());

    if (DebugRegAlloc) {
        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        QTextStream out(&buf);
        out << "***** split request for range ";
        current.dump(out);
        out << " before position " << beforePosition
            << " and skipOptionalRegisterUses = " << skipOptionalRegisterUses << endl;
        qDebug("%s", buf.data().constData());
    }

    assignSpillSlot(current.temp(), current.start(), current.end());

    const int firstPosition = current.start();
    Q_ASSERT(beforePosition > firstPosition && "split before start");

    int lastUse = firstPosition;
    int nextUse = -1;
    const std::vector<Use> &usePositions = _info->uses(current.temp());
    for (size_t i = 0, ei = usePositions.size(); i != ei; ++i) {
        const Use &usePosition = usePositions.at(i);
        const int usePos = usePosition.pos;
        if (lastUse < usePos && usePos < beforePosition) {
            lastUse = usePos;
        } else if (usePos >= beforePosition) {
            if (!skipOptionalRegisterUses || usePosition.mustHaveRegister()) {
                nextUse = usePos;
                break;
            }
        }
    }
    Q_ASSERT(lastUse != -1);
    Q_ASSERT(lastUse < beforePosition);

    LifeTimeInterval newInterval = current.split(lastUse, nextUse);
    if (DebugRegAlloc) {
        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        QTextStream out(&buf);
        out << "***** last use = " << lastUse << ", nextUse = " << nextUse << endl;
        out << "***** new interval: ";
        newInterval.dump(out);
        out << endl;
        out << "***** preceding interval: ";
        current.dump(out);
        out << endl;
        qDebug("%s", buf.data().constData());
    }
    if (newInterval.isValid()) {
        if (current.reg() != LifeTimeInterval::InvalidRegister)
            _info->addHint(current.temp(), current.reg());
        newInterval.setReg(LifeTimeInterval::InvalidRegister);
        LifeTimeInterval *newIntervalPtr = new LifeTimeInterval(newInterval);
        _lifeTimeIntervals->add(newIntervalPtr);
        insertReverseSorted(_unhandled, newIntervalPtr);
    }
}

void RegisterAllocator::splitInactiveAtEndOfLifetimeHole(int reg, bool isFPReg, int position)
{
    for (int i = 0, ei = _inactive.size(); i != ei; ++i) {
        LifeTimeInterval &interval = *_inactive[i];
        if (interval.isFixedInterval())
            continue;
        if (isFPReg == interval.isFP() && interval.reg() == reg) {
            LifeTimeInterval::Ranges ranges = interval.ranges();
            int endOfLifetimeHole = -1;
            for (int j = 0, ej = ranges.size(); j != ej; ++j) {
                if (position < ranges[j].start)
                    endOfLifetimeHole = ranges[j].start;
            }
            if (endOfLifetimeHole != -1)
                split(interval, endOfLifetimeHole);
        }
    }
}

void RegisterAllocator::assignSpillSlot(const Temp &t, int startPos, int endPos)
{
    if (_assignedSpillSlots[t.index] != InvalidSpillSlot)
        return;

    for (int i = 0, ei = _activeSpillSlots.size(); i != ei; ++i) {
        if (_activeSpillSlots.at(i) < startPos) {
            _activeSpillSlots[i] = endPos;
            _assignedSpillSlots[t.index] = i;
            return;
        }
    }

    Q_UNREACHABLE();
}

void RegisterAllocator::dump(IR::Function *function) const
{
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QTextStream qout(&buf);
    IRPrinterWithPositions printer(&qout, _lifeTimeIntervals);

    qout << "Ranges:" << endl;
    QVector<LifeTimeInterval *> handled = _handled;
    std::sort(handled.begin(), handled.end(), LifeTimeInterval::lessThanForTemp);
    for (const LifeTimeInterval *r : qAsConst(handled)) {
        r->dump(qout);
        qout << endl;
    }

    qout << "Spill slots:" << endl;
    for (unsigned i = 0; i < _assignedSpillSlots.size(); ++i)
        if (_assignedSpillSlots[i] != InvalidSpillSlot)
            qout << "\t%" << i << " -> " << _assignedSpillSlots[i] << endl;

    printer.print(function);
    qDebug("%s", buf.data().constData());
}

// References:
//  [Wimmer1] C. Wimmer and M. Franz. Linear Scan Register Allocation on SSA Form. In Proceedings of
//            CGO’10, ACM Press, 2010
//  [Wimmer2] C. Wimmer and H. Mossenbock. Optimized Interval Splitting in a Linear Scan Register
//            Allocator. In Proceedings of the ACM/USENIX International Conference on Virtual
//            Execution Environments, pages 132–141. ACM Press, 2005.
//  [Traub]   Omri Traub, Glenn Holloway, and Michael D. Smith. Quality and Speed in Linear-scan
//            Register Allocation. In Proceedings of the ACM SIGPLAN 1998 Conference on Programming
//            Language Design and Implementation, pages 142–151, June 1998.
