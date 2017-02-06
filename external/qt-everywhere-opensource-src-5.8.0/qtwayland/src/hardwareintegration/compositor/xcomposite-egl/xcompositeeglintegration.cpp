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

#include "xcompositeeglintegration.h"

#include "wayland-xcomposite-server-protocol.h"

#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtGui/QGuiApplication>
#include <QtGui/QOpenGLTexture>
#include <qpa/qplatformnativeinterface.h>
#include <qpa/qplatformopenglcontext.h>

#include "xcompositebuffer.h"
#include "xcompositehandler.h"
#include <X11/extensions/Xcomposite.h>

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

QVector<EGLint> eglbuildSpec()
{
    QVector<EGLint> spec;

    spec.append(EGL_SURFACE_TYPE); spec.append(EGL_WINDOW_BIT | EGL_PIXMAP_BIT);
    spec.append(EGL_RENDERABLE_TYPE); spec.append(EGL_OPENGL_ES2_BIT);
    spec.append(EGL_BIND_TO_TEXTURE_RGBA); spec.append(EGL_TRUE);
    spec.append(EGL_ALPHA_SIZE); spec.append(8);
    spec.append(EGL_NONE);
    return spec;
}

XCompositeEglClientBufferIntegration::XCompositeEglClientBufferIntegration()
    : QtWayland::ClientBufferIntegration()
    , mDisplay(0)
{

}

void XCompositeEglClientBufferIntegration::initializeHardware(struct ::wl_display *)
{
    QPlatformNativeInterface *nativeInterface = QGuiApplication::platformNativeInterface();
    if (nativeInterface) {
        mDisplay = static_cast<Display *>(nativeInterface->nativeResourceForIntegration("Display"));
        if (!mDisplay)
            qFatal("could not retrieve Display from platform integration");
        mEglDisplay = static_cast<EGLDisplay>(nativeInterface->nativeResourceForIntegration("EGLDisplay"));
        if (!mEglDisplay)
            qFatal("could not retrieve EGLDisplay from platform integration");
    } else {
        qFatal("Platform integration doesn't have native interface");
    }
    new XCompositeHandler(m_compositor, mDisplay);
}

QtWayland::ClientBuffer *XCompositeEglClientBufferIntegration::createBufferFor(wl_resource *buffer)
{
    if (wl_shm_buffer_get(buffer))
        return nullptr;
    return new XCompositeEglClientBuffer(this, buffer);
}


XCompositeEglClientBuffer::XCompositeEglClientBuffer(XCompositeEglClientBufferIntegration *integration, wl_resource *bufferResource)
    : QtWayland::ClientBuffer(bufferResource)
    , m_texture(nullptr)
    , m_integration(integration)
{
}

QOpenGLTexture *XCompositeEglClientBuffer::toOpenGlTexture(int plane)
{
    Q_UNUSED(plane);
    XCompositeBuffer *compositorBuffer = XCompositeBuffer::fromResource(m_buffer);
    Pixmap pixmap = XCompositeNameWindowPixmap(m_integration->xDisplay(), compositorBuffer->window());

    QVector<EGLint> eglConfigSpec = eglbuildSpec();

    EGLint matching = 0;
    EGLConfig config;
    bool matched = eglChooseConfig(m_integration->eglDisplay(),eglConfigSpec.constData(),&config,1,&matching);
    if (!matched || !matching) {
        qWarning("Could not retrieve a suitable EGL config");
        return nullptr;
    }

    QVector<EGLint> attribList;

    attribList.append(EGL_TEXTURE_FORMAT);
    attribList.append(EGL_TEXTURE_RGBA);
    attribList.append(EGL_TEXTURE_TARGET);
    attribList.append(EGL_TEXTURE_2D);
    attribList.append(EGL_NONE);

    EGLSurface surface = eglCreatePixmapSurface(m_integration->eglDisplay(),config,pixmap,attribList.constData());
    if (surface == EGL_NO_SURFACE) {
        qDebug() << "Failed to create eglsurface" << pixmap << compositorBuffer->window();
    }

    compositorBuffer->setOrigin(QWaylandSurface::OriginTopLeft);

    if (!m_texture) {
        m_texture = new QOpenGLTexture(QOpenGLTexture::Target2D);
        m_texture->create();
    }
    m_texture->bind();
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    if (!eglBindTexImage(m_integration->eglDisplay(),surface,EGL_BACK_BUFFER)) {
        qWarning() << "Failed to bind";
    }

    //    eglDestroySurface(mEglDisplay,surface);
    return m_texture;
}


QWaylandSurface::Origin XCompositeEglClientBuffer::origin() const
{
    XCompositeBuffer *compositorBuffer = XCompositeBuffer::fromResource(m_buffer);
    return compositorBuffer->origin();
}

QSize XCompositeEglClientBuffer::size() const
{
    XCompositeBuffer *compositorBuffer = XCompositeBuffer::fromResource(m_buffer);

    return compositorBuffer->size();
}

QT_END_NAMESPACE
