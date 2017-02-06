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

#include "qcallsite_p.h"

#include "qcalltargetdescription_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

CallTargetDescription::CallTargetDescription(const QXmlName &name) : m_name(name)
{
    Q_ASSERT(!m_name.isNull());
}

QXmlName CallTargetDescription::name() const
{
    return m_name;
}

void CallTargetDescription::checkArgumentsCircularity(CallTargetDescription::List &signList,
                                                      const Expression::Ptr callsite)
{
    /* Check the arguments. */
    const Expression::List ops(callsite->operands());
    const Expression::List::const_iterator end(ops.constEnd());
    Expression::List::const_iterator it(ops.constBegin());

    for(; it != end; ++it)
        checkCallsiteCircularity(signList, *it);
}

void CallTargetDescription::checkCallsiteCircularity(CallTargetDescription::List &signList,
                                                     const Expression::Ptr expr)
{
    Q_ASSERT(expr);

    if(expr->is(Expression::IDUserFunctionCallsite))
    {
        CallTargetDescription::List::const_iterator it(signList.constBegin());
        const CallTargetDescription::List::const_iterator end(signList.constEnd());
        CallSite *const callsite = static_cast<CallSite *>(expr.data());

        for(; it != end; ++it)
        {
            if(callsite->configureRecursion(*it))
            {
                /* A callsite inside the function body to the function. This user function
                 * is recursive if it's to the same function, in other words. Which it was
                 * if configureRecursion() returned true. */

                /* Now we continue and check the arguments of the callsite. That is, the arguments.
                 * This catches for instance local:foo(local:foo(3)). */
                checkArgumentsCircularity(signList, expr);
                return;
            }
        }
        /* Check the body of the function so this callsite isn't "indirectly" a
         * recursive call to the function we're checking. XQTS test case
         * default_namespace-011 is an example of this. */
        signList.append(callsite->callTargetDescription());
        checkCallsiteCircularity(signList, callsite->body());
    }

    checkArgumentsCircularity(signList, expr); /* We're done in this case. */
}

QT_END_NAMESPACE
