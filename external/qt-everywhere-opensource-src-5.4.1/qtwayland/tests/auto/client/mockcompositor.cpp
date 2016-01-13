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

#include <stdio.h>
MockCompositor::MockCompositor()
    : m_alive(true)
    , m_ready(false)
    , m_compositor(0)
{
    pthread_create(&m_thread, 0, run, this);

    m_mutex.lock();
    m_waitCondition.wait(&m_mutex);
    m_mutex.unlock();
}

MockCompositor::~MockCompositor()
{
    m_alive = false;
    m_waitCondition.wakeOne();
    pthread_join(m_thread, 0);
}

void MockCompositor::lock()
{
    m_mutex.lock();
}

void MockCompositor::unlock()
{
    m_mutex.unlock();
}

void MockCompositor::applicationInitialized()
{
    m_ready = true;
}

int MockCompositor::waylandFileDescriptor() const
{
    return m_compositor->fileDescriptor();
}

void MockCompositor::processWaylandEvents()
{
    m_waitCondition.wakeOne();
}

void MockCompositor::setOutputGeometry(const QRect &rect)
{
    Command command = makeCommand(Impl::Compositor::setOutputGeometry, m_compositor);
    command.parameters << rect;
    processCommand(command);
}

void MockCompositor::setKeyboardFocus(const QSharedPointer<MockSurface> &surface)
{
    Command command = makeCommand(Impl::Compositor::setKeyboardFocus, m_compositor);
    command.parameters << QVariant::fromValue(surface);
    processCommand(command);
}

void MockCompositor::sendMousePress(const QSharedPointer<MockSurface> &surface, const QPoint &pos)
{
    Command command = makeCommand(Impl::Compositor::sendMousePress, m_compositor);
    command.parameters << QVariant::fromValue(surface) << pos;
    processCommand(command);
}

void MockCompositor::sendMouseRelease(const QSharedPointer<MockSurface> &surface)
{
    Command command = makeCommand(Impl::Compositor::sendMouseRelease, m_compositor);
    command.parameters << QVariant::fromValue(surface);
    processCommand(command);
}

void MockCompositor::sendKeyPress(const QSharedPointer<MockSurface> &surface, uint code)
{
    Command command = makeCommand(Impl::Compositor::sendKeyPress, m_compositor);
    command.parameters << QVariant::fromValue(surface) << code;
    processCommand(command);
}

void MockCompositor::sendKeyRelease(const QSharedPointer<MockSurface> &surface, uint code)
{
    Command command = makeCommand(Impl::Compositor::sendKeyRelease, m_compositor);
    command.parameters << QVariant::fromValue(surface) << code;
    processCommand(command);
}

QSharedPointer<MockSurface> MockCompositor::surface()
{
    QSharedPointer<MockSurface> result;
    lock();
    QVector<Impl::Surface *> surfaces = m_compositor->surfaces();
    foreach (Impl::Surface *surface, surfaces) {
        // we don't want to mistake the cursor surface for a window surface
        if (surface->isMapped()) {
            result = surface->mockSurface();
            break;
        }
    }
    unlock();
    return result;
}

MockCompositor::Command MockCompositor::makeCommand(Command::Callback callback, void *target)
{
    Command command;
    command.callback = callback;
    command.target = target;
    return command;
}

void MockCompositor::processCommand(const Command &command)
{
    lock();
    m_commandQueue << command;
    unlock();

    m_waitCondition.wakeOne();
}

void MockCompositor::dispatchCommands()
{
    foreach (const Command &command, m_commandQueue)
        command.callback(command.target, command.parameters);
    m_commandQueue.clear();
}

void *MockCompositor::run(void *data)
{
    MockCompositor *controller = static_cast<MockCompositor *>(data);

    Impl::Compositor compositor;

    controller->m_compositor = &compositor;
    controller->m_waitCondition.wakeOne();

    while (!controller->m_ready) {
        controller->dispatchCommands();
        compositor.dispatchEvents(20);
    }

    while (controller->m_alive) {
        QMutexLocker locker(&controller->m_mutex);
        controller->m_waitCondition.wait(&controller->m_mutex);
        controller->dispatchCommands();
        compositor.dispatchEvents(20);
    }

    return 0;
}

namespace Impl {

Compositor::Compositor()
    : m_display(wl_display_create())
    , m_time(0)
{
    wl_list_init(&m_outputResources);

    if (wl_display_add_socket(m_display, 0)) {
        fprintf(stderr, "Fatal: Failed to open server socket\n");
        exit(EXIT_FAILURE);
    }

    wl_display_add_global(m_display, &wl_compositor_interface, this, bindCompositor);

    m_data_device_manager.reset(new DataDeviceManager(this, m_display));

    wl_display_init_shm(m_display);

    m_seat.reset(new Seat(this, m_display));
    m_pointer = m_seat->pointer();
    m_keyboard = m_seat->keyboard();

    wl_display_add_global(m_display, &wl_output_interface, this, bindOutput);
    wl_display_add_global(m_display, &wl_shell_interface, this, bindShell);

    m_loop = wl_display_get_event_loop(m_display);
    m_fd = wl_event_loop_get_fd(m_loop);
}

Compositor::~Compositor()
{
    wl_display_destroy(m_display);
}

void Compositor::dispatchEvents(int timeout)
{
    wl_display_flush_clients(m_display);
    wl_event_loop_dispatch(m_loop, timeout);
}

static void compositor_create_surface(wl_client *client, wl_resource *compositorResource, uint32_t id)
{
    Compositor *compositor = static_cast<Compositor *>(compositorResource->data);
    compositor->addSurface(new Surface(client, id, wl_resource_get_version(compositorResource), compositor));
}

static void compositor_create_region(wl_client *client, wl_resource *compositorResource, uint32_t id)
{
    Q_UNUSED(client);
    Q_UNUSED(compositorResource);
    Q_UNUSED(id);
}

void Compositor::bindCompositor(wl_client *client, void *compositorData, uint32_t version, uint32_t id)
{
    static const struct wl_compositor_interface compositorInterface = {
        compositor_create_surface,
        compositor_create_region
    };

    Q_UNUSED(version);
    wl_client_add_object(client, &wl_compositor_interface, &compositorInterface, id, compositorData);
}

static void unregisterResourceCallback(wl_listener *listener, void *data)
{
    struct wl_resource *resource = reinterpret_cast<struct wl_resource *>(data);
    wl_list_remove(&resource->link);
    delete listener;
}

void registerResource(wl_list *list, wl_resource *resource)
{
    wl_list_insert(list, &resource->link);

    wl_listener *listener = new wl_listener;
    listener->notify = unregisterResourceCallback;

    wl_signal_add(&resource->destroy_signal, listener);
}

QVector<Surface *> Compositor::surfaces() const
{
    return m_surfaces;
}

uint32_t Compositor::nextSerial()
{
    return wl_display_next_serial(m_display);
}

void Compositor::addSurface(Surface *surface)
{
    m_surfaces << surface;
}

void Compositor::removeSurface(Surface *surface)
{
    m_surfaces.removeOne(surface);
    if (m_keyboard->focus() == surface)
        m_keyboard->setFocus(0);
    if (m_pointer->focus() == surface)
        m_pointer->setFocus(0, QPoint());
}

}

