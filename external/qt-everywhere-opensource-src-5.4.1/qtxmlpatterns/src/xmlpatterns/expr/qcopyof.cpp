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

#include "qbuiltintypes_p.h"
#include "qcommonsequencetypes_p.h"
#include "qitemmappingiterator_p.h"

#include "qcopyof_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

CopyOf::CopyOf(const Expression::Ptr &operand,
               const bool inheritNSS,
               const bool preserveNSS) : SingleContainer(operand)
                                       , m_inheritNamespaces(inheritNSS)
                                       , m_preserveNamespaces(preserveNSS)
                                       , m_settings((m_inheritNamespaces ? QAbstractXmlNodeModel::InheritNamespaces : QAbstractXmlNodeModel::NodeCopySettings()) |
                                                    (m_preserveNamespaces ? QAbstractXmlNodeModel::PreserveNamespaces : QAbstractXmlNodeModel::NodeCopySettings()))
{
}

Expression::Ptr CopyOf::compress(const StaticContext::Ptr &context)
{
    /* We have zero effect if we have these settings. */
    if(m_inheritNamespaces && m_preserveNamespaces)
        return m_operand->compress(context);
    else
    {
        const ItemType::Ptr t(m_operand->staticType()->itemType());
        /* We have no effect on the empty sequence or atomic values. */
        if(BuiltinTypes::xsAnyAtomicType->xdtTypeMatches(t)
          || *t == *CommonSequenceTypes::Empty)
            return m_operand->compress(context);
        else
            return SingleContainer::compress(context);
    }
}

void CopyOf::evaluateToSequenceReceiver(const DynamicContext::Ptr &context) const
{
    /* Optimization: this completely breaks streaming. We get a call to
     * evaluateToSequenceReceiver() but we require heap allocations by calling
     * evaluateSequence(). */

    const Item::Iterator::Ptr it(m_operand->evaluateSequence(context));
    QAbstractXmlReceiver *const receiver = context->outputReceiver();
    Item next(it->next());

    while(next)
    {
        if(next.isNode())
        {
            const QXmlNodeModelIndex &asNode = next.asNode();
            asNode.model()->copyNodeTo(asNode, receiver, m_settings);
        }
        else
            receiver->item(next);

        next = it->next();
    }
}

ExpressionVisitorResult::Ptr CopyOf::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

SequenceType::Ptr CopyOf::staticType() const
{
    return m_operand->staticType();
}

SequenceType::List CopyOf::expectedOperandTypes() const
{
    SequenceType::List result;
    result.append(CommonSequenceTypes::ZeroOrMoreItems);
    return result;
}

Expression::Properties CopyOf::properties() const
{
    /* We have the content of node constructors as children, but even though
     * createCopyOf() typically avoids creating us, we can still end up with an operand
     * that allows compression. We must always avoid that, because we don't have
     * implementation of evaluateSequence(), and so on. */
    return (m_operand->properties() & ~CreatesFocusForLast) | DisableElimination;
}

ItemType::Ptr CopyOf::expectedContextItemType() const
{
    return m_operand->expectedContextItemType();
}

QT_END_NAMESPACE
