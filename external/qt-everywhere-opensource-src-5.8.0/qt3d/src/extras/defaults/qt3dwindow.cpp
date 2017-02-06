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

#include <Qt3DExtras/qforwardrenderer.h>
#include <Qt3DRender/qrendersettings.h>
#include <Qt3DRender/qrenderaspect.h>
#include <Qt3DInput/qinputaspect.h>
#include <Qt3DInput/qinputsettings.h>
#include <Qt3DLogic/qlogicaspect.h>

#include <Qt3DCore/qaspectengine.h>
#include <Qt3DRender/qcamera.h>
#include <Qt3DCore/qentity.h>

#include <QtGui/qopenglcontext.h>

QT_BEGIN_NAMESPACE

namespace Qt3DExtras {

Qt3DWindow::Qt3DWindow(QScreen *screen)
    : QWindow(screen)
    , m_aspectEngine(new Qt3DCore::QAspectEngine)
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
    create();

    m_aspectEngine->registerAspect(m_renderAspect);
    m_aspectEngine->registerAspect(m_inputAspect);
    m_aspectEngine->registerAspect(m_logicAspect);

    m_defaultCamera->setParent(m_root);
    m_forwardRenderer->setCamera(m_defaultCamera);
    m_forwardRenderer->setSurface(this);
    m_renderSettings->setActiveFrameGraph(m_forwardRenderer);
    m_inputSettings->setEventSource(this);
}

Qt3DWindow::~Qt3DWindow()
{
}

void Qt3DWindow::registerAspect(Qt3DCore::QAbstractAspect *aspect)
{
    Q_ASSERT(!isVisible());
    m_aspectEngine->registerAspect(aspect);
}

void Qt3DWindow::registerAspect(const QString &name)
{
    Q_ASSERT(!isVisible());
    m_aspectEngine->registerAspect(name);
}

void Qt3DWindow::setRootEntity(Qt3DCore::QEntity *root)
{
    Q_ASSERT(!isVisible());
    m_userRoot = root;
}

void Qt3DWindow::setActiveFrameGraph(Qt3DRender::QFrameGraphNode *activeFrameGraph)
{
    m_renderSettings->setActiveFrameGraph(activeFrameGraph);
}

Qt3DRender::QFrameGraphNode *Qt3DWindow::activeFrameGraph() const
{
    return m_renderSettings->activeFrameGraph();
}

Qt3DExtras::QForwardRenderer *Qt3DWindow::defaultFrameGraph() const
{
    return m_forwardRenderer;
}

Qt3DRender::QCamera *Qt3DWindow::camera() const
{
    return m_defaultCamera;
}

void Qt3DWindow::showEvent(QShowEvent *e)
{
    if (!m_initialized) {
        if (m_userRoot != nullptr)
            m_userRoot->setParent(m_root);

        m_root->addComponent(m_renderSettings);
        m_root->addComponent(m_inputSettings);
        m_aspectEngine->setRootEntity(Qt3DCore::QEntityPtr(m_root));

        m_initialized = true;
    }

    QWindow::showEvent(e);
}

void Qt3DWindow::resizeEvent(QResizeEvent *)
{
    m_defaultCamera->setAspectRatio(float(width()) / float(height()));
}

} // Qt3DExtras

QT_END_NAMESPACE
