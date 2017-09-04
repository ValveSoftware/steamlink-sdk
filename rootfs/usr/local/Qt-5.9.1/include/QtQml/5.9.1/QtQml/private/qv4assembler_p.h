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
#include "private/qv4context_p.h"
#include "private/qv4engine_p.h"
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

struct CompilationUnit : public QV4::CompiledData::CompilationUnit
{
    virtual ~CompilationUnit();

#if !defined(V4_BOOTSTRAP)
    void linkBackendToEngine(QV4::ExecutionEngine *engine) Q_DECL_OVERRIDE;
    bool memoryMapCode(QString *errorString) Q_DECL_OVERRIDE;
#endif
    void prepareCodeOffsetsForDiskStorage(CompiledData::Unit *unit) Q_DECL_OVERRIDE;
    bool saveCodeToDisk(QIODevice *device, const CompiledData::Unit *unit, QString *errorString) Q_DECL_OVERRIDE;

    // Coderef + execution engine

    QVector<JSC::MacroAssemblerCodeRef> codeRefs;
};

template <typename PlatformAssembler, TargetOperatingSystemSpecialization Specialization>
struct AssemblerTargetConfiguration
{
    typedef JSC::MacroAssembler<PlatformAssembler> MacroAssembler;
    typedef TargetPlatform<PlatformAssembler, Specialization> Platform;
    // More things coming here in the future, such as Target OS
};

#if CPU(ARM_THUMB2)
typedef JSC::MacroAssemblerARMv7 DefaultPlatformMacroAssembler;
typedef AssemblerTargetConfiguration<DefaultPlatformMacroAssembler, NoOperatingSystemSpecialization> DefaultAssemblerTargetConfiguration;
#elif CPU(ARM64)
typedef JSC::MacroAssemblerARM64 DefaultPlatformMacroAssembler;
typedef AssemblerTargetConfiguration<DefaultPlatformMacroAssembler, NoOperatingSystemSpecialization> DefaultAssemblerTargetConfiguration;
#elif CPU(ARM_TRADITIONAL)
typedef JSC::MacroAssemblerARM DefaultPlatformMacroAssembler;
typedef AssemblerTargetConfiguration<DefaultPlatformMacroAssembler, NoOperatingSystemSpecialization> DefaultAssemblerTargetConfiguration;
#elif CPU(MIPS)
typedef JSC::MacroAssemblerMIPS DefaultPlatformMacroAssembler;
typedef AssemblerTargetConfiguration<DefaultPlatformMacroAssembler, NoOperatingSystemSpecialization> DefaultAssemblerTargetConfiguration;
#elif CPU(X86)
typedef JSC::MacroAssemblerX86 DefaultPlatformMacroAssembler;
typedef AssemblerTargetConfiguration<DefaultPlatformMacroAssembler, NoOperatingSystemSpecialization> DefaultAssemblerTargetConfiguration;
#elif CPU(X86_64)
typedef JSC::MacroAssemblerX86_64 DefaultPlatformMacroAssembler;

#if OS(WINDOWS)
typedef AssemblerTargetConfiguration<DefaultPlatformMacroAssembler, WindowsSpecialization> DefaultAssemblerTargetConfiguration;
#else
typedef AssemblerTargetConfiguration<DefaultPlatformMacroAssembler, NoOperatingSystemSpecialization> DefaultAssemblerTargetConfiguration;
#endif

#elif CPU(SH4)
typedef JSC::MacroAssemblerSH4 DefaultPlatformMacroAssembler;
typedef AssemblerTargetConfiguration<DefaultPlatformMacroAssembler, NoOperatingSystemSpecialization> DefaultAssemblerTargetConfiguration;
#endif

#define isel_stringIfyx(s) #s
#define isel_stringIfy(s) isel_stringIfyx(s)

#define generateRuntimeCall(as, t, function, ...) \
    as->generateFunctionCallImp(Runtime::Method_##function##_NeedsExceptionCheck, t, "Runtime::" isel_stringIfy(function), typename JITAssembler::RuntimeCall(QV4::Runtime::function), __VA_ARGS__)


template <typename JITAssembler, typename MacroAssembler, typename TargetPlatform, int RegisterSize>
struct RegisterSizeDependentAssembler
{
};

template <typename JITAssembler, typename MacroAssembler, typename TargetPlatform>
struct RegisterSizeDependentAssembler<JITAssembler, MacroAssembler, TargetPlatform, 4>
{
    using RegisterID = typename JITAssembler::RegisterID;
    using FPRegisterID = typename JITAssembler::FPRegisterID;
    using RelationalCondition = typename JITAssembler::RelationalCondition;
    using ResultCondition = typename JITAssembler::ResultCondition;
    using Address = typename JITAssembler::Address;
    using Pointer = typename JITAssembler::Pointer;
    using TrustedImm32 = typename JITAssembler::TrustedImm32;
    using TrustedImm64 = typename JITAssembler::TrustedImm64;
    using Jump = typename JITAssembler::Jump;
    using Label = typename JITAssembler::Label;

    using ValueTypeInternal = Value::ValueTypeInternal_32;
    using TargetPrimitive = TargetPrimitive32;

    static void loadDouble(JITAssembler *as, Address addr, FPRegisterID dest)
    {
        as->MacroAssembler::loadDouble(addr, dest);
    }

    static void storeDouble(JITAssembler *as, FPRegisterID source, Address addr)
    {
        as->MacroAssembler::storeDouble(source, addr);
    }

    static void storeDouble(JITAssembler *as, FPRegisterID source, IR::Expr* target)
    {
        Pointer ptr = as->loadAddress(TargetPlatform::ScratchRegister, target);
        as->storeDouble(source, ptr);
    }

    static void storeValue(JITAssembler *as, TargetPrimitive value, Address destination)
    {
        as->store32(TrustedImm32(value.value()), destination);
        destination.offset += 4;
        as->store32(TrustedImm32(value.tag()), destination);
    }

    template <typename Source, typename Destination>
    static void copyValueViaRegisters(JITAssembler *as, Source source, Destination destination)
    {
        as->loadDouble(source, TargetPlatform::FPGpr0);
        as->storeDouble(TargetPlatform::FPGpr0, destination);
    }

    static void loadDoubleConstant(JITAssembler *as, IR::Const *c, FPRegisterID target)
    {
        as->MacroAssembler::loadDouble(as->loadConstant(c, TargetPlatform::ScratchRegister), target);
    }

    static void storeReturnValue(JITAssembler *as, FPRegisterID dest)
    {
        as->moveIntsToDouble(TargetPlatform::LowReturnValueRegister, TargetPlatform::HighReturnValueRegister, dest, TargetPlatform::FPGpr0);
    }

    static void storeReturnValue(JITAssembler *as, const Pointer &dest)
    {
        Address destination = dest;
        as->store32(TargetPlatform::LowReturnValueRegister, destination);
        destination.offset += 4;
        as->store32(TargetPlatform::HighReturnValueRegister, destination);
    }

    static void setFunctionReturnValueFromTemp(JITAssembler *as, IR::Temp *t)
    {
        const auto lowReg = TargetPlatform::LowReturnValueRegister;
        const auto highReg = TargetPlatform::HighReturnValueRegister;

        if (t->kind == IR::Temp::PhysicalRegister) {
            switch (t->type) {
            case IR::DoubleType:
                as->moveDoubleToInts((FPRegisterID) t->index, lowReg, highReg);
                break;
            case IR::UInt32Type: {
                RegisterID srcReg = (RegisterID) t->index;
                Jump intRange = as->branch32(JITAssembler::GreaterThanOrEqual, srcReg, TrustedImm32(0));
                as->convertUInt32ToDouble(srcReg, TargetPlatform::FPGpr0, TargetPlatform::ReturnValueRegister);
                as->moveDoubleToInts(TargetPlatform::FPGpr0, lowReg, highReg);
                Jump done = as->jump();
                intRange.link(as);
                as->move(srcReg, lowReg);
                as->move(TrustedImm32(quint32(QV4::Value::ValueTypeInternal_32::Integer)), highReg);
                done.link(as);
            } break;
            case IR::SInt32Type:
                as->move((RegisterID) t->index, lowReg);
                as->move(TrustedImm32(quint32(QV4::Value::ValueTypeInternal_32::Integer)), highReg);
                break;
            case IR::BoolType:
                as->move((RegisterID) t->index, lowReg);
                as->move(TrustedImm32(quint32(QV4::Value::ValueTypeInternal_32::Boolean)), highReg);
                break;
            default:
                Q_UNREACHABLE();
            }
        } else {
            Pointer addr = as->loadAddress(TargetPlatform::ScratchRegister, t);
            as->load32(addr, lowReg);
            addr.offset += 4;
            as->load32(addr, highReg);
        }
    }

    static void setFunctionReturnValueFromConst(JITAssembler *as, TargetPrimitive retVal)
    {
        as->move(TrustedImm32(retVal.value()), TargetPlatform::LowReturnValueRegister);
        as->move(TrustedImm32(retVal.tag()), TargetPlatform::HighReturnValueRegister);
    }

    static void loadArgumentInRegister(JITAssembler *as, IR::Temp* temp, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(as);
        Q_UNUSED(temp);
        Q_UNUSED(dest);
        Q_UNUSED(argumentNumber);
    }

    static void loadArgumentInRegister(JITAssembler *as, IR::ArgLocal* al, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(as);
        Q_UNUSED(al);
        Q_UNUSED(dest);
        Q_UNUSED(argumentNumber);
    }

    static void loadArgumentInRegister(JITAssembler *as, IR::Const* c, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(as);
        Q_UNUSED(c);
        Q_UNUSED(dest);
        Q_UNUSED(argumentNumber);
    }

    static void loadArgumentInRegister(JITAssembler *as, IR::Expr* expr, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(as);
        Q_UNUSED(expr);
        Q_UNUSED(dest);
        Q_UNUSED(argumentNumber);
    }

    static void zeroRegister(JITAssembler *as, RegisterID reg)
    {
        as->move(TrustedImm32(0), reg);
    }

    static void zeroStackSlot(JITAssembler *as, int slot)
    {
        as->poke(TrustedImm32(0), slot);
    }

    static void generateCJumpOnUndefined(JITAssembler *as,
                                  RelationalCondition cond, IR::Expr *right,
                                  RegisterID scratchRegister, RegisterID tagRegister,
                                  IR::BasicBlock *nextBlock, IR::BasicBlock *currentBlock,
                                  IR::BasicBlock *trueBlock, IR::BasicBlock *falseBlock)
    {
        Pointer tagAddr = as->loadAddress(scratchRegister, right);
        as->load32(tagAddr, tagRegister);
        Jump j = as->branch32(JITAssembler::invert(cond), tagRegister, TrustedImm32(0));
        as->addPatch(falseBlock, j);

        tagAddr.offset += 4;
        as->load32(tagAddr, tagRegister);
        const TrustedImm32 tag(QV4::Value::Managed_Type_Internal);
        Q_ASSERT(nextBlock == as->nextBlock());
        Q_UNUSED(nextBlock);
        as->generateCJumpOnCompare(cond, tagRegister, tag, currentBlock, trueBlock, falseBlock);
    }

    static void convertVarToSInt32(JITAssembler *as, IR::Expr *source, IR::Expr *target)
    {
        Q_ASSERT(source->type == IR::VarType);
        // load the tag:
        Pointer addr = as->loadAddress(TargetPlatform::ScratchRegister, source);
        Pointer tagAddr = addr;
        tagAddr.offset += 4;
        as->load32(tagAddr, TargetPlatform::ReturnValueRegister);

        // check if it's an int32:
        Jump fallback = as->branch32(RelationalCondition::NotEqual, TargetPlatform::ReturnValueRegister,
                                     TrustedImm32(quint32(Value::ValueTypeInternal_32::Integer)));
        IR::Temp *targetTemp = target->asTemp();
        if (!targetTemp || targetTemp->kind == IR::Temp::StackSlot) {
            as->load32(addr, TargetPlatform::ReturnValueRegister);
            Pointer targetAddr = as->loadAddress(TargetPlatform::ScratchRegister, target);
            as->store32(TargetPlatform::ReturnValueRegister, targetAddr);
            targetAddr.offset += 4;
            as->store32(TrustedImm32(quint32(Value::ValueTypeInternal_32::Integer)), targetAddr);
        } else {
            as->load32(addr, (RegisterID) targetTemp->index);
        }
        Jump intDone = as->jump();

        // not an int:
        fallback.link(as);
        generateRuntimeCall(as, TargetPlatform::ReturnValueRegister, toInt,
                            as->loadAddress(TargetPlatform::ScratchRegister, source));
        as->storeInt32(TargetPlatform::ReturnValueRegister, target);

        intDone.link(as);
    }

    static void loadManagedPointer(JITAssembler *as, RegisterID registerWithPtr, Pointer destAddr)
    {
        as->store32(registerWithPtr, destAddr);
        destAddr.offset += 4;
        as->store32(TrustedImm32(QV4::Value::Managed_Type_Internal_32), destAddr);
    }

    static Jump generateIsDoubleCheck(JITAssembler *as, RegisterID tagOrValueRegister)
    {
        as->and32(TrustedImm32(Value::NotDouble_Mask), tagOrValueRegister);
        return as->branch32(RelationalCondition::NotEqual, tagOrValueRegister,
                            TrustedImm32(Value::NotDouble_Mask));
    }

    static void initializeLocalVariables(JITAssembler *as, int localsCount)
    {
        as->move(TrustedImm32(0), TargetPlatform::ReturnValueRegister);
        as->move(TrustedImm32(localsCount), TargetPlatform::ScratchRegister);
        Label loop = as->label();
        as->store32(TargetPlatform::ReturnValueRegister, Address(TargetPlatform::LocalsRegister));
        as->add32(TrustedImm32(4), TargetPlatform::LocalsRegister);
        as->store32(TargetPlatform::ReturnValueRegister, Address(TargetPlatform::LocalsRegister));
        as->add32(TrustedImm32(4), TargetPlatform::LocalsRegister);
        Jump jump = as->branchSub32(ResultCondition::NonZero, TrustedImm32(1), TargetPlatform::ScratchRegister);
        jump.linkTo(loop, as);
    }

    static Jump checkIfTagRegisterIsDouble(JITAssembler *as, RegisterID tagRegister)
    {
        as->and32(TrustedImm32(Value::NotDouble_Mask), tagRegister);
        Jump isNoDbl = as->branch32(RelationalCondition::Equal, tagRegister, TrustedImm32(Value::NotDouble_Mask));
        return isNoDbl;
    }
};

template <typename JITAssembler, typename MacroAssembler, typename TargetPlatform>
struct RegisterSizeDependentAssembler<JITAssembler, MacroAssembler, TargetPlatform, 8>
{
    using RegisterID = typename JITAssembler::RegisterID;
    using FPRegisterID = typename JITAssembler::FPRegisterID;
    using Address = typename JITAssembler::Address;
    using TrustedImm32 = typename JITAssembler::TrustedImm32;
    using TrustedImm64 = typename JITAssembler::TrustedImm64;
    using Pointer = typename JITAssembler::Pointer;
    using RelationalCondition = typename JITAssembler::RelationalCondition;
    using ResultCondition = typename JITAssembler::ResultCondition;
    using BranchTruncateType = typename JITAssembler::BranchTruncateType;
    using Jump = typename JITAssembler::Jump;
    using Label = typename JITAssembler::Label;

    using ValueTypeInternal = Value::ValueTypeInternal_64;
    using TargetPrimitive = TargetPrimitive64;

    static void loadDouble(JITAssembler *as, Address addr, FPRegisterID dest)
    {
        as->load64(addr, TargetPlatform::ReturnValueRegister);
        as->xor64(TargetPlatform::DoubleMaskRegister, TargetPlatform::ReturnValueRegister);
        as->move64ToDouble(TargetPlatform::ReturnValueRegister, dest);
    }

    static void storeDouble(JITAssembler *as, FPRegisterID source, Address addr)
    {
        as->moveDoubleTo64(source, TargetPlatform::ReturnValueRegister);
        as->xor64(TargetPlatform::DoubleMaskRegister, TargetPlatform::ReturnValueRegister);
        as->store64(TargetPlatform::ReturnValueRegister, addr);
    }

    static void storeDouble(JITAssembler *as, FPRegisterID source, IR::Expr* target)
    {
        as->moveDoubleTo64(source, TargetPlatform::ReturnValueRegister);
        as->xor64(TargetPlatform::DoubleMaskRegister, TargetPlatform::ReturnValueRegister);
        Pointer ptr = as->loadAddress(TargetPlatform::ScratchRegister, target);
        as->store64(TargetPlatform::ReturnValueRegister, ptr);
    }

    static void storeReturnValue(JITAssembler *as, FPRegisterID dest)
    {
        as->xor64(TargetPlatform::DoubleMaskRegister, TargetPlatform::ReturnValueRegister);
        as->move64ToDouble(TargetPlatform::ReturnValueRegister, dest);
    }

    static void storeReturnValue(JITAssembler *as, const Pointer &dest)
    {
        as->store64(TargetPlatform::ReturnValueRegister, dest);
    }

    static void setFunctionReturnValueFromTemp(JITAssembler *as, IR::Temp *t)
    {
        if (t->kind == IR::Temp::PhysicalRegister) {
            if (t->type == IR::DoubleType) {
                as->moveDoubleTo64((FPRegisterID) t->index,
                                    TargetPlatform::ReturnValueRegister);
                as->xor64(TargetPlatform::DoubleMaskRegister, TargetPlatform::ReturnValueRegister);
            } else if (t->type == IR::UInt32Type) {
                RegisterID srcReg = (RegisterID) t->index;
                Jump intRange = as->branch32(RelationalCondition::GreaterThanOrEqual, srcReg, TrustedImm32(0));
                as->convertUInt32ToDouble(srcReg, TargetPlatform::FPGpr0, TargetPlatform::ReturnValueRegister);
                as->moveDoubleTo64(TargetPlatform::FPGpr0, TargetPlatform::ReturnValueRegister);
                as->xor64(TargetPlatform::DoubleMaskRegister, TargetPlatform::ReturnValueRegister);
                Jump done = as->jump();
                intRange.link(as);
                as->zeroExtend32ToPtr(srcReg, TargetPlatform::ReturnValueRegister);
                quint64 tag = quint64(QV4::Value::ValueTypeInternal_64::Integer);
                as->or64(TrustedImm64(tag << 32),
                         TargetPlatform::ReturnValueRegister);
                done.link(as);
            } else {
                as->zeroExtend32ToPtr((RegisterID) t->index, TargetPlatform::ReturnValueRegister);
                quint64 tag;
                switch (t->type) {
                case IR::SInt32Type:
                    tag = quint64(QV4::Value::ValueTypeInternal_64::Integer);
                    break;
                case IR::BoolType:
                    tag = quint64(QV4::Value::ValueTypeInternal_64::Boolean);
                    break;
                default:
                    tag = 31337; // bogus value
                    Q_UNREACHABLE();
                }
                as->or64(TrustedImm64(tag << 32),
                         TargetPlatform::ReturnValueRegister);
            }
        } else {
            as->copyValue(TargetPlatform::ReturnValueRegister, t);
        }
    }

    static void setFunctionReturnValueFromConst(JITAssembler *as, TargetPrimitive retVal)
    {
        as->move(TrustedImm64(retVal.rawValue()), TargetPlatform::ReturnValueRegister);
    }

    static void storeValue(JITAssembler *as, TargetPrimitive value, Address destination)
    {
        as->store64(TrustedImm64(value.rawValue()), destination);
    }

    template <typename Source, typename Destination>
    static void copyValueViaRegisters(JITAssembler *as, Source source, Destination destination)
    {
        // Use ReturnValueRegister as "scratch" register because loadArgument
        // and storeArgument are functions that may need a scratch register themselves.
        loadArgumentInRegister(as, source, TargetPlatform::ReturnValueRegister, 0);
        as->storeReturnValue(destination);
    }

    static void loadDoubleConstant(JITAssembler *as, IR::Const *c, FPRegisterID target)
    {
        Q_STATIC_ASSERT(sizeof(int64_t) == sizeof(double));
        int64_t i;
        memcpy(&i, &c->value, sizeof(double));
        as->move(TrustedImm64(i), TargetPlatform::ReturnValueRegister);
        as->move64ToDouble(TargetPlatform::ReturnValueRegister, target);
    }

    static void loadArgumentInRegister(JITAssembler *as, Address addressOfValue, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);
        as->load64(addressOfValue, dest);
    }

    static void loadArgumentInRegister(JITAssembler *as, IR::Temp* temp, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        if (temp) {
            Pointer addr = as->loadTempAddress(temp);
            as->load64(addr, dest);
        } else {
            auto undefined = TargetPrimitive::undefinedValue();
            as->move(TrustedImm64(undefined.rawValue()), dest);
        }
    }

    static void loadArgumentInRegister(JITAssembler *as, IR::ArgLocal* al, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        if (al) {
            Pointer addr = as->loadArgLocalAddress(dest, al);
            as->load64(addr, dest);
        } else {
            auto undefined = TargetPrimitive::undefinedValue();
            as->move(TrustedImm64(undefined.rawValue()), dest);
        }
    }

    static void loadArgumentInRegister(JITAssembler *as, IR::Const* c, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        auto v = convertToValue<TargetPrimitive64>(c);
        as->move(TrustedImm64(v.rawValue()), dest);
    }

    static void loadArgumentInRegister(JITAssembler *as, IR::Expr* expr, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        if (!expr) {
            auto undefined = TargetPrimitive::undefinedValue();
            as->move(TrustedImm64(undefined.rawValue()), dest);
        } else if (IR::Temp *t = expr->asTemp()){
            loadArgumentInRegister(as, t, dest, argumentNumber);
        } else if (IR::ArgLocal *al = expr->asArgLocal()) {
            loadArgumentInRegister(as, al, dest, argumentNumber);
        } else if (IR::Const *c = expr->asConst()) {
            loadArgumentInRegister(as, c, dest, argumentNumber);
        } else {
            Q_ASSERT(!"unimplemented expression type in loadArgument");
        }
    }

    static void zeroRegister(JITAssembler *as, RegisterID reg)
    {
        as->move(TrustedImm64(0), reg);
    }

    static void zeroStackSlot(JITAssembler *as, int slot)
    {
        as->store64(TrustedImm64(0), as->addressForPoke(slot));
    }

    static void generateCJumpOnCompare(JITAssembler *as,
                                       RelationalCondition cond,
                                       RegisterID left,
                                       TrustedImm64 right,
                                       IR::BasicBlock *nextBlock,
                                       IR::BasicBlock *currentBlock,
                                       IR::BasicBlock *trueBlock,
                                       IR::BasicBlock *falseBlock)
    {
        if (trueBlock == nextBlock) {
            Jump target = as->branch64(as->invert(cond), left, right);
            as->addPatch(falseBlock, target);
        } else {
            Jump target = as->branch64(cond, left, right);
            as->addPatch(trueBlock, target);
            as->jumpToBlock(currentBlock, falseBlock);
        }
    }

    static void generateCJumpOnUndefined(JITAssembler *as,
                                  RelationalCondition cond, IR::Expr *right,
                                  RegisterID scratchRegister, RegisterID tagRegister,
                                  IR::BasicBlock *nextBlock,  IR::BasicBlock *currentBlock,
                                  IR::BasicBlock *trueBlock, IR::BasicBlock *falseBlock)
    {
        Pointer addr = as->loadAddress(scratchRegister, right);
        as->load64(addr, tagRegister);
        const TrustedImm64 tag(0);
        generateCJumpOnCompare(as, cond, tagRegister, tag, nextBlock, currentBlock, trueBlock, falseBlock);
    }

    static void convertVarToSInt32(JITAssembler *as, IR::Expr *source, IR::Expr *target)
    {
        Q_ASSERT(source->type == IR::VarType);
        Pointer addr = as->loadAddress(TargetPlatform::ScratchRegister, source);
        as->load64(addr, TargetPlatform::ScratchRegister);
        as->move(TargetPlatform::ScratchRegister, TargetPlatform::ReturnValueRegister);

        // check if it's integer convertible
        as->urshift64(TrustedImm32(QV4::Value::IsIntegerConvertible_Shift), TargetPlatform::ScratchRegister);
        Jump isIntConvertible = as->branch32(RelationalCondition::Equal, TargetPlatform::ScratchRegister, TrustedImm32(3));

        // nope, not integer convertible, so check for a double:
        as->urshift64(TrustedImm32(
                           QV4::Value::IsDoubleTag_Shift - QV4::Value::IsIntegerConvertible_Shift),
                       TargetPlatform::ScratchRegister);
        Jump fallback = as->branch32(RelationalCondition::GreaterThan, TargetPlatform::ScratchRegister, TrustedImm32(0));

        // it's a double
        as->xor64(TargetPlatform::DoubleMaskRegister, TargetPlatform::ReturnValueRegister);
        as->move64ToDouble(TargetPlatform::ReturnValueRegister, TargetPlatform::FPGpr0);
        Jump success =
                as->branchTruncateDoubleToInt32(TargetPlatform::FPGpr0, TargetPlatform::ReturnValueRegister,
                                                 BranchTruncateType::BranchIfTruncateSuccessful);

        // not an int:
        fallback.link(as);
        generateRuntimeCall(as, TargetPlatform::ReturnValueRegister, toInt,
                            as->loadAddress(TargetPlatform::ScratchRegister, source));


        isIntConvertible.link(as);
        success.link(as);
        IR::Temp *targetTemp = target->asTemp();
        if (!targetTemp || targetTemp->kind == IR::Temp::StackSlot) {
            Pointer targetAddr = as->loadAddress(TargetPlatform::ScratchRegister, target);
            as->store32(TargetPlatform::ReturnValueRegister, targetAddr);
            targetAddr.offset += 4;
            as->store32(TrustedImm32(quint32(Value::ValueTypeInternal_64::Integer)), targetAddr);
        } else {
            as->storeInt32(TargetPlatform::ReturnValueRegister, target);
        }
    }

    static void loadManagedPointer(JITAssembler *as, RegisterID registerWithPtr, Pointer destAddr)
    {
        as->store64(registerWithPtr, destAddr);
    }

    static Jump generateIsDoubleCheck(JITAssembler *as, RegisterID tagOrValueRegister)
    {
        as->rshift32(TrustedImm32(Value::IsDoubleTag_Shift), tagOrValueRegister);
        return as->branch32(RelationalCondition::NotEqual, tagOrValueRegister,
                            TrustedImm32(0));
    }

    static void initializeLocalVariables(JITAssembler *as, int localsCount)
    {
        as->move(TrustedImm64(0), TargetPlatform::ReturnValueRegister);
        as->move(TrustedImm32(localsCount), TargetPlatform::ScratchRegister);
        Label loop = as->label();
        as->store64(TargetPlatform::ReturnValueRegister, Address(TargetPlatform::LocalsRegister));
        as->add64(TrustedImm32(8), TargetPlatform::LocalsRegister);
        Jump jump = as->branchSub32(ResultCondition::NonZero, TrustedImm32(1), TargetPlatform::ScratchRegister);
        jump.linkTo(loop, as);
    }

    static Jump checkIfTagRegisterIsDouble(JITAssembler *as, RegisterID tagRegister)
    {
        as->rshift32(TrustedImm32(Value::IsDoubleTag_Shift), tagRegister);
        Jump isNoDbl = as->branch32(RelationalCondition::Equal, tagRegister, TrustedImm32(0));
        return isNoDbl;
    }
};

template <typename TargetConfiguration>
class Assembler : public TargetConfiguration::MacroAssembler, public TargetConfiguration::Platform
{
    Q_DISABLE_COPY(Assembler)

public:
    Assembler(QV4::Compiler::JSUnitGenerator *jsGenerator, IR::Function* function, QV4::ExecutableAllocator *executableAllocator);

    using MacroAssembler = typename TargetConfiguration::MacroAssembler;
    using RegisterID = typename MacroAssembler::RegisterID;
    using FPRegisterID = typename MacroAssembler::FPRegisterID;
    using Address = typename MacroAssembler::Address;
    using Label = typename MacroAssembler::Label;
    using Jump = typename MacroAssembler::Jump;
    using DataLabelPtr = typename MacroAssembler::DataLabelPtr;
    using TrustedImm32 = typename MacroAssembler::TrustedImm32;
    using TrustedImm64 = typename MacroAssembler::TrustedImm64;
    using TrustedImmPtr = typename MacroAssembler::TrustedImmPtr;
    using RelationalCondition = typename MacroAssembler::RelationalCondition;
    using typename MacroAssembler::DoubleCondition;
    using MacroAssembler::label;
    using MacroAssembler::move;
    using MacroAssembler::jump;
    using MacroAssembler::add32;
    using MacroAssembler::and32;
    using MacroAssembler::store32;
    using MacroAssembler::loadPtr;
    using MacroAssembler::load32;
    using MacroAssembler::branch32;
    using MacroAssembler::subDouble;
    using MacroAssembler::subPtr;
    using MacroAssembler::addPtr;
    using MacroAssembler::call;
    using MacroAssembler::poke;
    using MacroAssembler::branchTruncateDoubleToUint32;
    using MacroAssembler::or32;
    using MacroAssembler::moveDouble;
    using MacroAssembler::convertUInt32ToDouble;
    using MacroAssembler::invert;
    using MacroAssembler::convertInt32ToDouble;
    using MacroAssembler::rshift32;
    using MacroAssembler::storePtr;
    using MacroAssembler::ret;

    using JITTargetPlatform = typename TargetConfiguration::Platform;
    using JITTargetPlatform::RegisterArgumentCount;
    using JITTargetPlatform::StackSpaceAllocatedUponFunctionEntry;
    using JITTargetPlatform::RegisterSize;
    using JITTargetPlatform::StackAlignment;
    using JITTargetPlatform::ReturnValueRegister;
    using JITTargetPlatform::StackPointerRegister;
    using JITTargetPlatform::ScratchRegister;
    using JITTargetPlatform::EngineRegister;
    using JITTargetPlatform::StackShadowSpace;
    using JITTargetPlatform::registerForArgument;
    using JITTargetPlatform::FPGpr0;
    using JITTargetPlatform::platformEnterStandardStackFrame;
    using JITTargetPlatform::platformFinishEnteringStandardStackFrame;
    using JITTargetPlatform::platformLeaveStandardStackFrame;

    static qint32 targetStructureOffset(qint32 hostOffset)
    {
        Q_ASSERT(hostOffset % QT_POINTER_SIZE == 0);
        return (hostOffset * RegisterSize) / QT_POINTER_SIZE;
    }

    struct LookupCall {
        Address addr;
        uint getterSetterOffset;

        LookupCall(const Address &addr, uint getterSetterOffset)
            : addr(addr)
            , getterSetterOffset(getterSetterOffset)
        {}
    };

    struct RuntimeCall {
        Address addr;

        inline RuntimeCall(Runtime::RuntimeMethods method = Runtime::InvalidRuntimeMethod);
        bool isValid() const { return addr.offset >= 0; }
    };

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

    using RegisterSizeDependentOps = RegisterSizeDependentAssembler<Assembler<TargetConfiguration>, MacroAssembler, JITTargetPlatform, RegisterSize>;
    using ValueTypeInternal = typename RegisterSizeDependentOps::ValueTypeInternal;
    using TargetPrimitive = typename RegisterSizeDependentOps::TargetPrimitive;

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
    void generateCJumpOnCompare(RelationalCondition cond, RegisterID left, TrustedImm32 right,
                                IR::BasicBlock *currentBlock, IR::BasicBlock *trueBlock,
                                IR::BasicBlock *falseBlock);
    void generateCJumpOnCompare(RelationalCondition cond, RegisterID left, RegisterID right,
                                IR::BasicBlock *currentBlock, IR::BasicBlock *trueBlock,
                                IR::BasicBlock *falseBlock);
    void generateCJumpOnUndefined(RelationalCondition cond, IR::Expr *right,
                                  RegisterID scratchRegister, RegisterID tagRegister,
                                  IR::BasicBlock *currentBlock, IR::BasicBlock *trueBlock,
                                  IR::BasicBlock *falseBlock)
    {
        RegisterSizeDependentOps::generateCJumpOnUndefined(this, cond, right, scratchRegister, tagRegister,
                                                           _nextBlock, currentBlock, trueBlock, falseBlock);
    }

    Jump generateIsDoubleCheck(RegisterID tagOrValueRegister)
    {
        return RegisterSizeDependentOps::generateIsDoubleCheck(this, tagOrValueRegister);
    }

    Jump genTryDoubleConversion(IR::Expr *src, FPRegisterID dest);
    Jump branchDouble(bool invertCondition, IR::AluOp op, IR::Expr *left, IR::Expr *right);
    Jump branchInt32(bool invertCondition, IR::AluOp op, IR::Expr *left, IR::Expr *right);

    Pointer loadAddress(RegisterID tmp, IR::Expr *t);
    Pointer loadTempAddress(IR::Temp *t);
    Pointer loadArgLocalAddress(RegisterID baseReg, IR::ArgLocal *al);
    Pointer loadStringAddress(RegisterID reg, const QString &string);
    Address loadConstant(IR::Const *c, RegisterID baseReg);
    Address loadConstant(const TargetPrimitive &v, RegisterID baseReg);
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
            RegisterSizeDependentOps::zeroRegister(this, dest);
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

    void loadArgumentInRegister(IR::Temp* temp, RegisterID dest, int argumentNumber)
    {
        RegisterSizeDependentOps::loadArgumentInRegister(this, temp, dest, argumentNumber);
    }

    void loadArgumentInRegister(IR::ArgLocal* al, RegisterID dest, int argumentNumber)
    {
        RegisterSizeDependentOps::loadArgumentInRegister(this, al, dest, argumentNumber);
    }

    void loadArgumentInRegister(IR::Const* c, RegisterID dest, int argumentNumber)
    {
        RegisterSizeDependentOps::loadArgumentInRegister(this, c, dest, argumentNumber);
    }

    void loadArgumentInRegister(IR::Expr* expr, RegisterID dest, int argumentNumber)
    {
        RegisterSizeDependentOps::loadArgumentInRegister(this, expr, dest, argumentNumber);
    }

    void loadArgumentInRegister(TrustedImm32 imm32, RegisterID dest, int argumentNumber)
    {
        Q_UNUSED(argumentNumber);

        RegisterSizeDependentOps::zeroRegister(this, dest);
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
        RegisterSizeDependentOps::storeReturnValue(this, dest);
    }

    void storeReturnValue(const Pointer &dest)
    {
        RegisterSizeDependentOps::storeReturnValue(this, dest);
    }

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
            RegisterSizeDependentOps::zeroStackSlot(this, StackSlot);
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
        RegisterSizeDependentOps::storeDouble(this, source, target);
    }

    void loadDouble(Address addr, FPRegisterID dest)
    {
        RegisterSizeDependentOps::loadDouble(this, addr, dest);
    }

    void storeDouble(FPRegisterID source, Address addr)
    {
        RegisterSizeDependentOps::storeDouble(this, source, addr);
    }

    template <typename Result, typename Source>
    void copyValue(Result result, Source source);
    template <typename Result>
    void copyValue(Result result, IR::Expr* source);

    // The scratch register is used to calculate the temp address for the source.
    void memcopyValue(Pointer target, IR::Expr *source, RegisterID scratchRegister)
    {
        Q_ASSERT(!source->asTemp() || source->asTemp()->kind != IR::Temp::PhysicalRegister);
        Q_ASSERT(target.base != scratchRegister);
        TargetConfiguration::MacroAssembler::loadDouble(loadAddress(scratchRegister, source), FPGpr0);
        TargetConfiguration::MacroAssembler::storeDouble(FPGpr0, target);
    }

    // The scratch register is used to calculate the temp address for the source.
    void memcopyValue(IR::Expr *target, Pointer source, FPRegisterID fpScratchRegister, RegisterID scratchRegister)
    {
        TargetConfiguration::MacroAssembler::loadDouble(source, fpScratchRegister);
        TargetConfiguration::MacroAssembler::storeDouble(fpScratchRegister, loadAddress(scratchRegister, target));
    }

    void storeValue(TargetPrimitive value, Address destination)
    {
        RegisterSizeDependentOps::storeValue(this, value, destination);
    }

    void storeValue(TargetPrimitive value, IR::Expr* temp);

    void enterStandardStackFrame(const RegisterInformation &regularRegistersToSave,
                                 const RegisterInformation &fpRegistersToSave);
    void leaveStandardStackFrame(const RegisterInformation &regularRegistersToSave,
                                 const RegisterInformation &fpRegistersToSave);

    void checkException() {
        load32(Address(EngineRegister, targetStructureOffset(offsetof(QV4::EngineBase, hasException))), ScratchRegister);
        Jump exceptionThrown = branch32(RelationalCondition::NotEqual, ScratchRegister, TrustedImm32(0));
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
            loadArgumentOnStack<argumentNumber - RegisterArgumentCount + (StackShadowSpace / RegisterSize)>(value, argumentNumber);
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
        enum { Size = Select<ArgumentIndex >= RegisterArgumentCount, RegisterSize, 0>::Chosen };
    };

    template <int ArgumentIndex>
    struct SizeOnStack<ArgumentIndex, VoidType>
    {
        enum { Size = 0 };
    };

    template <typename T> bool prepareCall(T &)
    { return true; }

    bool prepareCall(LookupCall &lookupCall)
    {
        // IMPORTANT! See generateLookupCall in qv4isel_masm_p.h for details!

        // load the table from the context
        loadPtr(Address(EngineRegister, targetStructureOffset(offsetof(QV4::EngineBase, current))), ScratchRegister);
        loadPtr(Address(ScratchRegister, targetStructureOffset(Heap::ExecutionContext::baseOffset + offsetof(Heap::ExecutionContextData, lookups))),
                    lookupCall.addr.base);
        // pre-calculate the indirect address for the lookupCall table:
        if (lookupCall.addr.offset)
            addPtr(TrustedImm32(lookupCall.addr.offset), lookupCall.addr.base);
        // store it as the first argument
        loadArgumentOnStackOrRegister<0>(lookupCall.addr.base);
        // set the destination addresses offset to the getterSetterOffset. The base is the lookupCall table's address
        lookupCall.addr.offset = lookupCall.getterSetterOffset;
        return false;
    }

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

        if (prepareCall(function))
            loadArgumentOnStackOrRegister<0>(arg1);

        if (JITTargetPlatform::gotRegister != -1)
            load32(Address(JITTargetPlatform::FramePointerRegister, JITTargetPlatform::savedGOTRegisterSlotOnStack()), static_cast<RegisterID>(JITTargetPlatform::gotRegister)); // restore the GOT ptr

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

            auto v = convertToValue<TargetPrimitive>(c);
            store32(TrustedImm32(v.value()), addr);
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
        store32(TrustedImm32(TargetPrimitive::fromBoolean(0).tag()), addr);
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
        store32(TrustedImm32(TargetPrimitive::fromInt32(0).tag()), addr);
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
        Jump intRange = branch32(RelationalCondition::GreaterThanOrEqual, reg, TrustedImm32(0));
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
            RegisterSizeDependentOps::loadDoubleConstant(this, c, target);
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
            move(TrustedImm32(convertToValue<Primitive>(c).int_32()), scratchReg);
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
        Jump inIntRange = branch32(RelationalCondition::Equal, scratchReg, TrustedImm32(quint32(ValueTypeInternal::Integer)));

        // it's not in signed int range, so load it as a double, and truncate it down
        loadDouble(addr, FPGpr0);
        Address inversionAddress = loadConstant(TargetPrimitive::fromDouble(double(INT_MAX) + 1), scratchReg);
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

    void returnFromFunction(IR::Ret *s, RegisterInformation regularRegistersToSave, RegisterInformation fpRegistersToSave);

    JSC::MacroAssemblerCodeRef link(int *codeSize);

    void setStackLayout(int maxArgCountForBuiltins, int regularRegistersToSave, int fpRegistersToSave);
    const StackLayout &stackLayout() const { return *_stackLayout.data(); }
    void initializeLocalVariables()
    {
        const int locals = _stackLayout->calculateJSStackFrameSize();
        if (locals <= 0)
            return;
        loadPtr(Address(JITTargetPlatform::EngineRegister, targetStructureOffset(offsetof(EngineBase, jsStackTop))), JITTargetPlatform::LocalsRegister);
        RegisterSizeDependentOps::initializeLocalVariables(this, locals);
        storePtr(JITTargetPlatform::LocalsRegister, Address(JITTargetPlatform::EngineRegister, targetStructureOffset(offsetof(EngineBase, jsStackTop))));
    }

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
    QV4::Compiler::JSUnitGenerator *_jsGenerator;
};

template <typename TargetConfiguration>
const typename Assembler<TargetConfiguration>::VoidType Assembler<TargetConfiguration>::Void;

template <typename TargetConfiguration>
template <typename Result, typename Source>
void Assembler<TargetConfiguration>::copyValue(Result result, Source source)
{
    RegisterSizeDependentOps::copyValueViaRegisters(this, source, result);
}

template <typename TargetConfiguration>
template <typename Result>
void Assembler<TargetConfiguration>::copyValue(Result result, IR::Expr* source)
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
        RegisterSizeDependentOps::copyValueViaRegisters(this, source, result);
    } else if (IR::Const *c = source->asConst()) {
        auto v = convertToValue<TargetPrimitive>(c);
        storeValue(v, result);
    } else {
        Q_UNREACHABLE();
    }
}

template <typename TargetConfiguration>
inline Assembler<TargetConfiguration>::RuntimeCall::RuntimeCall(Runtime::RuntimeMethods method)
    : addr(Assembler::EngineRegister,
           method == Runtime::InvalidRuntimeMethod ? -1 : (Assembler<TargetConfiguration>::targetStructureOffset(offsetof(EngineBase, runtime) + Runtime::runtimeMethodOffset(method))))
{
}

} // end of namespace JIT
} // end of namespace QV4

QT_END_NAMESPACE

#endif // ENABLE(ASSEMBLER)

#endif // QV4ISEL_MASM_P_H
