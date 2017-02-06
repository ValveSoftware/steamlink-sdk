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

#include "qforclause_p.h"
#include "qfunctionfactory_p.h"
#include "qrangevariablereference_p.h"

#include "qdocumentfn_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

Expression::Ptr DocumentFN::typeCheck(const StaticContext::Ptr &context,
                                      const SequenceType::Ptr &reqType)
{
    /* See the class documentation for the rewrite that we're doing here. */

    /* Generate type checking code for our operands such that they match. */
    typeCheckOperands(context);

    const QSourceLocation myLocation(context->locationFor(this));
    const FunctionFactory::Ptr functions(context->functionSignatures());

    Expression::Ptr uriSource;

    {
        Expression::List distinctValuesArgs;
        distinctValuesArgs.append(m_operands.first());

        uriSource = functions->createFunctionCall(QXmlName(StandardNamespaces::fn, StandardLocalNames::distinct_values),
                                                  distinctValuesArgs,
                                                  context,
                                                  this);
        context->addLocation(uriSource.data(), myLocation);
    }

    const VariableSlotID rangeSlot = context->allocateRangeSlot();
    const Expression::Ptr uriReference(new RangeVariableReference(uriSource, rangeSlot));
    context->addLocation(uriReference.data(), myLocation);

    Expression::List docArgs;

    if(m_operands.count() == 2)
    {
        Expression::List baseUriArgs;
        baseUriArgs.append(uriReference);
        baseUriArgs.append(m_operands.at(1));

        const Expression::Ptr fnBaseUri(functions->createFunctionCall(QXmlName(StandardNamespaces::fn, StandardLocalNames::resolve_uri),
                                                                      baseUriArgs,
                                                                      context,
                                                                      this));
        context->addLocation(fnBaseUri.data(), myLocation);
        docArgs.append(fnBaseUri);
    }
    else
        docArgs.append(uriReference);

    const Expression::Ptr fnDoc(functions->createFunctionCall(QXmlName(StandardNamespaces::fn, StandardLocalNames::doc),
                                                              docArgs,
                                                              context,
                                                              this));
    context->addLocation(fnDoc.data(), myLocation);


    Expression::Ptr newMe(new ForClause(rangeSlot,
                                        uriSource,
                                        fnDoc,
                                        -1 /* We have no position variable. */));

    Expression::Ptr oldMe(this);
    rewrite(oldMe, newMe, context);
    return newMe->typeCheck(context, reqType);
}

QT_END_NAMESPACE
