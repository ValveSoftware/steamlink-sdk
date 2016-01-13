/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "avfvideoframerenderer.h"

#include <QtMultimedia/qabstractvideosurface.h>
#include <QtGui/QOpenGLFramebufferObject>
#include <QtGui/QWindow>

#ifdef QT_DEBUG_AVF
#include <QtCore/qdebug.h>
#endif

#import <CoreVideo/CVBase.h>
#import <AVFoundation/AVFoundation.h>

QT_USE_NAMESPACE

AVFVideoFrameRenderer::AVFVideoFrameRenderer(QAbstractVideoSurface *surface, QObject *parent)
    : QObject(parent)
    , m_videoLayerRenderer(0)
    , m_surface(surface)
    , m_glContext(0)
    , m_currentBuffer(1)
    , m_isContextShared(true)
{
    m_fbo[0] = 0;
    m_fbo[1] = 0;

    //Create Hidden QWindow surface to create context in this thread
    m_offscreenSurface = new QWindow();
    m_offscreenSurface->setSurfaceType(QWindow::OpenGLSurface);
    //Needs geometry to be a valid surface, but size is not important
    m_offscreenSurface->setGeometry(0, 0, 1, 1);
    m_offscreenSurface->create();
}

AVFVideoFrameRenderer::~AVFVideoFrameRenderer()
{
#ifdef QT_DEBUG_AVF
    qDebug() << Q_FUNC_INFO;
#endif

    [m_videoLayerRenderer release];
    delete m_fbo[0];
    delete m_fbo[1];
    delete m_offscreenSurface;
    delete m_glContext;
}

GLuint AVFVideoFrameRenderer::renderLayerToTexture(AVPlayerLayer *layer)
{
    //Is layer valid
    if (!layer)
        return 0;

    //If the glContext isn't shared, it doesn't make sense to return a texture for us
    if (m_offscreenSurface && !m_isContextShared)
        return 0;

    QOpenGLFramebufferObject *fbo = initRenderer(layer);

    if (!fbo)
        return 0;

    renderLayerToFBO(layer, fbo);
    m_glContext->doneCurrent();

    return fbo->texture();
}

QImage AVFVideoFrameRenderer::renderLayerToImage(AVPlayerLayer *layer)
{
    //Is layer valid
    if (!layer) {
        return QImage();
    }

    QOpenGLFramebufferObject *fbo = initRenderer(layer);

    if (!fbo)
        return QImage();

    renderLayerToFBO(layer, fbo);
    QImage fboImage = fbo->toImage();
    m_glContext->doneCurrent();

    return fboImage;
}

QOpenGLFramebufferObject *AVFVideoFrameRenderer::initRenderer(AVPlayerLayer *layer)
{

    //Get size from AVPlayerLayer
    m_targetSize = QSize(layer.bounds.size.width, layer.bounds.size.height);

    //Make sure we have an OpenGL context to make current
    if (!m_glContext) {
        //Create OpenGL context and set share context from surface
        QOpenGLContext *shareContext = 0;
        if (m_surface) {
            //QOpenGLContext *renderThreadContext = 0;
            shareContext = qobject_cast<QOpenGLContext*>(m_surface->property("GLContext").value<QObject*>());
        }
        m_glContext = new QOpenGLContext();
        m_glContext->setFormat(m_offscreenSurface->requestedFormat());

        if (shareContext) {
            m_glContext->setShareContext(shareContext);
            m_isContextShared = true;
        } else {
#ifdef QT_DEBUG_AVF
            qWarning("failed to get Render Thread context");
#endif
            m_isContextShared = false;
        }
        if (!m_glContext->create()) {
            qWarning("failed to create QOpenGLContext");
            return 0;
        }
    }

    //Need current context
    m_glContext->makeCurrent(m_offscreenSurface);

    //Create the CARenderer if needed
    if (!m_videoLayerRenderer) {
        m_videoLayerRenderer = [CARenderer rendererWithCGLContext: CGLGetCurrentContext() options: nil];
        [m_videoLayerRenderer retain];
    }

    //Set/Change render source if needed
    if (m_videoLayerRenderer.layer != layer) {
        m_videoLayerRenderer.layer = layer;
        m_videoLayerRenderer.bounds = layer.bounds;
    }

    //Do we have FBO's already?
    if ((!m_fbo[0] && !m_fbo[0]) || (m_fbo[0]->size() != m_targetSize)) {
        delete m_fbo[0];
        delete m_fbo[1];
        m_fbo[0] = new QOpenGLFramebufferObject(m_targetSize);
        m_fbo[1] = new QOpenGLFramebufferObject(m_targetSize);
    }

    //Switch buffer target
    m_currentBuffer = !m_currentBuffer;
    return m_fbo[m_currentBuffer];
}

void AVFVideoFrameRenderer::renderLayerToFBO(AVPlayerLayer *layer, QOpenGLFramebufferObject *fbo)
{
    //Start Rendering
    //NOTE: This rendering method will NOT work on iOS as there is no CARenderer in iOS
    if (!fbo->bind()) {
        qWarning("AVFVideoRender FBO failed to bind");
        return;
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glViewport(0, 0, m_targetSize.width(), m_targetSize.height());

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    //Render to FBO with inverted Y
    glOrtho(0.0, m_targetSize.width(), 0.0, m_targetSize.height(), 0.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    [m_videoLayerRenderer beginFrameAtTime:CACurrentMediaTime() timeStamp:NULL];
    [m_videoLayerRenderer addUpdateRect:layer.bounds];
    [m_videoLayerRenderer render];
    [m_videoLayerRenderer endFrame];

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glFinish(); //Rendering needs to be done before passing texture to video frame

    fbo->release();
}
