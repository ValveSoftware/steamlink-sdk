/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QList>
#include <QPointer>
#include <QVariant>

#include "Global.h"

#include "qitem_p.h"
#include "qreportcontext_p.h"

#include "XDTItemItem.h"

using namespace QPatternistSDK;

XDTItemItem::XDTItemItem(const QPatternist::Item &item,
                         XDTItemItem *p) : m_item(item),
                                           m_parent(p)
{
}

XDTItemItem::~XDTItemItem()
{
    qDeleteAll(m_children);
}

int XDTItemItem::columnCount() const
{
    return 3;
}

QVariant XDTItemItem::data(const Qt::ItemDataRole role, int column) const
{
    Q_ASSERT(m_item);
    if(role != Qt::DisplayRole)
        return QVariant();

    switch(column)
    {
        case 0:
            /* + 1: We don't want the index; the XDT counts from 1. */
            return row() + 1;
        case 1:
            return m_item.stringValue();
        case 2:
            return m_item.type()->displayName(Global::namePool());
        default:
        {
            Q_ASSERT(false);
            return QString();
        }
    }
}

TreeItem::List XDTItemItem::children() const
{
    return m_children;
}

void XDTItemItem::appendChild(TreeItem *item)
{
    m_children.append(item);
}

TreeItem *XDTItemItem::child(const unsigned int rowP) const
{
    return m_children.value(rowP);
}

unsigned int XDTItemItem::childCount() const
{
    return m_children.count();
}

TreeItem *XDTItemItem::parent() const
{
    return m_parent;
}

// vim: et:ts=4:sw=4:sts=4
