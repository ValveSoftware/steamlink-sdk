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
#ifndef QV4ASSEMBLER_P_H
#define QV4ASSEMBLER_P_H

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

#include "private/qv4global_p.h"
#include "private/qv4jsir_p.h"
#include "private/qv4isel_p.h"
#include "private/qv4isel_util_p.h"
#include "private/qv4value_p.h"
#include "private/qv4lookup_p.h"
#include "qv4targetplatform_p.h"

#include <config.h>
#include <wtf/Vector.h>

#include <climits>

#if ENABLE(ASSEMBLER)

#include <assembler/MacroAssembler.h>
#include <assembler/MacroAssemblerCodeRef.h>

QT_BEGIN_NAMESPACE

namespace QV4 {
namespace JIT {

class InstructionSelection;

struct CompilationUnit : public QV4::CompiledData::CompilationUnit
{
    virtual ~CompilationUnit();

    void linkBackendToEngine(QV4::ExecutionEngine *engine) Q_DECL_OVERRIDE;
    void prepareCodeOffsetsForDiskStorage(CompiledData::Unit *unit) Q_DECL_OVERRIDE;
    bool saveCodeToDisk(QIODevice *device, const CompiledData::Unit *unit, QString *errorString) Q_DECL_OVERRIDE;
    bool memoryMapCode(QString *errorString) Q_DECL_OVERRIDE;

    // Coderef + execution engine

    QVector<JSC::MacroAssemblerCodeRef> codeRefs;
};

struct LookupCall {
    JSC::MacroAssembler::Address addr;
    uint getterSetterOffset;

    LookupCall(const JSC::MacroAssembler::Address &addr, uint getterSetterOffset)
        : addr(addr)
        , getterSetterOffset(getterSetterOffset)
    {}
};

struct RuntimeCall {
    JSC::MacroAssembler::Address addr;

    inline RuntimeCall(uint offset = uint(INT_MIN));
    bool isValid() const { return addr.offset >= 0; }
};

class Assembler : public JSC::MacroAssembler, public TargetPlatform
{
    Q_DISABLE_COPY(Assembler)

public:
    Assembler(InstructionSelection *isel, IR::Function* function, QV4::ExecutableAllocator *executableAllocator);

    // Explicit type to allow distinguishing between
    // pushing an address itself or the value it points
    // to onto the stack when calling functions.
    struct Pointer : public Address
    {
        explicit Pointer(const Address& addr)
            : Address(addr)
        {}
        explicit Pointer(RegisterID reg, int32_t offset)
            : Address(reg, offset)
        {}
    };

    // V4 uses two stacks: one stack with QV4::Value items, which is checked by the garbage
    // collector, and one stack used by the native C/C++/ABI code. This C++ stack is not scanned
    // by the garbage collector, so if any JS object needs to be retained, it should be put on the
    // JS stack.
    //
    // The "saved reg arg X" are on the C++ stack is used to store values in registers that need to
    // be passed by reference to native functions. It is fine to use the C++ stack, because only
    // non-object values can be stored in registers.
    //
    // Stack layout for the C++ stack:
    //   return address
    //   old FP                     <- FP
    //   callee saved reg n
    //   ...
    //   callee saved reg 0
    //   saved reg arg n
    //   ...
    //   saved reg arg 0            <- SP
    //
    // Stack layout for the JS stack:
    //   function call argument n   <- LocalsRegister
    //   ...
    //   function call argument 0
    //   local 0
    //   ...
    //   local n
    class StackLayout
    {
    public:
        StackLayout(IR::Function *function, int maxArgCountForBuiltins, int normalRegistersToSave, int fpRegistersToSave)
            : normalRegistersToSave(normalRegistersToSave)
            , fpRegistersToSave(fpRegistersToSave)
            , maxOutgoingArgumentCount(function->maxNumberOfArguments)
            , localCount(function->tempCount)
            , savedRegCount(maxArgCountForBuiltins)
        {
#if 0 // debug code
            qDebug("calleeSavedRegCount.....: %d",calleeSavedRegCount);
            qDebug("maxOutgoingArgumentCount: %d",maxOutgoingArgumentCount);
            qDebug("localCount..............: %d",localCount);
            qDebug("savedConstCount.........: %d",savedRegCount);
            for (int i = 0; i < maxOutgoingArgumentCount; ++i)
                qDebug("argumentAddressForCall(%d) = 0x%x / -0x%x", i,
                       argumentAddressForCall(i).offset, -argumentAddressForCall(i).offset);
            for (int i = 0; i < localCount; ++i)
                qDebug("local(%d) = 0x%x / -0x%x", i, stackSlotPointer(i).offset,
                       -stackSlotPointer(i).offset);
            qDebug("savedReg(0) = 0x%x / -0x%x", savedRegPointer(0).offset, -savedRegPointer(0).offset);
            qDebug("savedReg(1) = 0x%x / -0x%x", savedRegPointer(1).offset, -savedRegPointer(1).offset);
            qDebug("savedReg(2) = 0x%x / -0x%x", savedRegPointer(2).offset, -savedRegPointer(2).offset);
            qDebug("savedReg(3) = 0x%x / -0x%x", savedRegPointer(3).offset, -savedRegPointer(3).offset);
            qDebug("savedReg(4) = 0x%x / -0x%x", savedRegPointer(4).offset, -savedRegPointer(4).offset);
            qDebug("savedReg(5) = 0x%x / -0x%x", savedRegPointer(5).offset, -savedRegPointer(5).offset);

            qDebug("callDataAddress(0) = 0x%x", callDataAddress(0).offset);
#endif
        }

        int calculateStackFrameSize() const
        {
            // sp was aligned before executing the call instruction. So, calculate all contents
            // that were saved after that aligned stack...:
            const int stackSpaceAllocatedOtherwise = StackSpaceAllocatedUponFunctionEntry
                                                     + RegisterSize; // saved FramePointerRegister

            // ... then calculate the stuff we want to store ...:
            int frameSize = RegisterSize * normalRegistersToSave + sizeof(double) * fpRegistersToSave;
            frameSize += savedRegCount * sizeof(QV4::Value); // (these get written out as Values, not as native registers)

            Q_ASSERT(frameSize + stackSpaceAllocatedOtherwise < INT_MAX);
            // .. then align that chunk ..:
            frameSize = static_cast<int>(WTF::roundUpToMultipleOf(StackAlignment, frameSize + stackSpaceAllocatedOtherwise));
            // ... which now holds our frame size + the extra stuff that was pushed due to the call.
            // So subtract that extra stuff, and we have our frame size:
            frameSize -= stackSpaceAllocatedOtherwise;

            return frameSize;
        }

        /// \return the stack frame size in number of Value items.
        int calculateJSStackFrameSize() const
        {
            return (localCount + sizeof(QV4::CallData)/sizeof(QV4::Value) - 1 + maxOutgoingArgumentCount) + 1;
        }

        Address stackSlotPointer(int idx) const
        {
            Q_ASSERT(idx >= 0);
            Q_ASSERT(idx < localCount);

            Pointer addr = callDataAddress(0);
            addr.offset -= sizeof(QV4::Value) * (idx + 1);
            return addr;
        }

        // Some run-time functions take (Value* args, int argc). This function is for populating
        // the args.
        Pointer argumentAddressForCall(int argument) const
        {
            Q_ASSERT(argument >= 0);
            Q_ASSERT(argument < maxOutgoingArgumentCount);

            const int index = maxOutgoingArgumentCount - argument;
            return Pointer(Assembler::LocalsRegister, sizeof(QV4::Value) * (-index));
        }

        Pointer callDataAddress(int offset = 0) const {
            return Pointer(Assembler::LocalsRegister, offset - (sizeof(QV4::CallData) + sizeof(QV4::Value) * (maxOutgoingArgumentCount - 1)));
        }

        Address savedRegPointer(int offset) const
        {
            Q_ASSERT(offset >= 0);
            Q_ASSERT(offset < savedRegCount);

            // Get the address of the bottom-most element of our frame:
            Address ptr(Assembler::FramePointerRegister, -calculateStackFrameSize());
            // This now is the element with offset 0. So:
            ptr.offset += offset * sizeof(QV4::Value);
            // and we're done!
            return ptr;
        }

    private:
        int normalRegistersToSave;
        int fpRegistersToSave;

        /// arg count for calls to JS functions
        int maxOutgoingArgumentCount;

        /// the number of spill slots needed by this function
        int localCount;

        /// used by built-ins to save arguments (e.g. constants) to the stack when they need to be
        /// passed by reference.
        int savedRegCount;
    };

    struct VoidType { VoidType() {} };
    static const VoidType Void;

    typedef JSC::FunctionPtr FunctionPtr;

#ifndef QT_NO_DEBUG
    struct CallInfo {
        Label label;
        const char* functionName;
    };
#endif
    struct PointerToValue {
        PointerToValue(IR::Expr *value)
            : value(value)
        {}
        IR::Expr *value;
    };
    struct StringToIndex {
        explicit StringToIndex(const QString &string) : string(string) {}
        QString string;
    };
    struct Reference {
        Reference(IR::Expr *value) : value(value) {
            Q_ASSERT(value->asTemp() || value->asArgLocal());
        }
        IR::Expr *value;
    };

    void callAbsolute(const char* /*functionName*/, const LookupCall &lookupCall)
    {
        call(lookupCall.addr);
    }

    void callAbsolute(const char *functionName, const RuntimeCall &runtimeCall)
    {
        call(runtimeCall.addr);
#ifndef QT_NO_DEBUG
        // the code below is to get proper function names in the disassembly
        CallInfo info;
        info.functionName = functionName;
        info.label = label();
        _callInfos.append(info);
#else
        Q_UNUSED(functionName)
#endif
    }

    void registerBlock(IR::BasicBlock*, IR::BasicBlock *nextBlock);
    IR::BasicBlock *nextBlock() const { return _nextBlock; }
    void jumpToBlock(IR::BasicBlock* current, IR::BasicBlock *target);
    void addPatch(IR::BasicBlock* targetBlock, Jump targetJump);
    void addPatch(DataLabelPtr patch, Label target);
    void addPatch(DataLabelPtr patch, IR::BasicBlock *target);
    void generateCJumpOnNonZero(RegisterID reg, IR::BasicBlock *currentBlock,
                             IR::BasicBlock *trueBlock, IR::BasicBlock *falseBlock);
#ifdef QV4_USE_64_BIT_VALUE_ENCODING
    void generateCJumpOnCompare(RelationalCondition cond, RegisterID left, TrustedImm64 right,
                                IR::BasicBlock *currentBlock, IR::BasicBlock *trueBlock,
                                IR::BasicBlock *falseBlock);
#endif
    void generateCJumpOnCompare(RelationalCondition cond, RegisterID left, TrustedImm32 right,
                                IR::BasicBlock *currentBlock, IR::BasicBlock *trueBlock,
                                IR::BasicBlock *falseBlock);
    void generateCJumpOnCompare(RelationalCondition cond, RegisterID left, RegisterID right,
                                IR::BasicBlock *currentBlock, IR::BasicBlock *trueBlock,
                                IR::BasicBlock *falseBlock);
    Jump genTryDoubleConversion(IR::Expr *src, Assembler::FPRegisterID dest);
    Assembler::Jump branchDouble(bool invertCondition, IR::AluOp op, IR::Expr *left, IR::Expr *right);
    Assembler::Jump branchInt32(bool invertCondition, IR::AluOp op, IR::Expr *left, IR::Expr *right);

    Pointer loadAddress(RegisterID tmp, IR::Expr *t);
    Pointer loadTempAddress(IR::Temp *t);
    Pointer loadArgLocalAddress(RegisterID baseReg, IR::ArgLocal *al);
    Pointer loadStringAddress(RegisterID reg, const QString &string);
    Address loadConstant(IR::Const *c, RegisterID baseReg);
    Address loadConstant(const Primitive &v, RegisterID baseReg);
    void loadStringRef(RegisterID reg, const QString &string);
    Pointer stackSlotPointer(IR::Temp *t) const
    {
        Q_ASSERT(t->kind == IR::Temp::StackSlot);

        return Pointer(_stackLayout->stackSlotPointer(t->index));
    }

    template <int argumentNumber>
    void saveOutRegister(PointerToValue arg)
    {
        if (!arg.value)
            return;
        if (IR::Temp *t = arg.value->asTemp()) {
            if (t->kind == IR::Temp::PhysicalRegister) {
                Pointer addr(_stackLayout->savedRegPointer(argumentNumber));
                switch (t->type) {
                case IR::BoolType:
                    storeBool((RegisterID) t->index, addr);
                    break;
                case IR::SInt32Type:
                    storeInt32((RegisterID) t->index, addr);
                    break;
                case IR::UInt32Type:
                    storeUInt32((RegisterID) t->index, addr);
                    break;
                case IR::DoubleType:
                    storeDouble((FPRegisterID) t->index, addr);
                    break;
                default:
                    Q_UNIMPLEMENTED();
                }
            }
        }
    }

    template <int, typename ArgType>
    void saveOutRegister(ArgType)
    {}

    void loadArgumentInRegister(RegisterID source, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        move(source, dest);
    }

    void loadArgumentInRegister(const Pointer& ptr, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);
        addPtr(TrustedImm32(ptr.offset), ptr.base, dest);
    }

    void loadArgumentInRegister(PointerToValue temp, RegisterID dest, int argumentNumber)
    {
        if (!temp.value) {
            move(TrustedImmPtr(0), dest);
        } else {
            Pointer addr = toAddress(dest, temp.value, argumentNumber);
            loadArgumentInRegister(addr, dest, argumentNumber);
        }
    }
    void loadArgumentInRegister(StringToIndex temp, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);
        loadStringRef(dest, temp.string);
    }

    void loadArgumentInRegister(Reference temp, RegisterID dest, int argumentNumber)
    {
        Q_ASSERT(temp.value);
        Pointer addr = loadAddress(dest, temp.value);
        loadArgumentInRegister(addr, dest, argumentNumber);
    }

#ifdef VALUE_FITS_IN_REGISTER
    void loadArgumentInRegister(IR::Temp* temp, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        if (temp) {
            Pointer addr = loadTempAddress(temp);
            load64(addr, dest);
        } else {
            QV4::Value undefined = QV4::Primitive::undefinedValue();
            move(TrustedImm64(undefined.rawValue()), dest);
        }
    }

    void loadArgumentInRegister(IR::ArgLocal* al, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        if (al) {
            Pointer addr = loadArgLocalAddress(dest, al);
            load64(addr, dest);
        } else {
            QV4::Value undefined = QV4::Primitive::undefinedValue();
            move(TrustedImm64(undefined.rawValue()), dest);
        }
    }

    void loadArgumentInRegister(IR::Const* c, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        QV4::Value v = convertToValue(c);
        move(TrustedImm64(v.rawValue()), dest);
    }

    void loadArgumentInRegister(IR::Expr* expr, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        if (!expr) {
            QV4::Value undefined = QV4::Primitive::undefinedValue();
            move(TrustedImm64(undefined.rawValue()), dest);
        } else if (IR::Temp *t = expr->asTemp()){
            loadArgumentInRegister(t, dest, argumentNumber);
        } else if (IR::ArgLocal *al = expr->asArgLocal()) {
            loadArgumentInRegister(al, dest, argumentNumber);
        } else if (IR::Const *c = expr->asConst()) {
            loadArgumentInRegister(c, dest, argumentNumber);
        } else {
            Q_ASSERT(!"unimplemented expression type in loadArgument");
        }
    }
#else
    void loadArgumentInRegister(IR::Expr*, RegisterID)
    {
        Q_ASSERT(!"unimplemented: expression in loadArgument");
    }
#endif

    void loadArgumentInRegister(TrustedImm32 imm32, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        xorPtr(dest, dest);
        if (imm32.m_value)
            move(imm32, dest);
    }

    void storeReturnValue(RegisterID dest)
    {
        move(ReturnValueRegister, dest);
    }

    void storeUInt32ReturnValue(RegisterID dest)
    {
        subPtr(TrustedImm32(sizeof(QV4::Value)), StackPointerRegister);
        Pointer tmp(StackPointerRegister, 0);
        storeReturnValue(tmp);
        toUInt32Register(tmp, dest);
        addPtr(TrustedImm32(sizeof(QV4::Value)), StackPointerRegister);
    }

    void storeReturnValue(FPRegisterID dest)
    {
#ifdef VALUE_FITS_IN_REGISTER
        move(TrustedImm64(QV4::Value::NaNEncodeMask), ScratchRegister);
        xor64(ScratchRegister, ReturnValueRegister);
        move64ToDouble(ReturnValueRegister, dest);
#elif defined(Q_PROCESSOR_ARM)
        moveIntsToDouble(JSC::ARMRegisters::r0, JSC::ARMRegisters::r1, dest, FPGpr0);
#elif defined(Q_PROCESSOR_X86)
        moveIntsToDouble(JSC::X86Registers::eax, JSC::X86Registers::edx, dest, FPGpr0);
#elif defined(Q_PROCESSOR_MIPS)
        moveIntsToDouble(JSC::MIPSRegisters::v0, JSC::MIPSRegisters::v1, dest, FPGpr0);
#else
        subPtr(TrustedImm32(sizeof(QV4::Value)), StackPointerRegister);
        Pointer tmp(StackPointerRegister, 0);
        storeReturnValue(tmp);
        loadDouble(tmp, dest);
        addPtr(TrustedImm32(sizeof(QV4::Value)), StackPointerRegister);
#endif
    }

#ifdef VALUE_FITS_IN_REGISTER
    void storeReturnValue(const Pointer &dest)
    {
        store64(ReturnValueRegister, dest);
    }
#elif defined(Q_PROCESSOR_X86)
    void storeReturnValue(const Pointer &dest)
    {
        Pointer destination = dest;
        store32(JSC::X86Registers::eax, destination);
        destination.offset += 4;
        store32(JSC::X86Registers::edx, destination);
    }
#elif defined(Q_PROCESSOR_ARM)
    void storeReturnValue(const Pointer &dest)
    {
        Pointer destination = dest;
        store32(JSC::ARMRegisters::r0, destination);
        destination.offset += 4;
        store32(JSC::ARMRegisters::r1, destination);
    }
#elif defined(Q_PROCESSOR_MIPS)
    void storeReturnValue(const Pointer &dest)
    {
        Pointer destination = dest;
        store32(JSC::MIPSRegisters::v0, destination);
        destination.offset += 4;
        store32(JSC::MIPSRegisters::v1, destination);
    }
#endif

    void storeReturnValue(IR::Expr *target)
    {
        if (!target)
            return;

        if (IR::Temp *temp = target->asTemp()) {
            if (temp->kind == IR::Temp::PhysicalRegister) {
                if (temp->type == IR::DoubleType)
                    storeReturnValue((FPRegisterID) temp->index);
                else if (temp->type == IR::UInt32Type)
                    storeUInt32ReturnValue((RegisterID) temp->index);
                else
                    storeReturnValue((RegisterID) temp->index);
                return;
            } else {
                Pointer addr = loadTempAddress(temp);
                storeReturnValue(addr);
            }
        } else if (IR::ArgLocal *al = target->asArgLocal()) {
            Pointer addr = loadArgLocalAddress(ScratchRegister, al);
            storeReturnValue(addr);
        }
    }

    void storeReturnValue(VoidType)
    {
    }

    template <int StackSlot>
    void loadArgumentOnStack(RegisterID reg, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        poke(reg, StackSlot);
    }

    template <int StackSlot>
    void loadArgumentOnStack(TrustedImm32 value, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        poke(value, StackSlot);
    }

    template <int StackSlot>
    void loadArgumentOnStack(const Pointer& ptr, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        addPtr(TrustedImm32(ptr.offset), ptr.base, ScratchRegister);
        poke(ScratchRegister, StackSlot);
    }

    template <int StackSlot>
    void loadArgumentOnStack(PointerToValue temp, int argumentNumber)
    {
        if (temp.value) {
            Pointer ptr = toAddress(ScratchRegister, temp.value, argumentNumber);
            loadArgumentOnStack<StackSlot>(ptr, argumentNumber);
        } else {
            poke(TrustedImmPtr(0), StackSlot);
        }
    }

    template <int StackSlot>
    void loadArgumentOnStack(StringToIndex temp, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);
        loadStringRef(ScratchRegister, temp.string);
        poke(ScratchRegister, StackSlot);
    }

    template <int StackSlot>
    void loadArgumentOnStack(Reference temp, int argumentNumber)
    {
        Q_ASSERT (temp.value);

        Pointer ptr = loadAddress(ScratchRegister, temp.value);
        loadArgumentOnStack<StackSlot>(ptr, argumentNumber);
    }

    void loadDouble(IR::Expr *source, FPRegisterID dest)
    {
        IR::Temp *sourceTemp = source->asTemp();
        if (sourceTemp && sourceTemp->kind == IR::Temp::PhysicalRegister) {
            moveDouble((FPRegisterID) sourceTemp->index, dest);
            return;
        }
        Pointer ptr = loadAddress(ScratchRegister, source);
        loadDouble(ptr, dest);
    }

    void storeDouble(FPRegisterID source, IR::Expr* target)
    {
        IR::Temp *targetTemp = target->asTemp();
        if (targetTemp && targetTemp->kind == IR::Temp::PhysicalRegister) {
            moveDouble(source, (FPRegisterID) targetTemp->index);
            return;
        }
#ifdef QV4_USE_64_BIT_VALUE_ENCODING
        moveDoubleTo64(source, ReturnValueRegister);
        move(TrustedImm64(QV4::Value::NaNEncodeMask), ScratchRegister);
        xor64(ScratchRegister, ReturnValueRegister);
        Pointer ptr = loadAddress(ScratchRegister, target);
        store64(ReturnValueRegister, ptr);
#else
        Pointer ptr = loadAddress(ScratchRegister, target);
        storeDouble(source, ptr);
#endif
    }
#ifdef QV4_USE_64_BIT_VALUE_ENCODING
    // We need to (de)mangle the double
    void loadDouble(Address addr, FPRegisterID dest)
    {
        load64(addr, ReturnValueRegister);
        move(TrustedImm64(QV4::Value::NaNEncodeMask), ScratchRegister);
        xor64(ScratchRegister, ReturnValueRegister);
        move64ToDouble(ReturnValueRegister, dest);
    }

    void storeDouble(FPRegisterID source, Address addr)
    {
        moveDoubleTo64(source, ReturnValueRegister);
        move(TrustedImm64(QV4::Value::NaNEncodeMask), ScratchRegister);
        xor64(ScratchRegister, ReturnValueRegister);
        store64(ReturnValueRegister, addr);
    }
#else
    using JSC::MacroAssembler::loadDouble;
    using JSC::MacroAssembler::storeDouble;
#endif

    template <typename Result, typename Source>
    void copyValue(Result result, Source source);
    template <typename Result>
    void copyValue(Result result, IR::Expr* source);

    // The scratch register is used to calculate the temp address for the source.
    void memcopyValue(Pointer target, IR::Expr *source, RegisterID scratchRegister)
    {
        Q_ASSERT(!source->asTemp() || source->asTemp()->kind != IR::Temp::PhysicalRegister);
        Q_ASSERT(target.base != scratchRegister);
        JSC::MacroAssembler::loadDouble(loadAddress(scratchRegister, source), FPGpr0);
        JSC::MacroAssembler::storeDouble(FPGpr0, target);
    }

    void storeValue(QV4::Primitive value, RegisterID destination)
    {
        Q_UNUSED(value);
        Q_UNUSED(destination);
        Q_UNREACHABLE();
    }

    void storeValue(QV4::Primitive value, Address destination)
    {
#ifdef VALUE_FITS_IN_REGISTER
        store64(TrustedImm64(value.rawValue()), destination);
#else
        store32(TrustedImm32(value.int_32()), destination);
        destination.offset += 4;
        store32(TrustedImm32(value.tag()), destination);
#endif
    }

    void storeValue(QV4::Primitive value, IR::Expr* temp);

    void enterStandardStackFrame(const RegisterInformation &regularRegistersToSave,
                                 const RegisterInformation &fpRegistersToSave);
    void leaveStandardStackFrame(const RegisterInformation &regularRegistersToSave,
                                 const RegisterInformation &fpRegistersToSave);

    void checkException() {
        load32(Address(EngineRegister, qOffsetOf(QV4::ExecutionEngine, hasException)), ScratchRegister);
        Jump exceptionThrown = branch32(NotEqual, ScratchRegister, TrustedImm32(0));
        if (catchBlock)
            addPatch(catchBlock, exceptionThrown);
        else
            exceptionPropagationJumps.append(exceptionThrown);
    }
    void jumpToExceptionHandler() {
        Jump exceptionThrown = jump();
        if (catchBlock)
            addPatch(catchBlock, exceptionThrown);
        else
            exceptionPropagationJumps.append(exceptionThrown);
    }

    template <int argumentNumber, typename T>
    void loadArgumentOnStackOrRegister(const T &value)
    {
        if (argumentNumber < RegisterArgumentCount)
            loadArgumentInRegister(value, registerForArgument(argumentNumber), argumentNumber);
        else
#if OS(WINDOWS) && CPU(X86_64)
            loadArgumentOnStack<argumentNumber>(value, argumentNumber);
#elif CPU(MIPS) // Stack space for 4 arguments needs to be allocated for MIPS platforms.
            loadArgumentOnStack<argumentNumber>(value, argumentNumber + 4);
#else // Sanity:
            loadArgumentOnStack<argumentNumber - RegisterArgumentCount>(value, argumentNumber);
#endif
    }

    template <int argumentNumber>
    void loadArgumentOnStackOrRegister(const VoidType &value)
    {
        Q_UNUSED(value);
    }

    template <bool selectFirst, int First, int Second>
    struct Select
    {
        enum { Chosen = First };
    };

    template <int First, int Second>
    struct Select<false, First, Second>
    {
        enum { Chosen = Second };
    };

    template <int ArgumentIndex, typename Parameter>
    struct SizeOnStack
    {
        enum { Size = Select<ArgumentIndex >= RegisterArgumentCount, sizeof(void*), 0>::Chosen };
    };

    template <int ArgumentIndex>
    struct SizeOnStack<ArgumentIndex, VoidType>
    {
        enum { Size = 0 };
    };

    template <typename ArgRet, typename Callable, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
    void generateFunctionCallImp(bool needsExceptionCheck, ArgRet r, const char* functionName, Callable function, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6)
    {
        int stackSpaceNeeded =   SizeOnStack<0, Arg1>::Size
                               + SizeOnStack<1, Arg2>::Size
                               + SizeOnStack<2, Arg3>::Size
                               + SizeOnStack<3, Arg4>::Size
                               + SizeOnStack<4, Arg5>::Size
                               + SizeOnStack<5, Arg6>::Size
                               + StackShadowSpace;

        if (stackSpaceNeeded) {
            Q_ASSERT(stackSpaceNeeded < (INT_MAX - StackAlignment));
            stackSpaceNeeded = static_cast<int>(WTF::roundUpToMultipleOf(StackAlignment, stackSpaceNeeded));
            subPtr(TrustedImm32(stackSpaceNeeded), StackPointerRegister);
        }

        // First save any arguments that reside in registers, because they could be overwritten
        // if that register is also used to pass arguments.
        saveOutRegister<5>(arg6);
        saveOutRegister<4>(arg5);
        saveOutRegister<3>(arg4);
        saveOutRegister<2>(arg3);
        saveOutRegister<1>(arg2);
        saveOutRegister<0>(arg1);

        loadArgumentOnStackOrRegister<5>(arg6);
        loadArgumentOnStackOrRegister<4>(arg5);
        loadArgumentOnStackOrRegister<3>(arg4);
        loadArgumentOnStackOrRegister<2>(arg3);
        loadArgumentOnStackOrRegister<1>(arg2);

        if (prepareCall(function, this))
            loadArgumentOnStackOrRegister<0>(arg1);

#ifdef RESTORE_EBX_ON_CALL
        load32(ebxAddressOnStack(), JSC::X86Registers::ebx); // restore the GOT ptr
#endif

        callAbsolute(functionName, function);

        if (stackSpaceNeeded)
            addPtr(TrustedImm32(stackSpaceNeeded), StackPointerRegister);

        if (needsExceptionCheck) {
            checkException();
        }

        storeReturnValue(r);

    }

    template <typename ArgRet, typename Callable, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
    void generateFunctionCallImp(bool needsExceptionCheck, ArgRet r, const char* functionName, Callable function, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5)
    {
        generateFunctionCallImp(needsExceptionCheck, r, functionName, function, arg1, arg2, arg3, arg4, arg5, VoidType());
    }

    template <typename ArgRet, typename Callable, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
    void generateFunctionCallImp(bool needsExceptionCheck, ArgRet r, const char* functionName, Callable function, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
    {
        generateFunctionCallImp(needsExceptionCheck, r, functionName, function, arg1, arg2, arg3, arg4, VoidType(), VoidType());
    }

    template <typename ArgRet, typename Callable, typename Arg1, typename Arg2, typename Arg3>
    void generateFunctionCallImp(bool needsExceptionCheck, ArgRet r, const char* functionName, Callable function, Arg1 arg1, Arg2 arg2, Arg3 arg3)
    {
        generateFunctionCallImp(needsExceptionCheck, r, functionName, function, arg1, arg2, arg3, VoidType(), VoidType(), VoidType());
    }

    template <typename ArgRet, typename Callable, typename Arg1, typename Arg2>
    void generateFunctionCallImp(bool needsExceptionCheck, ArgRet r, const char* functionName, Callable function, Arg1 arg1, Arg2 arg2)
    {
        generateFunctionCallImp(needsExceptionCheck, r, functionName, function, arg1, arg2, VoidType(), VoidType(), VoidType(), VoidType());
    }

    template <typename ArgRet, typename Callable, typename Arg1>
    void generateFunctionCallImp(bool needsExceptionCheck, ArgRet r, const char* functionName, Callable function, Arg1 arg1)
    {
        generateFunctionCallImp(needsExceptionCheck, r, functionName, function, arg1, VoidType(), VoidType(), VoidType(), VoidType(), VoidType());
    }

    Pointer toAddress(RegisterID tmpReg, IR::Expr *e, int offset)
    {
        if (IR::Const *c = e->asConst()) {
            Address addr = _stackLayout->savedRegPointer(offset);
            Address tagAddr = addr;
            tagAddr.offset += 4;

            QV4::Primitive v = convertToValue(c);
            store32(TrustedImm32(v.int_32()), addr);
            store32(TrustedImm32(v.tag()), tagAddr);
            return Pointer(addr);
        }

        if (IR::Temp *t = e->asTemp())
            if (t->kind == IR::Temp::PhysicalRegister)
                return Pointer(_stackLayout->savedRegPointer(offset));

        return loadAddress(tmpReg, e);
    }

    void storeBool(RegisterID reg, Pointer addr)
    {
        store32(reg, addr);
        addr.offset += 4;
        store32(TrustedImm32(QV4::Primitive::fromBoolean(0).tag()), addr);
    }

    void storeBool(RegisterID src, RegisterID dest)
    {
        move(src, dest);
    }

    void storeBool(RegisterID reg, IR::Expr *target)
    {
        if (IR::Temp *targetTemp = target->asTemp()) {
            if (targetTemp->kind == IR::Temp::PhysicalRegister) {
                move(reg, (RegisterID) targetTemp->index);
                return;
            }
        }

        Pointer addr = loadAddress(ScratchRegister, target);
        storeBool(reg, addr);
    }

    void storeBool(bool value, IR::Expr *target) {
        TrustedImm32 trustedValue(value ? 1 : 0);

        if (IR::Temp *targetTemp = target->asTemp()) {
            if (targetTemp->kind == IR::Temp::PhysicalRegister) {
                move(trustedValue, (RegisterID) targetTemp->index);
                return;
            }
        }

        move(trustedValue, ScratchRegister);
        storeBool(ScratchRegister, target);
    }

    void storeInt32(RegisterID src, RegisterID dest)
    {
        move(src, dest);
    }

    void storeInt32(RegisterID reg, Pointer addr)
    {
        store32(reg, addr);
        addr.offset += 4;
        store32(TrustedImm32(QV4::Primitive::fromInt32(0).tag()), addr);
    }

    void storeInt32(RegisterID reg, IR::Expr *target)
    {
        if (IR::Temp *targetTemp = target->asTemp()) {
            if (targetTemp->kind == IR::Temp::PhysicalRegister) {
                move(reg, (RegisterID) targetTemp->index);
            } else {
                Pointer addr = loadTempAddress(targetTemp);
                storeInt32(reg, addr);
            }
        } else if (IR::ArgLocal *al = target->asArgLocal()) {
            Pointer addr = loadArgLocalAddress(ScratchRegister, al);
            storeInt32(reg, addr);
        }
    }

    void storeUInt32(RegisterID src, RegisterID dest)
    {
        move(src, dest);
    }

    void storeUInt32(RegisterID reg, Pointer addr)
    {
        // The UInt32 representation in QV4::Value is really convoluted. See also toUInt32Register.
        Jump intRange = branch32(GreaterThanOrEqual, reg, TrustedImm32(0));
        convertUInt32ToDouble(reg, FPGpr0, ReturnValueRegister);
        storeDouble(FPGpr0, addr);
        Jump done = jump();
        intRange.link(this);
        storeInt32(reg, addr);
        done.link(this);
    }

    void storeUInt32(RegisterID reg, IR::Expr *target)
    {
        IR::Temp *targetTemp = target->asTemp();
        if (targetTemp && targetTemp->kind == IR::Temp::PhysicalRegister) {
            move(reg, (RegisterID) targetTemp->index);
        } else {
            Pointer addr = loadAddress(ScratchRegister, target);
            storeUInt32(reg, addr);
        }
    }

    FPRegisterID toDoubleRegister(IR::Expr *e, FPRegisterID target = FPGpr0)
    {
        if (IR::Const *c = e->asConst()) {
#ifdef QV4_USE_64_BIT_VALUE_ENCODING
            Q_STATIC_ASSERT(sizeof(int64_t) == sizeof(double));
            int64_t i;
            memcpy(&i, &c->value, sizeof(double));
            move(TrustedImm64(i), ReturnValueRegister);
            move64ToDouble(ReturnValueRegister, target);
#else
            JSC::MacroAssembler::loadDouble(loadConstant(c, ScratchRegister), target);
#endif
            return target;
        }

        if (IR::Temp *t = e->asTemp())
            if (t->kind == IR::Temp::PhysicalRegister)
                return (FPRegisterID) t->index;

        loadDouble(e, target);
        return target;
    }

    RegisterID toBoolRegister(IR::Expr *e, RegisterID scratchReg)
    {
        return toInt32Register(e, scratchReg);
    }

    RegisterID toInt32Register(IR::Expr *e, RegisterID scratchReg)
    {
        if (IR::Const *c = e->asConst()) {
            move(TrustedImm32(convertToValue(c).int_32()), scratchReg);
            return scratchReg;
        }

        if (IR::Temp *t = e->asTemp())
            if (t->kind == IR::Temp::PhysicalRegister)
                return (RegisterID) t->index;

        return toInt32Register(loadAddress(scratchReg, e), scratchReg);
    }

    RegisterID toInt32Register(Pointer addr, RegisterID scratchReg)
    {
        load32(addr, scratchReg);
        return scratchReg;
    }

    RegisterID toUInt32Register(IR::Expr *e, RegisterID scratchReg)
    {
        if (IR::Const *c = e->asConst()) {
            move(TrustedImm32(unsigned(c->value)), scratchReg);
            return scratchReg;
        }

        if (IR::Temp *t = e->asTemp())
            if (t->kind == IR::Temp::PhysicalRegister)
                return (RegisterID) t->index;

        return toUInt32Register(loadAddress(scratchReg, e), scratchReg);
    }

    RegisterID toUInt32Register(Pointer addr, RegisterID scratchReg)
    {
        Q_ASSERT(addr.base != scratchReg);

        // The UInt32 representation in QV4::Value is really convoluted. See also storeUInt32.
        Pointer tagAddr = addr;
        tagAddr.offset += 4;
        load32(tagAddr, scratchReg);
        Jump inIntRange = branch32(Equal, scratchReg, TrustedImm32(QV4::Value::Integer_Type_Internal));

        // it's not in signed int range, so load it as a double, and truncate it down
        loadDouble(addr, FPGpr0);
        Address inversionAddress = loadConstant(QV4::Primitive::fromDouble(double(INT_MAX) + 1), scratchReg);
        subDouble(inversionAddress, FPGpr0);
        Jump canNeverHappen = branchTruncateDoubleToUint32(FPGpr0, scratchReg);
        canNeverHappen.link(this);
        or32(TrustedImm32(1 << 31), scratchReg);
        Jump done = jump();

        inIntRange.link(this);
        load32(addr, scratchReg);

        done.link(this);
        return scratchReg;
    }

    JSC::MacroAssemblerCodeRef link(int *codeSize);

    void setStackLayout(int maxArgCountForBuiltins, int regularRegistersToSave, int fpRegistersToSave);
    const StackLayout &stackLayout() const { return *_stackLayout.data(); }

    Label exceptionReturnLabel;
    IR::BasicBlock * catchBlock;
    QVector<Jump> exceptionPropagationJumps;
private:
    QScopedPointer<const StackLayout> _stackLayout;
    IR::Function *_function;
    std::vector<Label> _addrs;
    std::vector<std::vector<Jump>> _patches;
#ifndef QT_NO_DEBUG
    QVector<CallInfo> _callInfos;
#endif

    struct DataLabelPatch {
        DataLabelPtr dataLabel;
        Label target;
    };
    std::vector<DataLabelPatch> _dataLabelPatches;

    std::vector<std::vector<DataLabelPtr>> _labelPatches;
    IR::BasicBlock *_nextBlock;

    QV4::ExecutableAllocator *_executableAllocator;
    InstructionSelection *_isel;
};

template <typename Result, typename Source>
void Assembler::copyValue(Result result, Source source)
{
#ifdef VALUE_FITS_IN_REGISTER
    // Use ReturnValueRegister as "scratch" register because loadArgument
    // and storeArgument are functions that may need a scratch register themselves.
    loadArgumentInRegister(source, ReturnValueRegister, 0);
    storeReturnValue(result);
#else
    loadDouble(source, FPGpr0);
    storeDouble(FPGpr0, result);
#endif
}

template <typename Result>
void Assembler::copyValue(Result result, IR::Expr* source)
{
    if (source->type == IR::BoolType) {
        RegisterID reg = toInt32Register(source, ScratchRegister);
        storeBool(reg, result);
    } else if (source->type == IR::SInt32Type) {
        RegisterID reg = toInt32Register(source, ScratchRegister);
        storeInt32(reg, result);
    } else if (source->type == IR::UInt32Type) {
        RegisterID reg = toUInt32Register(source, ScratchRegister);
        storeUInt32(reg, result);
    } else if (source->type == IR::DoubleType) {
        storeDouble(toDoubleRegister(source), result);
    } else if (source->asTemp() || source->asArgLocal()) {
#ifdef VALUE_FITS_IN_REGISTER
            // Use ReturnValueRegister as "scratch" register because loadArgument
            // and storeArgument are functions that may need a scratch register themselves.
            loadArgumentInRegister(source, ReturnValueRegister, 0);
            storeReturnValue(result);
#else
            loadDouble(source, FPGpr0);
            storeDouble(FPGpr0, result);
#endif
    } else if (IR::Const *c = source->asConst()) {
        QV4::Primitive v = convertToValue(c);
        storeValue(v, result);
    } else {
        Q_UNREACHABLE();
    }
}

inline RuntimeCall::RuntimeCall(uint offset)
    : addr(Assembler::EngineRegister, offset + qOffsetOf(QV4::ExecutionEngine, runtime))
{
}



template <typename T> inline bool prepareCall(T &, Assembler *)
{ return true; }

template <> inline bool prepareCall(LookupCall &lookupCall, Assembler *as)
{
    // IMPORTANT! See generateLookupCall in qv4isel_masm_p.h for details!

    // load the table from the context
    as->loadPtr(Assembler::Address(Assembler::EngineRegister, qOffsetOf(QV4::ExecutionEngine, current)), Assembler::ScratchRegister);
    as->loadPtr(Assembler::Address(Assembler::ScratchRegister, qOffsetOf(QV4::Heap::ExecutionContext, lookups)),
                lookupCall.addr.base);
    // pre-calculate the indirect address for the lookupCall table:
    if (lookupCall.addr.offset)
        as->addPtr(Assembler::TrustedImm32(lookupCall.addr.offset), lookupCall.addr.base);
    // store it as the first argument
    as->loadArgumentOnStackOrRegister<0>(lookupCall.addr.base);
    // set the destination addresses offset to the getterSetterOffset. The base is the lookupCall table's address
    lookupCall.addr.offset = lookupCall.getterSetterOffset;
    return false;
}

} // end of namespace JIT
} // end of namespace QV4

QT_END_NAMESPACE

#endif // ENABLE(ASSEMBLER)

#endif // QV4ISEL_MASM_P_H
