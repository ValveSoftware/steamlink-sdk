/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "drmeglserverbufferintegration.h"
#include <QtWaylandClient/private/qwaylanddisplay_p.h>
#include <QDebug>
#include <QtGui/QOpenGLContext>

#include <EGL/egl.h>

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

DrmServerBuffer::DrmServerBuffer(DrmEglServerBufferIntegration *integration
                                , int32_t name
                                , int32_t width
                                , int32_t height
                                , int32_t stride
                                , int32_t format)
    : QWaylandServerBuffer()
    , m_integration(integration)
{
    m_size = QSize(width, height);
    EGLint egl_format;
    int32_t format_stride;
    switch (format) {
        case QtWayland::qt_drm_egl_server_buffer::format_RGBA32:
            m_format = QWaylandServerBuffer::RGBA32;
            egl_format = EGL_DRM_BUFFER_FORMAT_ARGB32_MESA;
            format_stride = stride / 4;
            break;
#ifdef EGL_DRM_BUFFER_FORMAT_A8_MESA
        case QtWayland::qt_drm_egl_server_buffer::format_A8:
            m_format = QWaylandServerBuffer::A8;
            egl_format = EGL_DRM_BUFFER_FORMAT_A8_MESA;
            format_stride = stride;
            break;
#endif
        default:
            qWarning("DrmServerBuffer: unknown format");
            m_format = QWaylandServerBuffer::RGBA32;
            egl_format = EGL_DRM_BUFFER_FORMAT_ARGB32_MESA;
            format_stride = stride / 4;
            break;
    }
    EGLint attribs[] = {
        EGL_WIDTH,                      width,
        EGL_HEIGHT,                     height,
        EGL_DRM_BUFFER_STRIDE_MESA,     format_stride,
        EGL_DRM_BUFFER_FORMAT_MESA,     egl_format,
        EGL_NONE
    };

    qintptr name_pointer = name;
    m_image = integration->eglCreateImageKHR(EGL_NO_CONTEXT, EGL_DRM_BUFFER_MESA, (EGLClientBuffer) name_pointer, attribs);

}

DrmServerBuffer::~DrmServerBuffer()
{
    m_integration->eglDestroyImageKHR(m_image);
}

void DrmServerBuffer::bindTextureToBuffer()
{
    if (!QOpenGLContext::currentContext())
        qWarning("DrmServerBuffer: creating texture with no current context");

    m_integration->glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, m_image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void DrmEglServerBufferIntegration::initialize(QWaylandDisplay *display)
{
    m_egl_display = eglGetDisplay((EGLNativeDisplayType) display->wl_display());
    if (EGL_NO_DISPLAY) {
        qWarning("Failed to initialize drm egl server buffer integration. Could not get egl display from wl_display.");
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

    m_gl_egl_image_target_texture = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    if (!m_gl_egl_image_target_texture) {
        qWarning("Failed to initialize drm egl server buffer integration. Could not resolve glEGLImageTargetTexture2DOES");
        return;
    }

    QtWayland::wl_registry::init(wl_display_get_registry(display->wl_display()));
}

QWaylandServerBuffer *DrmEglServerBufferIntegration::serverBuffer(struct qt_server_buffer *buffer)
{
    return static_cast<QWaylandServerBuffer *>(qt_server_buffer_get_user_data(buffer));
}

void DrmEglServerBufferIntegration::registry_global(uint32_t name, const QString &interface, uint32_t version)
{
    Q_UNUSED(version);
    if (interface == QStringLiteral("qt_drm_egl_server_buffer")) {
        struct ::wl_registry *registry = QtWayland::wl_registry::object();
        QtWayland::qt_drm_egl_server_buffer::init(registry, name, 1);
    }
}

void DrmEglServerBufferIntegration::drm_egl_server_buffer_server_buffer_created(struct ::qt_server_buffer *id
                                                                               , int32_t name
                                                                               , int32_t width
                                                                               , int32_t height
                                                                               , int32_t stride
                                                                               , int32_t format)
{
    DrmServerBuffer *server_buffer = new DrmServerBuffer(this, name, width, height, stride, format);
    qt_server_buffer_set_user_data(id, server_buffer);
}

}

QT_END_NAMESPACE
