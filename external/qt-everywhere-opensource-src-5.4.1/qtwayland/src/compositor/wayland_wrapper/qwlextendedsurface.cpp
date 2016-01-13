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

#include "qwlextendedsurface_p.h"

#include "qwlcompositor_p.h"
#include "qwlsurface_p.h"

QT_BEGIN_NAMESPACE

namespace QtWayland {

SurfaceExtensionGlobal::SurfaceExtensionGlobal(Compositor *compositor)
    : QtWaylandServer::qt_surface_extension(compositor->wl_display(), 1)
{
}

void SurfaceExtensionGlobal::surface_extension_get_extended_surface(Resource *resource,
                                                                    uint32_t id,
                                                                    struct wl_resource *surface_resource)
{
    Surface *surface = Surface::fromResource(surface_resource);
    new ExtendedSurface(resource->client(),id, wl_resource_get_version(resource->handle), surface);
}

ExtendedSurface::ExtendedSurface(struct wl_client *client, uint32_t id, int version, Surface *surface)
    : QWaylandSurfaceInterface(surface->waylandSurface())
    , QtWaylandServer::qt_extended_surface(client, id, version)
    , m_surface(surface)
    , m_windowFlags(0)
{
    Q_ASSERT(surface->extendedSurface() == 0);
    surface->setExtendedSurface(this);
}

ExtendedSurface::~ExtendedSurface()
{
    if (m_surface)
        m_surface->setExtendedSurface(0);
}

void ExtendedSurface::sendGenericProperty(const QString &name, const QVariant &variant)
{
    QByteArray byteValue;
    QDataStream ds(&byteValue, QIODevice::WriteOnly);
    ds << variant;
    send_set_generic_property(name, byteValue);

}

void ExtendedSurface::setVisibility(QWindow::Visibility visibility, bool updateClient)
{
    // If this change came from the client, we shouldn't update it
    if (updateClient)
        send_onscreen_visibility(visibility);
}

void ExtendedSurface::setParentSurface(Surface *surface)
{
    m_surface = surface;
}

bool ExtendedSurface::runOperation(QWaylandSurfaceOp *op)
{
    switch (op->type()) {
        case QWaylandSurfaceOp::Close:
            send_close();
            return true;
        case QWaylandSurfaceOp::SetVisibility:
            setVisibility(static_cast<QWaylandSurfaceSetVisibilityOp *>(op)->visibility());
            return true;
        default:
            break;
    }
    return false;
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
    setWindowProperty(name,variantValue,false);
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

    if (m_surface && mask != oldMask)
        emit m_surface->waylandSurface()->orientationUpdateMaskChanged();
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

void ExtendedSurface::setWindowProperty(const QString &name, const QVariant &value, bool writeUpdateToClient)
{
    m_windowProperties.insert(name, value);
    m_surface->waylandSurface()->windowPropertyChanged(name,value);
    if (writeUpdateToClient)
        sendGenericProperty(name, value);
}

void ExtendedSurface::extended_surface_set_window_flags(Resource *resource, int32_t flags)
{
    Q_UNUSED(resource);
    QWaylandSurface::WindowFlags windowFlags(flags);
    if (!m_surface || windowFlags == m_windowFlags)
        return;
    m_windowFlags = windowFlags;
    emit m_surface->waylandSurface()->windowFlagsChanged(windowFlags);
}

void ExtendedSurface::extended_surface_destroy_resource(Resource *)
{
    delete this;
}

void ExtendedSurface::extended_surface_raise(Resource *)
{
    if (m_surface)
        emit m_surface->waylandSurface()->raiseRequested();
}

void ExtendedSurface::extended_surface_lower(Resource *)
{
    if (m_surface)
        emit m_surface->waylandSurface()->lowerRequested();
}

}

QT_END_NAMESPACE
