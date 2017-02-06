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
#include "qitemmappingiterator_p.h"

#include "qquantifiedexpression_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

QuantifiedExpression::QuantifiedExpression(const VariableSlotID varSlot,
                                           const Operator quantifier,
                                           const Expression::Ptr &inClause,
                                           const Expression::Ptr &testExpression)
                                           : PairContainer(inClause, testExpression),
                                             m_varSlot(varSlot),
                                             m_quantifier(quantifier)
{
    Q_ASSERT(quantifier == Some || quantifier == Every);
}

Item QuantifiedExpression::mapToItem(const Item &item,
                                          const DynamicContext::Ptr &context) const
{
    context->setRangeVariable(m_varSlot, item);
    return item;
}

bool QuantifiedExpression::evaluateEBV(const DynamicContext::Ptr &context) const
{
    const Item::Iterator::Ptr it(makeItemMappingIterator<Item>(ConstPtr(this),
                                                               m_operand1->evaluateSequence(context),
                                                               context));

    Item item(it->next());

    if(m_quantifier == Some)
    {
        while(item)
        {
            if(m_operand2->evaluateEBV(context))
                return true;
            else
                item = it->next();
        };

        return false;
    }
    else
    {
        Q_ASSERT(m_quantifier == Every);

        while(item)
        {
            if(m_operand2->evaluateEBV(context))
                item = it->next();
            else
                return false;
        }

        return true;
    }
}

QString QuantifiedExpression::displayName(const Operator quantifier)
{
    if(quantifier == Some)
        return QLatin1String("some");
    else
    {
        Q_ASSERT(quantifier == Every);
        return QLatin1String("every");
    }
}

SequenceType::Ptr QuantifiedExpression::staticType() const
{
    return CommonSequenceTypes::ExactlyOneBoolean;
}

SequenceType::List QuantifiedExpression::expectedOperandTypes() const
{
    SequenceType::List result;
    result.append(CommonSequenceTypes::ZeroOrMoreItems);
    result.append(CommonSequenceTypes::EBV);
    return result;
}

QuantifiedExpression::Operator QuantifiedExpression::operatorID() const
{
    return m_quantifier;
}

ExpressionVisitorResult::Ptr QuantifiedExpression::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

QT_END_NAMESPACE
