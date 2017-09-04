/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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

#include <QCoreApplication>
#include <Qt3DCore/Qt3DCore>
#include <Qt3DExtras/Qt3DExtras>
#include <Qt3DExtras/QExtrudedTextMesh>

int main(int argc, char *argv[])
{
    QGuiApplication a(argc, argv);
    Qt3DExtras::Qt3DWindow *view = new Qt3DExtras::Qt3DWindow();
    view->setTitle(QStringLiteral("3D Text CPP"));
    view->defaultFrameGraph()->setClearColor(QColor(210, 210, 220));

    auto *root = new Qt3DCore::QEntity();

    { // plane
        auto *plane = new Qt3DCore::QEntity(root);
        auto *planeMesh = new Qt3DExtras::QPlaneMesh();
        auto *planeTransform = new Qt3DCore::QTransform();
        auto *planeMaterial = new Qt3DExtras::QPhongMaterial(root);
        planeMesh->setWidth(10); planeMesh->setHeight(10);
        planeTransform->setTranslation(QVector3D(0, 0, 0));
        planeMaterial->setDiffuse(QColor(150, 150, 150));

        plane->addComponent(planeMaterial);
        plane->addComponent(planeMesh);
        plane->addComponent(planeTransform);
    }

    auto *textMaterial = new Qt3DExtras::QPhongMaterial(root);
    { // text
        int i = 0;
        QStringList fonts = QFontDatabase().families();

        for (QString family : fonts)
        {
            auto *text = new Qt3DCore::QEntity(root);
            auto *textMesh = new Qt3DExtras::QExtrudedTextMesh();

            auto *textTransform = new Qt3DCore::QTransform();
            QFont font(family, 32, -1, false);
            textTransform->setTranslation(QVector3D(-2.45f, i * .5f, 0));
            textTransform->setScale(.2f);
            textMesh->setDepth(.45f);
            textMesh->setFont(font);
            textMesh->setText(QString(family));
            textMaterial->setDiffuse(QColor(111, 150, 255));

            text->addComponent(textMaterial);
            text->addComponent(textMesh);
            text->addComponent(textTransform);

            i++;
        }
    }

    { // camera
        float aspect = static_cast<float>(view->screen()->size().width()) / view->screen()->size().height();
        Qt3DRender::QCamera *camera = view->camera();
        camera->lens()->setPerspectiveProjection(65.f, aspect, 0.1f, 100.f);
        camera->setPosition(QVector3D(0, 5, 3));
        camera->setViewCenter(QVector3D(0, 5, 0));

        auto *cameraController = new Qt3DExtras::QOrbitCameraController(root);
        cameraController->setCamera(camera);
    }

    view->setRootEntity(root);
    view->show();

    return a.exec();
}
