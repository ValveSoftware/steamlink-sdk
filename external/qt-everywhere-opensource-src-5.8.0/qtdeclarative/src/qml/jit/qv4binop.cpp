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
#include <qv4binop_p.h>
#include <qv4assembler_p.h>

#if ENABLE(ASSEMBLER)

using namespace QV4;
using namespace JIT;

#define OP(op) \
    { "Runtime::" isel_stringIfy(op), offsetof(QV4::Runtime, op), INT_MIN, 0, 0, QV4::Runtime::Method_##op##_NeedsExceptionCheck }
#define OPCONTEXT(op) \
    { "Runtime::" isel_stringIfy(op), INT_MIN, offsetof(QV4::Runtime, op), 0, 0, QV4::Runtime::Method_##op##_NeedsExceptionCheck }

#define INLINE_OP(op, memOp, immOp) \
    { "Runtime::" isel_stringIfy(op), offsetof(QV4::Runtime, op), INT_MIN, memOp, immOp, QV4::Runtime::Method_##op##_NeedsExceptionCheck }
#define INLINE_OPCONTEXT(op, memOp, immOp) \
    { "Runtime::" isel_stringIfy(op), INT_MIN, offsetof(QV4::Runtime, op), memOp, immOp, QV4::Runtime::Method_##op##_NeedsExceptionCheck }

#define NULL_OP \
    { 0, 0, 0, 0, 0, false }

const Binop::OpInfo Binop::operations[IR::LastAluOp + 1] = {
    NULL_OP, // OpInvalid
    NULL_OP, // OpIfTrue
    NULL_OP, // OpNot
    NULL_OP, // OpUMinus
    NULL_OP, // OpUPlus
    NULL_OP, // OpCompl
    NULL_OP, // OpIncrement
    NULL_OP, // OpDecrement

    INLINE_OP(bitAnd, &Binop::inline_and32, &Binop::inline_and32), // OpBitAnd
    INLINE_OP(bitOr, &Binop::inline_or32, &Binop::inline_or32), // OpBitOr
    INLINE_OP(bitXor, &Binop::inline_xor32, &Binop::inline_xor32), // OpBitXor

    INLINE_OPCONTEXT(add, &Binop::inline_add32, &Binop::inline_add32), // OpAdd
    INLINE_OP(sub, &Binop::inline_sub32, &Binop::inline_sub32), // OpSub
    INLINE_OP(mul, &Binop::inline_mul32, &Binop::inline_mul32), // OpMul

    OP(div), // OpDiv
    OP(mod), // OpMod

    INLINE_OP(shl, &Binop::inline_shl32, &Binop::inline_shl32), // OpLShift
    INLINE_OP(shr, &Binop::inline_shr32, &Binop::inline_shr32), // OpRShift
    INLINE_OP(ushr, &Binop::inline_ushr32, &Binop::inline_ushr32), // OpURShift

    OP(greaterThan), // OpGt
    OP(lessThan), // OpLt
    OP(greaterEqual), // OpGe
    OP(lessEqual), // OpLe
    OP(equal), // OpEqual
    OP(notEqual), // OpNotEqual
    OP(strictEqual), // OpStrictEqual
    OP(strictNotEqual), // OpStrictNotEqual

    OPCONTEXT(instanceof), // OpInstanceof
    OPCONTEXT(in), // OpIn

    NULL_OP, // OpAnd
    NULL_OP // OpOr
};



void Binop::generate(IR::Expr *lhs, IR::Expr *rhs, IR::Expr *target)
{
    if (op != IR::OpMod
            && lhs->type == IR::DoubleType && rhs->type == IR::DoubleType) {
        doubleBinop(lhs, rhs, target);
        return;
    }
    if (lhs->type == IR::SInt32Type && rhs->type == IR::SInt32Type) {
        if (int32Binop(lhs, rhs, target))
            return;
    }

    Assembler::Jump done;
    if (lhs->type != IR::StringType && rhs->type != IR::StringType)
        done = genInlineBinop(lhs, rhs, target);

    // TODO: inline var===null and var!==null
    Binop::OpInfo info = Binop::operation(op);

    if (op == IR::OpAdd &&
            (lhs->type == IR::StringType || rhs->type == IR::StringType)) {
        const Binop::OpInfo stringAdd = OPCONTEXT(addString);
        info = stringAdd;
    }

    RuntimeCall fallBack(info.fallbackImplementation);
    RuntimeCall context(info.contextImplementation);
    if (fallBack.isValid()) {
        as->generateFunctionCallImp(info.needsExceptionCheck, target, info.name, fallBack,
                                     Assembler::PointerToValue(lhs),
                                     Assembler::PointerToValue(rhs));
    } else if (context.isValid()) {
        as->generateFunctionCallImp(info.needsExceptionCheck, target, info.name, context,
                                     Assembler::EngineRegister,
                                     Assembler::PointerToValue(lhs),
                                     Assembler::PointerToValue(rhs));
    } else {
        Q_ASSERT(!"unreachable");
    }

    if (done.isSet())
        done.link(as);

}

void Binop::doubleBinop(IR::Expr *lhs, IR::Expr *rhs, IR::Expr *target)
{
    IR::Temp *targetTemp = target->asTemp();
    Assembler::FPRegisterID targetReg;
    if (targetTemp && targetTemp->kind == IR::Temp::PhysicalRegister)
        targetReg = (Assembler::FPRegisterID) targetTemp->index;
    else
        targetReg = Assembler::FPGpr0;

    switch (op) {
    case IR::OpAdd:
        if (lhs->asConst())
            std::swap(lhs, rhs); // Y = constant + X -> Y = X + constant

#if CPU(X86)
        if (IR::Const *c = rhs->asConst()) { // Y = X + constant -> Y = X; Y += [constant-address]
            as->moveDouble(as->toDoubleRegister(lhs, targetReg), targetReg);
            Assembler::Address addr = as->loadConstant(c, Assembler::ScratchRegister);
            as->addDouble(addr, targetReg);
            break;
        }
        if (IR::Temp *t = rhs->asTemp()) { // Y = X + [temp-memory-address] -> Y = X; Y += [temp-memory-address]
            if (t->kind != IR::Temp::PhysicalRegister) {
                as->moveDouble(as->toDoubleRegister(lhs, targetReg), targetReg);
                as->addDouble(as->loadTempAddress(t), targetReg);
                break;
            }
        }
#endif
        as->addDouble(as->toDoubleRegister(lhs, Assembler::FPGpr0), as->toDoubleRegister(rhs, Assembler::FPGpr1), targetReg);
        break;

    case IR::OpMul:
        if (lhs->asConst())
            std::swap(lhs, rhs); // Y = constant * X -> Y = X * constant

#if CPU(X86)
        if (IR::Const *c = rhs->asConst()) { // Y = X * constant -> Y = X; Y *= [constant-address]
            as->moveDouble(as->toDoubleRegister(lhs, targetReg), targetReg);
            Assembler::Address addr = as->loadConstant(c, Assembler::ScratchRegister);
            as->mulDouble(addr, targetReg);
            break;
        }
        if (IR::Temp *t = rhs->asTemp()) { // Y = X * [temp-memory-address] -> Y = X; Y *= [temp-memory-address]
            if (t->kind != IR::Temp::PhysicalRegister) {
                as->moveDouble(as->toDoubleRegister(lhs, targetReg), targetReg);
                as->mulDouble(as->loadTempAddress(t), targetReg);
                break;
            }
        }
#endif
        as->mulDouble(as->toDoubleRegister(lhs, Assembler::FPGpr0), as->toDoubleRegister(rhs, Assembler::FPGpr1), targetReg);
        break;

    case IR::OpSub:
#if CPU(X86)
        if (IR::Const *c = rhs->asConst()) { // Y = X - constant -> Y = X; Y -= [constant-address]
            as->moveDouble(as->toDoubleRegister(lhs, targetReg), targetReg);
            Assembler::Address addr = as->loadConstant(c, Assembler::ScratchRegister);
            as->subDouble(addr, targetReg);
            break;
        }
        if (IR::Temp *t = rhs->asTemp()) { // Y = X - [temp-memory-address] -> Y = X; Y -= [temp-memory-address]
            if (t->kind != IR::Temp::PhysicalRegister) {
                as->moveDouble(as->toDoubleRegister(lhs, targetReg), targetReg);
                as->subDouble(as->loadTempAddress(t), targetReg);
                break;
            }
        }
#endif
        if (rhs->asTemp() && rhs->asTemp()->kind == IR::Temp::PhysicalRegister
                && targetTemp
                && targetTemp->kind == IR::Temp::PhysicalRegister
                && targetTemp->index == rhs->asTemp()->index) { // Y = X - Y -> Tmp = Y; Y = X - Tmp
            as->moveDouble(as->toDoubleRegister(rhs, Assembler::FPGpr1), Assembler::FPGpr1);
            as->subDouble(as->toDoubleRegister(lhs, Assembler::FPGpr0), Assembler::FPGpr1, targetReg);
            break;
        }

        as->subDouble(as->toDoubleRegister(lhs, Assembler::FPGpr0), as->toDoubleRegister(rhs, Assembler::FPGpr1), targetReg);
        break;

    case IR::OpDiv:
#if CPU(X86)
        if (IR::Const *c = rhs->asConst()) { // Y = X / constant -> Y = X; Y /= [constant-address]
            as->moveDouble(as->toDoubleRegister(lhs, targetReg), targetReg);
            Assembler::Address addr = as->loadConstant(c, Assembler::ScratchRegister);
            as->divDouble(addr, targetReg);
            break;
        }
        if (IR::Temp *t = rhs->asTemp()) { // Y = X / [temp-memory-address] -> Y = X; Y /= [temp-memory-address]
            if (t->kind != IR::Temp::PhysicalRegister) {
                as->moveDouble(as->toDoubleRegister(lhs, targetReg), targetReg);
                as->divDouble(as->loadTempAddress(t), targetReg);
                break;
            }
        }
#endif

        if (rhs->asTemp() && rhs->asTemp()->kind == IR::Temp::PhysicalRegister
                && targetTemp
                && targetTemp->kind == IR::Temp::PhysicalRegister
                && targetTemp->index == rhs->asTemp()->index) { // Y = X / Y -> Tmp = Y; Y = X / Tmp
            as->moveDouble(as->toDoubleRegister(rhs, Assembler::FPGpr1), Assembler::FPGpr1);
            as->divDouble(as->toDoubleRegister(lhs, Assembler::FPGpr0), Assembler::FPGpr1, targetReg);
            break;
        }

        as->divDouble(as->toDoubleRegister(lhs, Assembler::FPGpr0), as->toDoubleRegister(rhs, Assembler::FPGpr1), targetReg);
        break;

    default: {
        Q_ASSERT(target->type == IR::BoolType);
        Assembler::Jump trueCase = as->branchDouble(false, op, lhs, rhs);
        as->storeBool(false, target);
        Assembler::Jump done = as->jump();
        trueCase.link(as);
        as->storeBool(true, target);
        done.link(as);
    } return;
    }

    if (!targetTemp || targetTemp->kind != IR::Temp::PhysicalRegister)
        as->storeDouble(targetReg, target);
}


bool Binop::int32Binop(IR::Expr *leftSource, IR::Expr *rightSource, IR::Expr *target)
{
    Q_ASSERT(leftSource->type == IR::SInt32Type);
    Q_ASSERT(rightSource->type == IR::SInt32Type);

    switch (op) {
    case IR::OpBitAnd:
    case IR::OpBitOr:
    case IR::OpBitXor:
    case IR::OpAdd:
    case IR::OpMul:
        if (leftSource->asConst()) // X = Const op Y -> X = Y op Const
            std::swap(leftSource, rightSource);
        else if (IR::Temp *t = leftSource->asTemp()) {
            if (t->kind != IR::Temp::PhysicalRegister) // X = [address] op Y -> X = Y op [address]
                std::swap(leftSource, rightSource);
        }
        break;

    case IR::OpLShift:
    case IR::OpRShift:
    case IR::OpURShift:
    case IR::OpSub:
        // handled by this method, but we can't flip operands.
        break;

    default:
        return false; // not handled by this method, stop here.
    }

    bool inplaceOpWithAddress = false;

    IR::Temp *targetTemp = target->asTemp();
    Assembler::RegisterID targetReg = Assembler::ReturnValueRegister;
    if (targetTemp && targetTemp->kind == IR::Temp::PhysicalRegister) {
        IR::Temp *rhs = rightSource->asTemp();
        if (!rhs || rhs->kind != IR::Temp::PhysicalRegister || rhs->index != targetTemp->index) {
            // We try to load leftSource into the target's register, but we can't do that if
            // the target register is the same as rightSource.
            targetReg = (Assembler::RegisterID) targetTemp->index;
        } else if (rhs && rhs->kind == IR::Temp::PhysicalRegister && targetTemp->index == rhs->index) {
            // However, if the target register is the same as the rightSource register, we can flip
            // the operands for certain operations.
            switch (op) {
            case IR::OpBitAnd:
            case IR::OpBitOr:
            case IR::OpBitXor:
            case IR::OpAdd:
            case IR::OpMul:
                // X = Y op X -> X = X op Y (or rephrased: X op= Y (so an in-place operation))
                std::swap(leftSource, rightSource);
                targetReg = (Assembler::RegisterID) targetTemp->index;
                break;

            case IR::OpLShift:
            case IR::OpRShift:
            case IR::OpURShift:
            case IR::OpSub:
                break;

            default:
                Q_UNREACHABLE();
                return false;
            }
        }

        // determine if we have X op= [address]
        if (IR::Temp *lhs = leftSource->asTemp()) {
            if (lhs->kind == IR::Temp::PhysicalRegister && lhs->index == targetTemp->index) {
                if (IR::Temp *rhs = rightSource->asTemp()) {
                    if (rhs->kind != IR::Temp::PhysicalRegister) {
                        switch (op) {
                        case IR::OpBitAnd:
                        case IR::OpBitOr:
                        case IR::OpBitXor:
                        case IR::OpAdd:
                        case IR::OpMul:
                            inplaceOpWithAddress = true;
                            break;
                        default:
                            break;
                        }
                    }
                }
            }
        }
    }

    // Special cases:
    switch (op) {
    case IR::OpSub:
        if (rightSource->asTemp() && rightSource->asTemp()->kind == IR::Temp::PhysicalRegister
                && targetTemp
                && targetTemp->kind == IR::Temp::PhysicalRegister
                && targetTemp->index == rightSource->asTemp()->index) {
            // X = Y - X -> Tmp = X; X = Y; X -= Tmp
            targetReg = (Assembler::RegisterID) targetTemp->index;
            as->move(targetReg, Assembler::ScratchRegister);
            as->move(as->toInt32Register(leftSource, targetReg), targetReg);
            as->sub32(Assembler::ScratchRegister, targetReg);
        } else {
            as->move(as->toInt32Register(leftSource, targetReg), targetReg);
            as->sub32(as->toInt32Register(rightSource, Assembler::ScratchRegister), targetReg);
        }
        as->storeInt32(targetReg, target);
        return true;

    case IR::OpLShift:
    case IR::OpRShift:
    case IR::OpURShift:
        if (IR::Const *c = rightSource->asConst()) {
            if ((QV4::Primitive::toUInt32(c->value) & 0x1f) == 0) {
                Assembler::RegisterID r = as->toInt32Register(leftSource, targetReg);
                as->storeInt32(r, target);
                return true;
            }
        }
        break;

    default:
        break;
    }

    Assembler::RegisterID l = as->toInt32Register(leftSource, targetReg);
    if (IR::Const *c = rightSource->asConst()) { // All cases of Y = X op Const
        Assembler::TrustedImm32 r(int(c->value));
        Assembler::TrustedImm32 ur(QV4::Primitive::toUInt32(c->value) & 0x1f);

        switch (op) {
        case IR::OpBitAnd: as->and32(r, l, targetReg); break;
        case IR::OpBitOr:  as->or32 (r, l, targetReg); break;
        case IR::OpBitXor: as->xor32(r, l, targetReg); break;
        case IR::OpAdd:    as->add32(r, l, targetReg); break;
        case IR::OpMul:    as->mul32(r, l, targetReg); break;

        case IR::OpLShift:  as->lshift32(l, ur, targetReg); break;
        case IR::OpRShift:  as->rshift32(l, ur, targetReg); break;
        case IR::OpURShift: as->urshift32(l, ur, targetReg);
            as->storeUInt32(targetReg, target); // IMPORTANT: do NOT do a break here! The stored type of an urshift is different from the other binary operations!
            return true;

        case IR::OpSub: // already handled before
        default: // not handled by this method:
            Q_UNREACHABLE();
            return false;
        }
    } else if (inplaceOpWithAddress) { // All cases of X = X op [address-of-Y]
        Assembler::Pointer rhsAddr = as->loadAddress(Assembler::ScratchRegister, rightSource);
        switch (op) {
        case IR::OpBitAnd: as->and32(rhsAddr, targetReg); break;
        case IR::OpBitOr:  as->or32 (rhsAddr, targetReg); break;
        case IR::OpBitXor: as->xor32(rhsAddr, targetReg); break;
        case IR::OpAdd:    as->add32(rhsAddr, targetReg); break;
        case IR::OpMul:    as->mul32(rhsAddr, targetReg); break;
            break;

        default: // not handled by this method:
            Q_UNREACHABLE();
            return false;
        }
    } else { // All cases of Z = X op Y
        Assembler::RegisterID r = as->toInt32Register(rightSource, Assembler::ScratchRegister);
        switch (op) {
        case IR::OpBitAnd: as->and32(l, r, targetReg); break;
        case IR::OpBitOr:  as->or32 (l, r, targetReg); break;
        case IR::OpBitXor: as->xor32(l, r, targetReg); break;
        case IR::OpAdd:    as->add32(l, r, targetReg); break;
        case IR::OpMul:    as->mul32(l, r, targetReg); break;

#if CPU(X86) || CPU(X86_64)
        // Intel does the & 0x1f on the CPU, so:
        case IR::OpLShift:  as->lshift32(l, r, targetReg); break;
        case IR::OpRShift:  as->rshift32(l, r, targetReg); break;
        case IR::OpURShift: as->urshift32(l, r, targetReg);
            as->storeUInt32(targetReg, target); // IMPORTANT: do NOT do a break here! The stored type of an urshift is different from the other binary operations!
            return true;
#else
        // Not all CPUs accept shifts over more than 31 bits, and some CPUs (like ARM) will do
        // surprising stuff when shifting over 0 bits.
#define CHECK_RHS(op) { \
    as->and32(Assembler::TrustedImm32(0x1f), r, Assembler::ScratchRegister); \
    Assembler::Jump notZero = as->branch32(Assembler::NotEqual, Assembler::ScratchRegister, Assembler::TrustedImm32(0)); \
    as->move(l, targetReg); \
    Assembler::Jump done = as->jump(); \
    notZero.link(as); \
    op; \
    done.link(as); \
}
        case IR::OpLShift: CHECK_RHS(as->lshift32(l, Assembler::ScratchRegister, targetReg)); break;
        case IR::OpRShift: CHECK_RHS(as->rshift32(l, Assembler::ScratchRegister, targetReg)); break;
        case IR::OpURShift:
            CHECK_RHS(as->urshift32(l, Assembler::ScratchRegister, targetReg));
            as->storeUInt32(targetReg, target);
            // IMPORTANT: do NOT do a break here! The stored type of an urshift is different from the other binary operations!
            return true;
#undef CHECK_RHS
#endif

        case IR::OpSub: // already handled before
        default: // not handled by this method:
            Q_UNREACHABLE();
            return false;
        }
    }

    as->storeInt32(targetReg, target);
    return true;
}

static inline Assembler::FPRegisterID getFreeFPReg(IR::Expr *shouldNotOverlap, unsigned hint)
{
    if (IR::Temp *t = shouldNotOverlap->asTemp())
        if (t->type == IR::DoubleType)
            if (t->kind == IR::Temp::PhysicalRegister)
                if (t->index == hint)
                    return Assembler::FPRegisterID(hint + 1);
    return Assembler::FPRegisterID(hint);
}

Assembler::Jump Binop::genInlineBinop(IR::Expr *leftSource, IR::Expr *rightSource, IR::Expr *target)
{
    Assembler::Jump done;

    // Try preventing a call for a few common binary operations. This is used in two cases:
    // - no register allocation was performed (not available for the platform, or the IR was
    //   not transformed into SSA)
    // - type inference found that either or both operands can be of non-number type, and the
    //   register allocator will have prepared for a call (meaning: all registers that do not
    //   hold operands are spilled to the stack, which makes them available here)
    // Note: FPGPr0 can still not be used, because uint32->double conversion uses it as a scratch
    //       register.
    switch (op) {
    case IR::OpAdd: {
        Assembler::FPRegisterID lReg = getFreeFPReg(rightSource, 2);
        Assembler::FPRegisterID rReg = getFreeFPReg(leftSource, 4);
        Assembler::Jump leftIsNoDbl = as->genTryDoubleConversion(leftSource, lReg);
        Assembler::Jump rightIsNoDbl = as->genTryDoubleConversion(rightSource, rReg);

        as->addDouble(rReg, lReg);
        as->storeDouble(lReg, target);
        done = as->jump();

        if (leftIsNoDbl.isSet())
            leftIsNoDbl.link(as);
        if (rightIsNoDbl.isSet())
            rightIsNoDbl.link(as);
    } break;
    case IR::OpMul: {
        Assembler::FPRegisterID lReg = getFreeFPReg(rightSource, 2);
        Assembler::FPRegisterID rReg = getFreeFPReg(leftSource, 4);
        Assembler::Jump leftIsNoDbl = as->genTryDoubleConversion(leftSource, lReg);
        Assembler::Jump rightIsNoDbl = as->genTryDoubleConversion(rightSource, rReg);

        as->mulDouble(rReg, lReg);
        as->storeDouble(lReg, target);
        done = as->jump();

        if (leftIsNoDbl.isSet())
            leftIsNoDbl.link(as);
        if (rightIsNoDbl.isSet())
            rightIsNoDbl.link(as);
    } break;
    case IR::OpSub: {
        Assembler::FPRegisterID lReg = getFreeFPReg(rightSource, 2);
        Assembler::FPRegisterID rReg = getFreeFPReg(leftSource, 4);
        Assembler::Jump leftIsNoDbl = as->genTryDoubleConversion(leftSource, lReg);
        Assembler::Jump rightIsNoDbl = as->genTryDoubleConversion(rightSource, rReg);

        as->subDouble(rReg, lReg);
        as->storeDouble(lReg, target);
        done = as->jump();

        if (leftIsNoDbl.isSet())
            leftIsNoDbl.link(as);
        if (rightIsNoDbl.isSet())
            rightIsNoDbl.link(as);
    } break;
    case IR::OpDiv: {
        Assembler::FPRegisterID lReg = getFreeFPReg(rightSource, 2);
        Assembler::FPRegisterID rReg = getFreeFPReg(leftSource, 4);
        Assembler::Jump leftIsNoDbl = as->genTryDoubleConversion(leftSource, lReg);
        Assembler::Jump rightIsNoDbl = as->genTryDoubleConversion(rightSource, rReg);

        as->divDouble(rReg, lReg);
        as->storeDouble(lReg, target);
        done = as->jump();

        if (leftIsNoDbl.isSet())
            leftIsNoDbl.link(as);
        if (rightIsNoDbl.isSet())
            rightIsNoDbl.link(as);
    } break;
    default:
        break;
    }

    return done;
}

#endif
