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
        connect(m_recent, SIGNAL(visibilityChanged()), that, SLOT(rebuild()));
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
        connect(m_frequent, SIGNAL(visibilityChanged()), that, SLOT(rebuild()));
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
        connect(m_tasks, SIGNAL(visibilityChanged()), that, SLOT(rebuild()));
    }
    return m_tasks;
}

void QQuickJumpList::setTasks(QQuickJumpListCategory *tasks)
{
    if (m_tasks != tasks) {
        if (m_tasks)
            disconnect(m_tasks, SIGNAL(visibilityChanged()), this, SLOT(rebuild()));
        delete m_tasks;
        m_tasks = tasks;
        if (m_tasks)
            connect(m_tasks, SIGNAL(visibilityChanged()), this, SLOT(rebuild()));
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
        foreach (QWinJumpListItem *item, m_tasks->toItemList())
            jumpList.tasks()->addItem(item);
    }
    foreach (QQuickJumpListCategory *category, m_categories) {
        if (category->isVisible())
            jumpList.addCategory(category->title(), category->toItemList())->setVisible(true);
    }
}

void QQuickJumpList::data_append(QQmlListProperty<QObject> *property, QObject *object)
{
    if (QQuickJumpListCategory *category = qobject_cast<QQuickJumpListCategory *>(object)) {
        QQuickJumpList *jumpList = static_cast<QQuickJumpList *>(property->object);
        connect(category, SIGNAL(visibilityChanged()), jumpList, SLOT(rebuild()));
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
