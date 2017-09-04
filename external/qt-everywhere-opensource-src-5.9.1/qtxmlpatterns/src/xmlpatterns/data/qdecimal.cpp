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

#include <math.h>

#include <private/qlocale_tools_p.h>

#include "qabstractfloat_p.h"
#include "qatomictype_p.h"
#include "qbuiltintypes_p.h"
#include "qvalidationerror_p.h"

#include "qdecimal_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

Decimal::Decimal(const xsDecimal num) : m_value(num)
{
}

Decimal::Ptr Decimal::fromValue(const xsDecimal num)
{
    return Decimal::Ptr(new Decimal(num));
}

AtomicValue::Ptr Decimal::fromLexical(const QString &strNumeric)
{
    /* QString::toDouble() handles the whitespace facet. */
    const QString strNumericTrimmed(strNumeric.trimmed());

    /* Block these out, as QString::toDouble() supports them. */
    if(strNumericTrimmed.compare(QLatin1String("-INF"), Qt::CaseInsensitive) == 0
       || strNumericTrimmed.compare(QLatin1String("INF"), Qt::CaseInsensitive)  == 0
       || strNumericTrimmed.compare(QLatin1String("+INF"), Qt::CaseInsensitive)  == 0
       || strNumericTrimmed.compare(QLatin1String("nan"), Qt::CaseInsensitive)  == 0
       || strNumericTrimmed.contains(QLatin1Char('e'))
       || strNumericTrimmed.contains(QLatin1Char('E')))
    {
        return ValidationError::createError();
    }

    bool conversionOk = false;
    const xsDecimal num = strNumericTrimmed.toDouble(&conversionOk);

    if(conversionOk)
        return AtomicValue::Ptr(new Decimal(num));
    else
        return ValidationError::createError();
}

bool Decimal::evaluateEBV(const QExplicitlySharedDataPointer<DynamicContext> &) const
{
    return !Double::isEqual(m_value, 0.0);
}

QString Decimal::stringValue() const
{
    return toString(m_value);
}

QString Decimal::toString(const xsDecimal value)
{
    /*
     * If SV is in the value space of xs:integer, that is, if there are no
     * significant digits after the decimal point, then the value is converted
     * from an xs:decimal to an xs:integer and the resulting xs:integer is
     * converted to an xs:string using the rule above.
     */
    if(Double::isEqual(::floor(value), value))
    {
        /* The static_cast is identical to Integer::toInteger(). */
        return QString::number(static_cast<xsInteger>(value));
    }
    else
    {
        int sign;
        int decimalPoint;
        const QString qret = qdtoa(value, &decimalPoint, &sign);
        QString valueAsString;

        if(sign)
            valueAsString += QLatin1Char('-');

        if(0 < decimalPoint)
        {
            valueAsString += qret.left(decimalPoint);
            valueAsString += QLatin1Char('.');
            if (qret.size() <= decimalPoint)
                valueAsString += QLatin1Char('0');
            else
                valueAsString += qret.mid(decimalPoint);
        }
        else
        {
            valueAsString += QLatin1Char('0');
            valueAsString += QLatin1Char('.');

            for(int d = decimalPoint; d < 0; d++)
                valueAsString += QLatin1Char('0');

            valueAsString += qret;
        }

        return valueAsString;
    }
}

ItemType::Ptr Decimal::type() const
{
    return BuiltinTypes::xsDecimal;
}

xsDouble Decimal::toDouble() const
{
    return static_cast<xsDouble>(m_value);
}

xsInteger Decimal::toInteger() const
{
    return static_cast<xsInteger>(m_value);
}

xsFloat Decimal::toFloat() const
{
    return static_cast<xsFloat>(m_value);
}

xsDecimal Decimal::toDecimal() const
{
    return m_value;
}

qulonglong Decimal::toUnsignedInteger() const
{
    Q_ASSERT_X(false, Q_FUNC_INFO,
               "It makes no sense to call this function, see Numeric::toUnsignedInteger().");
    return 0;
}

Numeric::Ptr Decimal::round() const
{
    return Numeric::Ptr(new Decimal(roundFloat(m_value)));
}

Numeric::Ptr Decimal::roundHalfToEven(const xsInteger /*scale*/) const
{
    return Numeric::Ptr();
}

Numeric::Ptr Decimal::floor() const
{
    return Numeric::Ptr(new Decimal(static_cast<xsDecimal>(::floor(m_value))));
}

Numeric::Ptr Decimal::ceiling() const
{
    return Numeric::Ptr(new Decimal(static_cast<xsDecimal>(ceil(m_value))));
}

Numeric::Ptr Decimal::abs() const
{
    return Numeric::Ptr(new Decimal(static_cast<xsDecimal>(fabs(m_value))));
}

bool Decimal::isNaN() const
{
    return false;
}

bool Decimal::isInf() const
{
    return false;
}

Item Decimal::toNegated() const
{
    if(AbstractFloat<true>::isEqual(m_value, 0.0))
        return fromValue(0).data();
    else
        return fromValue(-m_value).data();
}

bool Decimal::isSigned() const
{
    Q_ASSERT_X(false, Q_FUNC_INFO,
               "It makes no sense to call this function, see Numeric::isSigned().");
    return false;
}

QT_END_NAMESPACE
