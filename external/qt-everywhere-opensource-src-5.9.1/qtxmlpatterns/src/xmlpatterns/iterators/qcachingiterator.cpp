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



#include "qcachingiterator_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

CachingIterator::CachingIterator(ItemSequenceCacheCell::Vector &cacheCells,
                                 const VariableSlotID slot,
                                 const DynamicContext::Ptr &context) : m_position(0),
                                                                       m_varSlot(slot),
                                                                       m_context(context),
                                                                       m_cacheCells(cacheCells),
                                                                       m_usingCache(true)
{
    Q_ASSERT(m_varSlot > -1);
    Q_ASSERT(m_context);
    Q_ASSERT(m_cacheCells.at(m_varSlot).sourceIterator);
    Q_ASSERT_X((m_cacheCells.at(m_varSlot).cachedItems.isEmpty() && m_cacheCells.at(m_varSlot).cacheState == ItemSequenceCacheCell::Empty) ||
               m_cacheCells.at(m_varSlot).cacheState == ItemSequenceCacheCell::PartiallyPopulated,
               Q_FUNC_INFO,
               "It makes no sense to construct a CachingIterator for a cache that is ItemSequenceCacheCell::Full.");
}

Item CachingIterator::next()
{
    ItemSequenceCacheCell &cell = m_cacheCells[m_varSlot];
    if(m_position == -1)
        return Item();

    if(m_usingCache)
    {
        ++m_position;

        /* QAbstractXmlForwardIterator::position() starts at 1, while Qt's container classes
         * starts at 0. */
        if(m_position - 1 < cell.cachedItems.count())
        {
            m_current = cell.cachedItems.at(m_position - 1);
            return m_current;
        }
        else
        {
            cell.cacheState = ItemSequenceCacheCell::PartiallyPopulated;
            m_usingCache = false;
            /* We decrement here so we don't have to add a branch for this
             * when using the source QAbstractXmlForwardIterator below. */
            --m_position;
        }
    }

    m_current = cell.sourceIterator->next();

    if(m_current)
    {
        cell.cachedItems.append(m_current);
        Q_ASSERT(cell.cacheState == ItemSequenceCacheCell::PartiallyPopulated);
        ++m_position;
        return m_current;
    }
    else
    {
        m_position = -1;
        cell.cacheState = ItemSequenceCacheCell::Full;
        return Item();
    }
}

Item CachingIterator::current() const
{
    return m_current;
}

xsInteger CachingIterator::position() const
{
    return m_position;
}

Item::Iterator::Ptr CachingIterator::copy() const
{
    const ItemSequenceCacheCell &cell = m_cacheCells.at(m_varSlot);
    if(cell.cacheState == ItemSequenceCacheCell::Full)
        return makeListIterator(cell.cachedItems);
    else
        return Item::Iterator::Ptr(new CachingIterator(m_cacheCells, m_varSlot, m_context));
}

QT_END_NAMESPACE
