/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd, author: <giulio.camuffo@jollamobile.com>
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

#ifndef LIBHYBRISEGLSERVERBUFFERINTEGRATION_H
#define LIBHYBRISEGLSERVERBUFFERINTEGRATION_H

#include <QtWaylandClient/private/qwayland-wayland.h>
#include "qwayland-libhybris-egl-server-buffer.h"
#include <QtWaylandClient/private/qwaylandserverbufferintegration_p.h>
#include <QtWaylandClient/private/qwaylanddisplay_p.h>
#include <QtCore/QTextStream>
#include <QtCore/QVector>

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

#ifndef EGL_HYBRIS_native_buffer
typedef EGLBoolean (EGLAPIENTRYP PFNEGLHYBRISCREATEREMOTEBUFFERPROC)(EGLint width, EGLint height, EGLint usage, EGLint format, EGLint stride,
                                                                     int num_ints, int *ints, int num_fds, int *fds, EGLClientBuffer *buffer);
#endif

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

class LibHybrisEglServerBufferIntegration;

class LibHybrisServerBuffer : public QWaylandServerBuffer, public QtWayland::qt_libhybris_buffer
{
public:
    LibHybrisServerBuffer(LibHybrisEglServerBufferIntegration *integration, int32_t numFds, wl_array *ints, int32_t name, int32_t width, int32_t height, int32_t stride, int32_t format);
    ~LibHybrisServerBuffer();
    void bindTextureToBuffer() Q_DECL_OVERRIDE;

protected:
    void libhybris_buffer_add_fd(int32_t fd) Q_DECL_OVERRIDE;

private:
    LibHybrisEglServerBufferIntegration *m_integration;
    EGLImageKHR m_image;
    int m_numFds;
    QVector<int32_t> m_ints;
    QVector<int32_t> m_fds;
    int32_t m_stride;
    int32_t m_hybrisFormat;
};

class LibHybrisEglServerBufferIntegration
    : public QWaylandServerBufferIntegration
    , public QtWayland::wl_registry
    , public QtWayland::qt_libhybris_egl_server_buffer
{
public:
    void initialize(QWaylandDisplay *display) Q_DECL_OVERRIDE;

    virtual QWaylandServerBuffer *serverBuffer(struct qt_server_buffer *buffer) Q_DECL_OVERRIDE;

    inline EGLImageKHR eglCreateImageKHR(EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);
    inline EGLBoolean eglDestroyImageKHR (EGLImageKHR image);
    inline void glEGLImageTargetTexture2DOES (GLenum target, GLeglImageOES image);
    inline EGLBoolean eglHybrisCreateRemoteBuffer(EGLint width, EGLint height, EGLint usage, EGLint format, EGLint stride, int num_ints, int *ints, int num_fds, int *fds, EGLClientBuffer *buffer);

protected:
    void registry_global(uint32_t name, const QString &interface, uint32_t version) Q_DECL_OVERRIDE;
    void libhybris_egl_server_buffer_server_buffer_created(struct ::qt_libhybris_buffer *id, struct ::qt_server_buffer *qid,
                                                           int32_t numFds, wl_array *ints, int32_t name, int32_t width, int32_t height, int32_t stride, int32_t format) Q_DECL_OVERRIDE;
private:
    PFNEGLCREATEIMAGEKHRPROC m_egl_create_image;
    PFNEGLDESTROYIMAGEKHRPROC m_egl_destroy_image;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC m_gl_egl_image_target_texture;
    PFNEGLHYBRISCREATEREMOTEBUFFERPROC m_egl_create_buffer;
    EGLDisplay m_egl_display;
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
    if (!m_gl_egl_image_target_texture) {
        qWarning("LibHybrisEglServerBufferIntegration: Trying to use unresolved function glEGLImageTargetRenderbufferStorageOES");
        return;
    }
    m_gl_egl_image_target_texture(target,image);
}

EGLBoolean LibHybrisEglServerBufferIntegration::eglHybrisCreateRemoteBuffer(EGLint width, EGLint height, EGLint usage, EGLint format, EGLint stride,
                                                                            int num_ints, int *ints, int num_fds, int *fds, EGLClientBuffer *buffer)
{
    if (!m_egl_create_buffer) {
        qWarning("LibHybrisEglServerBufferIntegration: Trying to use unresolved function eglHybrisCreateRemoteBuffer");
        return false;
    }
    return m_egl_create_buffer(width, height, usage, format, stride, num_ints, ints, num_fds, fds, buffer);
}

}

QT_END_NAMESPACE

#endif
