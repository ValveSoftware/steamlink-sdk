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
