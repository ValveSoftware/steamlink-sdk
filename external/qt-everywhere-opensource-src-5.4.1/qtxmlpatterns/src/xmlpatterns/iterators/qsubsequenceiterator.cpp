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

#include "qxpathhelper_p.h"

#include "qsubsequenceiterator_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

SubsequenceIterator::SubsequenceIterator(const Item::Iterator::Ptr &iterator,
                                         const xsInteger start,
                                         const xsInteger len)
                                         : m_position(0),
                                           m_it(iterator),
                                           m_counter(start),
                                           m_start(start),
                                           m_len(len),
                                           m_stop(m_start + m_len)
{
    Q_ASSERT(iterator);
    Q_ASSERT(start >= 1);
    Q_ASSERT(len == -1 || len >= 1);

    /* Note, "The first item of a sequence is located at position 1, not position 0." */
    for(xsInteger i = 1; i != m_start; ++i)
        m_it->next();
}

Item SubsequenceIterator::next()
{
    if(m_position == -1)
        return Item();

    m_current = m_it->next();
    ++m_position;

    if(m_len == -1)
    {
        if(!m_current)
            m_position = -1;

        return m_current;
    }

    ++m_counter;

    if(!(m_counter > m_stop) && m_current)
        return m_current;

    m_position = -1;
    m_current.reset();
    return Item();
}

Item SubsequenceIterator::current() const
{
    return m_current;
}

xsInteger SubsequenceIterator::position() const
{
    return m_position;
}

Item::Iterator::Ptr SubsequenceIterator::copy() const
{
    return Item::Iterator::Ptr(new SubsequenceIterator(m_it->copy(), m_start, m_len));
}

QT_END_NAMESPACE
