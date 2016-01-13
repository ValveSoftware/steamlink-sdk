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
