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

#include "qcommonnamespaces_p.h"

#include "qcommonsequencetypes_p.h"
#include "qfunctionfactory_p.h"
#include "qgeneralcomparison_p.h"
#include "qliteral_p.h"
#include "qschemanumeric_p.h"
#include "qvaluecomparison_p.h"

#include "qoptimizerblocks_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

ByIDIdentifier::ByIDIdentifier(const Expression::ID id) : m_id(id)
{
}

bool ByIDIdentifier::matches(const Expression::Ptr &expr) const
{
    return expr->is(m_id);
}

ComparisonIdentifier::ComparisonIdentifier(const QVector<Expression::ID> hosts,
                                           const AtomicComparator::Operator op) : m_hosts(hosts),
                                                                                  m_op(op)
{
}

bool ComparisonIdentifier::matches(const Expression::Ptr &e) const
{
    const Expression::ID eID = e->id();

    if(eID == Expression::IDGeneralComparison)
    {
        if(m_hosts.contains(Expression::IDGeneralComparison))
            return e->as<GeneralComparison>()->operatorID() == m_op;
        else
            return false;
    }
    else if(eID == Expression::IDValueComparison)
    {
        if(m_hosts.contains(Expression::IDValueComparison))
            return e->as<ValueComparison>()->operatorID() == m_op;
        else
            return false;
    }
    else
        return false;
}

BySequenceTypeIdentifier::BySequenceTypeIdentifier(const SequenceType::Ptr &seqType) : m_seqType(seqType)
{
    Q_ASSERT(seqType);
}

bool BySequenceTypeIdentifier::matches(const Expression::Ptr &expr) const
{
    const SequenceType::Ptr t(expr->staticType());

    return m_seqType->itemType()->xdtTypeMatches(t->itemType())
           &&
           m_seqType->cardinality().isMatch(t->cardinality());
}

IntegerIdentifier::IntegerIdentifier(const xsInteger num) : m_num(num)
{
}

bool IntegerIdentifier::matches(const Expression::Ptr &expr) const
{
    return expr->is(Expression::IDIntegerValue) &&
           expr->as<Literal>()->item().as<Numeric>()->toInteger() == m_num;
}

BooleanIdentifier::BooleanIdentifier(const bool value) : m_value(value)
{
}

bool BooleanIdentifier::matches(const Expression::Ptr &expr) const
{
    return expr->is(Expression::IDBooleanValue) &&
           expr->evaluateEBV(DynamicContext::Ptr()) == m_value;
}

ByIDCreator::ByIDCreator(const Expression::ID id) : m_id(id)
{
    Q_ASSERT(id != Expression::IDIgnorableExpression);
}

Expression::Ptr ByIDCreator::create(const Expression::List &operands,
                                    const StaticContext::Ptr &context,
                                    const SourceLocationReflection *const r) const
{
    return create(m_id, operands, context, r);
}

Expression::Ptr ByIDCreator::create(const Expression::ID id,
                                    const Expression::List &operands,
                                    const StaticContext::Ptr &context,
                                    const SourceLocationReflection *const r)
{
    Q_ASSERT(context);

    QXmlName::LocalNameCode fnName;

    switch(id)
    {
        case Expression::IDExistsFN:
        {
            fnName = StandardLocalNames::exists;
            break;
        }
        case Expression::IDEmptyFN:
        {
            fnName = StandardLocalNames::empty;
            break;
        }
        default:
        {
            Q_ASSERT_X(false, Q_FUNC_INFO,
                       "Cannot create an expression of requested type; m_id is wrong.");
            return Expression::Ptr();
        }
    }

    /* The reason we don't simply do 'new ExistsFN()' ourselves, is that all FunctionCall
     * instances needs their FunctionSignature in order to function, and the FunctionFactories
     * sets that. */
    const QXmlName qName(StandardNamespaces::fn, fnName);

    const Expression::Ptr result(context->functionSignatures()->createFunctionCall(qName, operands, context, r));
    context->wrapExpressionWith(r, result);
    return result;
}

QT_END_NAMESPACE
