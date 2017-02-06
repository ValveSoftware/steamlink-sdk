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

struct Binop {
    Binop(Assembler *assembler, IR::AluOp operation)
        : as(assembler)
        , op(operation)
    {}

    void generate(IR::Expr *lhs, IR::Expr *rhs, IR::Expr *target);
    void doubleBinop(IR::Expr *lhs, IR::Expr *rhs, IR::Expr *target);
    bool int32Binop(IR::Expr *leftSource, IR::Expr *rightSource, IR::Expr *target);
    Assembler::Jump genInlineBinop(IR::Expr *leftSource, IR::Expr *rightSource, IR::Expr *target);

    typedef Assembler::Jump (Binop::*MemRegOp)(Assembler::Address, Assembler::RegisterID);
    typedef Assembler::Jump (Binop::*ImmRegOp)(Assembler::TrustedImm32, Assembler::RegisterID);

    struct OpInfo {
        const char *name;
        int fallbackImplementation; // offsetOf(Runtime,...)
        int contextImplementation; // offsetOf(Runtime,...)
        MemRegOp inlineMemRegOp;
        ImmRegOp inlineImmRegOp;
        bool needsExceptionCheck;
    };

    static const OpInfo operations[IR::LastAluOp + 1];
    static const OpInfo &operation(IR::AluOp operation)
    { return operations[operation]; }

    Assembler::Jump inline_add32(Assembler::Address addr, Assembler::RegisterID reg)
    {
#if HAVE(ALU_OPS_WITH_MEM_OPERAND)
        return as->branchAdd32(Assembler::Overflow, addr, reg);
#else
        as->load32(addr, Assembler::ScratchRegister);
        return as->branchAdd32(Assembler::Overflow, Assembler::ScratchRegister, reg);
#endif
    }

    Assembler::Jump inline_add32(Assembler::TrustedImm32 imm, Assembler::RegisterID reg)
    {
        return as->branchAdd32(Assembler::Overflow, imm, reg);
    }

    Assembler::Jump inline_sub32(Assembler::Address addr, Assembler::RegisterID reg)
    {
#if HAVE(ALU_OPS_WITH_MEM_OPERAND)
        return as->branchSub32(Assembler::Overflow, addr, reg);
#else
        as->load32(addr, Assembler::ScratchRegister);
        return as->branchSub32(Assembler::Overflow, Assembler::ScratchRegister, reg);
#endif
    }

    Assembler::Jump inline_sub32(Assembler::TrustedImm32 imm, Assembler::RegisterID reg)
    {
        return as->branchSub32(Assembler::Overflow, imm, reg);
    }

    Assembler::Jump inline_mul32(Assembler::Address addr, Assembler::RegisterID reg)
    {
#if HAVE(ALU_OPS_WITH_MEM_OPERAND)
        return as->branchMul32(Assembler::Overflow, addr, reg);
#else
        as->load32(addr, Assembler::ScratchRegister);
        return as->branchMul32(Assembler::Overflow, Assembler::ScratchRegister, reg);
#endif
    }

    Assembler::Jump inline_mul32(Assembler::TrustedImm32 imm, Assembler::RegisterID reg)
    {
        return as->branchMul32(Assembler::Overflow, imm, reg, reg);
    }

    Assembler::Jump inline_shl32(Assembler::Address addr, Assembler::RegisterID reg)
    {
        as->load32(addr, Assembler::ScratchRegister);
        as->and32(Assembler::TrustedImm32(0x1f), Assembler::ScratchRegister);
        as->lshift32(Assembler::ScratchRegister, reg);
        return Assembler::Jump();
    }

    Assembler::Jump inline_shl32(Assembler::TrustedImm32 imm, Assembler::RegisterID reg)
    {
        imm.m_value &= 0x1f;
        as->lshift32(imm, reg);
        return Assembler::Jump();
    }

    Assembler::Jump inline_shr32(Assembler::Address addr, Assembler::RegisterID reg)
    {
        as->load32(addr, Assembler::ScratchRegister);
        as->and32(Assembler::TrustedImm32(0x1f), Assembler::ScratchRegister);
        as->rshift32(Assembler::ScratchRegister, reg);
        return Assembler::Jump();
    }

    Assembler::Jump inline_shr32(Assembler::TrustedImm32 imm, Assembler::RegisterID reg)
    {
        imm.m_value &= 0x1f;
        as->rshift32(imm, reg);
        return Assembler::Jump();
    }

    Assembler::Jump inline_ushr32(Assembler::Address addr, Assembler::RegisterID reg)
    {
        as->load32(addr, Assembler::ScratchRegister);
        as->and32(Assembler::TrustedImm32(0x1f), Assembler::ScratchRegister);
        as->urshift32(Assembler::ScratchRegister, reg);
        return as->branchTest32(Assembler::Signed, reg, reg);
    }

    Assembler::Jump inline_ushr32(Assembler::TrustedImm32 imm, Assembler::RegisterID reg)
    {
        imm.m_value &= 0x1f;
        as->urshift32(imm, reg);
        return as->branchTest32(Assembler::Signed, reg, reg);
    }

    Assembler::Jump inline_and32(Assembler::Address addr, Assembler::RegisterID reg)
    {
#if HAVE(ALU_OPS_WITH_MEM_OPERAND)
        as->and32(addr, reg);
#else
        as->load32(addr, Assembler::ScratchRegister);
        as->and32(Assembler::ScratchRegister, reg);
#endif
        return Assembler::Jump();
    }

    Assembler::Jump inline_and32(Assembler::TrustedImm32 imm, Assembler::RegisterID reg)
    {
        as->and32(imm, reg);
        return Assembler::Jump();
    }

    Assembler::Jump inline_or32(Assembler::Address addr, Assembler::RegisterID reg)
    {
#if HAVE(ALU_OPS_WITH_MEM_OPERAND)
        as->or32(addr, reg);
#else
        as->load32(addr, Assembler::ScratchRegister);
        as->or32(Assembler::ScratchRegister, reg);
#endif
        return Assembler::Jump();
    }

    Assembler::Jump inline_or32(Assembler::TrustedImm32 imm, Assembler::RegisterID reg)
    {
        as->or32(imm, reg);
        return Assembler::Jump();
    }

    Assembler::Jump inline_xor32(Assembler::Address addr, Assembler::RegisterID reg)
    {
#if HAVE(ALU_OPS_WITH_MEM_OPERAND)
        as->xor32(addr, reg);
#else
        as->load32(addr, Assembler::ScratchRegister);
        as->xor32(Assembler::ScratchRegister, reg);
#endif
        return Assembler::Jump();
    }

    Assembler::Jump inline_xor32(Assembler::TrustedImm32 imm, Assembler::RegisterID reg)
    {
        as->xor32(imm, reg);
        return Assembler::Jump();
    }



    Assembler *as;
    IR::AluOp op;
};

}
}

#endif

QT_END_NAMESPACE

#endif
