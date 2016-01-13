/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Copyright (C) 2013 Klarälvdalens Datakonsult AB (KDAB).
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

protected:
    void seat_bind_resource(Resource *resource) Q_DECL_OVERRIDE;
    void seat_get_keyboard(Resource *resource, uint32_t id) Q_DECL_OVERRIDE;
    void seat_get_pointer(Resource *resource, uint32_t id) Q_DECL_OVERRIDE;

private:
    Compositor *m_compositor;

    QScopedPointer<Keyboard> m_keyboard;
    QScopedPointer<Pointer> m_pointer;
};

class Keyboard : public QtWaylandServer::wl_keyboard
{
public:
    Keyboard(Compositor *compositor);
    ~Keyboard();

    Surface *focus() const { return m_focus; }
    void setFocus(Surface *surface);

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
    void sendMotion(const QPoint &pos);
    void sendButton(uint32_t button, uint32_t state);

protected:
    void pointer_destroy_resource(wl_pointer::Resource *resource) Q_DECL_OVERRIDE;

private:
    Compositor *m_compositor;

    Resource *m_focusResource;
    Surface *m_focus;
};

class DataDevice : public QtWaylandServer::wl_data_device
{
public:
    DataDevice(Compositor *compositor);
    ~DataDevice();

private:
    Compositor *m_compositor;
};

class DataDeviceManager : public QtWaylandServer::wl_data_device_manager
{
public:
    DataDeviceManager(Compositor *compositor, struct ::wl_display *display);
    ~DataDeviceManager();

protected:
    void data_device_manager_get_data_device(Resource *resource, uint32_t id, struct ::wl_resource *seat) Q_DECL_OVERRIDE;

private:
    Compositor *m_compositor;

    QScopedPointer<DataDevice> m_data_device;
};

}

#endif // MOCKINPUT_H
