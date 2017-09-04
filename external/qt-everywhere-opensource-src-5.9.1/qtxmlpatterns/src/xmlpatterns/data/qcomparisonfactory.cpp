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

#include "qatomiccomparators_p.h"
#include "qatomicstring_p.h"
#include "qcomparisonplatform_p.h"
#include "qvaluefactory_p.h"

#include "qcomparisonfactory_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

/**
 * @short Helper class for ComparisonFactory::fromLexical() which exposes
 * CastingPlatform appropriately.
 *
 * @relates ComparisonFactory
 */
class PerformComparison : public ComparisonPlatform<PerformComparison, true>
                        , public SourceLocationReflection
{
public:
    PerformComparison(const SourceLocationReflection *const sourceLocationReflection,
                      const AtomicComparator::Operator op) : m_sourceReflection(sourceLocationReflection)
                                                           , m_operator(op)
    {
        Q_ASSERT(m_sourceReflection);
    }

    bool operator()(const AtomicValue::Ptr &operand1,
                    const AtomicValue::Ptr &operand2,
                    const SchemaType::Ptr &type,
                    const ReportContext::Ptr &context)
    {
        const ItemType::Ptr asItemType((AtomicType::Ptr(type)));

        /* One area where the Query Transform world differs from the Schema
         * world is that @c xs:duration is not considedered comparable, because
         * it's according to Schema is partially comparable. This means
         * ComparisonPlatform::fetchComparator() flags it as impossible, and
         * hence we need to override that.
         *
         * SchemaType::wxsTypeMatches() will return true for sub-types of @c
         * xs:duration as well, but that's ok since AbstractDurationComparator
         * works for them too. */
        if(BuiltinTypes::xsDuration->wxsTypeMatches(type))
            prepareComparison(AtomicComparator::Ptr(new AbstractDurationComparator()));
        else if (BuiltinTypes::xsGYear->wxsTypeMatches(type) ||
                 BuiltinTypes::xsGYearMonth->wxsTypeMatches(type) ||
                 BuiltinTypes::xsGMonth->wxsTypeMatches(type) ||
                 BuiltinTypes::xsGMonthDay->wxsTypeMatches(type) ||
                 BuiltinTypes::xsGDay->wxsTypeMatches(type))
            prepareComparison(AtomicComparator::Ptr(new AbstractDateTimeComparator()));
        else
            prepareComparison(fetchComparator(asItemType, asItemType, context));

        return flexibleCompare(operand1, operand2, context);
    }

    const SourceLocationReflection *actualReflection() const
    {
        return m_sourceReflection;
    }

    AtomicComparator::Operator operatorID() const
    {
        return m_operator;
    }

private:
    const SourceLocationReflection *const m_sourceReflection;
    const AtomicComparator::Operator      m_operator;
};

bool ComparisonFactory::compare(const AtomicValue::Ptr &operand1,
                                const AtomicComparator::Operator op,
                                const AtomicValue::Ptr &operand2,
                                const SchemaType::Ptr &type,
                                const ReportContext::Ptr &context,
                                const SourceLocationReflection *const sourceLocationReflection)
{
    Q_ASSERT(operand1);
    Q_ASSERT(operand2);
    Q_ASSERT(context);
    Q_ASSERT(sourceLocationReflection);
    Q_ASSERT(type);
    Q_ASSERT_X(type->category() == SchemaType::SimpleTypeAtomic, Q_FUNC_INFO,
               "We can only compare atomic values.");

    return PerformComparison(sourceLocationReflection, op)(operand1, operand2, type, context);
}

bool ComparisonFactory::constructAndCompare(const DerivedString<TypeString>::Ptr &operand1,
                                            const AtomicComparator::Operator op,
                                            const DerivedString<TypeString>::Ptr &operand2,
                                            const SchemaType::Ptr &type,
                                            const ReportContext::Ptr &context,
                                            const SourceLocationReflection *const sourceLocationReflection)
{
    Q_ASSERT(operand1);
    Q_ASSERT(operand2);
    Q_ASSERT(context);
    Q_ASSERT(sourceLocationReflection);
    Q_ASSERT(type);
    Q_ASSERT_X(type->category() == SchemaType::SimpleTypeAtomic, Q_FUNC_INFO,
               "We can only compare atomic values.");

    const AtomicValue::Ptr value1 = ValueFactory::fromLexical(operand1->stringValue(), type, context, sourceLocationReflection);
    const AtomicValue::Ptr value2 = ValueFactory::fromLexical(operand2->stringValue(), type, context, sourceLocationReflection);

    return compare(value1, op, value2, type, context, sourceLocationReflection);
}

QT_END_NAMESPACE
