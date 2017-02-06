/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Klarälvdalens Datakonsult AB (KDAB).
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

#ifndef MOCKINPUT_H
#define MOCKINPUT_H

#include <qglobal.h>

#include "qwayland-server-wayland.h"

#include "mockcompositor.h"

namespace Impl {

class Keyboard;
class Pointer;

class Seat : public QtWaylandServer::wl_seat
{
public:
    Seat(Compositor *compositor, struct ::wl_display *display);
    ~Seat();

    Compositor *compositor() const { return m_compositor; }

    Keyboard *keyboard() const { return m_keyboard.data(); }
    Pointer *pointer() const { return m_pointer.data(); }
    Touch *touch() const { return m_touch.data(); }

protected:
    void seat_bind_resource(Resource *resource) Q_DECL_OVERRIDE;
    void seat_get_keyboard(Resource *resource, uint32_t id) Q_DECL_OVERRIDE;
    void seat_get_pointer(Resource *resource, uint32_t id) Q_DECL_OVERRIDE;
    void seat_get_touch(Resource *resource, uint32_t id) Q_DECL_OVERRIDE;

private:
    Compositor *m_compositor;

    QScopedPointer<Keyboard> m_keyboard;
    QScopedPointer<Pointer> m_pointer;
    QScopedPointer<Touch> m_touch;
};

class Keyboard : public QtWaylandServer::wl_keyboard
{
public:
    Keyboard(Compositor *compositor);
    ~Keyboard();

    Surface *focus() const { return m_focus; }
    void setFocus(Surface *surface);
    void handleSurfaceDestroyed(Surface *surface);

    void sendKey(uint32_t key, uint32_t state);

protected:
    void keyboard_destroy_resource(wl_keyboard::Resource *resource) Q_DECL_OVERRIDE;

private:
    Compositor *m_compositor;

    Resource *m_focusResource;
    Surface *m_focus;
};

class Pointer : public QtWaylandServer::wl_pointer
{
public:
    Pointer(Compositor *compositor);
    ~Pointer();

    Surface *focus() const { return m_focus; }

    void setFocus(Surface *surface, const QPoint &pos);
    void handleSurfaceDestroyed(Surface *surface);
    void sendMotion(const QPoint &pos);
    void sendButton(uint32_t button, uint32_t state);

protected:
    void pointer_destroy_resource(wl_pointer::Resource *resource) Q_DECL_OVERRIDE;

private:
    Compositor *m_compositor;

    Resource *m_focusResource;
    Surface *m_focus;
};

class Touch : public QtWaylandServer::wl_touch
{
public:
    Touch(Compositor *compositor);
    void sendDown(Surface *surface, const QPoint &position, int id);
    void sendUp(Surface *surface, int id);
    void sendMotion(Surface *surface, const QPoint &position, int id);
    void sendFrame(Surface *surface);
private:
    Compositor *m_compositor;
};

class DataOffer : public QtWaylandServer::wl_data_offer
{
public:
    DataOffer();
};

class DataDevice : public QtWaylandServer::wl_data_device
{
public:
    DataDevice(Compositor *compositor);
    void sendDataOffer(wl_client *client);
    void sendEnter(Surface *surface, const QPoint &position);
    void sendMotion(const QPoint &position);
    void sendDrop(Surface *surface);
    void sendLeave(Surface *surface);
    ~DataDevice();

protected:
    void data_device_start_drag(Resource *resource, struct ::wl_resource *source, struct ::wl_resource *origin, struct ::wl_resource *icon, uint32_t serial) override;

private:
    Compositor *m_compositor;
    QtWaylandServer::wl_data_offer *m_dataOffer;
    Surface* m_focus;
};

class DataDeviceManager : public QtWaylandServer::wl_data_device_manager
{
public:
    DataDeviceManager(Compositor *compositor, struct ::wl_display *display);
    ~DataDeviceManager();
    DataDevice *dataDevice() const;

protected:
    void data_device_manager_get_data_device(Resource *resource, uint32_t id, struct ::wl_resource *seat) Q_DECL_OVERRIDE;
    void data_device_manager_create_data_source(Resource *resource, uint32_t id) override;

private:
    Compositor *m_compositor;

    QScopedPointer<DataDevice> m_data_device;
};

}

#endif // MOCKINPUT_H
