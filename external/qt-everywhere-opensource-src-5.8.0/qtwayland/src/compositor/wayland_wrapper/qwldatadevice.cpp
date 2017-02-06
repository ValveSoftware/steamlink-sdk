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

#include "qwldatadevice_p.h"

#include "qwldatasource_p.h"
#include "qwldataoffer_p.h"
#include "qwaylandsurface_p.h"
#include "qwldatadevicemanager_p.h"

#include "qwaylanddrag.h"
#include "qwaylandview.h"
#include <QtWaylandCompositor/QWaylandClient>
#include <QtWaylandCompositor/private/qwaylandcompositor_p.h>
#include <QtWaylandCompositor/private/qwaylandseat_p.h>
#include <QtWaylandCompositor/private/qwaylandpointer_p.h>

#include <QtCore/QPointF>
#include <QDebug>

QT_BEGIN_NAMESPACE

namespace QtWayland {

DataDevice::DataDevice(QWaylandSeat *seat)
    : wl_data_device()
    , m_compositor(seat->compositor())
    , m_seat(seat)
    , m_selectionSource(0)
    , m_dragClient(0)
    , m_dragDataSource(0)
    , m_dragFocus(0)
    , m_dragFocusResource(0)
    , m_dragIcon(0)
    , m_dragOrigin(nullptr)
{
}

void DataDevice::setFocus(QWaylandClient *focusClient)
{
    if (!focusClient)
        return;

    Resource *resource = resourceMap().value(focusClient->client());

    if (!resource)
        return;

    if (m_selectionSource) {
        DataOffer *offer = new DataOffer(m_selectionSource, resource);
        send_selection(resource->handle, offer->resource()->handle);
    }
}

void DataDevice::setDragFocus(QWaylandSurface *focus, const QPointF &localPosition)
{
    if (m_dragFocusResource) {
        send_leave(m_dragFocusResource->handle);
        m_dragFocus = 0;
        m_dragFocusResource = 0;
    }

    if (!focus)
        return;

    if (!m_dragDataSource && m_dragClient != focus->waylandClient())
        return;

    Resource *resource = resourceMap().value(focus->waylandClient());

    if (!resource)
        return;

    uint32_t serial = m_compositor->nextSerial();

    DataOffer *offer = m_dragDataSource ? new DataOffer(m_dragDataSource, resource) : 0;

    if (m_dragDataSource && !offer)
        return;

    send_enter(resource->handle, serial, focus->resource(),
               wl_fixed_from_double(localPosition.x()), wl_fixed_from_double(localPosition.y()),
               offer->resource()->handle);

    m_dragFocus = focus;
    m_dragFocusResource = resource;
}

QWaylandSurface *DataDevice::dragIcon() const
{
    return m_dragIcon;
}

QWaylandSurface *DataDevice::dragOrigin() const
{
    return m_dragOrigin;
}

void DataDevice::sourceDestroyed(DataSource *source)
{
    if (m_selectionSource == source)
        m_selectionSource = 0;
}

void DataDevice::dragMove(QWaylandSurface *target, const QPointF &pos)
{
    if (target != m_dragFocus)
        setDragFocus(target, pos);
    if (!target)
        return;
    uint time = m_compositor->currentTimeMsecs(); //### should be serial
    send_motion(m_dragFocusResource->handle, time,
                wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()));
}

void DataDevice::drop()
{
    if (m_dragFocusResource) {
        send_drop(m_dragFocusResource->handle);
        setDragFocus(nullptr, QPoint());
    } else {
        m_dragDataSource->cancel();
    }
    m_dragOrigin = nullptr;
    setDragIcon(nullptr);
}

void DataDevice::cancelDrag()
{
    setDragFocus(nullptr, QPoint());
}
    
void DataDevice::data_device_start_drag(Resource *resource, struct ::wl_resource *source, struct ::wl_resource *origin, struct ::wl_resource *icon, uint32_t serial)
{
    m_dragClient = resource->client();
    m_dragDataSource = source ? DataSource::fromResource(source) : 0;
    m_dragOrigin = QWaylandSurface::fromResource(origin);
    QWaylandDrag *drag = m_seat->drag();
    setDragIcon(icon ? QWaylandSurface::fromResource(icon) : nullptr);
    Q_EMIT drag->dragStarted();
    Q_EMIT m_dragOrigin->dragStarted(drag);

    Q_UNUSED(serial);
    //### need to verify that we have an implicit grab with this serial
}

void DataDevice::data_device_set_selection(Resource *, struct ::wl_resource *source, uint32_t serial)
{
    Q_UNUSED(serial);

    DataSource *dataSource = source ? DataSource::fromResource(source) : 0;

    if (m_selectionSource)
        m_selectionSource->cancel();

    m_selectionSource = dataSource;
    QWaylandCompositorPrivate::get(m_compositor)->dataDeviceManager()->setCurrentSelectionSource(m_selectionSource);
    if (m_selectionSource)
        m_selectionSource->setDevice(this);

    QWaylandClient *focusClient = m_seat->keyboard()->focusClient();
    Resource *resource = focusClient ? resourceMap().value(focusClient->client()) : 0;

    if (resource && m_selectionSource) {
        DataOffer *offer = new DataOffer(m_selectionSource, resource);
        send_selection(resource->handle, offer->resource()->handle);
    } else if (resource) {
        send_selection(resource->handle, 0);
    }
}

void DataDevice::setDragIcon(QWaylandSurface *icon)
{
    if (icon == m_dragIcon)
        return;
    m_dragIcon = icon;
    Q_EMIT m_seat->drag()->iconChanged();
}

}

QT_END_NAMESPACE
