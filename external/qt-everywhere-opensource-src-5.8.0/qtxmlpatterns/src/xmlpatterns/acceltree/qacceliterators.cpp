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

#include <QtDebug>

#include "qacceliterators_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

xsInteger AccelIterator::position() const
{
    return m_position;
}

QXmlNodeModelIndex AccelIterator::current() const
{
    return m_current;
}

QXmlNodeModelIndex FollowingIterator::next()
{
    /* "the following axis contains all nodes that are descendants
     *  of the root of the tree in which the context node is found,
     *  are not descendants of the context node, and occur after
     *  the context node in document order." */

    if(m_position == 0)
    {
        /* Skip the descendants. */
        m_currentPre += m_document->size(m_preNumber) + 1;
    }

    if(m_currentPre > m_document->maximumPreNumber())
        return closedExit();

    while(m_document->kind(m_currentPre) == QXmlNodeModelIndex::Attribute)
    {
        ++m_currentPre;
        if(m_currentPre > m_document->maximumPreNumber())
            return closedExit();
    }

    m_current = m_document->createIndex(m_currentPre);
    ++m_position;
    ++m_currentPre;
    return m_current;
}

QXmlNodeModelIndex PrecedingIterator::next()
{
    if(m_currentPre == -1)
        return closedExit();

    /* We skip ancestors and attributes and take into account that they can be intermixed. If one
     * skips them in two separate loops, one can end up with skipping all the attributes to then
     * be positioned at an ancestor(which will be accepted because the ancestor loop was before the
     * attributes loop).  */
    while(m_document->kind(m_currentPre) == QXmlNodeModelIndex::Attribute ||
          m_document->postNumber(m_currentPre) > m_postNumber)
    {
        --m_currentPre;
        if(m_currentPre == -1)
            return closedExit();
    }

    if(m_currentPre == -1)
    {
        m_currentPre = -1;
        return closedExit();
    }

    /* Phew, m_currentPre is now 1) not an ancestor; and
     * 2) not an attribute; and 3) preceds the context node. */

    m_current = m_document->createIndex(m_currentPre);
    ++m_position;
    --m_currentPre;

    return m_current;
}

QXmlNodeModelIndex::Iterator::Ptr PrecedingIterator::copy() const
{
    return QXmlNodeModelIndex::Iterator::Ptr(new PrecedingIterator(m_document, m_preNumber));
}

QXmlNodeModelIndex::Iterator::Ptr FollowingIterator::copy() const
{
    return QXmlNodeModelIndex::Iterator::Ptr(new FollowingIterator(m_document, m_preNumber));
}

QXmlNodeModelIndex ChildIterator::next()
{
    if(m_currentPre == -1)
        return closedExit();

    ++m_position;
    m_current = m_document->createIndex(m_currentPre);

    /* We get the count of the descendants, and increment m_currentPre. After
     * this, m_currentPre is the node after the descendants. */
    m_currentPre += m_document->size(m_currentPre);
    ++m_currentPre;

    if(m_currentPre > m_document->maximumPreNumber() || m_document->depth(m_currentPre) != m_depth)
        m_currentPre = -1;

    return m_current;
}

QXmlNodeModelIndex::Iterator::Ptr ChildIterator::copy() const
{
    return QXmlNodeModelIndex::Iterator::Ptr(new ChildIterator(m_document, m_preNumber));
}

QXmlNodeModelIndex AttributeIterator::next()
{
    if(m_currentPre == -1)
        return closedExit();
    else
    {
        m_current = m_document->createIndex(m_currentPre);
        ++m_position;

        ++m_currentPre;

        if(m_currentPre > m_document->maximumPreNumber() ||
           m_document->kind(m_currentPre) != QXmlNodeModelIndex::Attribute)
            m_currentPre = -1;

        return m_current;
    }
}

QXmlNodeModelIndex::Iterator::Ptr AttributeIterator::copy() const
{
    return QXmlNodeModelIndex::Iterator::Ptr(new AttributeIterator(m_document, m_preNumber));
}

QT_END_NAMESPACE

