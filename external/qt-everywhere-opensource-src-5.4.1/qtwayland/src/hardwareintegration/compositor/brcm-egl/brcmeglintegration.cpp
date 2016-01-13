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

#include "brcmeglintegration.h"
#include "brcmbuffer.h"
#include <QtCompositor/private/qwlsurface_p.h>
#include <QtCompositor/private/qwlcompositor_p.h>
#include <QtCompositor/qwaylandsurface.h>
#include <qpa/qplatformnativeinterface.h>
#include <QtGui/QGuiApplication>
#include <QtGui/QOpenGLContext>
#include <qpa/qplatformscreen.h>
#include <QtGui/QWindow>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#define EGL_EGLEXT_PROTOTYPES
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

void BrcmEglIntegration::initializeHardware(QtWayland::Display *waylandDisplay)
{
    Q_D(BrcmEglIntegration);

    QPlatformNativeInterface *nativeInterface = QGuiApplication::platformNativeInterface();
    if (nativeInterface) {
        d->egl_display = nativeInterface->nativeResourceForIntegration("EglDisplay");
        if (!d->egl_display)
            qWarning("Failed to acquire EGL display from platform integration");

        d->eglQueryGlobalImageBRCM = eglQueryGlobalImageBRCM;

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
        init(waylandDisplay->handle(), 1);
    }
}

void BrcmEglIntegration::bindTextureToBuffer(struct ::wl_resource *buffer)
{
    Q_D(BrcmEglIntegration);
    if (!d->valid) {
        qWarning("bindTextureToBuffer failed!");
        return;
    }

    BrcmBuffer *brcmBuffer = BrcmBuffer::fromResource(buffer);

    if (!d->eglQueryGlobalImageBRCM(brcmBuffer->handle(), brcmBuffer->handle() + 2)) {
        qWarning("eglQueryGlobalImageBRCM failed!");
        return;
    }

    EGLImageKHR image = d->eglCreateImageKHR(d->egl_display, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)brcmBuffer->handle(), NULL);
    if (image == EGL_NO_IMAGE_KHR)
        qWarning("eglCreateImageKHR() failed: %x\n", eglGetError());

    d->glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    d->eglDestroyImageKHR(d->egl_display, image);
}

bool BrcmEglIntegration::isYInverted(struct ::wl_resource *) const
{
    return false;
}

void BrcmEglIntegration::brcm_bind_resource(Resource *)
{
}

void BrcmEglIntegration::brcm_create_buffer(Resource *resource, uint32_t id, int32_t width, int32_t height, wl_array *data)
{
    new BrcmBuffer(resource->client(), id, QSize(width, height), static_cast<EGLint *>(data->data), data->size / sizeof(EGLint));
}

QSize BrcmEglIntegration::bufferSize(struct ::wl_resource *buffer) const
{
    BrcmBuffer *brcmBuffer = BrcmBuffer::fromResource(buffer);

    return brcmBuffer->size();
}

QT_END_NAMESPACE
