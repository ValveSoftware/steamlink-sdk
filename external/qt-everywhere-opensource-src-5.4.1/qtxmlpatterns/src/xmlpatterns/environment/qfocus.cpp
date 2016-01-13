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

#include "qdaytimeduration_p.h"

#include "qfocus_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

Focus::Focus(const DynamicContext::Ptr &prevContext) : DelegatingDynamicContext(prevContext),
                                                       m_contextSizeCached(-1)
{
    Q_ASSERT(prevContext);
    Q_ASSERT(prevContext != this);
}

xsInteger Focus::contextPosition() const
{
    Q_ASSERT(m_focusIterator);
    return m_focusIterator->position();
}

Item Focus::contextItem() const
{
    Q_ASSERT(m_focusIterator);
    return m_focusIterator->current();
}

xsInteger Focus::contextSize()
{
    Q_ASSERT(m_focusIterator);
    if(m_contextSizeCached == -1)
        m_contextSizeCached = m_focusIterator->copy()->count();

    Q_ASSERT_X(m_contextSizeCached == m_focusIterator->copy()->count(), Q_FUNC_INFO,
               "If our cache is not the same as the real count, something is wrong.");

    return m_contextSizeCached;
}

void Focus::setFocusIterator(const Item::Iterator::Ptr &it)
{
    Q_ASSERT(it);
    m_focusIterator = it;
}

Item::Iterator::Ptr Focus::focusIterator() const
{
    return m_focusIterator;
}

Item Focus::currentItem() const
{
    /* In the case that there is no top level expression that creates a focus,
     * fn:current() should return the focus. This logic achieves this.
     * Effectively we traverse up our "context stack" through recursion, and
     * start returning when we've found the top most focus. */

    const Item current(m_prevContext->currentItem());

    if(current.isNull())
        return m_focusIterator->current();
    else
        return current;
}

QT_END_NAMESPACE
