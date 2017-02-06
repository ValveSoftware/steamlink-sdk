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

#include "brcmeglintegration.h"
#include "brcmbuffer.h"
#include <QtWaylandCompositor/qwaylandsurface.h>
#include <qpa/qplatformnativeinterface.h>
#include <QtGui/QGuiApplication>
#include <QtGui/QOpenGLContext>
#include <QOpenGLTexture>
#include <qpa/qplatformscreen.h>
#include <QtGui/QWindow>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <EGL/eglext_brcm.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

QT_BEGIN_NAMESPACE

class BrcmEglIntegrationPrivate
{
public:
    BrcmEglIntegrationPrivate()
        : egl_display(EGL_NO_DISPLAY)
        , valid(false)
    { }

    static BrcmEglIntegrationPrivate *get(BrcmEglIntegration *integration);

    EGLDisplay egl_display;
    bool valid;
    PFNEGLQUERYGLOBALIMAGEBRCMPROC eglQueryGlobalImageBRCM;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
    PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
};

BrcmEglIntegration::BrcmEglIntegration()
    : QtWayland::ClientBufferIntegration()
    , QtWaylandServer::qt_brcm()
    , d_ptr(new BrcmEglIntegrationPrivate)
{
}

void BrcmEglIntegration::initializeHardware(struct ::wl_display *display)
{
    Q_D(BrcmEglIntegration);

    QPlatformNativeInterface *nativeInterface = QGuiApplication::platformNativeInterface();
    if (nativeInterface) {
        d->egl_display = nativeInterface->nativeResourceForIntegration("EglDisplay");
        if (!d->egl_display)
            qWarning("Failed to acquire EGL display from platform integration");

        d->eglQueryGlobalImageBRCM = (PFNEGLQUERYGLOBALIMAGEBRCMPROC) eglGetProcAddress("eglQueryGlobalImageBRCM");

        if (!d->eglQueryGlobalImageBRCM) {
            qWarning("Failed to resolve eglQueryGlobalImageBRCM");
            return;
        }

        d->glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");

        if (!d->glEGLImageTargetTexture2DOES) {
            qWarning("Failed to resolve glEGLImageTargetTexture2DOES");
            return;
        }

        d->eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");

        if (!d->eglCreateImageKHR) {
            qWarning("Failed to resolve eglCreateImageKHR");
            return;
        }

        d->eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");

        if (!d->eglDestroyImageKHR) {
            qWarning("Failed to resolve eglDestroyImageKHR");
            return;
        }
        d->valid = true;
        init(display, 1);
    }
}

QtWayland::ClientBuffer *BrcmEglIntegration::createBufferFor(wl_resource *buffer)
{
    if (wl_shm_buffer_get(buffer))
        return nullptr;
    return new BrcmEglClientBuffer(this, buffer);
}

BrcmEglIntegrationPrivate *BrcmEglIntegrationPrivate::get(BrcmEglIntegration *integration)
{
    return integration->d_ptr.data();
}

QOpenGLTexture *BrcmEglClientBuffer::toOpenGlTexture(int plane)
{
    Q_UNUSED(plane);

    auto d = BrcmEglIntegrationPrivate::get(m_integration);
    if (!d->valid) {
        qWarning("bindTextureToBuffer failed!");
        return nullptr;
    }

    BrcmBuffer *brcmBuffer = BrcmBuffer::fromResource(m_buffer);

    if (!d->eglQueryGlobalImageBRCM(brcmBuffer->handle(), brcmBuffer->handle() + 2)) {
        qWarning("eglQueryGlobalImageBRCM failed!");
        return nullptr;
    }

    EGLImageKHR image = d->eglCreateImageKHR(d->egl_display, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)brcmBuffer->handle(), NULL);
    if (image == EGL_NO_IMAGE_KHR)
        qWarning("eglCreateImageKHR() failed: %x\n", eglGetError());

    if (!m_texture) {
        m_texture = new QOpenGLTexture(QOpenGLTexture::Target2D);
        m_texture->create();
    }

    m_texture->bind();

    d->glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    d->eglDestroyImageKHR(d->egl_display, image);

    return m_texture;
}

void BrcmEglIntegration::brcm_bind_resource(Resource *)
{
}

void BrcmEglIntegration::brcm_create_buffer(Resource *resource, uint32_t id, int32_t width, int32_t height, wl_array *data)
{
    new BrcmBuffer(resource->client(), id, QSize(width, height), static_cast<EGLint *>(data->data), data->size / sizeof(EGLint));
}

BrcmEglClientBuffer::BrcmEglClientBuffer(BrcmEglIntegration *integration, wl_resource *buffer)
    : ClientBuffer(buffer)
    , m_integration(integration)
    , m_texture(nullptr)
{
}

QWaylandBufferRef::BufferFormatEgl BrcmEglClientBuffer::bufferFormatEgl() const
{
    return QWaylandBufferRef::BufferFormatEgl_RGBA;
}

QSize BrcmEglClientBuffer::size() const
{
    BrcmBuffer *brcmBuffer = BrcmBuffer::fromResource(m_buffer);
    return brcmBuffer->size();
}

QWaylandSurface::Origin BrcmEglClientBuffer::origin() const
{
    BrcmBuffer *brcmBuffer = BrcmBuffer::fromResource(m_buffer);
    return brcmBuffer->isYInverted() ? QWaylandSurface::OriginTopLeft : QWaylandSurface::OriginBottomLeft;
}


QT_END_NAMESPACE
