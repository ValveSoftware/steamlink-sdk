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

#include "qbuiltintypes_p.h"
#include "qcommonsequencetypes_p.h"
#include "qcommonvalues_p.h"
#include "qgenericsequencetype_p.h"
#include "qinteger_p.h"
#include "qliteral_p.h"
#include "qrangeiterator_p.h"

#include "qrangeexpression_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

RangeExpression::RangeExpression(const Expression::Ptr &operand1,
                                 const Expression::Ptr &operand2) : PairContainer(operand1, operand2)
{
}

Item::Iterator::Ptr RangeExpression::evaluateSequence(const DynamicContext::Ptr &context) const
{
    const Item s(m_operand1->evaluateSingleton(context));

    if(!s)
        return CommonValues::emptyIterator;

    const Item e(m_operand2->evaluateSingleton(context));
    if(!e)
        return CommonValues::emptyIterator;

    const xsInteger start = s.as<Numeric>()->toInteger();
    const xsInteger end = e.as<Numeric>()->toInteger();

    if(start > end)
        return CommonValues::emptyIterator;
    else if(start == end)
        return makeSingletonIterator(s);
    else
        return Item::Iterator::Ptr(new RangeIterator(start, RangeIterator::Forward, end));
}

Item RangeExpression::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    return m_operand1->evaluateSingleton(context);
}

SequenceType::List RangeExpression::expectedOperandTypes() const
{
    SequenceType::List result;
    result.append(CommonSequenceTypes::ZeroOrOneInteger);
    result.append(CommonSequenceTypes::ZeroOrOneInteger);
    return result;
}

SequenceType::Ptr RangeExpression::staticType() const
{
    /* This logic makes the Cardinality more specific. */
    Cardinality::Count from;
    bool hasFrom;

    if(m_operand1->is(IDIntegerValue))
    {
        from = m_operand1->as<Literal>()->item().as<Integer>()->toInteger();
        hasFrom = true;
    }
    else
    {
        hasFrom = false;
        from = 0;
    }

    /* We can't check whether to is -1 since maybe the user wrote -1. Hence
     * hasTo is required. */
    bool hasTo;
    Cardinality::Count to;

    if(m_operand2->is(IDIntegerValue))
    {
        const xsInteger asInt = m_operand2->as<Literal>()->item().as<Integer>()->toInteger();
        to = asInt;

        if(to == asInt)
            hasTo = true;
        else
        {
            /* Cardinality::Count is not the same as type xsInteger. We had overflow. */
            to = -1;
            hasTo = false;
        }
    }
    else
    {
        to = -1;
        hasTo = false;
    }

    if(hasTo && hasFrom)
    {
        if(from > to)
        {
            /* The query is incorrectly written, we'll evaluate to the empty sequence.
             * Just return what's correct. */
            return CommonSequenceTypes::ZeroOrMoreIntegers;
        }
        else
        {
            Cardinality::Count count = (to - from) + 1; /* + 1, since it's inclusive. */
            return makeGenericSequenceType(BuiltinTypes::xsInteger, Cardinality::fromExact(count));
        }
    }
    else
    {
        /* We can't do fromExact(from, -1) since the latter can evaluate to a value that actually is
         * lower than from, although that unfortunately is very unlikely. */
        return CommonSequenceTypes::ZeroOrMoreIntegers;
    }
}

Expression::Properties RangeExpression::properties() const
{
    return Expression::DisableElimination;
}

ExpressionVisitorResult::Ptr RangeExpression::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

QT_END_NAMESPACE
