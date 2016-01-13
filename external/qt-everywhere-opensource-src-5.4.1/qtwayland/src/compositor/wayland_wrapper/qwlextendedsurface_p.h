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

#ifndef WLEXTENDEDSURFACE_H
#define WLEXTENDEDSURFACE_H

#include <wayland-server.h>

#include <QtCompositor/private/qwayland-server-surface-extension.h>
#include <private/qwlsurface_p.h>
#include <QtCompositor/qwaylandsurface.h>
#include <QtCompositor/qwaylandsurfaceinterface.h>

#include <QtCore/QVariant>
#include <QtCore/QLinkedList>
#include <QtGui/QWindow>

QT_BEGIN_NAMESPACE

class QWaylandSurface;

namespace QtWayland {

class Compositor;

class SurfaceExtensionGlobal : public QtWaylandServer::qt_surface_extension
{
public:
    SurfaceExtensionGlobal(Compositor *compositor);

private:
    void surface_extension_get_extended_surface(Resource *resource,
                                                uint32_t id,
                                                struct wl_resource *surface);

};

class ExtendedSurface : public QWaylandSurfaceInterface, public QtWaylandServer::qt_extended_surface
{
public:
    ExtendedSurface(struct wl_client *client, uint32_t id, int version, Surface *surface);
    ~ExtendedSurface();

    void sendGenericProperty(const QString &name, const QVariant &variant);

    void setVisibility(QWindow::Visibility visibility, bool updateClient = true);

    void setSubSurface(ExtendedSurface *subSurface,int x, int y);
    void removeSubSurface(ExtendedSurface *subSurfaces);
    ExtendedSurface *parent() const;
    void setParent(ExtendedSurface *parent);
    QLinkedList<QWaylandSurface *> subSurfaces() const;
    void setParentSurface(Surface *s);

    Qt::ScreenOrientations contentOrientationMask() const;

    QWaylandSurface::WindowFlags windowFlags() const { return m_windowFlags; }

    QVariantMap windowProperties() const;
    QVariant windowProperty(const QString &propertyName) const;
    void setWindowProperty(const QString &name, const QVariant &value, bool writeUpdateToClient = true);

protected:
    bool runOperation(QWaylandSurfaceOp *op) Q_DECL_OVERRIDE;

private:
    Surface *m_surface;

    Qt::ScreenOrientations m_contentOrientationMask;

    QWaylandSurface::WindowFlags m_windowFlags;

    QByteArray m_authenticationToken;
    QVariantMap m_windowProperties;

    void extended_surface_update_generic_property(Resource *resource,
                                                  const QString &name,
                                                  struct wl_array *value) Q_DECL_OVERRIDE;

    void extended_surface_set_content_orientation_mask(Resource *resource,
                                                       int32_t orientation) Q_DECL_OVERRIDE;

    void extended_surface_set_window_flags(Resource *resource,
                                           int32_t flags) Q_DECL_OVERRIDE;

    void extended_surface_destroy_resource(Resource *) Q_DECL_OVERRIDE;
    void extended_surface_raise(Resource *) Q_DECL_OVERRIDE;
    void extended_surface_lower(Resource *) Q_DECL_OVERRIDE;
};

}

QT_END_NAMESPACE

#endif // WLEXTENDEDSURFACE_H
