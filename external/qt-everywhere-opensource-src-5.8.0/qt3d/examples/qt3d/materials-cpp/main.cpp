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

#include <QGuiApplication>

#include <Qt3DCore/QEntity>
#include <Qt3DCore/QAspectEngine>
#include <Qt3DRender/QCamera>

#include <Qt3DInput/QInputAspect>

#include <Qt3DRender/QRenderAspect>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QDiffuseMapMaterial>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DRender/QTextureImage>

#include "planeentity.h"
#include "rotatingtrefoilknot.h"
#include "barrel.h"
#include "houseplant.h"
#include <Qt3DExtras/qt3dwindow.h>
#include <Qt3DExtras/qfirstpersoncameracontroller.h>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    Qt3DExtras::Qt3DWindow view;

    // Scene Root
    Qt3DCore::QEntity *sceneRoot = new Qt3DCore::QEntity();

    // Scene Camera
    Qt3DRender::QCamera *basicCamera = view.camera();
    basicCamera->setProjectionType(Qt3DRender::QCameraLens::PerspectiveProjection);
    basicCamera->setAspectRatio(view.width() / view.height());
    basicCamera->setUpVector(QVector3D(0.0f, 1.0f, 0.0f));
    basicCamera->setViewCenter(QVector3D(0.0f, 3.5f, 0.0f));
    basicCamera->setPosition(QVector3D(0.0f, 3.5f, 25.0f));
    // For camera controls
    Qt3DExtras::QFirstPersonCameraController *camController = new Qt3DExtras::QFirstPersonCameraController(sceneRoot);
    camController->setCamera(basicCamera);

    // Scene floor
    PlaneEntity *planeEntity = new PlaneEntity(sceneRoot);
    planeEntity->mesh()->setHeight(100.0f);
    planeEntity->mesh()->setWidth(100.0f);
    planeEntity->mesh()->setMeshResolution(QSize(20, 20));

    Qt3DExtras::QNormalDiffuseSpecularMapMaterial *normalDiffuseSpecularMapMaterial = new Qt3DExtras::QNormalDiffuseSpecularMapMaterial();
    normalDiffuseSpecularMapMaterial->setTextureScale(10.0f);
    normalDiffuseSpecularMapMaterial->setShininess(80.0f);
    normalDiffuseSpecularMapMaterial->setAmbient(QColor::fromRgbF(0.2f, 0.2f, 0.2f, 1.0f));

    Qt3DRender::QTextureImage *diffuseImage = new Qt3DRender::QTextureImage();
    diffuseImage->setSource(QUrl(QStringLiteral("qrc:/assets/textures/pattern_09/diffuse.webp")));
    normalDiffuseSpecularMapMaterial->diffuse()->addTextureImage(diffuseImage);

    Qt3DRender::QTextureImage *specularImage = new Qt3DRender::QTextureImage();
    specularImage->setSource(QUrl(QStringLiteral("qrc:/assets/textures/pattern_09/specular.webp")));
    normalDiffuseSpecularMapMaterial->specular()->addTextureImage(specularImage);

    Qt3DRender::QTextureImage *normalImage = new Qt3DRender::QTextureImage();
    normalImage->setSource(QUrl((QStringLiteral("qrc:/assets/textures/pattern_09/normal.webp"))));
    normalDiffuseSpecularMapMaterial->normal()->addTextureImage(normalImage);

    planeEntity->addComponent(normalDiffuseSpecularMapMaterial);

    // Chest
    RenderableEntity *chest = new RenderableEntity(sceneRoot);
    chest->transform()->setScale(0.03f);
    chest->mesh()->setSource(QUrl(QStringLiteral("qrc:/assets/chest/Chest.obj")));
    Qt3DExtras::QDiffuseMapMaterial *diffuseMapMaterial = new Qt3DExtras::QDiffuseMapMaterial();
    diffuseMapMaterial->setSpecular(QColor::fromRgbF(0.2f, 0.2f, 0.2f, 1.0f));
    diffuseMapMaterial->setShininess(2.0f);

    Qt3DRender::QTextureImage *chestDiffuseImage = new Qt3DRender::QTextureImage();
    chestDiffuseImage->setSource(QUrl(QStringLiteral("qrc:/assets/chest/diffuse.webp")));
    diffuseMapMaterial->diffuse()->addTextureImage(chestDiffuseImage);

    chest->addComponent(diffuseMapMaterial);


    // TrefoilKnot
    RotatingTrefoilKnot *trefoil = new RotatingTrefoilKnot(sceneRoot);
    trefoil->setPosition(QVector3D(0.0f, 3.5f, 0.0f));
    trefoil->setScale(0.5f);
    Qt3DExtras::QPhongMaterial *phongMaterial = new Qt3DExtras::QPhongMaterial();
    phongMaterial->setDiffuse(QColor(204, 205, 75)); // Safari Yellow #cccd4b
    phongMaterial->setSpecular(Qt::white);
    trefoil->addComponent(phongMaterial);

    // Barrels
    Barrel *basicBarrel = new Barrel(sceneRoot);
    basicBarrel->transform()->setTranslation(QVector3D(8.0f, 0.0f, 0.0f));

    Barrel *rustyBarrel = new Barrel(sceneRoot);
    rustyBarrel->setDiffuse(Barrel::RustDiffuse);
    rustyBarrel->setSpecular(Barrel::RustSpecular);
    rustyBarrel->setBumps(Barrel::HardBumps);
    rustyBarrel->transform()->setTranslation(QVector3D(10.0f, 0.0f, 0.0f));

    Barrel *blueBarrel = new Barrel(sceneRoot);
    blueBarrel->setDiffuse(Barrel::Blue);
    blueBarrel->setBumps(Barrel::MiddleBumps);
    blueBarrel->transform()->setTranslation(QVector3D(12.0f, 0.0f, 0.0f));

    Barrel *greenBarrel = new Barrel(sceneRoot);
    greenBarrel->setDiffuse(Barrel::Green);
    greenBarrel->setBumps(Barrel::SoftBumps);
    greenBarrel->transform()->setTranslation(QVector3D(14.0f, 0.0f, 0.0f));

    Barrel *stainlessBarrel = new Barrel(sceneRoot);
    stainlessBarrel->setDiffuse(Barrel::StainlessSteelDiffuse);
    stainlessBarrel->setBumps(Barrel::NoBumps);
    stainlessBarrel->setSpecular(Barrel::StainlessSteelSpecular);
    stainlessBarrel->setShininess(150.0f);
    stainlessBarrel->transform()->setTranslation(QVector3D(16.0f, 0.0f, 0.0f));

    // Plants
    HousePlant *squareBamboo = new HousePlant(sceneRoot);
    squareBamboo->setPotShape(HousePlant::Square);
    squareBamboo->setPosition(QVector3D(4.0f, 0.0f, 0.0f));

    HousePlant *trianglePalm = new HousePlant(sceneRoot);
    trianglePalm->setPlantType(HousePlant::Palm);
    trianglePalm->setPotShape(HousePlant::Triangle);
    trianglePalm->setPosition(QVector3D(0.0f, 0.0f, 4.0f));

    HousePlant *spherePine = new HousePlant(sceneRoot);
    spherePine->setPlantType(HousePlant::Pine);
    spherePine->setPotShape(HousePlant::Sphere);
    spherePine->setPosition(QVector3D(-4.0f, 0.0f, 0.0f));

    HousePlant *crossSpikes = new HousePlant(sceneRoot);
    crossSpikes->setPlantType(HousePlant::Spikes);
    crossSpikes->setPosition(QVector3D(0.0f, 0.0f, -4.0f));

    HousePlant *crossPalm = new HousePlant(sceneRoot);
    crossPalm->setPlantType(HousePlant::Palm);
    crossPalm->setPosition(QVector3D(0.0f, 0.0f, -8.0f));
    crossPalm->setScale(0.05f);

    HousePlant *crossShrub = new HousePlant(sceneRoot);
    crossShrub->setPlantType(HousePlant::Shrub);
    crossShrub->setPosition(QVector3D(0.0f, 0.0f, 8.0f));
    crossShrub->setScale(0.05f);

    view.setRootEntity(sceneRoot);
    view.show();

    return app.exec();
}
