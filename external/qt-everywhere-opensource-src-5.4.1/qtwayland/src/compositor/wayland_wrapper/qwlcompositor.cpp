/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Compositor.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwlcompositor_p.h"

#include "qwaylandinput.h"
#include "qwldisplay_p.h"
#include "qwloutput_p.h"
#include "qwlsurface_p.h"
#include "qwaylandcompositor.h"
#include "qwldatadevicemanager_p.h"
#include "qwldatadevice_p.h"
#include "qwlextendedoutput_p.h"
#include "qwlextendedsurface_p.h"
#include "qwlsubsurface_p.h"
#include "qwlshellsurface_p.h"
#include "qwlqttouch_p.h"
#include "qwlqtkey_p.h"
#include "qwlinputdevice_p.h"
#include "qwlinputpanel_p.h"
#include "qwlregion_p.h"
#include "qwlpointer_p.h"
#include "qwltextinputmanager_p.h"
#include "qwaylandglobalinterface.h"
#include "qwaylandsurfaceview.h"

#include <QWindow>
#include <QSocketNotifier>
#include <QScreen>
#include <qpa/qplatformscreen.h>
#include <QGuiApplication>
#include <QDebug>

#include <QtCore/QAbstractEventDispatcher>
#include <QtGui/private/qguiapplication_p.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/select.h>
#include <sys/time.h>

#include <wayland-server.h>

#include "hardware_integration/qwlhwintegration_p.h"
#include "hardware_integration/qwlclientbufferintegration_p.h"
#include "hardware_integration/qwlserverbufferintegration_p.h"
#include "windowmanagerprotocol/waylandwindowmanagerintegration_p.h"

#include "hardware_integration/qwlclientbufferintegrationfactory_p.h"
#include "hardware_integration/qwlserverbufferintegrationfactory_p.h"

QT_BEGIN_NAMESPACE

namespace QtWayland {

static Compositor *compositor;

Compositor *Compositor::instance()
{
    return compositor;
}

Compositor::Compositor(QWaylandCompositor *qt_compositor, QWaylandCompositor::ExtensionFlags extensions)
    : m_extensions(extensions)
    , m_display(new Display)
    , m_default_input_device(0)
    , m_current_frame(0)
    , m_last_queued_buf(-1)
    , m_qt_compositor(qt_compositor)
    , m_orientation(Qt::PrimaryOrientation)
#if defined (QT_COMPOSITOR_WAYLAND_GL)
    , m_hw_integration(0)
    , m_client_buffer_integration(0)
    , m_server_buffer_integration(0)
#endif
    , m_windowManagerIntegration(0)
    , m_outputExtension(0)
    , m_surfaceExtension(0)
    , m_subSurfaceExtension(0)
    , m_touchExtension(0)
    , m_qtkeyExtension(0)
    , m_textInputManager()
    , m_inputPanel()
    , m_retainSelection(false)
{
    m_timer.start();
    compositor = this;
}

void Compositor::init()
{
    QStringList arguments = QCoreApplication::instance()->arguments();

    int socketArg = arguments.indexOf(QLatin1String("--wayland-socket-name"));
    if (socketArg != -1 && socketArg + 1 < arguments.size())
        m_socket_name = arguments.at(socketArg + 1).toLocal8Bit();

    wl_compositor::init(m_display->handle(), 3);

    m_data_device_manager =  new DataDeviceManager(this);

    wl_display_init_shm(m_display->handle());

    m_output_global = new OutputGlobal(m_display->handle());

    if (wl_display_add_socket(m_display->handle(), m_qt_compositor->socketName())) {
        fprintf(stderr, "Fatal: Failed to open server socket\n");
        exit(EXIT_FAILURE);
    }

    m_loop = wl_display_get_event_loop(m_display->handle());

    int fd = wl_event_loop_get_fd(m_loop);

    QSocketNotifier *sockNot = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(sockNot, SIGNAL(activated(int)), this, SLOT(processWaylandEvents()));

    QAbstractEventDispatcher *dispatcher = QGuiApplicationPrivate::eventDispatcher;
    connect(dispatcher, SIGNAL(aboutToBlock()), this, SLOT(processWaylandEvents()));

    qRegisterMetaType<SurfaceBuffer*>("SurfaceBuffer*");
    qRegisterMetaType<QWaylandSurface*>("WaylandSurface*");
    qRegisterMetaType<QWaylandSurfaceView*>("WaylandSurfaceView*");
    //initialize distancefieldglyphcache here

    initializeHardwareIntegration();
    initializeExtensions();
    initializeDefaultInputDevice();
}

Compositor::~Compositor()
{
    if (!m_destroyed_surfaces.isEmpty())
        qWarning("QWaylandCompositor::cleanupGraphicsResources() must be called manually");

    delete m_outputExtension;
    delete m_surfaceExtension;
    delete m_subSurfaceExtension;
    delete m_touchExtension;
    delete m_qtkeyExtension;

    delete m_default_wayland_input_device;
    delete m_data_device_manager;

    delete m_output_global;
    delete m_display;
}

void Compositor::sendFrameCallbacks(QList<QWaylandSurface *> visibleSurfaces)
{
    foreach (QWaylandSurface *surface, visibleSurfaces) {
        surface->handle()->sendFrameCallback();
    }
    wl_display_flush_clients(m_display->handle());
}

uint Compositor::currentTimeMsecs() const
{
    return m_timer.elapsed();
}

void Compositor::processWaylandEvents()
{
    int ret = wl_event_loop_dispatch(m_loop, 0);
    if (ret)
        fprintf(stderr, "wl_event_loop_dispatch error: %d\n", ret);
    wl_display_flush_clients(m_display->handle());
}

void Compositor::destroySurface(Surface *surface)
{
    m_surfaces.removeOne(surface);

    waylandCompositor()->surfaceAboutToBeDestroyed(surface->waylandSurface());

    surface->releaseSurfaces();
    m_destroyed_surfaces << surface->waylandSurface();
}

void Compositor::cleanupGraphicsResources()
{
    qDeleteAll(m_destroyed_surfaces);
    m_destroyed_surfaces.clear();
}

void Compositor::compositor_create_surface(Resource *resource, uint32_t id)
{
    QWaylandSurface *surface = new QWaylandSurface(resource->client(), id, resource->version(), m_qt_compositor);
    m_surfaces << surface->handle();
    //BUG: This may not be an on-screen window surface though
    m_qt_compositor->surfaceCreated(surface);
}

void Compositor::compositor_create_region(Resource *resource, uint32_t id)
{
    Q_UNUSED(compositor);
    new Region(resource->client(), id);
}

void Compositor::destroyClient(WaylandClient *c)
{
    wl_client *client = static_cast<wl_client *>(c);
    if (!client)
        return;

    if (m_windowManagerIntegration)
        m_windowManagerIntegration->sendQuitMessage(client);

    wl_client_destroy(client);
}

QWindow *Compositor::window() const
{
    return m_qt_compositor->window();
}

ClientBufferIntegration * Compositor::clientBufferIntegration() const
{
#ifdef QT_COMPOSITOR_WAYLAND_GL
    return m_client_buffer_integration.data();
#else
    return 0;
#endif
}

ServerBufferIntegration * Compositor::serverBufferIntegration() const
{
#ifdef QT_COMPOSITOR_WAYLAND_GL
    return m_server_buffer_integration.data();
#else
    return 0;
#endif
}

void Compositor::initializeHardwareIntegration()
{
#ifdef QT_COMPOSITOR_WAYLAND_GL
    if (m_extensions & QWaylandCompositor::HardwareIntegrationExtension)
        m_hw_integration.reset(new HardwareIntegration(this));
    QWindow *window = m_qt_compositor->window();
    if (window && window->surfaceType() != QWindow::RasterSurface) {
        loadClientBufferIntegration();
        loadServerBufferIntegration();
    }

    if (m_client_buffer_integration)
        m_client_buffer_integration->initializeHardware(m_display);
    if (m_server_buffer_integration)
        m_server_buffer_integration->initializeHardware(m_qt_compositor);
#endif
}

void Compositor::initializeExtensions()
{
    if (m_extensions & QWaylandCompositor::OutputExtension)
        m_outputExtension = new OutputExtensionGlobal(this);
    if (m_extensions & QWaylandCompositor::SurfaceExtension)
        m_surfaceExtension = new SurfaceExtensionGlobal(this);
    if (m_extensions & QWaylandCompositor::SubSurfaceExtension)
        m_subSurfaceExtension = new SubSurfaceExtensionGlobal(this);
    if (m_extensions & QWaylandCompositor::TouchExtension)
        m_touchExtension = new TouchExtensionGlobal(this);
    if (m_extensions & QWaylandCompositor::QtKeyExtension)
        m_qtkeyExtension = new QtKeyExtensionGlobal(this);
    if (m_extensions & QWaylandCompositor::TextInputExtension) {
        m_textInputManager.reset(new TextInputManager(this));
        m_inputPanel.reset(new InputPanel(this));
    }
    if (m_extensions & QWaylandCompositor::WindowManagerExtension) {
        m_windowManagerIntegration = new WindowManagerServerIntegration(m_qt_compositor, this);
        m_windowManagerIntegration->initialize(m_display);
    }
}

void Compositor::initializeDefaultInputDevice()
{
    m_default_wayland_input_device = new QWaylandInputDevice(m_qt_compositor);
    m_default_input_device = m_default_wayland_input_device->handle();
}

QList<struct wl_client *> Compositor::clients() const
{
    QList<struct wl_client *> list;
    foreach (Surface *surface, m_surfaces) {
        struct wl_client *client = surface->resource()->client();
        if (!list.contains(client))
            list.append(client);
    }
    return list;
}

void Compositor::setScreenOrientation(Qt::ScreenOrientation orientation)
{
    m_orientation = orientation;
    m_output_global->sendOutputOrientation(orientation);
}

Qt::ScreenOrientation Compositor::screenOrientation() const
{
    return m_orientation;
}

void Compositor::setOutputGeometry(const QRect &geometry)
{
    if (m_output_global)
        m_output_global->setGeometry(geometry);
}

QRect Compositor::outputGeometry() const
{
    if (m_output_global)
        return m_output_global->geometry();
    return QRect();
}

void Compositor::setOutputRefreshRate(int rate)
{
    if (m_output_global)
        m_output_global->setRefreshRate(rate);
}

int Compositor::outputRefreshRate() const
{
    if (m_output_global)
        return m_output_global->refreshRate();
    return 0;
}

void Compositor::setClientFullScreenHint(bool value)
{
    if (m_windowManagerIntegration)
        m_windowManagerIntegration->setShowIsFullScreen(value);
}

QWaylandCompositor::ExtensionFlags Compositor::extensions() const
{
    return m_extensions;
}

InputDevice* Compositor::defaultInputDevice()
{
    return m_default_input_device;
}

QList<QtWayland::Surface *> Compositor::surfacesForClient(wl_client *client)
{
    QList<QtWayland::Surface *> ret;

    for (int i=0; i < m_surfaces.count(); ++i) {
        if (m_surfaces.at(i)->resource()->client() == client) {
            ret.append(m_surfaces.at(i));
        }
    }
    return ret;
}

void Compositor::configureTouchExtension(int flags)
{
    if (m_touchExtension)
        m_touchExtension->setFlags(flags);
}

InputPanel *Compositor::inputPanel() const
{
    return m_inputPanel.data();
}

DataDeviceManager *Compositor::dataDeviceManager() const
{
    return m_data_device_manager;
}

void Compositor::setRetainedSelectionEnabled(bool enabled)
{
    m_retainSelection = enabled;
}

bool Compositor::retainedSelectionEnabled() const
{
    return m_retainSelection;
}

void Compositor::feedRetainedSelectionData(QMimeData *data)
{
    if (m_retainSelection)
        m_qt_compositor->retainedSelectionReceived(data);
}

void Compositor::overrideSelection(const QMimeData *data)
{
    m_data_device_manager->overrideSelection(*data);
}

bool Compositor::isDragging() const
{
    return false;
}

void Compositor::sendDragMoveEvent(const QPoint &global, const QPoint &local,
                                            Surface *surface)
{
    Q_UNUSED(global);
    Q_UNUSED(local);
    Q_UNUSED(surface);
//    Drag::instance()->dragMove(global, local, surface);
}

void Compositor::sendDragEndEvent()
{
//    Drag::instance()->dragEnd();
}

void Compositor::bindGlobal(wl_client *client, void *data, uint32_t version, uint32_t id)
{
    QWaylandGlobalInterface *iface = static_cast<QWaylandGlobalInterface *>(data);
    iface->bind(client, qMin(iface->version(), version), id);
};

void Compositor::loadClientBufferIntegration()
{
    QStringList keys = ClientBufferIntegrationFactory::keys();
    QString targetKey;
    QByteArray clientBufferIntegration = qgetenv("QT_WAYLAND_HARDWARE_INTEGRATION");
    if (clientBufferIntegration.isEmpty())
        clientBufferIntegration = qgetenv("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION");
    if (keys.contains(QString::fromLocal8Bit(clientBufferIntegration.constData()))) {
        targetKey = QString::fromLocal8Bit(clientBufferIntegration.constData());
    } else if (keys.contains(QString::fromLatin1("wayland-egl"))) {
        targetKey = QString::fromLatin1("wayland-egl");
    } else if (!keys.isEmpty()) {
        targetKey = keys.first();
    }

    if (!targetKey.isEmpty()) {
        m_client_buffer_integration.reset(ClientBufferIntegrationFactory::create(targetKey, QStringList()));
        if (m_client_buffer_integration) {
            m_client_buffer_integration->setCompositor(m_qt_compositor);
            if (m_hw_integration)
                m_hw_integration->setClientBufferIntegration(targetKey);
        }
    }
    //BUG: if there is no client buffer integration, bad things will when opengl is used
}

void Compositor::loadServerBufferIntegration()
{
    QStringList keys = ServerBufferIntegrationFactory::keys();
    QString targetKey;
    QByteArray serverBufferIntegration = qgetenv("QT_WAYLAND_SERVER_BUFFER_INTEGRATION");
    if (keys.contains(QString::fromLocal8Bit(serverBufferIntegration.constData()))) {
        targetKey = QString::fromLocal8Bit(serverBufferIntegration.constData());
    }
    if (!targetKey.isEmpty()) {
        m_server_buffer_integration.reset(ServerBufferIntegrationFactory::create(targetKey, QStringList()));
        if (m_hw_integration)
            m_hw_integration->setServerBufferIntegration(targetKey);
    }
}

} // namespace Wayland

QT_END_NAMESPACE
