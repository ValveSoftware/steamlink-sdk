/****************************************************************************
**
** Copyright (C) 2014 Jolla Ltd, author: <giulio.camuffo@jollamobile.com>
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
    Q_ASSERT(QGuiApplication::platformNativeInterface());

    m_egl_display = static_cast<EGLDisplay>(QGuiApplication::platformNativeInterface()->nativeResourceForIntegration("egldisplay"));
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
