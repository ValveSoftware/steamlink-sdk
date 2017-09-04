/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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

#include "quick3dnodev9_p.h"
#include <QtQml/QJSValueIterator>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {
namespace Quick {

Quick3DNodeV9::Quick3DNodeV9(QObject *parent)
    : QObject(parent)
{
}

/*!
    \qmlproperty QJSValue Qt3DCore::Node::propertyTrackingOverrides
    \default {}

    Assuming a Qt3DCore::Node needs to override the PropertyTrackingMode on two
    properties (enabled and displacement), the value should be set as shown
    below.

    \badcode
    propertyTrackingOverrides:  {
        "enabled": Entity.DontTrackValues,
        "displacement": Entity.TrackFinalValues
    }
    \endcode

    \since 2.9
*/

QJSValue Quick3DNodeV9::propertyTrackingOverrides() const
{
    return m_propertyTrackingOverrides;
}

void Quick3DNodeV9::setPropertyTrackingOverrides(const QJSValue &value)
{
    m_propertyTrackingOverrides = value;

    QNode *parentNode = this->parentNode();
    parentNode->clearPropertyTrackings();

    if (value.isObject()) {
        QJSValueIterator it(value);
        while (it.hasNext()) {
            it.next();
            parentNode->setPropertyTracking(it.name(), static_cast<QNode::PropertyTrackingMode>(it.value().toInt()));
        }
    }
    emit propertyTrackingOverridesChanged(value);
}

/*!
    \qmlproperty list<QtQml::QtObject> Qt3DCore::Node::data
    \default
*/

QQmlListProperty<QObject> Quick3DNodeV9::data()
{
    return QQmlListProperty<QObject>(this, 0,
                                     Quick3DNodeV9::appendData,
                                     Quick3DNodeV9::dataCount,
                                     Quick3DNodeV9::dataAt,
                                     Quick3DNodeV9::clearData);
}

/*!
    \qmlproperty list<Node> Qt3DCore::Node::childNodes
    \readonly
*/

QQmlListProperty<QNode> Quick3DNodeV9::childNodes()
{
    return QQmlListProperty<QNode>(this, 0,
                                   Quick3DNodeV9::appendChild,
                                   Quick3DNodeV9::childCount,
                                   Quick3DNodeV9::childAt,
                                   Quick3DNodeV9::clearChildren);
}

void Quick3DNodeV9::appendData(QQmlListProperty<QObject> *list, QObject *obj)
{
    if (!obj)
        return;

    Quick3DNodeV9 *self = static_cast<Quick3DNodeV9 *>(list->object);
    self->childAppended(0, obj);
}

QObject *Quick3DNodeV9::dataAt(QQmlListProperty<QObject> *list, int index)
{
    Quick3DNodeV9 *self = static_cast<Quick3DNodeV9 *>(list->object);
    return self->parentNode()->children().at(index);
}

int Quick3DNodeV9::dataCount(QQmlListProperty<QObject> *list)
{
    Quick3DNodeV9 *self = static_cast<Quick3DNodeV9 *>(list->object);
    return self->parentNode()->children().count();
}

void Quick3DNodeV9::clearData(QQmlListProperty<QObject> *list)
{
    Quick3DNodeV9 *self = static_cast<Quick3DNodeV9 *>(list->object);
    for (QObject *const child : self->parentNode()->children())
        self->childRemoved(0, child);
}

void Quick3DNodeV9::appendChild(QQmlListProperty<Qt3DCore::QNode> *list, Qt3DCore::QNode *obj)
{
    if (!obj)
        return;

    Quick3DNodeV9 *self = static_cast<Quick3DNodeV9 *>(list->object);
    Q_ASSERT(!self->parentNode()->children().contains(obj));

    self->childAppended(0, obj);
}

Qt3DCore::QNode *Quick3DNodeV9::childAt(QQmlListProperty<Qt3DCore::QNode> *list, int index)
{
    Quick3DNodeV9 *self = static_cast<Quick3DNodeV9 *>(list->object);
    return qobject_cast<QNode *>(self->parentNode()->children().at(index));
}

int Quick3DNodeV9::childCount(QQmlListProperty<Qt3DCore::QNode> *list)
{
    Quick3DNodeV9 *self = static_cast<Quick3DNodeV9 *>(list->object);
    return self->parentNode()->children().count();
}

void Quick3DNodeV9::clearChildren(QQmlListProperty<Qt3DCore::QNode> *list)
{
    Quick3DNodeV9 *self = static_cast<Quick3DNodeV9 *>(list->object);
    for (QObject *const child : self->parentNode()->children())
        self->childRemoved(0, child);
}

void Quick3DNodeV9::childAppended(int, QObject *obj)
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

void Quick3DNodeV9::childRemoved(int, QObject *obj)
{
    if (QNode *n = qobject_cast<QNode *>(obj))
        n->setParent(Q_NODE_NULLPTR);
    else
        obj->setParent(nullptr);
}


} // namespace Quick
} // namespace Qt3DCore

QT_END_NAMESPACE
