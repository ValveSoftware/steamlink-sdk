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

#include "qabstractfloat_p.h"
#include "qanyuri_p.h"
#include "qboolean_p.h"
#include "qbuiltintypes_p.h"
#include "qcommonnamespaces_p.h"
#include "qcommonvalues_p.h"
#include "qliteral_p.h"
#include "qatomicstring_p.h"

#include "qnodefns_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

Item NameFN::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item node(m_operands.first()->evaluateSingleton(context));

    if(node)
    {
        const QXmlName name(node.asNode().name());

        if(name.isNull())
            return CommonValues::EmptyString;
        else
            return AtomicString::fromValue(context->namePool()->toLexical(name));
    }
    else
        return CommonValues::EmptyString;
}

Item LocalNameFN::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item node(m_operands.first()->evaluateSingleton(context));

    if(node)
    {
        const QXmlName name(node.asNode().name());

        if(name.isNull())
            return CommonValues::EmptyString;
        else
            return AtomicString::fromValue(context->namePool()->stringForLocalName(name.localName()));
    }
    else
        return CommonValues::EmptyString;
}

Item NamespaceURIFN::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item node(m_operands.first()->evaluateSingleton(context));

    if(node)
    {
        const QXmlName name(node.asNode().name());

        if(name.isNull())
            return CommonValues::EmptyAnyURI;
        else
            return toItem(AnyURI::fromValue(context->namePool()->stringForNamespace(name.namespaceURI())));
    }
    else
        return CommonValues::EmptyAnyURI;
}

Item NumberFN::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item item(m_operands.first()->evaluateSingleton(context));

    if(!item)
        return CommonValues::DoubleNaN;

    const Item val(cast(item, context));
    Q_ASSERT(val);

    if(val.as<AtomicValue>()->hasError())
        return CommonValues::DoubleNaN;
    else
        return val;
}

Expression::Ptr NumberFN::typeCheck(const StaticContext::Ptr &context,
                                    const SequenceType::Ptr &reqType)
{
    const Expression::Ptr me(FunctionCall::typeCheck(context, reqType));
    const ItemType::Ptr sourceType(m_operands.first()->staticType()->itemType());

    if(BuiltinTypes::xsDouble->xdtTypeMatches(sourceType))
    {
        /* The operand is already xs:double, no need for fn:number(). */
        return m_operands.first()->typeCheck(context, reqType);
    }
    else if(prepareCasting(context, sourceType))
        return me;
    else
    {
        /* Casting to xs:double will never succeed and we would always return NaN.*/
        return wrapLiteral(CommonValues::DoubleNaN, context, this)->typeCheck(context, reqType);
    }
}

bool LangFN::isLangMatch(const QString &candidate, const QString &toMatch)
{
    if(QString::compare(candidate, toMatch, Qt::CaseInsensitive) == 0)
        return true;

    return candidate.startsWith(toMatch, Qt::CaseInsensitive)
           && candidate.length() > toMatch.length()
           && candidate.at(toMatch.length()) == QLatin1Char('-');
}

Item LangFN::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item langArg(m_operands.first()->evaluateSingleton(context));
    const QString lang(langArg ? langArg.stringValue() : QString());

    const QXmlName xmlLang(StandardNamespaces::xml, StandardLocalNames::lang, StandardPrefixes::xml);
    const QXmlNodeModelIndex langNode(m_operands.at(1)->evaluateSingleton(context).asNode());

    const QXmlNodeModelIndex::Iterator::Ptr ancestors(langNode.iterate(QXmlNodeModelIndex::AxisAncestorOrSelf));
    QXmlNodeModelIndex ancestor(ancestors->next());

    while(!ancestor.isNull())
    {
        const QXmlNodeModelIndex::Iterator::Ptr attributes(ancestor.iterate(QXmlNodeModelIndex::AxisAttribute));
        QXmlNodeModelIndex attribute(attributes->next());

        while(!attribute.isNull())
        {
            Q_ASSERT(attribute.kind() == QXmlNodeModelIndex::Attribute);

            if(attribute.name() == xmlLang)
            {
                if(isLangMatch(attribute.stringValue(), lang))
                    return CommonValues::BooleanTrue;
                else
                    return CommonValues::BooleanFalse;
            }

            attribute = attributes->next();
        }

        ancestor = ancestors->next();
    }

    return CommonValues::BooleanFalse;
}

Item RootFN::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item arg(m_operands.first()->evaluateSingleton(context));

    if(arg)
        return arg.asNode().root();
    else
        return Item();
}

SequenceType::Ptr RootFN::staticType() const
{
    if(m_operands.isEmpty())
        return makeGenericSequenceType(BuiltinTypes::node, Cardinality::exactlyOne());
    else
        return makeGenericSequenceType(BuiltinTypes::node, m_operands.first()->staticType()->cardinality().toWithoutMany());
}

QT_END_NAMESPACE
