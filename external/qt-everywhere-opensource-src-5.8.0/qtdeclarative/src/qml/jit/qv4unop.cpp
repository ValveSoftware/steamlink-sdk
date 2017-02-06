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
#include <qv4unop_p.h>
#include <qv4assembler_p.h>

#if ENABLE(ASSEMBLER)

using namespace QV4;
using namespace JIT;

#define stringIfyx(s) #s
#define stringIfy(s) stringIfyx(s)
#define setOp(operation) \
    do { \
        call = RuntimeCall(qOffsetOf(QV4::Runtime, operation)); name = "Runtime::" stringIfy(operation); \
        needsExceptionCheck = Runtime::Method_##operation##_NeedsExceptionCheck; \
    } while (0)

void Unop::generate(IR::Expr *source, IR::Expr *target)
{
    bool needsExceptionCheck;
    RuntimeCall call;
    const char *name = 0;
    switch (op) {
    case IR::OpNot:
        generateNot(source, target);
        return;
    case IR::OpUMinus:
        generateUMinus(source, target);
        return;
    case IR::OpUPlus: setOp(uPlus); break;
    case IR::OpCompl:
        generateCompl(source, target);
        return;
    case IR::OpIncrement: setOp(increment); break;
    case IR::OpDecrement: setOp(decrement); break;
    default:
        Q_UNREACHABLE();
    } // switch

    Q_ASSERT(call.isValid());
    _as->generateFunctionCallImp(needsExceptionCheck, target, name, call, Assembler::PointerToValue(source));
}

void Unop::generateUMinus(IR::Expr *source, IR::Expr *target)
{
    IR::Temp *targetTemp = target->asTemp();
    if (source->type == IR::SInt32Type) {
        Assembler::RegisterID tReg = Assembler::ScratchRegister;
        if (targetTemp && targetTemp->kind == IR::Temp::PhysicalRegister)
            tReg = (Assembler::RegisterID) targetTemp->index;
        Assembler::RegisterID sReg = _as->toInt32Register(source, tReg);
        _as->move(sReg, tReg);
        _as->neg32(tReg);
        if (!targetTemp || targetTemp->kind != IR::Temp::PhysicalRegister)
            _as->storeInt32(tReg, target);
        return;
    }

    generateRuntimeCall(target, uMinus, Assembler::PointerToValue(source));
}

void Unop::generateNot(IR::Expr *source, IR::Expr *target)
{
    IR::Temp *targetTemp = target->asTemp();
    if (source->type == IR::BoolType) {
        Assembler::RegisterID tReg = Assembler::ScratchRegister;
        if (targetTemp && targetTemp->kind == IR::Temp::PhysicalRegister)
            tReg = (Assembler::RegisterID) targetTemp->index;
        _as->xor32(Assembler::TrustedImm32(0x1), _as->toInt32Register(source, tReg), tReg);
        if (!targetTemp || targetTemp->kind != IR::Temp::PhysicalRegister)
            _as->storeBool(tReg, target);
        return;
    } else if (source->type == IR::SInt32Type) {
        Assembler::RegisterID tReg = Assembler::ScratchRegister;
        if (targetTemp && targetTemp->kind == IR::Temp::PhysicalRegister)
            tReg = (Assembler::RegisterID) targetTemp->index;
        _as->compare32(Assembler::Equal,
                      _as->toInt32Register(source, Assembler::ScratchRegister), Assembler::TrustedImm32(0),
                      tReg);
        if (!targetTemp || targetTemp->kind != IR::Temp::PhysicalRegister)
            _as->storeBool(tReg, target);
        return;
    } else if (source->type == IR::DoubleType) {
        // ###
    }
    // ## generic implementation testing for int/bool

    generateRuntimeCall(target, uNot, Assembler::PointerToValue(source));
}

void Unop::generateCompl(IR::Expr *source, IR::Expr *target)
{
    IR::Temp *targetTemp = target->asTemp();
    if (source->type == IR::SInt32Type) {
        Assembler::RegisterID tReg = Assembler::ScratchRegister;
        if (targetTemp && targetTemp->kind == IR::Temp::PhysicalRegister)
            tReg = (Assembler::RegisterID) targetTemp->index;
        _as->xor32(Assembler::TrustedImm32(0xffffffff), _as->toInt32Register(source, tReg), tReg);
        if (!targetTemp || targetTemp->kind != IR::Temp::PhysicalRegister)
            _as->storeInt32(tReg, target);
        return;
    }
    generateRuntimeCall(target, complement, Assembler::PointerToValue(source));
}

#endif
