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

#include "qcommonsequencetypes_p.h"
#include "qpatternistlocale_p.h"
#include "qqnamevalue_p.h"

#include "qqnameconstructor_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

QNameConstructor::QNameConstructor(const Expression::Ptr &source,
                                   const NamespaceResolver::Ptr &nsResolver) : SingleContainer(source),
                                                                               m_nsResolver(nsResolver)
{
    Q_ASSERT(m_nsResolver);
}

Item QNameConstructor::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    Q_ASSERT(context);
    const QString lexQName(m_operand->evaluateSingleton(context).stringValue());

    const QXmlName expQName(expandQName<DynamicContext::Ptr,
                                        ReportContext::XQDY0074,
                                        ReportContext::XQDY0074>(lexQName,
                                                                 context,
                                                                 m_nsResolver,
                                                                 this));
    return toItem(QNameValue::fromValue(context->namePool(), expQName));
}

QXmlName::NamespaceCode QNameConstructor::namespaceForPrefix(const QXmlName::PrefixCode prefix,
                                                          const StaticContext::Ptr &context,
                                                          const SourceLocationReflection *const r)
{
    Q_ASSERT(context);
    const QXmlName::NamespaceCode ns(context->namespaceBindings()->lookupNamespaceURI(prefix));

    if(ns == NamespaceResolver::NoBinding)
    {
        context->error(QtXmlPatterns::tr("No namespace binding exists for the prefix %1")
                          .arg(formatKeyword(context->namePool()->stringForPrefix(prefix))),
                       ReportContext::XPST0081,
                       r);
        return NamespaceResolver::NoBinding;
    }
    else
        return ns;
}

SequenceType::Ptr QNameConstructor::staticType() const
{
    return CommonSequenceTypes::ExactlyOneQName;
}

SequenceType::List QNameConstructor::expectedOperandTypes() const
{
    SequenceType::List result;
    result.append(CommonSequenceTypes::ExactlyOneString);
    return result;
}

ExpressionVisitorResult::Ptr QNameConstructor::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

const SourceLocationReflection *QNameConstructor::actualReflection() const
{
    return m_operand.data();
}

QT_END_NAMESPACE
