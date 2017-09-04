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

#include "qxsdfacet_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

XsdFacet::XsdFacet()
    : m_type(None)
{
}

void XsdFacet::setType(Type type)
{
    m_type = type;
}

XsdFacet::Type XsdFacet::type() const
{
    return m_type;
}

void XsdFacet::setValue(const AtomicValue::Ptr &value)
{
    m_value = value;
}

AtomicValue::Ptr XsdFacet::value() const
{
    return m_value;
}

void XsdFacet::setMultiValue(const AtomicValue::List &value)
{
    m_multiValue = value;
}

AtomicValue::List XsdFacet::multiValue() const
{
    return m_multiValue;
}

void XsdFacet::setAssertions(const XsdAssertion::List &assertions)
{
    m_assertions = assertions;
}

XsdAssertion::List XsdFacet::assertions() const
{
    return m_assertions;
}

void XsdFacet::setFixed(bool fixed)
{
    m_fixed = fixed;
}

bool XsdFacet::fixed() const
{
    return m_fixed;
}

QString XsdFacet::typeName(Type type)
{
    switch (type) {
        case Length: return QLatin1String("length"); break;
        case MinimumLength: return QLatin1String("minLength"); break;
        case MaximumLength: return QLatin1String("maxLength"); break;
        case Pattern: return QLatin1String("pattern"); break;
        case WhiteSpace: return QLatin1String("whiteSpace"); break;
        case MaximumInclusive: return QLatin1String("maxInclusive"); break;
        case MaximumExclusive: return QLatin1String("maxExclusive"); break;
        case MinimumInclusive: return QLatin1String("minInclusive"); break;
        case MinimumExclusive: return QLatin1String("minExclusive"); break;
        case TotalDigits: return QLatin1String("totalDigits"); break;
        case FractionDigits: return QLatin1String("fractionDigits"); break;
        case Enumeration: return QLatin1String("enumeration"); break;
        case Assertion: return QLatin1String("assertion"); break;
        case None: // fall through
        default: return QLatin1String("none"); break;
    }
}

QT_END_NAMESPACE
