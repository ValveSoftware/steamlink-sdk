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

#ifndef WL_SURFACE_H
#define WL_SURFACE_H

#include <QtCompositor/qwaylandexport.h>

#include <private/qwlsurfacebuffer_p.h>
#include <QtCompositor/qwaylandsurface.h>
#include <QtCompositor/qwaylandbufferref.h>

#include <QtCore/QVector>
#include <QtCore/QRect>
#include <QtGui/QRegion>
#include <QtGui/QImage>
#include <QtGui/QWindow>

#include <QtCore/QTextStream>
#include <QtCore/QMetaType>

#include <wayland-util.h>

#include <QtCompositor/private/qwayland-server-wayland.h>

QT_BEGIN_NAMESPACE

class QTouchEvent;

namespace QtWayland {

class Compositor;
class Buffer;
class ExtendedSurface;
class InputPanelSurface;
class SubSurface;
class FrameCallback;

class Q_COMPOSITOR_EXPORT Surface : public QtWaylandServer::wl_surface
{
public:
    Surface(struct wl_client *client, uint32_t id, int version, QWaylandCompositor *compositor, QWaylandSurface *surface);
    ~Surface();

    static Surface *fromResource(struct ::wl_resource *resource);

    QWaylandSurface::Type type() const;
    bool isYInverted() const;

    bool mapped() const;

    using QtWaylandServer::wl_surface::resource;

    QSize size() const;
    void setSize(const QSize &size);

    QRegion inputRegion() const;
    QRegion opaqueRegion() const;

    void sendFrameCallback();
    void removeFrameCallback(FrameCallback *callback);

    QWaylandSurface *waylandSurface() const;

    QPoint lastMousePos() const;

    void setExtendedSurface(ExtendedSurface *extendedSurface);
    ExtendedSurface *extendedSurface() const;

    void setSubSurface(SubSurface *subSurface);
    SubSurface *subSurface() const;

    void setInputPanelSurface(InputPanelSurface *inputPanelSurface);
    InputPanelSurface *inputPanelSurface() const;

    Compositor *compositor() const;

    QString className() const { return m_className; }
    void setClassName(const QString &className);

    QString title() const { return m_title; }
    void setTitle(const QString &title);

    Surface *transientParent() const { return m_transientParent; }
    void setTransientParent(Surface *parent) { m_transientParent = parent; }

    bool transientInactive() const { return m_transientInactive; }
    void setTransientInactive(bool v) { m_transientInactive = v; }

    void setTransientOffset(qreal x, qreal y);

    bool isCursorSurface() const { return m_isCursorSurface; }
    void setCursorSurface(bool isCursor) { m_isCursorSurface = isCursor; }

    void releaseSurfaces();
    void frameStarted();

    void setMapped(bool mapped);

    inline bool isDestroyed() const { return m_destroyed; }

    Qt::ScreenOrientation contentOrientation() const;

protected:
    void surface_destroy_resource(Resource *resource) Q_DECL_OVERRIDE;

    void surface_destroy(Resource *resource) Q_DECL_OVERRIDE;
    void surface_attach(Resource *resource,
                        struct wl_resource *buffer, int x, int y) Q_DECL_OVERRIDE;
    void surface_damage(Resource *resource,
                        int32_t x, int32_t y, int32_t width, int32_t height) Q_DECL_OVERRIDE;
    void surface_frame(Resource *resource,
                       uint32_t callback) Q_DECL_OVERRIDE;
    void surface_set_opaque_region(Resource *resource,
                                   struct wl_resource *region) Q_DECL_OVERRIDE;
    void surface_set_input_region(Resource *resource,
                                  struct wl_resource *region) Q_DECL_OVERRIDE;
    void surface_commit(Resource *resource) Q_DECL_OVERRIDE;
    void surface_set_buffer_transform(Resource *resource, int32_t transform) Q_DECL_OVERRIDE;

    Q_DISABLE_COPY(Surface)

    Compositor *m_compositor;
    QWaylandSurface *m_waylandSurface;

    QRegion m_damage;
    SurfaceBuffer *m_buffer;
    QWaylandBufferRef m_bufferRef;
    bool m_surfaceMapped;
    QWaylandBufferAttacher *m_attacher;

    struct {
        SurfaceBuffer *buffer;
        QRegion damage;
        QPoint offset;
        bool newlyAttached;
    } m_pending;

    QPoint m_lastLocalMousePos;
    QPoint m_lastGlobalMousePos;

    QList<FrameCallback *> m_pendingFrameCallbacks;
    QList<FrameCallback *> m_frameCallbacks;

    ExtendedSurface *m_extendedSurface;
    SubSurface *m_subSurface;
    InputPanelSurface *m_inputPanelSurface;

    QRegion m_inputRegion;
    QRegion m_opaqueRegion;

    QVector<SurfaceBuffer *> m_bufferPool;

    QSize m_size;
    QString m_className;
    QString m_title;
    Surface *m_transientParent;
    bool m_transientInactive;
    QPointF m_transientOffset;
    bool m_isCursorSurface;
    bool m_destroyed;
    Qt::ScreenOrientation m_contentOrientation;
    QWindow::Visibility m_visibility;

    void setBackBuffer(SurfaceBuffer *buffer);
    SurfaceBuffer *createSurfaceBuffer(struct ::wl_resource *buffer);

    friend class QWaylandSurface;
};

}

QT_END_NAMESPACE

#endif //WL_SURFACE_H
