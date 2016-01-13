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

#include "qwlsurfacebuffer_p.h"

#include "qwlsurface_p.h"
#include "qwlcompositor_p.h"

#ifdef QT_COMPOSITOR_WAYLAND_GL
#include "hardware_integration/qwlclientbufferintegration_p.h"
#include <qpa/qplatformopenglcontext.h>
#endif

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

namespace QtWayland {

SurfaceBuffer::SurfaceBuffer(Surface *surface)
    : m_surface(surface)
    , m_compositor(surface->compositor())
    , m_buffer(0)
    , m_committed(false)
    , m_is_registered_for_buffer(false)
    , m_surface_has_buffer(false)
    , m_destroyed(false)
    , m_is_displayed(false)
    , m_texture(0)
    , m_is_shm_resolved(false)
    , m_shmBuffer(0)
    , m_isSizeResolved(false)
    , m_size()
    , m_used(false)
    , m_destroyIfUnused(false)
    , m_image(0)
{
}

SurfaceBuffer::~SurfaceBuffer()
{
    if (m_is_registered_for_buffer)
        destructBufferState();
}

void SurfaceBuffer::initialize(struct ::wl_resource *buffer)
{
    m_buffer = buffer;
    m_texture = 0;
    m_committed = false;
    m_is_registered_for_buffer = true;
    m_surface_has_buffer = true;
    m_is_displayed = false;
    m_destroyed = false;
    m_handle = 0;
    m_is_shm_resolved = false;
    m_shmBuffer = 0;
    m_isSizeResolved = false;
    m_size = QSize();
    m_destroy_listener.surfaceBuffer = this;
    m_destroy_listener.listener.notify = destroy_listener_callback;
    if (buffer)
        wl_signal_add(&buffer->destroy_signal, &m_destroy_listener.listener);
}

void SurfaceBuffer::destructBufferState()
{
    destroyTexture();
    if (m_buffer) {
        sendRelease();

        if (m_handle) {
            if (m_shmBuffer) {
                delete static_cast<QImage *>(m_handle);
#ifdef QT_COMPOSITOR_WAYLAND_GL
            } else {
                ClientBufferIntegration *hwIntegration = m_compositor->clientBufferIntegration();
                hwIntegration->unlockNativeBuffer(m_handle);
#endif
            }
        }
        wl_list_remove(&m_destroy_listener.listener.link);
    }
    m_buffer = 0;
    m_handle = 0;
    m_committed = false;
    m_is_registered_for_buffer = false;
    m_is_displayed = false;
    m_image = QImage();
}

QSize SurfaceBuffer::size() const
{
    if (!m_isSizeResolved) {
        if (isShmBuffer()) {
            m_size = QSize(wl_shm_buffer_get_width(m_shmBuffer), wl_shm_buffer_get_height(m_shmBuffer));
#ifdef QT_COMPOSITOR_WAYLAND_GL
        } else {
            ClientBufferIntegration *hwIntegration = m_compositor->clientBufferIntegration();
            m_size = hwIntegration->bufferSize(m_buffer);
#endif
        }
    }

    return m_size;
}

bool SurfaceBuffer::isShmBuffer() const
{
    if (!m_is_shm_resolved) {
#if (WAYLAND_VERSION_MAJOR >= 1) && (WAYLAND_VERSION_MINOR >= 2)
        m_shmBuffer = wl_shm_buffer_get(m_buffer);
#else
        if (wl_buffer_is_shm(static_cast<struct ::wl_buffer*>(m_buffer->data)))
            m_shmBuffer = static_cast<struct ::wl_buffer*>(m_buffer->data);
#endif
        m_is_shm_resolved = true;
    }
    return m_shmBuffer != 0;
}

void SurfaceBuffer::sendRelease()
{
    Q_ASSERT(m_buffer);
    wl_buffer_send_release(m_buffer);
}

void SurfaceBuffer::disown()
{
    m_surface_has_buffer = false;
    destructBufferState();
    destroyIfUnused();
}

void SurfaceBuffer::setDisplayed()
{
    m_is_displayed = true;
}

void SurfaceBuffer::destroyTexture()
{
#ifdef QT_COMPOSITOR_WAYLAND_GL
    if (m_texture) {
        Q_ASSERT(QOpenGLContext::currentContext());
        ClientBufferIntegration *hwIntegration = m_compositor->clientBufferIntegration();
        if (hwIntegration->textureForBuffer(m_buffer) == 0)
            glDeleteTextures(1, &m_texture);
        else
            hwIntegration->destroyTextureForBuffer(m_buffer);
        m_texture = 0;
    }
#endif
}

void SurfaceBuffer::handleAboutToBeDisplayed()
{
    qDebug() << Q_FUNC_INFO;
}

void SurfaceBuffer::handleDisplayed()
{
    qDebug() << Q_FUNC_INFO;
}

void *SurfaceBuffer::handle() const
{
    if (!m_buffer)
        return 0;

    if (!m_handle) {
        SurfaceBuffer *that = const_cast<SurfaceBuffer *>(this);
        if (isShmBuffer()) {
            const uchar *data = static_cast<const uchar *>(wl_shm_buffer_get_data(m_shmBuffer));
            int stride = wl_shm_buffer_get_stride(m_shmBuffer);
            int width = wl_shm_buffer_get_width(m_shmBuffer);
            int height = wl_shm_buffer_get_height(m_shmBuffer);
            QImage *image = new QImage(data,width,height,stride, QImage::Format_ARGB32_Premultiplied);
            that->m_handle = image;
#ifdef QT_COMPOSITOR_WAYLAND_GL
        } else {
            ClientBufferIntegration *clientBufferIntegration = m_compositor->clientBufferIntegration();
            that->m_handle = clientBufferIntegration->lockNativeBuffer(m_buffer);
#endif
        }
    }
    return m_handle;
}

QImage SurfaceBuffer::image()
{
    /* This api may be available on non-shm buffer. But be sure about it's format. */
    if (!m_buffer || !isShmBuffer())
        return QImage();

    if (m_image.isNull())
    {
        const uchar *data = static_cast<const uchar *>(wl_shm_buffer_get_data(m_shmBuffer));
        int stride = wl_shm_buffer_get_stride(m_shmBuffer);
        int width = wl_shm_buffer_get_width(m_shmBuffer);
        int height = wl_shm_buffer_get_height(m_shmBuffer);
        m_image = QImage(data, width, height, stride, QImage::Format_ARGB32_Premultiplied);
    }

    return m_image;
}

void SurfaceBuffer::destroy_listener_callback(wl_listener *listener, void *data)
{
    Q_UNUSED(data);
    struct surface_buffer_destroy_listener *destroy_listener =
            reinterpret_cast<struct surface_buffer_destroy_listener *>(listener);
    SurfaceBuffer *d = destroy_listener->surfaceBuffer;

    // Mark the buffer as destroyed and clear m_buffer right away to avoid
    // touching it before it is properly cleaned up.
    d->m_destroyed = true;
    d->m_buffer = 0;
}

void SurfaceBuffer::createTexture()
{
    destroyTexture();

    ClientBufferIntegration *hwIntegration = m_compositor->clientBufferIntegration();
#ifdef QT_COMPOSITOR_WAYLAND_GL
    if (!m_texture)
        m_texture = hwIntegration->textureForBuffer(m_buffer);
    if (!m_texture)
        glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    hwIntegration->bindTextureToBuffer(m_buffer);
#else
    Q_UNUSED(hwIntegration);
#endif
}

bool SurfaceBuffer::isYInverted() const
{
    bool ret = false;
    static bool negateReturn = qgetenv("QT_COMPOSITOR_NEGATE_INVERTED_Y").toInt();
    ClientBufferIntegration *clientBufferIntegration = m_compositor->clientBufferIntegration();

#ifdef QT_COMPOSITOR_WAYLAND_GL
    if (clientBufferIntegration && waylandBufferHandle() && !isShmBuffer()) {
        ret = clientBufferIntegration->isYInverted(waylandBufferHandle());
    } else
#endif
        ret = true;

    return ret != negateReturn;
}

void SurfaceBuffer::ref()
{
    m_used = m_refCount.ref();
}

void SurfaceBuffer::deref()
{
    m_used = m_refCount.deref();
    if (!m_used)
        disown();
}

void SurfaceBuffer::setDestroyIfUnused(bool destroy)
{
    m_destroyIfUnused = destroy;
    destroyIfUnused();
}

void SurfaceBuffer::destroyIfUnused()
{
    if (!m_used && m_destroyIfUnused)
        delete this;
}

}

QT_END_NAMESPACE
