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

#include "qcommonsequencetypes_p.h"
#include "qcommonvalues_p.h"
#include "qgenericpredicate_p.h"
#include "qgenericsequencetype_p.h"
#include "qinsertioniterator_p.h"
#include "qpatternistlocale_p.h"

#include "qcardinalityverifier_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

QString CardinalityVerifier::wrongCardinality(const Cardinality &req,
                                              const Cardinality &got)
{
    return QtXmlPatterns::tr("Required cardinality is %1; got cardinality %2.")
                .arg(formatType(req), formatType(got));
}

Expression::Ptr CardinalityVerifier::verifyCardinality(const Expression::Ptr &operand,
                                                       const Cardinality &requiredCard,
                                                       const StaticContext::Ptr &context,
                                                       const ReportContext::ErrorCode code)
{
    const Cardinality opCard(operand->staticType()->cardinality());

    if(requiredCard.isMatch(opCard))
        return operand;
    else if(requiredCard.canMatch(opCard))
        return Expression::Ptr(new CardinalityVerifier(operand, requiredCard, code));
    else if(context->compatModeEnabled() &&
            !opCard.isEmpty())
    {
        return GenericPredicate::createFirstItem(operand);
    }
    else
    {
        /* Sequences within this cardinality can never match. */
        context->error(wrongCardinality(requiredCard, opCard), code, operand.data());
        return operand;
    }
}

CardinalityVerifier::CardinalityVerifier(const Expression::Ptr &operand,
                                         const Cardinality &card,
                                         const ReportContext::ErrorCode code)
                                         : SingleContainer(operand),
                                           m_reqCard(card),
                                           m_allowsMany(operand->staticType()->cardinality().allowsMany()),
                                           m_errorCode(code)
{
    Q_ASSERT_X(m_reqCard != Cardinality::zeroOrMore(), Q_FUNC_INFO,
               "It makes no sense to use CardinalityVerifier for cardinality zero-or-more.");
}

Item::Iterator::Ptr CardinalityVerifier::evaluateSequence(const DynamicContext::Ptr &context) const
{
    const Item::Iterator::Ptr it(m_operand->evaluateSequence(context));
    const Item next(it->next());

    if(next)
    {
        const Item next2(it->next());

        if(next2)
        {
            if(m_reqCard.allowsMany())
            {
                Item::List start;
                start.append(next);
                start.append(next2);

                return Item::Iterator::Ptr(new InsertionIterator(it, 1, makeListIterator(start)));
            }
            else
            {
                context->error(wrongCardinality(m_reqCard, Cardinality::twoOrMore()), m_errorCode, this);
                return CommonValues::emptyIterator;
            }
        }
        else
        {
            /* We might be instantiated for the empty sequence. */
            if(m_reqCard.isEmpty())
            {
                context->error(wrongCardinality(m_reqCard, Cardinality::twoOrMore()), m_errorCode, this);
                return CommonValues::emptyIterator;
            }
            else
                return makeSingletonIterator(next);
        }
    }
    else
    {
        if(m_reqCard.allowsEmpty())
            return CommonValues::emptyIterator;
        else
        {
            context->error(wrongCardinality(m_reqCard, Cardinality::twoOrMore()), m_errorCode, this);
            return CommonValues::emptyIterator;
        }
    }
}

Item CardinalityVerifier::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    if(m_allowsMany)
    {
        const Item::Iterator::Ptr it(m_operand->evaluateSequence(context));
        const Item item(it->next());

        if(item)
        {
            if(it->next())
            {
                context->error(wrongCardinality(m_reqCard, Cardinality::twoOrMore()),
                               m_errorCode, this);
                return Item();
            }
            else
                return item;
        }
        else if(m_reqCard.allowsEmpty())
            return Item();
        else
        {
            context->error(wrongCardinality(m_reqCard), m_errorCode, this);
            return Item();
        }
    }
    else
    {
        const Item item(m_operand->evaluateSingleton(context));

        if(item)
            return item;
        else if(m_reqCard.allowsEmpty())
            return Item();
        else
        {
            context->error(wrongCardinality(m_reqCard), m_errorCode, this);
            return Item();
        }
    }
}

const SourceLocationReflection *CardinalityVerifier::actualReflection() const
{
    return m_operand->actualReflection();
}

Expression::Ptr CardinalityVerifier::compress(const StaticContext::Ptr &context)
{
    if(m_reqCard.isMatch(m_operand->staticType()->cardinality()))
        return m_operand->compress(context);
    else
        return SingleContainer::compress(context);
}

SequenceType::List CardinalityVerifier::expectedOperandTypes() const
{
    SequenceType::List result;
    result.append(CommonSequenceTypes::ZeroOrMoreItems);
    return result;
}

SequenceType::Ptr CardinalityVerifier::staticType() const
{
    return makeGenericSequenceType(m_operand->staticType()->itemType(), m_reqCard);
}

ExpressionVisitorResult::Ptr CardinalityVerifier::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

Expression::ID CardinalityVerifier::id() const
{
    return IDCardinalityVerifier;
}

QT_END_NAMESPACE
