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

#include "qrangeiterator_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

RangeIterator::RangeIterator(const xsInteger start,
                             const Direction direction,
                             const xsInteger end)
                             : m_start(start),
                               m_end(end),
                               m_position(0),
                               m_count(start),
                               m_direction(direction),
                               m_increment(m_direction == Forward ? 1 : -1)
{
    Q_ASSERT(m_start < m_end);
    Q_ASSERT(m_direction == Backward || m_direction == Forward);

    if(m_direction == Backward)
    {
        qSwap(m_start, m_end);
        m_count = m_start;
    }
}

Item RangeIterator::next()
{
    if(m_position == -1)
        return Item();
    else if((m_direction == Forward && m_count > m_end) ||
            (m_direction == Backward && m_count < m_end))
    {
        m_position = -1;
        m_current.reset();
        return Item();
    }
    else
    {
        m_current = Integer::fromValue(m_count);
        m_count += m_increment;
        ++m_position;
        return m_current;
    }
}

xsInteger RangeIterator::count()
{
    /* This complication is for handling that m_start & m_end may be reversed. */
    xsInteger ret;

    if(m_start < m_end)
        ret = m_end - m_start;
    else
        ret = m_start - m_end;

    return ret + 1;
}

Item::Iterator::Ptr RangeIterator::toReversed()
{
    return Item::Iterator::Ptr(new RangeIterator(m_start, Backward, m_end));
}

Item RangeIterator::current() const
{
    return m_current;
}

xsInteger RangeIterator::position() const
{
    return m_position;
}

Item::Iterator::Ptr RangeIterator::copy() const
{
    if(m_direction == Backward)
        return Item::Iterator::Ptr(new RangeIterator(m_end, Backward, m_start));
    else
        return Item::Iterator::Ptr(new RangeIterator(m_start, Forward, m_end));
}

QT_END_NAMESPACE
