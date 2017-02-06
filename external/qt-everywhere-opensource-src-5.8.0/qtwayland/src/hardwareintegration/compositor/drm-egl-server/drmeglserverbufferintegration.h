/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#ifndef DRMEGLSERVERBUFFERINTEGRATION_H
#define DRMEGLSERVERBUFFERINTEGRATION_H

#include <QtWaylandCompositor/private/qwlserverbufferintegration_p.h>

#include "qwayland-server-drm-egl-server-buffer.h"

#include <QtGui/QWindow>
#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QtGui/QGuiApplication>

#include <QtWaylandCompositor/qwaylandcompositor.h>
#include <QtWaylandCompositor/private/qwayland-server-server-buffer-extension.h>

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
