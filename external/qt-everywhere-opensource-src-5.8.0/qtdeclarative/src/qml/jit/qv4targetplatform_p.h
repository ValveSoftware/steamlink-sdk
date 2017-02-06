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

#ifndef QV4TARGETPLATFORM_P_H
#define QV4TARGETPLATFORM_P_H

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

#include <config.h>

#if ENABLE(ASSEMBLER)

#include "qv4registerinfo_p.h"
#include <assembler/MacroAssembler.h>

QT_BEGIN_NAMESPACE

namespace QV4 {
namespace JIT {

// The TargetPlatform class describes how the stack and the registers work on a CPU+ABI combination.
//
// All combinations have a separate definition, guarded by #ifdefs. The exceptions are:
//  - Linux/x86 and win32, which are the same, except that linux non-PIC/PIE code does not need to
//    restore ebx (which holds the GOT ptr) before a call
//  - All (supported) ARM platforms, where the only variety is the platform specific usage of r9,
//    and the frame-pointer in Thumb/Thumb2 v.s. ARM mode.
//
// Specific handling of ebx when it holds the GOT:
// In this case we can use it, but it needs to be restored when doing a call. So, the handling is as
// follows: it is marked as caller saved, meaning the value in it won't survive a call. When
// calculating the list of callee saved registers in getCalleeSavedRegisters (which is used to
// generate push/pop instructions in the prelude/postlude), we add ebx too. Then when synthesizing
// a call, we add a load it right before emitting the call instruction.
//
// NOTE: When adding new architecture, do not forget to whitelist it in qv4global_p.h!
class TargetPlatform
{
public:
#if CPU(X86) && (OS(LINUX) || OS(WINDOWS) || OS(QNX) || OS(FREEBSD) || defined(Q_OS_IOS))
    enum { RegAllocIsSupported = 1 };

    static const JSC::MacroAssembler::RegisterID FramePointerRegister   = JSC::X86Registers::ebp;
    static const JSC::MacroAssembler::RegisterID StackPointerRegister = JSC::X86Registers::esp;
    static const JSC::MacroAssembler::RegisterID LocalsRegister       = JSC::X86Registers::edi;
    static const JSC::MacroAssembler::RegisterID EngineRegister      = JSC::X86Registers::esi;
    static const JSC::MacroAssembler::RegisterID ReturnValueRegister  = JSC::X86Registers::eax;
    static const JSC::MacroAssembler::RegisterID ScratchRegister      = JSC::X86Registers::ecx;
    static const JSC::MacroAssembler::FPRegisterID FPGpr0             = JSC::X86Registers::xmm0;
    static const JSC::MacroAssembler::FPRegisterID FPGpr1             = JSC::X86Registers::xmm1;

    static RegisterInformation getPlatformRegisterInfo()
    {
        typedef RegisterInfo RI;
        return RegisterInformation()
                << RI(JSC::X86Registers::edx, QStringLiteral("edx"),   RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::X86Registers::ebx, QStringLiteral("ebx"),   RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::X86Registers::edi, QStringLiteral("edi"),   RI::RegularRegister,       RI::CalleeSaved, RI::Predefined)
                << RI(JSC::X86Registers::esi, QStringLiteral("esi"),   RI::RegularRegister,       RI::CalleeSaved, RI::Predefined)
                << RI(JSC::X86Registers::xmm2, QStringLiteral("xmm2"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::X86Registers::xmm3, QStringLiteral("xmm3"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::X86Registers::xmm4, QStringLiteral("xmm4"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::X86Registers::xmm5, QStringLiteral("xmm5"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::X86Registers::xmm6, QStringLiteral("xmm6"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::X86Registers::xmm7, QStringLiteral("xmm7"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                   ;
    }

#  define HAVE_ALU_OPS_WITH_MEM_OPERAND 1
#  undef VALUE_FITS_IN_REGISTER
    static const int RegisterSize = 4;

#  undef ARGUMENTS_IN_REGISTERS
    static const int RegisterArgumentCount = 0;
    static JSC::MacroAssembler::RegisterID registerForArgument(int) { Q_UNREACHABLE(); }

    static const int StackAlignment = 16;
    static const int StackShadowSpace = 0;
    static const int StackSpaceAllocatedUponFunctionEntry = RegisterSize; // Return address is pushed onto stack by the CPU.
    static void platformEnterStandardStackFrame(JSC::MacroAssembler *as) { as->push(FramePointerRegister); }
    static void platformLeaveStandardStackFrame(JSC::MacroAssembler *as) { as->pop(FramePointerRegister); }

#if OS(WINDOWS) || OS(QNX) || \
    ((OS(LINUX) || OS(FREEBSD)) && (defined(__PIC__) || defined(__PIE__)))

#define RESTORE_EBX_ON_CALL
    static JSC::MacroAssembler::Address ebxAddressOnStack()
    {
        static int ebxIdx = -1;
        if (ebxIdx == -1) {
            int calleeSaves = 0;
            const auto infos = getRegisterInfo();
            for (const RegisterInfo &info : infos) {
                if (info.reg<JSC::X86Registers::RegisterID>() == JSC::X86Registers::ebx) {
                    ebxIdx = calleeSaves;
                    break;
                } else if (info.isCalleeSaved()) {
                    ++calleeSaves;
                }
            }
            Q_ASSERT(ebxIdx >= 0);
            ebxIdx += 1;
        }
        return JSC::MacroAssembler::Address(FramePointerRegister, ebxIdx * -int(sizeof(void*)));
    }
#endif

#endif // Windows on x86

#if CPU(X86_64) && (OS(LINUX) || OS(MAC_OS_X) || OS(FREEBSD) || OS(QNX) || defined(Q_OS_IOS))
    enum { RegAllocIsSupported = 1 };

    static const JSC::MacroAssembler::RegisterID FramePointerRegister   = JSC::X86Registers::ebp;
    static const JSC::MacroAssembler::RegisterID StackPointerRegister = JSC::X86Registers::esp;
    static const JSC::MacroAssembler::RegisterID LocalsRegister       = JSC::X86Registers::r12;
    static const JSC::MacroAssembler::RegisterID EngineRegister      = JSC::X86Registers::r14;
    static const JSC::MacroAssembler::RegisterID ReturnValueRegister  = JSC::X86Registers::eax;
    static const JSC::MacroAssembler::RegisterID ScratchRegister      = JSC::X86Registers::r10;
    static const JSC::MacroAssembler::FPRegisterID FPGpr0             = JSC::X86Registers::xmm0;
    static const JSC::MacroAssembler::FPRegisterID FPGpr1             = JSC::X86Registers::xmm1;

    static RegisterInformation getPlatformRegisterInfo()
    {
        typedef RegisterInfo RI;
        return RegisterInformation()
                << RI(JSC::X86Registers::ebx,  QStringLiteral("rbx"),  RI::RegularRegister,       RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::X86Registers::edi,  QStringLiteral("rdi"),  RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::X86Registers::esi,  QStringLiteral("rsi"),  RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::X86Registers::edx,  QStringLiteral("rdx"),  RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::X86Registers::r9,   QStringLiteral("r9"),   RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::X86Registers::r8,   QStringLiteral("r8"),   RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::X86Registers::r12,  QStringLiteral("r12"),  RI::RegularRegister,       RI::CalleeSaved, RI::Predefined)
                << RI(JSC::X86Registers::r13,  QStringLiteral("r13"),  RI::RegularRegister,       RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::X86Registers::r14,  QStringLiteral("r14"),  RI::RegularRegister,       RI::CalleeSaved, RI::Predefined)
                << RI(JSC::X86Registers::r15,  QStringLiteral("r15"),  RI::RegularRegister,       RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::X86Registers::xmm2, QStringLiteral("xmm2"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::X86Registers::xmm3, QStringLiteral("xmm3"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::X86Registers::xmm4, QStringLiteral("xmm4"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::X86Registers::xmm5, QStringLiteral("xmm5"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::X86Registers::xmm6, QStringLiteral("xmm6"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::X86Registers::xmm7, QStringLiteral("xmm7"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                   ;
    }

#define HAVE_ALU_OPS_WITH_MEM_OPERAND 1
#define VALUE_FITS_IN_REGISTER
    static const int RegisterSize = 8;

#define ARGUMENTS_IN_REGISTERS
    static const int RegisterArgumentCount = 6;
    static JSC::MacroAssembler::RegisterID registerForArgument(int index)
    {
        static JSC::MacroAssembler::RegisterID regs[RegisterArgumentCount] = {
            JSC::X86Registers::edi,
            JSC::X86Registers::esi,
            JSC::X86Registers::edx,
            JSC::X86Registers::ecx,
            JSC::X86Registers::r8,
            JSC::X86Registers::r9
        };
        Q_ASSERT(index >= 0 && index < RegisterArgumentCount);
        return regs[index];
    };

    static const int StackAlignment = 16;
    static const int StackShadowSpace = 0;
    static const int StackSpaceAllocatedUponFunctionEntry = RegisterSize; // Return address is pushed onto stack by the CPU.
    static void platformEnterStandardStackFrame(JSC::MacroAssembler *as) { as->push(FramePointerRegister); }
    static void platformLeaveStandardStackFrame(JSC::MacroAssembler *as) { as->pop(FramePointerRegister); }
#endif // Linux/MacOS on x86_64

#if CPU(X86_64) && OS(WINDOWS)
    // Register allocation is not (yet) supported on win64, because the ABI related stack handling
    // is not completely implemented. Specifically, the saving of xmm registers, and the saving of
    // incoming function parameters to the shadow space is missing.
    enum { RegAllocIsSupported = 0 };

    static const JSC::MacroAssembler::RegisterID FramePointerRegister   = JSC::X86Registers::ebp;
    static const JSC::MacroAssembler::RegisterID StackPointerRegister = JSC::X86Registers::esp;
    static const JSC::MacroAssembler::RegisterID LocalsRegister       = JSC::X86Registers::r12;
    static const JSC::MacroAssembler::RegisterID EngineRegister      = JSC::X86Registers::r14;
    static const JSC::MacroAssembler::RegisterID ReturnValueRegister  = JSC::X86Registers::eax;
    static const JSC::MacroAssembler::RegisterID ScratchRegister      = JSC::X86Registers::r10;
    static const JSC::MacroAssembler::FPRegisterID FPGpr0             = JSC::X86Registers::xmm0;
    static const JSC::MacroAssembler::FPRegisterID FPGpr1             = JSC::X86Registers::xmm1;

    static RegisterInformation getPlatformRegisterInfo()
    {
        typedef RegisterInfo RI;
        return RegisterInformation()
                << RI(JSC::X86Registers::ebx,  QStringLiteral("rbx"),  RI::RegularRegister,       RI::CalleeSaved, RI::Predefined)
                << RI(JSC::X86Registers::edi,  QStringLiteral("rdi"),  RI::RegularRegister,       RI::CalleeSaved, RI::Predefined)
                << RI(JSC::X86Registers::esi,  QStringLiteral("rsi"),  RI::RegularRegister,       RI::CalleeSaved, RI::Predefined)
                << RI(JSC::X86Registers::edx,  QStringLiteral("rdx"),  RI::RegularRegister,       RI::CallerSaved, RI::Predefined)
                << RI(JSC::X86Registers::r9,   QStringLiteral("r9"),   RI::RegularRegister,       RI::CallerSaved, RI::Predefined)
                << RI(JSC::X86Registers::r8,   QStringLiteral("r8"),   RI::RegularRegister,       RI::CallerSaved, RI::Predefined)
                << RI(JSC::X86Registers::r12,  QStringLiteral("r12"),  RI::RegularRegister,       RI::CalleeSaved, RI::Predefined)
                << RI(JSC::X86Registers::r13,  QStringLiteral("r13"),  RI::RegularRegister,       RI::CalleeSaved, RI::Predefined)
                << RI(JSC::X86Registers::r14,  QStringLiteral("r14"),  RI::RegularRegister,       RI::CalleeSaved, RI::Predefined)
                << RI(JSC::X86Registers::r15,  QStringLiteral("r15"),  RI::RegularRegister,       RI::CalleeSaved, RI::Predefined)
                   ;
    }

#define HAVE_ALU_OPS_WITH_MEM_OPERAND 1
#define VALUE_FITS_IN_REGISTER
    static const int RegisterSize = 8;

#define ARGUMENTS_IN_REGISTERS
    static const int RegisterArgumentCount = 4;
    static JSC::MacroAssembler::RegisterID registerForArgument(int index)
    {
        static JSC::MacroAssembler::RegisterID regs[RegisterArgumentCount] = {
            JSC::X86Registers::ecx,
            JSC::X86Registers::edx,
            JSC::X86Registers::r8,
            JSC::X86Registers::r9
        };
        Q_ASSERT(index >= 0 && index < RegisterArgumentCount);
        return regs[index];
    };

    static const int StackAlignment = 16;
    static const int StackShadowSpace = 32;
    static const int StackSpaceAllocatedUponFunctionEntry = RegisterSize; // Return address is pushed onto stack by the CPU.
    static void platformEnterStandardStackFrame(JSC::MacroAssembler *as) { as->push(FramePointerRegister); }
    static void platformLeaveStandardStackFrame(JSC::MacroAssembler *as) { as->pop(FramePointerRegister); }
#endif // Windows on x86_64

#if CPU(ARM)
    enum { RegAllocIsSupported = 1 };

    // The AAPCS specifies that the platform ABI has to define the usage of r9. Known are:
    //  - The GNU/Linux ABI defines it as an additional callee-saved variable register (v6).
    //  - iOS (for which we cannot JIT, but still...) defines it as having a special use, so we do
    //    not touch it, nor use it.
    //  - Any other platform has not been verified, so we conservatively assume we cannot use it.
#if OS(LINUX)
#define CAN_USE_R9
#endif

    // There are two designated frame-pointer registers on ARM, depending on which instruction set
    // is used for the subroutine: r7 for Thumb or Thumb2, and r11 for ARM. We assign the constants
    // accordingly, and assign the locals-register to the "other" register.
#if CPU(ARM_THUMB2)
    static const JSC::MacroAssembler::RegisterID FramePointerRegister = JSC::ARMRegisters::r7;
    static const JSC::MacroAssembler::RegisterID LocalsRegister = JSC::ARMRegisters::r11;
#else // Thumbs down
    static const JSC::MacroAssembler::RegisterID FramePointerRegister = JSC::ARMRegisters::r11;
    static const JSC::MacroAssembler::RegisterID LocalsRegister = JSC::ARMRegisters::r7;
#endif
    static const JSC::MacroAssembler::RegisterID StackPointerRegister = JSC::ARMRegisters::r13;
    static const JSC::MacroAssembler::RegisterID ScratchRegister = JSC::ARMRegisters::r5;
    static const JSC::MacroAssembler::RegisterID EngineRegister = JSC::ARMRegisters::r10;
    static const JSC::MacroAssembler::RegisterID ReturnValueRegister = JSC::ARMRegisters::r0;
    static const JSC::MacroAssembler::FPRegisterID FPGpr0 = JSC::ARMRegisters::d0;
    static const JSC::MacroAssembler::FPRegisterID FPGpr1 = JSC::ARMRegisters::d1;

    static RegisterInformation getPlatformRegisterInfo()
    {
        typedef RegisterInfo RI;
        return RegisterInformation()
                << RI(JSC::ARMRegisters::r0,  QStringLiteral("r0"),  RI::RegularRegister,       RI::CallerSaved, RI::Predefined)
                << RI(JSC::ARMRegisters::r1,  QStringLiteral("r1"),  RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARMRegisters::r2,  QStringLiteral("r2"),  RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARMRegisters::r3,  QStringLiteral("r3"),  RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARMRegisters::r4,  QStringLiteral("r4"),  RI::RegularRegister,       RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARMRegisters::r5,  QStringLiteral("r5"),  RI::RegularRegister,       RI::CalleeSaved, RI::Predefined)
                << RI(JSC::ARMRegisters::r6,  QStringLiteral("r6"),  RI::RegularRegister,       RI::CalleeSaved, RI::Predefined)
#if !CPU(ARM_THUMB2)
                << RI(JSC::ARMRegisters::r7,  QStringLiteral("r7"),  RI::RegularRegister,       RI::CalleeSaved, RI::Predefined)
#endif
                << RI(JSC::ARMRegisters::r8,  QStringLiteral("r8"),  RI::RegularRegister,       RI::CalleeSaved, RI::RegAlloc)
#ifdef CAN_USE_R9
                << RI(JSC::ARMRegisters::r9,  QStringLiteral("r9"),  RI::RegularRegister,       RI::CalleeSaved, RI::RegAlloc)
#endif
                << RI(JSC::ARMRegisters::r10, QStringLiteral("r10"), RI::RegularRegister,       RI::CalleeSaved, RI::Predefined)
#if CPU(ARM_THUMB2)
                << RI(JSC::ARMRegisters::r11, QStringLiteral("r11"), RI::RegularRegister,       RI::CalleeSaved, RI::Predefined)
#endif
                << RI(JSC::ARMRegisters::d2,  QStringLiteral("d2"),  RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARMRegisters::d3,  QStringLiteral("d3"),  RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARMRegisters::d4,  QStringLiteral("d4"),  RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARMRegisters::d5,  QStringLiteral("d5"),  RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARMRegisters::d6,  QStringLiteral("d6"),  RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARMRegisters::d8,  QStringLiteral("d8"),  RI::FloatingPointRegister, RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARMRegisters::d9,  QStringLiteral("d9"),  RI::FloatingPointRegister, RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARMRegisters::d10, QStringLiteral("d10"), RI::FloatingPointRegister, RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARMRegisters::d11, QStringLiteral("d11"), RI::FloatingPointRegister, RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARMRegisters::d12, QStringLiteral("d12"), RI::FloatingPointRegister, RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARMRegisters::d13, QStringLiteral("d13"), RI::FloatingPointRegister, RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARMRegisters::d14, QStringLiteral("d14"), RI::FloatingPointRegister, RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARMRegisters::d15, QStringLiteral("d15"), RI::FloatingPointRegister, RI::CalleeSaved, RI::RegAlloc)
                   ;
    }

#undef HAVE_ALU_OPS_WITH_MEM_OPERAND
#undef VALUE_FITS_IN_REGISTER
    static const int RegisterSize = 4;

#define ARGUMENTS_IN_REGISTERS
    static const int RegisterArgumentCount = 4;
    static JSC::MacroAssembler::RegisterID registerForArgument(int index)
    {
        static JSC::MacroAssembler::RegisterID regs[RegisterArgumentCount] = {
            JSC::ARMRegisters::r0,
            JSC::ARMRegisters::r1,
            JSC::ARMRegisters::r2,
            JSC::ARMRegisters::r3
        };

        Q_ASSERT(index >= 0 && index < RegisterArgumentCount);
        return regs[index];
    };

    static const int StackAlignment = 8; // Per AAPCS
    static const int StackShadowSpace = 0;
    static const int StackSpaceAllocatedUponFunctionEntry = 1 * RegisterSize; // Registers saved in platformEnterStandardStackFrame below.

    static void platformEnterStandardStackFrame(JSC::MacroAssembler *as)
    {
        as->push(JSC::ARMRegisters::lr);
        as->push(FramePointerRegister);
    }

    static void platformLeaveStandardStackFrame(JSC::MacroAssembler *as)
    {
        as->pop(FramePointerRegister);
        as->pop(JSC::ARMRegisters::lr);
    }
#endif // ARM (32 bit)

#if CPU(ARM64)
    enum { RegAllocIsSupported = 1 };

    static const JSC::MacroAssembler::RegisterID FramePointerRegister = JSC::ARM64Registers::fp;
    static const JSC::MacroAssembler::RegisterID LocalsRegister = JSC::ARM64Registers::x28;
    static const JSC::MacroAssembler::RegisterID StackPointerRegister = JSC::ARM64Registers::sp;
    static const JSC::MacroAssembler::RegisterID ScratchRegister = JSC::ARM64Registers::x9;
    static const JSC::MacroAssembler::RegisterID EngineRegister = JSC::ARM64Registers::x27;
    static const JSC::MacroAssembler::RegisterID ReturnValueRegister = JSC::ARM64Registers::x0;
    static const JSC::MacroAssembler::FPRegisterID FPGpr0 = JSC::ARM64Registers::q0;
    static const JSC::MacroAssembler::FPRegisterID FPGpr1 = JSC::ARM64Registers::q1;

    static RegisterInformation getPlatformRegisterInfo()
    {
        typedef RegisterInfo RI;
        return RegisterInformation()
                << RI(JSC::ARM64Registers::x0,  QStringLiteral("x0"),  RI::RegularRegister,       RI::CallerSaved, RI::Predefined)
                << RI(JSC::ARM64Registers::x1,  QStringLiteral("x1"),  RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::x2,  QStringLiteral("x2"),  RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::x3,  QStringLiteral("x3"),  RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::x4,  QStringLiteral("x4"),  RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::x5,  QStringLiteral("x5"),  RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::x6,  QStringLiteral("x6"),  RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::x7,  QStringLiteral("x7"),  RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::x8,  QStringLiteral("x8"),  RI::RegularRegister,       RI::CallerSaved, RI::Predefined)
                << RI(JSC::ARM64Registers::x9,  QStringLiteral("x9"),  RI::RegularRegister,       RI::CalleeSaved, RI::Predefined)
                << RI(JSC::ARM64Registers::x10, QStringLiteral("x10"), RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::x11, QStringLiteral("x11"), RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::x12, QStringLiteral("x12"), RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::x13, QStringLiteral("x13"), RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::x14, QStringLiteral("x14"), RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::x15, QStringLiteral("x15"), RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::x19, QStringLiteral("x19"), RI::RegularRegister,       RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::x20, QStringLiteral("x20"), RI::RegularRegister,       RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::x21, QStringLiteral("x21"), RI::RegularRegister,       RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::x22, QStringLiteral("x22"), RI::RegularRegister,       RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::x23, QStringLiteral("x23"), RI::RegularRegister,       RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::x24, QStringLiteral("x24"), RI::RegularRegister,       RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::x25, QStringLiteral("x25"), RI::RegularRegister,       RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::x26, QStringLiteral("x26"), RI::RegularRegister,       RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::x27, QStringLiteral("x27"), RI::RegularRegister,       RI::CalleeSaved, RI::Predefined)
                << RI(JSC::ARM64Registers::x28, QStringLiteral("x28"), RI::RegularRegister,       RI::CalleeSaved, RI::Predefined)

                << RI(JSC::ARM64Registers::q2,  QStringLiteral("q2"),  RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q3,  QStringLiteral("q3"),  RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q4,  QStringLiteral("q4"),  RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q5,  QStringLiteral("q5"),  RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q6,  QStringLiteral("q6"),  RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q8,  QStringLiteral("q8"),  RI::FloatingPointRegister, RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q9,  QStringLiteral("q9"),  RI::FloatingPointRegister, RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q10, QStringLiteral("q10"), RI::FloatingPointRegister, RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q11, QStringLiteral("q11"), RI::FloatingPointRegister, RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q12, QStringLiteral("q12"), RI::FloatingPointRegister, RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q13, QStringLiteral("q13"), RI::FloatingPointRegister, RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q14, QStringLiteral("q14"), RI::FloatingPointRegister, RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q15, QStringLiteral("q15"), RI::FloatingPointRegister, RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q16, QStringLiteral("q16"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q17, QStringLiteral("q17"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q18, QStringLiteral("q18"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q19, QStringLiteral("q19"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q20, QStringLiteral("q20"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q21, QStringLiteral("q21"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q22, QStringLiteral("q22"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q23, QStringLiteral("q23"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q24, QStringLiteral("q24"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q25, QStringLiteral("q25"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q26, QStringLiteral("q26"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q27, QStringLiteral("q27"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q28, QStringLiteral("q28"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q29, QStringLiteral("q29"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q30, QStringLiteral("q30"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::ARM64Registers::q31, QStringLiteral("q31"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                   ;
    }

#undef HAVE_ALU_OPS_WITH_MEM_OPERAND
#define VALUE_FITS_IN_REGISTER
    static const int RegisterSize = 8;

#define ARGUMENTS_IN_REGISTERS
    static const int RegisterArgumentCount = 8;
    static JSC::MacroAssembler::RegisterID registerForArgument(int index)
    {
        static JSC::MacroAssembler::RegisterID regs[RegisterArgumentCount] = {
            JSC::ARM64Registers::x0,
            JSC::ARM64Registers::x1,
            JSC::ARM64Registers::x2,
            JSC::ARM64Registers::x3,
            JSC::ARM64Registers::x4,
            JSC::ARM64Registers::x5,
            JSC::ARM64Registers::x6,
            JSC::ARM64Registers::x7
        };

        Q_ASSERT(index >= 0 && index < RegisterArgumentCount);
        return regs[index];
    };

    static const int StackAlignment = 16;
    static const int StackShadowSpace = 0;
    static const int StackSpaceAllocatedUponFunctionEntry = 1 * RegisterSize; // Registers saved in platformEnterStandardStackFrame below.

    static void platformEnterStandardStackFrame(JSC::MacroAssembler *as)
    {
        as->pushPair(FramePointerRegister, JSC::ARM64Registers::lr);
    }

    static void platformLeaveStandardStackFrame(JSC::MacroAssembler *as)
    {
        as->popPair(FramePointerRegister, JSC::ARM64Registers::lr);
    }
#endif // ARM64

#if defined(Q_PROCESSOR_MIPS_32) && defined(Q_OS_LINUX)
    enum { RegAllocIsSupported = 1 };

    static const JSC::MacroAssembler::RegisterID FramePointerRegister = JSC::MIPSRegisters::fp;
    static const JSC::MacroAssembler::RegisterID StackPointerRegister = JSC::MIPSRegisters::sp;
    static const JSC::MacroAssembler::RegisterID LocalsRegister = JSC::MIPSRegisters::s0;
    static const JSC::MacroAssembler::RegisterID EngineRegister = JSC::MIPSRegisters::s1;
    static const JSC::MacroAssembler::RegisterID ReturnValueRegister = JSC::MIPSRegisters::v0;
    static const JSC::MacroAssembler::RegisterID ScratchRegister = JSC::MIPSRegisters::s2;
    static const JSC::MacroAssembler::FPRegisterID FPGpr0 = JSC::MIPSRegisters::f0;
    static const JSC::MacroAssembler::FPRegisterID FPGpr1 = JSC::MIPSRegisters::f2;

    static RegisterInformation getPlatformRegisterInfo()
    {
        typedef RegisterInfo RI;
        return RegisterInformation()
                // Note: t0, t1, t2, t3 and f16 are already used by MacroAssemblerMIPS.
                << RI(JSC::MIPSRegisters::t4,  QStringLiteral("t4"),  RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::MIPSRegisters::t5,  QStringLiteral("t5"),  RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::MIPSRegisters::t6,  QStringLiteral("t6"),  RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::MIPSRegisters::t7,  QStringLiteral("t7"),  RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::MIPSRegisters::t8,  QStringLiteral("t8"),  RI::RegularRegister,       RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::MIPSRegisters::s0,  QStringLiteral("s0"),  RI::RegularRegister,       RI::CalleeSaved, RI::Predefined)
                << RI(JSC::MIPSRegisters::s1,  QStringLiteral("s1"),  RI::RegularRegister,       RI::CalleeSaved, RI::Predefined)
                << RI(JSC::MIPSRegisters::s2,  QStringLiteral("s2"),  RI::RegularRegister,       RI::CalleeSaved, RI::Predefined)
                << RI(JSC::MIPSRegisters::s3,  QStringLiteral("s3"),  RI::RegularRegister,       RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::MIPSRegisters::f4,  QStringLiteral("f4"),  RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::MIPSRegisters::f6,  QStringLiteral("f6"),  RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::MIPSRegisters::f8,  QStringLiteral("f8"),  RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::MIPSRegisters::f10, QStringLiteral("f10"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::MIPSRegisters::f18, QStringLiteral("f18"), RI::FloatingPointRegister, RI::CallerSaved, RI::RegAlloc)
                << RI(JSC::MIPSRegisters::f20, QStringLiteral("f20"), RI::FloatingPointRegister, RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::MIPSRegisters::f22, QStringLiteral("f22"), RI::FloatingPointRegister, RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::MIPSRegisters::f24, QStringLiteral("f24"), RI::FloatingPointRegister, RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::MIPSRegisters::f26, QStringLiteral("f26"), RI::FloatingPointRegister, RI::CalleeSaved, RI::RegAlloc)
                << RI(JSC::MIPSRegisters::f28, QStringLiteral("f28"), RI::FloatingPointRegister, RI::CalleeSaved, RI::RegAlloc)
                   ;
    }

#undef HAVE_ALU_OPS_WITH_MEM_OPERAND
#undef VALUE_FITS_IN_REGISTER
    static const int RegisterSize = 4;

#define ARGUMENTS_IN_REGISTERS
    static const int RegisterArgumentCount = 4;
    static JSC::MacroAssembler::RegisterID registerForArgument(int index)
    {
        static JSC::MacroAssembler::RegisterID regs[RegisterArgumentCount] = {
            JSC::MIPSRegisters::a0,
            JSC::MIPSRegisters::a1,
            JSC::MIPSRegisters::a2,
            JSC::MIPSRegisters::a3
        };

        Q_ASSERT(index >= 0 && index < RegisterArgumentCount);
        return regs[index];
    };

    static const int StackAlignment = 8;
    static const int StackShadowSpace = 4 * RegisterSize; // Stack space for 4 argument registers.
    static const int StackSpaceAllocatedUponFunctionEntry = 1 * RegisterSize; // Registers saved in platformEnterStandardStackFrame below.

    static void platformEnterStandardStackFrame(JSC::MacroAssembler *as)
    {
        as->push(JSC::MIPSRegisters::ra);
        as->push(FramePointerRegister);
    }

    static void platformLeaveStandardStackFrame(JSC::MacroAssembler *as)
    {
        as->pop(FramePointerRegister);
        as->pop(JSC::MIPSRegisters::ra);
    }
#endif // Linux on MIPS (32 bit)

public: // utility functions
    static const RegisterInformation getRegisterInfo()
    {
        static const RegisterInformation info = getPlatformRegisterInfo();

        return info;
    }
};

} // JIT namespace
} // QV4 namespace

QT_END_NAMESPACE

#endif // ENABLE(ASSEMBLER)

#endif // QV4TARGETPLATFORM_P_H
