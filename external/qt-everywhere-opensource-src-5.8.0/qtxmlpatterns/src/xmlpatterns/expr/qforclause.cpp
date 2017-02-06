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
#include "qemptysequence_p.h"
#include "qgenericsequencetype_p.h"
#include "qitemmappingiterator_p.h"
#include "qoptimizationpasses_p.h"
#include "qsequencemappingiterator_p.h"

#include "qforclause_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

ForClause::ForClause(const VariableSlotID varSlot,
                     const Expression::Ptr &bindingSequence,
                     const Expression::Ptr &returnExpression,
                     const VariableSlotID positionSlot) : PairContainer(bindingSequence, returnExpression),
                                                          m_varSlot(varSlot),
                                                          m_positionSlot(positionSlot),
                                                          m_allowsMany(true)
{
    Q_ASSERT(m_positionSlot > -2);
}

Item ForClause::mapToItem(const Item &item,
                          const DynamicContext::Ptr &context) const
{
    context->setRangeVariable(m_varSlot, item);
    return m_operand2->evaluateSingleton(context);
}

Item::Iterator::Ptr ForClause::mapToSequence(const Item &item,
                                             const DynamicContext::Ptr &context) const
{
    context->setRangeVariable(m_varSlot, item);
    return m_operand2->evaluateSequence(context);
}

void ForClause::riggPositionalVariable(const DynamicContext::Ptr &context,
                                       const Item::Iterator::Ptr &source) const
{
    if(m_positionSlot > -1)
        context->setPositionIterator(m_positionSlot, source);
}

Item::Iterator::Ptr ForClause::evaluateSequence(const DynamicContext::Ptr &context) const
{
    const Item::Iterator::Ptr source(m_operand1->evaluateSequence(context));

    riggPositionalVariable(context, source);

    if(m_allowsMany)
    {
        return makeSequenceMappingIterator<Item>(ConstPtr(this),
                                                 source,
                                                 context);
    }
    else
    {
        return makeItemMappingIterator<Item>(ConstPtr(this),
                                             source,
                                             context);
    }
}

Item ForClause::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    return evaluateSequence(context)->next();
}

void ForClause::evaluateToSequenceReceiver(const DynamicContext::Ptr &context) const
{
    Item::Iterator::Ptr it;
    const Item::Iterator::Ptr source(m_operand1->evaluateSequence(context));

    riggPositionalVariable(context, source);

    Item next(source->next());

    while(next)
    {
        context->setRangeVariable(m_varSlot, next);
        m_operand2->evaluateToSequenceReceiver(context);
        next = source->next();
    }
}

Expression::Ptr ForClause::typeCheck(const StaticContext::Ptr &context,
                                     const SequenceType::Ptr &reqType)
{
    const Expression::Ptr me(PairContainer::typeCheck(context, reqType));
    const Cardinality card(m_operand1->staticType()->cardinality());

    /* If our source is empty we will always evaluate to the empty sequence, so rewrite. */
    if(card.isEmpty())
        return EmptySequence::create(this, context);
    else
        return me;

    /* This breaks because  the variable references haven't rewritten themselves, so
     * they dangle. When this is fixed, evaluateSingleton can be removed. */
    /*
    else if(card->allowsMany())
        return me;
    else
        return m_operand2;
        */
}

Expression::Ptr ForClause::compress(const StaticContext::Ptr &context)
{
    const Expression::Ptr me(PairContainer::compress(context));
    if(me != this)
        return me;

    /* This is done after calling PairContainer::typeCheck(). The advantage of this is that
     * m_allowsMany is updated according to what the operand says after it has compressed. However,
     * if it was initialized to false(as it once was..), ForClause::evaluateSequence()
     * would potentially have been called by PairContainer::compress(), and it would have
     * used an unsafe branch. */
    m_allowsMany = m_operand2->staticType()->cardinality().allowsMany();

    return me;
}

SequenceType::Ptr ForClause::staticType() const
{
    const SequenceType::Ptr returnType(m_operand2->staticType());

    return makeGenericSequenceType(returnType->itemType(),
                                   m_operand1->staticType()->cardinality()
                                        * /* multiply operator */
                                   returnType->cardinality());
}

SequenceType::List ForClause::expectedOperandTypes() const
{
    SequenceType::List result;
    result.append(CommonSequenceTypes::ZeroOrMoreItems);
    result.append(CommonSequenceTypes::ZeroOrMoreItems);
    return result;
}

ExpressionVisitorResult::Ptr ForClause::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

OptimizationPass::List ForClause::optimizationPasses() const
{
    return OptimizationPasses::forPasses;
}

Expression::ID ForClause::id() const
{
    return IDForClause;
}

QT_END_NAMESPACE
