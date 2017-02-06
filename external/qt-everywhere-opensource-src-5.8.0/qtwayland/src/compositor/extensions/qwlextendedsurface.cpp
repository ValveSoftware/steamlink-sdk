/****************************************************************************
**
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

#include "qwlextendedsurface_p.h"

#include <QtWaylandCompositor/QWaylandCompositor>

QT_BEGIN_NAMESPACE

namespace QtWayland {

SurfaceExtensionGlobal::SurfaceExtensionGlobal(QWaylandCompositor *compositor)
    : QWaylandCompositorExtensionTemplate(compositor)
    , QtWaylandServer::qt_surface_extension(compositor->display(), 1)
{
}

void SurfaceExtensionGlobal::surface_extension_get_extended_surface(Resource *resource,
                                                                    uint32_t id,
                                                                    struct wl_resource *surface_resource)
{
    QWaylandSurface *surface = QWaylandSurface::fromResource(surface_resource);
    ExtendedSurface *extSurface = new ExtendedSurface(resource->client(),id, wl_resource_get_version(resource->handle), surface);
    emit extendedSurfaceReady(extSurface, surface);
}

ExtendedSurface::ExtendedSurface(struct wl_client *client, uint32_t id, int version, QWaylandSurface *surface)
    : QWaylandCompositorExtensionTemplate(surface)
    , QtWaylandServer::qt_extended_surface(client, id, version)
    , m_surface(surface)
    , m_windowFlags(0)
{
}

ExtendedSurface::~ExtendedSurface()
{
}

void ExtendedSurface::sendGenericProperty(const QString &name, const QVariant &variant)
{
    QByteArray byteValue;
    QDataStream ds(&byteValue, QIODevice::WriteOnly);
    ds << variant;
    send_set_generic_property(name, byteValue);

}

void ExtendedSurface::sendOnScreenVisibilityChange(bool onScreen)
{
    setVisibility(onScreen ? QWindow::AutomaticVisibility : QWindow::Hidden);
}

void ExtendedSurface::setVisibility(QWindow::Visibility visibility)
{
    send_onscreen_visibility(visibility);
}

void ExtendedSurface::setParentSurface(QWaylandSurface *surface)
{
    m_surface = surface;
}

void ExtendedSurface::extended_surface_update_generic_property(Resource *resource,
                                                               const QString &name,
                                                               struct wl_array *value)
{
    Q_UNUSED(resource);
    QVariant variantValue;
    QByteArray byteValue((const char*)value->data, value->size);
    QDataStream ds(&byteValue, QIODevice::ReadOnly);
    ds >> variantValue;
    setWindowPropertyImpl(name,variantValue);
}

Qt::ScreenOrientations ExtendedSurface::contentOrientationMask() const
{
    return m_contentOrientationMask;
}

void ExtendedSurface::extended_surface_set_content_orientation_mask(Resource *resource, int32_t orientation)
{
    Q_UNUSED(resource);
    Qt::ScreenOrientations mask = 0;
    if (orientation & QT_EXTENDED_SURFACE_ORIENTATION_PORTRAITORIENTATION)
        mask |= Qt::PortraitOrientation;
    if (orientation & QT_EXTENDED_SURFACE_ORIENTATION_LANDSCAPEORIENTATION)
        mask |= Qt::LandscapeOrientation;
    if (orientation & QT_EXTENDED_SURFACE_ORIENTATION_INVERTEDPORTRAITORIENTATION)
        mask |= Qt::InvertedPortraitOrientation;
    if (orientation & QT_EXTENDED_SURFACE_ORIENTATION_INVERTEDLANDSCAPEORIENTATION)
        mask |= Qt::InvertedLandscapeOrientation;
    if (orientation & QT_EXTENDED_SURFACE_ORIENTATION_PRIMARYORIENTATION)
        mask |= Qt::PrimaryOrientation;

    Qt::ScreenOrientations oldMask = m_contentOrientationMask;
    m_contentOrientationMask = mask;

    if (mask != oldMask)
        emit contentOrientationMaskChanged();
}

QVariantMap ExtendedSurface::windowProperties() const
{
    return m_windowProperties;
}

QVariant ExtendedSurface::windowProperty(const QString &propertyName) const
{
    QVariantMap props = m_windowProperties;
    return props.value(propertyName);
}

void ExtendedSurface::setWindowProperty(const QString &name, const QVariant &value)
{
    setWindowPropertyImpl(name,value);
    sendGenericProperty(name, value);
}

void ExtendedSurface::setWindowPropertyImpl(const QString &name, const QVariant &value)
{
    m_windowProperties.insert(name, value);
    emit windowPropertyChanged(name,value);
}

void ExtendedSurface::extended_surface_set_window_flags(Resource *resource, int32_t flags)
{
    Q_UNUSED(resource);
    WindowFlags windowFlags(flags);
    if (windowFlags == m_windowFlags)
        return;
    m_windowFlags = windowFlags;
    emit windowFlagsChanged();
}

void ExtendedSurface::extended_surface_destroy_resource(Resource *)
{
    delete this;
}

void ExtendedSurface::extended_surface_raise(Resource *)
{
    emit raiseRequested();
}

void ExtendedSurface::extended_surface_lower(Resource *)
{
    emit lowerRequested();
}

}

QT_END_NAMESPACE
