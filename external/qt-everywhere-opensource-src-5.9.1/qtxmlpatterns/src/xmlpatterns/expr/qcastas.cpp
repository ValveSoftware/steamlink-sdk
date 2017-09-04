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
#include "qbuiltintypes_p.h"
#include "qcardinalityverifier_p.h"
#include "qcommonsequencetypes_p.h"
#include "qemptysequence_p.h"
#include "qgenericsequencetype_p.h"
#include "qliteral_p.h"
#include "qpatternistlocale_p.h"
#include "qnamespaceresolver_p.h"
#include "qqnameconstructor_p.h"
#include "qqnamevalue_p.h"
#include "qatomicstring_p.h"
#include "qvalidationerror_p.h"

#include "qcastas_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

CastAs::CastAs(const Expression::Ptr &source,
               const SequenceType::Ptr &tType) : SingleContainer(source),
                                                 m_targetType(tType)
{
    Q_ASSERT(source);
    Q_ASSERT(tType);
    Q_ASSERT(!tType->cardinality().allowsMany());
    Q_ASSERT(tType->itemType()->isAtomicType());
}

Item CastAs::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    Q_ASSERT(context);
    const Item val(m_operand->evaluateSingleton(context));

    if(val)
        return cast(val, context);
    else
    {
        /* No item supplied, let's handle the cardinality part. */

        if(m_targetType->cardinality().allowsEmpty())
            return Item();
        else
        {
            Q_ASSERT(context);
            context->error(QtXmlPatterns::tr("Type error in cast, expected %1, "
                                             "received %2.")
                           .arg(formatType(Cardinality::exactlyOne()))
                           .arg(formatType(Cardinality::empty())),
                           ReportContext::XPTY0004, this);
            return Item();
        }
    }
}

Expression::Ptr CastAs::typeCheck(const StaticContext::Ptr &context,
                                  const SequenceType::Ptr &reqType)
{
    checkTargetType(context);
    const SequenceType::Ptr seqt(m_operand->staticType());
    ItemType::Ptr t(seqt->itemType());

    /* Special case xs:QName */
    if(BuiltinTypes::xsQName->xdtTypeMatches(m_targetType->itemType()))
    {
        /* Ok, We're casting to xs:QName. */
        if(m_operand->is(IDStringValue)) /* A valid combination, let's do the cast. */
            return castToQName(context)->typeCheck(context, reqType);
        else if(BuiltinTypes::xsQName->xdtTypeMatches(t))
            return m_operand->typeCheck(context, reqType);
        else if(seqt->cardinality().isEmpty() && m_targetType->cardinality().allowsEmpty())
            return EmptySequence::create(this, context);
        else if(!(seqt->cardinality().isEmpty() && !m_targetType->cardinality().allowsEmpty()))
        {
            context->error(QtXmlPatterns::tr("When casting to %1 or types "
                                             "derived from it, the source "
                                             "value must be of the same type, "
                                             "or it must be a string literal. "
                                             "Type %2 is not allowed.")
                           .arg(formatType(context->namePool(), BuiltinTypes::xsQName))
                           .arg(formatType(context->namePool(), seqt)),
                           ReportContext::XPTY0004, this);
            return Expression::Ptr(this);
        }
    }

    const Expression::Ptr me(SingleContainer::typeCheck(context, reqType));
    /* Type may have changed, such as that atomization has been applied. */
    t = m_operand->staticType()->itemType();

    if(m_targetType->itemType()->xdtTypeMatches(t) &&
       !BuiltinTypes::xsDayTimeDuration->xdtTypeMatches(t) &&
       !BuiltinTypes::xsYearMonthDuration->xdtTypeMatches(t))
    { /* At least casting is superflorous. */
        if(m_operand->staticType()->cardinality().isMatch(m_targetType->cardinality()))
            return m_operand; /* The whole cast expression is redundant. */
        else
        { /* Only cardinality check is needed, rewrite to CardinalityVerifier. */
            return Expression::Ptr(new CardinalityVerifier(m_operand,
                                                           m_targetType->cardinality(),
                                                           ReportContext::FORG0001));
        }
    }

    /* Let the CastingPlatform look up its AtomicCaster. */
    prepareCasting(context, t);

    return me;
}

Expression::Ptr CastAs::compress(const StaticContext::Ptr &context)
{
    /* Simplify casts to itself. */
    if(*m_targetType->itemType() == *m_operand->staticType()->itemType())
        return m_operand->compress(context);
    else
        return SingleContainer::compress(context);
}

Expression::Ptr CastAs::castToQName(const StaticContext::Ptr &context) const
{
    /* Apply the whitespace facet by calling trimmed(). */
    /* We can assume m_operand is an Expression because this is a requirement
     * for casting to xs:QName. */
    const QString lexQName(m_operand->as<Literal>()->item().as<AtomicValue>()->stringValue().trimmed());

    const QXmlName
        expName(QNameConstructor::expandQName<StaticContext::Ptr,
                                              ReportContext::FORG0001,
                                              ReportContext::FONS0004>(lexQName,
                                                                       context,
                                                                       context->namespaceBindings(), this));
    return wrapLiteral(toItem(QNameValue::fromValue(context->namePool(), expName)), context, this);
}

SequenceType::Ptr CastAs::staticType() const
{
    if(m_operand->staticType()->cardinality().allowsEmpty())
        return m_targetType;
    else
        return makeGenericSequenceType(m_targetType->itemType(),
                                       Cardinality::exactlyOne());
}

SequenceType::List CastAs::expectedOperandTypes() const
{
    SequenceType::List result;

    if(m_targetType->cardinality().allowsEmpty())
        result.append(CommonSequenceTypes::ZeroOrOneAtomicType);
    else
        result.append(CommonSequenceTypes::ExactlyOneAtomicType);

    return result;
}

ExpressionVisitorResult::Ptr CastAs::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

QT_END_NAMESPACE
