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

#ifndef DRMEGLSERVERBUFFERINTEGRATION_H
#define DRMEGLSERVERBUFFERINTEGRATION_H

#include <QtCompositor/private/qwlserverbufferintegration_p.h>

#include "qwayland-server-drm-egl-server-buffer.h"

#include <QtGui/QWindow>
#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QtGui/QGuiApplication>

#include <QtCompositor/qwaylandcompositor.h>
#include <QtCompositor/private/qwayland-server-server-buffer-extension.h>

#include <QtCore/QDebug>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifndef EGL_KHR_image
typedef void *EGLImageKHR;
typedef EGLImageKHR (EGLAPIENTRYP PFNEGLCREATEIMAGEKHRPROC) (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYIMAGEKHRPROC) (EGLDisplay dpy, EGLImageKHR image);
#endif

#ifndef GL_OES_EGL_image
typedef void (GL_APIENTRYP PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) (GLenum target, GLeglImageOES image);
#endif
#ifndef EGL_MESA_drm_image
typedef EGLImageKHR (EGLAPIENTRYP PFNEGLCREATEDRMIMAGEMESAPROC) (EGLDisplay dpy, const EGLint *attrib_list);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLEXPORTDRMIMAGEMESAPROC) (EGLDisplay dpy, EGLImageKHR image, EGLint *name, EGLint *handle, EGLint *stride);
#endif

QT_BEGIN_NAMESPACE

class DrmEglServerBufferIntegration;

class DrmEglServerBuffer : public QtWayland::ServerBuffer, public QtWaylandServer::qt_server_buffer
{
public:
    DrmEglServerBuffer(DrmEglServerBufferIntegration *integration, const QSize &size, QtWayland::ServerBuffer::Format format);

    struct ::wl_resource *resourceForClient(struct ::wl_client *) Q_DECL_OVERRIDE;
    void bindTextureToBuffer() Q_DECL_OVERRIDE;

private:
    DrmEglServerBufferIntegration *m_integration;

    EGLImageKHR m_image;

    int32_t m_name;
    int32_t m_stride;
    QtWaylandServer::qt_drm_egl_server_buffer::format m_drm_format;
};

class DrmEglServerBufferIntegration :
    public QtWayland::ServerBufferIntegration,
    public QtWaylandServer::qt_drm_egl_server_buffer
{
public:
    DrmEglServerBufferIntegration();
    ~DrmEglServerBufferIntegration();

    void initializeHardware(QWaylandCompositor *);

    bool supportsFormat(QtWayland::ServerBuffer::Format format) const Q_DECL_OVERRIDE;
    QtWayland::ServerBuffer *createServerBuffer(const QSize &size, QtWayland::ServerBuffer::Format format) Q_DECL_OVERRIDE;

    EGLDisplay display() const { return m_egl_display; }

    inline EGLImageKHR eglCreateImageKHR(EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);
    inline EGLBoolean eglDestroyImageKHR (EGLImageKHR image);
    inline EGLImageKHR eglCreateDRMImageMESA (const EGLint *attrib_list);
    inline EGLBoolean eglExportDRMImageMESA (EGLImageKHR image, EGLint *name, EGLint *handle, EGLint *stride);
    inline void glEGLImageTargetTexture2DOES (GLenum target, GLeglImageOES image);

private:
    EGLDisplay m_egl_display;
    PFNEGLCREATEDRMIMAGEMESAPROC m_egl_create_drm_image;
    PFNEGLEXPORTDRMIMAGEMESAPROC m_egl_export_drm_image;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC m_gl_egl_image_target_texture_2d;

    PFNEGLCREATEIMAGEKHRPROC m_egl_create_image;
    PFNEGLDESTROYIMAGEKHRPROC m_egl_destroy_image;
};

EGLImageKHR DrmEglServerBufferIntegration::eglCreateImageKHR(EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list)
{
    if (!m_egl_create_image) {
        qWarning("DrmEglServerBufferIntegration: Trying to used unresolved function eglCreateImageKHR");
        return EGL_NO_IMAGE_KHR;
    }
    return m_egl_create_image(m_egl_display, ctx, target, buffer,attrib_list);
}

EGLBoolean DrmEglServerBufferIntegration::eglDestroyImageKHR (EGLImageKHR image)
{
    if (!m_egl_destroy_image) {
        qWarning("DrmEglServerBufferIntegration: Trying to use unresolved function eglDestroyImageKHR");
        return false;
    }
    return m_egl_destroy_image(m_egl_display, image);
}

EGLImageKHR DrmEglServerBufferIntegration::eglCreateDRMImageMESA (const EGLint *attrib_list)
{
    if (m_egl_create_drm_image)
        return m_egl_create_drm_image(m_egl_display, attrib_list);
    else
        qWarning("DrmEglServerBufferIntegration: Trying to use unresolved function eglCreateDRMImageMESA");
    return EGL_NO_IMAGE_KHR;

}

EGLBoolean DrmEglServerBufferIntegration::eglExportDRMImageMESA (EGLImageKHR image, EGLint *name, EGLint *handle, EGLint *stride)
{
    if (m_egl_export_drm_image)
        return m_egl_export_drm_image(m_egl_display, image, name, handle, stride);
    else
        qWarning("DrmEglServerBufferIntegration: Trying to use unresolved function eglExportDRMImageMESA");
    return 0;
}

void DrmEglServerBufferIntegration::glEGLImageTargetTexture2DOES (GLenum target, GLeglImageOES image)
{
    if (m_gl_egl_image_target_texture_2d)
        return m_gl_egl_image_target_texture_2d(target, image);
    else
        qWarning("DrmEglServerBufferIntegration: Trying to use unresolved function glEGLImageTargetTexture2DOES");
}
QT_END_NAMESPACE

#endif
