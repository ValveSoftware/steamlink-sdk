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

#ifndef QV4ISEL_UTIL_P_H
#define QV4ISEL_UTIL_P_H

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

#include "private/qv4value_p.h"
#include "qv4jsir_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

inline bool canConvertToSignedInteger(double value)
{
    int ival = (int) value;
    // +0 != -0, so we need to convert to double when negating 0
    return ival == value && !(value == 0 && isNegative(value));
}

inline bool canConvertToUnsignedInteger(double value)
{
    unsigned uval = (unsigned) value;
    // +0 != -0, so we need to convert to double when negating 0
    return uval == value && !(value == 0 && isNegative(value));
}

inline Primitive convertToValue(IR::Const *c)
{
    switch (c->type) {
    case IR::MissingType:
        return Primitive::emptyValue();
    case IR::NullType:
        return Primitive::nullValue();
    case IR::UndefinedType:
        return Primitive::undefinedValue();
    case IR::BoolType:
        return Primitive::fromBoolean(c->value != 0);
    case IR::SInt32Type:
        return Primitive::fromInt32(int(c->value));
    case IR::UInt32Type:
        return Primitive::fromUInt32(unsigned(c->value));
    case IR::DoubleType:
        return Primitive::fromDouble(c->value);
    case IR::NumberType: {
        int ival = (int)c->value;
        if (canConvertToSignedInteger(c->value)) {
            return Primitive::fromInt32(ival);
        } else {
            return Primitive::fromDouble(c->value);
        }
    }
    default:
        Q_UNREACHABLE();
    }
    // unreachable, but the function must return something
    return Primitive::undefinedValue();
}

class ConvertTemps
{
    void renumber(IR::Temp *t)
    {
        if (t->kind != IR::Temp::VirtualRegister)
            return;

        int stackSlot = _stackSlotForTemp.value(t->index, -1);
        if (stackSlot == -1) {
            stackSlot = allocateFreeSlot();
            _stackSlotForTemp[t->index] = stackSlot;
        }

        t->kind = IR::Temp::StackSlot;
        t->index = stackSlot;
    }

protected:
    int _nextUnusedStackSlot;
    QHash<int, int> _stackSlotForTemp;
    IR::BasicBlock *_currentBasicBlock;
    virtual int allocateFreeSlot()
    {
        return _nextUnusedStackSlot++;
    }

    virtual void process(IR::Stmt *s)
    {
        visit(s);
    }

public:
    ConvertTemps()
        : _nextUnusedStackSlot(0)
        , _currentBasicBlock(0)
    {}

    void toStackSlots(IR::Function *function)
    {
        _stackSlotForTemp.reserve(function->tempCount);

        for (IR::BasicBlock *bb : function->basicBlocks()) {
            if (bb->isRemoved())
                continue;
            _currentBasicBlock = bb;
            for (IR::Stmt *s : bb->statements())
                process(s);
        }

        function->tempCount = _nextUnusedStackSlot;
    }

protected:
    void visit(IR::Stmt *s) {
        switch (s->stmtKind) {
        case IR::Stmt::PhiStmt:
            visitPhi(s->asPhi());
            break;
        default:
            STMT_VISIT_ALL_KINDS(s);
            break;
        }
    }

    virtual void visitPhi(IR::Phi *)
    { Q_UNREACHABLE(); }

private:
    void visit(IR::Expr *e) {
        if (auto temp = e->asTemp()) {
            renumber(temp);
        } else {
            EXPR_VISIT_ALL_KINDS(e);
        }
    }
};
} // namespace QV4

QT_END_NAMESPACE

#endif // QV4ISEL_UTIL_P_H
