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

#include "qboolean_p.h"
#include "qcommonsequencetypes_p.h"
#include "qcommonvalues_p.h"
#include "qliteral_p.h"

#include "qinstanceof_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

InstanceOf::InstanceOf(const Expression::Ptr &operand,
                       const SequenceType::Ptr &tType) : SingleContainer(operand)
                                                       , m_targetType(tType)
{
    Q_ASSERT(m_targetType);
}

bool InstanceOf::evaluateEBV(const DynamicContext::Ptr &context) const
{
    const Item::Iterator::Ptr it(m_operand->evaluateSequence(context));
    Item item(it->next());
    unsigned int count = 1;

    if(!item)
        return m_targetType->cardinality().allowsEmpty();

    do
    {
        if(!m_targetType->itemType()->itemMatches(item))
            return false;

        if(count == 2 && !m_targetType->cardinality().allowsMany())
            return false;

        item = it->next();
        ++count;
    } while(item);

    return true;
}

Expression::Ptr InstanceOf::compress(const StaticContext::Ptr &context)
{
    const Expression::Ptr me(SingleContainer::compress(context));

    if(me != this || m_operand->has(DisableTypingDeduction))
        return me;

    const SequenceType::Ptr opType(m_operand->staticType());
    const ItemType::Ptr targetType(m_targetType->itemType());
    const ItemType::Ptr operandType(opType->itemType());

    if(m_targetType->cardinality().isMatch(opType->cardinality()))
    {
        if(*operandType == *CommonSequenceTypes::Empty || targetType->xdtTypeMatches(operandType))
            return wrapLiteral(CommonValues::BooleanTrue, context, this);
        else if(!operandType->xdtTypeMatches(targetType))
            return wrapLiteral(CommonValues::BooleanFalse, context, this);
    }
    /* Optimization: rule out the case where instance of will always fail. */

    return me;
}

SequenceType::Ptr InstanceOf::targetType() const
{
    return m_targetType;
}

SequenceType::Ptr InstanceOf::staticType() const
{
    return CommonSequenceTypes::ExactlyOneBoolean;
}

SequenceType::List InstanceOf::expectedOperandTypes() const
{
    SequenceType::List result;
    result.append(CommonSequenceTypes::ZeroOrMoreItems);
    return result;
}

ExpressionVisitorResult::Ptr InstanceOf::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

QT_END_NAMESPACE
