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

#include "qboolean_p.h"
#include "qdelegatingnamespaceresolver_p.h"
#include "qinteger_p.h"
#include "qqnameconstructor_p.h"

#include "qfunctionavailablefn_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

Item FunctionAvailableFN::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const QString lexQName(m_operands.first()->evaluateSingleton(context).stringValue());

    NamespaceResolver::Bindings override;
    override.insert(StandardPrefixes::empty, m_defFuncNS);

    const NamespaceResolver::Ptr resolver(new DelegatingNamespaceResolver(staticNamespaces(), override));

    const QXmlName name
        (QNameConstructor::expandQName<DynamicContext::Ptr,
                                       ReportContext::XTDE1400,
                                       ReportContext::XTDE1400>(lexQName,
                                                                context,
                                                                resolver,
                                                                this));

    xsInteger arity;

    if(m_operands.count() == 2)
        arity = m_operands.at(1)->evaluateSingleton(context).as<Numeric>()->toInteger();
    else
        arity = FunctionSignature::UnlimitedArity;

    return Boolean::fromValue(m_functionFactory->isAvailable(context->namePool(), name, arity));
}

Expression::Ptr FunctionAvailableFN::typeCheck(const StaticContext::Ptr &context,
                                               const SequenceType::Ptr &reqType)
{
    m_functionFactory = context->functionSignatures();
    Q_ASSERT(m_functionFactory);
    m_defFuncNS = context->namePool()->allocateNamespace(context->defaultFunctionNamespace());
    /* m_defFuncNS can be empty/null or an actual value. */

    return StaticNamespacesContainer::typeCheck(context, reqType);
}

QT_END_NAMESPACE
