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

#include "entity.h"

#include <QGuiApplication>

#include <QPropertyAnimation>
#include <QUrl>
#include <QTimer>
#include <Qt3DCore/QEntity>
#include <Qt3DRender/QCamera>
#include <Qt3DCore/QTransform>
#include <Qt3DCore/qaspectengine.h>
#include <Qt3DInput/QInputAspect>
#include <Qt3DRender/QParameter>
#include <Qt3DExtras/QCylinderMesh>
#include <Qt3DRender/QRenderAspect>
#include <Qt3DRender/QCameraSelector>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QForwardRenderer>
#include <qmath.h>
#include <Qt3DExtras/qt3dwindow.h>
#include <Qt3DExtras/qfirstpersoncameracontroller.h>

using namespace Qt3DCore;
using namespace Qt3DRender;

int main(int ac, char **av)
{
    QGuiApplication app(ac, av);
    Qt3DExtras::Qt3DWindow view;
    view.defaultFrameGraph()->setClearColor(Qt::black);

    QEntity *root = new QEntity();

    // Camera
    QCamera *cameraEntity = view.camera();
    cameraEntity->lens()->setPerspectiveProjection(45.0f, 16.0f/9.0f, 0.1f, 1000.0f);
    cameraEntity->setPosition(QVector3D(0, 250.0f, 50.0f));
    cameraEntity->setUpVector(QVector3D(0, 1, 0));
    cameraEntity->setViewCenter(QVector3D(0, 0, 0));

    // For camera controls
    Qt3DExtras::QFirstPersonCameraController *camController = new Qt3DExtras::QFirstPersonCameraController(root);
    camController->setCamera(cameraEntity);

    const float radius = 100.0f;
    const int max = 1000;
    const float det = 1.0f / max;

    // Scene
    for (int i = 0; i < max; i++) {
        Entity *e = new Entity();
        const float angle = M_PI * 2.0f * i * det * 10.;

        e->setDiffuseColor(QColor(qFabs(qCos(angle)) * 255, 204, 75));
        e->setPosition(QVector3D(radius * qCos(angle), -200.* i * det, radius * qSin(angle)));
        e->setTheta(30.0f * i);
        e->setPhi(45.0f * i);

        QPropertyAnimation *animX = new QPropertyAnimation(e, QByteArrayLiteral("theta"));
        animX->setDuration(2400 * (i + 1));
        animX->setStartValue(QVariant::fromValue(i * 30.0f));
        animX->setEndValue(QVariant::fromValue((i + 1) * 390.0f));
        animX->setLoopCount(-1);
        animX->start();

        QPropertyAnimation *animZ = new QPropertyAnimation(e, QByteArrayLiteral("phi"));
        animZ->setDuration(2400 * (i + 1));
        animZ->setStartValue(QVariant::fromValue(i * 20.0f));
        animZ->setEndValue(QVariant::fromValue((i + 1) * 380.0f));
        animZ->setLoopCount(-1);
        animZ->start();

        e->setParent(root);
    }

    view.setRootEntity(root);
    view.show();

    if (app.arguments().contains(("--bench")))
        QTimer::singleShot(25 * 1000, &app, &QCoreApplication::quit);

    return app.exec();
}
