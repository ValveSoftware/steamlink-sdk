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

void MockCompositor::sendTouchDown(const QSharedPointer<MockSurface> &surface, const QPoint &position, int id)
{
    Command command = makeCommand(Impl::Compositor::sendTouchDown, m_compositor);
    command.parameters << QVariant::fromValue(surface) << position << id;
    processCommand(command);
}

void MockCompositor::sendTouchMotion(const QSharedPointer<MockSurface> &surface, const QPoint &position, int id)
{
    Command command = makeCommand(Impl::Compositor::sendTouchMotion, m_compositor);
    command.parameters << QVariant::fromValue(surface) << position << id;
    processCommand(command);
}

void MockCompositor::sendTouchUp(const QSharedPointer<MockSurface> &surface, int id)
{
    Command command = makeCommand(Impl::Compositor::sendTouchUp, m_compositor);
    command.parameters << QVariant::fromValue(surface) << id;
    processCommand(command);
}

void MockCompositor::sendTouchFrame(const QSharedPointer<MockSurface> &surface)
{
    Command command = makeCommand(Impl::Compositor::sendTouchFrame, m_compositor);
    command.parameters << QVariant::fromValue(surface);
    processCommand(command);
}

void MockCompositor::sendDataDeviceDataOffer(const QSharedPointer<MockSurface> &surface)
{
    Command command = makeCommand(Impl::Compositor::sendDataDeviceDataOffer, m_compositor);
    command.parameters << QVariant::fromValue(surface);
    processCommand(command);
}

void MockCompositor::sendDataDeviceEnter(const QSharedPointer<MockSurface> &surface, const QPoint& position)
{
    Command command = makeCommand(Impl::Compositor::sendDataDeviceEnter, m_compositor);
    command.parameters << QVariant::fromValue(surface) << QVariant::fromValue(position);
    processCommand(command);
}

void MockCompositor::sendDataDeviceMotion(const QPoint &position)
{
    Command command = makeCommand(Impl::Compositor::sendDataDeviceMotion, m_compositor);
    command.parameters << QVariant::fromValue(position);
    processCommand(command);
}

void MockCompositor::sendDataDeviceDrop(const QSharedPointer<MockSurface> &surface)
{
    Command command = makeCommand(Impl::Compositor::sendDataDeviceDrop, m_compositor);
    command.parameters << QVariant::fromValue(surface);
    processCommand(command);
}

void MockCompositor::sendDataDeviceLeave(const QSharedPointer<MockSurface> &surface)
{
    Command command = makeCommand(Impl::Compositor::sendDataDeviceLeave, m_compositor);
    command.parameters << QVariant::fromValue(surface);
    processCommand(command);
}

void MockCompositor::waitForStartDrag()
{
    Command command = makeCommand(Impl::Compositor::waitForStartDrag, m_compositor);
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
    lock();
    int count = m_commandQueue.length();
    unlock();

    for (int i = 0; i < count; ++i) {
        lock();
        const Command command = m_commandQueue.takeFirst();
        unlock();
        command.callback(command.target, command.parameters);
    }
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
        {
            QMutexLocker locker(&controller->m_mutex);
            if (controller->m_commandQueue.isEmpty())
                controller->m_waitCondition.wait(&controller->m_mutex);
        }
        controller->dispatchCommands();
        compositor.dispatchEvents(20);
    }

    return 0;
}

namespace Impl {

Compositor::Compositor()
    : m_display(wl_display_create())
    , m_startDragSeen(false)
    , m_time(0)
{
    wl_list_init(&m_outputResources);

    if (wl_display_add_socket(m_display, 0)) {
        fprintf(stderr, "Fatal: Failed to open server socket\n");
        exit(EXIT_FAILURE);
    }

    wl_global_create(m_display, &wl_compositor_interface, 1, this, bindCompositor);

    m_data_device_manager.reset(new DataDeviceManager(this, m_display));

    wl_display_init_shm(m_display);

    m_seat.reset(new Seat(this, m_display));
    m_pointer = m_seat->pointer();
    m_keyboard = m_seat->keyboard();
    m_touch = m_seat->touch();

    wl_global_create(m_display, &wl_output_interface, 1, this, bindOutput);
    wl_global_create(m_display, &wl_shell_interface, 1, this, bindShell);

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

    wl_resource *resource = wl_resource_create(client, &wl_compositor_interface, static_cast<int>(version), id);
    wl_resource_set_implementation(resource, &compositorInterface, compositorData, nullptr);
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
    m_keyboard->handleSurfaceDestroyed(surface);
    m_pointer->handleSurfaceDestroyed(surface);
}

}

