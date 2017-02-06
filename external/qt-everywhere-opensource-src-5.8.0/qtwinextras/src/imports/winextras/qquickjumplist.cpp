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

#include "qquickjumplist_p.h"
#include "qquickjumplistcategory_p.h"
#include <QWinJumpList>

QT_BEGIN_NAMESPACE

/*!
    \qmltype JumpList
    \instantiates QQuickJumpList
    \inqmlmodule QtWinExtras

    \brief Enables you to modify the application Jump List.

    \since QtWinExtras 1.0

    The JumpList type enables you to modify Windows Jump Lists.

    An application can use Jump Lists to provide users with faster access to
    files or to display shortcuts to tasks or commands.
 */

/*!
    \class QQuickJumpList
    \internal
 */

QQuickJumpList::QQuickJumpList(QObject *parent) :
    QObject(parent), m_recent(0), m_frequent(0), m_tasks(0)
{
}

QQuickJumpList::~QQuickJumpList()
{
}

/*!
    \qmlproperty JumpListCategory JumpList::recent
    \readonly

    The recent items category.
 */
QQuickJumpListCategory *QQuickJumpList::recent() const
{
    if (!m_recent) {
        QQuickJumpList *that = const_cast<QQuickJumpList *>(this);
        that->m_recent = new QQuickJumpListCategory(that);
        connect(m_recent, &QQuickJumpListCategory::visibilityChanged, that, &QQuickJumpList::rebuild);
        m_recent->setVisible(false);
    }
    return m_recent;
}

/*!
    \qmlproperty JumpListCategory JumpList::frequent
    \readonly

    The frequent items category.
 */
QQuickJumpListCategory *QQuickJumpList::frequent() const
{
    if (!m_frequent) {
        QQuickJumpList *that = const_cast<QQuickJumpList *>(this);
        that->m_frequent = new QQuickJumpListCategory(that);
        connect(m_frequent, &QQuickJumpListCategory::visibilityChanged, that, &QQuickJumpList::rebuild);
        m_frequent->setVisible(false);
    }
    return m_frequent;
}

/*!
    \qmlproperty JumpListCategory JumpList::tasks

    The tasks category.
 */
QQuickJumpListCategory *QQuickJumpList::tasks() const
{
    if (!m_tasks) {
        QQuickJumpList *that = const_cast<QQuickJumpList *>(this);
        that->m_tasks = new QQuickJumpListCategory(that);
        connect(m_tasks, &QQuickJumpListCategory::visibilityChanged, that, &QQuickJumpList::rebuild);
    }
    return m_tasks;
}

void QQuickJumpList::setTasks(QQuickJumpListCategory *tasks)
{
    if (m_tasks != tasks) {
        if (m_tasks)
            disconnect(m_tasks, &QQuickJumpListCategory::visibilityChanged, this, &QQuickJumpList::rebuild);
        delete m_tasks;
        m_tasks = tasks;
        if (m_tasks)
            connect(m_tasks, &QQuickJumpListCategory::visibilityChanged, this, &QQuickJumpList::rebuild);
        emit tasksChanged();
    }
}

/*!
    \qmlproperty list<QtObject> JumpList::data
    \internal
 */
QQmlListProperty<QObject> QQuickJumpList::data()
{
    return QQmlListProperty<QObject>(this, this, &QQuickJumpList::data_append, 0, 0, 0);
}

/*!
    \qmlproperty list<JumpListCategory> JumpList::categories

    A list of custom categories.
 */
QQmlListProperty<QQuickJumpListCategory> QQuickJumpList::categories()
{
    return QQmlListProperty<QQuickJumpListCategory>(this, this, &QQuickJumpList::categories_count, &QQuickJumpList::categories_at);
}

void QQuickJumpList::classBegin()
{
}

void QQuickJumpList::componentComplete()
{
    rebuild();
}

void QQuickJumpList::rebuild()
{
    QWinJumpList jumpList;
    jumpList.recent()->setVisible(m_recent && m_recent->isVisible());
    jumpList.frequent()->setVisible(m_frequent && m_frequent->isVisible());
    if (m_tasks && m_tasks->isVisible()) {
        jumpList.tasks()->setVisible(true);
        const auto items = m_tasks->toItemList();
        for (QWinJumpListItem *item : items)
            jumpList.tasks()->addItem(item);
    }
    for (QQuickJumpListCategory *category : qAsConst(m_categories)) {
        if (category->isVisible())
            jumpList.addCategory(category->title(), category->toItemList())->setVisible(true);
    }
}

void QQuickJumpList::data_append(QQmlListProperty<QObject> *property, QObject *object)
{
    if (QQuickJumpListCategory *category = qobject_cast<QQuickJumpListCategory *>(object)) {
        QQuickJumpList *jumpList = static_cast<QQuickJumpList *>(property->object);
        connect(category, &QQuickJumpListCategory::visibilityChanged, jumpList, &QQuickJumpList::rebuild);
        jumpList->m_categories.append(category);
        emit jumpList->categoriesChanged();
    }
}

int QQuickJumpList::categories_count(QQmlListProperty<QQuickJumpListCategory> *property)
{
    return static_cast<QQuickJumpList *>(property->object)->m_categories.count();
}

QQuickJumpListCategory *QQuickJumpList::categories_at(QQmlListProperty<QQuickJumpListCategory> *property, int index)
{
    return static_cast<QQuickJumpList *>(property->object)->m_categories.value(index);
}

QT_END_NAMESPACE
