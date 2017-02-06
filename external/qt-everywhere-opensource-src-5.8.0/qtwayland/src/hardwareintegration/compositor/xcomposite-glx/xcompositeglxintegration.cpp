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

#include "xcompositeglxintegration.h"

#include "wayland-xcomposite-server-protocol.h"

#include <qpa/qplatformnativeinterface.h>
#include <qpa/qplatformintegration.h>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLTexture>

#include "xcompositebuffer.h"
#include "xcompositehandler.h"
#include <X11/extensions/Xcomposite.h>

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

QVector<int> qglx_buildSpec()
{
    QVector<int> spec(48);
    int i = 0;

    spec[i++] = GLX_LEVEL;
    spec[i++] = 0;
    spec[i++] = GLX_DRAWABLE_TYPE; spec[i++] = GLX_PIXMAP_BIT | GLX_WINDOW_BIT;
    spec[i++] = GLX_BIND_TO_TEXTURE_TARGETS_EXT; spec[i++] = GLX_TEXTURE_2D_BIT_EXT;
    spec[i++] = GLX_BIND_TO_TEXTURE_RGB_EXT; spec[i++] = true;

    spec[i++] = 0;
    return spec;
}


XCompositeGLXClientBufferIntegration::XCompositeGLXClientBufferIntegration()
    : QtWayland::ClientBufferIntegration()
    , mDisplay(0)
    , mHandler(0)
{
    qDebug() << "Loading GLX integration";
}

XCompositeGLXClientBufferIntegration::~XCompositeGLXClientBufferIntegration()
{
    delete mHandler;
}

void XCompositeGLXClientBufferIntegration::initializeHardware(struct ::wl_display *)
{
    qDebug() << "Initializing GLX integration";
    QPlatformNativeInterface *nativeInterface = QGuiApplicationPrivate::platformIntegration()->nativeInterface();
    if (nativeInterface) {
        mDisplay = static_cast<Display *>(nativeInterface->nativeResourceForIntegration("Display"));
        if (!mDisplay)
            qFatal("could not retrieve Display from platform integration");
    } else {
        qFatal("Platform integration doesn't have native interface");
    }
    mScreen = XDefaultScreen(mDisplay);

    mHandler = new XCompositeHandler(m_compositor, mDisplay);

    QOpenGLContext *glContext = new QOpenGLContext();
    glContext->create();

    m_glxBindTexImageEXT = reinterpret_cast<PFNGLXBINDTEXIMAGEEXTPROC>(glContext->getProcAddress("glXBindTexImageEXT"));
    if (!m_glxBindTexImageEXT) {
        qDebug() << "Did not find glxBindTexImageExt, everything will FAIL!";
    }
    m_glxReleaseTexImageEXT = reinterpret_cast<PFNGLXRELEASETEXIMAGEEXTPROC>(glContext->getProcAddress("glXReleaseTexImageEXT"));
    if (!m_glxReleaseTexImageEXT) {
        qDebug() << "Did not find glxReleaseTexImageExt";
    }

    delete glContext;
}

QtWayland::ClientBuffer *XCompositeGLXClientBufferIntegration::createBufferFor(wl_resource *buffer)
{
    if (wl_shm_buffer_get(buffer))
        return nullptr;
    return new XCompositeGLXClientBuffer(this, buffer);
}

XCompositeGLXClientBuffer::XCompositeGLXClientBuffer(XCompositeGLXClientBufferIntegration *integration, wl_resource *bufferResource)
    : QtWayland::ClientBuffer(bufferResource)
    , m_texture(nullptr)
    , m_integration(integration)
    , m_glxPixmap(0)
{
}


QOpenGLTexture *XCompositeGLXClientBuffer::toOpenGlTexture(int plane)
{
    Q_UNUSED(plane);
    XCompositeBuffer *compositorBuffer = XCompositeBuffer::fromResource(m_buffer);
    Pixmap pixmap = XCompositeNameWindowPixmap(m_integration->xDisplay(), compositorBuffer->window());

    QVector<int> glxConfigSpec = qglx_buildSpec();
    int numberOfConfigs;
    GLXFBConfig *configs = glXChooseFBConfig(m_integration->xDisplay(),m_integration->xScreen(),glxConfigSpec.constData(),&numberOfConfigs);

    QVector<int> attribList;
    attribList.append(GLX_TEXTURE_FORMAT_EXT);
    attribList.append(GLX_TEXTURE_FORMAT_RGB_EXT);
    attribList.append(GLX_TEXTURE_TARGET_EXT);
    attribList.append(GLX_TEXTURE_2D_EXT);
    attribList.append(0);

    if (!m_glxPixmap)
        m_glxPixmap = glXCreatePixmap(m_integration->xDisplay(), *configs, pixmap, attribList.constData());

    uint inverted = 0;
    glXQueryDrawable(m_integration->xDisplay(), m_glxPixmap, GLX_Y_INVERTED_EXT,&inverted);
    compositorBuffer->setOrigin(inverted ? QWaylandSurface::OriginBottomLeft : QWaylandSurface::OriginTopLeft);

    XFree(configs);
    auto tex = m_texture;
    if (!m_texture) {
        tex = new QOpenGLTexture(QOpenGLTexture::Target2D);
        tex->create();
        m_texture = tex;
    }
    tex->bind();
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    m_integration->m_glxBindTexImageEXT(m_integration->xDisplay(),m_glxPixmap,GLX_FRONT_EXT, 0);

    // TODO: release in the destructor?
    // m_glxReleaseTexImageEXT(mDisplay,glxPixmap,GLX_FRONT_EXT);
    return tex;
}


QWaylandSurface::Origin XCompositeGLXClientBuffer::origin() const
{
    XCompositeBuffer *compositorBuffer = XCompositeBuffer::fromResource(m_buffer);
    return compositorBuffer->origin();
}

QSize XCompositeGLXClientBuffer::size() const
{
    XCompositeBuffer *compositorBuffer = XCompositeBuffer::fromResource(m_buffer);

    return compositorBuffer->size();
}

QT_END_NAMESPACE
