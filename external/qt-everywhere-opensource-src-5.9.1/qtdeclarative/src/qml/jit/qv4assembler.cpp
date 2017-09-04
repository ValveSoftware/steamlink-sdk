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
#include "qv4ssa_p.h"
#include "qv4regalloc_p.h"
#include "qv4assembler_p.h"

#include <assembler/LinkBuffer.h>
#include <WTFStubs.h>

#if !defined(V4_BOOTSTRAP)
#include "qv4function_p.h"
#endif

#include <iostream>
#include <QBuffer>
#include <QCoreApplication>

#if ENABLE(ASSEMBLER)

#if USE(UDIS86)
#  include <udis86.h>
#endif

using namespace QV4;
using namespace QV4::JIT;

CompilationUnit::~CompilationUnit()
{
}

#if !defined(V4_BOOTSTRAP)

void CompilationUnit::linkBackendToEngine(ExecutionEngine *engine)
{
    runtimeFunctions.resize(data->functionTableSize);
    runtimeFunctions.fill(0);
    for (int i = 0 ;i < runtimeFunctions.size(); ++i) {
        const CompiledData::Function *compiledFunction = data->functionAt(i);

        QV4::Function *runtimeFunction = new QV4::Function(engine, this, compiledFunction,
                                                           (ReturnedValue (*)(QV4::ExecutionEngine *, const uchar *)) codeRefs[i].code().executableAddress());
        runtimeFunctions[i] = runtimeFunction;
    }
}

bool CompilationUnit::memoryMapCode(QString *errorString)
{
    Q_UNUSED(errorString);
    codeRefs.resize(data->functionTableSize);

    const char *basePtr = reinterpret_cast<const char *>(data);

    for (uint i = 0; i < data->functionTableSize; ++i) {
        const CompiledData::Function *compiledFunction = data->functionAt(i);
        void *codePtr = const_cast<void *>(reinterpret_cast<const void *>(basePtr + compiledFunction->codeOffset));
        JSC::MacroAssemblerCodeRef codeRef = JSC::MacroAssemblerCodeRef::createSelfManagedCodeRef(JSC::MacroAssemblerCodePtr(codePtr));
        JSC::ExecutableAllocator::makeExecutable(codePtr, compiledFunction->codeSize);
        codeRefs[i] = codeRef;

        static const bool showCode = qEnvironmentVariableIsSet("QV4_SHOW_ASM");
        if (showCode) {
            WTF::dataLogF("Mapped JIT code for %s\n", qPrintable(stringAt(compiledFunction->nameIndex)));
            disassemble(codeRef.code(), compiledFunction->codeSize, "    ", WTF::dataFile());
        }
    }

    return true;
}

#endif // !defined(V4_BOOTSTRAP)

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

        const void *undecoratedCodePtr = codeRefs.at(i).code().dataLocation();
        written = device->write(reinterpret_cast<const char *>(undecoratedCodePtr), compiledFunction->codeSize);
        if (written != qint64(compiledFunction->codeSize)) {
            *errorString = device->errorString();
            return false;
        }
    }
    return true;
}

template <typename TargetConfiguration>
Assembler<TargetConfiguration>::Assembler(QV4::Compiler::JSUnitGenerator *jsGenerator, IR::Function* function, QV4::ExecutableAllocator *executableAllocator)
    : _function(function)
    , _nextBlock(0)
    , _executableAllocator(executableAllocator)
    , _jsGenerator(jsGenerator)
{
    _addrs.resize(_function->basicBlockCount());
    _patches.resize(_function->basicBlockCount());
    _labelPatches.resize(_function->basicBlockCount());
}

template <typename TargetConfiguration>
void Assembler<TargetConfiguration>::registerBlock(IR::BasicBlock* block, IR::BasicBlock *nextBlock)
{
    _addrs[block->index()] = label();
    catchBlock = block->catchBlock;
    _nextBlock = nextBlock;
}

template <typename TargetConfiguration>
void Assembler<TargetConfiguration>::jumpToBlock(IR::BasicBlock* current, IR::BasicBlock *target)
{
    Q_UNUSED(current);

    if (target != _nextBlock)
        _patches[target->index()].push_back(jump());
}

template <typename TargetConfiguration>
void Assembler<TargetConfiguration>::addPatch(IR::BasicBlock* targetBlock, Jump targetJump)
{
    _patches[targetBlock->index()].push_back(targetJump);
}

template <typename TargetConfiguration>
void Assembler<TargetConfiguration>::addPatch(DataLabelPtr patch, Label target)
{
    DataLabelPatch p;
    p.dataLabel = patch;
    p.target = target;
    _dataLabelPatches.push_back(p);
}

template <typename TargetConfiguration>
void Assembler<TargetConfiguration>::addPatch(DataLabelPtr patch, IR::BasicBlock *target)
{
    _labelPatches[target->index()].push_back(patch);
}

template <typename TargetConfiguration>
void Assembler<TargetConfiguration>::generateCJumpOnNonZero(RegisterID reg, IR::BasicBlock *currentBlock,
                                       IR::BasicBlock *trueBlock, IR::BasicBlock *falseBlock)
{
    generateCJumpOnCompare(RelationalCondition::NotEqual, reg, TrustedImm32(0), currentBlock, trueBlock, falseBlock);
}

template <typename TargetConfiguration>
void Assembler<TargetConfiguration>::generateCJumpOnCompare(RelationalCondition cond,
                                       RegisterID left,
                                       TrustedImm32 right,
                                       IR::BasicBlock *currentBlock,
                                       IR::BasicBlock *trueBlock,
                                       IR::BasicBlock *falseBlock)
{
    if (trueBlock == _nextBlock) {
        Jump target = branch32(invert(cond), left, right);
        addPatch(falseBlock, target);
    } else {
        Jump target = branch32(cond, left, right);
        addPatch(trueBlock, target);
        jumpToBlock(currentBlock, falseBlock);
    }
}

template <typename TargetConfiguration>
void Assembler<TargetConfiguration>::generateCJumpOnCompare(RelationalCondition cond,
                                       RegisterID left,
                                       RegisterID right,
                                       IR::BasicBlock *currentBlock,
                                       IR::BasicBlock *trueBlock,
                                       IR::BasicBlock *falseBlock)
{
    if (trueBlock == _nextBlock) {
        Jump target = branch32(invert(cond), left, right);
        addPatch(falseBlock, target);
    } else {
        Jump target = branch32(cond, left, right);
        addPatch(trueBlock, target);
        jumpToBlock(currentBlock, falseBlock);
    }
}

template <typename TargetConfiguration>
typename Assembler<TargetConfiguration>::Pointer Assembler<TargetConfiguration>::loadAddress(RegisterID tmp, IR::Expr *e)
{
    IR::Temp *t = e->asTemp();
    if (t)
        return loadTempAddress(t);
    else
        return loadArgLocalAddress(tmp, e->asArgLocal());
}

template <typename TargetConfiguration>
typename Assembler<TargetConfiguration>::Pointer Assembler<TargetConfiguration>::loadTempAddress(IR::Temp *t)
{
    if (t->kind == IR::Temp::StackSlot)
        return stackSlotPointer(t);
    else
        Q_UNREACHABLE();
}

template <typename TargetConfiguration>
typename Assembler<TargetConfiguration>::Pointer Assembler<TargetConfiguration>::loadArgLocalAddress(RegisterID baseReg, IR::ArgLocal *al)
{
    int32_t offset = 0;
    int scope = al->scope;
    loadPtr(Address(EngineRegister, targetStructureOffset(offsetof(EngineBase, current))), baseReg);

    const qint32 outerOffset = targetStructureOffset(Heap::ExecutionContext::baseOffset + offsetof(Heap::ExecutionContextData, outer));

    if (scope) {
        loadPtr(Address(baseReg, outerOffset), baseReg);
        --scope;
        while (scope) {
            loadPtr(Address(baseReg, outerOffset), baseReg);
            --scope;
        }
    }
    switch (al->kind) {
    case IR::ArgLocal::Formal:
    case IR::ArgLocal::ScopedFormal: {
        const qint32 callDataOffset = targetStructureOffset(Heap::ExecutionContext::baseOffset + offsetof(Heap::ExecutionContextData, callData));
        loadPtr(Address(baseReg, callDataOffset), baseReg);
        offset = sizeof(CallData) + (al->index - 1) * sizeof(Value);
    } break;
    case IR::ArgLocal::Local:
    case IR::ArgLocal::ScopedLocal: {
        const qint32 localsOffset = targetStructureOffset(Heap::CallContext::baseOffset + offsetof(Heap::CallContextData, locals));
        loadPtr(Address(baseReg, localsOffset), baseReg);
        offset = al->index * sizeof(Value);
    } break;
    default:
        Q_UNREACHABLE();
    }
    return Pointer(baseReg, offset);
}

template <typename TargetConfiguration>
typename Assembler<TargetConfiguration>::Pointer Assembler<TargetConfiguration>::loadStringAddress(RegisterID reg, const QString &string)
{
    loadPtr(Address(Assembler::EngineRegister, targetStructureOffset(offsetof(QV4::EngineBase, current))), Assembler::ScratchRegister);
    loadPtr(Address(Assembler::ScratchRegister, targetStructureOffset(Heap::ExecutionContext::baseOffset + offsetof(Heap::ExecutionContextData, compilationUnit))), Assembler::ScratchRegister);
    loadPtr(Address(Assembler::ScratchRegister, offsetof(CompiledData::CompilationUnitBase, runtimeStrings)), reg);
    const int id = _jsGenerator->registerString(string);
    return Pointer(reg, id * RegisterSize);
}

template <typename TargetConfiguration>
typename Assembler<TargetConfiguration>::Address Assembler<TargetConfiguration>::loadConstant(IR::Const *c, RegisterID baseReg)
{
    return loadConstant(convertToValue<TargetPrimitive>(c), baseReg);
}

template <typename TargetConfiguration>
typename Assembler<TargetConfiguration>::Address Assembler<TargetConfiguration>::loadConstant(const TargetPrimitive &v, RegisterID baseReg)
{
    loadPtr(Address(Assembler::EngineRegister, targetStructureOffset(offsetof(QV4::EngineBase, current))), baseReg);
    loadPtr(Address(baseReg, targetStructureOffset(Heap::ExecutionContext::baseOffset + offsetof(Heap::ExecutionContextData, constantTable))), baseReg);
    const int index = _jsGenerator->registerConstant(v.rawValue());
    return Address(baseReg, index * sizeof(QV4::Value));
}

template <typename TargetConfiguration>
void Assembler<TargetConfiguration>::loadStringRef(RegisterID reg, const QString &string)
{
    const int id = _jsGenerator->registerString(string);
    move(TrustedImm32(id), reg);
}

template <typename TargetConfiguration>
void Assembler<TargetConfiguration>::storeValue(TargetPrimitive value, IR::Expr *destination)
{
    Address addr = loadAddress(ScratchRegister, destination);
    storeValue(value, addr);
}

template <typename TargetConfiguration>
void Assembler<TargetConfiguration>::enterStandardStackFrame(const RegisterInformation &regularRegistersToSave,
                                        const RegisterInformation &fpRegistersToSave)
{
    platformEnterStandardStackFrame(this);

    move(StackPointerRegister, JITTargetPlatform::FramePointerRegister);

    const int frameSize = _stackLayout->calculateStackFrameSize();
    subPtr(TrustedImm32(frameSize), StackPointerRegister);

    Address slotAddr(JITTargetPlatform::FramePointerRegister, 0);
    for (int i = 0, ei = fpRegistersToSave.size(); i < ei; ++i) {
        Q_ASSERT(fpRegistersToSave.at(i).isFloatingPoint());
        slotAddr.offset -= sizeof(double);
        TargetConfiguration::MacroAssembler::storeDouble(fpRegistersToSave.at(i).reg<FPRegisterID>(), slotAddr);
    }
    for (int i = 0, ei = regularRegistersToSave.size(); i < ei; ++i) {
        Q_ASSERT(regularRegistersToSave.at(i).isRegularRegister());
        slotAddr.offset -= RegisterSize;
        storePtr(regularRegistersToSave.at(i).reg<RegisterID>(), slotAddr);
    }

    platformFinishEnteringStandardStackFrame(this);
}

template <typename TargetConfiguration>
void Assembler<TargetConfiguration>::leaveStandardStackFrame(const RegisterInformation &regularRegistersToSave,
                                        const RegisterInformation &fpRegistersToSave)
{
    Address slotAddr(JITTargetPlatform::FramePointerRegister, -regularRegistersToSave.size() * RegisterSize - fpRegistersToSave.size() * sizeof(double));

    // restore the callee saved registers
    for (int i = regularRegistersToSave.size() - 1; i >= 0; --i) {
        Q_ASSERT(regularRegistersToSave.at(i).isRegularRegister());
        loadPtr(slotAddr, regularRegistersToSave.at(i).reg<RegisterID>());
        slotAddr.offset += RegisterSize;
    }
    for (int i = fpRegistersToSave.size() - 1; i >= 0; --i) {
        Q_ASSERT(fpRegistersToSave.at(i).isFloatingPoint());
        TargetConfiguration::MacroAssembler::loadDouble(slotAddr, fpRegistersToSave.at(i).reg<FPRegisterID>());
        slotAddr.offset += sizeof(double);
    }

    Q_ASSERT(slotAddr.offset == 0);

    const int frameSize = _stackLayout->calculateStackFrameSize();
    platformLeaveStandardStackFrame(this, frameSize);
}




// Try to load the source expression into the destination FP register. This assumes that two
// general purpose (integer) registers are available: the ScratchRegister and the
// ReturnValueRegister. It returns a Jump if no conversion can be performed.
template <typename TargetConfiguration>
typename Assembler<TargetConfiguration>::Jump Assembler<TargetConfiguration>::genTryDoubleConversion(IR::Expr *src, FPRegisterID dest)
{
    switch (src->type) {
    case IR::DoubleType:
        moveDouble(toDoubleRegister(src, dest), dest);
        return Assembler::Jump();
    case IR::SInt32Type:
        convertInt32ToDouble(toInt32Register(src, Assembler::ScratchRegister),
                                  dest);
        return Assembler::Jump();
    case IR::UInt32Type:
        convertUInt32ToDouble(toUInt32Register(src, Assembler::ScratchRegister),
                                   dest, Assembler::ReturnValueRegister);
        return Assembler::Jump();
    case IR::NullType:
    case IR::UndefinedType:
    case IR::BoolType:
        // TODO?
    case IR::StringType:
        return jump();
    default:
        break;
    }

    Q_ASSERT(src->asTemp() || src->asArgLocal());

    // It's not a number type, so it cannot be in a register.
    Q_ASSERT(src->asArgLocal() || src->asTemp()->kind != IR::Temp::PhysicalRegister || src->type == IR::BoolType);

    Assembler::Pointer tagAddr = loadAddress(Assembler::ScratchRegister, src);
    tagAddr.offset += 4;
    load32(tagAddr, Assembler::ScratchRegister);

    // check if it's an int32:
    Assembler::Jump isNoInt = branch32(Assembler::NotEqual, Assembler::ScratchRegister,
                                       Assembler::TrustedImm32(quint32(ValueTypeInternal::Integer)));
    convertInt32ToDouble(toInt32Register(src, Assembler::ScratchRegister), dest);
    Assembler::Jump intDone = jump();

    // not an int, check if it's a double:
    isNoInt.link(this);
    Assembler::Jump isNoDbl = RegisterSizeDependentOps::checkIfTagRegisterIsDouble(this, ScratchRegister);
    toDoubleRegister(src, dest);
    intDone.link(this);

    return isNoDbl;
}

template <typename TargetConfiguration>
typename Assembler<TargetConfiguration>::Jump Assembler<TargetConfiguration>::branchDouble(bool invertCondition, IR::AluOp op,
                                                   IR::Expr *left, IR::Expr *right)
{
    DoubleCondition cond;
    switch (op) {
    case IR::OpGt: cond = Assembler::DoubleGreaterThan; break;
    case IR::OpLt: cond = Assembler::DoubleLessThan; break;
    case IR::OpGe: cond = Assembler::DoubleGreaterThanOrEqual; break;
    case IR::OpLe: cond = Assembler::DoubleLessThanOrEqual; break;
    case IR::OpEqual:
    case IR::OpStrictEqual: cond = Assembler::DoubleEqual; break;
    case IR::OpNotEqual:
    case IR::OpStrictNotEqual: cond = Assembler::DoubleNotEqualOrUnordered; break; // No, the inversion of DoubleEqual is NOT DoubleNotEqual.
    default:
        Q_UNREACHABLE();
    }
    if (invertCondition)
        cond = TargetConfiguration::MacroAssembler::invert(cond);

    return TargetConfiguration::MacroAssembler::branchDouble(cond, toDoubleRegister(left, FPGpr0), toDoubleRegister(right, JITTargetPlatform::FPGpr1));
}

template <typename TargetConfiguration>
typename Assembler<TargetConfiguration>::Jump Assembler<TargetConfiguration>::branchInt32(bool invertCondition, IR::AluOp op, IR::Expr *left, IR::Expr *right)
{
    Assembler::RelationalCondition cond;
    switch (op) {
    case IR::OpGt: cond = Assembler::GreaterThan; break;
    case IR::OpLt: cond = Assembler::LessThan; break;
    case IR::OpGe: cond = Assembler::GreaterThanOrEqual; break;
    case IR::OpLe: cond = Assembler::LessThanOrEqual; break;
    case IR::OpEqual:
    case IR::OpStrictEqual: cond = Assembler::Equal; break;
    case IR::OpNotEqual:
    case IR::OpStrictNotEqual: cond = Assembler::NotEqual; break;
    default:
        Q_UNREACHABLE();
    }
    if (invertCondition)
        cond = TargetConfiguration::MacroAssembler::invert(cond);

    return TargetConfiguration::MacroAssembler::branch32(cond,
                                                         toInt32Register(left, Assembler::ScratchRegister),
                                                         toInt32Register(right, Assembler::ReturnValueRegister));
}

template <typename TargetConfiguration>
void Assembler<TargetConfiguration>::setStackLayout(int maxArgCountForBuiltins, int regularRegistersToSave, int fpRegistersToSave)
{
    _stackLayout.reset(new StackLayout(_function, maxArgCountForBuiltins, regularRegistersToSave, fpRegistersToSave));
}

template <typename TargetConfiguration>
void Assembler<TargetConfiguration>::returnFromFunction(IR::Ret *s, RegisterInformation regularRegistersToSave, RegisterInformation fpRegistersToSave)
{
    if (!s) {
        // this only happens if the method doesn't have a return statement and can
        // only exit through an exception
    } else if (IR::Temp *t = s->expr->asTemp()) {
        RegisterSizeDependentOps::setFunctionReturnValueFromTemp(this, t);
    } else if (IR::Const *c = s->expr->asConst()) {
        auto retVal = convertToValue<TargetPrimitive>(c);
        RegisterSizeDependentOps::setFunctionReturnValueFromConst(this, retVal);
    } else {
        Q_UNREACHABLE();
        Q_UNUSED(s);
    }

    Label leaveStackFrame = label();

    const int locals = stackLayout().calculateJSStackFrameSize();
    subPtr(TrustedImm32(sizeof(QV4::Value)*locals), JITTargetPlatform::LocalsRegister);
    storePtr(JITTargetPlatform::LocalsRegister, Address(JITTargetPlatform::EngineRegister, targetStructureOffset(offsetof(EngineBase, jsStackTop))));

    leaveStandardStackFrame(regularRegistersToSave, fpRegistersToSave);
    ret();

    exceptionReturnLabel = label();
    auto retVal = TargetPrimitive::undefinedValue();
    RegisterSizeDependentOps::setFunctionReturnValueFromConst(this, retVal);
    jump(leaveStackFrame);
}

namespace {
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
        const QByteArray ptrString = "0x" + QByteArray::number(quintptr(it.key()), 16);
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

template <typename TargetConfiguration>
JSC::MacroAssemblerCodeRef Assembler<TargetConfiguration>::link(int *codeSize)
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
    JSC::LinkBuffer<typename TargetConfiguration::MacroAssembler> linkBuffer(dummy, this, 0);

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
        if (name.isEmpty())
            name = "IR::Function(0x" + QByteArray::number(quintptr(_function), 16) + ')';
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
            if (name.isEmpty())
                name = "IR::Function(0x" + QByteArray::number(quintptr(_function), 16) + ')';
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

template class QV4::JIT::Assembler<DefaultAssemblerTargetConfiguration>;
#if defined(V4_BOOTSTRAP)
#if !CPU(ARM_THUMB2)
template class QV4::JIT::Assembler<AssemblerTargetConfiguration<JSC::MacroAssemblerARMv7, NoOperatingSystemSpecialization>>;
#endif
#if !CPU(ARM64)
template class QV4::JIT::Assembler<AssemblerTargetConfiguration<JSC::MacroAssemblerARM64, NoOperatingSystemSpecialization>>;
#endif
#endif

#endif
