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


#include "qdistinctiterator_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

DistinctIterator::DistinctIterator(const Item::Iterator::Ptr &seq,
                                   const AtomicComparator::Ptr &comp,
                                   const Expression::ConstPtr &expression,
                                   const DynamicContext::Ptr &context)
                                  : m_seq(seq)
                                  , m_context(context)
                                  , m_expr(expression)
                                  , m_position(0)
{
    Q_ASSERT(seq);
    prepareComparison(comp);
}

Item DistinctIterator::next()
{
    if(m_position == -1)
        return Item();

    const Item nitem(m_seq->next());
    if(!nitem)
    {
        m_position = -1;
        m_current.reset();
        return Item();
    }

    const Item::List::const_iterator end(m_processed.constEnd());
    Item::List::const_iterator it(m_processed.constBegin());

    for(; it != end; ++it)
    {
        if(flexibleCompare(*it, nitem, m_context))
        {
            return next();
        }
    }

    m_current = nitem;
    ++m_position;
    m_processed.append(nitem);
    return nitem;
}

Item DistinctIterator::current() const
{
    return m_current;
}

xsInteger DistinctIterator::position() const
{
    return m_position;
}

Item::Iterator::Ptr DistinctIterator::copy() const
{
    return Item::Iterator::Ptr(new DistinctIterator(m_seq->copy(), comparator(), m_expr, m_context));
}

const SourceLocationReflection *DistinctIterator::actualReflection() const
{
    return m_expr.data();
}

QT_END_NAMESPACE
