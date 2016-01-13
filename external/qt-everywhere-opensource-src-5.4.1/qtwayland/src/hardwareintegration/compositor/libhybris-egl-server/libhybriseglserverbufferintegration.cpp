/****************************************************************************
**
** Copyright (C) 2014 Jolla Ltd, author: <giulio.camuffo@jollamobile.com>
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

#include "libhybriseglserverbufferintegration.h"

#include <QtGui/QOpenGLContext>
#include <hybris/eglplatformcommon/hybris_nativebufferext.h>
#include <wayland-server.h>

QT_BEGIN_NAMESPACE
LibHybrisEglServerBuffer::LibHybrisEglServerBuffer(LibHybrisEglServerBufferIntegration *integration, const QSize &size, QtWayland::ServerBuffer::Format format)
    : QtWayland::ServerBuffer(size,format)
    , m_integration(integration)
{
    m_format = format;

    EGLint egl_format;
    switch (m_format) {
        case RGBA32:
            m_hybris_format = QtWaylandServer::qt_libhybris_egl_server_buffer::format_RGBA32;
            egl_format = HYBRIS_PIXEL_FORMAT_RGBA_8888;
            break;
        default:
            qWarning("LibHybrisEglServerBuffer: unsupported format");
            m_hybris_format = QtWaylandServer::qt_libhybris_egl_server_buffer::format_RGBA32;
            egl_format = HYBRIS_PIXEL_FORMAT_RGBA_8888;
            break;
    }

    if (!m_integration->eglHybrisCreateNativeBuffer(size.width(), size.height(), HYBRIS_USAGE_HW_TEXTURE, egl_format, &m_stride, &m_buffer)) {
        qWarning("LibHybrisEglServerBuffer: Failed to create egl buffer");
        return;
    }

    int numInts;
    int numFds;
    m_integration->eglHybrisGetNativeBufferInfo(m_buffer, &numInts, &numFds);
    m_ints.resize(numInts);
    m_fds.resize(numFds);
    m_integration->eglHybrisSerializeNativeBuffer(m_buffer, m_ints.data(), m_fds.data());

    m_image = m_integration->eglCreateImageKHR(EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, m_buffer, 0);
}

struct ::wl_resource *LibHybrisEglServerBuffer::resourceForClient(struct ::wl_client *client)
{
    QMultiMap<struct ::wl_client *, Resource *>::iterator it = resourceMap().find(client);
    if (it == resourceMap().end()) {
        QMultiMap<struct ::wl_client *, QtWaylandServer::qt_libhybris_egl_server_buffer::Resource *>::iterator egl_it = m_integration->resourceMap().find(client);
        if (egl_it == m_integration->resourceMap().end()) {
            qWarning("LibHybrisEglServerBuffer::resourceForClient: Trying to get resource for ServerBuffer. But client is not bound to the libhybris_egl interface");
            return 0;
        }
        struct ::wl_resource *egl_resource = (*egl_it)->handle;
        Resource *resource = add(client, 1, 1);
        wl_resource *bufRes = wl_client_new_object(client, &qt_libhybris_buffer_interface, 0, 0);

        m_integration->send_server_buffer_created(egl_resource, resource->handle, bufRes, m_fds.size(), QByteArray((char *)m_ints.data(), m_ints.size() * sizeof(int32_t)),
                                    m_name, m_size.width(), m_size.height(), m_stride, m_format);

        m_qtbuffers.insert(resource, bufRes);

        for (int i = 0; i < m_fds.size(); ++i) {
            send_add_fd(resource->handle, m_fds.at(i));
        }

        return bufRes;
    }
    return m_qtbuffers.value(*it);
}

void LibHybrisEglServerBuffer::bindTextureToBuffer()
{
    if (!QOpenGLContext::currentContext()) {
        qWarning("LibHybrisEglServerBuffer: No current context when creating buffer. Texture loading will fail");
        return;
    }

    m_integration->glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, m_image);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

LibHybrisEglServerBufferIntegration::LibHybrisEglServerBufferIntegration()
{
}

LibHybrisEglServerBufferIntegration::~LibHybrisEglServerBufferIntegration()
{
}

void LibHybrisEglServerBufferIntegration::initializeHardware(QWaylandCompositor *compositor)
{
    QWindow *window = compositor->window();
    Q_ASSERT(QGuiApplication::platformNativeInterface());

    m_egl_display = static_cast<EGLDisplay>(QGuiApplication::platformNativeInterface()->nativeResourceForWindow("egldisplay", window));
    if (!m_egl_display) {
        qWarning("Cant initialize libhybris egl server buffer integration. Missing egl display from platformplugin");
        return;
    }

    m_egl_create_buffer = reinterpret_cast<PFNEGLHYBRISCREATENATIVEBUFFERPROC>(eglGetProcAddress("eglHybrisCreateNativeBuffer"));
    if (!m_egl_create_buffer) {
        qWarning("Failed to initialize libhybris egl server buffer integration. Could not find eglHybrisCreateNativeBuffer.\n");
        return;
    }
    m_egl_get_buffer_info = reinterpret_cast<PFNEGLHYBRISGETNATIVEBUFFERINFOPROC>(eglGetProcAddress("eglHybrisGetNativeBufferInfo"));
    if (!m_egl_get_buffer_info) {
        qWarning("Failed to initialize libhybris egl server buffer integration. Could not find eglHybrisGetNativeBufferInfo.\n");
        return;
    }
    m_egl_serialize_buffer = reinterpret_cast<PFNEGLHYBRISSERIALIZENATIVEBUFFERPROC>(eglGetProcAddress("eglHybrisSerializeNativeBuffer"));
    if (!m_egl_serialize_buffer) {
        qWarning("Failed to initialize libhybris egl server buffer integration. Could not find eglHybrisSerializeNativeBuffer.\n");
        return;
    }

    const char *extensionString = eglQueryString(m_egl_display, EGL_EXTENSIONS);
    if (!extensionString || !strstr(extensionString, "EGL_KHR_image")) {
        qWarning("Failed to initialize libhybris egl server buffer integration. There is no EGL_KHR_image extension.\n");
        return;
    }
    m_egl_create_image = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
    m_egl_destroy_image = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
    if (!m_egl_create_image || !m_egl_destroy_image) {
        qWarning("Failed to initialize libhybris egl server buffer integration. Could not resolve eglCreateImageKHR or eglDestroyImageKHR");
        return;
    }

    m_gl_egl_image_target_texture_2d = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    if (!m_gl_egl_image_target_texture_2d) {
        qWarning("Failed to initialize libhybris egl server buffer integration. Could not find glEGLImageTargetTexture2DOES.\n");
        return;
    }

    QtWaylandServer::qt_libhybris_egl_server_buffer::init(compositor->waylandDisplay(), 1);
}

bool LibHybrisEglServerBufferIntegration::supportsFormat(QtWayland::ServerBuffer::Format format) const
{
    switch (format) {
        case QtWayland::ServerBuffer::RGBA32:
            return true;
        case QtWayland::ServerBuffer::A8:
            return false;
        default:
            return false;
    }
}

QtWayland::ServerBuffer *LibHybrisEglServerBufferIntegration::createServerBuffer(const QSize &size, QtWayland::ServerBuffer::Format format)
{
    return new LibHybrisEglServerBuffer(this, size, format);
}

QT_END_NAMESPACE
