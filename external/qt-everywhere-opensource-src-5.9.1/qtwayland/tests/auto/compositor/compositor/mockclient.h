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

#include <wayland-client.h>
#include <wayland-xdg-shell-client-protocol.h>
#include <wayland-ivi-application-client-protocol.h>

#include <QObject>
#include <QImage>
#include <QRect>
#include <QList>
#include <QWaylandOutputMode>

class MockSeat;

class ShmBuffer
{
public:
    ShmBuffer(const QSize &size, wl_shm *shm);
    ~ShmBuffer();

    struct wl_buffer *handle;
    struct wl_shm_pool *shm_pool;
    QImage image;
};

class MockClient : public QObject
{
    Q_OBJECT

public:
    MockClient();
    ~MockClient();

    wl_surface *createSurface();
    wl_shell_surface *createShellSurface(wl_surface *surface);
    xdg_surface *createXdgSurface(wl_surface *surface);
    ivi_surface *createIviSurface(wl_surface *surface, uint iviId);

    wl_display *display;
    wl_compositor *compositor;
    wl_output *output;
    wl_shm *shm;
    wl_registry *registry;
    wl_shell *wlshell;
    xdg_shell *xdgShell;
    ivi_application *iviApplication;

    QList<MockSeat *> m_seats;

    QRect geometry;
    QSize resolution;
    int refreshRate;
    QWaylandOutputMode currentMode;
    QWaylandOutputMode preferredMode;
    QList<QWaylandOutputMode> modes;

    int fd;
    int error;
    struct {
        uint id;
        uint code;
        const wl_interface *interface;
    } protocolError;

private slots:
    void readEvents();
    void flushDisplay();

private:
    static MockClient *resolve(void *data) { return static_cast<MockClient *>(data); }
    static const struct wl_registry_listener registryListener;
    static void handleGlobal(void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version);
    static int sourceUpdate(uint32_t mask, void *data);

    static void outputGeometryEvent(void *data,
                                    wl_output *output,
                                    int32_t x, int32_t y,
                                    int32_t width, int32_t height,
                                    int subpixel,
                                    const char *make,
                                    const char *model,
                                    int32_t transform);

    static void outputModeEvent(void *data,
                                wl_output *wl_output,
                                uint32_t flags,
                                int width,
                                int height,
                                int refreshRate);
    static void outputDone(void *data, wl_output *output);
    static void outputScale(void *data, wl_output *output, int factor);

    void handleGlobal(uint32_t id, const QByteArray &interface);

    static const wl_output_listener outputListener;
};

