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

#include "qinteger_p.h"

#include "qindexofiterator_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

IndexOfIterator::IndexOfIterator(const Item::Iterator::Ptr &seq,
                                 const Item &searchParam,
                                 const AtomicComparator::Ptr &comp,
                                 const DynamicContext::Ptr &context,
                                 const Expression::ConstPtr &expr)
                                : m_seq(seq)
                                , m_searchParam(searchParam)
                                , m_context(context)
                                , m_expr(expr)
                                , m_position(0)
                                , m_seqPos(0)
{
    Q_ASSERT(seq);
    Q_ASSERT(searchParam);
    prepareComparison(comp);
}

Item IndexOfIterator::next()
{
    if(m_position == -1)
        return Item();

    const Item item(m_seq->next());
    ++m_seqPos;

    if(!item)
    {
        m_current.reset();
        m_position = -1;
        return Item();
    }

    if(flexibleCompare(item, m_searchParam, m_context))
    {
        ++m_position;
        return Integer::fromValue(m_seqPos);
    }

    return next();
}

Item IndexOfIterator::current() const
{
    return m_current;
}

xsInteger IndexOfIterator::position() const
{
    return m_position;
}

Item::Iterator::Ptr IndexOfIterator::copy() const
{
    return Item::Iterator::Ptr(new IndexOfIterator(m_seq->copy(),
                                                   m_searchParam,
                                                   comparator(),
                                                   m_context,
                                                   m_expr));
}

const SourceLocationReflection *IndexOfIterator::actualReflection() const
{
    return m_expr.data();
}

QT_END_NAMESPACE
