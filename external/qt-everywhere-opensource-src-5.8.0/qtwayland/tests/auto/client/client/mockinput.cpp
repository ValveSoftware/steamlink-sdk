/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "mockcompositor.h"
#include "mockinput.h"
#include "mocksurface.h"

namespace Impl {

static Surface *resolveSurface(const QVariant &v)
{
    QSharedPointer<MockSurface> mockSurface = v.value<QSharedPointer<MockSurface> >();
    return mockSurface ? mockSurface->handle() : 0;
}

void Compositor::setKeyboardFocus(void *data, const QList<QVariant> &parameters)
{
    Compositor *compositor = static_cast<Compositor *>(data);
    compositor->m_keyboard->setFocus(resolveSurface(parameters.first()));
}

void Compositor::sendMousePress(void *data, const QList<QVariant> &parameters)
{
    Compositor *compositor = static_cast<Compositor *>(data);
    Surface *surface = resolveSurface(parameters.first());
    if (!surface)
        return;

    QPoint pos = parameters.last().toPoint();
    compositor->m_pointer->setFocus(surface, pos);
    compositor->m_pointer->sendMotion(pos);
    compositor->m_pointer->sendButton(0x110, 1);
}

void Compositor::sendMouseRelease(void *data, const QList<QVariant> &parameters)
{
    Compositor *compositor = static_cast<Compositor *>(data);
    Surface *surface = resolveSurface(parameters.first());
    if (!surface)
        return;

    compositor->m_pointer->sendButton(0x110, 0);
}

void Compositor::sendKeyPress(void *data, const QList<QVariant> &parameters)
{
    Compositor *compositor = static_cast<Compositor *>(data);
    Surface *surface = resolveSurface(parameters.first());
    if (!surface)
        return;

    compositor->m_keyboard->sendKey(parameters.last().toUInt() - 8, 1);
}

void Compositor::sendKeyRelease(void *data, const QList<QVariant> &parameters)
{
    Compositor *compositor = static_cast<Compositor *>(data);
    Surface *surface = resolveSurface(parameters.first());
    if (!surface)
        return;

    compositor->m_keyboard->sendKey(parameters.last().toUInt() - 8, 0);
}

void Compositor::sendTouchDown(void *data, const QList<QVariant> &parameters)
{
    Compositor *compositor = static_cast<Compositor *>(data);
    Surface *surface = resolveSurface(parameters.first());

    Q_ASSERT(compositor);
    Q_ASSERT(surface);

    QPoint position = parameters.at(1).toPoint();
    int id = parameters.at(2).toInt();

    compositor->m_touch->sendDown(surface, position, id);
}

void Compositor::sendTouchUp(void *data, const QList<QVariant> &parameters)
{
    Compositor *compositor = static_cast<Compositor *>(data);
    Surface *surface = resolveSurface(parameters.first());

    Q_ASSERT(compositor);
    Q_ASSERT(surface);

    int id = parameters.at(1).toInt();

    compositor->m_touch->sendUp(surface, id);
}

void Compositor::sendTouchMotion(void *data, const QList<QVariant> &parameters)
{
    Compositor *compositor = static_cast<Compositor *>(data);
    Surface *surface = resolveSurface(parameters.first());

    Q_ASSERT(compositor);
    Q_ASSERT(surface);

    QPoint position = parameters.at(1).toPoint();
    int id = parameters.at(2).toInt();

    compositor->m_touch->sendMotion(surface, position, id);
}

void Compositor::sendTouchFrame(void *data, const QList<QVariant> &parameters)
{
    Compositor *compositor = static_cast<Compositor *>(data);
    Surface *surface = resolveSurface(parameters.first());

    Q_ASSERT(compositor);
    Q_ASSERT(surface);

    compositor->m_touch->sendFrame(surface);
}

void Compositor::sendDataDeviceDataOffer(void *data, const QList<QVariant> &parameters)
{
    Compositor *compositor = static_cast<Compositor *>(data);
    Surface *surface = resolveSurface(parameters.first());

    Q_ASSERT(compositor);
    Q_ASSERT(surface);

    compositor->m_data_device_manager->dataDevice()->sendDataOffer(surface->resource()->client());
}

void Compositor::sendDataDeviceEnter(void *data, const QList<QVariant> &parameters)
{
    Compositor *compositor = static_cast<Compositor *>(data);
    Surface *surface = resolveSurface(parameters.first());
    QPoint position = parameters.at(1).toPoint();

    Q_ASSERT(compositor);
    Q_ASSERT(surface);

    compositor->m_data_device_manager->dataDevice()->sendEnter(surface, position);
}

void Compositor::sendDataDeviceMotion(void *data, const QList<QVariant> &parameters)
{
    Compositor *compositor = static_cast<Compositor *>(data);
    Q_ASSERT(compositor);
    QPoint position = parameters.first().toPoint();
    compositor->m_data_device_manager->dataDevice()->sendMotion(position);
}

void Compositor::sendDataDeviceDrop(void *data, const QList<QVariant> &parameters)
{
    Compositor *compositor = static_cast<Compositor *>(data);
    Surface *surface = resolveSurface(parameters.first());

    Q_ASSERT(compositor);
    Q_ASSERT(surface);

    compositor->m_data_device_manager->dataDevice()->sendDrop(surface);
}

void Compositor::sendDataDeviceLeave(void *data, const QList<QVariant> &parameters)
{
    Compositor *compositor = static_cast<Compositor *>(data);
    Surface *surface = resolveSurface(parameters.first());

    Q_ASSERT(compositor);
    Q_ASSERT(surface);

    compositor->m_data_device_manager->dataDevice()->sendLeave(surface);
}

void Compositor::waitForStartDrag(void *data, const QList<QVariant> &parameters)
{
    Compositor *compositor = static_cast<Compositor *>(data);
    Q_ASSERT(compositor);
    while (!compositor->m_startDragSeen) {
        wl_display_flush_clients(compositor->m_display);
        wl_event_loop_dispatch(compositor->m_loop, 100);
    }
    compositor->m_startDragSeen = false;
}

Seat::Seat(Compositor *compositor, struct ::wl_display *display)
    : wl_seat(display, 2)
    , m_compositor(compositor)
    , m_keyboard(new Keyboard(compositor))
    , m_pointer(new Pointer(compositor))
    , m_touch(new Touch(compositor))
{
}

Seat::~Seat()
{
}

void Seat::seat_bind_resource(Resource *resource)
{
    send_capabilities(resource->handle, capability_keyboard | capability_pointer | capability_touch);
}

void Seat::seat_get_keyboard(Resource *resource, uint32_t id)
{
    m_keyboard->add(resource->client(), id, resource->version());
}

void Seat::seat_get_pointer(Resource *resource, uint32_t id)
{
    m_pointer->add(resource->client(), id, resource->version());
}

void Seat::seat_get_touch(Resource *resource, uint32_t id)
{
    m_touch->add(resource->client(), id, resource->version());
}

Keyboard::Keyboard(Compositor *compositor)
    : wl_keyboard()
    , m_compositor(compositor)
    , m_focusResource(Q_NULLPTR)
    , m_focus(Q_NULLPTR)
{
}

Keyboard::~Keyboard()
{
}

void Keyboard::setFocus(Surface *surface)
{
    if (m_focusResource && m_focus != surface) {
        uint32_t serial = m_compositor->nextSerial();
        send_leave(m_focusResource->handle, serial, m_focus->resource()->handle);
    }

    Resource *resource = surface ? resourceMap().value(surface->resource()->client()) : 0;

    if (resource && (m_focus != surface || m_focusResource != resource)) {
        uint32_t serial = m_compositor->nextSerial();
        send_modifiers(resource->handle, serial, 0, 0, 0, 0);
        send_enter(resource->handle, serial, surface->resource()->handle, QByteArray());
    }

    m_focusResource = resource;
    m_focus = surface;
}

void Keyboard::handleSurfaceDestroyed(Surface *surface)
{
    if (surface == m_focus) {
        m_focusResource = nullptr;
        m_focus = nullptr;
    }
}

void Keyboard::sendKey(uint32_t key, uint32_t state)
{
    if (m_focusResource) {
        uint32_t serial = m_compositor->nextSerial();
        send_key(m_focusResource->handle, serial, m_compositor->time(), key, state);
    }
}


void Keyboard::keyboard_destroy_resource(wl_keyboard::Resource *resource)
{
    if (m_focusResource == resource)
        m_focusResource = 0;
}

Pointer::Pointer(Compositor *compositor)
    : wl_pointer()
    , m_compositor(compositor)
    , m_focusResource(Q_NULLPTR)
    , m_focus(Q_NULLPTR)
{
}

Pointer::~Pointer()
{

}

void Pointer::setFocus(Surface *surface, const QPoint &pos)
{
    if (m_focusResource && m_focus != surface) {
        uint32_t serial = m_compositor->nextSerial();
        send_leave(m_focusResource->handle, serial, m_focus->resource()->handle);
    }

    Resource *resource = surface ? resourceMap().value(surface->resource()->client()) : 0;

    if (resource && (m_focus != surface || resource != m_focusResource)) {
        uint32_t serial = m_compositor->nextSerial();
        send_enter(resource->handle, serial, surface->resource()->handle,
                   wl_fixed_from_int(pos.x()), wl_fixed_from_int(pos.y()));
    }

    m_focusResource = resource;
    m_focus = surface;
}

void Pointer::handleSurfaceDestroyed(Surface *surface)
{
    if (m_focus == surface) {
        m_focus = nullptr;
        m_focusResource = nullptr;
    }
}

void Pointer::sendMotion(const QPoint &pos)
{
    if (m_focusResource)
        send_motion(m_focusResource->handle, m_compositor->time(),
                    wl_fixed_from_int(pos.x()), wl_fixed_from_int(pos.y()));
}

void Pointer::sendButton(uint32_t button, uint32_t state)
{
    if (m_focusResource) {
        uint32_t serial = m_compositor->nextSerial();
        send_button(m_focusResource->handle, serial, m_compositor->time(),
                    button, state);
    }
}

void Pointer::pointer_destroy_resource(wl_pointer::Resource *resource)
{
    if (m_focusResource == resource)
        m_focusResource = 0;
}

Touch::Touch(Compositor *compositor)
    : wl_touch()
    , m_compositor(compositor)
{
}

void Touch::sendDown(Surface *surface, const QPoint &position, int id)
{
    uint32_t serial = m_compositor->nextSerial();
    uint32_t time = m_compositor->time();
    Q_ASSERT(surface);
    Resource *resource = resourceMap().value(surface->resource()->client());
    Q_ASSERT(resource);
    wl_touch_send_down(resource->handle, serial, time, surface->resource()->handle, id, position.x(), position.y());
}

void Touch::sendUp(Surface *surface, int id)
{
    Resource *resource = resourceMap().value(surface->resource()->client());
    wl_touch_send_up(resource->handle, m_compositor->nextSerial(), m_compositor->time(), id);
}

void Touch::sendMotion(Surface *surface, const QPoint &position, int id)
{
    Resource *resource = resourceMap().value(surface->resource()->client());
    uint32_t time = m_compositor->time();
    wl_touch_send_motion(resource->handle, time, id, position.x(), position.y());
}

void Touch::sendFrame(Surface *surface)
{
    Resource *resource = resourceMap().value(surface->resource()->client());
    wl_touch_send_frame(resource->handle);
}

DataOffer::DataOffer()
    : wl_data_offer()
{

}

DataDevice::DataDevice(Compositor *compositor)
    : wl_data_device()
    , m_compositor(compositor)
    , m_dataOffer(nullptr)
    , m_focus(nullptr)
{

}

void DataDevice::sendDataOffer(wl_client *client)
{
    m_dataOffer = new QtWaylandServer::wl_data_offer(client, 0, 1);
    Resource *resource = resourceMap().value(client);
    send_data_offer(resource->handle, m_dataOffer->resource()->handle);
}

void DataDevice::sendEnter(Surface *surface, const QPoint& position)
{
    uint serial = m_compositor->nextSerial();
    m_focus = surface;
    Resource *resource = resourceMap().value(surface->resource()->client());
    send_enter(resource->handle, serial, surface->resource()->handle, position.x(), position.y(), m_dataOffer->resource()->handle);
}

void DataDevice::sendMotion(const QPoint &position)
{
    uint32_t time = m_compositor->time();
    Resource *resource = resourceMap().value(m_focus->resource()->client());
    send_motion(resource->handle, time, position.x(), position.y());
}

void DataDevice::sendDrop(Surface *surface)
{
    Resource *resource = resourceMap().value(surface->resource()->client());
    send_drop(resource->handle);
}

void DataDevice::sendLeave(Surface *surface)
{
    Resource *resource = resourceMap().value(surface->resource()->client());
    send_leave(resource->handle);
}

DataDevice::~DataDevice()
{

}

void DataDevice::data_device_start_drag(QtWaylandServer::wl_data_device::Resource *resource, wl_resource *source, wl_resource *origin, wl_resource *icon, uint32_t serial)
{
    m_compositor->m_startDragSeen = true;
}

DataDeviceManager::DataDeviceManager(Compositor *compositor, wl_display *display)
    : wl_data_device_manager(display, 1)
    , m_compositor(compositor)
{

}

DataDeviceManager::~DataDeviceManager()
{

}

DataDevice *DataDeviceManager::dataDevice() const
{
    return m_data_device.data();
}

void DataDeviceManager::data_device_manager_get_data_device(Resource *resource, uint32_t id, struct ::wl_resource *seat)
{
    if (!m_data_device)
        m_data_device.reset(new DataDevice(m_compositor));
    m_data_device->add(resource->client(), id, 1);
}

void DataDeviceManager::data_device_manager_create_data_source(QtWaylandServer::wl_data_device_manager::Resource *resource, uint32_t id)
{
    new QtWaylandServer::wl_data_source(resource->client(), id, 1);
}

}
