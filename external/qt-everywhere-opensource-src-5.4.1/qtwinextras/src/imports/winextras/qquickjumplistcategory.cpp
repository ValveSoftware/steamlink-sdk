/****************************************************************************
 **
 ** Copyright (C) 2013 Ivan Vizir <define-true-false@yandex.com>
 ** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
 ** Contact: http://www.qt-project.org/legal
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#include "qquickjumplistcategory_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype JumpListCategory
    \instantiates QQuickJumpListCategory
    \inqmlmodule QtWinExtras

    \brief Represents a jump list category.

    \since QtWinExtras 1.0

    The JumpListCategory type represents a category that consists of several
    Jump List destinations and has a title.
 */

/*!
    \class QQuickJumpListCategory
    \internal
 */

QQuickJumpListCategory::QQuickJumpListCategory(QObject *parent) :
    QObject(parent), m_visible(true)
{
}

QQuickJumpListCategory::~QQuickJumpListCategory()
{
}

/*!
    \qmlproperty list<QtObject> JumpListCategory::data
    \internal
 */
QQmlListProperty<QObject> QQuickJumpListCategory::data()
{
    return QQmlListProperty<QObject>(this, this, &QQuickJumpListCategory::data_append, 0, 0, 0);
}

/*!
    \qmlproperty list<JumpListItem> JumpListCategory::items

    A list of items.
 */
QQmlListProperty<QQuickJumpListItem> QQuickJumpListCategory::items()
{
    return QQmlListProperty<QQuickJumpListItem>(this, this, &QQuickJumpListCategory::items_count, &QQuickJumpListCategory::items_at);
}

/*!
    \qmlproperty string JumpListCategory::title

    The title of the category.
 */
QString QQuickJumpListCategory::title() const
{
    return m_title;
}

void QQuickJumpListCategory::setTitle(const QString &title)
{
    if (m_title != title) {
        m_title = title;
        emit titleChanged();
    }
}

/*!
    \qmlproperty bool JumpListCategory::visible

    The visibility of the category.
 */
bool QQuickJumpListCategory::isVisible() const
{
    return m_visible;
}

void QQuickJumpListCategory::setVisible(bool visible)
{
    if (m_visible != visible) {
        m_visible = visible;
        emit visibilityChanged();
    }
}

QList<QWinJumpListItem *> QQuickJumpListCategory::toItemList() const
{
    QList<QWinJumpListItem *> items;
    foreach (QQuickJumpListItem *item, m_items)
        items.append(item->toJumpListItem());
    return items;
}

void QQuickJumpListCategory::data_append(QQmlListProperty<QObject> *property, QObject *object)
{
    if (QQuickJumpListItem *item = qobject_cast<QQuickJumpListItem *>(object)) {
        QQuickJumpListCategory *category = static_cast<QQuickJumpListCategory *>(property->object);
        category->m_items.append(item);
        emit category->itemsChanged();
    }
}

int QQuickJumpListCategory::items_count(QQmlListProperty<QQuickJumpListItem> *property)
{
    return static_cast<QQuickJumpListCategory *>(property->object)->m_items.count();
}

QQuickJumpListItem *QQuickJumpListCategory::items_at(QQmlListProperty<QQuickJumpListItem> *property, int index)
{
    return static_cast<QQuickJumpListCategory *>(property->object)->m_items.value(index);
}

QT_END_NAMESPACE
