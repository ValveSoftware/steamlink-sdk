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

#include "qatomictype_p.h"

#include "qmultiitemtype_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

MultiItemType::MultiItemType(const ItemType::List &list) : m_types(list),
                                                           m_end(list.constEnd())
{
    Q_ASSERT_X(list.count() >= 2, Q_FUNC_INFO,
               "It makes no sense to use MultiItemType for types less than two.");
    Q_ASSERT_X(list.count(ItemType::Ptr()) == 0, Q_FUNC_INFO,
               "No member in the list can be null.");
}

QString MultiItemType::displayName(const NamePool::Ptr &np) const
{
    QString result;
    ItemType::List::const_iterator it(m_types.constBegin());

    while(true)
    {
        result += (*it)->displayName(np);
        ++it;

        if(it != m_end)
            result += QLatin1String(" | ");
        else
            break;
    }

    return result;
}

bool MultiItemType::itemMatches(const Item &item) const
{
    for(ItemType::List::const_iterator it(m_types.constBegin()); it != m_end; ++it)
        if((*it)->itemMatches(item))
            return true;

    return false;
}

bool MultiItemType::xdtTypeMatches(const ItemType::Ptr &type) const
{
    for(ItemType::List::const_iterator it(m_types.constBegin()); it != m_end; ++it)
        if((*it)->xdtTypeMatches(type))
            return true;

    return false;
}

bool MultiItemType::isNodeType() const
{
    for(ItemType::List::const_iterator it(m_types.constBegin()); it != m_end; ++it)
        if((*it)->isNodeType())
            return true;

    return false;
}

bool MultiItemType::isAtomicType() const
{
    for(ItemType::List::const_iterator it(m_types.constBegin()); it != m_end; ++it)
        if((*it)->isAtomicType())
            return true;

    return false;
}

ItemType::Ptr MultiItemType::xdtSuperType() const
{
    ItemType::List::const_iterator it(m_types.constBegin());
    /* Load the first one, and jump over it in the loop. */
    ItemType::Ptr result((*it)->xdtSuperType());
    ++it;

    for(; it != m_end; ++it)
        result |= (*it)->xdtSuperType();

    return result;
}

ItemType::Ptr MultiItemType::atomizedType() const
{
    ItemType::List::const_iterator it(m_types.constBegin());
    /* Load the first one, and jump over it in the loop. */
    ItemType::Ptr result((*it)->atomizedType());
    ++it;

    for(; it != m_end; ++it)
        result |= (*it)->atomizedType();

    return result;
}

QT_END_NAMESPACE
