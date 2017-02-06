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

#include "qpatternistlocale_p.h"

#include "qcardinality_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

QString Cardinality::displayName(const CustomizeDisplayName explain) const
{
    if(explain == IncludeExplanation)
    {
        if(isEmpty())
            return QString(QtXmlPatterns::tr("empty") + QLatin1String("(\"empty-sequence()\")"));
        else if(isZeroOrOne())
            return QString(QtXmlPatterns::tr("zero or one") + QLatin1String("(\"?\")"));
        else if(isExactlyOne())
            return QString(QtXmlPatterns::tr("exactly one"));
        else if(isOneOrMore())
            return QString(QtXmlPatterns::tr("one or more") + QLatin1String("(\"+\")"));
        else
            return QString(QtXmlPatterns::tr("zero or more") + QLatin1String("(\"*\")"));
    }
    else
    {
        Q_ASSERT(explain == ExcludeExplanation);

        if(isEmpty() || isZeroOrOne())
            return QLatin1String("?");
        else if(isExactlyOne())
            return QString();
        else if(isExact())
        {
            return QString(QLatin1Char('{'))    +
                   QString::number(maximum())   +
                   QLatin1Char('}');
        }
        else
        {
            if(m_max == -1)
            {
                if(isOneOrMore())
                    return QChar::fromLatin1('+');
                else
                    return QChar::fromLatin1('*');
            }
            else
            {
                /* We have a range. We use a RegExp-like syntax. */
                return QString(QLatin1Char('{'))    +
                       QString::number(minimum())   +
                       QLatin1String(", ")          +
                       QString::number(maximum())   +
                       QLatin1Char('}');

            }
        }
    }
}

QT_END_NAMESPACE
