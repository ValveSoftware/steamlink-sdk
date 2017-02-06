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

#include "qbuiltintypes_p.h"
#include "qcommonnamespaces_p.h"
#include "qcommonsequencetypes_p.h"
#include "qpatternistlocale_p.h"
#include "qqnamevalue_p.h"
#include "qatomicstring_p.h"

#include "qattributenamevalidator_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

AttributeNameValidator::AttributeNameValidator(const Expression::Ptr &source) : SingleContainer(source)
{
}

Item AttributeNameValidator::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item name(m_operand->evaluateSingleton(context));
    const QXmlName qName(name.as<QNameValue>()->qName());

    if(qName.namespaceURI() == StandardNamespaces::xmlns)
    {
        context->error(QtXmlPatterns::tr("The namespace URI in the name for a "
                                         "computed attribute cannot be %1.")
                       .arg(formatURI(CommonNamespaces::XMLNS)),
                       ReportContext::XQDY0044, this);
        return Item(); /* Silence warning. */
    }
    else if(qName.namespaceURI() == StandardNamespaces::empty &&
            qName.localName() == StandardLocalNames::xmlns)
    {
        context->error(QtXmlPatterns::tr("The name for a computed attribute "
                                         "cannot have the namespace URI %1 "
                                         "with the local name %2.")
                          .arg(formatURI(CommonNamespaces::XMLNS))
                          .arg(formatKeyword("xmlns")),
                       ReportContext::XQDY0044, this);
        return Item(); /* Silence warning. */
    }
    else if(!qName.hasPrefix() && qName.hasNamespace())
    {
        return Item(QNameValue::fromValue(context->namePool(),
                                          QXmlName(qName.namespaceURI(), qName.localName(), StandardPrefixes::ns0)));
    }
    else
        return name;
}

SequenceType::Ptr AttributeNameValidator::staticType() const
{
    return m_operand->staticType();
}

SequenceType::List AttributeNameValidator::expectedOperandTypes() const
{
    SequenceType::List result;
    result.append(CommonSequenceTypes::ExactlyOneQName);
    return result;
}

ExpressionVisitorResult::Ptr AttributeNameValidator::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

QT_END_NAMESPACE
