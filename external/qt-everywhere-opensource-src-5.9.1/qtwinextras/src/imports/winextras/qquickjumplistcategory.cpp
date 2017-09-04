/****************************************************************************
 **
 ** Copyright (C) 2016 Ivan Vizir <define-true-false@yandex.com>
 ** Copyright (C) 2016 The Qt Company Ltd.
 ** Contact: https://www.qt.io/licensing/
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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
    for (QQuickJumpListItem *item : m_items)
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
