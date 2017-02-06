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

#include "qwaylandtouch.h"
#include "qwaylandtouch_p.h"

#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandSeat>
#include <QtWaylandCompositor/QWaylandView>
#include <QtWaylandCompositor/QWaylandClient>

#include <QtWaylandCompositor/private/qwlqttouch_p.h>

QT_BEGIN_NAMESPACE

QWaylandTouchPrivate::QWaylandTouchPrivate(QWaylandTouch *touch, QWaylandSeat *seat)
    : wl_touch()
    , seat(seat)
{
    Q_UNUSED(touch);
}

void QWaylandTouchPrivate::touch_release(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

uint QWaylandTouchPrivate::sendDown(QWaylandSurface *surface, uint32_t time, int touch_id, const QPointF &position)
{
    Q_Q(QWaylandTouch);
    auto focusResource = resourceMap().value(surface->client()->client());
    if (!focusResource)
        return 0;

    uint32_t serial = q->compositor()->nextSerial();

    wl_touch_send_down(focusResource->handle, serial, time, surface->resource(), touch_id,
                       wl_fixed_from_double(position.x()), wl_fixed_from_double(position.y()));
    return serial;
}

uint QWaylandTouchPrivate::sendUp(QWaylandClient *client, uint32_t time, int touch_id)
{
    auto focusResource = resourceMap().value(client->client());

    if (!focusResource)
        return 0;

    uint32_t serial = compositor()->nextSerial();

    wl_touch_send_up(focusResource->handle, serial, time, touch_id);
    return serial;
}

void QWaylandTouchPrivate::sendMotion(QWaylandClient *client, uint32_t time, int touch_id, const QPointF &position)
{
    auto focusResource = resourceMap().value(client->client());

    if (!focusResource)
        return;

    wl_touch_send_motion(focusResource->handle, time, touch_id,
                         wl_fixed_from_double(position.x()), wl_fixed_from_double(position.y()));
}

/*!
 * \class QWaylandTouch
 * \inmodule QtWaylandCompositor
 * \since 5.8
 * \brief The QWaylandTouch class provides access to a touch device.
 *
 * This class provides access to the touch device in a QWaylandSeat. It corresponds to
 * the Wayland interface wl_touch.
 */

/*!
 * Constructs a QWaylandTouch for the \a seat and with the given \a parent.
 */
QWaylandTouch::QWaylandTouch(QWaylandSeat *seat, QObject *parent)
    : QWaylandObject(*new QWaylandTouchPrivate(this, seat), parent)
{
}

/*!
 * Returns the input device for this QWaylandTouch.
 */
QWaylandSeat *QWaylandTouch::seat() const
{
    Q_D(const QWaylandTouch);
    return d->seat;
}

/*!
 * Returns the compositor for this QWaylandTouch.
 */
QWaylandCompositor *QWaylandTouch::compositor() const
{
    Q_D(const QWaylandTouch);
    return d->compositor();
}

/*!
 * Sends a touch point event to the touch device of \a surface with the given \a id,
 * \a position, and \a state.
 *
 * Returns the serial of the down or up event if sent, otherwise 0.
 */
uint QWaylandTouch::sendTouchPointEvent(QWaylandSurface *surface, int id, const QPointF &position, Qt::TouchPointState state)
{
    Q_D(QWaylandTouch);
    uint32_t time = compositor()->currentTimeMsecs();
    uint serial = 0;
    switch (state) {
    case Qt::TouchPointPressed:
        serial = d->sendDown(surface, time, id, position);
        break;
    case Qt::TouchPointMoved:
        d->sendMotion(surface->client(), time, id, position);
        break;
    case Qt::TouchPointReleased:
        serial = d->sendUp(surface->client(), time, id);
        break;
    case Qt::TouchPointStationary:
        // stationary points are not sent through wayland, the client must cache them
        break;
    }

    return serial;
}

/*!
 * Sends a touch frame event to the touch device of a \a client. This indicates the end of a
 * contact point list.
 */
void QWaylandTouch::sendFrameEvent(QWaylandClient *client)
{
    Q_D(QWaylandTouch);
    auto focusResource = d->resourceMap().value(client->client());
    if (focusResource)
        d->send_frame(focusResource->handle);
}

/*!
 * Sends a touch cancel event to the touch device of a \a client.
 */
void QWaylandTouch::sendCancelEvent(QWaylandClient *client)
{
    Q_D(QWaylandTouch);
    auto focusResource = d->resourceMap().value(client->client());
    if (focusResource)
        d->send_cancel(focusResource->handle);
}

/*!
 * Sends all touch points in \a event to the specified \a surface,
 * followed by a touch frame event.
 *
 * \sa sendTouchPointEvent(), sendFrameEvent()
 */
void QWaylandTouch::sendFullTouchEvent(QWaylandSurface *surface, QTouchEvent *event)
{
    Q_D(QWaylandTouch);
    if (event->type() == QEvent::TouchCancel) {
        sendCancelEvent(surface->client());
        return;
    }

    QtWayland::TouchExtensionGlobal *ext = QtWayland::TouchExtensionGlobal::findIn(d->compositor());
    if (ext && ext->postTouchEvent(event, surface))
        return;

    const QList<QTouchEvent::TouchPoint> points = event->touchPoints();
    if (points.isEmpty())
        return;

    const int pointCount = points.count();
    for (int i = 0; i < pointCount; ++i) {
        const QTouchEvent::TouchPoint &tp(points.at(i));
        // Convert the local pos in the compositor window to surface-relative.
        sendTouchPointEvent(surface, tp.id(), tp.pos(), tp.state());
    }
    sendFrameEvent(surface->client());
}

/*!
 * \internal
 */
void QWaylandTouch::addClient(QWaylandClient *client, uint32_t id, uint32_t version)
{
    Q_D(QWaylandTouch);
    d->add(client->client(), id, qMin<uint32_t>(QtWaylandServer::wl_touch::interfaceVersion(), version));
}

QT_END_NAMESPACE
