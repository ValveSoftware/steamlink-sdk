/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "qwaylandxdgshellv5.h"
#include "qwaylandxdgshellv5_p.h"

#ifdef QT_WAYLAND_COMPOSITOR_QUICK
#include "qwaylandxdgshellv5integration_p.h"
#endif

#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandSurface>
#include <QtWaylandCompositor/QWaylandSurfaceRole>
#include <QtWaylandCompositor/QWaylandResource>
#include <QtWaylandCompositor/QWaylandSeat>

#include <QtCore/QObject>

#include <algorithm>

QT_BEGIN_NAMESPACE

QWaylandSurfaceRole QWaylandXdgSurfaceV5Private::s_role("xdg_surface");
QWaylandSurfaceRole QWaylandXdgPopupV5Private::s_role("xdg_popup");

QWaylandXdgShellV5Private::QWaylandXdgShellV5Private()
    : QWaylandCompositorExtensionPrivate()
    , xdg_shell()
{
}

void QWaylandXdgShellV5Private::ping(Resource *resource, uint32_t serial)
{
    m_pings.insert(serial);
    send_ping(resource->handle, serial);
}

void QWaylandXdgShellV5Private::registerSurface(QWaylandXdgSurfaceV5 *xdgSurface)
{
    m_xdgSurfaces.insert(xdgSurface->surface()->client()->client(), xdgSurface);
}

void QWaylandXdgShellV5Private::unregisterXdgSurface(QWaylandXdgSurfaceV5 *xdgSurface)
{
    auto xdgSurfacePrivate = QWaylandXdgSurfaceV5Private::get(xdgSurface);
    if (!m_xdgSurfaces.remove(xdgSurfacePrivate->resource()->client(), xdgSurface))
        qWarning("%s Unexpected state. Can't find registered xdg surface\n", Q_FUNC_INFO);
}

void QWaylandXdgShellV5Private::registerXdgPopup(QWaylandXdgPopupV5 *xdgPopup)
{
    m_xdgPopups.insert(xdgPopup->surface()->client()->client(), xdgPopup);
}

void QWaylandXdgShellV5Private::unregisterXdgPopup(QWaylandXdgPopupV5 *xdgPopup)
{
    auto xdgPopupPrivate = QWaylandXdgPopupV5Private::get(xdgPopup);
    if (!m_xdgPopups.remove(xdgPopupPrivate->resource()->client(), xdgPopup))
        qWarning("%s Unexpected state. Can't find registered xdg popup\n", Q_FUNC_INFO);
}

bool QWaylandXdgShellV5Private::isValidPopupParent(QWaylandSurface *parentSurface) const
{
    QWaylandXdgPopupV5 *topmostPopup = topmostPopupForClient(parentSurface->client()->client());
    if (topmostPopup && topmostPopup->surface() != parentSurface) {
        return false;
    }

    QWaylandSurfaceRole *parentRole = parentSurface->role();
    if (parentRole != QWaylandXdgSurfaceV5::role() && parentRole != QWaylandXdgPopupV5::role()) {
        return false;
    }

    return true;
}

QWaylandXdgPopupV5 *QWaylandXdgShellV5Private::topmostPopupForClient(wl_client *client) const
{
    QList<QWaylandXdgPopupV5 *> clientPopups = m_xdgPopups.values(client);
    return clientPopups.empty() ? nullptr : clientPopups.last();
}

QWaylandXdgSurfaceV5 *QWaylandXdgShellV5Private::xdgSurfaceFromSurface(QWaylandSurface *surface)
{
    Q_FOREACH (QWaylandXdgSurfaceV5 *xdgSurface, m_xdgSurfaces) {
        if (surface == xdgSurface->surface())
            return xdgSurface;
    }
    return nullptr;
}

void QWaylandXdgShellV5Private::xdg_shell_destroy(Resource *resource)
{
    if (!m_xdgSurfaces.values(resource->client()).empty())
        wl_resource_post_error(resource->handle, XDG_SHELL_ERROR_DEFUNCT_SURFACES,
                               "xdg_shell was destroyed before children");

    wl_resource_destroy(resource->handle);
}

void QWaylandXdgShellV5Private::xdg_shell_get_xdg_surface(Resource *resource, uint32_t id,
                                                        wl_resource *surface_res)
{
    Q_Q(QWaylandXdgShellV5);
    QWaylandSurface *surface = QWaylandSurface::fromResource(surface_res);

    if (xdgSurfaceFromSurface(surface)) {
        wl_resource_post_error(resource->handle, XDG_SHELL_ERROR_ROLE,
                               "An active xdg_surface already exists for wl_surface@%d",
                               wl_resource_get_id(surface->resource()));
        return;
    }

    if (!surface->setRole(QWaylandXdgSurfaceV5::role(), resource->handle, XDG_SHELL_ERROR_ROLE))
        return;

    QWaylandResource xdgSurfaceResource(wl_resource_create(resource->client(), &xdg_surface_interface,
                                                           wl_resource_get_version(resource->handle), id));

    emit q->xdgSurfaceRequested(surface, xdgSurfaceResource);

    QWaylandXdgSurfaceV5 *xdgSurface = QWaylandXdgSurfaceV5::fromResource(xdgSurfaceResource.resource());
    if (!xdgSurface) {
        // A QWaylandXdgSurfaceV5 was not created in response to the xdgSurfaceRequested signal, so we
        // create one as fallback here instead.
        xdgSurface = new QWaylandXdgSurfaceV5(q, surface, xdgSurfaceResource);
    }

    registerSurface(xdgSurface);
    emit q->xdgSurfaceCreated(xdgSurface);
}

void QWaylandXdgShellV5Private::xdg_shell_use_unstable_version(Resource *resource, int32_t version)
{
    if (xdg_shell::version_current != version) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "incompatible version, server is %d, but client wants %d",
                               xdg_shell::version_current, version);
    }
}

void QWaylandXdgShellV5Private::xdg_shell_get_xdg_popup(Resource *resource, uint32_t id,
                                                      wl_resource *surface_res, wl_resource *parent,
                                                      wl_resource *seatResource, uint32_t serial,
                                                      int32_t x, int32_t y)
{
    Q_UNUSED(serial);
    Q_Q(QWaylandXdgShellV5);
    QWaylandSurface *surface = QWaylandSurface::fromResource(surface_res);
    QWaylandSurface *parentSurface = QWaylandSurface::fromResource(parent);

    if (!isValidPopupParent(parentSurface)) {
        wl_resource_post_error(resource->handle, XDG_SHELL_ERROR_INVALID_POPUP_PARENT,
                               "the client specified an invalid popup parent surface");
        return;
    }

    if (!surface->setRole(QWaylandXdgPopupV5::role(), resource->handle, XDG_SHELL_ERROR_ROLE)) {
        return;
    }

    QWaylandResource xdgPopupResource (wl_resource_create(resource->client(), &xdg_popup_interface,
                                                          wl_resource_get_version(resource->handle), id));
    QWaylandSeat *seat = QWaylandSeat::fromSeatResource(seatResource);
    QPoint position(x, y);
    emit q->xdgPopupRequested(surface, parentSurface, seat, position, xdgPopupResource);

    QWaylandXdgPopupV5 *xdgPopup = QWaylandXdgPopupV5::fromResource(xdgPopupResource.resource());
    if (!xdgPopup) {
        // A QWaylandXdgPopupV5 was not created in response to the xdgPopupRequested signal, so we
        // create one as fallback here instead.
        xdgPopup = new QWaylandXdgPopupV5(q, surface, parentSurface, position, xdgPopupResource);
    }

    registerXdgPopup(xdgPopup);
    emit q->xdgPopupCreated(xdgPopup);
}

void QWaylandXdgShellV5Private::xdg_shell_pong(Resource *resource, uint32_t serial)
{
    Q_UNUSED(resource);
    Q_Q(QWaylandXdgShellV5);
    if (m_pings.remove(serial))
        emit q->pong(serial);
    else
        qWarning("Received an unexpected pong!");
}

QWaylandXdgSurfaceV5Private::QWaylandXdgSurfaceV5Private()
    : QWaylandCompositorExtensionPrivate()
    , xdg_surface()
    , m_surface(nullptr)
    , m_parentSurface(nullptr)
    , m_windowType(UnknownWindowType)
    , m_unsetWindowGeometry(true)
    , m_lastAckedConfigure({{}, QSize(0, 0), 0})
{
}

void QWaylandXdgSurfaceV5Private::handleFocusLost()
{
    Q_Q(QWaylandXdgSurfaceV5);
    QWaylandXdgSurfaceV5Private::ConfigureEvent current = lastSentConfigure();
    current.states.removeOne(QWaylandXdgSurfaceV5::State::ActivatedState);
    q->sendConfigure(current.size, current.states);
}

void QWaylandXdgSurfaceV5Private::handleFocusReceived()
{
    Q_Q(QWaylandXdgSurfaceV5);

    QWaylandXdgSurfaceV5Private::ConfigureEvent current = lastSentConfigure();
    if (!current.states.contains(QWaylandXdgSurfaceV5::State::ActivatedState)) {
        current.states.push_back(QWaylandXdgSurfaceV5::State::ActivatedState);
    }

    q->sendConfigure(current.size, current.states);
}

QRect QWaylandXdgSurfaceV5Private::calculateFallbackWindowGeometry() const
{
    // TODO: The unset window geometry should include subsurfaces as well, so this solution
    // won't work too well on those kinds of clients.
    return QRect(QPoint(0, 0), m_surface->size() / m_surface->bufferScale());
}

void QWaylandXdgSurfaceV5Private::updateFallbackWindowGeometry()
{
    Q_Q(QWaylandXdgSurfaceV5);
    if (!m_unsetWindowGeometry)
        return;

    const QRect unsetGeometry = calculateFallbackWindowGeometry();
    if (unsetGeometry == m_windowGeometry)
        return;

    m_windowGeometry = unsetGeometry;
    emit q->windowGeometryChanged();
}

void QWaylandXdgSurfaceV5Private::xdg_surface_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource);
    Q_Q(QWaylandXdgSurfaceV5);
    QWaylandXdgShellV5Private::get(m_xdgShell)->unregisterXdgSurface(q);
    delete q;
}

void QWaylandXdgSurfaceV5Private::xdg_surface_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void QWaylandXdgSurfaceV5Private::xdg_surface_move(Resource *resource, wl_resource *seat, uint32_t serial)
{
    Q_UNUSED(resource);
    Q_UNUSED(serial);

    Q_Q(QWaylandXdgSurfaceV5);
    QWaylandSeat *input_device = QWaylandSeat::fromSeatResource(seat);
    emit q->startMove(input_device);
}

void QWaylandXdgSurfaceV5Private::xdg_surface_resize(Resource *resource, wl_resource *seat,
                                                   uint32_t serial, uint32_t edges)
{
    Q_UNUSED(resource);
    Q_UNUSED(serial);

    Q_Q(QWaylandXdgSurfaceV5);
    QWaylandSeat *input_device = QWaylandSeat::fromSeatResource(seat);
    emit q->startResize(input_device, QWaylandXdgSurfaceV5::ResizeEdge(edges));
}

void QWaylandXdgSurfaceV5Private::xdg_surface_set_maximized(Resource *resource)
{
    Q_UNUSED(resource);
    Q_Q(QWaylandXdgSurfaceV5);
    emit q->setMaximized();
}

void QWaylandXdgSurfaceV5Private::xdg_surface_unset_maximized(Resource *resource)
{
    Q_UNUSED(resource);
    Q_Q(QWaylandXdgSurfaceV5);
    emit q->unsetMaximized();
}

void QWaylandXdgSurfaceV5Private::xdg_surface_set_fullscreen(Resource *resource, wl_resource *output_res)
{
    Q_UNUSED(resource);
    Q_Q(QWaylandXdgSurfaceV5);
    QWaylandOutput *output = output_res ? QWaylandOutput::fromResource(output_res) : nullptr;
    emit q->setFullscreen(output);
}

void QWaylandXdgSurfaceV5Private::xdg_surface_unset_fullscreen(Resource *resource)
{
    Q_UNUSED(resource);
    Q_Q(QWaylandXdgSurfaceV5);
    emit q->unsetFullscreen();
}

void QWaylandXdgSurfaceV5Private::xdg_surface_set_minimized(Resource *resource)
{
    Q_UNUSED(resource);
    Q_Q(QWaylandXdgSurfaceV5);
    emit q->setMinimized();
}

void QWaylandXdgSurfaceV5Private::xdg_surface_set_parent(Resource *resource, wl_resource *parent)
{
    Q_UNUSED(resource);
    QWaylandXdgSurfaceV5 *parentSurface = nullptr;
    if (parent) {
        parentSurface = static_cast<QWaylandXdgSurfaceV5Private *>(
                    QWaylandXdgSurfaceV5Private::Resource::fromResource(parent)->xdg_surface_object)->q_func();
    }

    Q_Q(QWaylandXdgSurfaceV5);

    if (m_parentSurface != parentSurface) {
        m_parentSurface = parentSurface;
        emit q->parentSurfaceChanged();
    }

    if (m_parentSurface && m_windowType != TransientWindowType) {
        // There's a parent now, which means the surface is transient
        m_windowType = TransientWindowType;
        emit q->setTransient();
    } else if (!m_parentSurface && m_windowType != TopLevelWindowType) {
        // When the surface has no parent it is toplevel
        m_windowType = TopLevelWindowType;
        emit q->setTopLevel();
    }
}

void QWaylandXdgSurfaceV5Private::xdg_surface_set_app_id(Resource *resource, const QString &app_id)
{
    Q_UNUSED(resource);
    if (app_id == m_appId)
        return;
    Q_Q(QWaylandXdgSurfaceV5);
    m_appId = app_id;
    emit q->appIdChanged();
}

void QWaylandXdgSurfaceV5Private::xdg_surface_show_window_menu(Resource *resource, wl_resource *seatResource,
                                                             uint32_t serial, int32_t x, int32_t y)
{
    Q_UNUSED(resource);
    Q_UNUSED(serial);
    QPoint position(x, y);
    auto seat = QWaylandSeat::fromSeatResource(seatResource);
    Q_Q(QWaylandXdgSurfaceV5);
    emit q->showWindowMenu(seat, position);
}

void QWaylandXdgSurfaceV5Private::xdg_surface_ack_configure(Resource *resource, uint32_t serial)
{
    Q_UNUSED(resource);
    Q_Q(QWaylandXdgSurfaceV5);

    ConfigureEvent config;
    Q_FOREVER {
        if (m_pendingConfigures.empty()) {
            qWarning("Received an unexpected ack_configure!");
            return;
        }

        config = m_pendingConfigures.takeFirst();

        if (config.serial == serial)
            break;
    }

    QVector<uint> changedStates;
    std::set_symmetric_difference(
                m_lastAckedConfigure.states.begin(), m_lastAckedConfigure.states.end(),
                config.states.begin(), config.states.end(),
                std::back_inserter(changedStates));

    m_lastAckedConfigure = config;

    if (!changedStates.empty()) {
        Q_FOREACH (uint state, changedStates) {
            switch (state) {
            case QWaylandXdgSurfaceV5::State::MaximizedState:
                emit q->maximizedChanged();
                break;
            case QWaylandXdgSurfaceV5::State::FullscreenState:
                emit q->fullscreenChanged();
                break;
            case QWaylandXdgSurfaceV5::State::ResizingState:
                emit q->resizingChanged();
                break;
            case QWaylandXdgSurfaceV5::State::ActivatedState:
                emit q->activatedChanged();
                break;
            }
        }
        emit q->statesChanged();
    }

    emit q->ackConfigure(serial);
}

void QWaylandXdgSurfaceV5Private::xdg_surface_set_title(Resource *resource, const QString &title)
{
    Q_UNUSED(resource);
    if (title == m_title)
        return;
    Q_Q(QWaylandXdgSurfaceV5);
    m_title = title;
    emit q->titleChanged();
}

void QWaylandXdgSurfaceV5Private::xdg_surface_set_window_geometry(Resource *resource,
                                                                int32_t x, int32_t y,
                                                                int32_t width, int32_t height)
{
    Q_UNUSED(resource);

    if (width <= 0 || height <= 0) {
        qWarning() << "Invalid (non-positive) dimensions received in set_window_geometry";
        return;
    }

    m_unsetWindowGeometry = false;

    QRect geometry(x, y, width, height);

    Q_Q(QWaylandXdgSurfaceV5);
    if ((q->maximized() || q->fullscreen()) && m_lastAckedConfigure.size != geometry.size())
        qWarning() << "Client window geometry did not obey last acked configure";

    if (geometry == m_windowGeometry)
        return;

    m_windowGeometry = geometry;
    emit q->windowGeometryChanged();
}

QWaylandXdgPopupV5Private::QWaylandXdgPopupV5Private()
    : QWaylandCompositorExtensionPrivate()
    , xdg_popup()
    , m_surface(nullptr)
    , m_parentSurface(nullptr)
    , m_xdgShell(nullptr)
{
}

void QWaylandXdgPopupV5Private::xdg_popup_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource);
    Q_Q(QWaylandXdgPopupV5);
    QWaylandXdgShellV5Private::get(m_xdgShell)->unregisterXdgPopup(q);
    delete q;
}

void QWaylandXdgPopupV5Private::xdg_popup_destroy(Resource *resource)
{
    //TODO: post error if not topmost popup
    wl_resource_destroy(resource->handle);
}

/*!
 * Constructs a QWaylandXdgShellV5 object.
 */
QWaylandXdgShellV5::QWaylandXdgShellV5()
    : QWaylandShellTemplate<QWaylandXdgShellV5>(*new QWaylandXdgShellV5Private())
{ }

/*!
 * Constructs a QWaylandXdgShellV5 object for the provided \a compositor.
 */
QWaylandXdgShellV5::QWaylandXdgShellV5(QWaylandCompositor *compositor)
    : QWaylandShellTemplate<QWaylandXdgShellV5>(compositor, *new QWaylandXdgShellV5Private())
{ }

/*!
 * Initializes the shell extension.
 */
void QWaylandXdgShellV5::initialize()
{
    Q_D(QWaylandXdgShellV5);
    QWaylandShellTemplate::initialize();
    QWaylandCompositor *compositor = static_cast<QWaylandCompositor *>(extensionContainer());
    if (!compositor) {
        qWarning() << "Failed to find QWaylandCompositor when initializing QWaylandXdgShellV5";
        return;
    }
    d->init(compositor->display(), 1);

    handleSeatChanged(compositor->defaultSeat(), nullptr);

    connect(compositor, &QWaylandCompositor::defaultSeatChanged,
            this, &QWaylandXdgShellV5::handleSeatChanged);
}

QWaylandClient *QWaylandXdgShellV5::popupClient() const
{
    Q_D(const QWaylandXdgShellV5);
    Q_FOREACH (QWaylandXdgPopupV5 *popup, d->m_xdgPopups) {
        if (popup->surface()->hasContent())
            return popup->surface()->client();
    }
    return nullptr;
}

/*!
 * Returns the Wayland interface for the QWaylandXdgShellV5.
 */
const struct wl_interface *QWaylandXdgShellV5::interface()
{
    return QWaylandXdgShellV5Private::interface();
}

QByteArray QWaylandXdgShellV5::interfaceName()
{
    return QWaylandXdgShellV5Private::interfaceName();
}

/*!
 * \qmlmethod void QtWaylandCompositor::XdgSurface::ping()
 *
 * Sends a ping event to the client. If the client replies to the event the
 * \a pong signal will be emitted.
 */

/*!
 * Sends a ping event to the client. If the client replies to the event the
 * \a pong signal will be emitted.
 */
uint QWaylandXdgShellV5::ping(QWaylandClient *client)
{
    Q_D(QWaylandXdgShellV5);

    QWaylandCompositor *compositor = static_cast<QWaylandCompositor *>(extensionContainer());
    Q_ASSERT(compositor);

    uint32_t serial = compositor->nextSerial();

    QWaylandXdgShellV5Private::Resource *clientResource = d->resourceMap().value(client->client(), nullptr);
    Q_ASSERT(clientResource);

    d->ping(clientResource, serial);
    return serial;
}

void QWaylandXdgShellV5::closeAllPopups()
{
    Q_D(QWaylandXdgShellV5);
    Q_FOREACH (struct wl_client *client, d->m_xdgPopups.keys()) {
        QList<QWaylandXdgPopupV5 *> popups = d->m_xdgPopups.values(client);
        std::reverse(popups.begin(), popups.end());
        Q_FOREACH (QWaylandXdgPopupV5 *currentTopmostPopup, popups) {
            currentTopmostPopup->sendPopupDone();
        }
    }
}

void QWaylandXdgShellV5::handleSeatChanged(QWaylandSeat *newSeat, QWaylandSeat *oldSeat)
{
    if (oldSeat != nullptr) {
        disconnect(oldSeat, &QWaylandSeat::keyboardFocusChanged,
                   this, &QWaylandXdgShellV5::handleFocusChanged);
    }

    if (newSeat != nullptr) {
        connect(newSeat, &QWaylandSeat::keyboardFocusChanged,
                this, &QWaylandXdgShellV5::handleFocusChanged);
    }
}

void QWaylandXdgShellV5::handleFocusChanged(QWaylandSurface *newSurface, QWaylandSurface *oldSurface)
{
    Q_D(QWaylandXdgShellV5);

    QWaylandXdgSurfaceV5 *newXdgSurface = d->xdgSurfaceFromSurface(newSurface);
    QWaylandXdgSurfaceV5 *oldXdgSurface = d->xdgSurfaceFromSurface(oldSurface);

    if (newXdgSurface)
        QWaylandXdgSurfaceV5Private::get(newXdgSurface)->handleFocusReceived();

    if (oldXdgSurface)
        QWaylandXdgSurfaceV5Private::get(oldXdgSurface)->handleFocusLost();
}

/*!
 * \class QWaylandXdgSurfaceV5
 * \inmodule QtWaylandCompositor
 * \since 5.8
 * \brief The QWaylandXdgSurfaceV5 class provides desktop-style compositor-specific features to an xdg surface.
 *
 * This class is part of the QWaylandXdgShellV5 extension and provides a way to
 * extend the functionality of an existing QWaylandSurface with features
 * specific to desktop-style compositors, such as resizing and moving the
 * surface.
 *
 * It corresponds to the Wayland interface xdg_surface.
 */

/*!
 * \qmlsignal QtWaylandCompositor::XdgSurface::setTopLevel()
 *
 * This signal is emitted when the parent surface is unset, effectively
 * making the window top level.
 */

/*!
 * \qmlsignal QtWaylandCompositor::XdgSurface::setTransient()
 *
 * This signal is emitted when the parent surface is set, effectively
 * making the window transient.
 */

/*!
 * Constructs a QWaylandXdgSurfaceV5.
 */
QWaylandXdgSurfaceV5::QWaylandXdgSurfaceV5()
    : QWaylandShellSurfaceTemplate<QWaylandXdgSurfaceV5>(*new QWaylandXdgSurfaceV5Private)
{
}

/*!
 * Constructs a QWaylandXdgSurfaceV5 for \a surface and initializes it with the
 * given \a xdgShell, \a surface, and resource \a res.
 */
QWaylandXdgSurfaceV5::QWaylandXdgSurfaceV5(QWaylandXdgShellV5 *xdgShell, QWaylandSurface *surface, const QWaylandResource &res)
    : QWaylandShellSurfaceTemplate<QWaylandXdgSurfaceV5>(*new QWaylandXdgSurfaceV5Private)
{
    initialize(xdgShell, surface, res);
}

/*!
 * \qmlmethod void QtWaylandCompositor::XdgSurface::initialize(object surface, object client, int id)
 *
 * Initializes the XdgSurface, associating it with the given \a surface,
 * \a client, and \a id.
 */

/*!
 * Initializes the QWaylandXdgSurfaceV5, associating it with the given \a xdgShell, \a surface
 * and \a resource.
 */
void QWaylandXdgSurfaceV5::initialize(QWaylandXdgShellV5 *xdgShell, QWaylandSurface *surface, const QWaylandResource &resource)
{
    Q_D(QWaylandXdgSurfaceV5);
    d->m_xdgShell = xdgShell;
    d->m_surface = surface;
    d->init(resource.resource());
    setExtensionContainer(surface);
    d->m_windowGeometry = d->calculateFallbackWindowGeometry();
    connect(surface, &QWaylandSurface::sizeChanged, this, &QWaylandXdgSurfaceV5::handleSurfaceSizeChanged);
    connect(surface, &QWaylandSurface::bufferScaleChanged, this, &QWaylandXdgSurfaceV5::handleBufferScaleChanged);
    emit shellChanged();
    emit surfaceChanged();
    emit windowGeometryChanged();
    QWaylandCompositorExtension::initialize();
}

/*!
 * \internal
 */
void QWaylandXdgSurfaceV5::initialize()
{
    QWaylandCompositorExtension::initialize();
}

QList<int> QWaylandXdgSurfaceV5::statesAsInts() const
{
   QList<int> list;
   Q_FOREACH (uint state, states()) {
       list << static_cast<int>(state);
   }
   return list;
}

void QWaylandXdgSurfaceV5::handleSurfaceSizeChanged()
{
    Q_D(QWaylandXdgSurfaceV5);
    d->updateFallbackWindowGeometry();
}

void QWaylandXdgSurfaceV5::handleBufferScaleChanged()
{
    Q_D(QWaylandXdgSurfaceV5);
    d->updateFallbackWindowGeometry();
}

/*!
 * \qmlproperty object QtWaylandCompositor::XdgSurface::shell
 *
 * This property holds the shell associated with this XdgSurface.
 */

/*!
 * \property QWaylandXdgSurfaceV5::shell
 *
 * This property holds the shell associated with this QWaylandXdgSurfaceV5.
 */
QWaylandXdgShellV5 *QWaylandXdgSurfaceV5::shell() const
{
    Q_D(const QWaylandXdgSurfaceV5);
    return d->m_xdgShell;
}

/*!
 * \qmlproperty object QtWaylandCompositor::XdgSurface::surface
 *
 * This property holds the surface associated with this XdgSurface.
 */

/*!
 * \property QWaylandXdgSurfaceV5::surface
 *
 * This property holds the surface associated with this QWaylandXdgSurfaceV5.
 */
QWaylandSurface *QWaylandXdgSurfaceV5::surface() const
{
    Q_D(const QWaylandXdgSurfaceV5);
    return d->m_surface;
}

/*!
 * \qmlproperty object QtWaylandCompositor::XdgSurface::parentSurface
 *
 * This property holds the XdgSurface parent of this XdgSurface.
 * When a parent surface is set, the parentSurfaceChanged() signal
 * is guaranteed to be emitted before setTopLevel() and setTransient().
 *
 * \sa QtWaylandCompositor::XdgSurface::setTopLevel()
 * \sa QtWaylandCompositor::XdgSurface::setTransient()
 */

/*!
 * \property QWaylandXdgSurfaceV5::parentSurface
 *
 * This property holds the XdgSurface parent of this XdgSurface.
 * When a parent surface is set, the parentSurfaceChanged() signal
 * is guaranteed to be emitted before setTopLevel() and setTransient().
 *
 * \sa QWaylandXdgSurfaceV5::setTopLevel()
 * \sa QWaylandXdgSurfaceV5::setTransient()
 */
QWaylandXdgSurfaceV5 *QWaylandXdgSurfaceV5::parentSurface() const
{
    Q_D(const QWaylandXdgSurfaceV5);
    return d->m_parentSurface;
}

/*!
 * \qmlproperty string QtWaylandCompositor::XdgSurface::title
 *
 * This property holds the title of the XdgSurface.
 */

/*!
 * \property QWaylandXdgSurfaceV5::title
 *
 * This property holds the title of the QWaylandXdgSurfaceV5.
 */
QString QWaylandXdgSurfaceV5::title() const
{
    Q_D(const QWaylandXdgSurfaceV5);
    return d->m_title;
}

/*!
 * \property QWaylandXdgSurfaceV5::appId
 *
 * This property holds the app id of the QWaylandXdgSurfaceV5.
 */
QString QWaylandXdgSurfaceV5::appId() const
{
    Q_D(const QWaylandXdgSurfaceV5);
    return d->m_appId;
}

/*!
 * \property QWaylandXdgSurfaceV5::windowGeometry
 *
 * This property holds the window geometry of the QWaylandXdgSurfaceV5. The window
 * geometry describes the window's visible bounds from the user's perspective.
 * The geometry includes title bars and borders if drawn by the client, but
 * excludes drop shadows. It is meant to be used for aligning and tiling
 * windows.
 */
QRect QWaylandXdgSurfaceV5::windowGeometry() const
{
    Q_D(const QWaylandXdgSurfaceV5);
    return d->m_windowGeometry;
}

/*!
 * \property QWaylandXdgSurfaceV5::states
 *
 * This property holds the last states the client acknowledged for this QWaylandXdgSurfaceV5.
 */
QVector<uint> QWaylandXdgSurfaceV5::states() const
{
    Q_D(const QWaylandXdgSurfaceV5);
    return d->m_lastAckedConfigure.states;
}

bool QWaylandXdgSurfaceV5::maximized() const
{
    Q_D(const QWaylandXdgSurfaceV5);
    return d->m_lastAckedConfigure.states.contains(QWaylandXdgSurfaceV5::State::MaximizedState);
}

bool QWaylandXdgSurfaceV5::fullscreen() const
{
    Q_D(const QWaylandXdgSurfaceV5);
    return d->m_lastAckedConfigure.states.contains(QWaylandXdgSurfaceV5::State::FullscreenState);
}

bool QWaylandXdgSurfaceV5::resizing() const
{
    Q_D(const QWaylandXdgSurfaceV5);
    return d->m_lastAckedConfigure.states.contains(QWaylandXdgSurfaceV5::State::ResizingState);
}

bool QWaylandXdgSurfaceV5::activated() const
{
    Q_D(const QWaylandXdgSurfaceV5);
    return d->m_lastAckedConfigure.states.contains(QWaylandXdgSurfaceV5::State::ActivatedState);
}

/*!
 * Returns the Wayland interface for the QWaylandXdgSurfaceV5.
 */
const wl_interface *QWaylandXdgSurfaceV5::interface()
{
    return QWaylandXdgSurfaceV5Private::interface();
}

QByteArray QWaylandXdgSurfaceV5::interfaceName()
{
    return QWaylandXdgSurfaceV5Private::interfaceName();
}

/*!
 * Returns the surface role for the QWaylandXdgSurfaceV5.
 */
QWaylandSurfaceRole *QWaylandXdgSurfaceV5::role()
{
    return &QWaylandXdgSurfaceV5Private::s_role;
}

/*!
 * Returns the QWaylandXdgSurfaceV5 corresponding to the \a resource.
 */
QWaylandXdgSurfaceV5 *QWaylandXdgSurfaceV5::fromResource(wl_resource *resource)
{
    auto xsResource = QWaylandXdgSurfaceV5Private::Resource::fromResource(resource);
    if (!xsResource)
        return nullptr;
    return static_cast<QWaylandXdgSurfaceV5Private *>(xsResource->xdg_surface_object)->q_func();
}

QSize QWaylandXdgSurfaceV5::sizeForResize(const QSizeF &size, const QPointF &delta,
                                        QWaylandXdgSurfaceV5::ResizeEdge edge)
{
    qreal width = size.width();
    qreal height = size.height();
    if (edge & LeftEdge)
        width -= delta.x();
    else if (edge & RightEdge)
        width += delta.x();

    if (edge & TopEdge)
        height -= delta.y();
    else if (edge & BottomEdge)
        height += delta.y();

    return QSizeF(width, height).toSize();
}

/*!
 * \qmlmethod int QtWaylandCompositor::XdgSurface::sendConfigure(size size, list<uint> states)
 *
 * Sends a configure event to the client. \a size contains the pixel size of the surface.
 * Known \a states are enumerated in XdgSurface::State.
 */

/*!
 * Sends a configure event to the client. Parameter \a size contains the pixel size
 * of the surface. Known \a states are enumerated in QWaylandXdgSurfaceV5::State.
 */
uint QWaylandXdgSurfaceV5::sendConfigure(const QSize &size, const QVector<uint> &states)
{
    Q_D(QWaylandXdgSurfaceV5);
    auto statesBytes = QByteArray::fromRawData((char *)states.data(), states.size() * sizeof(State));
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(extensionContainer());
    Q_ASSERT(surface);
    QWaylandCompositor *compositor = surface->compositor();
    Q_ASSERT(compositor);
    uint32_t serial = compositor->nextSerial();
    d->m_pendingConfigures.append(QWaylandXdgSurfaceV5Private::ConfigureEvent{states, size, serial});
    d->send_configure(size.width(), size.height(), statesBytes, serial);
    return serial;
}

uint QWaylandXdgSurfaceV5::sendConfigure(const QSize &size, const QVector<QWaylandXdgSurfaceV5::State> &states)
{
    QVector<uint> asUints;
    Q_FOREACH (QWaylandXdgSurfaceV5::State state, states) {
        asUints << state;
    }
    return sendConfigure(size, asUints);
}

/*!
 * \qmlmethod void QtWaylandCompositor::XdgSurface::sendClose()
 *
 * Sends a close event to the client.
 */

/*!
 * Sends a close event to the client.
 */
void QWaylandXdgSurfaceV5::sendClose()
{
    Q_D(QWaylandXdgSurfaceV5);
    d->send_close();
}

uint QWaylandXdgSurfaceV5::sendMaximized(const QSize &size)
{
    Q_D(QWaylandXdgSurfaceV5);
    QWaylandXdgSurfaceV5Private::ConfigureEvent conf = d->lastSentConfigure();

    if (!conf.states.contains(QWaylandXdgSurfaceV5::State::MaximizedState))
        conf.states.append(QWaylandXdgSurfaceV5::State::MaximizedState);
    conf.states.removeOne(QWaylandXdgSurfaceV5::State::FullscreenState);
    conf.states.removeOne(QWaylandXdgSurfaceV5::State::ResizingState);

    return sendConfigure(size, conf.states);
}

uint QWaylandXdgSurfaceV5::sendUnmaximized(const QSize &size)
{
    Q_D(QWaylandXdgSurfaceV5);
    QWaylandXdgSurfaceV5Private::ConfigureEvent conf = d->lastSentConfigure();

    conf.states.removeOne(QWaylandXdgSurfaceV5::State::MaximizedState);
    conf.states.removeOne(QWaylandXdgSurfaceV5::State::FullscreenState);
    conf.states.removeOne(QWaylandXdgSurfaceV5::State::ResizingState);

    return sendConfigure(size, conf.states);
}

uint QWaylandXdgSurfaceV5::sendFullscreen(const QSize &size)
{
    Q_D(QWaylandXdgSurfaceV5);
    QWaylandXdgSurfaceV5Private::ConfigureEvent conf = d->lastSentConfigure();

    if (!conf.states.contains(QWaylandXdgSurfaceV5::State::FullscreenState))
        conf.states.append(QWaylandXdgSurfaceV5::State::FullscreenState);
    conf.states.removeOne(QWaylandXdgSurfaceV5::State::MaximizedState);
    conf.states.removeOne(QWaylandXdgSurfaceV5::State::ResizingState);

    return sendConfigure(size, conf.states);
}

uint QWaylandXdgSurfaceV5::sendResizing(const QSize &maxSize)
{
    Q_D(QWaylandXdgSurfaceV5);
    QWaylandXdgSurfaceV5Private::ConfigureEvent conf = d->lastSentConfigure();

    if (!conf.states.contains(QWaylandXdgSurfaceV5::State::ResizingState))
        conf.states.append(QWaylandXdgSurfaceV5::State::ResizingState);
    conf.states.removeOne(QWaylandXdgSurfaceV5::State::MaximizedState);
    conf.states.removeOne(QWaylandXdgSurfaceV5::State::FullscreenState);

    return sendConfigure(maxSize, conf.states);
}

#ifdef QT_WAYLAND_COMPOSITOR_QUICK
QWaylandQuickShellIntegration *QWaylandXdgSurfaceV5::createIntegration(QWaylandQuickShellSurfaceItem *item)
{
    return new QtWayland::XdgShellV5Integration(item);
}
#endif

/*!
 * \class QWaylandXdgPopupV5
 * \inmodule QtWaylandCompositor
 * \since 5.8
 * \brief The QWaylandXdgPopupV5 class provides menus for an xdg surface
 *
 * This class is part of the QWaylandXdgShellV5 extension and provides a way to
 * extend the functionality of an existing QWaylandSurface with features
 * specific to desktop-style menus for an xdg surface.
 *
 * It corresponds to the Wayland interface xdg_popup.
 */

/*!
 * Constructs a QWaylandXdgPopupV5.
 */
QWaylandXdgPopupV5::QWaylandXdgPopupV5()
    : QWaylandShellSurfaceTemplate<QWaylandXdgPopupV5>(*new QWaylandXdgPopupV5Private)
{
}

/*!
 * Constructs a QWaylandXdgPopupV5, associating it with \a xdgShell at the specified \a position
 * for \a surface and initializes it with the given \a parentSurface and \a resource.
 */
QWaylandXdgPopupV5::QWaylandXdgPopupV5(QWaylandXdgShellV5 *xdgShell, QWaylandSurface *surface,
                                   QWaylandSurface *parentSurface, const QPoint &position, const QWaylandResource &resource)
    : QWaylandShellSurfaceTemplate<QWaylandXdgPopupV5>(*new QWaylandXdgPopupV5Private)
{
    initialize(xdgShell, surface, parentSurface, position, resource);
}

/*!
 * \qmlmethod void QtWaylandCompositor::XdgPopup::initialize(object surface, object parentSurface, object resource)
 *
 * Initializes the xdg popup, associating it with the given \a shell, \a surface,
 * \a parentSurface and \a resource.
 */

/*!
 * Initializes the QWaylandXdgPopupV5, associating it with the given \a shell \a surface,
 * \a parentSurface and \a resource.
 */
void QWaylandXdgPopupV5::initialize(QWaylandXdgShellV5 *shell, QWaylandSurface *surface, QWaylandSurface *parentSurface,
                                  const QPoint& position, const QWaylandResource &resource)
{
    Q_D(QWaylandXdgPopupV5);
    d->m_surface = surface;
    d->m_parentSurface = parentSurface;
    d->m_xdgShell = shell;
    d->m_position = position;
    d->init(resource.resource());
    setExtensionContainer(surface);
    emit shellChanged();
    emit surfaceChanged();
    emit parentSurfaceChanged();
    emit positionChanged();
    QWaylandCompositorExtension::initialize();
}

/*!
 * \qmlproperty object QtWaylandCompositor::XdgPopup::shell
 *
 * This property holds the shell associated with this XdgPopup.
 */

/*!
 * \property QWaylandXdgPopupV5::shell
 *
 * This property holds the shell associated with this QWaylandXdgPopupV5.
 */
QWaylandXdgShellV5 *QWaylandXdgPopupV5::shell() const
{
    Q_D(const QWaylandXdgPopupV5);
    return d->m_xdgShell;
}

/*!
 * \qmlproperty object QtWaylandCompositor::XdgPopup::surface
 *
 * This property holds the surface associated with this XdgPopup.
 */

/*!
 * \property QWaylandXdgPopupV5::surface
 *
 * This property holds the surface associated with this QWaylandXdgPopupV5.
 */
QWaylandSurface *QWaylandXdgPopupV5::surface() const
{
    Q_D(const QWaylandXdgPopupV5);
    return d->m_surface;
}

/*!
 * \qmlproperty object QtWaylandCompositor::XdgPopup::parentSurface
 *
 * This property holds the surface associated with the parent of this XdgPopup.
 */

/*!
 * \property QWaylandXdgPopupV5::parentSurface
 *
 * This property holds the surface associated with the parent of this
 * QWaylandXdgPopupV5.
 */
QWaylandSurface *QWaylandXdgPopupV5::parentSurface() const
{
    Q_D(const QWaylandXdgPopupV5);
    return d->m_parentSurface;
}


/*!
 * \qmlproperty object QtWaylandCompositor::XdgPopup::position
 *
 * This property holds the location of the upper left corner of the surface
 * relative to the upper left corner of the parent surface, in surface local
 * coordinates.
 */

/*!
 * \property QWaylandXdgPopupV5::position
 *
 * This property holds the location of the upper left corner of the surface
 * relative to the upper left corner of the parent surface, in surface local
 * coordinates.
 */
QPoint QWaylandXdgPopupV5::position() const
{
    Q_D(const QWaylandXdgPopupV5);
    return d->m_position;
}

/*!
 * \internal
 */
void QWaylandXdgPopupV5::initialize()
{
    QWaylandCompositorExtension::initialize();
}

/*!
 * Returns the Wayland interface for the QWaylandXdgPopupV5.
 */
const wl_interface *QWaylandXdgPopupV5::interface()
{
    return QWaylandXdgPopupV5Private::interface();
}

QByteArray QWaylandXdgPopupV5::interfaceName()
{
    return QWaylandXdgPopupV5Private::interfaceName();
}

/*!
 * Returns the surface role for the QWaylandXdgPopupV5.
 */
QWaylandSurfaceRole *QWaylandXdgPopupV5::role()
{
    return &QWaylandXdgPopupV5Private::s_role;
}

QWaylandXdgPopupV5 *QWaylandXdgPopupV5::fromResource(wl_resource *resource)
{
    auto popupResource = QWaylandXdgPopupV5Private::Resource::fromResource(resource);
    if (!popupResource)
        return nullptr;
    return static_cast<QWaylandXdgPopupV5Private *>(popupResource->xdg_popup_object)->q_func();
}

void QWaylandXdgPopupV5::sendPopupDone()
{
    Q_D(QWaylandXdgPopupV5);
    d->send_popup_done();
}

#ifdef QT_WAYLAND_COMPOSITOR_QUICK
QWaylandQuickShellIntegration *QWaylandXdgPopupV5::createIntegration(QWaylandQuickShellSurfaceItem *item)
{
    return new QtWayland::XdgPopupV5Integration(item);
}
#endif

QT_END_NAMESPACE
