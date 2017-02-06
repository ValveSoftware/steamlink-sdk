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

#include "qitemmappingiterator_p.h"
#include "qsequencemappingiterator_p.h"

#include "qargumentconverter_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

ArgumentConverter::ArgumentConverter(const Expression::Ptr &operand,
                                     const ItemType::Ptr &reqType) : UntypedAtomicConverter(operand, reqType)
{
}

ExpressionVisitorResult::Ptr ArgumentConverter::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

Item::Iterator::Ptr ArgumentConverter::mapToSequence(const Item &item,
                                                     const DynamicContext::Ptr &context) const
{
    if(item.isAtomicValue() && !BuiltinTypes::xsUntypedAtomic->xdtTypeMatches(item.type()))
        return makeSingletonIterator(item);
    else
    {
        /* We're using UntypedAtomicConverter::mapToItem(). */
        return makeItemMappingIterator<Item>(ConstPtr(this),
                                             item.sequencedTypedValue(),
                                             context);
    }
}

Item::Iterator::Ptr ArgumentConverter::evaluateSequence(const DynamicContext::Ptr &context) const
{
    return makeSequenceMappingIterator<Item>(ConstPtr(this),
                                             m_operand->evaluateSequence(context),
                                             context);
}

Item ArgumentConverter::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item item(m_operand->evaluateSingleton(context));

    if(item)
        return mapToItem(item, context);
    else /* Empty is allowed. ArgumentConverter doesn't care about cardinality. */
        return Item();
}

SequenceType::List ArgumentConverter::expectedOperandTypes() const
{
    SequenceType::List result;
    result.append(CommonSequenceTypes::ZeroOrMoreItems);
    return result;
}

SequenceType::Ptr ArgumentConverter::staticType() const
{
    return CommonSequenceTypes::ZeroOrMoreAtomicTypes;
}

QT_END_NAMESPACE
