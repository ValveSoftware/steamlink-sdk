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

#include "qabstractfunctionfactory_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

Expression::Ptr AbstractFunctionFactory::createFunctionCall(const QXmlName name,
                                                            const Expression::List &args,
                                                            const StaticContext::Ptr &context,
                                                            const SourceLocationReflection *const r)
{
    const FunctionSignature::Ptr sign(retrieveFunctionSignature(context->namePool(), name));

    if(!sign) /* The function doesn't exist(at least not in this factory). */
        return Expression::Ptr();

    /* May throw. */
    verifyArity(sign, context, args.count(), r);

    /* Ok, the function does exist and the arity is correct. */
    return retrieveExpression(name, args, sign);
}

void AbstractFunctionFactory::verifyArity(const FunctionSignature::Ptr &s,
                                          const StaticContext::Ptr &context,
                                          const xsInteger arity,
                                          const SourceLocationReflection *const r) const
{
    /* Same code in both branches, but more specific error messages in order
     * to improve usability. */
    if(s->maximumArguments() != FunctionSignature::UnlimitedArity &&
       arity > s->maximumArguments())
    {
        context->error(QtXmlPatterns::tr("%1 takes at most %n argument(s). "
                                         "%2 is therefore invalid.", 0, s->maximumArguments())
                          .arg(formatFunction(context->namePool(), s))
                          .arg(arity),
                       ReportContext::XPST0017,
                       r);
        return;
    }

    if(arity < s->minimumArguments())
    {
        context->error(QtXmlPatterns::tr("%1 requires at least %n argument(s). "
                                         "%2 is therefore invalid.", 0, s->minimumArguments())
                          .arg(formatFunction(context->namePool(), s))
                          .arg(arity),
                       ReportContext::XPST0017,
                       r);
        return;
    }
}

FunctionSignature::Hash AbstractFunctionFactory::functionSignatures() const
{
    return m_signatures;
}

QT_END_NAMESPACE
