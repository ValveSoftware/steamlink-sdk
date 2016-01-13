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

#include "xcompositeglxintegration.h"

#include <QtCompositor/private/qwlcompositor_p.h>
#include "wayland-xcomposite-server-protocol.h"

#include <qpa/qplatformnativeinterface.h>
#include <qpa/qplatformintegration.h>
#include <QtGui/QOpenGLContext>

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

void XCompositeGLXClientBufferIntegration::initializeHardware(QtWayland::Display *)
{
    qDebug() << "Initializing GLX integration";
    QPlatformNativeInterface *nativeInterface = QGuiApplicationPrivate::platformIntegration()->nativeInterface();
    if (nativeInterface) {
        mDisplay = static_cast<Display *>(nativeInterface->nativeResourceForWindow("Display",m_compositor->window()));
        if (!mDisplay)
            qFatal("could not retireve Display from platform integration");
    } else {
        qFatal("Platform integration doesn't have native interface");
    }
    mScreen = XDefaultScreen(mDisplay);

    mHandler = new XCompositeHandler(m_compositor->handle(), mDisplay);

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

void XCompositeGLXClientBufferIntegration::bindTextureToBuffer(struct ::wl_resource *buffer)
{
    XCompositeBuffer *compositorBuffer = XCompositeBuffer::fromResource(buffer);
    Pixmap pixmap = XCompositeNameWindowPixmap(mDisplay, compositorBuffer->window());

    QVector<int> glxConfigSpec = qglx_buildSpec();
    int numberOfConfigs;
    GLXFBConfig *configs = glXChooseFBConfig(mDisplay,mScreen,glxConfigSpec.constData(),&numberOfConfigs);

    QVector<int> attribList;
    attribList.append(GLX_TEXTURE_FORMAT_EXT);
    attribList.append(GLX_TEXTURE_FORMAT_RGB_EXT);
    attribList.append(GLX_TEXTURE_TARGET_EXT);
    attribList.append(GLX_TEXTURE_2D_EXT);
    attribList.append(0);
    GLXPixmap glxPixmap = glXCreatePixmap(mDisplay,*configs,pixmap,attribList.constData());

    uint inverted = 0;
    glXQueryDrawable(mDisplay, glxPixmap, GLX_Y_INVERTED_EXT,&inverted);
    compositorBuffer->setInvertedY(!inverted);

    XFree(configs);

    m_glxBindTexImageEXT(mDisplay,glxPixmap,GLX_FRONT_EXT, 0);
    //Do we need to change the api so that we do bind and release in the painevent?
    //The specification states that when deleting the texture the color buffer is deleted
//    m_glxReleaseTexImageEXT(mDisplay,glxPixmap,GLX_FRONT_EXT);
}

bool XCompositeGLXClientBufferIntegration::isYInverted(struct ::wl_resource *buffer) const
{
    return XCompositeBuffer::fromResource(buffer)->isYInverted();
}

QSize XCompositeGLXClientBufferIntegration::bufferSize(struct ::wl_resource *buffer) const
{
    XCompositeBuffer *compositorBuffer = XCompositeBuffer::fromResource(buffer);

    return compositorBuffer->size();
}

QT_END_NAMESPACE
