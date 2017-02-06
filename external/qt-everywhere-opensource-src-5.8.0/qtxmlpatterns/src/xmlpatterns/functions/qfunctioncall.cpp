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


#include "qcontextitem_p.h"
#include "qcommonsequencetypes_p.h"
#include "qemptysequence_p.h"
#include "qfunctionsignature_p.h"
#include "qgenericsequencetype_p.h"
#include "qcollationchecker_p.h"
#include "qcommonnamespaces_p.h"

#include "qfunctioncall_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

SequenceType::List FunctionCall::expectedOperandTypes() const
{
    const FunctionArgument::List args(signature()->arguments());
    FunctionArgument::List::const_iterator it(args.constBegin());
    const FunctionArgument::List::const_iterator end(args.constEnd());
    // TODO reserve/resize()
    SequenceType::List result;

    for(; it != end; ++it)
        result.append((*it)->type());

    return result;
}

Expression::Ptr FunctionCall::typeCheck(const StaticContext::Ptr &context,
                                        const SequenceType::Ptr &reqType)
{
    /* We don't cache properties() at some stages because it can be invalidated
     * by the typeCheck(). */

    const FunctionSignature::Arity maxArgs = signature()->maximumArguments();
    /* We do this before the typeCheck() such that the appropriate conversions
     * are applied to the ContextItem. */
    if(m_operands.count() < maxArgs &&
       has(UseContextItem))
    {
        m_operands.append(Expression::Ptr(new ContextItem()));
        context->wrapExpressionWith(this, m_operands.last());
    }

    const Expression::Ptr me(UnlimitedContainer::typeCheck(context, reqType));
    if(me != this)
        return me;

    const Properties props(properties());

    if(props.testFlag(RewriteToEmptyOnEmpty) &&
       *CommonSequenceTypes::Empty == *m_operands.first()->staticType()->itemType())
    {
        return EmptySequence::create(this, context);
    }

    if(props.testFlag(LastOperandIsCollation) &&
       m_operands.count() == maxArgs)
    {
        m_operands.last() = Expression::Ptr(new CollationChecker(m_operands.last()));
        context->wrapExpressionWith(this, m_operands.last());
    }

    return me;
}

void FunctionCall::setSignature(const FunctionSignature::Ptr &sign)
{
    m_signature = sign;
}

FunctionSignature::Ptr FunctionCall::signature() const
{
    Q_ASSERT(m_signature); /* It really should be set. */
    return m_signature;
}

SequenceType::Ptr FunctionCall::staticType() const
{
    Q_ASSERT(m_signature);
    if(has(EmptynessFollowsChild))
    {
        if(m_operands.isEmpty())
        {
            /* This is a function which uses the context item when having no arguments. */
            return signature()->returnType();
        }
        const Cardinality card(m_operands.first()->staticType()->cardinality());
        if(card.allowsEmpty())
            return signature()->returnType();
        else
        {
            /* Remove empty. */
            return makeGenericSequenceType(signature()->returnType()->itemType(),
                                           card & Cardinality::oneOrMore());
        }
    }
    return signature()->returnType();
}

Expression::Properties FunctionCall::properties() const
{
    Q_ASSERT(m_signature);
    return signature()->properties();
}

ExpressionVisitorResult::Ptr FunctionCall::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

Expression::ID FunctionCall::id() const
{
    Q_ASSERT(m_signature);
    return m_signature->id();
}

QT_END_NAMESPACE
