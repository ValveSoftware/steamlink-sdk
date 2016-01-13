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

#include "drmeglserverbufferintegration.h"

#include <QtGui/QOpenGLContext>

QT_BEGIN_NAMESPACE

DrmEglServerBuffer::DrmEglServerBuffer(DrmEglServerBufferIntegration *integration, const QSize &size, QtWayland::ServerBuffer::Format format)
    : QtWayland::ServerBuffer(size,format)
    , m_integration(integration)
{
    m_format = format;

    EGLint egl_format;
    switch (m_format) {
        case RGBA32:
            m_drm_format = QtWaylandServer::qt_drm_egl_server_buffer::format_RGBA32;
            egl_format = EGL_DRM_BUFFER_FORMAT_ARGB32_MESA;
            break;
#ifdef EGL_DRM_BUFFER_FORMAT_A8_MESA
        case A8:
            m_drm_format = QtWaylandServer::qt_drm_egl_server_buffer::format_A8;
            egl_format = EGL_DRM_BUFFER_FORMAT_A8_MESA;
            break;
#endif
        default:
            qWarning("DrmEglServerBuffer: unsupported format");
            m_drm_format = QtWaylandServer::qt_drm_egl_server_buffer::format_RGBA32;
            egl_format = EGL_DRM_BUFFER_FORMAT_ARGB32_MESA;
            break;
    }
    EGLint imageAttribs[] = {
        EGL_WIDTH, m_size.width(),
        EGL_HEIGHT, m_size.height(),
        EGL_DRM_BUFFER_FORMAT_MESA, egl_format,
        EGL_DRM_BUFFER_USE_MESA, EGL_DRM_BUFFER_USE_SHARE_MESA,
        EGL_NONE
    };

    m_image = m_integration->eglCreateDRMImageMESA(imageAttribs);

    EGLint handle;
    if (!m_integration->eglExportDRMImageMESA(m_image, &m_name, &handle, &m_stride)) {
        qWarning("DrmEglServerBuffer: Failed to export egl image");
    }

}

struct ::wl_resource *DrmEglServerBuffer::resourceForClient(struct ::wl_client *client)
{
    QMultiMap<struct ::wl_client *, Resource *>::iterator it = resourceMap().find(client);
    if (it == resourceMap().end()) {
        QMultiMap<struct ::wl_client *, QtWaylandServer::qt_drm_egl_server_buffer::Resource *>::iterator drm_egl_it = m_integration->resourceMap().find(client);
        if (drm_egl_it == m_integration->resourceMap().end()) {
            qWarning("DrmEglServerBuffer::resourceForClient: Trying to get resource for ServerBuffer. But client is not bound to the drm_egl interface");
            return 0;
        }
        struct ::wl_resource *drm_egl_resource = (*drm_egl_it)->handle;
        Resource *resource = add(client, 1, 1);
        m_integration->send_server_buffer_created(drm_egl_resource, resource->handle, m_name, m_size.width(), m_size.height(), m_stride, m_drm_format);
        return resource->handle;
    }
    return (*it)->handle;
}

void DrmEglServerBuffer::bindTextureToBuffer()
{
    if (!QOpenGLContext::currentContext()) {
        qWarning("DrmEglServerBuffer: No current context when creating buffer. Texture loading will fail");
        return;
    }

    m_integration->glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, m_image);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

DrmEglServerBufferIntegration::DrmEglServerBufferIntegration()
{
}

DrmEglServerBufferIntegration::~DrmEglServerBufferIntegration()
{
}

void DrmEglServerBufferIntegration::initializeHardware(QWaylandCompositor *compositor)
{
    QWindow *window = compositor->window();
    Q_ASSERT(QGuiApplication::platformNativeInterface());

    m_egl_display = static_cast<EGLDisplay>(QGuiApplication::platformNativeInterface()->nativeResourceForWindow("egldisplay", window));
    if (!m_egl_display) {
        qWarning("Cant initialize drm egl server buffer integration. Missing egl display from platformplugin");
        return;
    }

    const char *extensionString = eglQueryString(m_egl_display, EGL_EXTENSIONS);
    if (!extensionString || !strstr(extensionString, "EGL_KHR_image")) {
        qWarning("Failed to initialize drm egl server buffer integration. There is no EGL_KHR_image extension.\n");
        return;
    }
    m_egl_create_image = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
    m_egl_destroy_image = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
    if (!m_egl_create_image || !m_egl_destroy_image) {
        qWarning("Failed to initialize drm egl server buffer integration. Could not resolve eglCreateImageKHR or eglDestroyImageKHR");
        return;
    }

    if (!extensionString || !strstr(extensionString, "EGL_MESA_drm_image")) {
        qWarning("Failed to initialize drm egl server buffer integration. There is no EGL_MESA_drm_image extension.\n");
        return;
    }

    m_egl_create_drm_image = reinterpret_cast<PFNEGLCREATEDRMIMAGEMESAPROC>(eglGetProcAddress("eglCreateDRMImageMESA"));
    m_egl_export_drm_image = reinterpret_cast<PFNEGLEXPORTDRMIMAGEMESAPROC>(eglGetProcAddress("eglExportDRMImageMESA"));
    if (!m_egl_create_drm_image || !m_egl_export_drm_image) {
        qWarning("Failed to initialize drm egl server buffer integration. Could not find eglCreateDRMImageMESA or eglExportDRMImageMESA.\n");
        return;
    }

    m_gl_egl_image_target_texture_2d = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    if (!m_gl_egl_image_target_texture_2d) {
        qWarning("Failed to initialize drm egl server buffer integration. Could not find glEGLImageTargetTexture2DOES.\n");
        return;
    }

    QtWaylandServer::qt_drm_egl_server_buffer::init(compositor->waylandDisplay(), 1);
}

bool DrmEglServerBufferIntegration::supportsFormat(QtWayland::ServerBuffer::Format format) const
{
    switch (format) {
        case QtWayland::ServerBuffer::RGBA32:
            return true;
        case QtWayland::ServerBuffer::A8:
#ifdef EGL_DRM_BUFFER_FORMAT_A8_MESA
            return true;
#else
            return false;
#endif
        default:
            return false;
    }
}

QtWayland::ServerBuffer *DrmEglServerBufferIntegration::createServerBuffer(const QSize &size, QtWayland::ServerBuffer::Format format)
{
    return new DrmEglServerBuffer(this, size, format);
}

QT_END_NAMESPACE
