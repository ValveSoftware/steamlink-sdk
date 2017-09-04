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


#include "qinsertioniterator_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

InsertionIterator::InsertionIterator(const Item::Iterator::Ptr &target,
                                     const xsInteger pos,
                                     const Item::Iterator::Ptr &inserts)
                                    : m_target(target),
                                      m_insertPos(pos),
                                      m_inserts(inserts),
                                      m_position(0),
                                      m_isInserting(pos == 1)
{
    Q_ASSERT(target);
    Q_ASSERT(inserts);
    Q_ASSERT(m_insertPos >= 1);
}

Item InsertionIterator::next()
{
    if(m_isInserting)
    {
        m_current = m_inserts->next();

        if(m_current)
        {
            ++m_position;
            return m_current;
        }
    }
    else if(m_position == (m_insertPos - 1) && !m_isInserting)
    { /* Entered only the first time insertion starts. */
        m_isInserting = true;
        return next();
    }

    ++m_position;
    m_current = m_target->next();

    if(m_current)
        return m_current;
    else if(m_inserts->position() == -1) /* We're at the end of the both iterators. */
    {
        m_position = -1;
        m_current.reset();
        return Item();
    }

    /* Insert the insertion iterator, since it's still left. */
    Q_ASSERT(m_target->position() < m_insertPos);
    m_isInserting = true;
    m_current = m_inserts->next();

    if(m_current)
        return m_current;
    else
    {
        /* m_current is already null, so no need to reset it. */
        m_position = -1;
        return Item();
    }
}

xsInteger InsertionIterator::count()
{
    return m_target->count() + m_inserts->count();
}

Item InsertionIterator::current() const
{
    return m_current;
}

xsInteger InsertionIterator::position() const
{
    return m_position;
}

Item::Iterator::Ptr InsertionIterator::copy() const
{
    return Item::Iterator::Ptr(new InsertionIterator(m_target->copy(), m_insertPos, m_inserts->copy()));
}

QT_END_NAMESPACE
