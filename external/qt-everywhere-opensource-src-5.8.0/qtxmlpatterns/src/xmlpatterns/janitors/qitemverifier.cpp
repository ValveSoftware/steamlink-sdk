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
#include "qpatternistlocale_p.h"

#include "qitemverifier_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

ItemVerifier::ItemVerifier(const Expression::Ptr &operand,
                           const ItemType::Ptr &reqType,
                           const ReportContext::ErrorCode errorCode) : SingleContainer(operand),
                                                                       m_reqType(reqType),
                                                                       m_errorCode(errorCode)
{
    Q_ASSERT(reqType);
}

void ItemVerifier::verifyItem(const Item &item, const DynamicContext::Ptr &context) const
{
    if(m_reqType->itemMatches(item))
        return;

    context->error(QtXmlPatterns::tr("The item %1 did not match the required type %2.")
                                    .arg(formatData(item.stringValue()),
                                         formatType(context->namePool(), m_reqType)),
                   m_errorCode,
                   this);
}

const SourceLocationReflection *ItemVerifier::actualReflection() const
{
    return m_operand->actualReflection();
}

Item ItemVerifier::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item item(m_operand->evaluateSingleton(context));

    if(item)
    {
        verifyItem(item, context);
        return item;
    }
    else
        return Item();
}

Item ItemVerifier::mapToItem(const Item &item, const DynamicContext::Ptr &context) const
{
    verifyItem(item, context);
    return item;
}

Item::Iterator::Ptr ItemVerifier::evaluateSequence(const DynamicContext::Ptr &context) const
{
    return makeItemMappingIterator<Item>(ConstPtr(this),
                                         m_operand->evaluateSequence(context),
                                         context);
}

SequenceType::Ptr ItemVerifier::staticType() const
{
    return makeGenericSequenceType(m_reqType, m_operand->staticType()->cardinality());
}

SequenceType::List ItemVerifier::expectedOperandTypes() const
{
    SequenceType::List result;
    result.append(CommonSequenceTypes::ZeroOrMoreItems);
    return result;
}

ExpressionVisitorResult::Ptr ItemVerifier::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

QT_END_NAMESPACE
