/****************************************************************************
**
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

#include "qwaylandmousetracker_p.h"

#include <QtQuick/private/qquickitem_p.h>

QT_BEGIN_NAMESPACE

class QWaylandMouseTrackerPrivate : public QQuickItemPrivate
{
    Q_DECLARE_PUBLIC(QWaylandMouseTracker)
public:
    QWaylandMouseTrackerPrivate()
        : windowSystemCursorEnabled(false)
        , hovered(false)
    {
        QImage cursorImage(64,64,QImage::Format_ARGB32);
        cursorImage.fill(Qt::transparent);
        cursorPixmap = QPixmap::fromImage(cursorImage);
    }
    void handleMousePos(const QPointF &mousePos)
    {
        Q_Q(QWaylandMouseTracker);
        bool xChanged = mousePos.x() != this->mousePos.x();
        bool yChanged = mousePos.y() != this->mousePos.y();
        if (xChanged || yChanged) {
            this->mousePos = mousePos;
            if (xChanged)
                emit q->mouseXChanged();
            if (yChanged)
                emit q->mouseYChanged();
        }
    }

    void setHovered(bool hovered)
    {
        Q_Q(QWaylandMouseTracker);
        if (this->hovered == hovered)
            return;
        this->hovered = hovered;
        emit q->hoveredChanged();
    }

    QPointF mousePos;
    bool windowSystemCursorEnabled;
    QPixmap cursorPixmap;
    bool hovered;
};

QWaylandMouseTracker::QWaylandMouseTracker(QQuickItem *parent)
        : QQuickItem(*(new QWaylandMouseTrackerPrivate), parent)
{
    Q_D(QWaylandMouseTracker);
    setFiltersChildMouseEvents(true);
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setCursor(QCursor(d->cursorPixmap));
}

qreal QWaylandMouseTracker::mouseX() const
{
    Q_D(const QWaylandMouseTracker);
    return d->mousePos.x();
}
qreal QWaylandMouseTracker::mouseY() const
{
    Q_D(const QWaylandMouseTracker);
    return d->mousePos.y();
}

void QWaylandMouseTracker::setWindowSystemCursorEnabled(bool enable)
{
    Q_D(QWaylandMouseTracker);
    if (d->windowSystemCursorEnabled != enable) {
        d->windowSystemCursorEnabled = enable;
        if (enable) {
            unsetCursor();
        } else {
            setCursor(QCursor(d->cursorPixmap));
        }
        emit windowSystemCursorEnabledChanged();
    }
}

bool QWaylandMouseTracker::windowSystemCursorEnabled() const
{
    Q_D(const QWaylandMouseTracker);
    return d->windowSystemCursorEnabled;
}

bool QWaylandMouseTracker::hovered() const
{
    Q_D(const QWaylandMouseTracker);
    return d->hovered;
}

bool QWaylandMouseTracker::childMouseEventFilter(QQuickItem *item, QEvent *event)
{
    Q_D(QWaylandMouseTracker);
    if (event->type() == QEvent::MouseMove) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        d->handleMousePos(mapFromItem(item, mouseEvent->localPos()));
    } else if (event->type() == QEvent::HoverMove) {
        QHoverEvent *hoverEvent = static_cast<QHoverEvent *>(event);
        d->handleMousePos(mapFromItem(item, hoverEvent->posF()));
    }
    return false;
}

void QWaylandMouseTracker::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QWaylandMouseTracker);
    QQuickItem::mouseMoveEvent(event);
    d->handleMousePos(event->localPos());
}

void QWaylandMouseTracker::hoverMoveEvent(QHoverEvent *event)
{
    Q_D(QWaylandMouseTracker);
    QQuickItem::hoverMoveEvent(event);
    d->handleMousePos(event->posF());
}

void QWaylandMouseTracker::hoverEnterEvent(QHoverEvent *event)
{
    Q_D(QWaylandMouseTracker);
    Q_UNUSED(event);
    d->setHovered(true);
}

void QWaylandMouseTracker::hoverLeaveEvent(QHoverEvent *event)
{
    Q_D(QWaylandMouseTracker);
    Q_UNUSED(event);
    d->setHovered(false);
}

QT_END_NAMESPACE
