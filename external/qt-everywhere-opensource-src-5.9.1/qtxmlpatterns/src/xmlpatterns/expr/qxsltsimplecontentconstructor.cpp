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

#include "qatomicstring_p.h"
#include "qcommonsequencetypes_p.h"

#include "qxsltsimplecontentconstructor_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

XSLTSimpleContentConstructor::XSLTSimpleContentConstructor(const Expression::Ptr &source) : SimpleContentConstructor(source)
{
}

QString XSLTSimpleContentConstructor::processItem(const Item &item,
                                                  bool &discard,
                                                  bool &isText)
{
    if(item.isNode())
    {
        isText = (item.asNode().kind() == QXmlNodeModelIndex::Text);

        if(isText)
        {
            const QString value(item.stringValue());

            /* "1. Zero-length text nodes in the sequence are discarded." */
            discard = value.isEmpty();
            return value;
        }
        else
        {
            Item::Iterator::Ptr it(item.sequencedTypedValue()); /* Atomic values. */
            Item next(it->next());
            QString result;

            if(next)
                result = next.stringValue();

            next = it->next();

            while(next)
            {
                result += next.stringValue();
                result += QLatin1Char(' ');
                next = it->next();
            }

            return result;
        }
    }
    else
    {
        discard = false;
        isText = false;
        return item.stringValue();
    }
}

Item XSLTSimpleContentConstructor::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item::Iterator::Ptr it(m_operand->evaluateSequence(context));

    Item next(it->next());
    QString result;

    bool previousIsText = false;
    bool discard = false;

    if(next)
    {
        const QString unit(processItem(next, discard, previousIsText));

        if(!discard)
            result = unit;

        next = it->next();
    }
    else
        return Item();

    while(next)
    {
        bool currentIsText = false;
        const QString unit(processItem(next, discard, currentIsText));

        if(!discard)
        {
            /* "Adjacent text nodes in the sequence are merged into a single text
             * node." */
            if(previousIsText && currentIsText)
                ;
            else
                result += QLatin1Char(' ');

            result += unit;
        }

        next = it->next();
        previousIsText = currentIsText;
    }

    return AtomicString::fromValue(result);
}

SequenceType::List XSLTSimpleContentConstructor::expectedOperandTypes() const
{
    SequenceType::List result;
    result.append(CommonSequenceTypes::ZeroOrMoreItems);
    return result;
}

SequenceType::Ptr XSLTSimpleContentConstructor::staticType() const
{
    return CommonSequenceTypes::ZeroOrOneString;
}

QT_END_NAMESPACE
