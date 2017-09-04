/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
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

#include "qt3dwindow.h"
#include "qt3dwindow_p.h"

#include <Qt3DCore/qaspectengine.h>
#include <Qt3DCore/qentity.h>
#include <Qt3DExtras/qforwardrenderer.h>
#include <Qt3DRender/qrendersettings.h>
#include <Qt3DRender/qrenderaspect.h>
#include <Qt3DInput/qinputaspect.h>
#include <Qt3DInput/qinputsettings.h>
#include <Qt3DLogic/qlogicaspect.h>
#include <Qt3DRender/qcamera.h>
#include <QtGui/qopenglcontext.h>

QT_BEGIN_NAMESPACE

namespace Qt3DExtras {

Qt3DWindowPrivate::Qt3DWindowPrivate()
    : m_aspectEngine(new Qt3DCore::QAspectEngine)
    , m_renderAspect(new Qt3DRender::QRenderAspect)
    , m_inputAspect(new Qt3DInput::QInputAspect)
    , m_logicAspect(new Qt3DLogic::QLogicAspect)
    , m_renderSettings(new Qt3DRender::QRenderSettings)
    , m_forwardRenderer(new Qt3DExtras::QForwardRenderer)
    , m_defaultCamera(new Qt3DRender::QCamera)
    , m_inputSettings(new Qt3DInput::QInputSettings)
    , m_root(new Qt3DCore::QEntity)
    , m_userRoot(nullptr)
    , m_initialized(false)
{
}

Qt3DWindow::Qt3DWindow(QScreen *screen)
    : QWindow(*new Qt3DWindowPrivate(), nullptr)
{
    Q_D(Qt3DWindow);

    if (!d->parentWindow)
        d->connectToScreen(screen ? screen : d->topLevelScreen.data());

    setSurfaceType(QSurface::OpenGLSurface);

    resize(1024, 768);

    QSurfaceFormat format;
#ifdef QT_OPENGL_ES_2
    format.setRenderableType(QSurfaceFormat::OpenGLES);
#else
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL) {
        format.setVersion(4, 3);
        format.setProfile(QSurfaceFormat::CoreProfile);
    }
#endif
    format.setDepthBufferSize(24);
    format.setSamples(4);
    format.setStencilBufferSize(8);
    setFormat(format);
    QSurfaceFormat::setDefaultFormat(format);

    d->m_aspectEngine->registerAspect(d->m_renderAspect);
    d->m_aspectEngine->registerAspect(d->m_inputAspect);
    d->m_aspectEngine->registerAspect(d->m_logicAspect);

    d->m_defaultCamera->setParent(d->m_root);
    d->m_forwardRenderer->setCamera(d->m_defaultCamera);
    d->m_forwardRenderer->setSurface(this);
    d->m_renderSettings->setActiveFrameGraph(d->m_forwardRenderer);
    d->m_inputSettings->setEventSource(this);
}

Qt3DWindow::~Qt3DWindow()
{
    Q_D(Qt3DWindow);
    delete d->m_aspectEngine;
}

void Qt3DWindow::registerAspect(Qt3DCore::QAbstractAspect *aspect)
{
    Q_ASSERT(!isVisible());
    Q_D(Qt3DWindow);
    d->m_aspectEngine->registerAspect(aspect);
}

void Qt3DWindow::registerAspect(const QString &name)
{
    Q_ASSERT(!isVisible());
    Q_D(Qt3DWindow);
    d->m_aspectEngine->registerAspect(name);
}

void Qt3DWindow::setRootEntity(Qt3DCore::QEntity *root)
{
    Q_D(Qt3DWindow);
    if (d->m_userRoot != root) {
        if (d->m_userRoot != nullptr)
            d->m_userRoot->setParent(static_cast<Qt3DCore::QNode*>(nullptr));
        if (root != nullptr)
            root->setParent(d->m_root);
        d->m_userRoot = root;
    }
}

void Qt3DWindow::setActiveFrameGraph(Qt3DRender::QFrameGraphNode *activeFrameGraph)
{
    Q_D(Qt3DWindow);
    d->m_renderSettings->setActiveFrameGraph(activeFrameGraph);
}

Qt3DRender::QFrameGraphNode *Qt3DWindow::activeFrameGraph() const
{
    Q_D(const Qt3DWindow);
    return d->m_renderSettings->activeFrameGraph();
}

Qt3DExtras::QForwardRenderer *Qt3DWindow::defaultFrameGraph() const
{
    Q_D(const Qt3DWindow);
    return d->m_forwardRenderer;
}

Qt3DRender::QCamera *Qt3DWindow::camera() const
{
    Q_D(const Qt3DWindow);
    return d->m_defaultCamera;
}

Qt3DRender::QRenderSettings *Qt3DWindow::renderSettings() const
{
    Q_D(const Qt3DWindow);
    return d->m_renderSettings;
}

void Qt3DWindow::showEvent(QShowEvent *e)
{
    Q_D(Qt3DWindow);
    if (!d->m_initialized) {
        d->m_root->addComponent(d->m_renderSettings);
        d->m_root->addComponent(d->m_inputSettings);
        d->m_aspectEngine->setRootEntity(Qt3DCore::QEntityPtr(d->m_root));

        d->m_initialized = true;
    }

    QWindow::showEvent(e);
}

void Qt3DWindow::resizeEvent(QResizeEvent *)
{
    Q_D(Qt3DWindow);
    d->m_defaultCamera->setAspectRatio(float(width()) / float(height()));
}

} // Qt3DExtras

QT_END_NAMESPACE
