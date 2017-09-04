/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#include "qquicksceneposlistener_p.h"
#include <QtQuick/private/qquickitem_p.h>

QT_BEGIN_NAMESPACE

static const QQuickItemPrivate::ChangeTypes AncestorChangeTypes = QQuickItemPrivate::Geometry
                                                                  | QQuickItemPrivate::Parent
                                                                  | QQuickItemPrivate::Children;

static const QQuickItemPrivate::ChangeTypes ItemChangeTypes = QQuickItemPrivate::Geometry
                                                             | QQuickItemPrivate::Parent
                                                             | QQuickItemPrivate::Destroyed;

QQuickScenePosListener1::QQuickScenePosListener1(QObject *parent)
    : QObject(parent)
    , m_enabled(false)
    , m_item(0)
{
}

QQuickScenePosListener1::~QQuickScenePosListener1()
{
    if (m_item == 0)
        return;

    QQuickItemPrivate::get(m_item)->removeItemChangeListener(this, ItemChangeTypes);
    removeAncestorListeners(m_item->parentItem());
}

QQuickItem *QQuickScenePosListener1::item() const
{
    return m_item;
}

void QQuickScenePosListener1::setItem(QQuickItem *item)
{
    if (m_item == item)
        return;

    if (m_item != 0) {
        QQuickItemPrivate::get(m_item)->removeItemChangeListener(this, ItemChangeTypes);
        removeAncestorListeners(m_item->parentItem());
    }

    m_item = item;

    if (m_item == 0)
        return;

    if (m_enabled) {
        QQuickItemPrivate::get(m_item)->addItemChangeListener(this, ItemChangeTypes);
        addAncestorListeners(m_item->parentItem());
    }

    updateScenePos();
}

QPointF QQuickScenePosListener1::scenePos() const
{
    return m_scenePos;
}

bool QQuickScenePosListener1::isEnabled() const
{
    return m_enabled;
}

void QQuickScenePosListener1::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;

    if (m_item != 0) {
        if (enabled) {
            QQuickItemPrivate::get(m_item)->addItemChangeListener(this, ItemChangeTypes);
            addAncestorListeners(m_item->parentItem());
        } else {
            QQuickItemPrivate::get(m_item)->removeItemChangeListener(this, ItemChangeTypes);
            removeAncestorListeners(m_item->parentItem());
        }
    }

    emit enabledChanged();
}

void QQuickScenePosListener1::itemGeometryChanged(QQuickItem *, QQuickGeometryChange, const QRectF &)
{
    updateScenePos();
}

void QQuickScenePosListener1::itemParentChanged(QQuickItem *, QQuickItem *parent)
{
    addAncestorListeners(parent);
}

void QQuickScenePosListener1::itemChildRemoved(QQuickItem *, QQuickItem *child)
{
    if (isAncestor(child))
        removeAncestorListeners(child);
}

void QQuickScenePosListener1::itemDestroyed(QQuickItem *item)
{
    Q_ASSERT(m_item == item);

    // Remove all injected listeners.
    m_item = 0;
    QQuickItemPrivate::get(item)->removeItemChangeListener(this, ItemChangeTypes);
    removeAncestorListeners(item->parentItem());
}

void QQuickScenePosListener1::updateScenePos()
{
    const QPointF &scenePos = m_item->mapToScene(QPointF(0, 0));
    if (m_scenePos != scenePos) {
        m_scenePos = scenePos;
        emit scenePosChanged();
    }
}

/*!
    \internal

    Remove this listener from \a item and all its ancestors.
 */
void QQuickScenePosListener1::removeAncestorListeners(QQuickItem *item)
{
    if (item == m_item)
        return;

    QQuickItem *p = item;
    while (p != 0) {
        QQuickItemPrivate::get(p)->removeItemChangeListener(this, AncestorChangeTypes);
        p = p->parentItem();
    }
}

/*!
    \internal

    Injects this as a listener to \a item and all its ancestors.
 */
void QQuickScenePosListener1::addAncestorListeners(QQuickItem *item)
{
    if (item == m_item)
        return;

    QQuickItem *p = item;
    while (p != 0) {
        QQuickItemPrivate::get(p)->addItemChangeListener(this, AncestorChangeTypes);
        p = p->parentItem();
    }
}

bool QQuickScenePosListener1::isAncestor(QQuickItem *item) const
{
    if (!m_item)
        return false;

    QQuickItem *parent = m_item->parentItem();
    while (parent) {
        if (parent == item)
            return true;
        parent = parent->parentItem();
    }
    return false;
}

QT_END_NAMESPACE
