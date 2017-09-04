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

#include "qcommonnamespaces_p.h"

#include "qboolean_p.h"
#include "qcommonvalues_p.h"
#include "qinteger_p.h"
#include "qatomicstring_p.h"

#include "qcomparestringfns_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

Item CodepointEqualFN::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item op1(m_operands.first()->evaluateSingleton(context));
    if(!op1)
        return Item();

    const Item op2(m_operands.last()->evaluateSingleton(context));
    if(!op2)
        return Item();

    if(caseSensitivity() == Qt::CaseSensitive)
        return Boolean::fromValue(op1.stringValue() == op2.stringValue());
    else
    {
        const QString s1(op1.stringValue());
        const QString s2(op2.stringValue());

        return Boolean::fromValue(s1.length() == s2.length() &&
                                  s1.startsWith(s2, Qt::CaseInsensitive));
    }
}

Item CompareFN::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item op1(m_operands.first()->evaluateSingleton(context));
    if(!op1)
        return Item();

    const Item op2(m_operands.at(1)->evaluateSingleton(context));
    if(!op2)
        return Item();

    const int retval = caseSensitivity() == Qt::CaseSensitive
                       ? op1.stringValue().compare(op2.stringValue())
                       : op1.stringValue().toLower().compare(op2.stringValue().toLower());

    if(retval > 0)
        return CommonValues::IntegerOne;
    else if(retval < 0)
        return CommonValues::IntegerOneNegative;
    else
    {
        Q_ASSERT(retval == 0);
        return CommonValues::IntegerZero;
    }
}

QT_END_NAMESPACE
