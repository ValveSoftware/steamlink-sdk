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

#include "qanyuri_p.h"
#include "qboolean_p.h"
#include "qbuiltintypes_p.h"
#include "qcommonvalues_p.h"
#include "qliteral_p.h"
#include "qitem_p.h"
#include "qqnamevalue_p.h"
#include "qatomicstring_p.h"

#include "qaccessorfns_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

Item NodeNameFN::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item item(m_operands.first()->evaluateSingleton(context));

    if(item)
    {
        const QXmlName name(item.asNode().name());

        if(name.isNull())
            return Item();
        else
            return toItem(QNameValue::fromValue(context->namePool(), name));
    }
    else
        return Item();
}

Item NilledFN::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item item(m_operands.first()->evaluateSingleton(context));

    if(item && item.asNode().kind() == QXmlNodeModelIndex::Element)
    {
        /* We have no access to the PSVI -- always return false. */
        return CommonValues::BooleanFalse;
    }
    else
        return Item();
}

Item StringFN::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item item(m_operands.first()->evaluateSingleton(context));

    if(item)
        return AtomicString::fromValue(item.stringValue());
    else
        return CommonValues::EmptyString;
}

Expression::Ptr StringFN::typeCheck(const StaticContext::Ptr &context,
                                    const SequenceType::Ptr &reqType)
{
    const Expression::Ptr me(FunctionCall::typeCheck(context, reqType));
    if(me != this)
        return me;

    if(BuiltinTypes::xsString->xdtTypeMatches(m_operands.first()->staticType()->itemType()))
        return m_operands.first(); /* No need for string(), it's already a string. */
    else
        return me;
}

Item BaseURIFN::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item node(m_operands.first()->evaluateSingleton(context));

    if(node)
    {
        const QUrl base(node.asNode().baseUri());

        if(base.isEmpty())
            return Item();
        else if(base.isValid())
        {
            Q_ASSERT_X(!base.isRelative(), Q_FUNC_INFO,
                       "The base URI must be absolute.");
            return toItem(AnyURI::fromValue(base));
        }
        else
            return Item();
    }
    else
        return Item();
}

Item DocumentURIFN::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item node(m_operands.first()->evaluateSingleton(context));

    if(node)
    {
        const QUrl documentURI(node.asNode().documentUri());

        if(documentURI.isValid())
        {
            if(documentURI.isEmpty())
                return Item();
            else
            {
                Q_ASSERT_X(!documentURI.isRelative(), Q_FUNC_INFO,
                           "The document URI must be absolute.");
                return toItem(AnyURI::fromValue(documentURI));
            }
        }
        else
            return Item();
    }
    else
        return Item();
}

QT_END_NAMESPACE
