/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
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

#include <Qt3DCore/QEntity>

#include <Qt3DRender/QMaterial>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DExtras/QPlaneMesh>
#include <Qt3DRender/QLayer>
#include <Qt3DRender/QParameter>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QCameraLens>
#include <Qt3DCore/QTransform>
#include <Qt3DRender/QPointLight>
#include <Qt3DCore/qaspectengine.h>

#include <QGuiApplication>

#include "gbuffer.h"
#include "deferredrenderer.h"
#include "finaleffect.h"
#include "sceneeffect.h"
#include "pointlightblock.h"
#include "sceneentity.h"
#include "screenquadentity.h"

#include <Qt3DExtras/qt3dwindow.h>
#include <Qt3DExtras/qfirstpersoncameracontroller.h>

int main(int ac, char **av)
{
    QGuiApplication app(ac, av);

    Qt3DExtras::Qt3DWindow view;

    // Root entity
    Qt3DCore::QEntity *rootEntity = new Qt3DCore::QEntity();

    // Scene Content
    SceneEntity *sceneContent = new SceneEntity(rootEntity);

    // Screen Quad
    ScreenQuadEntity *screenQuad = new ScreenQuadEntity(rootEntity);

    // Shared Components
    Qt3DRender::QLayer *sceneLayer = sceneContent->layer();
    Qt3DRender::QLayer *quadLayer = screenQuad->layer();
    SceneEffect *sceneEffect = sceneContent->effect();
    FinalEffect *finalEffect = screenQuad->effect();

    // Scene Camera
    Qt3DRender::QCamera *camera = view.camera();
    camera->setFieldOfView(45.0f);
    camera->setNearPlane(0.01f);
    camera->setFarPlane(1000.0f);
    camera->setProjectionType(Qt3DRender::QCameraLens::PerspectiveProjection);
    camera->setPosition(QVector3D(10.0f, 10.0f, -25.0f));
    camera->setUpVector(QVector3D(0.0f, 1.0f, 0.0f));
    camera->setViewCenter(QVector3D(0.0f, 0.0f, 10.0f));

    // For camera controls
    Qt3DExtras::QFirstPersonCameraController *camController = new Qt3DExtras::QFirstPersonCameraController(rootEntity);
    camController->setCamera(camera);

    // FrameGraph
    DeferredRenderer *deferredRenderer = new DeferredRenderer();
    deferredRenderer->setNormalizedRect(QRectF(0.0f, 0.0f, 1.0f, 1.0f));
    deferredRenderer->setFinalPassCriteria(finalEffect->passCriteria());
    deferredRenderer->setGeometryPassCriteria(sceneEffect->passCriteria());
    deferredRenderer->setSceneCamera(camera);
    deferredRenderer->setGBufferLayer(sceneLayer);
    deferredRenderer->setScreenQuadLayer(quadLayer);
    deferredRenderer->setSurface(&view);

    view.setActiveFrameGraph(deferredRenderer);

    // Set root object of the scene
    view.setRootEntity(rootEntity);
    // Show window
    view.show();

    return app.exec();
}
