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

#include "qabstractfloat_p.h"
#include "qboolean_p.h"
#include "qbuiltintypes_p.h"
#include "qcommonvalues_p.h"
#include "qdecimal_p.h"
#include "qinteger_p.h"

#include "qschemanumeric_p.h"

/**
 * @file Contains class Numeric. This file was originally called qnumeric.cpp,
 * but was renamed to stay consistent with qschemanumeric_p.h
 */

QT_BEGIN_NAMESPACE

using namespace QPatternist;

AtomicValue::Ptr Numeric::fromLexical(const QString &number)
{
    Q_ASSERT(!number.isEmpty());
    Q_ASSERT_X(!number.contains(QLatin1Char('e')) &&
               !number.contains(QLatin1Char('E')),
               Q_FUNC_INFO, "Should not contain any e/E");

    if(number.contains(QLatin1Char('.'))) /* an xs:decimal. */
        return Decimal::fromLexical(number);
    else /* It's an integer, of some sort. E.g, -3, -2, -1, 0, 1, 2, 3 */
        return Integer::fromLexical(number);
}

xsDouble Numeric::roundFloat(const xsDouble val)
{
    if(qIsInf(val) || AbstractFloat<true>::isEqual(val, 0.0))
        return val;
    else if(qIsNaN(val))
        return val;
    else
    {
        if(val >= -0.5 && val < 0)
            return -0.0;
        else
            return ::floor(val + 0.5);

    }
}

QT_END_NAMESPACE
