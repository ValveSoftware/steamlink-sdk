/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "qwaylandxdgshellv5integration_p.h"

#include <QtWaylandCompositor/QWaylandQuickShellSurfaceItem>
#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandSeat>
#include <QtWaylandCompositor/private/qwaylandxdgshellv5_p.h>
#include <QMouseEvent>
#include <QGuiApplication>

QT_BEGIN_NAMESPACE

namespace QtWayland {

XdgShellV5Integration::XdgShellV5Integration(QWaylandQuickShellSurfaceItem *item)
    : QWaylandQuickShellIntegration(item)
    , m_item(item)
    , m_xdgSurface(qobject_cast<QWaylandXdgSurfaceV5 *>(item->shellSurface()))
    , grabberState(GrabberState::Default)
{
    m_item->setSurface(m_xdgSurface->surface());
    connect(m_xdgSurface, &QWaylandXdgSurfaceV5::startMove, this, &XdgShellV5Integration::handleStartMove);
    connect(m_xdgSurface, &QWaylandXdgSurfaceV5::startResize, this, &XdgShellV5Integration::handleStartResize);
    connect(m_xdgSurface, &QWaylandXdgSurfaceV5::setTopLevel, this, &XdgShellV5Integration::handleSetTopLevel);
    connect(m_xdgSurface, &QWaylandXdgSurfaceV5::setTransient, this, &XdgShellV5Integration::handleSetTransient);
    connect(m_xdgSurface, &QWaylandXdgSurfaceV5::setMaximized, this, &XdgShellV5Integration::handleSetMaximized);
    connect(m_xdgSurface, &QWaylandXdgSurfaceV5::unsetMaximized, this, &XdgShellV5Integration::handleUnsetMaximized);
    connect(m_xdgSurface, &QWaylandXdgSurfaceV5::maximizedChanged, this, &XdgShellV5Integration::handleMaximizedChanged);
    connect(m_xdgSurface, &QWaylandXdgSurfaceV5::activatedChanged, this, &XdgShellV5Integration::handleActivatedChanged);
    connect(m_xdgSurface->surface(), &QWaylandSurface::sizeChanged, this, &XdgShellV5Integration::handleSurfaceSizeChanged);
}

bool XdgShellV5Integration::mouseMoveEvent(QMouseEvent *event)
{
    if (grabberState == GrabberState::Resize) {
        Q_ASSERT(resizeState.seat == m_item->compositor()->seatFor(event));
        if (!resizeState.initialized) {
            resizeState.initialMousePos = event->windowPos();
            resizeState.initialized = true;
            return true;
        }
        QPointF delta = m_item->mapToSurface(event->windowPos() - resizeState.initialMousePos);
        QSize newSize = m_xdgSurface->sizeForResize(resizeState.initialWindowSize, delta, resizeState.resizeEdges);
        m_xdgSurface->sendResizing(newSize);
    } else if (grabberState == GrabberState::Move) {
        Q_ASSERT(moveState.seat == m_item->compositor()->seatFor(event));
        QQuickItem *moveItem = m_item->moveItem();
        if (!moveState.initialized) {
            moveState.initialOffset = moveItem->mapFromItem(nullptr, event->windowPos());
            moveState.initialized = true;
            return true;
        }
        if (!moveItem->parentItem())
            return true;
        QPointF parentPos = moveItem->parentItem()->mapFromItem(nullptr, event->windowPos());
        moveItem->setPosition(parentPos - moveState.initialOffset);
    }
    return false;
}

bool XdgShellV5Integration::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);

    if (grabberState == GrabberState::Resize) {
        m_xdgSurface->sendUnmaximized();
        grabberState = GrabberState::Default;
        return true;
    } else if (grabberState == GrabberState::Move) {
        grabberState = GrabberState::Default;
        return true;
    }
    return false;
}

void XdgShellV5Integration::handleStartMove(QWaylandSeat *seat)
{
    grabberState = GrabberState::Move;
    moveState.seat = seat;
    moveState.initialized = false;
}

void XdgShellV5Integration::handleStartResize(QWaylandSeat *seat, QWaylandXdgSurfaceV5::ResizeEdge edges)
{
    grabberState = GrabberState::Resize;
    resizeState.seat = seat;
    resizeState.resizeEdges = edges;
    resizeState.initialWindowSize = m_xdgSurface->windowGeometry().size();
    resizeState.initialPosition = m_item->moveItem()->position();
    resizeState.initialSurfaceSize = m_item->surface()->size();
    resizeState.initialized = false;
}

void XdgShellV5Integration::handleSetTopLevel()
{
    if (m_xdgSurface->shell()->focusPolicy() == QWaylandShell::AutomaticFocus)
        m_item->takeFocus();
}

void XdgShellV5Integration::handleSetTransient()
{
    if (m_xdgSurface->shell()->focusPolicy() == QWaylandShell::AutomaticFocus)
        m_item->takeFocus();
}

void XdgShellV5Integration::handleSetMaximized()
{
    if (!m_item->view()->isPrimary())
        return;

    maximizeState.initialWindowSize = m_xdgSurface->windowGeometry().size();
    maximizeState.initialPosition = m_item->moveItem()->position();

    QWaylandOutput *output = m_item->view()->output();
    m_xdgSurface->sendMaximized(output->availableGeometry().size() / output->scaleFactor());
}

void XdgShellV5Integration::handleUnsetMaximized()
{
    if (!m_item->view()->isPrimary())
        return;

    m_xdgSurface->sendUnmaximized(maximizeState.initialWindowSize);
}

void XdgShellV5Integration::handleMaximizedChanged()
{
    if (m_xdgSurface->maximized()) {
        QWaylandOutput *output = m_item->view()->output();
        m_item->moveItem()->setPosition(output->position() + output->availableGeometry().topLeft());
    } else {
        m_item->moveItem()->setPosition(maximizeState.initialPosition);
    }
}

void XdgShellV5Integration::handleActivatedChanged()
{
    if (m_xdgSurface->activated())
        m_item->raise();
}

void XdgShellV5Integration::handleSurfaceSizeChanged()
{
    if (grabberState == GrabberState::Resize) {
        qreal x = resizeState.initialPosition.x();
        qreal y = resizeState.initialPosition.y();
        if (resizeState.resizeEdges & QWaylandXdgSurfaceV5::ResizeEdge::TopEdge)
            y += resizeState.initialSurfaceSize.height() - m_item->surface()->size().height();

        if (resizeState.resizeEdges & QWaylandXdgSurfaceV5::ResizeEdge::LeftEdge)
            x += resizeState.initialSurfaceSize.width() - m_item->surface()->size().width();
        m_item->moveItem()->setPosition(QPointF(x, y));
    }
}

XdgPopupV5Integration::XdgPopupV5Integration(QWaylandQuickShellSurfaceItem *item)
    : QWaylandQuickShellIntegration (item)
    , m_xdgPopup(qobject_cast<QWaylandXdgPopupV5 *>(item->shellSurface()))
    , m_xdgShell(QWaylandXdgPopupV5Private::get(m_xdgPopup)->m_xdgShell)
{
    item->setSurface(m_xdgPopup->surface());
    if (item->view()->output())
        item->moveItem()->setPosition(QPointF(m_xdgPopup->position() * item->view()->output()->scaleFactor()));
    else
        qWarning() << "XdgPopupV5Integration popup item without output" << item;

    QWaylandClient *client = m_xdgPopup->surface()->client();
    auto shell = m_xdgShell;
    QWaylandQuickShellEventFilter::startFilter(client, [shell]() { shell->closeAllPopups(); });

    connect(m_xdgPopup, &QWaylandXdgPopupV5::destroyed, this, &XdgPopupV5Integration::handlePopupDestroyed);
}

void XdgPopupV5Integration::handlePopupDestroyed()
{
    QWaylandXdgShellV5Private *shellPrivate = QWaylandXdgShellV5Private::get(m_xdgShell);
    auto popups = shellPrivate->m_xdgPopups;
    if (popups.isEmpty())
        QWaylandQuickShellEventFilter::cancelFilter();
}

}

QT_END_NAMESPACE
