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

#include "mockclient.h"
#include "mockseat.h"

#include <QElapsedTimer>
#include <QSocketNotifier>

#include <private/qguiapplication_p.h>

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>

const struct wl_registry_listener MockClient::registryListener = {
    MockClient::handleGlobal
};

MockClient::MockClient()
    : display(wl_display_connect("wayland-qt-test-0"))
    , compositor(0)
    , output(0)
    , registry(0)
    , wlshell(0)
    , xdgShell(nullptr)
    , iviApplication(nullptr)
    , refreshRate(-1)
    , error(0 /* means no error according to spec */)
    , protocolError({0, 0, nullptr})
{
    if (!display)
        qFatal("MockClient(): wl_display_connect() failed");

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registryListener, this);

    fd = wl_display_get_fd(display);

    QSocketNotifier *readNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(readNotifier, SIGNAL(activated(int)), this, SLOT(readEvents()));

    QAbstractEventDispatcher *dispatcher = QGuiApplicationPrivate::eventDispatcher;
    connect(dispatcher, SIGNAL(awake()), this, SLOT(flushDisplay()));

    QElapsedTimer timeout;
    timeout.start();
    do {
        QCoreApplication::processEvents();
    } while (!(compositor && output) && timeout.elapsed() < 1000);

    if (!compositor || !output)
        qFatal("MockClient(): failed to receive globals from display");
}

const wl_output_listener MockClient::outputListener = {
    MockClient::outputGeometryEvent,
    MockClient::outputModeEvent,
    MockClient::outputDone,
    MockClient::outputScale
};

MockClient::~MockClient()
{
    wl_display_disconnect(display);
}

void MockClient::outputGeometryEvent(void *data, wl_output *,
                                     int32_t x, int32_t y,
                                     int32_t width, int32_t height,
                                     int, const char *, const char *,
                                     int32_t )
{
    Q_UNUSED(width);
    Q_UNUSED(height);
    resolve(data)->geometry.moveTopLeft(QPoint(x, y));
}

void MockClient::outputModeEvent(void *data, wl_output *, uint32_t flags,
                                 int w, int h, int refreshRate)
{
    QWaylandOutputMode mode(QSize(w, h), refreshRate);

    if (flags & WL_OUTPUT_MODE_CURRENT) {
        resolve(data)->geometry.setSize(QSize(w, h));
        resolve(data)->resolution = QSize(w, h);
        resolve(data)->refreshRate = refreshRate;
        resolve(data)->currentMode = mode;
    }

    if (flags & WL_OUTPUT_MODE_PREFERRED)
        resolve(data)->preferredMode = mode;

    resolve(data)->modes.append(mode);
}

void MockClient::outputDone(void *, wl_output *)
{

}

void MockClient::outputScale(void *, wl_output *, int)
{

}

void MockClient::readEvents()
{
    if (error)
        return;
    wl_display_dispatch(display);
}

void MockClient::flushDisplay()
{
    if (error)
        return;

    if (wl_display_prepare_read(display) == 0) {
        wl_display_read_events(display);
    }

    if (wl_display_dispatch_pending(display) < 0) {
        error = wl_display_get_error(display);
        if (error == EPROTO) {
            protocolError.code = wl_display_get_protocol_error(display, &protocolError.interface, &protocolError.id);
            return;
        }
    }

    wl_display_flush(display);
}

void MockClient::handleGlobal(void *data, wl_registry *registry, uint32_t id, const char *interface, uint32_t version)
{
    Q_UNUSED(registry);
    Q_UNUSED(version);
    resolve(data)->handleGlobal(id, QByteArray(interface));
}

void MockClient::handleGlobal(uint32_t id, const QByteArray &interface)
{
    if (interface == "wl_compositor") {
        compositor = static_cast<wl_compositor *>(wl_registry_bind(registry, id, &wl_compositor_interface, 1));
    } else if (interface == "wl_output") {
        output = static_cast<wl_output *>(wl_registry_bind(registry, id, &wl_output_interface, 2));
        wl_output_add_listener(output, &outputListener, this);
    } else if (interface == "wl_shm") {
        shm = static_cast<wl_shm *>(wl_registry_bind(registry, id, &wl_shm_interface, 1));
    } else if (interface == "wl_shell") {
        wlshell = static_cast<wl_shell *>(wl_registry_bind(registry, id, &wl_shell_interface, 1));
    } else if (interface == "xdg_shell") {
        xdgShell = static_cast<xdg_shell *>(wl_registry_bind(registry, id, &xdg_shell_interface, 1));
    } else if (interface == "ivi_application") {
        iviApplication = static_cast<ivi_application *>(wl_registry_bind(registry, id, &ivi_application_interface, 1));
    } else if (interface == "wl_seat") {
        wl_seat *s = static_cast<wl_seat *>(wl_registry_bind(registry, id, &wl_seat_interface, 1));
        m_seats << new MockSeat(s);
    }
}

wl_surface *MockClient::createSurface()
{
    flushDisplay();
    return wl_compositor_create_surface(compositor);
}

wl_shell_surface *MockClient::createShellSurface(wl_surface *surface)
{
    flushDisplay();
    return wl_shell_get_shell_surface(wlshell, surface);
}

xdg_surface *MockClient::createXdgSurface(wl_surface *surface)
{
    flushDisplay();
    return xdg_shell_get_xdg_surface(xdgShell, surface);
}

ivi_surface *MockClient::createIviSurface(wl_surface *surface, uint iviId)
{
    flushDisplay();
    return ivi_application_surface_create(iviApplication, iviId, surface);
}

ShmBuffer::ShmBuffer(const QSize &size, wl_shm *shm)
    : handle(0)
{
    int stride = size.width() * 4;
    int alloc = stride * size.height();

    char filename[] = "/tmp/wayland-shm-XXXXXX";

    int fd = mkstemp(filename);
    if (fd < 0) {
        qWarning("open %s failed: %s", filename, strerror(errno));
        return;
    }

    if (ftruncate(fd, alloc) < 0) {
        qWarning("ftruncate failed: %s", strerror(errno));
        close(fd);
        return;
    }

    void *data = mmap(0, alloc, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    unlink(filename);

    if (data == MAP_FAILED) {
        qWarning("mmap failed: %s", strerror(errno));
        close(fd);
        return;
    }

    image = QImage(static_cast<uchar *>(data), size.width(), size.height(), stride, QImage::Format_ARGB32_Premultiplied);
    shm_pool = wl_shm_create_pool(shm,fd,alloc);
    handle = wl_shm_pool_create_buffer(shm_pool,0, size.width(), size.height(),
                                   stride, WL_SHM_FORMAT_ARGB8888);
    close(fd);
}

ShmBuffer::~ShmBuffer()
{
    munmap(image.bits(), image.byteCount());
    wl_buffer_destroy(handle);
    wl_shm_pool_destroy(shm_pool);
}

