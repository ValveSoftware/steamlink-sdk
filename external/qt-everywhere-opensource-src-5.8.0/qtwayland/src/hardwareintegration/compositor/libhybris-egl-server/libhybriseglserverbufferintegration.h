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

#ifndef LIBHYBRISEGLSERVERBUFFERINTEGRATION_H
#define LIBHYBRISEGLSERVERBUFFERINTEGRATION_H

#include <QtWaylandCompositor/private/qwlserverbufferintegration_p.h>

#include "qwayland-server-libhybris-egl-server-buffer.h"

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

#ifndef EGL_HYBRIS_native_buffer
typedef EGLBoolean (EGLAPIENTRYP PFNEGLHYBRISCREATENATIVEBUFFERPROC)(EGLint width, EGLint height, EGLint usage, EGLint format, EGLint *stride, EGLClientBuffer *buffer);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLHYBRISGETNATIVEBUFFERINFOPROC)(EGLClientBuffer buffer, int *num_ints, int *num_fds);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLHYBRISSERIALIZENATIVEBUFFERPROC)(EGLClientBuffer buffer, int *ints, int *fds);
#endif

#ifndef GL_OES_EGL_image
typedef void (GL_APIENTRYP PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) (GLenum target, GLeglImageOES image);
#endif

QT_BEGIN_NAMESPACE

class LibHybrisEglServerBufferIntegration;

class LibHybrisEglServerBuffer : public QtWayland::ServerBuffer, public QtWaylandServer::qt_libhybris_buffer
{
public:
    LibHybrisEglServerBuffer(LibHybrisEglServerBufferIntegration *integration, const QSize &size, QtWayland::ServerBuffer::Format format);

    struct ::wl_resource *resourceForClient(struct ::wl_client *) Q_DECL_OVERRIDE;
    void bindTextureToBuffer() Q_DECL_OVERRIDE;

private:
    LibHybrisEglServerBufferIntegration *m_integration;

    EGLImageKHR m_image;
    EGLClientBuffer m_buffer;

    int32_t m_name;
    int32_t m_stride;
    QtWaylandServer::qt_libhybris_egl_server_buffer::format m_hybris_format;
    QVector<int32_t> m_ints;
    QVector<int32_t> m_fds;
    QHash<Resource *, wl_resource *> m_qtbuffers;
};

class LibHybrisEglServerBufferIntegration :
    public QtWayland::ServerBufferIntegration,
    public QtWaylandServer::qt_libhybris_egl_server_buffer
{
public:
    LibHybrisEglServerBufferIntegration();
    ~LibHybrisEglServerBufferIntegration();

    void initializeHardware(QWaylandCompositor *);

    bool supportsFormat(QtWayland::ServerBuffer::Format format) const Q_DECL_OVERRIDE;
    QtWayland::ServerBuffer *createServerBuffer(const QSize &size, QtWayland::ServerBuffer::Format format) Q_DECL_OVERRIDE;

    EGLDisplay display() const { return m_egl_display; }

    inline EGLImageKHR eglCreateImageKHR(EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);
    inline EGLBoolean eglDestroyImageKHR (EGLImageKHR image);
    inline void glEGLImageTargetTexture2DOES (GLenum target, GLeglImageOES image);

    inline EGLBoolean eglHybrisCreateNativeBuffer(EGLint width, EGLint height, EGLint usage, EGLint format, EGLint *stride, EGLClientBuffer *buffer);
    inline void eglHybrisGetNativeBufferInfo(EGLClientBuffer buffer, int *num_ints, int *num_fds);
    inline void eglHybrisSerializeNativeBuffer(EGLClientBuffer buffer, int *ints, int *fds);

private:
    EGLDisplay m_egl_display;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC m_gl_egl_image_target_texture_2d;

    PFNEGLHYBRISCREATENATIVEBUFFERPROC m_egl_create_buffer;
    PFNEGLHYBRISGETNATIVEBUFFERINFOPROC m_egl_get_buffer_info;
    PFNEGLHYBRISSERIALIZENATIVEBUFFERPROC m_egl_serialize_buffer;

    PFNEGLCREATEIMAGEKHRPROC m_egl_create_image;
    PFNEGLDESTROYIMAGEKHRPROC m_egl_destroy_image;
};

EGLImageKHR LibHybrisEglServerBufferIntegration::eglCreateImageKHR(EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list)
{
    if (!m_egl_create_image) {
        qWarning("LibHybrisEglServerBufferIntegration: Trying to used unresolved function eglCreateImageKHR");
        return EGL_NO_IMAGE_KHR;
    }
    return m_egl_create_image(m_egl_display, ctx, target, buffer,attrib_list);
}

EGLBoolean LibHybrisEglServerBufferIntegration::eglDestroyImageKHR (EGLImageKHR image)
{
    if (!m_egl_destroy_image) {
        qWarning("LibHybrisEglServerBufferIntegration: Trying to use unresolved function eglDestroyImageKHR");
        return false;
    }
    return m_egl_destroy_image(m_egl_display, image);
}

void LibHybrisEglServerBufferIntegration::glEGLImageTargetTexture2DOES (GLenum target, GLeglImageOES image)
{
    if (m_gl_egl_image_target_texture_2d)
        return m_gl_egl_image_target_texture_2d(target, image);
    else
        qWarning("LibHybrisEglServerBufferIntegration: Trying to use unresolved function glEGLImageTargetTexture2DOES");
}

EGLBoolean LibHybrisEglServerBufferIntegration::eglHybrisCreateNativeBuffer(EGLint width, EGLint height, EGLint usage, EGLint format, EGLint *stride, EGLClientBuffer *buffer)
{
    if (m_egl_create_buffer)
        return m_egl_create_buffer(width, height, usage, format, stride, buffer);
    else
        qWarning("LibHybrisEglServerBufferIntegration: Trying to use unresolved function eglHybrisCreateNativeBuffer");
    return EGL_FALSE;
}

void LibHybrisEglServerBufferIntegration::eglHybrisGetNativeBufferInfo(EGLClientBuffer buffer, int *num_ints, int *num_fds)
{
    if (m_egl_get_buffer_info)
        m_egl_get_buffer_info(buffer, num_ints, num_fds);
    else
        qWarning("LibHybrisEglServerBufferIntegration: Trying to use unresolved function eglHybrisGetNativeBufferInfo");
}

void LibHybrisEglServerBufferIntegration::eglHybrisSerializeNativeBuffer(EGLClientBuffer buffer, int *ints, int *fds)
{
    if (m_egl_serialize_buffer)
        m_egl_serialize_buffer(buffer, ints, fds);
    else
        qWarning("LibHybrisEglServerBufferIntegration: Trying to use unresolved function eglHybrisSerializeNativeBuffer");
}

QT_END_NAMESPACE

#endif
