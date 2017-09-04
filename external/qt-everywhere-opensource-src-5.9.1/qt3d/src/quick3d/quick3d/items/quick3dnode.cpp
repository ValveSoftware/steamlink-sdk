/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "quick3dnode_p.h"

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {
namespace Quick {

/*!
    \qmltype Node
    \inqmlmodule Qt3D.Core
    \since 5.5

    \brief A base QML type that other types inherit. It cannot be directly
    created.
*/

Quick3DNode::Quick3DNode(QObject *parent)
    : QObject(parent)
{
}

/*!
    \qmlproperty list<QtQml::QtObject> Qt3DCore::Node::data
    \default
*/

QQmlListProperty<QObject> Quick3DNode::data()
{
    return QQmlListProperty<QObject>(this, 0,
                                     Quick3DNode::appendData,
                                     Quick3DNode::dataCount,
                                     Quick3DNode::dataAt,
                                     Quick3DNode::clearData);
}

/*!
    \qmlproperty list<Node> Qt3DCore::Node::childNodes
    \readonly
*/

QQmlListProperty<QNode> Quick3DNode::childNodes()
{
    return QQmlListProperty<QNode>(this, 0,
                                  Quick3DNode::appendChild,
                                  Quick3DNode::childCount,
                                  Quick3DNode::childAt,
                                  Quick3DNode::clearChildren);
}

void Quick3DNode::appendData(QQmlListProperty<QObject> *list, QObject *obj)
{
    if (!obj)
        return;

    Quick3DNode *self = static_cast<Quick3DNode *>(list->object);
    self->childAppended(0, obj);
}

QObject *Quick3DNode::dataAt(QQmlListProperty<QObject> *list, int index)
{
    Quick3DNode *self = static_cast<Quick3DNode *>(list->object);
    return self->parentNode()->children().at(index);
}

int Quick3DNode::dataCount(QQmlListProperty<QObject> *list)
{
    Quick3DNode *self = static_cast<Quick3DNode *>(list->object);
    return self->parentNode()->children().count();
}

void Quick3DNode::clearData(QQmlListProperty<QObject> *list)
{
    Quick3DNode *self = static_cast<Quick3DNode *>(list->object);
    for (QObject *const child : self->parentNode()->children())
        self->childRemoved(0, child);
}

void Quick3DNode::appendChild(QQmlListProperty<Qt3DCore::QNode> *list, Qt3DCore::QNode *obj)
{
    if (!obj)
        return;

    Quick3DNode *self = static_cast<Quick3DNode *>(list->object);
    Q_ASSERT(!self->parentNode()->children().contains(obj));

    self->childAppended(0, obj);
}

Qt3DCore::QNode *Quick3DNode::childAt(QQmlListProperty<Qt3DCore::QNode> *list, int index)
{
    Quick3DNode *self = static_cast<Quick3DNode *>(list->object);
    return qobject_cast<QNode *>(self->parentNode()->children().at(index));
}

int Quick3DNode::childCount(QQmlListProperty<Qt3DCore::QNode> *list)
{
    Quick3DNode *self = static_cast<Quick3DNode *>(list->object);
    return self->parentNode()->children().count();
}

void Quick3DNode::clearChildren(QQmlListProperty<Qt3DCore::QNode> *list)
{
    Quick3DNode *self = static_cast<Quick3DNode *>(list->object);
    for (QObject *const child : self->parentNode()->children())
        self->childRemoved(0, child);
}

void Quick3DNode::childAppended(int, QObject *obj)
{
    QNode *parentNode = this->parentNode();
    if (obj->parent() == parentNode)
        obj->setParent(0);
    // Set after otherwise addChild might not work
    if (QNode *n = qobject_cast<QNode *>(obj))
        n->setParent(parentNode);
    else
        obj->setParent(parentNode);
}

void Quick3DNode::childRemoved(int, QObject *obj)
{
    if (QNode *n = qobject_cast<QNode *>(obj))
        n->setParent(Q_NODE_NULLPTR);
    else
        obj->setParent(nullptr);
}

} // namespace Quick
} // namespace Qt3DCore

QT_END_NAMESPACE
