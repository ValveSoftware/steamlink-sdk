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

#include <QString>

#include "qatomiccomparator_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

AtomicComparator::AtomicComparator()
{
}

AtomicComparator::~AtomicComparator()
{
}

AtomicComparator::ComparisonResult
AtomicComparator::compare(const Item &,
                          const AtomicComparator::Operator,
                          const Item &) const
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "This function should never be called.");
    return LessThan;
}

QString AtomicComparator::displayName(const AtomicComparator::Operator op,
                                      const ComparisonType type)
{
    Q_ASSERT(type == AsGeneralComparison || type == AsValueComparison);
    if(type == AsGeneralComparison)
    {
        switch(op)
        {
            case OperatorEqual:
                return QLatin1String("=");
            case OperatorGreaterOrEqual:
                return QLatin1String("<=");
            case OperatorGreaterThan:
                return QLatin1String("<");
            case OperatorLessOrEqual:
                return QLatin1String(">=");
            case OperatorLessThanNaNLeast:
            case OperatorLessThanNaNGreatest:
            case OperatorLessThan:
                return QLatin1String(">");
            case OperatorNotEqual:
                return QLatin1String("!=");
        }
    }

    switch(op)
    {
        case OperatorEqual:
            return QLatin1String("eq");
        case OperatorGreaterOrEqual:
            return QLatin1String("ge");
        case OperatorGreaterThan:
            return QLatin1String("gt");
        case OperatorLessOrEqual:
            return QLatin1String("le");
        case OperatorLessThanNaNLeast:
        case OperatorLessThanNaNGreatest:
        case OperatorLessThan:
            return QLatin1String("lt");
        case OperatorNotEqual:
            return QLatin1String("ne");
    }

    Q_ASSERT(false);
    return QString(); /* GCC unbarfer. */
}

QT_END_NAMESPACE
