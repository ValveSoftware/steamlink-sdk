/****************************************************************************
**
** Copyright (C) 2015 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#include "qtwaylandcompositorglobal_p.h"
#include "qwaylandcompositor.h"
#include "qwaylandcompositor_p.h"

#include <QtWaylandCompositor/qwaylandclient.h>
#include <QtWaylandCompositor/qwaylandseat.h>
#include <QtWaylandCompositor/qwaylandoutput.h>
#include <QtWaylandCompositor/qwaylandview.h>
#include <QtWaylandCompositor/qwaylandclient.h>
#include <QtWaylandCompositor/qwaylandkeyboard.h>
#include <QtWaylandCompositor/qwaylandpointer.h>
#include <QtWaylandCompositor/qwaylandtouch.h>
#include <QtWaylandCompositor/qwaylandsurfacegrabber.h>

#include <QtWaylandCompositor/private/qwaylandkeyboard_p.h>
#include <QtWaylandCompositor/private/qwaylandsurface_p.h>

#include "wayland_wrapper/qwldatadevice_p.h"
#include "wayland_wrapper/qwldatadevicemanager_p.h"
#include "wayland_wrapper/qwlbuffermanager_p.h"

#include "hardware_integration/qwlclientbufferintegration_p.h"
#include "hardware_integration/qwlclientbufferintegrationfactory_p.h"
#include "hardware_integration/qwlserverbufferintegration_p.h"
#include "hardware_integration/qwlserverbufferintegrationfactory_p.h"

#if QT_CONFIG(opengl)
#include "hardware_integration/qwlhwintegration_p.h"
#endif

#include "extensions/qwaylandqtwindowmanager.h"

#include "qwaylandxkb_p.h"
#include "qwaylandsharedmemoryformathelper_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QStringList>
#include <QtCore/QSocketNotifier>

#include <QtGui/QDesktopServices>
#include <QtGui/QScreen>

#include <QtGui/qpa/qwindowsysteminterface_p.h>
#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QtGui/private/qguiapplication_p.h>

#if QT_CONFIG(opengl)
#   include <QOpenGLTextureBlitter>
#   include <QOpenGLTexture>
#   include <QOpenGLContext>
#   include <QOpenGLFramebufferObject>
#   include <QMatrix4x4>
#endif

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(qLcCompositorInputMethods, "qt.compositor.input.methods")

namespace QtWayland {

class WindowSystemEventHandler : public QWindowSystemEventHandler
{
public:
    WindowSystemEventHandler(QWaylandCompositor *c) : compositor(c) {}
    bool sendEvent(QWindowSystemInterfacePrivate::WindowSystemEvent *e) Q_DECL_OVERRIDE
    {
        if (e->type == QWindowSystemInterfacePrivate::Key) {
            QWindowSystemInterfacePrivate::KeyEvent *ke = static_cast<QWindowSystemInterfacePrivate::KeyEvent *>(e);
            QWaylandKeyboardPrivate *keyb = QWaylandKeyboardPrivate::get(compositor->defaultSeat()->keyboard());

            uint32_t code = ke->nativeScanCode;
            bool isDown = ke->keyType == QEvent::KeyPress;

#if QT_CONFIG(xkbcommon_evdev)
            QString text;
            Qt::KeyboardModifiers modifiers = QWaylandXkb::modifiers(keyb->xkbState());

            const xkb_keysym_t sym = xkb_state_key_get_one_sym(keyb->xkbState(), code);
            int qtkey;
            std::tie(qtkey, text) = QWaylandXkb::keysymToQtKey(sym, modifiers);

            ke->key = qtkey;
            ke->modifiers = modifiers;
            ke->nativeVirtualKey = sym;
            ke->nativeModifiers = keyb->xkbModsMask();
            ke->unicode = text;
#endif
            if (!ke->repeat)
                keyb->keyEvent(code, isDown ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED);

            QWindowSystemEventHandler::sendEvent(e);

            if (!ke->repeat) {
                keyb->maybeUpdateKeymap();
                keyb->updateModifierState(code, isDown ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED);
            }
        } else {
            QWindowSystemEventHandler::sendEvent(e);
        }
        return true;
    }

    QWaylandCompositor *compositor;
};

} // namespace

QWaylandCompositorPrivate::QWaylandCompositorPrivate(QWaylandCompositor *compositor)
    : display(0)
#if QT_CONFIG(opengl)
    , use_hw_integration_extension(true)
    , client_buffer_integration(0)
    , server_buffer_integration(0)
#endif
    , retainSelection(false)
    , preInitialized(false)
    , initialized(false)
{
    if (QGuiApplication::platformNativeInterface())
        display = static_cast<wl_display*>(QGuiApplication::platformNativeInterface()->nativeResourceForIntegration("server_wl_display"));
    if (!display)
        display = wl_display_create();
    eventHandler.reset(new QtWayland::WindowSystemEventHandler(compositor));
    timer.start();

    QWindowSystemInterfacePrivate::installWindowSystemEventHandler(eventHandler.data());
}

void QWaylandCompositorPrivate::init()
{
    Q_Q(QWaylandCompositor);
    QStringList arguments = QCoreApplication::instance()->arguments();

    if (socket_name.isEmpty()) {
        const int socketArg = arguments.indexOf(QLatin1String("--wayland-socket-name"));
        if (socketArg != -1 && socketArg + 1 < arguments.size())
            socket_name = arguments.at(socketArg + 1).toLocal8Bit();
    }
    wl_compositor::init(display, 3);
    wl_subcompositor::init(display, 1);

    data_device_manager =  new QtWayland::DataDeviceManager(q);
    buffer_manager = new QtWayland::BufferManager(q);

    wl_display_init_shm(display);
    QVector<wl_shm_format> formats = QWaylandSharedMemoryFormatHelper::supportedWaylandFormats();
    foreach (wl_shm_format format, formats)
        wl_display_add_shm_format(display, format);

    if (!socket_name.isEmpty()) {
        if (wl_display_add_socket(display, socket_name.constData()))
            qFatal("Fatal: Failed to open server socket\n");
    } else {
        const char *autoSocketName = wl_display_add_socket_auto(display);
        if (!autoSocketName)
            qFatal("Fatal: Failed to open server socket\n");
        socket_name = autoSocketName;
    }

    loop = wl_display_get_event_loop(display);

    int fd = wl_event_loop_get_fd(loop);

    QSocketNotifier *sockNot = new QSocketNotifier(fd, QSocketNotifier::Read, q);
    QObject::connect(sockNot, SIGNAL(activated(int)), q, SLOT(processWaylandEvents()));

    QAbstractEventDispatcher *dispatcher = QGuiApplicationPrivate::eventDispatcher;
    QObject::connect(dispatcher, SIGNAL(aboutToBlock()), q, SLOT(processWaylandEvents()));

    initializeHardwareIntegration();
    initializeSeats();

    initialized = true;

    Q_FOREACH (QPointer<QObject> object, polish_objects) {
        if (object) {
            QEvent polishEvent(QEvent::Polish);
            QCoreApplication::sendEvent(object.data(), &polishEvent);
        }
    }

    emit q->createdChanged();
}

QWaylandCompositorPrivate::~QWaylandCompositorPrivate()
{
    qDeleteAll(clients);

    qDeleteAll(outputs);

    delete data_device_manager;

    wl_display_destroy(display);
}

void QWaylandCompositorPrivate::preInit()
{
    Q_Q(QWaylandCompositor);

    if (preInitialized)
        return;

    if (seats.empty())
        seats.append(q->createSeat());

    preInitialized = true;
}

void QWaylandCompositorPrivate::destroySurface(QWaylandSurface *surface)
{
    Q_Q(QWaylandCompositor);
    q->surfaceAboutToBeDestroyed(surface);

    delete surface;
}

void QWaylandCompositorPrivate::unregisterSurface(QWaylandSurface *surface)
{
    if (!all_surfaces.removeOne(surface))
        qWarning("%s Unexpected state. Cant find registered surface\n", Q_FUNC_INFO);
}

void QWaylandCompositorPrivate::feedRetainedSelectionData(QMimeData *data)
{
    Q_Q(QWaylandCompositor);
    if (retainSelection)
        q->retainedSelectionReceived(data);
}

void QWaylandCompositorPrivate::addPolishObject(QObject *object)
{
    if (initialized) {
        QCoreApplication::postEvent(object, new QEvent(QEvent::Polish));
    } else {
        polish_objects.append(object);
    }
}

/*!
  \qmlsignal void QtWaylandCompositor::WaylandCompositor::createSurface(object client, int id, int version)

  This signal is emitted when a client has created a surface.
  The slot connecting to this signal may create and initialize
  a WaylandSurface instance in the scope of the slot.
  Otherwise a default surface is created.
*/

/*!
  \fn void QWaylandCompositor::createSurface(QWaylandClient *client, uint id, int version)

  This signal is emitted when a client has created a surface.
  The slot connecting to this signal may create and initialize
  a QWaylandSurface instance in the scope of the slot.
  Otherwise a default surface is created.

  Connections to this signal must be of Qt::DirectConnection connection type.
*/

/*
  \qmlsignal void surfaceCreated(QWaylandSurface *surface)

  This signal is emitted when a new WaylandSurface instance has been created.
*/

/*
  \fn void surfaceCreated(QWaylandSurface *surface)

  This signal is emitted when a new QWaylandSurface instance has been created.
*/


void QWaylandCompositorPrivate::compositor_create_surface(wl_compositor::Resource *resource, uint32_t id)
{
    Q_Q(QWaylandCompositor);
    QWaylandClient *client = QWaylandClient::fromWlClient(q, resource->client());
    emit q->surfaceRequested(client, id, resource->version());
#ifndef QT_NO_DEBUG
    Q_ASSERT_X(!QWaylandSurfacePrivate::hasUninitializedSurface(), "QWaylandCompositor", QStringLiteral("Found uninitialized QWaylandSurface after emitting QWaylandCompositor::createSurface for id %1. All surfaces has to be initialized immediately after creation. See QWaylandSurface::initialize.").arg(id).toLocal8Bit().constData());
#endif
    struct wl_resource *surfResource = wl_client_get_object(client->client(), id);

    QWaylandSurface *surface;
    if (surfResource) {
        surface = QWaylandSurface::fromResource(surfResource);
    } else {
        surface = createDefaultSurface();
        surface->initialize(q, client, id, resource->version());
    }
    Q_ASSERT(surface);
    all_surfaces.append(surface);
    emit q->surfaceCreated(surface);
}

void QWaylandCompositorPrivate::compositor_create_region(wl_compositor::Resource *resource, uint32_t id)
{
    new QtWayland::Region(resource->client(), id);
}

void QWaylandCompositorPrivate::subcompositor_get_subsurface(wl_subcompositor::Resource *resource, uint32_t id, wl_resource *surface, wl_resource *parent)
{
    Q_Q(QWaylandCompositor);
    QWaylandSurface *childSurface = QWaylandSurface::fromResource(surface);
    QWaylandSurface *parentSurface = QWaylandSurface::fromResource(parent);
    QWaylandSurfacePrivate::get(childSurface)->initSubsurface(parentSurface, resource->client(), id, 1);
    emit q->subsurfaceChanged(childSurface, parentSurface);
}

/*!
  \internal
  Used to create a fallback QWaylandSurface when no surface was
  created by emitting the QWaylandCompositor::createSurface signal.
*/
QWaylandSurface *QWaylandCompositorPrivate::createDefaultSurface()
{
    return new QWaylandSurface();
}


void QWaylandCompositorPrivate::initializeHardwareIntegration()
{
#if QT_CONFIG(opengl)
    Q_Q(QWaylandCompositor);
    if (use_hw_integration_extension)
        hw_integration.reset(new QtWayland::HardwareIntegration(q));

    loadClientBufferIntegration();
    loadServerBufferIntegration();

    if (client_buffer_integration)
        client_buffer_integration->initializeHardware(display);
    if (server_buffer_integration)
        server_buffer_integration->initializeHardware(q);
#endif
}

void QWaylandCompositorPrivate::initializeSeats()
{
    for (QWaylandSeat *seat : qAsConst(seats))
        seat->initialize();
}

void QWaylandCompositorPrivate::loadClientBufferIntegration()
{
#if QT_CONFIG(opengl)
    Q_Q(QWaylandCompositor);
    QStringList keys = QtWayland::ClientBufferIntegrationFactory::keys();
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
        client_buffer_integration.reset(QtWayland::ClientBufferIntegrationFactory::create(targetKey, QStringList()));
        if (client_buffer_integration) {
            client_buffer_integration->setCompositor(q);
            if (hw_integration)
                hw_integration->setClientBufferIntegration(targetKey);
        }
    }
    //BUG: if there is no client buffer integration, bad things will happen when opengl is used
#endif
}

void QWaylandCompositorPrivate::loadServerBufferIntegration()
{
#if QT_CONFIG(opengl)
    QStringList keys = QtWayland::ServerBufferIntegrationFactory::keys();
    QString targetKey;
    QByteArray serverBufferIntegration = qgetenv("QT_WAYLAND_SERVER_BUFFER_INTEGRATION");
    if (keys.contains(QString::fromLocal8Bit(serverBufferIntegration.constData()))) {
        targetKey = QString::fromLocal8Bit(serverBufferIntegration.constData());
    }
    if (!targetKey.isEmpty()) {
        server_buffer_integration.reset(QtWayland::ServerBufferIntegrationFactory::create(targetKey, QStringList()));
        if (hw_integration)
            hw_integration->setServerBufferIntegration(targetKey);
    }
#endif
}

/*!
  \qmltype WaylandCompositor
  \inqmlmodule QtWayland.Compositor
  \since 5.8
  \brief Manages the Wayland display server.

  The WaylandCompositor manages the connections to the clients, as well as the different
  \l{WaylandOutput}{outputs} and \l{QWaylandSeat}{seats}.

  Normally, a compositor application will have a single WaylandCompositor
  instance, which can have several outputs as children. When a client
  requests the compositor to create a surface, the request is handled by
  the onSurfaceRequested handler.

  Extensions that are supported by the compositor should be instantiated and added to the
  extensions property.
*/


/*!
   \class QWaylandCompositor
   \inmodule QtWaylandCompositor
   \since 5.8
   \brief The QWaylandCompositor class manages the Wayland display server.

   The QWaylandCompositor manages the connections to the clients, as well as the different \l{QWaylandOutput}{outputs}
   and \l{QWaylandSeat}{seats}.

   Normally, a compositor application will have a single WaylandCompositor
   instance, which can have several outputs as children.
*/

/*!
 * Constructs a QWaylandCompositor with the given \a parent.
 */
QWaylandCompositor::QWaylandCompositor(QObject *parent)
    : QWaylandObject(*new QWaylandCompositorPrivate(this), parent)
{
}

/*!
 * \internal
 * Constructs a QWaylandCompositor with the private object \a dptr and \a parent.
 */
QWaylandCompositor::QWaylandCompositor(QWaylandCompositorPrivate &dptr, QObject *parent)
    : QWaylandObject(dptr, parent)
{
}

/*!
 * Destroys the QWaylandCompositor
 */
QWaylandCompositor::~QWaylandCompositor()
{
}

/*!
 * Initializes the QWaylandCompositor.
 * If you override this function in your subclass, be sure to call the base class implementation.
 */
void QWaylandCompositor::create()
{
    Q_D(QWaylandCompositor);
    d->preInit();
    d->init();
}

/*!
 * \qmlproperty bool QtWaylandCompositor::WaylandCompositor::created
 *
 * This property is true if WaylandCompositor has been initialized,
 * otherwise it's false.
 */

/*!
 * \property QWaylandCompositor::created
 *
 * This property is true if QWaylandCompositor has been initialized,
 * otherwise it's false.
 */
bool QWaylandCompositor::isCreated() const
{
    Q_D(const QWaylandCompositor);
    return d->initialized;
}

/*!
 * \qmlproperty string QtWaylandCompositor::WaylandCompositor::socketName
 *
 * This property holds the socket name used by WaylandCompositor to communicate with
 * clients. It must be set before the component is completed.
 *
 * If the socketName is empty (the default), the contents of the start argument
 * \c --wayland-socket-name are used instead. If the argument is not set, the
 * compositor tries to find a socket name, which is \c{wayland-0} by default.
 */

/*!
 * \property QWaylandCompositor::socketName
 *
 * This property holds the socket name used by QWaylandCompositor to communicate with
 * clients. This must be set before the QWaylandCompositor is \l{create()}{created}.
 *
 * If the socketName is empty (the default), the contents of the start argument
 * \c --wayland-socket-name are used instead. If the argument is not set, the
 * compositor tries to find a socket name, which is \c{wayland-0} by default.
 */
void QWaylandCompositor::setSocketName(const QByteArray &name)
{
    Q_D(QWaylandCompositor);

    if (d->socket_name == name)
        return;

    if (d->initialized) {
        qWarning("%s: Changing socket name after initializing the compositor is not supported.\n", Q_FUNC_INFO);
        return;
    }

    d->socket_name = name;
    emit socketNameChanged(name);
}

QByteArray QWaylandCompositor::socketName() const
{
    Q_D(const QWaylandCompositor);
    return d->socket_name;
}

/*!
 * \internal
 */
struct wl_display *QWaylandCompositor::display() const
{
    Q_D(const QWaylandCompositor);
    return d->display;
}

/*!
 * \internal
 */
uint32_t QWaylandCompositor::nextSerial()
{
    Q_D(QWaylandCompositor);
    return wl_display_next_serial(d->display);
}

/*!
 * \internal
 */
QList<QWaylandClient *>QWaylandCompositor::clients() const
{
    Q_D(const QWaylandCompositor);
    return d->clients;
}

/*!
 * \qmlmethod QtWaylandCompositor::WaylandCompositor::destroyClientForSurface(surface)
 *
 * Destroys the client for the WaylandSurface \a surface.
 */

/*!
 * Destroys the client for the \a surface.
 */
void QWaylandCompositor::destroyClientForSurface(QWaylandSurface *surface)
{
    destroyClient(surface->client());
}

/*!
 * \qmlmethod QtWaylandCompositor::WaylandCompositor::destroyClient(client)
 *
 * Destroys the given WaylandClient \a client.
 */

/*!
 * Destroys the \a client.
 */
void QWaylandCompositor::destroyClient(QWaylandClient *client)
{
    if (!client)
        return;

    QWaylandQtWindowManager *wmExtension = QWaylandQtWindowManager::findIn(this);
    if (wmExtension)
        wmExtension->sendQuitMessage(client);

    wl_client_destroy(client->client());
}

/*!
 * \internal
 */
QList<QWaylandSurface *> QWaylandCompositor::surfacesForClient(QWaylandClient* client) const
{
    Q_D(const QWaylandCompositor);
    QList<QWaylandSurface *> surfs;
    foreach (QWaylandSurface *surface, d->all_surfaces) {
        if (surface->client() == client)
            surfs.append(surface);
    }
    return surfs;
}

/*!
 * \internal
 */
QList<QWaylandSurface *> QWaylandCompositor::surfaces() const
{
    Q_D(const QWaylandCompositor);
    return d->all_surfaces;
}

/*!
 * Returns the QWaylandOutput that is connected to the given \a window.
 */
QWaylandOutput *QWaylandCompositor::outputFor(QWindow *window) const
{
    Q_D(const QWaylandCompositor);
    foreach (QWaylandOutput *output, d->outputs) {
        if (output->window() == window)
            return output;
    }

    return Q_NULLPTR;
}

/*!
 * \qmlproperty object QtWaylandCompositor::WaylandCompositor::defaultOutput
 *
 * This property contains the first in the list of outputs added to the
 * WaylandCompositor, or null if no outputs have been added.
 *
 * Setting a new default output prepends it to the output list, making
 * it the new default, but the previous default is not removed from
 * the list.
 */
/*!
 * \property QWaylandCompositor::defaultOutput
 *
 * This property contains the first in the list of outputs added to the
 * QWaylandCompositor, or null if no outputs have been added.
 *
 * Setting a new default output prepends it to the output list, making
 * it the new default, but the previous default is not removed from
 * the list. If the new default output was already in the list of outputs,
 * it is moved to the beginning of the list.
 */
QWaylandOutput *QWaylandCompositor::defaultOutput() const
{
    Q_D(const QWaylandCompositor);
    return d->defaultOutput();
}

void QWaylandCompositor::setDefaultOutput(QWaylandOutput *output)
{
    Q_D(QWaylandCompositor);
    if (d->outputs.size() && d->outputs.first() == output)
        return;
    d->outputs.removeOne(output);
    d->outputs.prepend(output);
    defaultOutputChanged();
}

/*!
 * \internal
 */
QList<QWaylandOutput *> QWaylandCompositor::outputs() const
{
    Q_D(const QWaylandCompositor);
    return d->outputs;
}

/*!
 * \internal
 */
uint QWaylandCompositor::currentTimeMsecs() const
{
    Q_D(const QWaylandCompositor);
    return d->timer.elapsed();
}

/*!
 * \internal
 */
void QWaylandCompositor::processWaylandEvents()
{
    Q_D(QWaylandCompositor);
    int ret = wl_event_loop_dispatch(d->loop, 0);
    if (ret)
        fprintf(stderr, "wl_event_loop_dispatch error: %d\n", ret);
    wl_display_flush_clients(d->display);
}

/*!
 * \internal
 */
QWaylandSeat *QWaylandCompositor::createSeat()
{
    return new QWaylandSeat(this);
}

/*!
 * \internal
 */
QWaylandPointer *QWaylandCompositor::createPointerDevice(QWaylandSeat *seat)
{
    return new QWaylandPointer(seat);
}

/*!
 * \internal
 */
QWaylandKeyboard *QWaylandCompositor::createKeyboardDevice(QWaylandSeat *seat)
{
    return new QWaylandKeyboard(seat);
}

/*!
 * \internal
 */
QWaylandTouch *QWaylandCompositor::createTouchDevice(QWaylandSeat *seat)
{
    return new QWaylandTouch(seat);
}

/*!
 * \qmlproperty bool QtWaylandCompositor::WaylandCompositor::retainedSelection
 *
 * This property holds whether retained selection is enabled.
 */

/*!
 * \property QWaylandCompositor::retainedSelection
 *
 * This property holds whether retained selection is enabled.
 */
void QWaylandCompositor::setRetainedSelectionEnabled(bool enabled)
{
    Q_D(QWaylandCompositor);

    if (d->retainSelection == enabled)
        return;

    d->retainSelection = enabled;
    emit retainedSelectionChanged(enabled);
}

bool QWaylandCompositor::retainedSelectionEnabled() const
{
    Q_D(const QWaylandCompositor);
    return d->retainSelection;
}

/*!
 * \internal
 */
void QWaylandCompositor::retainedSelectionReceived(QMimeData *)
{
}

/*!
 * \internal
 */
void QWaylandCompositor::overrideSelection(const QMimeData *data)
{
    Q_D(QWaylandCompositor);
    d->data_device_manager->overrideSelection(*data);
}

/*!
 * \qmlproperty object QtWaylandCompositor::WaylandCompositor::defaultSeat
 *
 * This property contains the default seat for this
 * WaylandCompositor.
 */

/*!
 * \property QWaylandCompositor::defaultSeat
 *
 * This property contains the default seat for this
 * QWaylandCompositor.
 */
QWaylandSeat *QWaylandCompositor::defaultSeat() const
{
    Q_D(const QWaylandCompositor);
    if (d->seats.size())
        return d->seats.first();
    return Q_NULLPTR;
}

/*!
 * \internal
 *
 * Currently, Qt only supports a single seat, so this exists for
 * future proofing the APIs.
 */
QWaylandSeat *QWaylandCompositor::seatFor(QInputEvent *inputEvent)
{
    Q_D(QWaylandCompositor);
    QWaylandSeat *dev = NULL;
    for (int i = 0; i < d->seats.size(); i++) {
        QWaylandSeat *candidate = d->seats.at(i);
        if (candidate->isOwner(inputEvent)) {
            dev = candidate;
            break;
        }
    }
    return dev;
}

/*!
 * \qmlproperty bool QtWaylandCompositor::WaylandCompositor::useHardwareIntegrationExtension
 *
 * This property holds whether the hardware integration extension should be enabled for
 * this WaylandCompositor.
 *
 * This property must be set before the compositor component is completed.
 */

/*!
 * \property QWaylandCompositor::useHardwareIntegrationExtension
 *
 * This property holds whether the hardware integration extension should be enabled for
 * this QWaylandCompositor.
 *
 * This property must be set before the compositor is \l{create()}{created}.
 */
bool QWaylandCompositor::useHardwareIntegrationExtension() const
{
#if QT_CONFIG(opengl)
    Q_D(const QWaylandCompositor);
    return d->use_hw_integration_extension;
#else
    return false;
#endif
}

void QWaylandCompositor::setUseHardwareIntegrationExtension(bool use)
{
#if QT_CONFIG(opengl)
    Q_D(QWaylandCompositor);
    if (use == d->use_hw_integration_extension)
        return;

    if (d->initialized)
        qWarning("Setting QWaylandCompositor::useHardwareIntegrationExtension after initialization has no effect");

    d->use_hw_integration_extension = use;
    useHardwareIntegrationExtensionChanged();
#else
    if (use)
        qWarning() << "Hardware integration not supported without OpenGL support";
#endif
}

/*!
 * Grab the surface content from the given \a buffer.
 * The default implementation requires a OpenGL context to be bound to the current thread
 * to work. If this is not possible, reimplement this function in your compositor subclass
 * to implement custom logic.
 * The default implementation only grabs shared memory and OpenGL buffers, reimplement this in your
 * compositor subclass to handle more buffer types.
 * \note You should not call this manually, but rather use QWaylandSurfaceGrabber (\a grabber).
 */
void QWaylandCompositor::grabSurface(QWaylandSurfaceGrabber *grabber, const QWaylandBufferRef &buffer)
{
    if (buffer.isSharedMemory()) {
        emit grabber->success(buffer.image());
    } else {
#if QT_CONFIG(opengl)
        if (QOpenGLContext::currentContext()) {
            QOpenGLFramebufferObject fbo(buffer.size());
            fbo.bind();
            QOpenGLTextureBlitter blitter;
            blitter.create();


            glViewport(0, 0, buffer.size().width(), buffer.size().height());

            QOpenGLTextureBlitter::Origin surfaceOrigin =
                buffer.origin() == QWaylandSurface::OriginTopLeft
                ? QOpenGLTextureBlitter::OriginTopLeft
                : QOpenGLTextureBlitter::OriginBottomLeft;

            auto texture = buffer.toOpenGLTexture();
            blitter.bind(texture->target());
            blitter.blit(texture->textureId(), QMatrix4x4(), surfaceOrigin);
            blitter.release();

            emit grabber->success(fbo.toImage());
        } else
#endif
        emit grabber->failed(QWaylandSurfaceGrabber::UnknownBufferType);
    }
}

QT_END_NAMESPACE
