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

#ifndef SURFACEBUFFER_H
#define SURFACEBUFFER_H

#include <QtCore/QRect>
#include <QtGui/qopengl.h>
#include <QImage>
#include <QAtomicInt>

#include <wayland-server.h>

QT_BEGIN_NAMESPACE

class QWaylandClientBufferIntegration;
class QWaylandBufferRef;

namespace QtWayland {

class Surface;
class Compositor;

struct surface_buffer_destroy_listener
{
    struct wl_listener listener;
    class SurfaceBuffer *surfaceBuffer;
};

class SurfaceBuffer
{
public:
    SurfaceBuffer(Surface *surface);

    ~SurfaceBuffer();

    void initialize(struct ::wl_resource *bufferResource);
    void destructBufferState();

    QSize size() const;

    bool isShmBuffer() const;
    bool isYInverted() const;

    inline bool isRegisteredWithBuffer() const { return m_is_registered_for_buffer; }

    void sendRelease();
    void disown();

    void setDisplayed();

    inline bool isComitted() const { return m_committed; }
    inline void setCommitted() { m_committed = true; }
    inline bool isDisplayed() const { return m_is_displayed; }

    inline bool textureCreated() const { return m_texture; }

    bool isDestroyed() { return m_destroyed; }

    void createTexture();
    inline GLuint texture() const;
    void destroyTexture();

    inline struct ::wl_resource *waylandBufferHandle() const { return m_buffer; }

    void handleAboutToBeDisplayed();
    void handleDisplayed();

    void bufferWasDestroyed();
    void setDestroyIfUnused(bool destroy);

    void *handle() const;
    QImage image();
private:
    void ref();
    void deref();
    void destroyIfUnused();

    Surface *m_surface;
    Compositor *m_compositor;
    struct ::wl_resource *m_buffer;
    struct surface_buffer_destroy_listener m_destroy_listener;
    bool m_committed;
    bool m_is_registered_for_buffer;
    bool m_surface_has_buffer;
    bool m_destroyed;

    bool m_is_displayed;
#ifdef QT_COMPOSITOR_WAYLAND_GL
    GLuint m_texture;
#else
    uint m_texture;
#endif
    void *m_handle;
    mutable bool m_is_shm_resolved;

#if (WAYLAND_VERSION_MAJOR >= 1) && (WAYLAND_VERSION_MINOR >= 2)
    mutable struct ::wl_shm_buffer *m_shmBuffer;
#else
    mutable struct ::wl_buffer *m_shmBuffer;
#endif

    mutable bool m_isSizeResolved;
    mutable QSize m_size;
    QAtomicInt m_refCount;
    bool m_used;
    bool m_destroyIfUnused;

    QImage m_image;

    static void destroy_listener_callback(wl_listener *listener, void *data);

    friend class ::QWaylandBufferRef;
};

GLuint SurfaceBuffer::texture() const
{
    if (m_buffer)
        return m_texture;
    return 0;
}

}

QT_END_NAMESPACE

#endif // SURFACEBUFFER_H
