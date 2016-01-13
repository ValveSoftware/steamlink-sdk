/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
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

Seat::Seat(Compositor *compositor, struct ::wl_display *display)
    : wl_seat(display, 2)
    , m_compositor(compositor)
    , m_keyboard(new Keyboard(compositor))
    , m_pointer(new Pointer(compositor))
{
}

Seat::~Seat()
{
}

void Seat::seat_bind_resource(Resource *resource)
{
    send_capabilities(resource->handle, capability_keyboard | capability_pointer);
}

void Seat::seat_get_keyboard(Resource *resource, uint32_t id)
{
    m_keyboard->add(resource->client(), id, resource->version());
}

void Seat::seat_get_pointer(Resource *resource, uint32_t id)
{
    m_pointer->add(resource->client(), id, resource->version());
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

DataDevice::DataDevice(Compositor *compositor)
    : wl_data_device()
    , m_compositor(compositor)
{

}

DataDevice::~DataDevice()
{

}

DataDeviceManager::DataDeviceManager(Compositor *compositor, wl_display *display)
    : wl_data_device_manager(display, 1)
    , m_compositor(compositor)
{

}

DataDeviceManager::~DataDeviceManager()
{

}

void DataDeviceManager::data_device_manager_get_data_device(Resource *resource, uint32_t id, struct ::wl_resource *seat)
{
    if (!m_data_device)
        m_data_device.reset(new DataDevice(m_compositor));
    m_data_device->add(resource->client(), id, 1);
}

}
