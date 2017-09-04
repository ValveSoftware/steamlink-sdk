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
#ifndef QV4BINOP_P_H
#define QV4BINOP_P_H

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

#include <qv4jsir_p.h>
#include <qv4isel_masm_p.h>
#include <qv4assembler_p.h>

QT_BEGIN_NAMESPACE

#if ENABLE(ASSEMBLER)

namespace QV4 {
namespace JIT {

template <typename JITAssembler>
struct Binop {
    Binop(JITAssembler *assembler, IR::AluOp operation)
        : as(assembler)
        , op(operation)
    {}

    using Jump = typename JITAssembler::Jump;
    using Address = typename JITAssembler::Address;
    using RegisterID = typename JITAssembler::RegisterID;
    using FPRegisterID = typename JITAssembler::FPRegisterID;
    using TrustedImm32 = typename JITAssembler::TrustedImm32;
    using ResultCondition = typename JITAssembler::ResultCondition;
    using RelationalCondition = typename JITAssembler::RelationalCondition;
    using Pointer = typename JITAssembler::Pointer;
    using PointerToValue = typename JITAssembler::PointerToValue;

    void generate(IR::Expr *lhs, IR::Expr *rhs, IR::Expr *target);
    void doubleBinop(IR::Expr *lhs, IR::Expr *rhs, IR::Expr *target);
    bool int32Binop(IR::Expr *leftSource, IR::Expr *rightSource, IR::Expr *target);
    Jump genInlineBinop(IR::Expr *leftSource, IR::Expr *rightSource, IR::Expr *target);

    typedef Jump (Binop::*MemRegOp)(Address, RegisterID);
    typedef Jump (Binop::*ImmRegOp)(TrustedImm32, RegisterID);

    struct OpInfo {
        const char *name;
        Runtime::RuntimeMethods fallbackImplementation;
        Runtime::RuntimeMethods contextImplementation;
        MemRegOp inlineMemRegOp;
        ImmRegOp inlineImmRegOp;
        bool needsExceptionCheck;
    };

    static const OpInfo operations[IR::LastAluOp + 1];
    static const OpInfo &operation(IR::AluOp operation)
    { return operations[operation]; }

    Jump inline_add32(Address addr, RegisterID reg)
    {
#if HAVE(ALU_OPS_WITH_MEM_OPERAND)
        return as->branchAdd32(ResultCondition::Overflow, addr, reg);
#else
        as->load32(addr, JITAssembler::ScratchRegister);
        return as->branchAdd32(ResultCondition::Overflow, JITAssembler::ScratchRegister, reg);
#endif
    }

    Jump inline_add32(TrustedImm32 imm, RegisterID reg)
    {
        return as->branchAdd32(ResultCondition::Overflow, imm, reg);
    }

    Jump inline_sub32(Address addr, RegisterID reg)
    {
#if HAVE(ALU_OPS_WITH_MEM_OPERAND)
        return as->branchSub32(ResultCondition::Overflow, addr, reg);
#else
        as->load32(addr, JITAssembler::ScratchRegister);
        return as->branchSub32(ResultCondition::Overflow, JITAssembler::ScratchRegister, reg);
#endif
    }

    Jump inline_sub32(TrustedImm32 imm, RegisterID reg)
    {
        return as->branchSub32(ResultCondition::Overflow, imm, reg);
    }

    Jump inline_mul32(Address addr, RegisterID reg)
    {
#if HAVE(ALU_OPS_WITH_MEM_OPERAND)
        return as->branchMul32(JITAssembler::Overflow, addr, reg);
#else
        as->load32(addr, JITAssembler::ScratchRegister);
        return as->branchMul32(ResultCondition::Overflow, JITAssembler::ScratchRegister, reg);
#endif
    }

    Jump inline_mul32(TrustedImm32 imm, RegisterID reg)
    {
        return as->branchMul32(ResultCondition::Overflow, imm, reg, reg);
    }

    Jump inline_shl32(Address addr, RegisterID reg)
    {
        as->load32(addr, JITAssembler::ScratchRegister);
        as->and32(TrustedImm32(0x1f), JITAssembler::ScratchRegister);
        as->lshift32(JITAssembler::ScratchRegister, reg);
        return Jump();
    }

    Jump inline_shl32(TrustedImm32 imm, RegisterID reg)
    {
        imm.m_value &= 0x1f;
        as->lshift32(imm, reg);
        return Jump();
    }

    Jump inline_shr32(Address addr, RegisterID reg)
    {
        as->load32(addr, JITAssembler::ScratchRegister);
        as->and32(TrustedImm32(0x1f), JITAssembler::ScratchRegister);
        as->rshift32(JITAssembler::ScratchRegister, reg);
        return Jump();
    }

    Jump inline_shr32(TrustedImm32 imm, RegisterID reg)
    {
        imm.m_value &= 0x1f;
        as->rshift32(imm, reg);
        return Jump();
    }

    Jump inline_ushr32(Address addr, RegisterID reg)
    {
        as->load32(addr, JITAssembler::ScratchRegister);
        as->and32(TrustedImm32(0x1f), JITAssembler::ScratchRegister);
        as->urshift32(JITAssembler::ScratchRegister, reg);
        return as->branchTest32(ResultCondition::Signed, reg, reg);
    }

    Jump inline_ushr32(TrustedImm32 imm, RegisterID reg)
    {
        imm.m_value &= 0x1f;
        as->urshift32(imm, reg);
        return as->branchTest32(ResultCondition::Signed, reg, reg);
    }

    Jump inline_and32(Address addr, RegisterID reg)
    {
#if HAVE(ALU_OPS_WITH_MEM_OPERAND)
        as->and32(addr, reg);
#else
        as->load32(addr, JITAssembler::ScratchRegister);
        as->and32(JITAssembler::ScratchRegister, reg);
#endif
        return Jump();
    }

    Jump inline_and32(TrustedImm32 imm, RegisterID reg)
    {
        as->and32(imm, reg);
        return Jump();
    }

    Jump inline_or32(Address addr, RegisterID reg)
    {
#if HAVE(ALU_OPS_WITH_MEM_OPERAND)
        as->or32(addr, reg);
#else
        as->load32(addr, JITAssembler::ScratchRegister);
        as->or32(JITAssembler::ScratchRegister, reg);
#endif
        return Jump();
    }

    Jump inline_or32(TrustedImm32 imm, RegisterID reg)
    {
        as->or32(imm, reg);
        return Jump();
    }

    Jump inline_xor32(Address addr, RegisterID reg)
    {
#if HAVE(ALU_OPS_WITH_MEM_OPERAND)
        as->xor32(addr, reg);
#else
        as->load32(addr, JITAssembler::ScratchRegister);
        as->xor32(JITAssembler::ScratchRegister, reg);
#endif
        return Jump();
    }

    Jump inline_xor32(TrustedImm32 imm, RegisterID reg)
    {
        as->xor32(imm, reg);
        return Jump();
    }



    JITAssembler *as;
    IR::AluOp op;
};

}
}

#endif

QT_END_NAMESPACE

#endif
