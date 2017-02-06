/****************************************************************************
**
** Copyright (C) 2015 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwaylandquickoutput.h"
#include "qwaylandquickcompositor.h"
#include "qwaylandquickitem_p.h"

QT_BEGIN_NAMESPACE

QWaylandQuickOutput::QWaylandQuickOutput()
    : QWaylandOutput()
    , m_updateScheduled(false)
    , m_automaticFrameCallback(true)
{
}

QWaylandQuickOutput::QWaylandQuickOutput(QWaylandCompositor *compositor, QWindow *window)
    : QWaylandOutput(compositor, window)
    , m_updateScheduled(false)
    , m_automaticFrameCallback(true)
{
}

void QWaylandQuickOutput::initialize()
{
    QWaylandOutput::initialize();

    QQuickWindow *quickWindow = qobject_cast<QQuickWindow *>(window());
    if (!quickWindow) {
        qWarning("Initialization error: Could not locate QQuickWindow on initializing QWaylandQuickOutput %p.\n", this);
        return;
    }
    connect(quickWindow, &QQuickWindow::beforeSynchronizing,
            this, &QWaylandQuickOutput::updateStarted,
            Qt::DirectConnection);

    connect(quickWindow, &QQuickWindow::beforeRendering,
            this, &QWaylandQuickOutput::doFrameCallbacks);
}

void QWaylandQuickOutput::update()
{
    if (!m_updateScheduled) {
        //don't qobject_cast since we have verified the type in initialize
        static_cast<QQuickWindow *>(window())->update();
        m_updateScheduled = true;
    }
}

/*!
 * \qmlproperty bool QtWaylandCompositor::WaylandOutput::automaticFrameCallback
 *
 * This property holds whether the WaylandOutput automatically sends frame
 * callbacks when rendering.
 *
 * The default is true.
 */
bool QWaylandQuickOutput::automaticFrameCallback() const
{
    return m_automaticFrameCallback;
}

void QWaylandQuickOutput::setAutomaticFrameCallback(bool automatic)
{
    if (m_automaticFrameCallback == automatic)
        return;

    m_automaticFrameCallback = automatic;
    automaticFrameCallbackChanged();
}

static QQuickItem* clickableItemAtPosition(QQuickItem *rootItem, const QPointF &position)
{
    if (!rootItem->isEnabled() || !rootItem->isVisible())
        return nullptr;

    QList<QQuickItem *> paintOrderItems = QQuickItemPrivate::get(rootItem)->paintOrderChildItems();
    auto negativeZStart = paintOrderItems.crend();
    for (auto it = paintOrderItems.crbegin(); it != paintOrderItems.crend(); ++it) {
        if ((*it)->z() < 0) {
            negativeZStart = it;
            break;
        }
        QQuickItem *item = clickableItemAtPosition(*it, rootItem->mapToItem(*it, position));
        if (item)
            return item;
    }

    if (rootItem->contains(position) && rootItem->acceptedMouseButtons() != Qt::NoButton)
        return rootItem;

    for (auto it = negativeZStart; it != paintOrderItems.crend(); ++it) {
        QQuickItem *item = clickableItemAtPosition(*it, rootItem->mapToItem(*it, position));
        if (item)
            return item;
    }

    return nullptr;
}

QQuickItem *QWaylandQuickOutput::pickClickableItem(const QPointF &position)
{
    QQuickWindow *quickWindow = qobject_cast<QQuickWindow *>(window());
    if (!quickWindow)
        return nullptr;

    return clickableItemAtPosition(quickWindow->contentItem(), position);
}

/*!
 * \internal
 */
void QWaylandQuickOutput::updateStarted()
{
    m_updateScheduled = false;

    if (!compositor())
        return;

    frameStarted();
}

void QWaylandQuickOutput::doFrameCallbacks()
{
    if (m_automaticFrameCallback)
        sendFrameCallbacks();
}
QT_END_NAMESPACE
