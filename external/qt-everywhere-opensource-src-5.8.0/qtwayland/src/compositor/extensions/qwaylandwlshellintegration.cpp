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

#include "qwaylandwlshellintegration_p.h"

#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandWlShellSurface>
#include <QtWaylandCompositor/QWaylandQuickShellSurfaceItem>
#include <QtWaylandCompositor/QWaylandSeat>

QT_BEGIN_NAMESPACE

namespace QtWayland {

WlShellIntegration::WlShellIntegration(QWaylandQuickShellSurfaceItem *item)
    : QWaylandQuickShellIntegration(item)
    , m_item(item)
    , m_shellSurface(qobject_cast<QWaylandWlShellSurface *>(item->shellSurface()))
    , grabberState(GrabberState::Default)
    , isPopup(false)
    , currentState(State::Windowed)
    , nextState(State::Windowed)
{
    m_item->setSurface(m_shellSurface->surface());
    connect(m_shellSurface.data(), &QWaylandWlShellSurface::startMove, this, &WlShellIntegration::handleStartMove);
    connect(m_shellSurface.data(), &QWaylandWlShellSurface::startResize, this, &WlShellIntegration::handleStartResize);
    connect(m_shellSurface->surface(), &QWaylandSurface::redraw, this, &WlShellIntegration::handleRedraw);
    connect(m_shellSurface->surface(), &QWaylandSurface::offsetForNextFrame, this, &WlShellIntegration::adjustOffsetForNextFrame);
    connect(m_shellSurface->surface(), &QWaylandSurface::hasContentChanged, this, &WlShellIntegration::handleSurfaceHasContentChanged);
    connect(m_shellSurface.data(), &QWaylandWlShellSurface::setDefaultToplevel, this, &WlShellIntegration::handleSetDefaultTopLevel);
    connect(m_shellSurface.data(), &QWaylandWlShellSurface::setTransient, this, &WlShellIntegration::handleSetTransient);
    connect(m_shellSurface.data(), &QWaylandWlShellSurface::setMaximized, this, &WlShellIntegration::handleSetMaximized);
    connect(m_shellSurface.data(), &QWaylandWlShellSurface::setFullScreen, this, &WlShellIntegration::handleSetFullScreen);
    connect(m_shellSurface.data(), &QWaylandWlShellSurface::setPopup, this, &WlShellIntegration::handleSetPopup);
    connect(m_shellSurface.data(), &QWaylandWlShellSurface::destroyed, this, &WlShellIntegration::handleShellSurfaceDestroyed);
}

void WlShellIntegration::handleStartMove(QWaylandSeat *seat)
{
    grabberState = GrabberState::Move;
    moveState.seat = seat;
    moveState.initialized = false;
}

void WlShellIntegration::handleStartResize(QWaylandSeat *seat, QWaylandWlShellSurface::ResizeEdge edges)
{
    grabberState = GrabberState::Resize;
    resizeState.seat = seat;
    resizeState.resizeEdges = edges;
    float scaleFactor = m_item->view()->output()->scaleFactor();
    resizeState.initialSize = m_shellSurface->surface()->size() / scaleFactor;
    resizeState.initialized = false;
}

void WlShellIntegration::handleSetDefaultTopLevel()
{
    // Take focus if the policy allows
    if (m_shellSurface->shell()->focusPolicy() == QWaylandShell::AutomaticFocus)
        m_item->takeFocus();

    // In order to restore the window state, the client calls setDefaultToplevel()
    // so we need to unset the flags here but we save the previous state and move
    // to the initial position when redrawing
    nextState = State::Windowed;
}

void WlShellIntegration::handleSetTransient(QWaylandSurface *parentSurface, const QPoint &relativeToParent, bool inactive)
{
    Q_UNUSED(parentSurface)
    Q_UNUSED(relativeToParent)

    // Take focus if the policy allows and it's not inactive
    if (m_shellSurface->shell()->focusPolicy() == QWaylandShell::AutomaticFocus && !inactive)
        m_item->takeFocus();
}

void WlShellIntegration::handleSetMaximized(QWaylandOutput *output)
{
    if (!m_item->view()->isPrimary())
        return;

    if (currentState == State::Maximized)
        return;

    QWaylandOutput *designatedOutput = output ? output : m_item->view()->output();
    if (!designatedOutput)
        return;

    if (currentState == State::Windowed)
        normalPosition = m_item->moveItem()->position();

    nextState = State::Maximized;
    finalPosition = designatedOutput->position() + designatedOutput->availableGeometry().topLeft();

    m_shellSurface->sendConfigure(designatedOutput->availableGeometry().size(), QWaylandWlShellSurface::NoneEdge);
}

void WlShellIntegration::handleSetFullScreen(QWaylandWlShellSurface::FullScreenMethod method, uint framerate, QWaylandOutput *output)
{
    Q_UNUSED(method);
    Q_UNUSED(framerate);

    if (!m_item->view()->isPrimary())
        return;

    if (currentState == State::FullScreen)
        return;

    QWaylandOutput *designatedOutput = output ? output : m_item->view()->output();
    if (!designatedOutput)
        return;

    if (currentState == State::Windowed)
        normalPosition = m_item->moveItem()->position();

    nextState = State::FullScreen;
    finalPosition = designatedOutput->position();

    m_shellSurface->sendConfigure(designatedOutput->geometry().size(), QWaylandWlShellSurface::NoneEdge);
}

void WlShellIntegration::handleSetPopup(QWaylandSeat *seat, QWaylandSurface *parent, const QPoint &relativeToParent)
{
    Q_UNUSED(seat);

    // Find the parent item on the same output
    QWaylandQuickShellSurfaceItem *parentItem = nullptr;
    Q_FOREACH (QWaylandView *view, parent->views()) {
        if (view->output() == m_item->view()->output()) {
            QWaylandQuickShellSurfaceItem *item = qobject_cast<QWaylandQuickShellSurfaceItem*>(view->renderObject());
            if (item) {
                parentItem = item;
                break;
            }
        }
    }

    if (parentItem) {
        // Clear all the transforms for this ShellSurfaceItem. They are not
        // applicable when the item becomes a child to a surface that has its
        // own transforms. Otherwise the transforms would be applied twice.
        QQmlListProperty<QQuickTransform> t = m_item->transform();
        t.clear(&t);
        m_item->setRotation(0);
        m_item->setScale(1.0);
        m_item->setX(relativeToParent.x());
        m_item->setY(relativeToParent.y());
        m_item->setParentItem(parentItem);
    }

    isPopup = true;
    auto shell = m_shellSurface->shell();
    QWaylandQuickShellEventFilter::startFilter(m_shellSurface->surface()->client(), [shell]() {
        shell->closeAllPopups();
    });

    QObject::connect(m_shellSurface->surface(), &QWaylandSurface::hasContentChanged,
                     this, &WlShellIntegration::handleSurfaceHasContentChanged);
}

void WlShellIntegration::handlePopupClosed()
{
    handlePopupRemoved();
    if (m_shellSurface)
        QObject::disconnect(m_shellSurface->surface(), &QWaylandSurface::hasContentChanged,
                            this, &WlShellIntegration::handleSurfaceHasContentChanged);
}

void WlShellIntegration::handlePopupRemoved()
{
    if (!m_shellSurface || m_shellSurface->shell()->mappedPopups().isEmpty())
        QWaylandQuickShellEventFilter::cancelFilter();
    isPopup = false;
}

void WlShellIntegration::handleShellSurfaceDestroyed()
{
    if (isPopup)
        handlePopupRemoved();
    m_shellSurface = nullptr;
}

void WlShellIntegration::handleSurfaceHasContentChanged()
{
    if (m_shellSurface && m_shellSurface->surface()->size().isEmpty()
            && m_shellSurface->windowType() == Qt::WindowType::Popup) {
        handlePopupClosed();
    }
}

void WlShellIntegration::handleRedraw()
{
    if (currentState == nextState)
        return;

    m_item->moveItem()->setPosition(nextState == State::Windowed ? normalPosition : finalPosition);
    currentState = nextState;
}

void WlShellIntegration::adjustOffsetForNextFrame(const QPointF &offset)
{
    if (!m_item->view()->isPrimary())
        return;

    float scaleFactor = m_item->view()->output()->scaleFactor();
    QQuickItem *moveItem = m_item->moveItem();
    moveItem->setPosition(moveItem->position() + offset * scaleFactor);
}

bool WlShellIntegration::mouseMoveEvent(QMouseEvent *event)
{
    if (grabberState == GrabberState::Resize) {
        Q_ASSERT(resizeState.seat == m_item->compositor()->seatFor(event));
        if (!resizeState.initialized) {
            resizeState.initialMousePos = event->windowPos();
            resizeState.initialized = true;
            return true;
        }
        float scaleFactor = m_item->view()->output()->scaleFactor();
        QPointF delta = (event->windowPos() - resizeState.initialMousePos) / scaleFactor;
        QSize newSize = m_shellSurface->sizeForResize(resizeState.initialSize, delta, resizeState.resizeEdges);
        m_shellSurface->sendConfigure(newSize, resizeState.resizeEdges);
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

bool WlShellIntegration::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    if (grabberState != GrabberState::Default) {
        grabberState = GrabberState::Default;
        return true;
    }
    return false;
}

}

QT_END_NAMESPACE
