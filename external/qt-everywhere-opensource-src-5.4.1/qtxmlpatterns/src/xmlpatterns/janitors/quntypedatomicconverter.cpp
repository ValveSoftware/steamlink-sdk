/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qitem_p.h"
#include "qcommonsequencetypes_p.h"
#include "qgenericsequencetype_p.h"
#include "qitemmappingiterator_p.h"

#include "quntypedatomicconverter_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

UntypedAtomicConverter::UntypedAtomicConverter(const Expression::Ptr &operand,
                                               const ItemType::Ptr &reqType,
                                               const ReportContext::ErrorCode code) : SingleContainer(operand)
                                                                                    , CastingPlatform<UntypedAtomicConverter, true>(code)
                                                                                    , m_reqType(reqType)
{
    Q_ASSERT(reqType);
}

Item::Iterator::Ptr UntypedAtomicConverter::evaluateSequence(const DynamicContext::Ptr &context) const
{
    return makeItemMappingIterator<Item>(ConstPtr(this),
                                         m_operand->evaluateSequence(context),
                                         context);
}

Item UntypedAtomicConverter::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item item(m_operand->evaluateSingleton(context));

    if(item)
        return cast(item, context);
    else /* Empty is allowed. UntypedAtomicConverter doesn't care about cardinality. */
        return Item();
}

Expression::Ptr UntypedAtomicConverter::typeCheck(const StaticContext::Ptr &context,
                                                  const SequenceType::Ptr &reqType)
{
    const Expression::Ptr me(SingleContainer::typeCheck(context, reqType));

    /* Let the CastingPlatform look up its AtomicCaster. */
    prepareCasting(context, m_operand->staticType()->itemType());

    return me;
}

SequenceType::List UntypedAtomicConverter::expectedOperandTypes() const
{
    SequenceType::List result;
    result.append(CommonSequenceTypes::ZeroOrMoreAtomicTypes);
    return result;
}

SequenceType::Ptr UntypedAtomicConverter::staticType() const
{
    return makeGenericSequenceType(m_reqType,
                                   m_operand->staticType()->cardinality());
}

ExpressionVisitorResult::Ptr UntypedAtomicConverter::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

const SourceLocationReflection *UntypedAtomicConverter::actualReflection() const
{
    return m_operand.data();
}

QT_END_NAMESPACE
