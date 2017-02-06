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

#include "qwaylandivisurface.h"
#include "qwaylandivisurface_p.h"
#include "qwaylandiviapplication_p.h"
#include "qwaylandivisurfaceintegration_p.h"

#include <QtWaylandCompositor/QWaylandResource>

QT_BEGIN_NAMESPACE

QWaylandSurfaceRole QWaylandIviSurfacePrivate::s_role("ivi_surface");

/*!
 * \class QWaylandIviSurface
 * \inmodule QtWaylandCompositor
 * \since 5.8
 * \brief The QWaylandIviSurface class provides a simple way to identify and resize a surface.
 *
 * This class is part of the QWaylandIviApplication extension and provides a way to
 * extend the functionality of an existing QWaylandSurface with features a way to
 * resize and identify it.
 *
 * It corresponds to the Wayland interface ivi_surface.
 */

/*!
 * Constructs a QWaylandIviSurface.
 */
QWaylandIviSurface::QWaylandIviSurface()
    : QWaylandShellSurfaceTemplate<QWaylandIviSurface>(*new QWaylandIviSurfacePrivate())
{
}

/*!
 * Constructs a QWaylandIviSurface for \a surface and initializes it with the
 * given \a application, \a surface, \a iviId, and \a resource.
 */
QWaylandIviSurface::QWaylandIviSurface(QWaylandIviApplication *application, QWaylandSurface *surface, uint iviId, const QWaylandResource &resource)
    : QWaylandShellSurfaceTemplate<QWaylandIviSurface>(*new QWaylandIviSurfacePrivate())
{
    initialize(application, surface, iviId, resource);
}

/*!
 * \qmlmethod void QtWaylandCompositor::IviSurface::initialize(object iviApplication, object surface, int iviId, object resource)
 *
 * Initializes the IviSurface, associating it with the given \a iviApplication, \a surface,
 * \a client, \a iviId, and \a resource.
 */

/*!
 * Initializes the QWaylandIviSurface, associating it with the given \a iviApplication, \a surface,
 * \a iviId, and \a resource.
 */
void QWaylandIviSurface::initialize(QWaylandIviApplication *iviApplication, QWaylandSurface *surface, uint iviId, const QWaylandResource &resource)
{
    Q_D(QWaylandIviSurface);

    d->m_iviApplication = iviApplication;
    d->m_surface = surface;
    d->m_iviId = iviId;

    d->init(resource.resource());
    setExtensionContainer(surface);

    emit surfaceChanged();
    emit iviIdChanged();

    QWaylandCompositorExtension::initialize();
}

/*!
 * \qmlproperty object QtWaylandCompositor::IviSurface::surface
 *
 * This property holds the surface associated with this IviSurface.
 */

/*!
 * \property QWaylandIviSurface::surface
 *
 * This property holds the surface associated with this QWaylandIviSurface.
 */
QWaylandSurface *QWaylandIviSurface::surface() const
{
    Q_D(const QWaylandIviSurface);
    return d->m_surface;
}

/*!
 * \qmlproperty int QtWaylandCompositor::IviSurface::iviId
 * \readonly
 *
 * This property holds the ivi id id of this IviSurface.
 */

/*!
 * \property QWaylandIviSurface::iviId
 *
 * This property holds the ivi id of this QWaylandIviSurface.
 */
uint QWaylandIviSurface::iviId() const
{
    Q_D(const QWaylandIviSurface);
    return d->m_iviId;
}

/*!
 * Returns the Wayland interface for the QWaylandIviSurface.
 */
const struct wl_interface *QWaylandIviSurface::interface()
{
    return QWaylandIviSurfacePrivate::interface();
}

QByteArray QWaylandIviSurface::interfaceName()
{
    return QWaylandIviSurfacePrivate::interfaceName();
}

/*!
 * Returns the surface role for the QWaylandIviSurface.
 */
QWaylandSurfaceRole *QWaylandIviSurface::role()
{
    return &QWaylandIviSurfacePrivate::s_role;
}

/*!
 * Returns the QWaylandIviSurface corresponding to the \a resource.
 */
QWaylandIviSurface *QWaylandIviSurface::fromResource(wl_resource *resource)
{
    auto iviSurfaceResource = QWaylandIviSurfacePrivate::Resource::fromResource(resource);
    if (!iviSurfaceResource)
        return nullptr;
    return static_cast<QWaylandIviSurfacePrivate *>(iviSurfaceResource->ivi_surface_object)->q_func();
}

/*!
 * \qmlmethod int QtWaylandCompositor::IviSurface::sendConfigure(size size)
 *
 * Sends a configure event to the client, telling it to resize the surface to the given \a size.
 */

/*!
 * Sends a configure event to the client, telling it to resize the surface to the given \a size.
 */
void QWaylandIviSurface::sendConfigure(const QSize &size)
{
    Q_D(QWaylandIviSurface);
    d->send_configure(size.width(), size.height());
}

#ifdef QT_WAYLAND_COMPOSITOR_QUICK
QWaylandQuickShellIntegration *QWaylandIviSurface::createIntegration(QWaylandQuickShellSurfaceItem *item)
{
    return new QtWayland::IviSurfaceIntegration(item);
}
#endif

/*!
 * \internal
 */
void QWaylandIviSurface::initialize()
{
    QWaylandShellSurfaceTemplate::initialize();
}

QWaylandIviSurfacePrivate::QWaylandIviSurfacePrivate()
    : QWaylandCompositorExtensionPrivate()
    , ivi_surface()
    , m_iviApplication(nullptr)
    , m_surface(nullptr)
    , m_iviId(UINT_MAX)
{
}

void QWaylandIviSurfacePrivate::ivi_surface_destroy_resource(QtWaylandServer::ivi_surface::Resource *resource)
{
    Q_UNUSED(resource);
    Q_Q(QWaylandIviSurface);
    QWaylandIviApplicationPrivate::get(m_iviApplication)->unregisterIviSurface(q);
    delete q;
}

void QWaylandIviSurfacePrivate::ivi_surface_destroy(QtWaylandServer::ivi_surface::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

QT_END_NAMESPACE
