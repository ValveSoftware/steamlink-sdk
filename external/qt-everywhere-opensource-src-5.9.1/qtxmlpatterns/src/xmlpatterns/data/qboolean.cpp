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

#include "qatomictype_p.h"
#include "qbuiltintypes_p.h"
#include "qcommonvalues_p.h"
#include "qdynamiccontext_p.h"
#include "qpatternistlocale_p.h"
#include "qvalidationerror_p.h"

#include "qboolean_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

bool Boolean::evaluateEBV(const Item::Iterator::Ptr &it,
                          const QExplicitlySharedDataPointer<DynamicContext> &context)
{
    return evaluateEBV(it->next(), it, context);
}

bool Boolean::evaluateEBV(const Item &first,
                          const Item::Iterator::Ptr &it,
                          const QExplicitlySharedDataPointer<DynamicContext> &context)
{
    Q_ASSERT(it);
    Q_ASSERT(context);

    if(!first)
        return false;
    else if(first.isNode())
        return true;

    const Item second(it->next());

    if(second)
    {
        Q_ASSERT(context);
        context->error(QtXmlPatterns::tr("Effective Boolean Value cannot be calculated for a sequence "
                                         "containing two or more atomic values."),
                          ReportContext::FORG0006,
                          QSourceLocation());
        return false;
    }
    else
        return first.as<AtomicValue>()->evaluateEBV(context);
}

bool Boolean::evaluateEBV(const Item &item,
                          const QExplicitlySharedDataPointer<DynamicContext> &context)
{
    if(!item)
        return false;
    else if(item.isNode())
        return true;
    else
        return item.as<AtomicValue>()->evaluateEBV(context);
}

Boolean::Boolean(const bool value) : m_value(value)
{
}

QString Boolean::stringValue() const
{
    return m_value
            ? CommonValues::TrueString->stringValue()
            : CommonValues::FalseString->stringValue();
}

bool Boolean::evaluateEBV(const QExplicitlySharedDataPointer<DynamicContext> &) const
{
    return m_value;
}

Boolean::Ptr Boolean::fromValue(const bool value)
{
    return value ? CommonValues::BooleanTrue : CommonValues::BooleanFalse;
}

AtomicValue::Ptr Boolean::fromLexical(const QString &lexical)
{
    const QString val(lexical.trimmed()); /* Apply the whitespace facet. */

    if(val == QLatin1String("true") || val == QChar(QLatin1Char('1')))
        return CommonValues::BooleanTrue;
    else if(val == QLatin1String("false") || val == QChar(QLatin1Char('0')))
        return CommonValues::BooleanFalse;
    else
        return ValidationError::createError();
}

ItemType::Ptr Boolean::type() const
{
    return BuiltinTypes::xsBoolean;
}

QT_END_NAMESPACE
