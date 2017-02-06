/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Extras module of the Qt Toolkit.
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

#include "qquickmousethief_p.h"

#include <QtCore/QMetaObject>
#include <QtQuick/QQuickWindow>

QQuickMouseThief::QQuickMouseThief(QObject *parent) :
    QObject(parent),
    mItem(0),
    mReceivedPressEvent(false),
    mAcceptCurrentEvent(false)
{
}

bool QQuickMouseThief::receivedPressEvent() const
{
    return mReceivedPressEvent;
}

void QQuickMouseThief::setReceivedPressEvent(bool receivedPressEvent)
{
    if (receivedPressEvent != mReceivedPressEvent) {
        mReceivedPressEvent = receivedPressEvent;
        emit receivedPressEventChanged();
    }
}

void QQuickMouseThief::grabMouse(QQuickItem *item)
{
    if (item) {
        mItem = item;

        // Handle the case where someone makes the PieMenu an orphan. E.g.:
        // property PieMenu pieMenu: PieMenu {}
        // They have to set the parent explicitly if they want that to work.
        // This can also be null if the menu is visible when constructed.
        if (mItem->window()) {
            // We install an event filter so that we can track release events.
            // This is because the MouseArea that we steal the mouse from stores the
            // pressed flag (before item is visible) and we don't, so we don't know
            // that it's a release.
            mItem->grabMouse();
            mItem->window()->installEventFilter(this);
        } else {
            // The menu was visible when constructed (visible: true), so install the
            // event filter later on.
            connect(mItem, SIGNAL(windowChanged(QQuickWindow*)),
                this, SLOT(itemWindowChanged(QQuickWindow*)));
        }
    }
}

void QQuickMouseThief::ungrabMouse()
{
    if (mItem) {
        // This can be null if someone does the following:
        // PieMenu { Component.onCompleted: { visible = true; visible = false; }
        if (mItem->window()) {
            if (mItem->window()->mouseGrabberItem() == mItem) {
                mItem->ungrabMouse();
            }
            mItem->window()->removeEventFilter(this);
        }
        mItem = 0;
    }
}

void QQuickMouseThief::acceptCurrentEvent()
{
    mAcceptCurrentEvent = true;
}

void QQuickMouseThief::itemWindowChanged(QQuickWindow *window)
{
    // The window will be null when the application is closing.
    if (window) {
        // This can be null if someone does the following:
        // PieMenu { Component.onCompleted: { visible = true; visible = false; }
        if (mItem) {
            mItem->grabMouse();
            window->installEventFilter(this);
        }
    }
}

bool QQuickMouseThief::eventFilter(QObject *, QEvent *event)
{
    if (!mItem)
        return false;

    // The PieMenu QML code dictates which events are accepted by
    // setting this property when it responds to our signals.
    mAcceptCurrentEvent = false;

    if (event->type() == QEvent::MouseButtonRelease) {
        const QPointF mouseWindowPos = static_cast<QMouseEvent *>(event)->windowPos();
        emitReleased(mouseWindowPos);
        // Even if the release should close the menu, we should emit
        // clicked to be consistent since we have to do it ourselves.
        bool releaseAccepted = mAcceptCurrentEvent;
        mAcceptCurrentEvent = false;

        emitClicked(mouseWindowPos);
        if (!mAcceptCurrentEvent) {
            // We might not have accepted click, but we may have accepted release.
            mAcceptCurrentEvent = releaseAccepted;
        }
    } else if (event->type() == QEvent::MouseButtonPress) {
        emitPressed(static_cast<QMouseEvent *>(event)->windowPos());
    } else if (event->type() == QEvent::TouchEnd) {
        // The finger(s) were lifted off the screen. We don't get this as a release event since
        // we're doing our own custom handling, so we handle it here.
        QTouchEvent *touchEvent = static_cast<QTouchEvent*>(event);
        const QList<QTouchEvent::TouchPoint> points = touchEvent->touchPoints();
        // We only care about one finger. If there's more than one, ignore them.
        if (!points.isEmpty()) {
            const QPointF pos = points.first().pos();
            emitReleased(pos);
            // Even if the release should close the menu, we should emit
            // clicked to be consistent since we have to do it ourselves.
            bool releaseAccepted = mAcceptCurrentEvent;
            mAcceptCurrentEvent = false;

            emitClicked(pos);
            if (!mAcceptCurrentEvent) {
                // We might not have accepted click, but we may have accepted release.
                mAcceptCurrentEvent = releaseAccepted;
            }
        }
    } else if (event->type() == QEvent::TouchBegin) {
        QTouchEvent *touchEvent = static_cast<QTouchEvent*>(event);
        const QList<QTouchEvent::TouchPoint> points = touchEvent->touchPoints();
        if (!points.isEmpty()) {
            emitPressed(points.first().pos());
        }
    } else if (event->type() == QEvent::TouchUpdate) {
        QTouchEvent *touchEvent = static_cast<QTouchEvent*>(event);
        const QList<QTouchEvent::TouchPoint> points = touchEvent->touchPoints();
        if (!points.isEmpty()) {
            const QPointF mappedPos = mItem->mapFromScene(points.first().pos());
            emit touchUpdate(mappedPos.x(), mappedPos.y());
        }
    }
    return mAcceptCurrentEvent;
}

void QQuickMouseThief::emitPressed(const QPointF &pos)
{
    setReceivedPressEvent(true);

    // PieMenu can't detect presses outside its own MouseArea,
    // so we need to provide these as well.
    QPointF mappedPos = mItem->mapFromScene(pos);
    emit pressed(mappedPos.x(), mappedPos.y());
}

void QQuickMouseThief::emitReleased(const QPointF &pos)
{
    QPointF mappedPos = mItem->mapFromScene(pos);
    emit released(mappedPos.x(), mappedPos.y());
}

void QQuickMouseThief::emitClicked(const QPointF &pos)
{
    // mItem can be destroyed if the slots connected to released() caused it to be hidden.
    // That's fine for us, since PieMenu doesn't need to handle anything if released()
    // is emitted when the triggerMode is PieMenu.TriggerOnRelease.
    if (mItem) {
        QPointF mappedPos = mItem->mapFromScene(pos);
        // Since we are creating the release events, we must also handle clicks.
        emit clicked(mappedPos.x(), mappedPos.y());
    }
}
