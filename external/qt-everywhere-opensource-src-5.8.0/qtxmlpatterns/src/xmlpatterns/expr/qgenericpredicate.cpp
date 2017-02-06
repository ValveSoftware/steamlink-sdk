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

#include "qabstractfloat_p.h"
#include "qboolean_p.h"
#include "qbuiltintypes_p.h"
#include "qcommonsequencetypes_p.h"
#include "qemptysequence_p.h"
#include "qfirstitempredicate_p.h"
#include "qgenericsequencetype_p.h"
#include "qitemmappingiterator_p.h"
#include "qliteral_p.h"
#include "qpatternistlocale_p.h"
#include "qtruthpredicate_p.h"

#include "qgenericpredicate_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

GenericPredicate::GenericPredicate(const Expression::Ptr &sourceExpression,
                                   const Expression::Ptr &predicate) : PairContainer(sourceExpression,
                                                                                     predicate)
{
}

Expression::Ptr GenericPredicate::create(const Expression::Ptr &sourceExpression,
                                         const Expression::Ptr &predicateExpression,
                                         const StaticContext::Ptr &context,
                                         const QSourceLocation &location)
{
    Q_ASSERT(sourceExpression);
    Q_ASSERT(predicateExpression);
    Q_ASSERT(context);
    const ItemType::Ptr type(predicateExpression->staticType()->itemType());

    if(predicateExpression->is(IDIntegerValue) &&
       predicateExpression->as<Literal>()->item().as<Numeric>()->toInteger() == 1)
    { /* Handle [1] */
        return createFirstItem(sourceExpression);
    }
    else if(BuiltinTypes::numeric->xdtTypeMatches(type))
    { /* A numeric predicate, other than [1]. */
        /* TODO at somepoint we'll return a specialized expr here, NumericPredicate or so.
         * Dependency analysis is a bit tricky, since the contained expression can depend on
         * some loop component. */
        return Expression::Ptr(new GenericPredicate(sourceExpression, predicateExpression));
    }
    else if(*CommonSequenceTypes::Empty == *type)
    {
        return EmptySequence::create(predicateExpression.data(), context);
    }
    else if(*BuiltinTypes::item == *type ||
            *BuiltinTypes::xsAnyAtomicType == *type)
    {
        /* The type couldn't be narrowed at compile time, so we use
         * a generic predicate. This check is before the CommonSequenceTypes::EBV check,
         * because the latter matches these types as well. */
        return Expression::Ptr(new GenericPredicate(sourceExpression, predicateExpression));
    }
    else if(CommonSequenceTypes::EBV->itemType()->xdtTypeMatches(type))
    {
        return Expression::Ptr(new TruthPredicate(sourceExpression, predicateExpression));
    }
    else
    {
        context->error(QtXmlPatterns::tr("A value of type %1 cannot be a "
                                         "predicate. A predicate must have "
                                         "either a numeric type or an "
                                         "Effective Boolean Value type.")
                       .arg(formatType(context->namePool(),
                                       sourceExpression->staticType())),
                       ReportContext::FORG0006, location);
        return Expression::Ptr(); /* Silence compiler warning. */
    }
}

Expression::Ptr GenericPredicate::createFirstItem(const Expression::Ptr &sourceExpression)

{
    return Expression::Ptr(new FirstItemPredicate(sourceExpression));
}

Item GenericPredicate::mapToItem(const Item &item,
                                 const DynamicContext::Ptr &context) const
{
    const Item::Iterator::Ptr it(m_operand2->evaluateSequence(context));
    const Item pcateItem(it->next());

    if(!pcateItem)
        return Item(); /* The predicate evaluated to the empty sequence */
    else if(pcateItem.isNode())
        return item;
    /* Ok, now it must be an AtomicValue */
    else if(BuiltinTypes::numeric->xdtTypeMatches(pcateItem.type()))
    { /* It's a positional predicate. */
        if(it->next())
        {
            context->error(QtXmlPatterns::tr("A positional predicate must "
                                             "evaluate to a single numeric "
                                             "value."),
                              ReportContext::FORG0006, this);
            return Item();
        }

        if(Double::isEqual(static_cast<xsDouble>(context->contextPosition()),
                           pcateItem.as<Numeric>()->toDouble()))
        {
            return item;
        }
        else
            return Item();
    }
    else if(Boolean::evaluateEBV(pcateItem, it, context)) /* It's a truth predicate. */
        return item;
    else
        return Item();
}

Item::Iterator::Ptr GenericPredicate::evaluateSequence(const DynamicContext::Ptr &context) const
{
    const Item::Iterator::Ptr focus(m_operand1->evaluateSequence(context));
    const DynamicContext::Ptr newContext(context->createFocus());
    newContext->setFocusIterator(focus);

    return makeItemMappingIterator<Item>(ConstPtr(this),
                                         focus,
                                         newContext);
}

Item GenericPredicate::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item::Iterator::Ptr focus(m_operand1->evaluateSequence(context));
    const DynamicContext::Ptr newContext(context->createFocus());
    newContext->setFocusIterator(focus);
    return mapToItem(focus->next(), newContext);
}

SequenceType::List GenericPredicate::expectedOperandTypes() const
{
    SequenceType::List result;
    result.append(CommonSequenceTypes::ZeroOrMoreItems);
    result.append(CommonSequenceTypes::ZeroOrMoreItems);
    return result;
}

SequenceType::Ptr GenericPredicate::staticType() const
{
    const SequenceType::Ptr type(m_operand1->staticType());
    return makeGenericSequenceType(type->itemType(),
                                   type->cardinality() | Cardinality::zeroOrOne());
}

ExpressionVisitorResult::Ptr GenericPredicate::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

ItemType::Ptr GenericPredicate::newFocusType() const
{
    return m_operand1->staticType()->itemType();
}

Expression::Properties GenericPredicate::properties() const
{
    return CreatesFocusForLast;
}

QString GenericPredicate::description() const
{
    return QLatin1String("predicate");
}

Expression::ID GenericPredicate::id() const
{
    return IDGenericPredicate;
}

QT_END_NAMESPACE
