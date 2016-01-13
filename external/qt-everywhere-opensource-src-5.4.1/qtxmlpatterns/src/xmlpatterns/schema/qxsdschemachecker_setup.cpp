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

#include "qxsdschemachecker_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

void XsdSchemaChecker::setupAllowedAtomicFacets()
{
    // string
    {
        QSet<XsdFacet::Type> facets;
        facets << XsdFacet::Length
               << XsdFacet::MinimumLength
               << XsdFacet::MaximumLength
               << XsdFacet::Pattern
               << XsdFacet::Enumeration
               << XsdFacet::WhiteSpace
               << XsdFacet::Assertion;

        m_allowedAtomicFacets.insert(BuiltinTypes::xsString->name(m_namePool), facets);
    }

    // boolean
    {
        QSet<XsdFacet::Type> facets;
        facets << XsdFacet::Pattern
               << XsdFacet::WhiteSpace
               << XsdFacet::Assertion;

        m_allowedAtomicFacets.insert(BuiltinTypes::xsBoolean->name(m_namePool), facets);
    }

    // float
    {
        QSet<XsdFacet::Type> facets;
        facets << XsdFacet::Pattern
               << XsdFacet::Enumeration
               << XsdFacet::WhiteSpace
               << XsdFacet::MaximumInclusive
               << XsdFacet::MaximumExclusive
               << XsdFacet::MinimumInclusive
               << XsdFacet::MinimumExclusive
               << XsdFacet::Assertion;

        m_allowedAtomicFacets.insert(BuiltinTypes::xsFloat->name(m_namePool), facets);
    }

    // double
    {
        QSet<XsdFacet::Type> facets;
        facets << XsdFacet::Pattern
               << XsdFacet::Enumeration
               << XsdFacet::WhiteSpace
               << XsdFacet::MaximumInclusive
               << XsdFacet::MaximumExclusive
               << XsdFacet::MinimumInclusive
               << XsdFacet::MinimumExclusive
               << XsdFacet::Assertion;

        m_allowedAtomicFacets.insert(BuiltinTypes::xsDouble->name(m_namePool), facets);
    }

    // decimal
    {
        QSet<XsdFacet::Type> facets;
        facets << XsdFacet::TotalDigits
               << XsdFacet::FractionDigits
               << XsdFacet::Pattern
               << XsdFacet::Enumeration
               << XsdFacet::WhiteSpace
               << XsdFacet::MaximumInclusive
               << XsdFacet::MaximumExclusive
               << XsdFacet::MinimumInclusive
               << XsdFacet::MinimumExclusive
               << XsdFacet::Assertion;

        m_allowedAtomicFacets.insert(BuiltinTypes::xsDecimal->name(m_namePool), facets);
    }

    // duration
    {
        QSet<XsdFacet::Type> facets;
        facets << XsdFacet::Pattern
               << XsdFacet::Enumeration
               << XsdFacet::WhiteSpace
               << XsdFacet::MaximumInclusive
               << XsdFacet::MaximumExclusive
               << XsdFacet::MinimumInclusive
               << XsdFacet::MinimumExclusive
               << XsdFacet::Assertion;

        m_allowedAtomicFacets.insert(BuiltinTypes::xsDuration->name(m_namePool), facets);
    }

    // dateTime
    {
        QSet<XsdFacet::Type> facets;
        facets << XsdFacet::Pattern
               << XsdFacet::Enumeration
               << XsdFacet::WhiteSpace
               << XsdFacet::MaximumInclusive
               << XsdFacet::MaximumExclusive
               << XsdFacet::MinimumInclusive
               << XsdFacet::MinimumExclusive
               << XsdFacet::Assertion;

        m_allowedAtomicFacets.insert(BuiltinTypes::xsDateTime->name(m_namePool), facets);
    }

    // time
    {
        QSet<XsdFacet::Type> facets;
        facets << XsdFacet::Pattern
               << XsdFacet::Enumeration
               << XsdFacet::WhiteSpace
               << XsdFacet::MaximumInclusive
               << XsdFacet::MaximumExclusive
               << XsdFacet::MinimumInclusive
               << XsdFacet::MinimumExclusive
               << XsdFacet::Assertion;

        m_allowedAtomicFacets.insert(BuiltinTypes::xsTime->name(m_namePool), facets);
    }

    // date
    {
        QSet<XsdFacet::Type> facets;
        facets << XsdFacet::Pattern
               << XsdFacet::Enumeration
               << XsdFacet::WhiteSpace
               << XsdFacet::MaximumInclusive
               << XsdFacet::MaximumExclusive
               << XsdFacet::MinimumInclusive
               << XsdFacet::MinimumExclusive
               << XsdFacet::Assertion;

        m_allowedAtomicFacets.insert(BuiltinTypes::xsDate->name(m_namePool), facets);
    }

    // gYearMonth
    {
        QSet<XsdFacet::Type> facets;
        facets << XsdFacet::Pattern
               << XsdFacet::Enumeration
               << XsdFacet::WhiteSpace
               << XsdFacet::MaximumInclusive
               << XsdFacet::MaximumExclusive
               << XsdFacet::MinimumInclusive
               << XsdFacet::MinimumExclusive
               << XsdFacet::Assertion;

        m_allowedAtomicFacets.insert(BuiltinTypes::xsGYearMonth->name(m_namePool), facets);
    }

    // gYear
    {
        QSet<XsdFacet::Type> facets;
        facets << XsdFacet::Pattern
               << XsdFacet::Enumeration
               << XsdFacet::WhiteSpace
               << XsdFacet::MaximumInclusive
               << XsdFacet::MaximumExclusive
               << XsdFacet::MinimumInclusive
               << XsdFacet::MinimumExclusive
               << XsdFacet::Assertion;

        m_allowedAtomicFacets.insert(BuiltinTypes::xsGYear->name(m_namePool), facets);
    }

    // gMonthDay
    {
        QSet<XsdFacet::Type> facets;
        facets << XsdFacet::Pattern
               << XsdFacet::Enumeration
               << XsdFacet::WhiteSpace
               << XsdFacet::MaximumInclusive
               << XsdFacet::MaximumExclusive
               << XsdFacet::MinimumInclusive
               << XsdFacet::MinimumExclusive
               << XsdFacet::Assertion;

        m_allowedAtomicFacets.insert(BuiltinTypes::xsGMonthDay->name(m_namePool), facets);
    }

    // gDay
    {
        QSet<XsdFacet::Type> facets;
        facets << XsdFacet::Pattern
               << XsdFacet::Enumeration
               << XsdFacet::WhiteSpace
               << XsdFacet::MaximumInclusive
               << XsdFacet::MaximumExclusive
               << XsdFacet::MinimumInclusive
               << XsdFacet::MinimumExclusive
               << XsdFacet::Assertion;

        m_allowedAtomicFacets.insert(BuiltinTypes::xsGDay->name(m_namePool), facets);
    }

    // gMonth
    {
        QSet<XsdFacet::Type> facets;
        facets << XsdFacet::Pattern
               << XsdFacet::Enumeration
               << XsdFacet::WhiteSpace
               << XsdFacet::MaximumInclusive
               << XsdFacet::MaximumExclusive
               << XsdFacet::MinimumInclusive
               << XsdFacet::MinimumExclusive
               << XsdFacet::Assertion;

        m_allowedAtomicFacets.insert(BuiltinTypes::xsGMonth->name(m_namePool), facets);
    }

    // hexBinary
    {
        QSet<XsdFacet::Type> facets;
        facets << XsdFacet::Length
               << XsdFacet::MinimumLength
               << XsdFacet::MaximumLength
               << XsdFacet::Pattern
               << XsdFacet::Enumeration
               << XsdFacet::WhiteSpace
               << XsdFacet::Assertion;

        m_allowedAtomicFacets.insert(BuiltinTypes::xsHexBinary->name(m_namePool), facets);
    }

    // base64Binary
    {
        QSet<XsdFacet::Type> facets;
        facets << XsdFacet::Length
               << XsdFacet::MinimumLength
               << XsdFacet::MaximumLength
               << XsdFacet::Pattern
               << XsdFacet::Enumeration
               << XsdFacet::WhiteSpace
               << XsdFacet::Assertion;

        m_allowedAtomicFacets.insert(BuiltinTypes::xsBase64Binary->name(m_namePool), facets);
    }

    // anyURI
    {
        QSet<XsdFacet::Type> facets;
        facets << XsdFacet::Length
               << XsdFacet::MinimumLength
               << XsdFacet::MaximumLength
               << XsdFacet::Pattern
               << XsdFacet::Enumeration
               << XsdFacet::WhiteSpace
               << XsdFacet::Assertion;

        m_allowedAtomicFacets.insert(BuiltinTypes::xsAnyURI->name(m_namePool), facets);
    }

    // QName
    {
        QSet<XsdFacet::Type> facets;
        facets << XsdFacet::Length
               << XsdFacet::MinimumLength
               << XsdFacet::MaximumLength
               << XsdFacet::Pattern
               << XsdFacet::Enumeration
               << XsdFacet::WhiteSpace
               << XsdFacet::Assertion;

        m_allowedAtomicFacets.insert(BuiltinTypes::xsQName->name(m_namePool), facets);
    }

    // NOTATION
    {
        QSet<XsdFacet::Type> facets;
        facets << XsdFacet::Length
               << XsdFacet::MinimumLength
               << XsdFacet::MaximumLength
               << XsdFacet::Pattern
               << XsdFacet::Enumeration
               << XsdFacet::WhiteSpace
               << XsdFacet::Assertion;

        m_allowedAtomicFacets.insert(BuiltinTypes::xsNOTATION->name(m_namePool), facets);
    }
}

QT_END_NAMESPACE
