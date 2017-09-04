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

#include "qitem_p.h"

#include "qintersectiterator_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

IntersectIterator::IntersectIterator(const Item::Iterator::Ptr &it1,
                                     const Item::Iterator::Ptr &it2) : m_it1(it1),
                                                                       m_it2(it2),
                                                                       m_position(0),
                                                                       m_node1(m_it1->next()),
                                                                       m_node2(m_it2->next())
{
    Q_ASSERT(m_it1);
    Q_ASSERT(m_it2);
}

Item IntersectIterator::next()
{
    if(!m_node1 || !m_node2)
        return closedExit();

    do
    {
        if(m_node1.asNode().model() == m_node2.asNode().model())
        {
            switch(m_node1.asNode().compareOrder(m_node2.asNode()))
            {
                case QXmlNodeModelIndex::Precedes:
                {
                    m_node1 = m_it1->next();
                    break;
                }
                case QXmlNodeModelIndex::Follows:
                {
                    m_node2 = m_it2->next();
                    break;
                }
                default:
                {
                    m_current = m_node2;
                    m_node1 = m_it1->next();
                    m_node2 = m_it2->next();
                    ++m_position;
                    return m_current;
                }
            }
        }
        else
            m_node2 = m_it2->next();
    }
    while(m_node1 && m_node2);

    return Item();
}

Item IntersectIterator::current() const
{
    return m_current;
}

xsInteger IntersectIterator::position() const
{
    return m_position;
}

Item::Iterator::Ptr IntersectIterator::copy() const
{
    return Item::Iterator::Ptr(new IntersectIterator(m_it1->copy(), m_it2->copy()));
}

QT_END_NAMESPACE
