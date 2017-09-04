/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include <QGuiApplication>
#include <QPropertyAnimation>

#include <Qt3DInput/QInputAspect>

#include <Qt3DRender/qcamera.h>
#include <Qt3DExtras/qcylindermesh.h>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QMorphPhongMaterial>
#include <Qt3DAnimation/QVertexBlendAnimation>
#include <Qt3DAnimation/QMorphTarget>
#include <Qt3DExtras/QCylinderGeometry>

#include <Qt3DCore/qentity.h>
#include <Qt3DCore/qtransform.h>
#include <Qt3DCore/qaspectengine.h>

#include <Qt3DExtras/qt3dwindow.h>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/qfirstpersoncameracontroller.h>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    Qt3DExtras::Qt3DWindow view;

    // Root entity
    Qt3DCore::QEntity *rootEntity = new Qt3DCore::QEntity();

    // Camera
    Qt3DRender::QCamera *camera = view.camera();
    camera->lens()->setPerspectiveProjection(45.0f, 16.0f/9.0f, 0.1f, 1000.0f);
    camera->setPosition(QVector3D(0, 2, 20.0f));
    camera->setUpVector(QVector3D(0, 1, 0));
    camera->setViewCenter(QVector3D(0, 0, 0));

    // For camera controls
    Qt3DExtras::QFirstPersonCameraController *cameraController
            = new Qt3DExtras::QFirstPersonCameraController(rootEntity);
    cameraController->setCamera(camera);

    view.defaultFrameGraph()->setCamera(camera);
    view.defaultFrameGraph()->setClearColor(Qt::gray);

    // Transform for mesh
    Qt3DCore::QTransform *transform = new Qt3DCore::QTransform;
    transform->setTranslation(QVector3D(0,-1,-1));

    // Base mesh to morph
    Qt3DExtras::QCylinderMesh *mesh = new Qt3DExtras::QCylinderMesh(rootEntity);
    mesh->setRings(6);
    mesh->setSlices(20);

    // create morh targets from geometry
    Qt3DExtras::QCylinderGeometry *cylinder1 = new Qt3DExtras::QCylinderGeometry(rootEntity);
    Qt3DExtras::QCylinderGeometry *cylinder2 = new Qt3DExtras::QCylinderGeometry(rootEntity);
    Qt3DExtras::QCylinderGeometry *cylinder3 = new Qt3DExtras::QCylinderGeometry(rootEntity);

    cylinder1->setRings(6);
    cylinder1->setSlices(20);
    cylinder1->setLength(2.0f);
    cylinder1->setRadius(1.0f);

    cylinder2->setRings(6);
    cylinder2->setSlices(20);
    cylinder2->setLength(1.0f);
    cylinder2->setRadius(5.0f);

    cylinder3->setRings(6);
    cylinder3->setSlices(20);
    cylinder3->setLength(9.0f);
    cylinder3->setRadius(1.0f);

    QStringList attributes;
    attributes.push_back(Qt3DRender::QAttribute::defaultPositionAttributeName());
    attributes.push_back(Qt3DRender::QAttribute::defaultNormalAttributeName());

    QVector<Qt3DAnimation::QMorphTarget*> morphTargets;
    morphTargets.push_back(Qt3DAnimation::QMorphTarget::fromGeometry(cylinder1, attributes));
    morphTargets.push_back(Qt3DAnimation::QMorphTarget::fromGeometry(cylinder2, attributes));
    morphTargets.push_back(Qt3DAnimation::QMorphTarget::fromGeometry(cylinder3, attributes));
    morphTargets.push_back(morphTargets.first());

    Qt3DAnimation::QVertexBlendAnimation *animation = new Qt3DAnimation::QVertexBlendAnimation;
    QVector<float> times;
    times.push_back(0.0f);
    times.push_back(5.0f);
    times.push_back(8.0f);
    times.push_back(12.0f);

    animation->setTargetPositions(times);
    animation->setTarget(mesh);
    animation->setMorphTargets(morphTargets);

    // Material
    Qt3DExtras::QMorphPhongMaterial *material = new Qt3DExtras::QMorphPhongMaterial(rootEntity);
    material->setDiffuse(Qt::red);

    QObject::connect(animation, &Qt3DAnimation::QVertexBlendAnimation::interpolatorChanged,
                     material, &Qt3DExtras::QMorphPhongMaterial::setInterpolator);

    // Cylinder
    Qt3DCore::QEntity *morphingEntity = new Qt3DCore::QEntity(rootEntity);
    morphingEntity->addComponent(mesh);
    morphingEntity->addComponent(transform);
    morphingEntity->addComponent(material);

    // Cylinder shape data
    Qt3DExtras::QCylinderMesh *cylinderMesh = new Qt3DExtras::QCylinderMesh();
    cylinderMesh->setRadius(1);
    cylinderMesh->setLength(3);
    cylinderMesh->setRings(100);
    cylinderMesh->setSlices(20);

    // Transform for cylinder
    Qt3DCore::QTransform *cylinderTransform = new Qt3DCore::QTransform;
    cylinderTransform->setScale(0.5f);
    cylinderTransform->setRotation(QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), 45.0f));

    // Material
    Qt3DExtras::QPhongMaterial *cylinderMaterial = new Qt3DExtras::QPhongMaterial(rootEntity);
    cylinderMaterial->setDiffuse(Qt::red);

    // Cylinder
    Qt3DCore::QEntity *cylinder = new Qt3DCore::QEntity(rootEntity);
    cylinder->addComponent(cylinderMesh);
    cylinder->addComponent(cylinderTransform);
    cylinder->addComponent(cylinderMaterial);

    QPropertyAnimation* anim = new QPropertyAnimation(animation);
    anim->setDuration(5000);
    anim->setEndValue(QVariant::fromValue(12.0f));
    anim->setStartValue(QVariant::fromValue(0.0f));
    anim->setLoopCount(-1);
    anim->setTargetObject(animation);
    anim->setPropertyName("position");
    anim->start();

    // Set root object of the scene
    view.setRootEntity(rootEntity);
    view.show();

    return app.exec();
}
