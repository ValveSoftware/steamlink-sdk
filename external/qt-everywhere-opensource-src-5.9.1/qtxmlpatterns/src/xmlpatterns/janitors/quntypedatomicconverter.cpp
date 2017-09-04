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
