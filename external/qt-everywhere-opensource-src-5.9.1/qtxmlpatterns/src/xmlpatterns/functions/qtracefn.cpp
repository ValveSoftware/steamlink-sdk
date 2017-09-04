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

#include "qcommonsequencetypes_p.h"
#include "qcommonvalues_p.h"
#include "qitemmappingiterator_p.h"
#include "qpatternistlocale_p.h"

#include "qtracefn_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

namespace QPatternist
{
    /**
     * @short TraceCallback is a MappingCallback and takes care of
     * the tracing of each individual item.
     *
     * Because Patternist must be thread safe, TraceFN creates a TraceCallback
     * each time the function is evaluated. In other words, TraceFN, which is
     * an Expression sub class, can't modify its members, but MappingCallback
     * does not have this limitation since it's created on a per evaluation basis.
     *
     * @author Frans Englich <frans.englich@nokia.com>
     */
    class TraceCallback : public QSharedData
    {
    public:
        typedef QExplicitlySharedDataPointer<TraceCallback> Ptr;

        inline TraceCallback(const QString &msg) : m_position(0),
                                                   m_msg(msg)
        {
        }

        /**
         * Performs the actual tracing.
         */
        Item mapToItem(const Item &item,
                            const DynamicContext::Ptr &context)
        {
            QTextStream out(stderr);
            ++m_position;
            if(m_position == 1)
            {
                if(item)
                {
                    out << qPrintable(m_msg)
                        << " : "
                        << qPrintable(item.stringValue());
                }
                else
                {
                    out << qPrintable(m_msg)
                        << " : ("
                        << qPrintable(formatType(context->namePool(), CommonSequenceTypes::Empty))
                        << ")\n";
                    return Item();
                }
            }
            else
            {
                out << qPrintable(item.stringValue())
                    << '['
                    << m_position
                    << "]\n";
            }

            return item;
        }

    private:
        xsInteger m_position;
        const QString m_msg;
    };
}

Item::Iterator::Ptr TraceFN::evaluateSequence(const DynamicContext::Ptr &context) const
{
    const QString msg(m_operands.last()->evaluateSingleton(context).stringValue());

    return makeItemMappingIterator<Item>(TraceCallback::Ptr(new TraceCallback(msg)),
                                              m_operands.first()->evaluateSequence(context),
                                              context);
}

Item TraceFN::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const QString msg(m_operands.last()->evaluateSingleton(context).stringValue());
    const Item item(m_operands.first()->evaluateSingleton(context));

    return TraceCallback::Ptr(new TraceCallback(msg))->mapToItem(item, context);
}

SequenceType::Ptr TraceFN::staticType() const
{
    return m_operands.first()->staticType();
}

QT_END_NAMESPACE
