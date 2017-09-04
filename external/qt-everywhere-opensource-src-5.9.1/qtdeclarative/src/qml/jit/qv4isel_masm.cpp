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

#include "qv4isel_masm_p.h"
#include "qv4runtime_p.h"
#include "qv4lookup_p.h"
#include "qv4ssa_p.h"
#include "qv4regalloc_p.h"
#include "qv4assembler_p.h"
#include "qv4unop_p.h"
#include "qv4binop_p.h"

#include <QtCore/QBuffer>
#include <QtCore/QCoreApplication>

#include <assembler/LinkBuffer.h>
#include <WTFStubs.h>

#include <iostream>

#if ENABLE(ASSEMBLER)

#if USE(UDIS86)
#  include <udis86.h>
#endif

using namespace QV4;
using namespace QV4::JIT;


template <typename JITAssembler>
InstructionSelection<JITAssembler>::InstructionSelection(QQmlEnginePrivate *qmlEngine, QV4::ExecutableAllocator *execAllocator, IR::Module *module, Compiler::JSUnitGenerator *jsGenerator, EvalISelFactory *iselFactory)
    : EvalInstructionSelection(execAllocator, module, jsGenerator, iselFactory)
    , _block(0)
    , _as(0)
    , compilationUnit(new CompilationUnit)
    , qmlEngine(qmlEngine)
{
    compilationUnit->codeRefs.resize(module->functions.size());
    module->unitFlags |= QV4::CompiledData::Unit::ContainsMachineCode;
}

template <typename JITAssembler>
InstructionSelection<JITAssembler>::~InstructionSelection()
{
    delete _as;
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::run(int functionIndex)
{
    IR::Function *function = irModule->functions[functionIndex];
    qSwap(_function, function);

    IR::Optimizer opt(_function);
    opt.run(qmlEngine);

    static const bool withRegisterAllocator = qEnvironmentVariableIsEmpty("QV4_NO_REGALLOC");
    if (JITTargetPlatform::RegAllocIsSupported && opt.isInSSA() && withRegisterAllocator) {
        RegisterAllocator regalloc(JITTargetPlatform::getRegisterInfo());
        regalloc.run(_function, opt);
        calculateRegistersToSave(regalloc.usedRegisters());
    } else {
        if (opt.isInSSA())
            // No register allocator available for this platform, or env. var was set, so:
            opt.convertOutOfSSA();
        ConvertTemps().toStackSlots(_function);
        IR::Optimizer::showMeTheCode(_function, "After stack slot allocation");
        calculateRegistersToSave(JITTargetPlatform::getRegisterInfo()); // FIXME: this saves all registers. We can probably do with a subset: those that are not used by the register allocator.
    }
    BitVector removableJumps = opt.calculateOptionalJumps();
    qSwap(_removableJumps, removableJumps);

    JITAssembler* oldAssembler = _as;
    _as = new JITAssembler(jsGenerator, _function, executableAllocator);
    _as->setStackLayout(6, // 6 == max argc for calls to built-ins with an argument array
                        regularRegistersToSave.size(),
                        fpRegistersToSave.size());
    _as->enterStandardStackFrame(regularRegistersToSave, fpRegistersToSave);

    if (JITTargetPlatform::RegisterArgumentCount > 0)
        _as->move(_as->registerForArgument(0), JITTargetPlatform::EngineRegister);
    else
        _as->loadPtr(addressForArgument(0), JITTargetPlatform::EngineRegister);

    _as->initializeLocalVariables();

    int lastLine = 0;
    for (int i = 0, ei = _function->basicBlockCount(); i != ei; ++i) {
        IR::BasicBlock *nextBlock = (i < ei - 1) ? _function->basicBlock(i + 1) : 0;
        _block = _function->basicBlock(i);
        if (_block->isRemoved())
            continue;
        _as->registerBlock(_block, nextBlock);

        for (IR::Stmt *s : _block->statements()) {
            if (s->location.isValid()) {
                if (int(s->location.startLine) != lastLine) {
                    _as->loadPtr(Address(JITTargetPlatform::EngineRegister, JITAssembler::targetStructureOffset(offsetof(QV4::EngineBase, current))), JITTargetPlatform::ScratchRegister);
                    Address lineAddr(JITTargetPlatform::ScratchRegister, JITAssembler::targetStructureOffset(Heap::ExecutionContext::baseOffset + offsetof(Heap::ExecutionContextData, lineNumber)));
                    _as->store32(TrustedImm32(s->location.startLine), lineAddr);
                    lastLine = s->location.startLine;
                }
            }
            visit(s);
        }
    }

    if (!_as->exceptionReturnLabel.isSet())
        visitRet(0);

    int dummySize;
    JSC::MacroAssemblerCodeRef codeRef =_as->link(&dummySize);
    compilationUnit->codeRefs[functionIndex] = codeRef;

    qSwap(_function, function);
    delete _as;
    _as = oldAssembler;
    qSwap(_removableJumps, removableJumps);
}

template <typename JITAssembler>
QQmlRefPointer<QV4::CompiledData::CompilationUnit> InstructionSelection<JITAssembler>::backendCompileStep()
{
    QQmlRefPointer<QV4::CompiledData::CompilationUnit> result;
    result.adopt(compilationUnit.take());
    return result;
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinInvalid(IR::Name *func, IR::ExprList *args, IR::Expr *result)
{
    prepareCallData(args, 0);

    if (useFastLookups && func->global) {
        uint index = registerGlobalGetterLookup(*func->id);
        generateRuntimeCall(_as, result, callGlobalLookup,
                             JITTargetPlatform::EngineRegister,
                             TrustedImm32(index),
                             baseAddressForCallData());
    } else {
        generateRuntimeCall(_as, result, callActivationProperty,
                             JITTargetPlatform::EngineRegister,
                             StringToIndex(*func->id),
                             baseAddressForCallData());
    }
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinTypeofQmlContextProperty(IR::Expr *base,
                                                               IR::Member::MemberKind kind,
                                                               int propertyIndex, IR::Expr *result)
{
    if (kind == IR::Member::MemberOfQmlScopeObject) {
        generateRuntimeCall(_as, result, typeofScopeObjectProperty, JITTargetPlatform::EngineRegister,
                             PointerToValue(base),
                             TrustedImm32(propertyIndex));
    } else if (kind == IR::Member::MemberOfQmlContextObject) {
        generateRuntimeCall(_as, result, typeofContextObjectProperty,
                             JITTargetPlatform::EngineRegister, PointerToValue(base),
                             TrustedImm32(propertyIndex));
    } else {
        Q_UNREACHABLE();
    }
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinTypeofMember(IR::Expr *base, const QString &name,
                                                   IR::Expr *result)
{
    generateRuntimeCall(_as, result, typeofMember, JITTargetPlatform::EngineRegister,
                         PointerToValue(base), StringToIndex(name));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinTypeofSubscript(IR::Expr *base, IR::Expr *index,
                                                      IR::Expr *result)
{
    generateRuntimeCall(_as, result, typeofElement,
                         JITTargetPlatform::EngineRegister,
                         PointerToValue(base), PointerToValue(index));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinTypeofName(const QString &name, IR::Expr *result)
{
    generateRuntimeCall(_as, result, typeofName, JITTargetPlatform::EngineRegister,
                         StringToIndex(name));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinTypeofValue(IR::Expr *value, IR::Expr *result)
{
    generateRuntimeCall(_as, result, typeofValue, JITTargetPlatform::EngineRegister,
                         PointerToValue(value));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinDeleteMember(IR::Expr *base, const QString &name, IR::Expr *result)
{
    generateRuntimeCall(_as, result, deleteMember, JITTargetPlatform::EngineRegister,
                         Reference(base), StringToIndex(name));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinDeleteSubscript(IR::Expr *base, IR::Expr *index,
                                                      IR::Expr *result)
{
    generateRuntimeCall(_as, result, deleteElement, JITTargetPlatform::EngineRegister,
                         Reference(base), PointerToValue(index));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinDeleteName(const QString &name, IR::Expr *result)
{
    generateRuntimeCall(_as, result, deleteName, JITTargetPlatform::EngineRegister,
                         StringToIndex(name));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinDeleteValue(IR::Expr *result)
{
    _as->storeValue(JITAssembler::TargetPrimitive::fromBoolean(false), result);
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinThrow(IR::Expr *arg)
{
    generateRuntimeCall(_as, JITTargetPlatform::ReturnValueRegister, throwException, JITTargetPlatform::EngineRegister,
                         PointerToValue(arg));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinReThrow()
{
    _as->jumpToExceptionHandler();
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinUnwindException(IR::Expr *result)
{
    generateRuntimeCall(_as, result, unwindException, JITTargetPlatform::EngineRegister);

}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinPushCatchScope(const QString &exceptionName)
{
    generateRuntimeCall(_as, JITAssembler::Void, pushCatchScope, JITTargetPlatform::EngineRegister, StringToIndex(exceptionName));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinForeachIteratorObject(IR::Expr *arg, IR::Expr *result)
{
    Q_ASSERT(arg);
    Q_ASSERT(result);

    generateRuntimeCall(_as, result, foreachIterator, JITTargetPlatform::EngineRegister, PointerToValue(arg));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinForeachNextPropertyname(IR::Expr *arg, IR::Expr *result)
{
    Q_ASSERT(arg);
    Q_ASSERT(result);

    generateRuntimeCall(_as, result, foreachNextPropertyName, Reference(arg));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinPushWithScope(IR::Expr *arg)
{
    Q_ASSERT(arg);

    generateRuntimeCall(_as, JITAssembler::Void, pushWithScope, Reference(arg), JITTargetPlatform::EngineRegister);
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinPopScope()
{
    generateRuntimeCall(_as, JITAssembler::Void, popScope, JITTargetPlatform::EngineRegister);
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinDeclareVar(bool deletable, const QString &name)
{
    generateRuntimeCall(_as, JITAssembler::Void, declareVar, JITTargetPlatform::EngineRegister,
                         TrustedImm32(deletable), StringToIndex(name));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinDefineArray(IR::Expr *result, IR::ExprList *args)
{
    Q_ASSERT(result);

    int length = prepareVariableArguments(args);
    generateRuntimeCall(_as, result, arrayLiteral, JITTargetPlatform::EngineRegister,
                         baseAddressForCallArguments(), TrustedImm32(length));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinDefineObjectLiteral(IR::Expr *result, int keyValuePairCount, IR::ExprList *keyValuePairs, IR::ExprList *arrayEntries, bool needSparseArray)
{
    Q_ASSERT(result);

    int argc = 0;

    const int classId = registerJSClass(keyValuePairCount, keyValuePairs);

    IR::ExprList *it = keyValuePairs;
    for (int i = 0; i < keyValuePairCount; ++i, it = it->next) {
        it = it->next;

        bool isData = it->expr->asConst()->value;
        it = it->next;

        _as->copyValue(_as->stackLayout().argumentAddressForCall(argc++), it->expr);

        if (!isData) {
            it = it->next;
            _as->copyValue(_as->stackLayout().argumentAddressForCall(argc++), it->expr);
        }
    }

    it = arrayEntries;
    uint arrayValueCount = 0;
    while (it) {
        uint index = it->expr->asConst()->value;
        it = it->next;

        bool isData = it->expr->asConst()->value;
        it = it->next;

        if (!isData) {
            it = it->next; // getter
            it = it->next; // setter
            continue;
        }

        ++arrayValueCount;

        // Index
        _as->storeValue(JITAssembler::TargetPrimitive::fromUInt32(index), _as->stackLayout().argumentAddressForCall(argc++));

        // Value
        _as->copyValue(_as->stackLayout().argumentAddressForCall(argc++), it->expr);
        it = it->next;
    }

    it = arrayEntries;
    uint arrayGetterSetterCount = 0;
    while (it) {
        uint index = it->expr->asConst()->value;
        it = it->next;

        bool isData = it->expr->asConst()->value;
        it = it->next;

        if (isData) {
            it = it->next; // value
            continue;
        }

        ++arrayGetterSetterCount;

        // Index
        _as->storeValue(JITAssembler::TargetPrimitive::fromUInt32(index), _as->stackLayout().argumentAddressForCall(argc++));

        // Getter
        _as->copyValue(_as->stackLayout().argumentAddressForCall(argc++), it->expr);
        it = it->next;

        // Setter
        _as->copyValue(_as->stackLayout().argumentAddressForCall(argc++), it->expr);
        it = it->next;
    }

    generateRuntimeCall(_as, result, objectLiteral, JITTargetPlatform::EngineRegister,
                         baseAddressForCallArguments(), TrustedImm32(classId),
                         TrustedImm32(arrayValueCount), TrustedImm32(arrayGetterSetterCount | (needSparseArray << 30)));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinSetupArgumentObject(IR::Expr *result)
{
    generateRuntimeCall(_as, result, setupArgumentsObject, JITTargetPlatform::EngineRegister);
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callBuiltinConvertThisToObject()
{
    generateRuntimeCall(_as, JITAssembler::Void, convertThisToObject, JITTargetPlatform::EngineRegister);
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callValue(IR::Expr *value, IR::ExprList *args, IR::Expr *result)
{
    Q_ASSERT(value);

    prepareCallData(args, 0);
    if (value->asConst())
        generateRuntimeCall(_as, result, callValue, JITTargetPlatform::EngineRegister,
                             PointerToValue(value),
                             baseAddressForCallData());
    else
        generateRuntimeCall(_as, result, callValue, JITTargetPlatform::EngineRegister,
                             Reference(value),
                             baseAddressForCallData());
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::loadThisObject(IR::Expr *temp)
{
    _as->loadPtr(Address(JITTargetPlatform::EngineRegister, JITAssembler::targetStructureOffset(offsetof(QV4::EngineBase, current))), JITTargetPlatform::ScratchRegister);
    _as->loadPtr(Address(JITTargetPlatform::ScratchRegister, JITAssembler::targetStructureOffset(Heap::ExecutionContext::baseOffset + offsetof(Heap::ExecutionContextData, callData))), JITTargetPlatform::ScratchRegister);
    _as->copyValue(temp, Address(JITTargetPlatform::ScratchRegister, offsetof(CallData, thisObject)));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::loadQmlContext(IR::Expr *temp)
{
    generateRuntimeCall(_as, temp, getQmlContext, JITTargetPlatform::EngineRegister);
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::loadQmlImportedScripts(IR::Expr *temp)
{
    generateRuntimeCall(_as, temp, getQmlImportedScripts, JITTargetPlatform::EngineRegister);
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::loadQmlSingleton(const QString &name, IR::Expr *temp)
{
    generateRuntimeCall(_as, temp, getQmlSingleton, JITTargetPlatform::EngineRegister, StringToIndex(name));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::loadConst(IR::Const *sourceConst, IR::Expr *target)
{
    if (IR::Temp *targetTemp = target->asTemp()) {
        if (targetTemp->kind == IR::Temp::PhysicalRegister) {
            if (targetTemp->type == IR::DoubleType) {
                Q_ASSERT(sourceConst->type == IR::DoubleType);
                _as->toDoubleRegister(sourceConst, (FPRegisterID) targetTemp->index);
            } else if (targetTemp->type == IR::SInt32Type) {
                Q_ASSERT(sourceConst->type == IR::SInt32Type);
                _as->toInt32Register(sourceConst, (RegisterID) targetTemp->index);
            } else if (targetTemp->type == IR::UInt32Type) {
                Q_ASSERT(sourceConst->type == IR::UInt32Type);
                _as->toUInt32Register(sourceConst, (RegisterID) targetTemp->index);
            } else if (targetTemp->type == IR::BoolType) {
                Q_ASSERT(sourceConst->type == IR::BoolType);
                _as->move(TrustedImm32(convertToValue<Primitive>(sourceConst).int_32()),
                          (RegisterID) targetTemp->index);
            } else {
                Q_UNREACHABLE();
            }
            return;
        }
    }

    _as->storeValue(convertToValue<typename JITAssembler::TargetPrimitive>(sourceConst), target);
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::loadString(const QString &str, IR::Expr *target)
{
    Pointer srcAddr = _as->loadStringAddress(JITTargetPlatform::ReturnValueRegister, str);
    _as->loadPtr(srcAddr, JITTargetPlatform::ReturnValueRegister);
    Pointer destAddr = _as->loadAddress(JITTargetPlatform::ScratchRegister, target);
    JITAssembler::RegisterSizeDependentOps::loadManagedPointer(_as, JITTargetPlatform::ReturnValueRegister, destAddr);
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::loadRegexp(IR::RegExp *sourceRegexp, IR::Expr *target)
{
    int id = registerRegExp(sourceRegexp);
    generateRuntimeCall(_as, target, regexpLiteral, JITTargetPlatform::EngineRegister, TrustedImm32(id));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::getActivationProperty(const IR::Name *name, IR::Expr *target)
{
    if (useFastLookups && name->global) {
        uint index = registerGlobalGetterLookup(*name->id);
        generateLookupCall(target, index, offsetof(QV4::Lookup, globalGetter), JITTargetPlatform::EngineRegister, JITAssembler::Void);
        return;
    }
    generateRuntimeCall(_as, target, getActivationProperty, JITTargetPlatform::EngineRegister, StringToIndex(*name->id));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::setActivationProperty(IR::Expr *source, const QString &targetName)
{
    // ### should use a lookup call here
    generateRuntimeCall(_as, JITAssembler::Void, setActivationProperty,
                         JITTargetPlatform::EngineRegister, StringToIndex(targetName), PointerToValue(source));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::initClosure(IR::Closure *closure, IR::Expr *target)
{
    int id = closure->value;
    generateRuntimeCall(_as, target, closure, JITTargetPlatform::EngineRegister, TrustedImm32(id));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::getProperty(IR::Expr *base, const QString &name, IR::Expr *target)
{
    if (useFastLookups) {
        uint index = registerGetterLookup(name);
        generateLookupCall(target, index, offsetof(QV4::Lookup, getter), JITTargetPlatform::EngineRegister, PointerToValue(base), JITAssembler::Void);
    } else {
        generateRuntimeCall(_as, target, getProperty, JITTargetPlatform::EngineRegister,
                             PointerToValue(base), StringToIndex(name));
    }
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::getQmlContextProperty(IR::Expr *base, IR::Member::MemberKind kind, int index, bool captureRequired, IR::Expr *target)
{
    if (kind == IR::Member::MemberOfQmlScopeObject)
        generateRuntimeCall(_as, target, getQmlScopeObjectProperty, JITTargetPlatform::EngineRegister, PointerToValue(base), TrustedImm32(index), TrustedImm32(captureRequired));
    else if (kind == IR::Member::MemberOfQmlContextObject)
        generateRuntimeCall(_as, target, getQmlContextObjectProperty, JITTargetPlatform::EngineRegister, PointerToValue(base), TrustedImm32(index), TrustedImm32(captureRequired));
    else if (kind == IR::Member::MemberOfIdObjectsArray)
        generateRuntimeCall(_as, target, getQmlIdObject, JITTargetPlatform::EngineRegister, PointerToValue(base), TrustedImm32(index));
    else
        Q_ASSERT(false);
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::getQObjectProperty(IR::Expr *base, int propertyIndex, bool captureRequired, bool isSingleton, int attachedPropertiesId, IR::Expr *target)
{
    if (attachedPropertiesId != 0)
        generateRuntimeCall(_as, target, getQmlAttachedProperty, JITTargetPlatform::EngineRegister, TrustedImm32(attachedPropertiesId), TrustedImm32(propertyIndex));
    else if (isSingleton)
        generateRuntimeCall(_as, target, getQmlSingletonQObjectProperty, JITTargetPlatform::EngineRegister, PointerToValue(base), TrustedImm32(propertyIndex),
                             TrustedImm32(captureRequired));
    else
        generateRuntimeCall(_as, target, getQmlQObjectProperty, JITTargetPlatform::EngineRegister, PointerToValue(base), TrustedImm32(propertyIndex),
                             TrustedImm32(captureRequired));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::setProperty(IR::Expr *source, IR::Expr *targetBase,
                                       const QString &targetName)
{
    if (useFastLookups) {
        uint index = registerSetterLookup(targetName);
        generateLookupCall(JITAssembler::Void, index, offsetof(QV4::Lookup, setter),
                           JITTargetPlatform::EngineRegister,
                           PointerToValue(targetBase),
                           PointerToValue(source));
    } else {
        generateRuntimeCall(_as, JITAssembler::Void, setProperty, JITTargetPlatform::EngineRegister,
                             PointerToValue(targetBase), StringToIndex(targetName),
                             PointerToValue(source));
    }
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::setQmlContextProperty(IR::Expr *source, IR::Expr *targetBase, IR::Member::MemberKind kind, int propertyIndex)
{
    if (kind == IR::Member::MemberOfQmlScopeObject)
        generateRuntimeCall(_as, JITAssembler::Void, setQmlScopeObjectProperty, JITTargetPlatform::EngineRegister, PointerToValue(targetBase),
                             TrustedImm32(propertyIndex), PointerToValue(source));
    else if (kind == IR::Member::MemberOfQmlContextObject)
        generateRuntimeCall(_as, JITAssembler::Void, setQmlContextObjectProperty, JITTargetPlatform::EngineRegister, PointerToValue(targetBase),
                             TrustedImm32(propertyIndex), PointerToValue(source));
    else
        Q_ASSERT(false);
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::setQObjectProperty(IR::Expr *source, IR::Expr *targetBase, int propertyIndex)
{
    generateRuntimeCall(_as, JITAssembler::Void, setQmlQObjectProperty, JITTargetPlatform::EngineRegister, PointerToValue(targetBase),
                         TrustedImm32(propertyIndex), PointerToValue(source));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::getElement(IR::Expr *base, IR::Expr *index, IR::Expr *target)
{
    if (0 && useFastLookups) {
        uint lookup = registerIndexedGetterLookup();
        generateLookupCall(target, lookup, offsetof(QV4::Lookup, indexedGetter),
                           PointerToValue(base),
                           PointerToValue(index));
        return;
    }

    generateRuntimeCall(_as, target, getElement, JITTargetPlatform::EngineRegister,
                         PointerToValue(base), PointerToValue(index));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::setElement(IR::Expr *source, IR::Expr *targetBase, IR::Expr *targetIndex)
{
    if (0 && useFastLookups) {
        uint lookup = registerIndexedSetterLookup();
        generateLookupCall(JITAssembler::Void, lookup, offsetof(QV4::Lookup, indexedSetter),
                           PointerToValue(targetBase), PointerToValue(targetIndex),
                           PointerToValue(source));
        return;
    }
    generateRuntimeCall(_as, JITAssembler::Void, setElement, JITTargetPlatform::EngineRegister,
                         PointerToValue(targetBase), PointerToValue(targetIndex),
                         PointerToValue(source));
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::copyValue(IR::Expr *source, IR::Expr *target)
{
    IR::Temp *sourceTemp = source->asTemp();
    IR::Temp *targetTemp = target->asTemp();

    if (sourceTemp && targetTemp && *sourceTemp == *targetTemp)
        return;
    if (IR::ArgLocal *sal = source->asArgLocal())
        if (IR::ArgLocal *tal = target->asArgLocal())
            if (*sal == *tal)
                return;

    if (sourceTemp && sourceTemp->kind == IR::Temp::PhysicalRegister) {
        if (targetTemp && targetTemp->kind == IR::Temp::PhysicalRegister) {
            if (sourceTemp->type == IR::DoubleType)
                _as->moveDouble((FPRegisterID) sourceTemp->index,
                                (FPRegisterID) targetTemp->index);
            else
                _as->move((RegisterID) sourceTemp->index,
                          (RegisterID) targetTemp->index);
            return;
        } else {
            switch (sourceTemp->type) {
            case IR::DoubleType:
                _as->storeDouble((FPRegisterID) sourceTemp->index, target);
                break;
            case IR::SInt32Type:
                _as->storeInt32((RegisterID) sourceTemp->index, target);
                break;
            case IR::UInt32Type:
                _as->storeUInt32((RegisterID) sourceTemp->index, target);
                break;
            case IR::BoolType:
                _as->storeBool((RegisterID) sourceTemp->index, target);
                break;
            default:
                Q_ASSERT(!"Unreachable");
                break;
            }
            return;
        }
    } else if (targetTemp && targetTemp->kind == IR::Temp::PhysicalRegister) {
        switch (targetTemp->type) {
        case IR::DoubleType:
            Q_ASSERT(source->type == IR::DoubleType);
            _as->toDoubleRegister(source, (FPRegisterID) targetTemp->index);
            return;
        case IR::BoolType:
            Q_ASSERT(source->type == IR::BoolType);
            _as->toInt32Register(source, (RegisterID) targetTemp->index);
            return;
        case IR::SInt32Type:
            Q_ASSERT(source->type == IR::SInt32Type);
            _as->toInt32Register(source, (RegisterID) targetTemp->index);
            return;
        case IR::UInt32Type:
            Q_ASSERT(source->type == IR::UInt32Type);
            _as->toUInt32Register(source, (RegisterID) targetTemp->index);
            return;
        default:
            Q_ASSERT(!"Unreachable");
            break;
        }
    }

    // The target is not a physical register, nor is the source. So we can do a memory-to-memory copy:
    _as->memcopyValue(_as->loadAddress(JITTargetPlatform::ReturnValueRegister, target), source, JITTargetPlatform::ScratchRegister);
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::swapValues(IR::Expr *source, IR::Expr *target)
{
    IR::Temp *sourceTemp = source->asTemp();
    IR::Temp *targetTemp = target->asTemp();

    if (sourceTemp && sourceTemp->kind == IR::Temp::PhysicalRegister) {
        if (targetTemp && targetTemp->kind == IR::Temp::PhysicalRegister) {
            Q_ASSERT(sourceTemp->type == targetTemp->type);

            if (sourceTemp->type == IR::DoubleType) {
                _as->moveDouble((FPRegisterID) targetTemp->index, JITTargetPlatform::FPGpr0);
                _as->moveDouble((FPRegisterID) sourceTemp->index,
                                (FPRegisterID) targetTemp->index);
                _as->moveDouble(JITTargetPlatform::FPGpr0, (FPRegisterID) sourceTemp->index);
            } else {
                _as->swap((RegisterID) sourceTemp->index,
                          (RegisterID) targetTemp->index);
            }
            return;
        }
    } else if (!sourceTemp || sourceTemp->kind == IR::Temp::StackSlot) {
        if (!targetTemp || targetTemp->kind == IR::Temp::StackSlot) {
            // Note: a swap for two stack-slots can involve different types.
            Pointer sAddr = _as->loadAddress(JITTargetPlatform::ScratchRegister, source);
            Pointer tAddr = _as->loadAddress(JITTargetPlatform::ReturnValueRegister, target);
            // use the implementation in JSC::MacroAssembler, as it doesn't do bit swizzling
            auto platformAs = static_cast<typename JITAssembler::MacroAssembler*>(_as);
            platformAs->loadDouble(sAddr, JITTargetPlatform::FPGpr0);
            platformAs->loadDouble(tAddr, JITTargetPlatform::FPGpr1);
            platformAs->storeDouble(JITTargetPlatform::FPGpr1, sAddr);
            platformAs->storeDouble(JITTargetPlatform::FPGpr0, tAddr);
            return;
        }
    }

    IR::Expr *memExpr = !sourceTemp || sourceTemp->kind == IR::Temp::StackSlot ? source : target;
    IR::Temp *regTemp = sourceTemp && sourceTemp->kind == IR::Temp::PhysicalRegister ? sourceTemp
                                                                                     : targetTemp;
    Q_ASSERT(memExpr);
    Q_ASSERT(regTemp);

    Pointer addr = _as->loadAddress(JITTargetPlatform::ReturnValueRegister, memExpr);
    if (regTemp->type == IR::DoubleType) {
        _as->loadDouble(addr, JITTargetPlatform::FPGpr0);
        _as->storeDouble((FPRegisterID) regTemp->index, addr);
        _as->moveDouble(JITTargetPlatform::FPGpr0, (FPRegisterID) regTemp->index);
    } else if (regTemp->type == IR::UInt32Type) {
        _as->toUInt32Register(addr, JITTargetPlatform::ScratchRegister);
        _as->storeUInt32((RegisterID) regTemp->index, addr);
        _as->move(JITTargetPlatform::ScratchRegister, (RegisterID) regTemp->index);
    } else {
        _as->load32(addr, JITTargetPlatform::ScratchRegister);
        _as->store32((RegisterID) regTemp->index, addr);
        if (regTemp->type != memExpr->type) {
            addr.offset += 4;
            quint32 tag;
            switch (regTemp->type) {
            case IR::BoolType:
                tag = quint32(JITAssembler::ValueTypeInternal::Boolean);
                break;
            case IR::SInt32Type:
                tag = quint32(JITAssembler::ValueTypeInternal::Integer);
                break;
            default:
                tag = 31337; // bogus value
                Q_UNREACHABLE();
            }
            _as->store32(TrustedImm32(tag), addr);
        }
        _as->move(JITTargetPlatform::ScratchRegister, (RegisterID) regTemp->index);
    }
}

#define setOp(op, opName, operation) \
    do { \
        op = typename JITAssembler::RuntimeCall(QV4::Runtime::operation); opName = "Runtime::" isel_stringIfy(operation); \
        needsExceptionCheck = QV4::Runtime::Method_##operation##_NeedsExceptionCheck; \
    } while (0)
#define setOpContext(op, opName, operation) \
    do { \
        opContext = typename JITAssembler::RuntimeCall(QV4::Runtime::operation); opName = "Runtime::" isel_stringIfy(operation); \
        needsExceptionCheck = QV4::Runtime::Method_##operation##_NeedsExceptionCheck; \
    } while (0)

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::unop(IR::AluOp oper, IR::Expr *source, IR::Expr *target)
{
    QV4::JIT::Unop<JITAssembler> unop(_as, oper);
    unop.generate(source, target);
}


template <typename JITAssembler>
void InstructionSelection<JITAssembler>::binop(IR::AluOp oper, IR::Expr *leftSource, IR::Expr *rightSource, IR::Expr *target)
{
    QV4::JIT::Binop<JITAssembler> binop(_as, oper);
    binop.generate(leftSource, rightSource, target);
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callQmlContextProperty(IR::Expr *base, IR::Member::MemberKind kind, int propertyIndex, IR::ExprList *args, IR::Expr *result)
{
    prepareCallData(args, base);

    if (kind == IR::Member::MemberOfQmlScopeObject)
        generateRuntimeCall(_as, result, callQmlScopeObjectProperty,
                             JITTargetPlatform::EngineRegister,
                             TrustedImm32(propertyIndex),
                             baseAddressForCallData());
    else if (kind == IR::Member::MemberOfQmlContextObject)
        generateRuntimeCall(_as, result, callQmlContextObjectProperty,
                             JITTargetPlatform::EngineRegister,
                             TrustedImm32(propertyIndex),
                             baseAddressForCallData());
    else
        Q_ASSERT(false);
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callProperty(IR::Expr *base, const QString &name, IR::ExprList *args,
                                        IR::Expr *result)
{
    Q_ASSERT(base != 0);

    prepareCallData(args, base);

    if (useFastLookups) {
        uint index = registerGetterLookup(name);
        generateRuntimeCall(_as, result, callPropertyLookup,
                             JITTargetPlatform::EngineRegister,
                             TrustedImm32(index),
                             baseAddressForCallData());
    } else {
        generateRuntimeCall(_as, result, callProperty, JITTargetPlatform::EngineRegister,
                             StringToIndex(name),
                             baseAddressForCallData());
    }
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::callSubscript(IR::Expr *base, IR::Expr *index, IR::ExprList *args,
                                         IR::Expr *result)
{
    Q_ASSERT(base != 0);

    prepareCallData(args, base);
    generateRuntimeCall(_as, result, callElement, JITTargetPlatform::EngineRegister,
                         PointerToValue(index),
                         baseAddressForCallData());
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::convertType(IR::Expr *source, IR::Expr *target)
{
    switch (target->type) {
    case IR::DoubleType:
        convertTypeToDouble(source, target);
        break;
    case IR::BoolType:
        convertTypeToBool(source, target);
        break;
    case IR::SInt32Type:
        convertTypeToSInt32(source, target);
        break;
    case IR::UInt32Type:
        convertTypeToUInt32(source, target);
        break;
    default:
        convertTypeSlowPath(source, target);
        break;
    }
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::convertTypeSlowPath(IR::Expr *source, IR::Expr *target)
{
    Q_ASSERT(target->type != IR::BoolType);

    if (target->type & IR::NumberType)
        unop(IR::OpUPlus, source, target);
    else
        copyValue(source, target);
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::convertTypeToDouble(IR::Expr *source, IR::Expr *target)
{
    switch (source->type) {
    case IR::SInt32Type:
    case IR::BoolType:
    case IR::NullType:
        convertIntToDouble(source, target);
        break;
    case IR::UInt32Type:
        convertUIntToDouble(source, target);
        break;
    case IR::UndefinedType:
        _as->loadDouble(_as->loadAddress(JITTargetPlatform::ScratchRegister, source), JITTargetPlatform::FPGpr0);
        _as->storeDouble(JITTargetPlatform::FPGpr0, target);
        break;
    case IR::StringType:
    case IR::VarType: {
        // load the tag:
        Pointer tagAddr = _as->loadAddress(JITTargetPlatform::ScratchRegister, source);
        tagAddr.offset += 4;
        _as->load32(tagAddr, JITTargetPlatform::ScratchRegister);

        // check if it's an int32:
        Jump isNoInt = _as->branch32(RelationalCondition::NotEqual, JITTargetPlatform::ScratchRegister,
                                     TrustedImm32(quint32(JITAssembler::ValueTypeInternal::Integer)));
        convertIntToDouble(source, target);
        Jump intDone = _as->jump();

        // not an int, check if it's NOT a double:
        isNoInt.link(_as);
        Jump isDbl = _as->generateIsDoubleCheck(JITTargetPlatform::ScratchRegister);

        generateRuntimeCall(_as, target, toDouble, PointerToValue(source));
        Jump noDoubleDone = _as->jump();

        // it is a double:
        isDbl.link(_as);
        Pointer addr2 = _as->loadAddress(JITTargetPlatform::ScratchRegister, source);
        IR::Temp *targetTemp = target->asTemp();
        if (!targetTemp || targetTemp->kind == IR::Temp::StackSlot) {
            _as->memcopyValue(target, addr2, JITTargetPlatform::FPGpr0, JITTargetPlatform::ReturnValueRegister);
        } else {
            _as->loadDouble(addr2, (FPRegisterID) targetTemp->index);
        }

        noDoubleDone.link(_as);
        intDone.link(_as);
    } break;
    default:
        convertTypeSlowPath(source, target);
        break;
    }
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::convertTypeToBool(IR::Expr *source, IR::Expr *target)
{
    IR::Temp *sourceTemp = source->asTemp();
    switch (source->type) {
    case IR::SInt32Type:
    case IR::UInt32Type:
        convertIntToBool(source, target);
        break;
    case IR::DoubleType: {
        // The source is in a register if the register allocator is used. If the register
        // allocator was not used, then that means that we can use any register for to
        // load the double into.
        FPRegisterID reg;
        if (sourceTemp && sourceTemp->kind == IR::Temp::PhysicalRegister)
            reg = (FPRegisterID) sourceTemp->index;
        else
            reg = _as->toDoubleRegister(source, (FPRegisterID) 1);
        Jump nonZero = _as->branchDoubleNonZero(reg, JITTargetPlatform::FPGpr0);

        // it's 0, so false:
        _as->storeBool(false, target);
        Jump done = _as->jump();

        // it's non-zero, so true:
        nonZero.link(_as);
        _as->storeBool(true, target);

        // done:
        done.link(_as);
    } break;
    case IR::UndefinedType:
    case IR::NullType:
        _as->storeBool(false, target);
        break;
    case IR::StringType:
        generateRuntimeCall(_as, JITTargetPlatform::ReturnValueRegister, toBoolean,
                            PointerToValue(source));
        _as->storeBool(JITTargetPlatform::ReturnValueRegister, target);
        Q_FALLTHROUGH();
    case IR::VarType:
    default:
        Pointer addr = _as->loadAddress(JITTargetPlatform::ScratchRegister, source);
        Pointer tagAddr = addr;
        tagAddr.offset += 4;
        _as->load32(tagAddr, JITTargetPlatform::ReturnValueRegister);

        // checkif it's a bool:
        Jump notBool = _as->branch32(RelationalCondition::NotEqual, JITTargetPlatform::ReturnValueRegister,
                                     TrustedImm32(quint32(JITAssembler::ValueTypeInternal::Boolean)));
        _as->load32(addr, JITTargetPlatform::ReturnValueRegister);
        Jump boolDone = _as->jump();
        // check if it's an int32:
        notBool.link(_as);
        Jump fallback = _as->branch32(RelationalCondition::NotEqual, JITTargetPlatform::ReturnValueRegister,
                                      TrustedImm32(quint32(JITAssembler::ValueTypeInternal::Integer)));
        _as->load32(addr, JITTargetPlatform::ReturnValueRegister);
        Jump isZero = _as->branch32(RelationalCondition::Equal, JITTargetPlatform::ReturnValueRegister,
                                               TrustedImm32(0));
        _as->move(TrustedImm32(1), JITTargetPlatform::ReturnValueRegister);
        Jump intDone = _as->jump();

        // not an int:
        fallback.link(_as);
        generateRuntimeCall(_as, JITTargetPlatform::ReturnValueRegister, toBoolean,
                            PointerToValue(source));

        isZero.link(_as);
        intDone.link(_as);
        boolDone.link(_as);
        _as->storeBool(JITTargetPlatform::ReturnValueRegister, target);

        break;
    }
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::convertTypeToSInt32(IR::Expr *source, IR::Expr *target)
{
    switch (source->type) {
    case IR::VarType: {
        JITAssembler::RegisterSizeDependentOps::convertVarToSInt32(_as, source, target);
    } break;
    case IR::DoubleType: {
        Jump success =
                _as->branchTruncateDoubleToInt32(_as->toDoubleRegister(source),
                                                 JITTargetPlatform::ReturnValueRegister,
                                                 BranchTruncateType::BranchIfTruncateSuccessful);
        generateRuntimeCall(_as, JITTargetPlatform::ReturnValueRegister, doubleToInt,
                             PointerToValue(source));
        success.link(_as);
        _as->storeInt32(JITTargetPlatform::ReturnValueRegister, target);
    } break;
    case IR::UInt32Type:
        _as->storeInt32(_as->toUInt32Register(source, JITTargetPlatform::ReturnValueRegister), target);
        break;
    case IR::NullType:
    case IR::UndefinedType:
        _as->move(TrustedImm32(0), JITTargetPlatform::ReturnValueRegister);
        _as->storeInt32(JITTargetPlatform::ReturnValueRegister, target);
        break;
    case IR::BoolType:
        _as->storeInt32(_as->toInt32Register(source, JITTargetPlatform::ReturnValueRegister), target);
        break;
    case IR::StringType:
    default:
        generateRuntimeCall(_as, JITTargetPlatform::ReturnValueRegister, toInt,
                             _as->loadAddress(JITTargetPlatform::ScratchRegister, source));
        _as->storeInt32(JITTargetPlatform::ReturnValueRegister, target);
        break;
    } // switch (source->type)
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::convertTypeToUInt32(IR::Expr *source, IR::Expr *target)
{
    switch (source->type) {
    case IR::VarType: {
        // load the tag:
        Pointer tagAddr = _as->loadAddress(JITTargetPlatform::ScratchRegister, source);
        tagAddr.offset += 4;
        _as->load32(tagAddr, JITTargetPlatform::ScratchRegister);

        // check if it's an int32:
        Jump isNoInt = _as->branch32(RelationalCondition::NotEqual, JITTargetPlatform::ScratchRegister,
                                     TrustedImm32(quint32(JITAssembler::ValueTypeInternal::Integer)));
        Pointer addr = _as->loadAddress(JITTargetPlatform::ScratchRegister, source);
        _as->storeUInt32(_as->toInt32Register(addr, JITTargetPlatform::ScratchRegister), target);
        Jump intDone = _as->jump();

        // not an int:
        isNoInt.link(_as);
        generateRuntimeCall(_as, JITTargetPlatform::ReturnValueRegister, toUInt,
                             _as->loadAddress(JITTargetPlatform::ScratchRegister, source));
        _as->storeInt32(JITTargetPlatform::ReturnValueRegister, target);

        intDone.link(_as);
    } break;
    case IR::DoubleType: {
        FPRegisterID reg = _as->toDoubleRegister(source);
        Jump success =
                _as->branchTruncateDoubleToUint32(reg, JITTargetPlatform::ReturnValueRegister,
                                                  BranchTruncateType::BranchIfTruncateSuccessful);
        generateRuntimeCall(_as, JITTargetPlatform::ReturnValueRegister, doubleToUInt,
                             PointerToValue(source));
        success.link(_as);
        _as->storeUInt32(JITTargetPlatform::ReturnValueRegister, target);
    } break;
    case IR::NullType:
    case IR::UndefinedType:
        _as->move(TrustedImm32(0), JITTargetPlatform::ReturnValueRegister);
        _as->storeUInt32(JITTargetPlatform::ReturnValueRegister, target);
        break;
    case IR::StringType:
        generateRuntimeCall(_as, JITTargetPlatform::ReturnValueRegister, toUInt,
                             PointerToValue(source));
        _as->storeUInt32(JITTargetPlatform::ReturnValueRegister, target);
        break;
    case IR::SInt32Type:
    case IR::BoolType:
        _as->storeUInt32(_as->toInt32Register(source, JITTargetPlatform::ReturnValueRegister), target);
        break;
    default:
        break;
    } // switch (source->type)
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::constructActivationProperty(IR::Name *func, IR::ExprList *args, IR::Expr *result)
{
    Q_ASSERT(func != 0);
    prepareCallData(args, 0);

    if (useFastLookups && func->global) {
        uint index = registerGlobalGetterLookup(*func->id);
        generateRuntimeCall(_as, result, constructGlobalLookup,
                             JITTargetPlatform::EngineRegister,
                             TrustedImm32(index), baseAddressForCallData());
        return;
    }

    generateRuntimeCall(_as, result, constructActivationProperty,
                         JITTargetPlatform::EngineRegister,
                         StringToIndex(*func->id),
                         baseAddressForCallData());
}


template <typename JITAssembler>
void InstructionSelection<JITAssembler>::constructProperty(IR::Expr *base, const QString &name, IR::ExprList *args, IR::Expr *result)
{
    prepareCallData(args, base);
    if (useFastLookups) {
        uint index = registerGetterLookup(name);
        generateRuntimeCall(_as, result, constructPropertyLookup,
                             JITTargetPlatform::EngineRegister,
                             TrustedImm32(index),
                             baseAddressForCallData());
        return;
    }

    generateRuntimeCall(_as, result, constructProperty, JITTargetPlatform::EngineRegister,
                         StringToIndex(name),
                         baseAddressForCallData());
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::constructValue(IR::Expr *value, IR::ExprList *args, IR::Expr *result)
{
    Q_ASSERT(value != 0);

    prepareCallData(args, 0);
    generateRuntimeCall(_as, result, constructValue,
                         JITTargetPlatform::EngineRegister,
                         Reference(value),
                         baseAddressForCallData());
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::visitJump(IR::Jump *s)
{
    if (!_removableJumps.at(_block->index()))
        _as->jumpToBlock(_block, s->target);
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::visitCJump(IR::CJump *s)
{
    IR::Temp *t = s->cond->asTemp();
    if (t || s->cond->asArgLocal()) {
        RegisterID reg;
        if (t && t->kind == IR::Temp::PhysicalRegister) {
            Q_ASSERT(t->type == IR::BoolType);
            reg = (RegisterID) t->index;
        } else if (t && t->kind == IR::Temp::StackSlot && t->type == IR::BoolType) {
            reg = JITTargetPlatform::ReturnValueRegister;
            _as->toInt32Register(t, reg);
        } else {
            Address temp = _as->loadAddress(JITTargetPlatform::ScratchRegister, s->cond);
            Address tag = temp;
            tag.offset += QV4::Value::tagOffset();
            Jump booleanConversion = _as->branch32(RelationalCondition::NotEqual, tag,
                                                   TrustedImm32(quint32(JITAssembler::ValueTypeInternal::Boolean)));

            Address data = temp;
            data.offset += QV4::Value::valueOffset();
            _as->load32(data, JITTargetPlatform::ReturnValueRegister);
            Jump testBoolean = _as->jump();

            booleanConversion.link(_as);
            reg = JITTargetPlatform::ReturnValueRegister;
            generateRuntimeCall(_as, reg, toBoolean, Reference(s->cond));

            testBoolean.link(_as);
        }

        _as->generateCJumpOnNonZero(reg, _block, s->iftrue, s->iffalse);
        return;
    } else if (IR::Const *c = s->cond->asConst()) {
        // TODO: SSA optimization for constant condition evaluation should remove this.
        // See also visitCJump() in RegAllocInfo.
        generateRuntimeCall(_as, JITTargetPlatform::ReturnValueRegister, toBoolean,
                             PointerToValue(c));
        _as->generateCJumpOnNonZero(JITTargetPlatform::ReturnValueRegister, _block, s->iftrue, s->iffalse);
        return;
    } else if (IR::Binop *b = s->cond->asBinop()) {
        if (b->left->type == IR::DoubleType && b->right->type == IR::DoubleType
                && visitCJumpDouble(b->op, b->left, b->right, s->iftrue, s->iffalse))
            return;

        if (b->left->type == IR::SInt32Type && b->right->type == IR::SInt32Type
                && visitCJumpSInt32(b->op, b->left, b->right, s->iftrue, s->iffalse))
            return;

        if (b->op == IR::OpStrictEqual || b->op == IR::OpStrictNotEqual) {
            visitCJumpStrict(b, s->iftrue, s->iffalse);
            return;
        }
        if (b->op == IR::OpEqual || b->op == IR::OpNotEqual) {
            visitCJumpEqual(b, s->iftrue, s->iffalse);
            return;
        }

        typename JITAssembler::RuntimeCall op;
        typename JITAssembler::RuntimeCall opContext;
        const char *opName = 0;
        bool needsExceptionCheck;
        switch (b->op) {
        default: Q_UNREACHABLE(); Q_ASSERT(!"todo"); break;
        case IR::OpGt: setOp(op, opName, compareGreaterThan); break;
        case IR::OpLt: setOp(op, opName, compareLessThan); break;
        case IR::OpGe: setOp(op, opName, compareGreaterEqual); break;
        case IR::OpLe: setOp(op, opName, compareLessEqual); break;
        case IR::OpEqual: setOp(op, opName, compareEqual); break;
        case IR::OpNotEqual: setOp(op, opName, compareNotEqual); break;
        case IR::OpStrictEqual: setOp(op, opName, compareStrictEqual); break;
        case IR::OpStrictNotEqual: setOp(op, opName, compareStrictNotEqual); break;
        case IR::OpInstanceof: setOpContext(op, opName, compareInstanceof); break;
        case IR::OpIn: setOpContext(op, opName, compareIn); break;
        } // switch

        // TODO: in SSA optimization, do constant expression evaluation.
        // The case here is, for example:
        //   if (true === true) .....
        // Of course, after folding the CJUMP to a JUMP, dead-code (dead-basic-block)
        // elimination (which isn't there either) would remove the whole else block.
        if (opContext.isValid())
            _as->generateFunctionCallImp(needsExceptionCheck,
                                         JITTargetPlatform::ReturnValueRegister, opName, opContext,
                                         JITTargetPlatform::EngineRegister,
                                         PointerToValue(b->left),
                                         PointerToValue(b->right));
        else
            _as->generateFunctionCallImp(needsExceptionCheck,
                                         JITTargetPlatform::ReturnValueRegister, opName, op,
                                         PointerToValue(b->left),
                                         PointerToValue(b->right));

        _as->generateCJumpOnNonZero(JITTargetPlatform::ReturnValueRegister, _block, s->iftrue, s->iffalse);
        return;
    }
    Q_UNREACHABLE();
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::visitRet(IR::Ret *s)
{
    _as->returnFromFunction(s, regularRegistersToSave, fpRegistersToSave);
}

template <typename JITAssembler>
int InstructionSelection<JITAssembler>::prepareVariableArguments(IR::ExprList* args)
{
    int argc = 0;
    for (IR::ExprList *it = args; it; it = it->next) {
        ++argc;
    }

    int i = 0;
    for (IR::ExprList *it = args; it; it = it->next, ++i) {
        IR::Expr *arg = it->expr;
        Q_ASSERT(arg != 0);
        Pointer dst(_as->stackLayout().argumentAddressForCall(i));
        if (arg->asTemp() && arg->asTemp()->kind != IR::Temp::PhysicalRegister)
            _as->memcopyValue(dst, arg->asTemp(), JITTargetPlatform::ScratchRegister);
        else
            _as->copyValue(dst, arg);
    }

    return argc;
}

template <typename JITAssembler>
int InstructionSelection<JITAssembler>::prepareCallData(IR::ExprList* args, IR::Expr *thisObject)
{
    int argc = 0;
    for (IR::ExprList *it = args; it; it = it->next) {
        ++argc;
    }

    Pointer p = _as->stackLayout().callDataAddress(offsetof(CallData, tag));
    _as->store32(TrustedImm32(quint32(JITAssembler::ValueTypeInternal::Integer)), p);
    p = _as->stackLayout().callDataAddress(offsetof(CallData, argc));
    _as->store32(TrustedImm32(argc), p);
    p = _as->stackLayout().callDataAddress(offsetof(CallData, thisObject));
    if (!thisObject)
        _as->storeValue(JITAssembler::TargetPrimitive::undefinedValue(), p);
    else
        _as->copyValue(p, thisObject);

    int i = 0;
    for (IR::ExprList *it = args; it; it = it->next, ++i) {
        IR::Expr *arg = it->expr;
        Q_ASSERT(arg != 0);
        Pointer dst(_as->stackLayout().argumentAddressForCall(i));
        if (arg->asTemp() && arg->asTemp()->kind != IR::Temp::PhysicalRegister)
            _as->memcopyValue(dst, arg->asTemp(), JITTargetPlatform::ScratchRegister);
        else
            _as->copyValue(dst, arg);
    }
    return argc;
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::calculateRegistersToSave(const RegisterInformation &used)
{
    regularRegistersToSave.clear();
    fpRegistersToSave.clear();

    for (const RegisterInfo &ri : JITTargetPlatform::getRegisterInfo()) {
        if (JITTargetPlatform::gotRegister != -1 && ri.isRegularRegister() && ri.reg<RegisterID>() == JITTargetPlatform::gotRegister) {
            regularRegistersToSave.append(ri);
            continue;
        }
        if (ri.isCallerSaved())
            continue;
        if (ri.isRegularRegister()) {
            if (ri.isPredefined() || used.contains(ri))
                regularRegistersToSave.append(ri);
        } else {
            Q_ASSERT(ri.isFloatingPoint());
            if (ri.isPredefined() || used.contains(ri))
                fpRegistersToSave.append(ri);
        }
    }
}

QT_BEGIN_NAMESPACE
namespace QV4 {
bool operator==(const Primitive &v1, const Primitive &v2)
{
    return v1.rawValue() == v2.rawValue();
}
} // QV4 namespace
QT_END_NAMESPACE

template <typename JITAssembler>
bool InstructionSelection<JITAssembler>::visitCJumpDouble(IR::AluOp op, IR::Expr *left, IR::Expr *right,
                                            IR::BasicBlock *iftrue, IR::BasicBlock *iffalse)
{
    if (_as->nextBlock() == iftrue) {
        Jump target = _as->branchDouble(true, op, left, right);
        _as->addPatch(iffalse, target);
    } else {
        Jump target = _as->branchDouble(false, op, left, right);
        _as->addPatch(iftrue, target);
        _as->jumpToBlock(_block, iffalse);
    }
    return true;
}

template <typename JITAssembler>
bool InstructionSelection<JITAssembler>::visitCJumpSInt32(IR::AluOp op, IR::Expr *left, IR::Expr *right,
                                            IR::BasicBlock *iftrue, IR::BasicBlock *iffalse)
{
    if (_as->nextBlock() == iftrue) {
        Jump target = _as->branchInt32(true, op, left, right);
        _as->addPatch(iffalse, target);
    } else {
        Jump target = _as->branchInt32(false, op, left, right);
        _as->addPatch(iftrue, target);
        _as->jumpToBlock(_block, iffalse);
    }
    return true;
}

template <typename JITAssembler>
void InstructionSelection<JITAssembler>::visitCJumpStrict(IR::Binop *binop, IR::BasicBlock *trueBlock,
                                            IR::BasicBlock *falseBlock)
{
    Q_ASSERT(binop->op == IR::OpStrictEqual || binop->op == IR::OpStrictNotEqual);

    if (visitCJumpStrictNull(binop, trueBlock, falseBlock))
        return;
    if (visitCJumpStrictUndefined(binop, trueBlock, falseBlock))
        return;
    if (visitCJumpStrictBool(binop, trueBlock, falseBlock))
        return;

    IR::Expr *left = binop->left;
    IR::Expr *right = binop->right;

    generateRuntimeCall(_as, JITTargetPlatform::ReturnValueRegister, compareStrictEqual,
                                 PointerToValue(left), PointerToValue(right));
    _as->generateCJumpOnCompare(binop->op == IR::OpStrictEqual ? RelationalCondition::NotEqual : RelationalCondition::Equal,
                                JITTargetPlatform::ReturnValueRegister, TrustedImm32(0),
                                _block, trueBlock, falseBlock);
}

// Only load the non-null temp.
template <typename JITAssembler>
bool InstructionSelection<JITAssembler>::visitCJumpStrictNull(IR::Binop *binop,
                                                IR::BasicBlock *trueBlock,
                                                IR::BasicBlock *falseBlock)
{
    IR::Expr *varSrc = 0;
    if (binop->left->type == IR::VarType && binop->right->type == IR::NullType)
        varSrc = binop->left;
    else if (binop->left->type == IR::NullType && binop->right->type == IR::VarType)
        varSrc = binop->right;
    if (!varSrc)
        return false;

    if (varSrc->asTemp() && varSrc->asTemp()->kind == IR::Temp::PhysicalRegister) {
        _as->jumpToBlock(_block, falseBlock);
        return true;
    }

    if (IR::Const *c = varSrc->asConst()) {
        if (c->type == IR::NullType)
            _as->jumpToBlock(_block, trueBlock);
        else
            _as->jumpToBlock(_block, falseBlock);
        return true;
    }

    Pointer tagAddr = _as->loadAddress(JITTargetPlatform::ScratchRegister, varSrc);
    tagAddr.offset += 4;
    const RegisterID tagReg = JITTargetPlatform::ScratchRegister;
    _as->load32(tagAddr, tagReg);

    RelationalCondition cond = binop->op == IR::OpStrictEqual ? RelationalCondition::Equal
                                                                         : RelationalCondition::NotEqual;
    const TrustedImm32 tag{quint32(JITAssembler::ValueTypeInternal::Null)};
    _as->generateCJumpOnCompare(cond, tagReg, tag, _block, trueBlock, falseBlock);
    return true;
}

template <typename JITAssembler>
bool InstructionSelection<JITAssembler>::visitCJumpStrictUndefined(IR::Binop *binop,
                                                     IR::BasicBlock *trueBlock,
                                                     IR::BasicBlock *falseBlock)
{
    IR::Expr *varSrc = 0;
    if (binop->left->type == IR::VarType && binop->right->type == IR::UndefinedType)
        varSrc = binop->left;
    else if (binop->left->type == IR::UndefinedType && binop->right->type == IR::VarType)
        varSrc = binop->right;
    if (!varSrc)
        return false;

    if (varSrc->asTemp() && varSrc->asTemp()->kind == IR::Temp::PhysicalRegister) {
        _as->jumpToBlock(_block, falseBlock);
        return true;
    }

    if (IR::Const *c = varSrc->asConst()) {
        if (c->type == IR::UndefinedType)
            _as->jumpToBlock(_block, trueBlock);
        else
            _as->jumpToBlock(_block, falseBlock);
        return true;
    }

    RelationalCondition cond = binop->op == IR::OpStrictEqual ? RelationalCondition::Equal
                                                                         : RelationalCondition::NotEqual;
    const RegisterID tagReg = JITTargetPlatform::ReturnValueRegister;
    _as->generateCJumpOnUndefined(cond, varSrc, JITTargetPlatform::ScratchRegister, tagReg, _block, trueBlock, falseBlock);
    return true;
}

template <typename JITAssembler>
bool InstructionSelection<JITAssembler>::visitCJumpStrictBool(IR::Binop *binop, IR::BasicBlock *trueBlock,
                                                IR::BasicBlock *falseBlock)
{
    IR::Expr *boolSrc = 0, *otherSrc = 0;
    if (binop->left->type == IR::BoolType) {
        boolSrc = binop->left;
        otherSrc = binop->right;
    } else if (binop->right->type == IR::BoolType) {
        boolSrc = binop->right;
        otherSrc = binop->left;
    } else {
        // neither operands are statically typed as bool, so bail out.
        return false;
    }
    if (otherSrc->type == IR::UnknownType) {
        // Ok, we really need to call into the runtime.
        // (This case doesn't happen when the optimizer ran, because everything will be typed (yes,
        // possibly as "var" meaning anything), but it does happen for $0===true, which is generated
        // for things where the optimizer didn't run (like functions with a try block).)
        return false;
    }

    RelationalCondition cond = binop->op == IR::OpStrictEqual ? RelationalCondition::Equal
                                                              : RelationalCondition::NotEqual;

    if (otherSrc->type == IR::BoolType) { // both are boolean
        RegisterID one = _as->toBoolRegister(boolSrc, JITTargetPlatform::ReturnValueRegister);
        RegisterID two = _as->toBoolRegister(otherSrc, JITTargetPlatform::ScratchRegister);
        _as->generateCJumpOnCompare(cond, one, two, _block, trueBlock, falseBlock);
        return true;
    }

    if (otherSrc->type != IR::VarType) {
        _as->jumpToBlock(_block, falseBlock);
        return true;
    }

    Pointer otherAddr = _as->loadAddress(JITTargetPlatform::ReturnValueRegister, otherSrc);
    otherAddr.offset += 4; // tag address

    // check if the tag of the var operand is indicates 'boolean'
    _as->load32(otherAddr, JITTargetPlatform::ScratchRegister);
    Jump noBool = _as->branch32(RelationalCondition::NotEqual, JITTargetPlatform::ScratchRegister,
                                TrustedImm32(quint32(JITAssembler::ValueTypeInternal::Boolean)));
    if (binop->op == IR::OpStrictEqual)
        _as->addPatch(falseBlock, noBool);
    else
        _as->addPatch(trueBlock, noBool);

    // ok, both are boolean, so let's load them and compare them.
    otherAddr.offset -= 4; // int_32 address
    _as->load32(otherAddr, JITTargetPlatform::ReturnValueRegister);
    RegisterID boolReg = _as->toBoolRegister(boolSrc, JITTargetPlatform::ScratchRegister);
    _as->generateCJumpOnCompare(cond, boolReg, JITTargetPlatform::ReturnValueRegister, _block, trueBlock,
                                falseBlock);
    return true;
}

template <typename JITAssembler>
bool InstructionSelection<JITAssembler>::visitCJumpNullUndefined(IR::Type nullOrUndef, IR::Binop *binop,
                                                         IR::BasicBlock *trueBlock,
                                                         IR::BasicBlock *falseBlock)
{
    Q_ASSERT(nullOrUndef == IR::NullType || nullOrUndef == IR::UndefinedType);

    IR::Expr *varSrc = 0;
    if (binop->left->type == IR::VarType && binop->right->type == nullOrUndef)
        varSrc = binop->left;
    else if (binop->left->type == nullOrUndef && binop->right->type == IR::VarType)
        varSrc = binop->right;
    if (!varSrc)
        return false;

    if (varSrc->asTemp() && varSrc->asTemp()->kind == IR::Temp::PhysicalRegister) {
        _as->jumpToBlock(_block, falseBlock);
        return true;
    }

    if (IR::Const *c = varSrc->asConst()) {
        if (c->type == nullOrUndef)
            _as->jumpToBlock(_block, trueBlock);
        else
            _as->jumpToBlock(_block, falseBlock);
        return true;
    }

    Pointer tagAddr = _as->loadAddress(JITTargetPlatform::ScratchRegister, varSrc);
    tagAddr.offset += 4;
    const RegisterID tagReg = JITTargetPlatform::ReturnValueRegister;
    _as->load32(tagAddr, tagReg);

    if (binop->op == IR::OpNotEqual)
        qSwap(trueBlock, falseBlock);
    Jump isNull = _as->branch32(RelationalCondition::Equal, tagReg, TrustedImm32(quint32(JITAssembler::ValueTypeInternal::Null)));
    Jump isNotUndefinedTag = _as->branch32(RelationalCondition::NotEqual, tagReg, TrustedImm32(int(QV4::Value::Managed_Type_Internal)));
    tagAddr.offset -= 4;
    _as->load32(tagAddr, tagReg);
    Jump isNotUndefinedValue = _as->branch32(RelationalCondition::NotEqual, tagReg, TrustedImm32(0));
    _as->addPatch(trueBlock, isNull);
    _as->addPatch(falseBlock, isNotUndefinedTag);
    _as->addPatch(falseBlock, isNotUndefinedValue);
    _as->jumpToBlock(_block, trueBlock);

    return true;
}


template <typename JITAssembler>
void InstructionSelection<JITAssembler>::visitCJumpEqual(IR::Binop *binop, IR::BasicBlock *trueBlock,
                                            IR::BasicBlock *falseBlock)
{
    Q_ASSERT(binop->op == IR::OpEqual || binop->op == IR::OpNotEqual);

    if (visitCJumpNullUndefined(IR::NullType, binop, trueBlock, falseBlock))
        return;

    IR::Expr *left = binop->left;
    IR::Expr *right = binop->right;

    generateRuntimeCall(_as, JITTargetPlatform::ReturnValueRegister, compareEqual,
                                 PointerToValue(left), PointerToValue(right));
    _as->generateCJumpOnCompare(binop->op == IR::OpEqual ? RelationalCondition::NotEqual : RelationalCondition::Equal,
                                JITTargetPlatform::ReturnValueRegister, TrustedImm32(0),
                                _block, trueBlock, falseBlock);
}

template <typename JITAssembler>
QQmlRefPointer<CompiledData::CompilationUnit> ISelFactory<JITAssembler>::createUnitForLoading()
{
    QQmlRefPointer<CompiledData::CompilationUnit> result;
    result.adopt(new JIT::CompilationUnit);
    return result;
}

#endif // ENABLE(ASSEMBLER)

QT_BEGIN_NAMESPACE
namespace QV4 { namespace JIT {
#if ENABLE(ASSEMBLER)
template class Q_QML_EXPORT InstructionSelection<>;
template class Q_QML_EXPORT ISelFactory<>;
#endif

#if defined(V4_BOOTSTRAP)

Q_QML_EXPORT QV4::EvalISelFactory *createISelForArchitecture(const QString &architecture)
{
#if ENABLE(ASSEMBLER)
    using ARMv7CrossAssembler = QV4::JIT::Assembler<AssemblerTargetConfiguration<JSC::MacroAssemblerARMv7, NoOperatingSystemSpecialization>>;
    using ARM64CrossAssembler = QV4::JIT::Assembler<AssemblerTargetConfiguration<JSC::MacroAssemblerARM64, NoOperatingSystemSpecialization>>;

    if (architecture == QLatin1String("arm"))
        return new ISelFactory<ARMv7CrossAssembler>;
    else if (architecture == QLatin1String("arm64"))
        return new ISelFactory<ARM64CrossAssembler>;

    QString hostArch;
#if CPU(ARM_THUMB2)
    hostArch = QStringLiteral("arm");
#elif CPU(MIPS)
    hostArch = QStringLiteral("mips");
#elif CPU(X86)
    hostArch = QStringLiteral("i386");
#elif CPU(X86_64)
    hostArch = QStringLiteral("x86_64");
#endif
    if (!hostArch.isEmpty() && architecture == hostArch)
        return new ISelFactory<>;
#endif // ENABLE(ASSEMBLER)

    return nullptr;
}

#endif
} }
QT_END_NAMESPACE

