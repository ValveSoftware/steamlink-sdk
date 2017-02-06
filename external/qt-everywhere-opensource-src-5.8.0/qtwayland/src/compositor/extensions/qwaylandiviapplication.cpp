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

#include "qwaylandiviapplication.h"
#include "qwaylandiviapplication_p.h"

#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandSurface>
#include <QtWaylandCompositor/QWaylandIviSurface>
#include <QtWaylandCompositor/QWaylandResource>

QT_BEGIN_NAMESPACE

/*!
 * Constructs a QWaylandIviApplication object.
 */
QWaylandIviApplication::QWaylandIviApplication()
    : QWaylandCompositorExtensionTemplate<QWaylandIviApplication>(*new QWaylandIviApplicationPrivate())
{
}

/*!
 * Constructs a QWaylandIviApplication object for the provided \a compositor.
 */
QWaylandIviApplication::QWaylandIviApplication(QWaylandCompositor *compositor)
    : QWaylandCompositorExtensionTemplate<QWaylandIviApplication>(compositor, *new QWaylandIviApplicationPrivate())
{
}

/*!
 * Initializes the shell extension.
 */
void QWaylandIviApplication::initialize()
{
    Q_D(QWaylandIviApplication);
    QWaylandCompositorExtensionTemplate::initialize();

    QWaylandCompositor *compositor = static_cast<QWaylandCompositor *>(extensionContainer());
    if (!compositor) {
        qWarning() << "Failed to find QWaylandCompositor when initializing QWaylandIviApplication";
        return;
    }

    d->init(compositor->display(), 1);
}

/*!
 * Returns the Wayland interface for the QWaylandIviApplication.
 */
const struct wl_interface *QWaylandIviApplication::interface()
{
    return QWaylandIviApplicationPrivate::interface();
}

QByteArray QWaylandIviApplication::interfaceName()
{
    return QWaylandIviApplicationPrivate::interfaceName();
}

/*!
 * \qmlsignal void QtWaylandCompositor::IviApplication::iviSurfaceRequested(object surface, int iviId, object resource)
 *
 * This signal is emitted when the client has requested an \c ivi_surface to be associated
 * with \a surface, which is identified by \a id. The handler for this signal is
 * expected to create the ivi surface and initialize it within the scope of the
 * signal emission. If no ivi surface is created, a default one will be created instead.
 */

/*!
 * \fn void QWaylandWlShell::iviSurfaceRequested(QWaylandSurface *surface, uint iviId, const QWaylandResource &resource)
 *
 * This signal is emitted when the client has requested an \c ivi_surface to be associated
 * with \a surface, which is identified by \a id. The handler for this signal is
 * expected to create the ivi surface and initialize it within the scope of the
 * signal emission. If no ivi surface is created, a default one will be created instead.
 */

/*!
 * \qmlsignal void QtWaylandCompositor::IviApplication::iviSurfaceCreated(object *iviSurface)
 *
 * This signal is emitted when an IviSurface has been created. The supplied \a iviSurface is
 * most commonly used to instantiate a ShellSurfaceItem.
 */

/*!
 * \fn void QtWaylandCompositor::IviApplication::iviSurfaceCreated(QWaylandIviSurface *iviSurface)
 *
 * This signal is emitted when an IviSurface has been created.
 */

QWaylandIviApplicationPrivate::QWaylandIviApplicationPrivate()
    : QWaylandCompositorExtensionPrivate()
    , ivi_application()
{
}

void QWaylandIviApplicationPrivate::unregisterIviSurface(QWaylandIviSurface *iviSurface)
{
    m_iviSurfaces.remove(iviSurface->iviId());
}

void QWaylandIviApplicationPrivate::ivi_application_surface_create(QtWaylandServer::ivi_application::Resource *resource,
                                                                   uint32_t ivi_id, wl_resource *surfaceResource, uint32_t id)
{
    Q_Q(QWaylandIviApplication);
    QWaylandSurface *surface = QWaylandSurface::fromResource(surfaceResource);

    if (m_iviSurfaces.contains(ivi_id)) {
        wl_resource_post_error(resource->handle, IVI_APPLICATION_ERROR_IVI_ID,
                               "Given ivi_id, %d, is already assigned to wl_surface@%d", ivi_id,
                               wl_resource_get_id(m_iviSurfaces[ivi_id]->surface()->resource()));
        return;
    }

    if (!surface->setRole(QWaylandIviSurface::role(), resource->handle, IVI_APPLICATION_ERROR_ROLE))
        return;

    QWaylandResource iviSurfaceResource(wl_resource_create(resource->client(), &ivi_surface_interface,
                                                           wl_resource_get_version(resource->handle), id));

    emit q->iviSurfaceRequested(surface, ivi_id, iviSurfaceResource);

    QWaylandIviSurface *iviSurface = QWaylandIviSurface::fromResource(iviSurfaceResource.resource());

    if (!iviSurface)
        iviSurface = new QWaylandIviSurface(q, surface, ivi_id, iviSurfaceResource);

    m_iviSurfaces.insert(ivi_id, iviSurface);

    emit q->iviSurfaceCreated(iviSurface);
}

QT_END_NAMESPACE
