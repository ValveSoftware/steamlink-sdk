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

#include "qcommonsequencetypes_p.h"
#include "qgenericsequencetype_p.h"
#include "qoptimizationpasses_p.h"

#include "qifthenclause_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

IfThenClause::IfThenClause(const Expression::Ptr &test,
                           const Expression::Ptr &then,
                           const Expression::Ptr &el) : TripleContainer(test, then, el)
{
}

Item::Iterator::Ptr IfThenClause::evaluateSequence(const DynamicContext::Ptr &context) const
{
    return m_operand1->evaluateEBV(context)
            ? m_operand2->evaluateSequence(context)
            : m_operand3->evaluateSequence(context);
}

Item IfThenClause::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    return m_operand1->evaluateEBV(context)
            ? m_operand2->evaluateSingleton(context)
            : m_operand3->evaluateSingleton(context);
}

bool IfThenClause::evaluateEBV(const DynamicContext::Ptr &context) const
{
    return m_operand1->evaluateEBV(context)
            ? m_operand2->evaluateEBV(context)
            : m_operand3->evaluateEBV(context);
}

void IfThenClause::evaluateToSequenceReceiver(const DynamicContext::Ptr &context) const
{
    if(m_operand1->evaluateEBV(context))
        m_operand2->evaluateToSequenceReceiver(context);
    else
        m_operand3->evaluateToSequenceReceiver(context);
}

Expression::Ptr IfThenClause::compress(const StaticContext::Ptr &context)
{
    const Expression::Ptr me(TripleContainer::compress(context));

    if(me != this)
        return me;

    /* All operands mustn't be evaluated in order for const folding to
     * be possible. Let's see how far we get. */

    if(m_operand1->isEvaluated())
    {
        if(m_operand1->evaluateEBV(context->dynamicContext()))
            return m_operand2;
        else
            return m_operand3;
    }
    else
        return me;
}

QList<QExplicitlySharedDataPointer<OptimizationPass> > IfThenClause::optimizationPasses() const
{
    return OptimizationPasses::ifThenPasses;
}

SequenceType::List IfThenClause::expectedOperandTypes() const
{
    SequenceType::List result;
    result.append(CommonSequenceTypes::EBV);
    result.append(CommonSequenceTypes::ZeroOrMoreItems);
    result.append(CommonSequenceTypes::ZeroOrMoreItems);
    return result;
}

SequenceType::Ptr IfThenClause::staticType() const
{
    const SequenceType::Ptr t1(m_operand2->staticType());
    const SequenceType::Ptr t2(m_operand3->staticType());

    return makeGenericSequenceType(t1->itemType() | t2->itemType(),
                                   t1->cardinality() | t2->cardinality());
}

ExpressionVisitorResult::Ptr IfThenClause::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

Expression::ID IfThenClause::id() const
{
    return IDIfThenClause;
}

/*
Expression::Properties IfThenClause::properties() const
{
    return   m_operand1->properties()
           | m_operand2->properties()
           | m_operand3->properties()
           & (  Expression::RequiresFocus
              | Expression::IsEvaluated
              | Expression::DisableElimination);
}
*/
QT_END_NAMESPACE
