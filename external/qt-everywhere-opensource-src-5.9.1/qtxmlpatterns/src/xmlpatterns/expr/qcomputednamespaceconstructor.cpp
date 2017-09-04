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

#include "qanyuri_p.h"
#include "qbuiltintypes_p.h"
#include "qcommonsequencetypes_p.h"
#include "qpatternistlocale_p.h"
#include "private/qxmlutils_p.h"

#include "qcomputednamespaceconstructor_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

ComputedNamespaceConstructor::ComputedNamespaceConstructor(const Expression::Ptr &prefix,
                                                           const Expression::Ptr &namespaceURI) : PairContainer(prefix, namespaceURI)
{
}

void ComputedNamespaceConstructor::evaluateToSequenceReceiver(const DynamicContext::Ptr &context) const
{
    const Item prefixItem(m_operand1->evaluateSingleton(context));
    const QString prefix(prefixItem ? prefixItem.stringValue() : QString());

    const Item namespaceItem(m_operand2->evaluateSingleton(context));
    const QString namespaceURI(namespaceItem ? namespaceItem.stringValue() : QString());

    if(namespaceURI.isEmpty())
    {
        context->error(QtXmlPatterns::tr("In a namespace constructor, the value for a namespace cannot be an empty string."),
                       ReportContext::XTDE0930,
                       this);
    }

    /* One optimization could be to store a pointer to
     * the name pool as a member in order to avoid the virtual call(s). */
    const NamePool::Ptr np(context->namePool());

    if(!prefix.isEmpty() && !QXmlUtils::isNCName(prefix))
    {
        context->error(QtXmlPatterns::tr("The prefix must be a valid %1, which %2 is not.")
                                        .arg(formatType(np, BuiltinTypes::xsNCName),
                                             formatKeyword(prefix)),
                       ReportContext::XTDE0920,
                       this);
    }
    const QXmlName binding(np->allocateBinding(prefix, namespaceURI));

    AnyURI::toQUrl<ReportContext::XTDE0905, DynamicContext::Ptr>(namespaceURI,
                                                                 context,
                                                                 this);

    if(binding.prefix() == StandardPrefixes::xmlns)
    {
        context->error(QtXmlPatterns::tr("The prefix %1 cannot be bound.")
                                        .arg(formatKeyword(prefix)),
                       ReportContext::XTDE0920,
                       this);
    }

    if((binding.prefix() == StandardPrefixes::xml && binding.namespaceURI() != StandardNamespaces::xml)
       ||
       (binding.prefix() != StandardPrefixes::xml && binding.namespaceURI() == StandardNamespaces::xml))
    {
        context->error(QtXmlPatterns::tr("Only the prefix %1 can be bound to %2 and vice versa.")
                                        .arg(formatKeyword(prefix), formatKeyword(namespaceURI)),
                       ReportContext::XTDE0925,
                       this);
    }

     context->outputReceiver()->namespaceBinding(binding);
}

SequenceType::Ptr ComputedNamespaceConstructor::staticType() const
{
    return CommonSequenceTypes::ExactlyOneAttribute;
}

SequenceType::List ComputedNamespaceConstructor::expectedOperandTypes() const
{
    SequenceType::List result;
    result.append(CommonSequenceTypes::ZeroOrOneString);
    result.append(CommonSequenceTypes::ZeroOrOneString);
    return result;
}

Expression::Properties ComputedNamespaceConstructor::properties() const
{
    return DisableElimination | IsNodeConstructor;
}

ExpressionVisitorResult::Ptr ComputedNamespaceConstructor::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

QT_END_NAMESPACE
