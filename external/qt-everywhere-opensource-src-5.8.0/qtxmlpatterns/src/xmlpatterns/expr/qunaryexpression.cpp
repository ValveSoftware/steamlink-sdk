/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
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

#include "qarithmeticexpression_p.h"
#include "qcommonvalues_p.h"
#include "qliteral_p.h"
#include "qschemanumeric_p.h"

#include "qunaryexpression_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

UnaryExpression::UnaryExpression(const AtomicMathematician::Operator op,
                                 const Expression::Ptr &operand,
                                 const StaticContext::Ptr &context) : ArithmeticExpression(wrapLiteral(CommonValues::IntegerZero, context, operand.data()),
                                                                                           op,
                                                                                           operand)
{
    Q_ASSERT(op == AtomicMathematician::Substract ||
             op == AtomicMathematician::Add);
    Q_ASSERT(context);
}

Item UnaryExpression::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    if(operatorID() == AtomicMathematician::Substract)
    {
        const Item item(m_operand2->evaluateSingleton(context));

        if (item)
            // If the item is an untypedAtomic (or atomicString), the cast to Numeric will
            // return a garbage object and will most likely cause a crash.
            // There is simply no way to static cast from an instance of AtomicString to a Number.
            return item.as<Numeric>()->toNegated();
        else
            return Item();
    }
    else
        return m_operand2->evaluateSingleton(context);
}

QT_END_NAMESPACE
