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

#include <assembler/LinkBuffer.h>
#include <WTFStubs.h>

#include <iostream>

#if ENABLE(ASSEMBLER)

#if USE(UDIS86)
#  include <udis86.h>
#endif

using namespace QV4;
using namespace QV4::JIT;

CompilationUnit::~CompilationUnit()
{
}

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
    }

    return true;
}

const Assembler::VoidType Assembler::Void;

Assembler::Assembler(InstructionSelection *isel, IR::Function* function, QV4::ExecutableAllocator *executableAllocator)
    : _function(function)
    , _nextBlock(0)
    , _executableAllocator(executableAllocator)
    , _isel(isel)
{
    _addrs.resize(_function->basicBlockCount());
    _patches.resize(_function->basicBlockCount());
    _labelPatches.resize(_function->basicBlockCount());
}

void Assembler::registerBlock(IR::BasicBlock* block, IR::BasicBlock *nextBlock)
{
    _addrs[block->index()] = label();
    catchBlock = block->catchBlock;
    _nextBlock = nextBlock;
}

void Assembler::jumpToBlock(IR::BasicBlock* current, IR::BasicBlock *target)
{
    Q_UNUSED(current);

    if (target != _nextBlock)
        _patches[target->index()].push_back(jump());
}

void Assembler::addPatch(IR::BasicBlock* targetBlock, Jump targetJump)
{
    _patches[targetBlock->index()].push_back(targetJump);
}

void Assembler::addPatch(DataLabelPtr patch, Label target)
{
    DataLabelPatch p;
    p.dataLabel = patch;
    p.target = target;
    _dataLabelPatches.push_back(p);
}

void Assembler::addPatch(DataLabelPtr patch, IR::BasicBlock *target)
{
    _labelPatches[target->index()].push_back(patch);
}

void Assembler::generateCJumpOnNonZero(RegisterID reg, IR::BasicBlock *currentBlock,
                                       IR::BasicBlock *trueBlock, IR::BasicBlock *falseBlock)
{
    generateCJumpOnCompare(NotEqual, reg, TrustedImm32(0), currentBlock, trueBlock, falseBlock);
}

#ifdef QV4_USE_64_BIT_VALUE_ENCODING
void Assembler::generateCJumpOnCompare(RelationalCondition cond,
                                       RegisterID left,
                                       TrustedImm64 right,
                                       IR::BasicBlock *currentBlock,
                                       IR::BasicBlock *trueBlock,
                                       IR::BasicBlock *falseBlock)
{
    if (trueBlock == _nextBlock) {
        Jump target = branch64(invert(cond), left, right);
        addPatch(falseBlock, target);
    } else {
        Jump target = branch64(cond, left, right);
        addPatch(trueBlock, target);
        jumpToBlock(currentBlock, falseBlock);
    }
}
#endif

void Assembler::generateCJumpOnCompare(RelationalCondition cond,
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

void Assembler::generateCJumpOnCompare(RelationalCondition cond,
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

Assembler::Pointer Assembler::loadAddress(RegisterID tmp, IR::Expr *e)
{
    IR::Temp *t = e->asTemp();
    if (t)
        return loadTempAddress(t);
    else
        return loadArgLocalAddress(tmp, e->asArgLocal());
}

Assembler::Pointer Assembler::loadTempAddress(IR::Temp *t)
{
    if (t->kind == IR::Temp::StackSlot)
        return stackSlotPointer(t);
    else
        Q_UNREACHABLE();
}

Assembler::Pointer Assembler::loadArgLocalAddress(RegisterID baseReg, IR::ArgLocal *al)
{
    int32_t offset = 0;
    int scope = al->scope;
    loadPtr(Address(EngineRegister, qOffsetOf(ExecutionEngine, current)), baseReg);
    if (scope) {
        loadPtr(Address(baseReg, qOffsetOf(ExecutionContext::Data, outer)), baseReg);
        --scope;
        while (scope) {
            loadPtr(Address(baseReg, qOffsetOf(ExecutionContext::Data, outer)), baseReg);
            --scope;
        }
    }
    switch (al->kind) {
    case IR::ArgLocal::Formal:
    case IR::ArgLocal::ScopedFormal: {
        loadPtr(Address(baseReg, qOffsetOf(ExecutionContext::Data, callData)), baseReg);
        offset = sizeof(CallData) + (al->index - 1) * sizeof(Value);
    } break;
    case IR::ArgLocal::Local:
    case IR::ArgLocal::ScopedLocal: {
        loadPtr(Address(baseReg, qOffsetOf(CallContext::Data, locals)), baseReg);
        offset = al->index * sizeof(Value);
    } break;
    default:
        Q_UNREACHABLE();
    }
    return Pointer(baseReg, offset);
}

Assembler::Pointer Assembler::loadStringAddress(RegisterID reg, const QString &string)
{
    loadPtr(Address(Assembler::EngineRegister, qOffsetOf(QV4::ExecutionEngine, current)), Assembler::ScratchRegister);
    loadPtr(Address(Assembler::ScratchRegister, qOffsetOf(QV4::Heap::ExecutionContext, compilationUnit)), Assembler::ScratchRegister);
    loadPtr(Address(Assembler::ScratchRegister, qOffsetOf(QV4::CompiledData::CompilationUnit, runtimeStrings)), reg);
    const int id = _isel->registerString(string);
    return Pointer(reg, id * sizeof(QV4::String*));
}

Assembler::Address Assembler::loadConstant(IR::Const *c, RegisterID baseReg)
{
    return loadConstant(convertToValue(c), baseReg);
}

Assembler::Address Assembler::loadConstant(const Primitive &v, RegisterID baseReg)
{
    loadPtr(Address(Assembler::EngineRegister, qOffsetOf(QV4::ExecutionEngine, current)), baseReg);
    loadPtr(Address(baseReg, qOffsetOf(QV4::Heap::ExecutionContext, constantTable)), baseReg);
    const int index = _isel->jsUnitGenerator()->registerConstant(v.asReturnedValue());
    return Address(baseReg, index * sizeof(QV4::Value));
}

void Assembler::loadStringRef(RegisterID reg, const QString &string)
{
    const int id = _isel->registerString(string);
    move(TrustedImm32(id), reg);
}

void Assembler::storeValue(QV4::Primitive value, IR::Expr *destination)
{
    Address addr = loadAddress(ScratchRegister, destination);
    storeValue(value, addr);
}

void Assembler::enterStandardStackFrame(const RegisterInformation &regularRegistersToSave,
                                        const RegisterInformation &fpRegistersToSave)
{
    platformEnterStandardStackFrame(this);

    move(StackPointerRegister, FramePointerRegister);

    const int frameSize = _stackLayout->calculateStackFrameSize();
    subPtr(TrustedImm32(frameSize), StackPointerRegister);

    Address slotAddr(FramePointerRegister, 0);
    for (int i = 0, ei = fpRegistersToSave.size(); i < ei; ++i) {
        Q_ASSERT(fpRegistersToSave.at(i).isFloatingPoint());
        slotAddr.offset -= sizeof(double);
        JSC::MacroAssembler::storeDouble(fpRegistersToSave.at(i).reg<FPRegisterID>(), slotAddr);
    }
    for (int i = 0, ei = regularRegistersToSave.size(); i < ei; ++i) {
        Q_ASSERT(regularRegistersToSave.at(i).isRegularRegister());
        slotAddr.offset -= RegisterSize;
        storePtr(regularRegistersToSave.at(i).reg<RegisterID>(), slotAddr);
    }
}

void Assembler::leaveStandardStackFrame(const RegisterInformation &regularRegistersToSave,
                                        const RegisterInformation &fpRegistersToSave)
{
    Address slotAddr(FramePointerRegister, -regularRegistersToSave.size() * RegisterSize - fpRegistersToSave.size() * sizeof(double));

    // restore the callee saved registers
    for (int i = regularRegistersToSave.size() - 1; i >= 0; --i) {
        Q_ASSERT(regularRegistersToSave.at(i).isRegularRegister());
        loadPtr(slotAddr, regularRegistersToSave.at(i).reg<RegisterID>());
        slotAddr.offset += RegisterSize;
    }
    for (int i = fpRegistersToSave.size() - 1; i >= 0; --i) {
        Q_ASSERT(fpRegistersToSave.at(i).isFloatingPoint());
        JSC::MacroAssembler::loadDouble(slotAddr, fpRegistersToSave.at(i).reg<FPRegisterID>());
        slotAddr.offset += sizeof(double);
    }

    Q_ASSERT(slotAddr.offset == 0);

    const int frameSize = _stackLayout->calculateStackFrameSize();
    // Work around bug in ARMv7Assembler.h where add32(imm, sp, sp) doesn't
    // work well for large immediates.
#if CPU(ARM_THUMB2)
    move(TrustedImm32(frameSize), JSC::ARMRegisters::r3);
    add32(JSC::ARMRegisters::r3, StackPointerRegister);
#else
    addPtr(TrustedImm32(frameSize), StackPointerRegister);
#endif

    platformLeaveStandardStackFrame(this);
}




// Try to load the source expression into the destination FP register. This assumes that two
// general purpose (integer) registers are available: the ScratchRegister and the
// ReturnValueRegister. It returns a Jump if no conversion can be performed.
Assembler::Jump Assembler::genTryDoubleConversion(IR::Expr *src, Assembler::FPRegisterID dest)
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
                                            Assembler::TrustedImm32(Value::Integer_Type_Internal));
    convertInt32ToDouble(toInt32Register(src, Assembler::ScratchRegister), dest);
    Assembler::Jump intDone = jump();

    // not an int, check if it's a double:
    isNoInt.link(this);
#ifdef QV4_USE_64_BIT_VALUE_ENCODING
    rshift32(TrustedImm32(Value::IsDoubleTag_Shift), ScratchRegister);
    Assembler::Jump isNoDbl = branch32(Equal, ScratchRegister, TrustedImm32(0));
#else
    and32(Assembler::TrustedImm32(Value::NotDouble_Mask), Assembler::ScratchRegister);
    Assembler::Jump isNoDbl = branch32(Assembler::Equal, Assembler::ScratchRegister,
                                            Assembler::TrustedImm32(Value::NotDouble_Mask));
#endif
    toDoubleRegister(src, dest);
    intDone.link(this);

    return isNoDbl;
}

Assembler::Jump Assembler::branchDouble(bool invertCondition, IR::AluOp op,
                                                   IR::Expr *left, IR::Expr *right)
{
    Assembler::DoubleCondition cond;
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
        cond = JSC::MacroAssembler::invert(cond);

    return JSC::MacroAssembler::branchDouble(cond, toDoubleRegister(left, FPGpr0), toDoubleRegister(right, FPGpr1));
}

Assembler::Jump Assembler::branchInt32(bool invertCondition, IR::AluOp op, IR::Expr *left, IR::Expr *right)
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
        cond = JSC::MacroAssembler::invert(cond);

    return JSC::MacroAssembler::branch32(cond,
                                         toInt32Register(left, Assembler::ScratchRegister),
                                         toInt32Register(right, Assembler::ReturnValueRegister));
}

void Assembler::setStackLayout(int maxArgCountForBuiltins, int regularRegistersToSave, int fpRegistersToSave)
{
    _stackLayout.reset(new StackLayout(_function, maxArgCountForBuiltins, regularRegistersToSave, fpRegistersToSave));
}

#endif
