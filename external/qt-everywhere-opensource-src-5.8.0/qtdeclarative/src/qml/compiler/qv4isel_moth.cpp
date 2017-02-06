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

#include "qv4isel_util_p.h"
#include "qv4isel_moth_p.h"
#include "qv4vme_moth_p.h"
#include "qv4ssa_p.h"
#include <private/qv4debugging_p.h>
#include <private/qv4function_p.h>
#include <private/qv4regexpobject_p.h>
#include <private/qv4compileddata_p.h>
#include <private/qqmlengine_p.h>
#include <wtf/MathExtras.h>

#undef USE_TYPE_INFO

using namespace QV4;
using namespace QV4::Moth;

namespace {

inline uint aluOpFunction(IR::AluOp op)
{
    switch (op) {
    case IR::OpInvalid:
        return 0;
    case IR::OpIfTrue:
        return 0;
    case IR::OpNot:
        return 0;
    case IR::OpUMinus:
        return 0;
    case IR::OpUPlus:
        return 0;
    case IR::OpCompl:
        return 0;
    case IR::OpBitAnd:
        return offsetof(QV4::Runtime, bitAnd);
    case IR::OpBitOr:
        return offsetof(QV4::Runtime, bitOr);
    case IR::OpBitXor:
        return offsetof(QV4::Runtime, bitXor);
    case IR::OpAdd:
        return 0;
    case IR::OpSub:
        return offsetof(QV4::Runtime, sub);
    case IR::OpMul:
        return offsetof(QV4::Runtime, mul);
    case IR::OpDiv:
        return offsetof(QV4::Runtime, div);
    case IR::OpMod:
        return offsetof(QV4::Runtime, mod);
    case IR::OpLShift:
        return offsetof(QV4::Runtime, shl);
    case IR::OpRShift:
        return offsetof(QV4::Runtime, shr);
    case IR::OpURShift:
        return offsetof(QV4::Runtime, ushr);
    case IR::OpGt:
        return offsetof(QV4::Runtime, greaterThan);
    case IR::OpLt:
        return offsetof(QV4::Runtime, lessThan);
    case IR::OpGe:
        return offsetof(QV4::Runtime, greaterEqual);
    case IR::OpLe:
        return offsetof(QV4::Runtime, lessEqual);
    case IR::OpEqual:
        return offsetof(QV4::Runtime, equal);
    case IR::OpNotEqual:
        return offsetof(QV4::Runtime, notEqual);
    case IR::OpStrictEqual:
        return offsetof(QV4::Runtime, strictEqual);
    case IR::OpStrictNotEqual:
        return offsetof(QV4::Runtime, strictNotEqual);
    case IR::OpInstanceof:
        return 0;
    case IR::OpIn:
        return 0;
    case IR::OpAnd:
        return 0;
    case IR::OpOr:
        return 0;
    default:
        Q_ASSERT(!"Unknown AluOp");
        return 0;
    }
};

inline bool isNumberType(IR::Expr *e)
{
    switch (e->type) {
    case IR::SInt32Type:
    case IR::UInt32Type:
    case IR::DoubleType:
        return true;
    default:
        return false;
    }
}

inline bool isIntegerType(IR::Expr *e)
{
    switch (e->type) {
    case IR::SInt32Type:
    case IR::UInt32Type:
        return true;
    default:
        return false;
    }
}

inline bool isBoolType(IR::Expr *e)
{
    return (e->type == IR::BoolType);
}

} // anonymous namespace

InstructionSelection::InstructionSelection(QQmlEnginePrivate *qmlEngine, QV4::ExecutableAllocator *execAllocator, IR::Module *module, QV4::Compiler::JSUnitGenerator *jsGenerator, EvalISelFactory *iselFactory)
    : EvalInstructionSelection(execAllocator, module, jsGenerator, iselFactory)
    , qmlEngine(qmlEngine)
    , _block(0)
    , _codeStart(0)
    , _codeNext(0)
    , _codeEnd(0)
    , _currentStatement(0)
    , compilationUnit(new CompilationUnit)
{
    setUseTypeInference(false);
}

InstructionSelection::~InstructionSelection()
{
}

void InstructionSelection::run(int functionIndex)
{
    IR::Function *function = irModule->functions[functionIndex];
    IR::BasicBlock *block = 0, *nextBlock = 0;

    QHash<IR::BasicBlock *, QVector<ptrdiff_t> > patches;
    QHash<IR::BasicBlock *, ptrdiff_t> addrs;

    int codeSize = 4096;
    uchar *codeStart = new uchar[codeSize];
    memset(codeStart, 0, codeSize);
    uchar *codeNext = codeStart;
    uchar *codeEnd = codeStart + codeSize;

    qSwap(_function, function);
    qSwap(block, _block);
    qSwap(nextBlock, _nextBlock);
    qSwap(patches, _patches);
    qSwap(addrs, _addrs);
    qSwap(codeStart, _codeStart);
    qSwap(codeNext, _codeNext);
    qSwap(codeEnd, _codeEnd);

    IR::Optimizer opt(_function);
    opt.run(qmlEngine, useTypeInference, /*peelLoops =*/ false);
    if (opt.isInSSA()) {
        static const bool doStackSlotAllocation =
                qEnvironmentVariableIsEmpty("QV4_NO_INTERPRETER_STACK_SLOT_ALLOCATION");

        if (doStackSlotAllocation) {
            IR::AllocateStackSlots(opt.lifeTimeIntervals()).forFunction(_function);
        } else {
            opt.convertOutOfSSA();
            ConvertTemps().toStackSlots(_function);
        }
        opt.showMeTheCode(_function, "After stack slot allocation");
    } else {
        ConvertTemps().toStackSlots(_function);
    }

    QSet<IR::Jump *> removableJumps = opt.calculateOptionalJumps();
    qSwap(_removableJumps, removableJumps);

    IR::Stmt *cs = 0;
    qSwap(_currentStatement, cs);

    int locals = frameSize();
    Q_ASSERT(locals >= 0);

    IR::BasicBlock *exceptionHandler = 0;

    Instruction::Push push;
    push.value = quint32(locals);
    addInstruction(push);

    currentLine = 0;
    const QVector<IR::BasicBlock *> &basicBlocks = _function->basicBlocks();
    for (int i = 0, ei = basicBlocks.size(); i != ei; ++i) {
        blockNeedsDebugInstruction = irModule->debugMode;
        _block = basicBlocks[i];
        _nextBlock = (i < ei - 1) ? basicBlocks[i + 1] : 0;
        _addrs.insert(_block, _codeNext - _codeStart);

        if (_block->catchBlock != exceptionHandler) {
            Instruction::SetExceptionHandler set;
            set.offset = 0;
            if (_block->catchBlock) {
                ptrdiff_t loc = addInstruction(set) + (((const char *)&set.offset) - ((const char *)&set));
                _patches[_block->catchBlock].append(loc);
            } else {
                addInstruction(set);
            }
            exceptionHandler = _block->catchBlock;
        } else if (_block->catchBlock == nullptr && _block->index() != 0 && _block->in.isEmpty()) {
            exceptionHandler = nullptr;
            Instruction::SetExceptionHandler set;
            set.offset = 0;
            addInstruction(set);
        }

        for (IR::Stmt *s : _block->statements()) {
            _currentStatement = s;

            if (s->location.isValid()) {
                if (s->location.startLine != currentLine) {
                    blockNeedsDebugInstruction = false;
                    currentLine = s->location.startLine;
#ifndef QT_NO_QML_DEBUGGER
                    if (irModule->debugMode) {
                        Instruction::Debug debug;
                        debug.lineNumber = currentLine;
                        addInstruction(debug);
                    } else {
                        Instruction::Line line;
                        line.lineNumber = currentLine;
                        addInstruction(line);
                    }
#endif
                }
            }

            visit(s);
        }
    }

    // TODO: patch stack size (the push instruction)
    patchJumpAddresses();

    codeRefs.insert(_function, squeezeCode());

    qSwap(_currentStatement, cs);
    qSwap(_removableJumps, removableJumps);
    qSwap(_function, function);
    qSwap(block, _block);
    qSwap(nextBlock, _nextBlock);
    qSwap(patches, _patches);
    qSwap(addrs, _addrs);
    qSwap(codeStart, _codeStart);
    qSwap(codeNext, _codeNext);
    qSwap(codeEnd, _codeEnd);

    delete[] codeStart;
}

QQmlRefPointer<QV4::CompiledData::CompilationUnit> InstructionSelection::backendCompileStep()
{
    compilationUnit->codeRefs.resize(irModule->functions.size());
    int i = 0;
    foreach (IR::Function *irFunction, irModule->functions)
        compilationUnit->codeRefs[i++] = codeRefs[irFunction];
    QQmlRefPointer<QV4::CompiledData::CompilationUnit> result;
    result.adopt(compilationUnit.take());
    return result;
}

void InstructionSelection::callValue(IR::Expr *value, IR::ExprList *args, IR::Expr *result)
{
    Instruction::CallValue call;
    prepareCallArgs(args, call.argc);
    call.callData = callDataStart();
    call.dest = getParam(value);
    call.result = getResultParam(result);
    addInstruction(call);
}

void InstructionSelection::callQmlContextProperty(IR::Expr *base, IR::Member::MemberKind kind, int propertyIndex, IR::ExprList *args, IR::Expr *result)
{
    if (kind == IR::Member::MemberOfQmlScopeObject) {
        Instruction::CallScopeObjectProperty call;
        call.base = getParam(base);
        call.index = propertyIndex;
        prepareCallArgs(args, call.argc);
        call.callData = callDataStart();
        call.result = getResultParam(result);
        addInstruction(call);
    } else if (kind == IR::Member::MemberOfQmlContextObject) {
        Instruction::CallContextObjectProperty call;
        call.base = getParam(base);
        call.index = propertyIndex;
        prepareCallArgs(args, call.argc);
        call.callData = callDataStart();
        call.result = getResultParam(result);
        addInstruction(call);
    } else {
        Q_ASSERT(false);
    }
}

void InstructionSelection::callProperty(IR::Expr *base, const QString &name, IR::ExprList *args,
                                        IR::Expr *result)
{
    if (useFastLookups) {
        Instruction::CallPropertyLookup call;
        call.base = getParam(base);
        call.lookupIndex = registerGetterLookup(name);
        prepareCallArgs(args, call.argc);
        call.callData = callDataStart();
        call.result = getResultParam(result);
        addInstruction(call);
    } else {
        // call the property on the loaded base
        Instruction::CallProperty call;
        call.base = getParam(base);
        call.name = registerString(name);
        prepareCallArgs(args, call.argc);
        call.callData = callDataStart();
        call.result = getResultParam(result);
        addInstruction(call);
    }
}

void InstructionSelection::callSubscript(IR::Expr *base, IR::Expr *index, IR::ExprList *args,
                                         IR::Expr *result)
{
    // call the property on the loaded base
    Instruction::CallElement call;
    call.base = getParam(base);
    call.index = getParam(index);
    prepareCallArgs(args, call.argc);
    call.callData = callDataStart();
    call.result = getResultParam(result);
    addInstruction(call);
}

void InstructionSelection::convertType(IR::Expr *source, IR::Expr *target)
{
    // FIXME: do something more useful with this info
    if (target->type & IR::NumberType && !(source->type & IR::NumberType))
        unop(IR::OpUPlus, source, target);
    else
        copyValue(source, target);
}

void InstructionSelection::constructActivationProperty(IR::Name *func,
                                                       IR::ExprList *args,
                                                       IR::Expr *target)
{
    if (useFastLookups && func->global) {
        Instruction::ConstructGlobalLookup call;
        call.index = registerGlobalGetterLookup(*func->id);
        prepareCallArgs(args, call.argc);
        call.callData = callDataStart();
        call.result = getResultParam(target);
        addInstruction(call);
        return;
    }
    Instruction::CreateActivationProperty create;
    create.name = registerString(*func->id);
    prepareCallArgs(args, create.argc);
    create.callData = callDataStart();
    create.result = getResultParam(target);
    addInstruction(create);
}

void InstructionSelection::constructProperty(IR::Expr *base, const QString &name, IR::ExprList *args, IR::Expr *target)
{
    if (useFastLookups) {
        Instruction::ConstructPropertyLookup call;
        call.base = getParam(base);
        call.index = registerGetterLookup(name);
        prepareCallArgs(args, call.argc);
        call.callData = callDataStart();
        call.result = getResultParam(target);
        addInstruction(call);
        return;
    }
    Instruction::CreateProperty create;
    create.base = getParam(base);
    create.name = registerString(name);
    prepareCallArgs(args, create.argc);
    create.callData = callDataStart();
    create.result = getResultParam(target);
    addInstruction(create);
}

void InstructionSelection::constructValue(IR::Expr *value, IR::ExprList *args, IR::Expr *target)
{
    Instruction::CreateValue create;
    create.func = getParam(value);
    prepareCallArgs(args, create.argc);
    create.callData = callDataStart();
    create.result = getResultParam(target);
    addInstruction(create);
}

void InstructionSelection::loadThisObject(IR::Expr *e)
{
    Instruction::LoadThis load;
    load.result = getResultParam(e);
    addInstruction(load);
}

void InstructionSelection::loadQmlContext(IR::Expr *e)
{
    Instruction::LoadQmlContext load;
    load.result = getResultParam(e);
    addInstruction(load);
}

void InstructionSelection::loadQmlImportedScripts(IR::Expr *e)
{
    Instruction::LoadQmlImportedScripts load;
    load.result = getResultParam(e);
    addInstruction(load);
}

void InstructionSelection::loadQmlSingleton(const QString &name, IR::Expr *e)
{
    Instruction::LoadQmlSingleton load;
    load.result = getResultParam(e);
    load.name = registerString(name);
    addInstruction(load);
}

void InstructionSelection::loadConst(IR::Const *sourceConst, IR::Expr *e)
{
    Q_ASSERT(sourceConst);

    Instruction::MoveConst move;
    move.source = convertToValue(sourceConst).asReturnedValue();
    move.result = getResultParam(e);
    addInstruction(move);
}

void InstructionSelection::loadString(const QString &str, IR::Expr *target)
{
    Instruction::LoadRuntimeString load;
    load.stringId = registerString(str);
    load.result = getResultParam(target);
    addInstruction(load);
}

void InstructionSelection::loadRegexp(IR::RegExp *sourceRegexp, IR::Expr *target)
{
    Instruction::LoadRegExp load;
    load.regExpId = registerRegExp(sourceRegexp);
    load.result = getResultParam(target);
    addInstruction(load);
}

void InstructionSelection::getActivationProperty(const IR::Name *name, IR::Expr *target)
{
    if (useFastLookups && name->global) {
        Instruction::GetGlobalLookup load;
        load.index = registerGlobalGetterLookup(*name->id);
        load.result = getResultParam(target);
        addInstruction(load);
        return;
    }
    Instruction::LoadName load;
    load.name = registerString(*name->id);
    load.result = getResultParam(target);
    addInstruction(load);
}

void InstructionSelection::setActivationProperty(IR::Expr *source, const QString &targetName)
{
    Instruction::StoreName store;
    store.source = getParam(source);
    store.name = registerString(targetName);
    addInstruction(store);
}

void InstructionSelection::initClosure(IR::Closure *closure, IR::Expr *target)
{
    int id = closure->value;
    Instruction::LoadClosure load;
    load.value = id;
    load.result = getResultParam(target);
    addInstruction(load);
}

void InstructionSelection::getProperty(IR::Expr *base, const QString &name, IR::Expr *target)
{
    if (useFastLookups) {
        Instruction::GetLookup load;
        load.base = getParam(base);
        load.index = registerGetterLookup(name);
        load.result = getResultParam(target);
        addInstruction(load);
        return;
    }
    Instruction::LoadProperty load;
    load.base = getParam(base);
    load.name = registerString(name);
    load.result = getResultParam(target);
    addInstruction(load);
}

void InstructionSelection::setProperty(IR::Expr *source, IR::Expr *targetBase,
                                       const QString &targetName)
{
    if (useFastLookups) {
        Instruction::SetLookup store;
        store.base = getParam(targetBase);
        store.index = registerSetterLookup(targetName);
        store.source = getParam(source);
        addInstruction(store);
        return;
    }
    Instruction::StoreProperty store;
    store.base = getParam(targetBase);
    store.name = registerString(targetName);
    store.source = getParam(source);
    addInstruction(store);
}

void InstructionSelection::setQmlContextProperty(IR::Expr *source, IR::Expr *targetBase, IR::Member::MemberKind kind, int propertyIndex)
{
    if (kind == IR::Member::MemberOfQmlScopeObject) {
        Instruction::StoreScopeObjectProperty store;
        store.base = getParam(targetBase);
        store.propertyIndex = propertyIndex;
        store.source = getParam(source);
        addInstruction(store);
    } else if (kind == IR::Member::MemberOfQmlContextObject) {
        Instruction::StoreContextObjectProperty store;
        store.base = getParam(targetBase);
        store.propertyIndex = propertyIndex;
        store.source = getParam(source);
        addInstruction(store);
    } else {
        Q_ASSERT(false);
    }
}

void InstructionSelection::setQObjectProperty(IR::Expr *source, IR::Expr *targetBase, int propertyIndex)
{
    Instruction::StoreQObjectProperty store;
    store.base = getParam(targetBase);
    store.propertyIndex = propertyIndex;
    store.source = getParam(source);
    addInstruction(store);
}

void InstructionSelection::getQmlContextProperty(IR::Expr *source, IR::Member::MemberKind kind, int index, bool captureRequired, IR::Expr *target)
{
    if (kind == IR::Member::MemberOfQmlScopeObject) {
        Instruction::LoadScopeObjectProperty load;
        load.base = getParam(source);
        load.propertyIndex = index;
        load.captureRequired = captureRequired;
        load.result = getResultParam(target);
        addInstruction(load);
    } else if (kind == IR::Member::MemberOfQmlContextObject) {
        Instruction::LoadContextObjectProperty load;
        load.base = getParam(source);
        load.propertyIndex = index;
        load.captureRequired = captureRequired;
        load.result = getResultParam(target);
        addInstruction(load);
    } else if (kind == IR::Member::MemberOfIdObjectsArray) {
        Instruction::LoadIdObject load;
        load.base = getParam(source);
        load.index = index;
        load.result = getResultParam(target);
        addInstruction(load);
    } else {
        Q_ASSERT(false);
    }
}

void InstructionSelection::getQObjectProperty(IR::Expr *base, int propertyIndex, bool captureRequired, bool isSingletonProperty, int attachedPropertiesId, IR::Expr *target)
{
    if (attachedPropertiesId != 0) {
        Instruction::LoadAttachedQObjectProperty load;
        load.propertyIndex = propertyIndex;
        load.result = getResultParam(target);
        load.attachedPropertiesId = attachedPropertiesId;
        addInstruction(load);
    } else if (isSingletonProperty) {
        Instruction::LoadSingletonQObjectProperty load;
        load.base = getParam(base);
        load.propertyIndex = propertyIndex;
        load.result = getResultParam(target);
        load.captureRequired = captureRequired;
        addInstruction(load);
    } else {
        Instruction::LoadQObjectProperty load;
        load.base = getParam(base);
        load.propertyIndex = propertyIndex;
        load.result = getResultParam(target);
        load.captureRequired = captureRequired;
        addInstruction(load);
    }
}

void InstructionSelection::getElement(IR::Expr *base, IR::Expr *index, IR::Expr *target)
{
    if (useFastLookups) {
        Instruction::LoadElementLookup load;
        load.lookup = registerIndexedGetterLookup();
        load.base = getParam(base);
        load.index = getParam(index);
        load.result = getResultParam(target);
        addInstruction(load);
        return;
    }
    Instruction::LoadElement load;
    load.base = getParam(base);
    load.index = getParam(index);
    load.result = getResultParam(target);
    addInstruction(load);
}

void InstructionSelection::setElement(IR::Expr *source, IR::Expr *targetBase,
                                      IR::Expr *targetIndex)
{
    if (useFastLookups) {
        Instruction::StoreElementLookup store;
        store.lookup = registerIndexedSetterLookup();
        store.base = getParam(targetBase);
        store.index = getParam(targetIndex);
        store.source = getParam(source);
        addInstruction(store);
        return;
    }
    Instruction::StoreElement store;
    store.base = getParam(targetBase);
    store.index = getParam(targetIndex);
    store.source = getParam(source);
    addInstruction(store);
}

void InstructionSelection::copyValue(IR::Expr *source, IR::Expr *target)
{
    Instruction::Move move;
    move.source = getParam(source);
    move.result = getResultParam(target);
    if (move.source != move.result)
        addInstruction(move);
}

void InstructionSelection::swapValues(IR::Expr *source, IR::Expr *target)
{
    Instruction::SwapTemps swap;
    swap.left = getParam(source);
    swap.right = getParam(target);
    addInstruction(swap);
}

void InstructionSelection::unop(IR::AluOp oper, IR::Expr *source, IR::Expr *target)
{
    switch (oper) {
    case IR::OpIfTrue:
        Q_ASSERT(!"unreachable"); break;
    case IR::OpNot: {
        // ### enabling this fails in some cases, where apparently the value is not a bool at runtime
        if (0 && isBoolType(source)) {
            Instruction::UNotBool unot;
            unot.source = getParam(source);
            unot.result = getResultParam(target);
            addInstruction(unot);
            return;
        }
        Instruction::UNot unot;
        unot.source = getParam(source);
        unot.result = getResultParam(target);
        addInstruction(unot);
        return;
    }
    case IR::OpUMinus: {
        Instruction::UMinus uminus;
        uminus.source = getParam(source);
        uminus.result = getResultParam(target);
        addInstruction(uminus);
        return;
    }
    case IR::OpUPlus: {
        if (isNumberType(source)) {
            // use a move
            Instruction::Move move;
            move.source = getParam(source);
            move.result = getResultParam(target);
            if (move.source != move.result)
                addInstruction(move);
            return;
        }
        Instruction::UPlus uplus;
        uplus.source = getParam(source);
        uplus.result = getResultParam(target);
        addInstruction(uplus);
        return;
    }
    case IR::OpCompl: {
        // ### enabling this fails in some cases, where apparently the value is not a int at runtime
        if (0 && isIntegerType(source)) {
            Instruction::UComplInt unot;
            unot.source = getParam(source);
            unot.result = getResultParam(target);
            addInstruction(unot);
            return;
        }
        Instruction::UCompl ucompl;
        ucompl.source = getParam(source);
        ucompl.result = getResultParam(target);
        addInstruction(ucompl);
        return;
    }
    case IR::OpIncrement: {
        Instruction::Increment inc;
        inc.source = getParam(source);
        inc.result = getResultParam(target);
        addInstruction(inc);
        return;
    }
    case IR::OpDecrement: {
        Instruction::Decrement dec;
        dec.source = getParam(source);
        dec.result = getResultParam(target);
        addInstruction(dec);
        return;
    }
    default:  break;
    } // switch

    Q_ASSERT(!"unreachable");
}

void InstructionSelection::binop(IR::AluOp oper, IR::Expr *leftSource, IR::Expr *rightSource, IR::Expr *target)
{
    binopHelper(oper, leftSource, rightSource, target);
}

Param InstructionSelection::binopHelper(IR::AluOp oper, IR::Expr *leftSource, IR::Expr *rightSource, IR::Expr *target)
{
    if (oper == IR::OpAdd) {
        Instruction::Add add;
        add.lhs = getParam(leftSource);
        add.rhs = getParam(rightSource);
        add.result = getResultParam(target);
        addInstruction(add);
        return add.result;
    }
    if (oper == IR::OpSub) {
        Instruction::Sub sub;
        sub.lhs = getParam(leftSource);
        sub.rhs = getParam(rightSource);
        sub.result = getResultParam(target);
        addInstruction(sub);
        return sub.result;
    }
    if (oper == IR::OpMul) {
        Instruction::Mul mul;
        mul.lhs = getParam(leftSource);
        mul.rhs = getParam(rightSource);
        mul.result = getResultParam(target);
        addInstruction(mul);
        return mul.result;
    }
    if (oper == IR::OpBitAnd) {
        if (leftSource->asConst())
            qSwap(leftSource, rightSource);
        if (IR::Const *c = rightSource->asConst()) {
            Instruction::BitAndConst bitAnd;
            bitAnd.lhs = getParam(leftSource);
            bitAnd.rhs = convertToValue(c).Value::toInt32();
            bitAnd.result = getResultParam(target);
            addInstruction(bitAnd);
            return bitAnd.result;
        }
        Instruction::BitAnd bitAnd;
        bitAnd.lhs = getParam(leftSource);
        bitAnd.rhs = getParam(rightSource);
        bitAnd.result = getResultParam(target);
        addInstruction(bitAnd);
        return bitAnd.result;
    }
    if (oper == IR::OpBitOr) {
        if (leftSource->asConst())
            qSwap(leftSource, rightSource);
        if (IR::Const *c = rightSource->asConst()) {
            Instruction::BitOrConst bitOr;
            bitOr.lhs = getParam(leftSource);
            bitOr.rhs = convertToValue(c).Value::toInt32();
            bitOr.result = getResultParam(target);
            addInstruction(bitOr);
            return bitOr.result;
        }
        Instruction::BitOr bitOr;
        bitOr.lhs = getParam(leftSource);
        bitOr.rhs = getParam(rightSource);
        bitOr.result = getResultParam(target);
        addInstruction(bitOr);
        return bitOr.result;
    }
    if (oper == IR::OpBitXor) {
        if (leftSource->asConst())
            qSwap(leftSource, rightSource);
        if (IR::Const *c = rightSource->asConst()) {
            Instruction::BitXorConst bitXor;
            bitXor.lhs = getParam(leftSource);
            bitXor.rhs = convertToValue(c).Value::toInt32();
            bitXor.result = getResultParam(target);
            addInstruction(bitXor);
            return bitXor.result;
        }
        Instruction::BitXor bitXor;
        bitXor.lhs = getParam(leftSource);
        bitXor.rhs = getParam(rightSource);
        bitXor.result = getResultParam(target);
        addInstruction(bitXor);
        return bitXor.result;
    }
    if (oper == IR::OpRShift) {
        if (IR::Const *c = rightSource->asConst()) {
            Instruction::ShrConst shr;
            shr.lhs = getParam(leftSource);
            shr.rhs = convertToValue(c).Value::toInt32() & 0x1f;
            shr.result = getResultParam(target);
            addInstruction(shr);
            return shr.result;
        }
        Instruction::Shr shr;
        shr.lhs = getParam(leftSource);
        shr.rhs = getParam(rightSource);
        shr.result = getResultParam(target);
        addInstruction(shr);
        return shr.result;
    }
    if (oper == IR::OpLShift) {
        if (IR::Const *c = rightSource->asConst()) {
            Instruction::ShlConst shl;
            shl.lhs = getParam(leftSource);
            shl.rhs = convertToValue(c).Value::toInt32() & 0x1f;
            shl.result = getResultParam(target);
            addInstruction(shl);
            return shl.result;
        }
        Instruction::Shl shl;
        shl.lhs = getParam(leftSource);
        shl.rhs = getParam(rightSource);
        shl.result = getResultParam(target);
        addInstruction(shl);
        return shl.result;
    }

    if (oper == IR::OpInstanceof || oper == IR::OpIn || oper == IR::OpAdd) {
        Instruction::BinopContext binop;
        if (oper == IR::OpInstanceof)
            binop.alu = offsetof(QV4::Runtime, instanceof);
        else if (oper == IR::OpIn)
            binop.alu = offsetof(QV4::Runtime, in);
        else
            binop.alu = offsetof(QV4::Runtime, add);
        binop.lhs = getParam(leftSource);
        binop.rhs = getParam(rightSource);
        binop.result = getResultParam(target);
        Q_ASSERT(binop.alu);
        addInstruction(binop);
        return binop.result;
    } else {
        Instruction::Binop binop;
        binop.alu = aluOpFunction(oper);
        binop.lhs = getParam(leftSource);
        binop.rhs = getParam(rightSource);
        binop.result = getResultParam(target);
        Q_ASSERT(binop.alu);
        addInstruction(binop);
        return binop.result;
    }
}

void InstructionSelection::prepareCallArgs(IR::ExprList *e, quint32 &argc, quint32 *args)
{
    int argLocation = outgoingArgumentTempStart();
    argc = 0;
    if (args)
        *args = argLocation;
    if (e) {
        // We need to move all the temps into the function arg array
        Q_ASSERT(argLocation >= 0);
        while (e) {
            if (IR::Const *c = e->expr->asConst()) {
                Instruction::MoveConst move;
                move.source = convertToValue(c).asReturnedValue();
                move.result = Param::createTemp(argLocation);
                addInstruction(move);
            } else {
                Instruction::Move move;
                move.source = getParam(e->expr);
                move.result = Param::createTemp(argLocation);
                addInstruction(move);
            }
            ++argLocation;
            ++argc;
            e = e->next;
        }
    }
}

void InstructionSelection::addDebugInstruction()
{
#ifndef QT_NO_QML_DEBUGGER
    if (blockNeedsDebugInstruction) {
        Instruction::Debug debug;
        debug.lineNumber = -int(currentLine);
        addInstruction(debug);
    }
#endif
}

void InstructionSelection::visitJump(IR::Jump *s)
{
    if (s->target == _nextBlock)
        return;
    if (_removableJumps.contains(s))
        return;

    addDebugInstruction();

    Instruction::Jump jump;
    jump.offset = 0;
    ptrdiff_t loc = addInstruction(jump) + (((const char *)&jump.offset) - ((const char *)&jump));

    _patches[s->target].append(loc);
}

void InstructionSelection::visitCJump(IR::CJump *s)
{
    addDebugInstruction();

    Param condition;
    if (IR::Temp *t = s->cond->asTemp()) {
        condition = getResultParam(t);
    } else if (IR::Binop *b = s->cond->asBinop()) {
        condition = binopHelper(b->op, b->left, b->right, /*target*/0);
    } else {
        Q_UNIMPLEMENTED();
    }

    if (s->iftrue == _nextBlock) {
        Instruction::JumpNe jump;
        jump.offset = 0;
        jump.condition = condition;
        ptrdiff_t falseLoc = addInstruction(jump) + (((const char *)&jump.offset) - ((const char *)&jump));
        _patches[s->iffalse].append(falseLoc);
    } else {
        Instruction::JumpEq jump;
        jump.offset = 0;
        jump.condition = condition;
        ptrdiff_t trueLoc = addInstruction(jump) + (((const char *)&jump.offset) - ((const char *)&jump));
        _patches[s->iftrue].append(trueLoc);

        if (s->iffalse != _nextBlock) {
            Instruction::Jump jump;
            jump.offset = 0;
            ptrdiff_t falseLoc = addInstruction(jump) + (((const char *)&jump.offset) - ((const char *)&jump));
            _patches[s->iffalse].append(falseLoc);
        }
    }
}

void InstructionSelection::visitRet(IR::Ret *s)
{
    // this is required so stepOut will always be guaranteed to stop in every stack frame
    addDebugInstruction();

    Instruction::Ret ret;
    ret.result = getParam(s->expr);
    addInstruction(ret);
}

void InstructionSelection::callBuiltinInvalid(IR::Name *func, IR::ExprList *args, IR::Expr *result)
{
    if (useFastLookups && func->global) {
        Instruction::CallGlobalLookup call;
        call.index = registerGlobalGetterLookup(*func->id);
        prepareCallArgs(args, call.argc);
        call.callData = callDataStart();
        call.result = getResultParam(result);
        addInstruction(call);
        return;
    }
    Instruction::CallActivationProperty call;
    call.name = registerString(*func->id);
    prepareCallArgs(args, call.argc);
    call.callData = callDataStart();
    call.result = getResultParam(result);
    addInstruction(call);
}

void InstructionSelection::callBuiltinTypeofQmlContextProperty(IR::Expr *base, IR::Member::MemberKind kind, int propertyIndex, IR::Expr *result)
{
    if (kind == IR::Member::MemberOfQmlScopeObject) {
        Instruction::CallBuiltinTypeofScopeObjectProperty call;
        call.base = getParam(base);
        call.index = propertyIndex;
        call.result = getResultParam(result);
        addInstruction(call);
    } else if (kind == IR::Member::MemberOfQmlContextObject) {
        Instruction::CallBuiltinTypeofContextObjectProperty call;
        call.base = getParam(base);
        call.index = propertyIndex;
        call.result = getResultParam(result);
        addInstruction(call);
    } else {
        Q_UNREACHABLE();
    }
}

void InstructionSelection::callBuiltinTypeofMember(IR::Expr *base, const QString &name,
                                                   IR::Expr *result)
{
    Instruction::CallBuiltinTypeofMember call;
    call.base = getParam(base);
    call.member = registerString(name);
    call.result = getResultParam(result);
    addInstruction(call);
}

void InstructionSelection::callBuiltinTypeofSubscript(IR::Expr *base, IR::Expr *index,
                                                      IR::Expr *result)
{
    Instruction::CallBuiltinTypeofSubscript call;
    call.base = getParam(base);
    call.index = getParam(index);
    call.result = getResultParam(result);
    addInstruction(call);
}

void InstructionSelection::callBuiltinTypeofName(const QString &name, IR::Expr *result)
{
    Instruction::CallBuiltinTypeofName call;
    call.name = registerString(name);
    call.result = getResultParam(result);
    addInstruction(call);
}

void InstructionSelection::callBuiltinTypeofValue(IR::Expr *value, IR::Expr *result)
{
    Instruction::CallBuiltinTypeofValue call;
    call.value = getParam(value);
    call.result = getResultParam(result);
    addInstruction(call);
}

void InstructionSelection::callBuiltinDeleteMember(IR::Expr *base, const QString &name, IR::Expr *result)
{
    Instruction::CallBuiltinDeleteMember call;
    call.base = getParam(base);
    call.member = registerString(name);
    call.result = getResultParam(result);
    addInstruction(call);
}

void InstructionSelection::callBuiltinDeleteSubscript(IR::Expr *base, IR::Expr *index,
                                                      IR::Expr *result)
{
    Instruction::CallBuiltinDeleteSubscript call;
    call.base = getParam(base);
    call.index = getParam(index);
    call.result = getResultParam(result);
    addInstruction(call);
}

void InstructionSelection::callBuiltinDeleteName(const QString &name, IR::Expr *result)
{
    Instruction::CallBuiltinDeleteName call;
    call.name = registerString(name);
    call.result = getResultParam(result);
    addInstruction(call);
}

void InstructionSelection::callBuiltinDeleteValue(IR::Expr *result)
{
    Instruction::MoveConst move;
    move.source = QV4::Encode(false);
    move.result = getResultParam(result);
    addInstruction(move);
}

void InstructionSelection::callBuiltinThrow(IR::Expr *arg)
{
    Instruction::CallBuiltinThrow call;
    call.arg = getParam(arg);
    addInstruction(call);
}

void InstructionSelection::callBuiltinReThrow()
{
    if (_block->catchBlock) {
        // jump to exception handler
        Instruction::Jump jump;
        jump.offset = 0;
        ptrdiff_t loc = addInstruction(jump) + (((const char *)&jump.offset) - ((const char *)&jump));

        _patches[_block->catchBlock].append(loc);
    } else {
        Instruction::Ret ret;
        int idx = jsUnitGenerator()->registerConstant(QV4::Encode::undefined());
        ret.result = Param::createConstant(idx);
        addInstruction(ret);
    }
}

void InstructionSelection::callBuiltinUnwindException(IR::Expr *result)
{
    Instruction::CallBuiltinUnwindException call;
    call.result = getResultParam(result);
    addInstruction(call);
}


void InstructionSelection::callBuiltinPushCatchScope(const QString &exceptionName)
{
    Instruction::CallBuiltinPushCatchScope call;
    call.name = registerString(exceptionName);
    addInstruction(call);
}

void InstructionSelection::callBuiltinForeachIteratorObject(IR::Expr *arg, IR::Expr *result)
{
    Instruction::CallBuiltinForeachIteratorObject call;
    call.arg = getParam(arg);
    call.result = getResultParam(result);
    addInstruction(call);
}

void InstructionSelection::callBuiltinForeachNextPropertyname(IR::Expr *arg, IR::Expr *result)
{
    Instruction::CallBuiltinForeachNextPropertyName call;
    call.arg = getParam(arg);
    call.result = getResultParam(result);
    addInstruction(call);
}

void InstructionSelection::callBuiltinPushWithScope(IR::Expr *arg)
{
    Instruction::CallBuiltinPushScope call;
    call.arg = getParam(arg);
    addInstruction(call);
}

void InstructionSelection::callBuiltinPopScope()
{
    Instruction::CallBuiltinPopScope call;
    addInstruction(call);
}

void InstructionSelection::callBuiltinDeclareVar(bool deletable, const QString &name)
{
    Instruction::CallBuiltinDeclareVar call;
    call.isDeletable = deletable;
    call.varName = registerString(name);
    addInstruction(call);
}

void InstructionSelection::callBuiltinDefineArray(IR::Expr *result, IR::ExprList *args)
{
    Instruction::CallBuiltinDefineArray call;
    prepareCallArgs(args, call.argc, &call.args);
    call.result = getResultParam(result);
    addInstruction(call);
}

void InstructionSelection::callBuiltinDefineObjectLiteral(IR::Expr *result, int keyValuePairCount, IR::ExprList *keyValuePairs, IR::ExprList *arrayEntries, bool needSparseArray)
{
    int argLocation = outgoingArgumentTempStart();

    const int classId = registerJSClass(keyValuePairCount, keyValuePairs);

    // Process key/value pairs first
    IR::ExprList *it = keyValuePairs;
    for (int i = 0; i < keyValuePairCount; ++i, it = it->next) {
        // Skip name
        it = it->next;

        bool isData = it->expr->asConst()->value;
        it = it->next;

        if (IR::Const *c = it->expr->asConst()) {
            Instruction::MoveConst move;
            move.source = convertToValue(c).asReturnedValue();
            move.result = Param::createTemp(argLocation);
            addInstruction(move);
        } else {
            Instruction::Move move;
            move.source = getParam(it->expr);
            move.result = Param::createTemp(argLocation);
            addInstruction(move);
        }
        ++argLocation;

        if (!isData) {
            it = it->next;

            Instruction::Move move;
            move.source = getParam(it->expr);
            move.result = Param::createTemp(argLocation);
            addInstruction(move);
            ++argLocation;
        }
    }

    // Process array values
    uint arrayValueCount = 0;
    it = arrayEntries;
    while (it) {
        IR::Const *index = it->expr->asConst();
        it = it->next;

        bool isData = it->expr->asConst()->value;
        it = it->next;

        if (!isData) {
            it = it->next; // getter
            it = it->next; // setter
            continue;
        }

        ++arrayValueCount;

        Instruction::MoveConst indexMove;
        indexMove.source = convertToValue(index).asReturnedValue();
        indexMove.result = Param::createTemp(argLocation);
        addInstruction(indexMove);
        ++argLocation;

        Instruction::Move move;
        move.source = getParam(it->expr);
        move.result = Param::createTemp(argLocation);
        addInstruction(move);
        ++argLocation;
        it = it->next;
    }

    // Process array getter/setter pairs
    uint arrayGetterSetterCount = 0;
    it = arrayEntries;
    while (it) {
        IR::Const *index = it->expr->asConst();
        it = it->next;

        bool isData = it->expr->asConst()->value;
        it = it->next;

        if (isData) {
            it = it->next; // value
            continue;
        }

        ++arrayGetterSetterCount;

        Instruction::MoveConst indexMove;
        indexMove.source = convertToValue(index).asReturnedValue();
        indexMove.result = Param::createTemp(argLocation);
        addInstruction(indexMove);
        ++argLocation;

        // getter
        Instruction::Move moveGetter;
        moveGetter.source = getParam(it->expr);
        moveGetter.result = Param::createTemp(argLocation);
        addInstruction(moveGetter);
        ++argLocation;
        it = it->next;

        // setter
        Instruction::Move moveSetter;
        moveSetter.source = getParam(it->expr);
        moveSetter.result = Param::createTemp(argLocation);
        addInstruction(moveSetter);
        ++argLocation;
        it = it->next;
    }

    Instruction::CallBuiltinDefineObjectLiteral call;
    call.internalClassId = classId;
    call.arrayValueCount = arrayValueCount;
    call.arrayGetterSetterCountAndFlags = arrayGetterSetterCount | (needSparseArray << 30);
    call.args = outgoingArgumentTempStart();
    call.result = getResultParam(result);
    addInstruction(call);
}

void InstructionSelection::callBuiltinSetupArgumentObject(IR::Expr *result)
{
    Instruction::CallBuiltinSetupArgumentsObject call;
    call.result = getResultParam(result);
    addInstruction(call);
}


void QV4::Moth::InstructionSelection::callBuiltinConvertThisToObject()
{
    Instruction::CallBuiltinConvertThisToObject call;
    addInstruction(call);
}

ptrdiff_t InstructionSelection::addInstructionHelper(Instr::Type type, Instr &instr)
{
    instr.common.instructionType = type;

    int instructionSize = Instr::size(type);
    if (_codeEnd - _codeNext < instructionSize) {
        int currSize = _codeEnd - _codeStart;
        uchar *newCode = new uchar[currSize * 2];
        ::memset(newCode + currSize, 0, currSize);
        ::memcpy(newCode, _codeStart, currSize);
        _codeNext = _codeNext - _codeStart + newCode;
        delete[] _codeStart;
        _codeStart = newCode;
        _codeEnd = _codeStart + currSize * 2;
    }

    ::memcpy(_codeNext, reinterpret_cast<const char *>(&instr), instructionSize);
    ptrdiff_t ptrOffset = _codeNext - _codeStart;
    _codeNext += instructionSize;

    return ptrOffset;
}

void InstructionSelection::patchJumpAddresses()
{
    typedef QHash<IR::BasicBlock *, QVector<ptrdiff_t> >::ConstIterator PatchIt;
    for (PatchIt i = _patches.cbegin(), ei = _patches.cend(); i != ei; ++i) {
        Q_ASSERT(_addrs.contains(i.key()));
        ptrdiff_t target = _addrs.value(i.key());

        const QVector<ptrdiff_t> &patchList = i.value();
        for (int ii = 0, eii = patchList.count(); ii < eii; ++ii) {
            ptrdiff_t patch = patchList.at(ii);

            *((ptrdiff_t *)(_codeStart + patch)) = target - patch;
        }
    }

    _patches.clear();
    _addrs.clear();
}

QByteArray InstructionSelection::squeezeCode() const
{
    int codeSize = _codeNext - _codeStart;
    QByteArray squeezed;
    squeezed.resize(codeSize);
    ::memcpy(squeezed.data(), _codeStart, codeSize);
    return squeezed;
}

Param InstructionSelection::getParam(IR::Expr *e) {
    Q_ASSERT(e);

    if (IR::Const *c = e->asConst()) {
        int idx = jsUnitGenerator()->registerConstant(convertToValue(c).asReturnedValue());
        return Param::createConstant(idx);
    } else if (IR::Temp *t = e->asTemp()) {
        switch (t->kind) {
        case IR::Temp::StackSlot:
            return Param::createTemp(t->index);
        default:
            Q_UNREACHABLE();
            return Param();
        }
    } else if (IR::ArgLocal *al = e->asArgLocal()) {
        switch (al->kind) {
        case IR::ArgLocal::Formal:
        case IR::ArgLocal::ScopedFormal: return Param::createArgument(al->index, al->scope);
        case IR::ArgLocal::Local: return Param::createLocal(al->index);
        case IR::ArgLocal::ScopedLocal: return Param::createScopedLocal(al->index, al->scope);
        default:
            Q_UNREACHABLE();
            return Param();
        }
    } else {
        Q_UNIMPLEMENTED();
        return Param();
    }
}


CompilationUnit::~CompilationUnit()
{
}

void CompilationUnit::linkBackendToEngine(QV4::ExecutionEngine *engine)
{
#ifdef MOTH_THREADED_INTERPRETER
    // link byte code against addresses of instructions
    for (int i = 0; i < codeRefs.count(); ++i) {
        QByteArray &codeRef = codeRefs[i];
        char *code = codeRef.data();
        int index = 0;
        while (index < codeRef.size()) {
            Instr *genericInstr = reinterpret_cast<Instr *>(code + index);

            switch (genericInstr->common.instructionType) {
#define LINK_INSTRUCTION(InstructionType, Member) \
            case Instr::InstructionType: \
                genericInstr->common.code = VME::instructionJumpTable()[static_cast<int>(genericInstr->common.instructionType)]; \
                index += InstrMeta<(int)Instr::InstructionType>::Size; \
            break;

            FOR_EACH_MOTH_INSTR(LINK_INSTRUCTION)

            }
        }
    }
#endif

    runtimeFunctions.resize(data->functionTableSize);
    runtimeFunctions.fill(0);
    for (int i = 0 ;i < runtimeFunctions.size(); ++i) {
        const QV4::CompiledData::Function *compiledFunction = data->functionAt(i);

        QV4::Function *runtimeFunction = new QV4::Function(engine, this, compiledFunction, &VME::exec);
        runtimeFunction->codeData = reinterpret_cast<const uchar *>(codeRefs.at(i).constData());
        runtimeFunctions[i] = runtimeFunction;
    }
}

void CompilationUnit::prepareCodeOffsetsForDiskStorage(CompiledData::Unit *unit)
{
    const int codeAlignment = 16;
    quint64 offset = WTF::roundUpToMultipleOf(codeAlignment, unit->unitSize);
    Q_ASSERT(int(unit->functionTableSize) == codeRefs.size());
    for (int i = 0; i < codeRefs.size(); ++i) {
        CompiledData::Function *compiledFunction = const_cast<CompiledData::Function *>(unit->functionAt(i));
        compiledFunction->codeOffset = offset;
        compiledFunction->codeSize = codeRefs.at(i).size();
        offset = WTF::roundUpToMultipleOf(codeAlignment, offset + compiledFunction->codeSize);
    }
}

bool CompilationUnit::saveCodeToDisk(QIODevice *device, const CompiledData::Unit *unit, QString *errorString)
{
    Q_ASSERT(device->pos() == unit->unitSize);
    Q_ASSERT(device->atEnd());
    Q_ASSERT(int(unit->functionTableSize) == codeRefs.size());

    QByteArray padding;

#ifdef MOTH_THREADED_INTERPRETER
    // Map from instruction label back to instruction type. Only needed when persisting
    // already linked compilation units;
    QHash<void*, int> reverseInstructionMapping;
    if (engine) {
        void **instructions = VME::instructionJumpTable();
        for (int i = 0; i < Instr::LastInstruction; ++i)
            reverseInstructionMapping.insert(instructions[i], i);
    }
#endif

    for (int i = 0; i < codeRefs.size(); ++i) {
        const CompiledData::Function *compiledFunction = unit->functionAt(i);

        if (device->pos() > qint64(compiledFunction->codeOffset)) {
            *errorString = QStringLiteral("Invalid state of cache file to write.");
            return false;
        }

        const quint64 paddingSize = compiledFunction->codeOffset - device->pos();
        padding.fill(0, paddingSize);
        qint64 written = device->write(padding);
        if (written != padding.size()) {
            *errorString = device->errorString();
            return false;
        }

        QByteArray code = codeRefs.at(i);

#ifdef MOTH_THREADED_INTERPRETER
        if (!reverseInstructionMapping.isEmpty()) {
            char *codePtr = code.data(); // detaches
            int index = 0;
            while (index < code.size()) {
                Instr *genericInstr = reinterpret_cast<Instr *>(codePtr + index);

                genericInstr->common.instructionType = reverseInstructionMapping.value(genericInstr->common.code);

                switch (genericInstr->common.instructionType) {
    #define REVERSE_INSTRUCTION(InstructionType, Member) \
                case Instr::InstructionType: \
                    index += InstrMeta<(int)Instr::InstructionType>::Size; \
                break;

                FOR_EACH_MOTH_INSTR(REVERSE_INSTRUCTION)
                }
            }
        }
#endif

        written = device->write(code.constData(), compiledFunction->codeSize);
        if (written != qint64(compiledFunction->codeSize)) {
            *errorString = device->errorString();
            return false;
        }
    }
    return true;
}

bool CompilationUnit::memoryMapCode(QString *errorString)
{
    Q_UNUSED(errorString);
    codeRefs.resize(data->functionTableSize);

    const char *basePtr = reinterpret_cast<const char *>(data);

    for (uint i = 0; i < data->functionTableSize; ++i) {
        const CompiledData::Function *compiledFunction = data->functionAt(i);
        const char *codePtr = const_cast<const char *>(reinterpret_cast<const char *>(basePtr + compiledFunction->codeOffset));
#ifdef MOTH_THREADED_INTERPRETER
        // for the threaded interpreter we need to make a copy of the data because it needs to be
        // modified for the instruction handler addresses.
        QByteArray code(codePtr, compiledFunction->codeSize);
#else
        QByteArray code = QByteArray::fromRawData(codePtr, compiledFunction->codeSize);
#endif
        codeRefs[i] = code;
    }

    return true;
}

QQmlRefPointer<CompiledData::CompilationUnit> ISelFactory::createUnitForLoading()
{
    QQmlRefPointer<CompiledData::CompilationUnit> result;
    result.adopt(new Moth::CompilationUnit);
    return result;
}
