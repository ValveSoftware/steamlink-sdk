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
#include "qv4object_p.h"
#include "qv4functionobject_p.h"
#include "qv4regexpobject_p.h"
#include "qv4lookup_p.h"
#include "qv4function_p.h"
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


namespace {
inline bool isPregOrConst(IR::Expr *e)
{
    if (IR::Temp *t = e->asTemp())
        return t->kind == IR::Temp::PhysicalRegister;
    return e->asConst() != 0;
}

class QIODevicePrintStream: public FilePrintStream
{
    Q_DISABLE_COPY(QIODevicePrintStream)

public:
    explicit QIODevicePrintStream(QIODevice *dest)
        : FilePrintStream(0)
        , dest(dest)
        , buf(4096, '0')
    {
        Q_ASSERT(dest);
    }

    ~QIODevicePrintStream()
    {}

    void vprintf(const char* format, va_list argList) WTF_ATTRIBUTE_PRINTF(2, 0)
    {
        const int written = qvsnprintf(buf.data(), buf.size(), format, argList);
        if (written > 0)
            dest->write(buf.constData(), written);
        memset(buf.data(), 0, qMin(written, buf.size()));
    }

    void flush()
    {}

private:
    QIODevice *dest;
    QByteArray buf;
};
} // anonymous namespace

static void printDisassembledOutputWithCalls(QByteArray processedOutput, const QHash<void*, const char*>& functions)
{
    for (QHash<void*, const char*>::ConstIterator it = functions.begin(), end = functions.end();
         it != end; ++it) {
        QByteArray ptrString = QByteArray::number(quintptr(it.key()), 16);
        ptrString.prepend("0x");
        int idx = processedOutput.indexOf(ptrString);
        if (idx < 0)
            continue;
        idx = processedOutput.lastIndexOf('\n', idx);
        if (idx < 0)
            continue;
        processedOutput = processedOutput.insert(idx, QByteArrayLiteral("                          ; call ") + it.value());
    }

    qDebug("%s", processedOutput.constData());
}

#if defined(Q_OS_LINUX)
static FILE *pmap;

static void qt_closePmap()
{
    if (pmap) {
        fclose(pmap);
        pmap = 0;
    }
}

#endif

JSC::MacroAssemblerCodeRef Assembler::link(int *codeSize)
{
    Label endOfCode = label();

    {
        for (size_t i = 0, ei = _patches.size(); i != ei; ++i) {
            Label target = _addrs.at(i);
            Q_ASSERT(target.isSet());
            for (Jump jump : qAsConst(_patches.at(i)))
                jump.linkTo(target, this);
        }
    }

    JSC::JSGlobalData dummy(_executableAllocator);
    JSC::LinkBuffer linkBuffer(dummy, this, 0);

    for (const DataLabelPatch &p : qAsConst(_dataLabelPatches))
        linkBuffer.patch(p.dataLabel, linkBuffer.locationOf(p.target));

    // link exception handlers
    for (Jump jump : qAsConst(exceptionPropagationJumps))
        linkBuffer.link(jump, linkBuffer.locationOf(exceptionReturnLabel));

    {
        for (size_t i = 0, ei = _labelPatches.size(); i != ei; ++i) {
            Label target = _addrs.at(i);
            Q_ASSERT(target.isSet());
            for (DataLabelPtr label : _labelPatches.at(i))
                linkBuffer.patch(label, linkBuffer.locationOf(target));
        }
    }

    *codeSize = linkBuffer.offsetOf(endOfCode);

    QByteArray name;

    JSC::MacroAssemblerCodeRef codeRef;

    static const bool showCode = qEnvironmentVariableIsSet("QV4_SHOW_ASM");
    if (showCode) {
        QHash<void*, const char*> functions;
#ifndef QT_NO_DEBUG
        for (CallInfo call : qAsConst(_callInfos))
            functions[linkBuffer.locationOf(call.label).dataLocation()] = call.functionName;
#endif

        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        WTF::setDataFile(new QIODevicePrintStream(&buf));

        name = _function->name->toUtf8();
        if (name.isEmpty()) {
            name = QByteArray::number(quintptr(_function), 16);
            name.prepend("IR::Function(0x");
            name.append(')');
        }
        codeRef = linkBuffer.finalizeCodeWithDisassembly("%s", name.data());

        WTF::setDataFile(stderr);
        printDisassembledOutputWithCalls(buf.data(), functions);
    } else {
        codeRef = linkBuffer.finalizeCodeWithoutDisassembly();
    }

#if defined(Q_OS_LINUX)
    // This implements writing of JIT'd addresses so that perf can find the
    // symbol names.
    //
    // Perf expects the mapping to be in a certain place and have certain
    // content, for more information, see:
    // https://github.com/torvalds/linux/blob/master/tools/perf/Documentation/jit-interface.txt
    static bool doProfile = !qEnvironmentVariableIsEmpty("QV4_PROFILE_WRITE_PERF_MAP");
    static bool profileInitialized = false;
    if (doProfile && !profileInitialized) {
        profileInitialized = true;

        char pname[PATH_MAX];
        snprintf(pname, PATH_MAX - 1, "/tmp/perf-%lu.map",
                                      (unsigned long)QCoreApplication::applicationPid());

        pmap = fopen(pname, "w");
        if (!pmap)
            qWarning("QV4: Can't write %s, call stacks will not contain JavaScript function names", pname);

        // make sure we clean up nicely
        std::atexit(qt_closePmap);
    }

    if (pmap) {
        // this may have been pre-populated, if QV4_SHOW_ASM was on
        if (name.isEmpty()) {
            name = _function->name->toUtf8();
            if (name.isEmpty()) {
                name = QByteArray::number(quintptr(_function), 16);
                name.prepend("IR::Function(0x");
                name.append(')');
            }
        }

        fprintf(pmap, "%llx %x %.*s\n",
                      (long long unsigned int)codeRef.code().executableAddress(),
                      *codeSize,
                      name.length(),
                      name.constData());
        fflush(pmap);
    }
#endif

    return codeRef;
}

InstructionSelection::InstructionSelection(QQmlEnginePrivate *qmlEngine, QV4::ExecutableAllocator *execAllocator, IR::Module *module, Compiler::JSUnitGenerator *jsGenerator, EvalISelFactory *iselFactory)
    : EvalInstructionSelection(execAllocator, module, jsGenerator, iselFactory)
    , _block(0)
    , _as(0)
    , compilationUnit(new CompilationUnit)
    , qmlEngine(qmlEngine)
{
    compilationUnit->codeRefs.resize(module->functions.size());
    module->unitFlags |= QV4::CompiledData::Unit::ContainsMachineCode;
}

InstructionSelection::~InstructionSelection()
{
    delete _as;
}

void InstructionSelection::run(int functionIndex)
{
    IR::Function *function = irModule->functions[functionIndex];
    qSwap(_function, function);

    IR::Optimizer opt(_function);
    opt.run(qmlEngine);

    static const bool withRegisterAllocator = qEnvironmentVariableIsEmpty("QV4_NO_REGALLOC");
    if (Assembler::RegAllocIsSupported && opt.isInSSA() && withRegisterAllocator) {
        RegisterAllocator regalloc(Assembler::getRegisterInfo());
        regalloc.run(_function, opt);
        calculateRegistersToSave(regalloc.usedRegisters());
    } else {
        if (opt.isInSSA())
            // No register allocator available for this platform, or env. var was set, so:
            opt.convertOutOfSSA();
        ConvertTemps().toStackSlots(_function);
        IR::Optimizer::showMeTheCode(_function, "After stack slot allocation");
        calculateRegistersToSave(Assembler::getRegisterInfo()); // FIXME: this saves all registers. We can probably do with a subset: those that are not used by the register allocator.
    }
    QSet<IR::Jump *> removableJumps = opt.calculateOptionalJumps();
    qSwap(_removableJumps, removableJumps);

    Assembler* oldAssembler = _as;
    _as = new Assembler(this, _function, executableAllocator);
    _as->setStackLayout(6, // 6 == max argc for calls to built-ins with an argument array
                        regularRegistersToSave.size(),
                        fpRegistersToSave.size());
    _as->enterStandardStackFrame(regularRegistersToSave, fpRegistersToSave);

#ifdef ARGUMENTS_IN_REGISTERS
    _as->move(_as->registerForArgument(0), Assembler::EngineRegister);
#else
    _as->loadPtr(addressForArgument(0), Assembler::EngineRegister);
#endif

    const int locals = _as->stackLayout().calculateJSStackFrameSize();
    if (locals > 0) {
        _as->loadPtr(Address(Assembler::EngineRegister, qOffsetOf(ExecutionEngine, jsStackTop)), Assembler::LocalsRegister);
#ifdef VALUE_FITS_IN_REGISTER
        _as->move(Assembler::TrustedImm64(0), Assembler::ReturnValueRegister);
        _as->move(Assembler::TrustedImm32(locals), Assembler::ScratchRegister);
        Assembler::Label loop = _as->label();
        _as->store64(Assembler::ReturnValueRegister, Assembler::Address(Assembler::LocalsRegister));
        _as->add64(Assembler::TrustedImm32(8), Assembler::LocalsRegister);
        Assembler::Jump jump = _as->branchSub32(Assembler::NonZero, Assembler::TrustedImm32(1), Assembler::ScratchRegister);
        jump.linkTo(loop, _as);
#else
        _as->move(Assembler::TrustedImm32(0), Assembler::ReturnValueRegister);
        _as->move(Assembler::TrustedImm32(locals), Assembler::ScratchRegister);
        Assembler::Label loop = _as->label();
        _as->store32(Assembler::ReturnValueRegister, Assembler::Address(Assembler::LocalsRegister));
        _as->add32(Assembler::TrustedImm32(4), Assembler::LocalsRegister);
        _as->store32(Assembler::ReturnValueRegister, Assembler::Address(Assembler::LocalsRegister));
        _as->add32(Assembler::TrustedImm32(4), Assembler::LocalsRegister);
        Assembler::Jump jump = _as->branchSub32(Assembler::NonZero, Assembler::TrustedImm32(1), Assembler::ScratchRegister);
        jump.linkTo(loop, _as);
#endif
        _as->storePtr(Assembler::LocalsRegister, Address(Assembler::EngineRegister, qOffsetOf(ExecutionEngine, jsStackTop)));
    }


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
                    _as->loadPtr(Address(Assembler::EngineRegister, qOffsetOf(QV4::ExecutionEngine, current)), Assembler::ScratchRegister);
                    Assembler::Address lineAddr(Assembler::ScratchRegister, qOffsetOf(QV4::ExecutionContext::Data, lineNumber));
                    _as->store32(Assembler::TrustedImm32(s->location.startLine), lineAddr);
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

QQmlRefPointer<QV4::CompiledData::CompilationUnit> InstructionSelection::backendCompileStep()
{
    QQmlRefPointer<QV4::CompiledData::CompilationUnit> result;
    result.adopt(compilationUnit.take());
    return result;
}

void InstructionSelection::callBuiltinInvalid(IR::Name *func, IR::ExprList *args, IR::Expr *result)
{
    prepareCallData(args, 0);

    if (useFastLookups && func->global) {
        uint index = registerGlobalGetterLookup(*func->id);
        generateRuntimeCall(result, callGlobalLookup,
                             Assembler::EngineRegister,
                             Assembler::TrustedImm32(index),
                             baseAddressForCallData());
    } else {
        generateRuntimeCall(result, callActivationProperty,
                             Assembler::EngineRegister,
                             Assembler::StringToIndex(*func->id),
                             baseAddressForCallData());
    }
}

void InstructionSelection::callBuiltinTypeofQmlContextProperty(IR::Expr *base,
                                                               IR::Member::MemberKind kind,
                                                               int propertyIndex, IR::Expr *result)
{
    if (kind == IR::Member::MemberOfQmlScopeObject) {
        generateRuntimeCall(result, typeofScopeObjectProperty, Assembler::EngineRegister,
                             Assembler::PointerToValue(base),
                             Assembler::TrustedImm32(propertyIndex));
    } else if (kind == IR::Member::MemberOfQmlContextObject) {
        generateRuntimeCall(result, typeofContextObjectProperty,
                             Assembler::EngineRegister, Assembler::PointerToValue(base),
                             Assembler::TrustedImm32(propertyIndex));
    } else {
        Q_UNREACHABLE();
    }
}

void InstructionSelection::callBuiltinTypeofMember(IR::Expr *base, const QString &name,
                                                   IR::Expr *result)
{
    generateRuntimeCall(result, typeofMember, Assembler::EngineRegister,
                         Assembler::PointerToValue(base), Assembler::StringToIndex(name));
}

void InstructionSelection::callBuiltinTypeofSubscript(IR::Expr *base, IR::Expr *index,
                                                      IR::Expr *result)
{
    generateRuntimeCall(result, typeofElement,
                         Assembler::EngineRegister,
                         Assembler::PointerToValue(base), Assembler::PointerToValue(index));
}

void InstructionSelection::callBuiltinTypeofName(const QString &name, IR::Expr *result)
{
    generateRuntimeCall(result, typeofName, Assembler::EngineRegister,
                         Assembler::StringToIndex(name));
}

void InstructionSelection::callBuiltinTypeofValue(IR::Expr *value, IR::Expr *result)
{
    generateRuntimeCall(result, typeofValue, Assembler::EngineRegister,
                         Assembler::PointerToValue(value));
}

void InstructionSelection::callBuiltinDeleteMember(IR::Expr *base, const QString &name, IR::Expr *result)
{
    generateRuntimeCall(result, deleteMember, Assembler::EngineRegister,
                         Assembler::Reference(base), Assembler::StringToIndex(name));
}

void InstructionSelection::callBuiltinDeleteSubscript(IR::Expr *base, IR::Expr *index,
                                                      IR::Expr *result)
{
    generateRuntimeCall(result, deleteElement, Assembler::EngineRegister,
                         Assembler::Reference(base), Assembler::PointerToValue(index));
}

void InstructionSelection::callBuiltinDeleteName(const QString &name, IR::Expr *result)
{
    generateRuntimeCall(result, deleteName, Assembler::EngineRegister,
                         Assembler::StringToIndex(name));
}

void InstructionSelection::callBuiltinDeleteValue(IR::Expr *result)
{
    _as->storeValue(Primitive::fromBoolean(false), result);
}

void InstructionSelection::callBuiltinThrow(IR::Expr *arg)
{
    generateRuntimeCall(Assembler::ReturnValueRegister, throwException, Assembler::EngineRegister,
                         Assembler::PointerToValue(arg));
}

void InstructionSelection::callBuiltinReThrow()
{
    _as->jumpToExceptionHandler();
}

void InstructionSelection::callBuiltinUnwindException(IR::Expr *result)
{
    generateRuntimeCall(result, unwindException, Assembler::EngineRegister);

}

void InstructionSelection::callBuiltinPushCatchScope(const QString &exceptionName)
{
    generateRuntimeCall(Assembler::Void, pushCatchScope, Assembler::EngineRegister, Assembler::StringToIndex(exceptionName));
}

void InstructionSelection::callBuiltinForeachIteratorObject(IR::Expr *arg, IR::Expr *result)
{
    Q_ASSERT(arg);
    Q_ASSERT(result);

    generateRuntimeCall(result, foreachIterator, Assembler::EngineRegister, Assembler::PointerToValue(arg));
}

void InstructionSelection::callBuiltinForeachNextPropertyname(IR::Expr *arg, IR::Expr *result)
{
    Q_ASSERT(arg);
    Q_ASSERT(result);

    generateRuntimeCall(result, foreachNextPropertyName, Assembler::Reference(arg));
}

void InstructionSelection::callBuiltinPushWithScope(IR::Expr *arg)
{
    Q_ASSERT(arg);

    generateRuntimeCall(Assembler::Void, pushWithScope, Assembler::Reference(arg), Assembler::EngineRegister);
}

void InstructionSelection::callBuiltinPopScope()
{
    generateRuntimeCall(Assembler::Void, popScope, Assembler::EngineRegister);
}

void InstructionSelection::callBuiltinDeclareVar(bool deletable, const QString &name)
{
    generateRuntimeCall(Assembler::Void, declareVar, Assembler::EngineRegister,
                         Assembler::TrustedImm32(deletable), Assembler::StringToIndex(name));
}

void InstructionSelection::callBuiltinDefineArray(IR::Expr *result, IR::ExprList *args)
{
    Q_ASSERT(result);

    int length = prepareVariableArguments(args);
    generateRuntimeCall(result, arrayLiteral, Assembler::EngineRegister,
                         baseAddressForCallArguments(), Assembler::TrustedImm32(length));
}

void InstructionSelection::callBuiltinDefineObjectLiteral(IR::Expr *result, int keyValuePairCount, IR::ExprList *keyValuePairs, IR::ExprList *arrayEntries, bool needSparseArray)
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
        _as->storeValue(QV4::Primitive::fromUInt32(index), _as->stackLayout().argumentAddressForCall(argc++));

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
        _as->storeValue(QV4::Primitive::fromUInt32(index), _as->stackLayout().argumentAddressForCall(argc++));

        // Getter
        _as->copyValue(_as->stackLayout().argumentAddressForCall(argc++), it->expr);
        it = it->next;

        // Setter
        _as->copyValue(_as->stackLayout().argumentAddressForCall(argc++), it->expr);
        it = it->next;
    }

    generateRuntimeCall(result, objectLiteral, Assembler::EngineRegister,
                         baseAddressForCallArguments(), Assembler::TrustedImm32(classId),
                         Assembler::TrustedImm32(arrayValueCount), Assembler::TrustedImm32(arrayGetterSetterCount | (needSparseArray << 30)));
}

void InstructionSelection::callBuiltinSetupArgumentObject(IR::Expr *result)
{
    generateRuntimeCall(result, setupArgumentsObject, Assembler::EngineRegister);
}

void InstructionSelection::callBuiltinConvertThisToObject()
{
    generateRuntimeCall(Assembler::Void, convertThisToObject, Assembler::EngineRegister);
}

void InstructionSelection::callValue(IR::Expr *value, IR::ExprList *args, IR::Expr *result)
{
    Q_ASSERT(value);

    prepareCallData(args, 0);
    if (value->asConst())
        generateRuntimeCall(result, callValue, Assembler::EngineRegister,
                             Assembler::PointerToValue(value),
                             baseAddressForCallData());
    else
        generateRuntimeCall(result, callValue, Assembler::EngineRegister,
                             Assembler::Reference(value),
                             baseAddressForCallData());
}

void InstructionSelection::loadThisObject(IR::Expr *temp)
{
    _as->loadPtr(Address(Assembler::EngineRegister, qOffsetOf(QV4::ExecutionEngine, current)), Assembler::ScratchRegister);
    _as->loadPtr(Address(Assembler::ScratchRegister, qOffsetOf(ExecutionContext::Data, callData)), Assembler::ScratchRegister);
#if defined(VALUE_FITS_IN_REGISTER)
    _as->load64(Pointer(Assembler::ScratchRegister, qOffsetOf(CallData, thisObject)),
                Assembler::ReturnValueRegister);
    _as->storeReturnValue(temp);
#else
    _as->copyValue(temp, Pointer(Assembler::ScratchRegister, qOffsetOf(CallData, thisObject)));
#endif
}

void InstructionSelection::loadQmlContext(IR::Expr *temp)
{
    generateRuntimeCall(temp, getQmlContext, Assembler::EngineRegister);
}

void InstructionSelection::loadQmlImportedScripts(IR::Expr *temp)
{
    generateRuntimeCall(temp, getQmlImportedScripts, Assembler::EngineRegister);
}

void InstructionSelection::loadQmlSingleton(const QString &name, IR::Expr *temp)
{
    generateRuntimeCall(temp, getQmlSingleton, Assembler::EngineRegister, Assembler::StringToIndex(name));
}

void InstructionSelection::loadConst(IR::Const *sourceConst, IR::Expr *target)
{
    if (IR::Temp *targetTemp = target->asTemp()) {
        if (targetTemp->kind == IR::Temp::PhysicalRegister) {
            if (targetTemp->type == IR::DoubleType) {
                Q_ASSERT(sourceConst->type == IR::DoubleType);
                _as->toDoubleRegister(sourceConst, (Assembler::FPRegisterID) targetTemp->index);
            } else if (targetTemp->type == IR::SInt32Type) {
                Q_ASSERT(sourceConst->type == IR::SInt32Type);
                _as->toInt32Register(sourceConst, (Assembler::RegisterID) targetTemp->index);
            } else if (targetTemp->type == IR::UInt32Type) {
                Q_ASSERT(sourceConst->type == IR::UInt32Type);
                _as->toUInt32Register(sourceConst, (Assembler::RegisterID) targetTemp->index);
            } else if (targetTemp->type == IR::BoolType) {
                Q_ASSERT(sourceConst->type == IR::BoolType);
                _as->move(Assembler::TrustedImm32(convertToValue(sourceConst).int_32()),
                          (Assembler::RegisterID) targetTemp->index);
            } else {
                Q_UNREACHABLE();
            }
            return;
        }
    }

    _as->storeValue(convertToValue(sourceConst), target);
}

void InstructionSelection::loadString(const QString &str, IR::Expr *target)
{
    Pointer srcAddr = _as->loadStringAddress(Assembler::ReturnValueRegister, str);
    _as->loadPtr(srcAddr, Assembler::ReturnValueRegister);
    Pointer destAddr = _as->loadAddress(Assembler::ScratchRegister, target);
#ifdef QV4_USE_64_BIT_VALUE_ENCODING
    _as->store64(Assembler::ReturnValueRegister, destAddr);
#else
    _as->store32(Assembler::ReturnValueRegister, destAddr);
    destAddr.offset += 4;
    _as->store32(Assembler::TrustedImm32(QV4::Value::Managed_Type_Internal), destAddr);
#endif
}

void InstructionSelection::loadRegexp(IR::RegExp *sourceRegexp, IR::Expr *target)
{
    int id = registerRegExp(sourceRegexp);
    generateRuntimeCall(target, regexpLiteral, Assembler::EngineRegister, Assembler::TrustedImm32(id));
}

void InstructionSelection::getActivationProperty(const IR::Name *name, IR::Expr *target)
{
    if (useFastLookups && name->global) {
        uint index = registerGlobalGetterLookup(*name->id);
        generateLookupCall(target, index, qOffsetOf(QV4::Lookup, globalGetter), Assembler::EngineRegister, Assembler::Void);
        return;
    }
    generateRuntimeCall(target, getActivationProperty, Assembler::EngineRegister, Assembler::StringToIndex(*name->id));
}

void InstructionSelection::setActivationProperty(IR::Expr *source, const QString &targetName)
{
    // ### should use a lookup call here
    generateRuntimeCall(Assembler::Void, setActivationProperty,
                         Assembler::EngineRegister, Assembler::StringToIndex(targetName), Assembler::PointerToValue(source));
}

void InstructionSelection::initClosure(IR::Closure *closure, IR::Expr *target)
{
    int id = closure->value;
    generateRuntimeCall(target, closure, Assembler::EngineRegister, Assembler::TrustedImm32(id));
}

void InstructionSelection::getProperty(IR::Expr *base, const QString &name, IR::Expr *target)
{
    if (useFastLookups) {
        uint index = registerGetterLookup(name);
        generateLookupCall(target, index, qOffsetOf(QV4::Lookup, getter), Assembler::EngineRegister, Assembler::PointerToValue(base), Assembler::Void);
    } else {
        generateRuntimeCall(target, getProperty, Assembler::EngineRegister,
                             Assembler::PointerToValue(base), Assembler::StringToIndex(name));
    }
}

void InstructionSelection::getQmlContextProperty(IR::Expr *base, IR::Member::MemberKind kind, int index, bool captureRequired, IR::Expr *target)
{
    if (kind == IR::Member::MemberOfQmlScopeObject)
        generateRuntimeCall(target, getQmlScopeObjectProperty, Assembler::EngineRegister, Assembler::PointerToValue(base), Assembler::TrustedImm32(index), Assembler::TrustedImm32(captureRequired));
    else if (kind == IR::Member::MemberOfQmlContextObject)
        generateRuntimeCall(target, getQmlContextObjectProperty, Assembler::EngineRegister, Assembler::PointerToValue(base), Assembler::TrustedImm32(index), Assembler::TrustedImm32(captureRequired));
    else if (kind == IR::Member::MemberOfIdObjectsArray)
        generateRuntimeCall(target, getQmlIdObject, Assembler::EngineRegister, Assembler::PointerToValue(base), Assembler::TrustedImm32(index));
    else
        Q_ASSERT(false);
}

void InstructionSelection::getQObjectProperty(IR::Expr *base, int propertyIndex, bool captureRequired, bool isSingleton, int attachedPropertiesId, IR::Expr *target)
{
    if (attachedPropertiesId != 0)
        generateRuntimeCall(target, getQmlAttachedProperty, Assembler::EngineRegister, Assembler::TrustedImm32(attachedPropertiesId), Assembler::TrustedImm32(propertyIndex));
    else if (isSingleton)
        generateRuntimeCall(target, getQmlSingletonQObjectProperty, Assembler::EngineRegister, Assembler::PointerToValue(base), Assembler::TrustedImm32(propertyIndex),
                             Assembler::TrustedImm32(captureRequired));
    else
        generateRuntimeCall(target, getQmlQObjectProperty, Assembler::EngineRegister, Assembler::PointerToValue(base), Assembler::TrustedImm32(propertyIndex),
                             Assembler::TrustedImm32(captureRequired));
}

void InstructionSelection::setProperty(IR::Expr *source, IR::Expr *targetBase,
                                       const QString &targetName)
{
    if (useFastLookups) {
        uint index = registerSetterLookup(targetName);
        generateLookupCall(Assembler::Void, index, qOffsetOf(QV4::Lookup, setter),
                           Assembler::EngineRegister,
                           Assembler::PointerToValue(targetBase),
                           Assembler::PointerToValue(source));
    } else {
        generateRuntimeCall(Assembler::Void, setProperty, Assembler::EngineRegister,
                             Assembler::PointerToValue(targetBase), Assembler::StringToIndex(targetName),
                             Assembler::PointerToValue(source));
    }
}

void InstructionSelection::setQmlContextProperty(IR::Expr *source, IR::Expr *targetBase, IR::Member::MemberKind kind, int propertyIndex)
{
    if (kind == IR::Member::MemberOfQmlScopeObject)
        generateRuntimeCall(Assembler::Void, setQmlScopeObjectProperty, Assembler::EngineRegister, Assembler::PointerToValue(targetBase),
                             Assembler::TrustedImm32(propertyIndex), Assembler::PointerToValue(source));
    else if (kind == IR::Member::MemberOfQmlContextObject)
        generateRuntimeCall(Assembler::Void, setQmlContextObjectProperty, Assembler::EngineRegister, Assembler::PointerToValue(targetBase),
                             Assembler::TrustedImm32(propertyIndex), Assembler::PointerToValue(source));
    else
        Q_ASSERT(false);
}

void InstructionSelection::setQObjectProperty(IR::Expr *source, IR::Expr *targetBase, int propertyIndex)
{
    generateRuntimeCall(Assembler::Void, setQmlQObjectProperty, Assembler::EngineRegister, Assembler::PointerToValue(targetBase),
                         Assembler::TrustedImm32(propertyIndex), Assembler::PointerToValue(source));
}

void InstructionSelection::getElement(IR::Expr *base, IR::Expr *index, IR::Expr *target)
{
    if (useFastLookups) {
        uint lookup = registerIndexedGetterLookup();
        generateLookupCall(target, lookup, qOffsetOf(QV4::Lookup, indexedGetter),
                           Assembler::PointerToValue(base),
                           Assembler::PointerToValue(index));
        return;
    }

    generateRuntimeCall(target, getElement, Assembler::EngineRegister,
                         Assembler::PointerToValue(base), Assembler::PointerToValue(index));
}

void InstructionSelection::setElement(IR::Expr *source, IR::Expr *targetBase, IR::Expr *targetIndex)
{
    if (useFastLookups) {
        uint lookup = registerIndexedSetterLookup();
        generateLookupCall(Assembler::Void, lookup, qOffsetOf(QV4::Lookup, indexedSetter),
                           Assembler::PointerToValue(targetBase), Assembler::PointerToValue(targetIndex),
                           Assembler::PointerToValue(source));
        return;
    }
    generateRuntimeCall(Assembler::Void, setElement, Assembler::EngineRegister,
                         Assembler::PointerToValue(targetBase), Assembler::PointerToValue(targetIndex),
                         Assembler::PointerToValue(source));
}

void InstructionSelection::copyValue(IR::Expr *source, IR::Expr *target)
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
                _as->moveDouble((Assembler::FPRegisterID) sourceTemp->index,
                                (Assembler::FPRegisterID) targetTemp->index);
            else
                _as->move((Assembler::RegisterID) sourceTemp->index,
                          (Assembler::RegisterID) targetTemp->index);
            return;
        } else {
            switch (sourceTemp->type) {
            case IR::DoubleType:
                _as->storeDouble((Assembler::FPRegisterID) sourceTemp->index, target);
                break;
            case IR::SInt32Type:
                _as->storeInt32((Assembler::RegisterID) sourceTemp->index, target);
                break;
            case IR::UInt32Type:
                _as->storeUInt32((Assembler::RegisterID) sourceTemp->index, target);
                break;
            case IR::BoolType:
                _as->storeBool((Assembler::RegisterID) sourceTemp->index, target);
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
            _as->toDoubleRegister(source, (Assembler::FPRegisterID) targetTemp->index);
            return;
        case IR::BoolType:
            Q_ASSERT(source->type == IR::BoolType);
            _as->toInt32Register(source, (Assembler::RegisterID) targetTemp->index);
            return;
        case IR::SInt32Type:
            Q_ASSERT(source->type == IR::SInt32Type);
            _as->toInt32Register(source, (Assembler::RegisterID) targetTemp->index);
            return;
        case IR::UInt32Type:
            Q_ASSERT(source->type == IR::UInt32Type);
            _as->toUInt32Register(source, (Assembler::RegisterID) targetTemp->index);
            return;
        default:
            Q_ASSERT(!"Unreachable");
            break;
        }
    }

    // The target is not a physical register, nor is the source. So we can do a memory-to-memory copy:
    _as->memcopyValue(_as->loadAddress(Assembler::ReturnValueRegister, target), source, Assembler::ScratchRegister);
}

void InstructionSelection::swapValues(IR::Expr *source, IR::Expr *target)
{
    IR::Temp *sourceTemp = source->asTemp();
    IR::Temp *targetTemp = target->asTemp();

    if (sourceTemp && sourceTemp->kind == IR::Temp::PhysicalRegister) {
        if (targetTemp && targetTemp->kind == IR::Temp::PhysicalRegister) {
            Q_ASSERT(sourceTemp->type == targetTemp->type);

            if (sourceTemp->type == IR::DoubleType) {
                _as->moveDouble((Assembler::FPRegisterID) targetTemp->index, Assembler::FPGpr0);
                _as->moveDouble((Assembler::FPRegisterID) sourceTemp->index,
                                (Assembler::FPRegisterID) targetTemp->index);
                _as->moveDouble(Assembler::FPGpr0, (Assembler::FPRegisterID) sourceTemp->index);
            } else {
                _as->swap((Assembler::RegisterID) sourceTemp->index,
                          (Assembler::RegisterID) targetTemp->index);
            }
            return;
        }
    } else if (!sourceTemp || sourceTemp->kind == IR::Temp::StackSlot) {
        if (!targetTemp || targetTemp->kind == IR::Temp::StackSlot) {
            // Note: a swap for two stack-slots can involve different types.
            Assembler::Pointer sAddr = _as->loadAddress(Assembler::ScratchRegister, source);
            Assembler::Pointer tAddr = _as->loadAddress(Assembler::ReturnValueRegister, target);
            // use the implementation in JSC::MacroAssembler, as it doesn't do bit swizzling
            _as->JSC::MacroAssembler::loadDouble(sAddr, Assembler::FPGpr0);
            _as->JSC::MacroAssembler::loadDouble(tAddr, Assembler::FPGpr1);
            _as->JSC::MacroAssembler::storeDouble(Assembler::FPGpr1, sAddr);
            _as->JSC::MacroAssembler::storeDouble(Assembler::FPGpr0, tAddr);
            return;
        }
    }

    IR::Expr *memExpr = !sourceTemp || sourceTemp->kind == IR::Temp::StackSlot ? source : target;
    IR::Temp *regTemp = sourceTemp && sourceTemp->kind == IR::Temp::PhysicalRegister ? sourceTemp
                                                                                     : targetTemp;
    Q_ASSERT(memExpr);
    Q_ASSERT(regTemp);

    Assembler::Pointer addr = _as->loadAddress(Assembler::ReturnValueRegister, memExpr);
    if (regTemp->type == IR::DoubleType) {
        _as->loadDouble(addr, Assembler::FPGpr0);
        _as->storeDouble((Assembler::FPRegisterID) regTemp->index, addr);
        _as->moveDouble(Assembler::FPGpr0, (Assembler::FPRegisterID) regTemp->index);
    } else if (regTemp->type == IR::UInt32Type) {
        _as->toUInt32Register(addr, Assembler::ScratchRegister);
        _as->storeUInt32((Assembler::RegisterID) regTemp->index, addr);
        _as->move(Assembler::ScratchRegister, (Assembler::RegisterID) regTemp->index);
    } else {
        _as->load32(addr, Assembler::ScratchRegister);
        _as->store32((Assembler::RegisterID) regTemp->index, addr);
        if (regTemp->type != memExpr->type) {
            addr.offset += 4;
            quint32 tag;
            switch (regTemp->type) {
            case IR::BoolType:
                tag = QV4::Value::Boolean_Type_Internal;
                break;
            case IR::SInt32Type:
                tag = QV4::Value::Integer_Type_Internal;
                break;
            default:
                tag = 31337; // bogus value
                Q_UNREACHABLE();
            }
            _as->store32(Assembler::TrustedImm32(tag), addr);
        }
        _as->move(Assembler::ScratchRegister, (Assembler::RegisterID) regTemp->index);
    }
}

#define setOp(op, opName, operation) \
    do { \
        op = RuntimeCall(qOffsetOf(QV4::Runtime, operation)); opName = "Runtime::" isel_stringIfy(operation); \
        needsExceptionCheck = QV4::Runtime::Method_##operation##_NeedsExceptionCheck; \
    } while (0)
#define setOpContext(op, opName, operation) \
    do { \
        opContext = RuntimeCall(qOffsetOf(QV4::Runtime, operation)); opName = "Runtime::" isel_stringIfy(operation); \
        needsExceptionCheck = QV4::Runtime::Method_##operation##_NeedsExceptionCheck; \
    } while (0)

void InstructionSelection::unop(IR::AluOp oper, IR::Expr *source, IR::Expr *target)
{
    QV4::JIT::Unop unop(_as, oper);
    unop.generate(source, target);
}


void InstructionSelection::binop(IR::AluOp oper, IR::Expr *leftSource, IR::Expr *rightSource, IR::Expr *target)
{
    QV4::JIT::Binop binop(_as, oper);
    binop.generate(leftSource, rightSource, target);
}

void InstructionSelection::callQmlContextProperty(IR::Expr *base, IR::Member::MemberKind kind, int propertyIndex, IR::ExprList *args, IR::Expr *result)
{
    prepareCallData(args, base);

    if (kind == IR::Member::MemberOfQmlScopeObject)
        generateRuntimeCall(result, callQmlScopeObjectProperty,
                             Assembler::EngineRegister,
                             Assembler::TrustedImm32(propertyIndex),
                             baseAddressForCallData());
    else if (kind == IR::Member::MemberOfQmlContextObject)
        generateRuntimeCall(result, callQmlContextObjectProperty,
                             Assembler::EngineRegister,
                             Assembler::TrustedImm32(propertyIndex),
                             baseAddressForCallData());
    else
        Q_ASSERT(false);
}

void InstructionSelection::callProperty(IR::Expr *base, const QString &name, IR::ExprList *args,
                                        IR::Expr *result)
{
    Q_ASSERT(base != 0);

    prepareCallData(args, base);

    if (useFastLookups) {
        uint index = registerGetterLookup(name);
        generateRuntimeCall(result, callPropertyLookup,
                             Assembler::EngineRegister,
                             Assembler::TrustedImm32(index),
                             baseAddressForCallData());
    } else {
        generateRuntimeCall(result, callProperty, Assembler::EngineRegister,
                             Assembler::StringToIndex(name),
                             baseAddressForCallData());
    }
}

void InstructionSelection::callSubscript(IR::Expr *base, IR::Expr *index, IR::ExprList *args,
                                         IR::Expr *result)
{
    Q_ASSERT(base != 0);

    prepareCallData(args, base);
    generateRuntimeCall(result, callElement, Assembler::EngineRegister,
                         Assembler::PointerToValue(index),
                         baseAddressForCallData());
}

void InstructionSelection::convertType(IR::Expr *source, IR::Expr *target)
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

void InstructionSelection::convertTypeSlowPath(IR::Expr *source, IR::Expr *target)
{
    Q_ASSERT(target->type != IR::BoolType);

    if (target->type & IR::NumberType)
        unop(IR::OpUPlus, source, target);
    else
        copyValue(source, target);
}

void InstructionSelection::convertTypeToDouble(IR::Expr *source, IR::Expr *target)
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
        _as->loadDouble(_as->loadAddress(Assembler::ScratchRegister, source), Assembler::FPGpr0);
        _as->storeDouble(Assembler::FPGpr0, target);
        break;
    case IR::StringType:
    case IR::VarType: {
        // load the tag:
        Assembler::Pointer tagAddr = _as->loadAddress(Assembler::ScratchRegister, source);
        tagAddr.offset += 4;
        _as->load32(tagAddr, Assembler::ScratchRegister);

        // check if it's an int32:
        Assembler::Jump isNoInt = _as->branch32(Assembler::NotEqual, Assembler::ScratchRegister,
                                                Assembler::TrustedImm32(Value::Integer_Type_Internal));
        convertIntToDouble(source, target);
        Assembler::Jump intDone = _as->jump();

        // not an int, check if it's NOT a double:
        isNoInt.link(_as);
#ifdef QV4_USE_64_BIT_VALUE_ENCODING
        _as->rshift32(Assembler::TrustedImm32(Value::IsDoubleTag_Shift), Assembler::ScratchRegister);
        Assembler::Jump isDbl = _as->branch32(Assembler::NotEqual, Assembler::ScratchRegister,
                                              Assembler::TrustedImm32(0));
#else
        _as->and32(Assembler::TrustedImm32(Value::NotDouble_Mask), Assembler::ScratchRegister);
        Assembler::Jump isDbl = _as->branch32(Assembler::NotEqual, Assembler::ScratchRegister,
                                              Assembler::TrustedImm32(Value::NotDouble_Mask));
#endif

        generateRuntimeCall(target, toDouble, Assembler::PointerToValue(source));
        Assembler::Jump noDoubleDone = _as->jump();

        // it is a double:
        isDbl.link(_as);
        Assembler::Pointer addr2 = _as->loadAddress(Assembler::ScratchRegister, source);
        IR::Temp *targetTemp = target->asTemp();
        if (!targetTemp || targetTemp->kind == IR::Temp::StackSlot) {
#if Q_PROCESSOR_WORDSIZE == 8
            _as->load64(addr2, Assembler::ScratchRegister);
            _as->store64(Assembler::ScratchRegister, _as->loadAddress(Assembler::ReturnValueRegister, target));
#else
            _as->loadDouble(addr2, Assembler::FPGpr0);
            _as->storeDouble(Assembler::FPGpr0, _as->loadAddress(Assembler::ReturnValueRegister, target));
#endif
        } else {
            _as->loadDouble(addr2, (Assembler::FPRegisterID) targetTemp->index);
        }

        noDoubleDone.link(_as);
        intDone.link(_as);
    } break;
    default:
        convertTypeSlowPath(source, target);
        break;
    }
}

void InstructionSelection::convertTypeToBool(IR::Expr *source, IR::Expr *target)
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
        Assembler::FPRegisterID reg;
        if (sourceTemp && sourceTemp->kind == IR::Temp::PhysicalRegister)
            reg = (Assembler::FPRegisterID) sourceTemp->index;
        else
            reg = _as->toDoubleRegister(source, (Assembler::FPRegisterID) 1);
        Assembler::Jump nonZero = _as->branchDoubleNonZero(reg, Assembler::FPGpr0);

        // it's 0, so false:
        _as->storeBool(false, target);
        Assembler::Jump done = _as->jump();

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
    case IR::VarType:
    default:
        generateRuntimeCall(Assembler::ReturnValueRegister, toBoolean,
                             Assembler::PointerToValue(source));
        _as->storeBool(Assembler::ReturnValueRegister, target);
        break;
    }
}

void InstructionSelection::convertTypeToSInt32(IR::Expr *source, IR::Expr *target)
{
    switch (source->type) {
    case IR::VarType: {

#ifdef QV4_USE_64_BIT_VALUE_ENCODING
        Assembler::Pointer addr = _as->loadAddress(Assembler::ScratchRegister, source);
        _as->load64(addr, Assembler::ScratchRegister);
        _as->move(Assembler::ScratchRegister, Assembler::ReturnValueRegister);

        // check if it's integer convertible
        _as->urshift64(Assembler::TrustedImm32(QV4::Value::IsIntegerConvertible_Shift), Assembler::ScratchRegister);
        Assembler::Jump isIntConvertible = _as->branch32(Assembler::Equal, Assembler::ScratchRegister, Assembler::TrustedImm32(3));

        // nope, not integer convertible, so check for a double:
        _as->urshift64(Assembler::TrustedImm32(
                           QV4::Value::IsDoubleTag_Shift - QV4::Value::IsIntegerConvertible_Shift),
                       Assembler::ScratchRegister);
        Assembler::Jump fallback = _as->branch32(Assembler::GreaterThan, Assembler::ScratchRegister, Assembler::TrustedImm32(0));

        // it's a double
        _as->move(Assembler::TrustedImm64(QV4::Value::NaNEncodeMask), Assembler::ScratchRegister);
        _as->xor64(Assembler::ScratchRegister, Assembler::ReturnValueRegister);
        _as->move64ToDouble(Assembler::ReturnValueRegister, Assembler::FPGpr0);
        Assembler::Jump success =
                _as->branchTruncateDoubleToInt32(Assembler::FPGpr0, Assembler::ReturnValueRegister,
                                                 Assembler::BranchIfTruncateSuccessful);

        // not an int:
        fallback.link(_as);
        generateRuntimeCall(Assembler::ReturnValueRegister, toInt,
                             _as->loadAddress(Assembler::ScratchRegister, source));

        isIntConvertible.link(_as);
        success.link(_as);
        IR::Temp *targetTemp = target->asTemp();
        if (!targetTemp || targetTemp->kind == IR::Temp::StackSlot) {
            Assembler::Pointer targetAddr = _as->loadAddress(Assembler::ScratchRegister, target);
            _as->store32(Assembler::ReturnValueRegister, targetAddr);
            targetAddr.offset += 4;
            _as->store32(Assembler::TrustedImm32(Value::Integer_Type_Internal), targetAddr);
        } else {
            _as->storeInt32(Assembler::ReturnValueRegister, target);
        }
#else
        // load the tag:
        Assembler::Pointer addr = _as->loadAddress(Assembler::ScratchRegister, source);
        Assembler::Pointer tagAddr = addr;
        tagAddr.offset += 4;
        _as->load32(tagAddr, Assembler::ReturnValueRegister);

        // check if it's an int32:
        Assembler::Jump fallback = _as->branch32(Assembler::NotEqual, Assembler::ReturnValueRegister,
                                                Assembler::TrustedImm32(Value::Integer_Type_Internal));
        IR::Temp *targetTemp = target->asTemp();
        if (!targetTemp || targetTemp->kind == IR::Temp::StackSlot) {
            _as->load32(addr, Assembler::ReturnValueRegister);
            Assembler::Pointer targetAddr = _as->loadAddress(Assembler::ScratchRegister, target);
            _as->store32(Assembler::ReturnValueRegister, targetAddr);
            targetAddr.offset += 4;
            _as->store32(Assembler::TrustedImm32(Value::Integer_Type_Internal), targetAddr);
        } else {
            _as->load32(addr, (Assembler::RegisterID) targetTemp->index);
        }
        Assembler::Jump intDone = _as->jump();

        // not an int:
        fallback.link(_as);
        generateRuntimeCall(Assembler::ReturnValueRegister, toInt,
                             _as->loadAddress(Assembler::ScratchRegister, source));
        _as->storeInt32(Assembler::ReturnValueRegister, target);

        intDone.link(_as);
#endif

    } break;
    case IR::DoubleType: {
        Assembler::Jump success =
                _as->branchTruncateDoubleToInt32(_as->toDoubleRegister(source),
                                                 Assembler::ReturnValueRegister,
                                                 Assembler::BranchIfTruncateSuccessful);
        generateRuntimeCall(Assembler::ReturnValueRegister, doubleToInt,
                             Assembler::PointerToValue(source));
        success.link(_as);
        _as->storeInt32(Assembler::ReturnValueRegister, target);
    } break;
    case IR::UInt32Type:
        _as->storeInt32(_as->toUInt32Register(source, Assembler::ReturnValueRegister), target);
        break;
    case IR::NullType:
    case IR::UndefinedType:
        _as->move(Assembler::TrustedImm32(0), Assembler::ReturnValueRegister);
        _as->storeInt32(Assembler::ReturnValueRegister, target);
        break;
    case IR::BoolType:
        _as->storeInt32(_as->toInt32Register(source, Assembler::ReturnValueRegister), target);
        break;
    case IR::StringType:
    default:
        generateRuntimeCall(Assembler::ReturnValueRegister, toInt,
                             _as->loadAddress(Assembler::ScratchRegister, source));
        _as->storeInt32(Assembler::ReturnValueRegister, target);
        break;
    } // switch (source->type)
}

void InstructionSelection::convertTypeToUInt32(IR::Expr *source, IR::Expr *target)
{
    switch (source->type) {
    case IR::VarType: {
        // load the tag:
        Assembler::Pointer tagAddr = _as->loadAddress(Assembler::ScratchRegister, source);
        tagAddr.offset += 4;
        _as->load32(tagAddr, Assembler::ScratchRegister);

        // check if it's an int32:
        Assembler::Jump isNoInt = _as->branch32(Assembler::NotEqual, Assembler::ScratchRegister,
                                                Assembler::TrustedImm32(Value::Integer_Type_Internal));
        Assembler::Pointer addr = _as->loadAddress(Assembler::ScratchRegister, source);
        _as->storeUInt32(_as->toInt32Register(addr, Assembler::ScratchRegister), target);
        Assembler::Jump intDone = _as->jump();

        // not an int:
        isNoInt.link(_as);
        generateRuntimeCall(Assembler::ReturnValueRegister, toUInt,
                             _as->loadAddress(Assembler::ScratchRegister, source));
        _as->storeInt32(Assembler::ReturnValueRegister, target);

        intDone.link(_as);
    } break;
    case IR::DoubleType: {
        Assembler::FPRegisterID reg = _as->toDoubleRegister(source);
        Assembler::Jump success =
                _as->branchTruncateDoubleToUint32(reg, Assembler::ReturnValueRegister,
                                                  Assembler::BranchIfTruncateSuccessful);
        generateRuntimeCall(Assembler::ReturnValueRegister, doubleToUInt,
                             Assembler::PointerToValue(source));
        success.link(_as);
        _as->storeUInt32(Assembler::ReturnValueRegister, target);
    } break;
    case IR::NullType:
    case IR::UndefinedType:
        _as->move(Assembler::TrustedImm32(0), Assembler::ReturnValueRegister);
        _as->storeUInt32(Assembler::ReturnValueRegister, target);
        break;
    case IR::StringType:
        generateRuntimeCall(Assembler::ReturnValueRegister, toUInt,
                             Assembler::PointerToValue(source));
        _as->storeUInt32(Assembler::ReturnValueRegister, target);
        break;
    case IR::SInt32Type:
    case IR::BoolType:
        _as->storeUInt32(_as->toInt32Register(source, Assembler::ReturnValueRegister), target);
        break;
    default:
        break;
    } // switch (source->type)
}

void InstructionSelection::constructActivationProperty(IR::Name *func, IR::ExprList *args, IR::Expr *result)
{
    Q_ASSERT(func != 0);
    prepareCallData(args, 0);

    if (useFastLookups && func->global) {
        uint index = registerGlobalGetterLookup(*func->id);
        generateRuntimeCall(result, constructGlobalLookup,
                             Assembler::EngineRegister,
                             Assembler::TrustedImm32(index), baseAddressForCallData());
        return;
    }

    generateRuntimeCall(result, constructActivationProperty,
                         Assembler::EngineRegister,
                         Assembler::StringToIndex(*func->id),
                         baseAddressForCallData());
}


void InstructionSelection::constructProperty(IR::Expr *base, const QString &name, IR::ExprList *args, IR::Expr *result)
{
    prepareCallData(args, base);
    if (useFastLookups) {
        uint index = registerGetterLookup(name);
        generateRuntimeCall(result, constructPropertyLookup,
                             Assembler::EngineRegister,
                             Assembler::TrustedImm32(index),
                             baseAddressForCallData());
        return;
    }

    generateRuntimeCall(result, constructProperty, Assembler::EngineRegister,
                         Assembler::StringToIndex(name),
                         baseAddressForCallData());
}

void InstructionSelection::constructValue(IR::Expr *value, IR::ExprList *args, IR::Expr *result)
{
    Q_ASSERT(value != 0);

    prepareCallData(args, 0);
    generateRuntimeCall(result, constructValue,
                         Assembler::EngineRegister,
                         Assembler::Reference(value),
                         baseAddressForCallData());
}

void InstructionSelection::visitJump(IR::Jump *s)
{
    if (!_removableJumps.contains(s))
        _as->jumpToBlock(_block, s->target);
}

void InstructionSelection::visitCJump(IR::CJump *s)
{
    IR::Temp *t = s->cond->asTemp();
    if (t || s->cond->asArgLocal()) {
        Assembler::RegisterID reg;
        if (t && t->kind == IR::Temp::PhysicalRegister) {
            Q_ASSERT(t->type == IR::BoolType);
            reg = (Assembler::RegisterID) t->index;
        } else if (t && t->kind == IR::Temp::StackSlot && t->type == IR::BoolType) {
            reg = Assembler::ReturnValueRegister;
            _as->toInt32Register(t, reg);
        } else {
            Address temp = _as->loadAddress(Assembler::ScratchRegister, s->cond);
            Address tag = temp;
            tag.offset += QV4::Value::tagOffset();
            Assembler::Jump booleanConversion = _as->branch32(Assembler::NotEqual, tag, Assembler::TrustedImm32(QV4::Value::Boolean_Type_Internal));

            Address data = temp;
            data.offset += QV4::Value::valueOffset();
            _as->load32(data, Assembler::ReturnValueRegister);
            Assembler::Jump testBoolean = _as->jump();

            booleanConversion.link(_as);
            reg = Assembler::ReturnValueRegister;
            generateRuntimeCall(reg, toBoolean, Assembler::Reference(s->cond));

            testBoolean.link(_as);
        }

        _as->generateCJumpOnNonZero(reg, _block, s->iftrue, s->iffalse);
        return;
    } else if (IR::Const *c = s->cond->asConst()) {
        // TODO: SSA optimization for constant condition evaluation should remove this.
        // See also visitCJump() in RegAllocInfo.
        generateRuntimeCall(Assembler::ReturnValueRegister, toBoolean,
                             Assembler::PointerToValue(c));
        _as->generateCJumpOnNonZero(Assembler::ReturnValueRegister, _block, s->iftrue, s->iffalse);
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

        RuntimeCall op;
        RuntimeCall opContext;
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
                                         Assembler::ReturnValueRegister, opName, opContext,
                                         Assembler::EngineRegister,
                                         Assembler::PointerToValue(b->left),
                                         Assembler::PointerToValue(b->right));
        else
            _as->generateFunctionCallImp(needsExceptionCheck,
                                         Assembler::ReturnValueRegister, opName, op,
                                         Assembler::PointerToValue(b->left),
                                         Assembler::PointerToValue(b->right));

        _as->generateCJumpOnNonZero(Assembler::ReturnValueRegister, _block, s->iftrue, s->iffalse);
        return;
    }
    Q_UNREACHABLE();
}

void InstructionSelection::visitRet(IR::Ret *s)
{
    if (!s) {
        // this only happens if the method doesn't have a return statement and can
        // only exit through an exception
    } else if (IR::Temp *t = s->expr->asTemp()) {
#if CPU(X86) || CPU(ARM) || CPU(MIPS)

#  if CPU(X86)
        Assembler::RegisterID lowReg = JSC::X86Registers::eax;
        Assembler::RegisterID highReg = JSC::X86Registers::edx;
#  elif CPU(MIPS)
        Assembler::RegisterID lowReg = JSC::MIPSRegisters::v0;
        Assembler::RegisterID highReg = JSC::MIPSRegisters::v1;
#  else // CPU(ARM)
        Assembler::RegisterID lowReg = JSC::ARMRegisters::r0;
        Assembler::RegisterID highReg = JSC::ARMRegisters::r1;
#  endif

        if (t->kind == IR::Temp::PhysicalRegister) {
            switch (t->type) {
            case IR::DoubleType:
                _as->moveDoubleToInts((Assembler::FPRegisterID) t->index, lowReg, highReg);
                break;
            case IR::UInt32Type: {
                Assembler::RegisterID srcReg = (Assembler::RegisterID) t->index;
                Assembler::Jump intRange = _as->branch32(Assembler::GreaterThanOrEqual, srcReg, Assembler::TrustedImm32(0));
                _as->convertUInt32ToDouble(srcReg, Assembler::FPGpr0, Assembler::ReturnValueRegister);
                _as->moveDoubleToInts(Assembler::FPGpr0, lowReg, highReg);
                Assembler::Jump done = _as->jump();
                intRange.link(_as);
                _as->move(srcReg, lowReg);
                _as->move(Assembler::TrustedImm32(QV4::Value::Integer_Type_Internal), highReg);
                done.link(_as);
            } break;
            case IR::SInt32Type:
                _as->move((Assembler::RegisterID) t->index, lowReg);
                _as->move(Assembler::TrustedImm32(QV4::Value::Integer_Type_Internal), highReg);
                break;
            case IR::BoolType:
                _as->move((Assembler::RegisterID) t->index, lowReg);
                _as->move(Assembler::TrustedImm32(QV4::Value::Boolean_Type_Internal), highReg);
                break;
            default:
                Q_UNREACHABLE();
            }
        } else {
            Pointer addr = _as->loadAddress(Assembler::ScratchRegister, t);
            _as->load32(addr, lowReg);
            addr.offset += 4;
            _as->load32(addr, highReg);
        }
#else
        if (t->kind == IR::Temp::PhysicalRegister) {
            if (t->type == IR::DoubleType) {
                _as->moveDoubleTo64((Assembler::FPRegisterID) t->index,
                                    Assembler::ReturnValueRegister);
                _as->move(Assembler::TrustedImm64(QV4::Value::NaNEncodeMask),
                          Assembler::ScratchRegister);
                _as->xor64(Assembler::ScratchRegister, Assembler::ReturnValueRegister);
            } else if (t->type == IR::UInt32Type) {
                Assembler::RegisterID srcReg = (Assembler::RegisterID) t->index;
                Assembler::Jump intRange = _as->branch32(Assembler::GreaterThanOrEqual, srcReg, Assembler::TrustedImm32(0));
                _as->convertUInt32ToDouble(srcReg, Assembler::FPGpr0, Assembler::ReturnValueRegister);
                _as->moveDoubleTo64(Assembler::FPGpr0, Assembler::ReturnValueRegister);
                _as->move(Assembler::TrustedImm64(QV4::Value::NaNEncodeMask), Assembler::ScratchRegister);
                _as->xor64(Assembler::ScratchRegister, Assembler::ReturnValueRegister);
                Assembler::Jump done = _as->jump();
                intRange.link(_as);
                _as->zeroExtend32ToPtr(srcReg, Assembler::ReturnValueRegister);
                quint64 tag = QV4::Value::Integer_Type_Internal;
                _as->or64(Assembler::TrustedImm64(tag << 32),
                          Assembler::ReturnValueRegister);
                done.link(_as);
            } else {
                _as->zeroExtend32ToPtr((Assembler::RegisterID) t->index, Assembler::ReturnValueRegister);
                quint64 tag;
                switch (t->type) {
                case IR::SInt32Type:
                    tag = QV4::Value::Integer_Type_Internal;
                    break;
                case IR::BoolType:
                    tag = QV4::Value::Boolean_Type_Internal;
                    break;
                default:
                    tag = 31337; // bogus value
                    Q_UNREACHABLE();
                }
                _as->or64(Assembler::TrustedImm64(tag << 32),
                          Assembler::ReturnValueRegister);
            }
        } else {
            _as->copyValue(Assembler::ReturnValueRegister, t);
        }
#endif
    } else if (IR::Const *c = s->expr->asConst()) {
        QV4::Primitive retVal = convertToValue(c);
#if CPU(X86)
        _as->move(Assembler::TrustedImm32(retVal.int_32()), JSC::X86Registers::eax);
        _as->move(Assembler::TrustedImm32(retVal.tag()), JSC::X86Registers::edx);
#elif CPU(ARM)
        _as->move(Assembler::TrustedImm32(retVal.int_32()), JSC::ARMRegisters::r0);
        _as->move(Assembler::TrustedImm32(retVal.tag()), JSC::ARMRegisters::r1);
#elif CPU(MIPS)
        _as->move(Assembler::TrustedImm32(retVal.int_32()), JSC::MIPSRegisters::v0);
        _as->move(Assembler::TrustedImm32(retVal.tag()), JSC::MIPSRegisters::v1);
#else
        _as->move(Assembler::TrustedImm64(retVal.rawValue()), Assembler::ReturnValueRegister);
#endif
    } else {
        Q_UNREACHABLE();
        Q_UNUSED(s);
    }

    Assembler::Label leaveStackFrame = _as->label();

    const int locals = _as->stackLayout().calculateJSStackFrameSize();
    _as->subPtr(Assembler::TrustedImm32(sizeof(QV4::Value)*locals), Assembler::LocalsRegister);
    _as->loadPtr(Address(Assembler::EngineRegister, qOffsetOf(QV4::ExecutionEngine, current)), Assembler::ScratchRegister);
    _as->loadPtr(Address(Assembler::ScratchRegister, qOffsetOf(ExecutionContext::Data, engine)), Assembler::ScratchRegister);
    _as->storePtr(Assembler::LocalsRegister, Address(Assembler::ScratchRegister, qOffsetOf(ExecutionEngine, jsStackTop)));

    _as->leaveStandardStackFrame(regularRegistersToSave, fpRegistersToSave);
    _as->ret();

    _as->exceptionReturnLabel = _as->label();
    QV4::Primitive retVal = Primitive::undefinedValue();
#if CPU(X86)
    _as->move(Assembler::TrustedImm32(retVal.int_32()), JSC::X86Registers::eax);
    _as->move(Assembler::TrustedImm32(retVal.tag()), JSC::X86Registers::edx);
#elif CPU(ARM)
    _as->move(Assembler::TrustedImm32(retVal.int_32()), JSC::ARMRegisters::r0);
    _as->move(Assembler::TrustedImm32(retVal.tag()), JSC::ARMRegisters::r1);
#elif CPU(MIPS)
    _as->move(Assembler::TrustedImm32(retVal.int_32()), JSC::MIPSRegisters::v0);
    _as->move(Assembler::TrustedImm32(retVal.tag()), JSC::MIPSRegisters::v1);
#else
    _as->move(Assembler::TrustedImm64(retVal.rawValue()), Assembler::ReturnValueRegister);
#endif
    _as->jump(leaveStackFrame);
}

int InstructionSelection::prepareVariableArguments(IR::ExprList* args)
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
            _as->memcopyValue(dst, arg->asTemp(), Assembler::ScratchRegister);
        else
            _as->copyValue(dst, arg);
    }

    return argc;
}

int InstructionSelection::prepareCallData(IR::ExprList* args, IR::Expr *thisObject)
{
    int argc = 0;
    for (IR::ExprList *it = args; it; it = it->next) {
        ++argc;
    }

    Pointer p = _as->stackLayout().callDataAddress(qOffsetOf(CallData, tag));
    _as->store32(Assembler::TrustedImm32(QV4::Value::Integer_Type_Internal), p);
    p = _as->stackLayout().callDataAddress(qOffsetOf(CallData, argc));
    _as->store32(Assembler::TrustedImm32(argc), p);
    p = _as->stackLayout().callDataAddress(qOffsetOf(CallData, thisObject));
    if (!thisObject)
        _as->storeValue(QV4::Primitive::undefinedValue(), p);
    else
        _as->copyValue(p, thisObject);

    int i = 0;
    for (IR::ExprList *it = args; it; it = it->next, ++i) {
        IR::Expr *arg = it->expr;
        Q_ASSERT(arg != 0);
        Pointer dst(_as->stackLayout().argumentAddressForCall(i));
        if (arg->asTemp() && arg->asTemp()->kind != IR::Temp::PhysicalRegister)
            _as->memcopyValue(dst, arg->asTemp(), Assembler::ScratchRegister);
        else
            _as->copyValue(dst, arg);
    }
    return argc;
}

void InstructionSelection::calculateRegistersToSave(const RegisterInformation &used)
{
    regularRegistersToSave.clear();
    fpRegistersToSave.clear();

    for (const RegisterInfo &ri : Assembler::getRegisterInfo()) {
#if defined(RESTORE_EBX_ON_CALL)
        if (ri.isRegularRegister() && ri.reg<JSC::X86Registers::RegisterID>() == JSC::X86Registers::ebx) {
            regularRegistersToSave.append(ri);
            continue;
        }
#endif // RESTORE_EBX_ON_CALL
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

bool InstructionSelection::visitCJumpDouble(IR::AluOp op, IR::Expr *left, IR::Expr *right,
                                            IR::BasicBlock *iftrue, IR::BasicBlock *iffalse)
{
    if (!isPregOrConst(left) || !isPregOrConst(right))
        return false;

    if (_as->nextBlock() == iftrue) {
        Assembler::Jump target = _as->branchDouble(true, op, left, right);
        _as->addPatch(iffalse, target);
    } else {
        Assembler::Jump target = _as->branchDouble(false, op, left, right);
        _as->addPatch(iftrue, target);
        _as->jumpToBlock(_block, iffalse);
    }
    return true;
}

bool InstructionSelection::visitCJumpSInt32(IR::AluOp op, IR::Expr *left, IR::Expr *right,
                                            IR::BasicBlock *iftrue, IR::BasicBlock *iffalse)
{
    if (!isPregOrConst(left) || !isPregOrConst(right))
        return false;

    if (_as->nextBlock() == iftrue) {
        Assembler::Jump target = _as->branchInt32(true, op, left, right);
        _as->addPatch(iffalse, target);
    } else {
        Assembler::Jump target = _as->branchInt32(false, op, left, right);
        _as->addPatch(iftrue, target);
        _as->jumpToBlock(_block, iffalse);
    }
    return true;
}

void InstructionSelection::visitCJumpStrict(IR::Binop *binop, IR::BasicBlock *trueBlock,
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

    generateRuntimeCall(Assembler::ReturnValueRegister, compareStrictEqual,
                                 Assembler::PointerToValue(left), Assembler::PointerToValue(right));
    _as->generateCJumpOnCompare(binop->op == IR::OpStrictEqual ? Assembler::NotEqual : Assembler::Equal,
                                Assembler::ReturnValueRegister, Assembler::TrustedImm32(0),
                                _block, trueBlock, falseBlock);
}

// Only load the non-null temp.
bool InstructionSelection::visitCJumpStrictNull(IR::Binop *binop,
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

    Assembler::Pointer tagAddr = _as->loadAddress(Assembler::ScratchRegister, varSrc);
    tagAddr.offset += 4;
    const Assembler::RegisterID tagReg = Assembler::ScratchRegister;
    _as->load32(tagAddr, tagReg);

    Assembler::RelationalCondition cond = binop->op == IR::OpStrictEqual ? Assembler::Equal
                                                                         : Assembler::NotEqual;
    const Assembler::TrustedImm32 tag(QV4::Value::Null_Type_Internal);
    _as->generateCJumpOnCompare(cond, tagReg, tag, _block, trueBlock, falseBlock);
    return true;
}

bool InstructionSelection::visitCJumpStrictUndefined(IR::Binop *binop,
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

    Assembler::RelationalCondition cond = binop->op == IR::OpStrictEqual ? Assembler::Equal
                                                                         : Assembler::NotEqual;
    const Assembler::RegisterID tagReg = Assembler::ReturnValueRegister;
#ifdef QV4_USE_64_BIT_VALUE_ENCODING
    Assembler::Pointer addr = _as->loadAddress(Assembler::ScratchRegister, varSrc);
    _as->load64(addr, tagReg);
    const Assembler::TrustedImm64 tag(0);
#else // !QV4_USE_64_BIT_VALUE_ENCODING
    Assembler::Pointer tagAddr = _as->loadAddress(Assembler::ScratchRegister, varSrc);
    _as->load32(tagAddr, tagReg);
    Assembler::Jump j = _as->branch32(Assembler::invert(cond), tagReg, Assembler::TrustedImm32(0));
    _as->addPatch(falseBlock, j);

    tagAddr.offset += 4;
    _as->load32(tagAddr, tagReg);
    const Assembler::TrustedImm32 tag(QV4::Value::Managed_Type_Internal);
#endif
    _as->generateCJumpOnCompare(cond, tagReg, tag, _block, trueBlock, falseBlock);
    return true;
}

bool InstructionSelection::visitCJumpStrictBool(IR::Binop *binop, IR::BasicBlock *trueBlock,
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

    Assembler::RelationalCondition cond = binop->op == IR::OpStrictEqual ? Assembler::Equal
                                                                           : Assembler::NotEqual;

    if (otherSrc->type == IR::BoolType) { // both are boolean
        Assembler::RegisterID one = _as->toBoolRegister(boolSrc, Assembler::ReturnValueRegister);
        Assembler::RegisterID two = _as->toBoolRegister(otherSrc, Assembler::ScratchRegister);
        _as->generateCJumpOnCompare(cond, one, two, _block, trueBlock, falseBlock);
        return true;
    }

    if (otherSrc->type != IR::VarType) {
        _as->jumpToBlock(_block, falseBlock);
        return true;
    }

    Assembler::Pointer otherAddr = _as->loadAddress(Assembler::ReturnValueRegister, otherSrc);
    otherAddr.offset += 4; // tag address

    // check if the tag of the var operand is indicates 'boolean'
    _as->load32(otherAddr, Assembler::ScratchRegister);
    Assembler::Jump noBool = _as->branch32(Assembler::NotEqual, Assembler::ScratchRegister,
                                           Assembler::TrustedImm32(QV4::Value::Boolean_Type_Internal));
    if (binop->op == IR::OpStrictEqual)
        _as->addPatch(falseBlock, noBool);
    else
        _as->addPatch(trueBlock, noBool);

    // ok, both are boolean, so let's load them and compare them.
    otherAddr.offset -= 4; // int_32 address
    _as->load32(otherAddr, Assembler::ReturnValueRegister);
    Assembler::RegisterID boolReg = _as->toBoolRegister(boolSrc, Assembler::ScratchRegister);
    _as->generateCJumpOnCompare(cond, boolReg, Assembler::ReturnValueRegister, _block, trueBlock,
                                falseBlock);
    return true;
}

bool InstructionSelection::visitCJumpNullUndefined(IR::Type nullOrUndef, IR::Binop *binop,
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

    Assembler::Pointer tagAddr = _as->loadAddress(Assembler::ScratchRegister, varSrc);
    tagAddr.offset += 4;
    const Assembler::RegisterID tagReg = Assembler::ReturnValueRegister;
    _as->load32(tagAddr, tagReg);

    if (binop->op == IR::OpNotEqual)
        qSwap(trueBlock, falseBlock);
    Assembler::Jump isNull = _as->branch32(Assembler::Equal, tagReg, Assembler::TrustedImm32(int(QV4::Value::Null_Type_Internal)));
    Assembler::Jump isNotUndefinedTag = _as->branch32(Assembler::NotEqual, tagReg, Assembler::TrustedImm32(int(QV4::Value::Managed_Type_Internal)));
    tagAddr.offset -= 4;
    _as->load32(tagAddr, tagReg);
    Assembler::Jump isNotUndefinedValue = _as->branch32(Assembler::NotEqual, tagReg, Assembler::TrustedImm32(0));
    _as->addPatch(trueBlock, isNull);
    _as->addPatch(falseBlock, isNotUndefinedTag);
    _as->addPatch(falseBlock, isNotUndefinedValue);
    _as->jumpToBlock(_block, trueBlock);

    return true;
}


void InstructionSelection::visitCJumpEqual(IR::Binop *binop, IR::BasicBlock *trueBlock,
                                            IR::BasicBlock *falseBlock)
{
    Q_ASSERT(binop->op == IR::OpEqual || binop->op == IR::OpNotEqual);

    if (visitCJumpNullUndefined(IR::NullType, binop, trueBlock, falseBlock))
        return;

    IR::Expr *left = binop->left;
    IR::Expr *right = binop->right;

    generateRuntimeCall(Assembler::ReturnValueRegister, compareEqual,
                                 Assembler::PointerToValue(left), Assembler::PointerToValue(right));
    _as->generateCJumpOnCompare(binop->op == IR::OpEqual ? Assembler::NotEqual : Assembler::Equal,
                                Assembler::ReturnValueRegister, Assembler::TrustedImm32(0),
                                _block, trueBlock, falseBlock);
}

QQmlRefPointer<CompiledData::CompilationUnit> ISelFactory::createUnitForLoading()
{
    QQmlRefPointer<CompiledData::CompilationUnit> result;
    result.adopt(new JIT::CompilationUnit);
    return result;
}

#endif // ENABLE(ASSEMBLER)
