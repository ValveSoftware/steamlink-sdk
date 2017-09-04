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

#include "scene.h"

#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>

#include <QtGui/QPainter>
#include <Qt3DRender/QPaintedTextureImage>
#include <Qt3DRender/QTexture>

#include <Qt3DExtras/QCuboidMesh>
#include <Qt3DExtras/QDiffuseMapMaterial>

class MyTextureImage : public Qt3DRender::QPaintedTextureImage
{
public:
    MyTextureImage(int w, int h)
    {
        setSize(QSize(w, h));
    }

protected:
    void paint(QPainter *painter)
    {
        int w = painter->device()->width();
        int h = painter->device()->height();

        // clear to white
        painter->fillRect(0, 0, w, h, QColor(255, 255, 255));

        // draw some text
        painter->setPen(QColor(255, 0, 0));
        painter->setFont(QFont("Times", 30, QFont::Normal, true));
        painter->drawText(w / 8, h / 6, QDateTime::currentDateTime().toString());

        painter->setPen(QPen(QBrush(QColor(0, 0, 0)) ,10));
        painter->setBrush(QColor(0, 0, 255));

        // draw some circles
        for (int i = 0; i < m_circleCount; i++) {
            float r = w / 8.f;
            float cw =  0.25 * w +   0.5 * h * (i % 2);
            float ch = 0.375 * w + 0.375 * h * (i / 2);
            painter->drawEllipse(QPointF(cw, ch), r, r);
        }

        m_circleCount = (m_circleCount % 4) + 1;
    }

    int m_circleCount = 1;
};

Scene::Scene(Qt3DCore::QEntity *rootEntity)
    : m_rootEntity(rootEntity)
{
    // Cuboid shape data
    Qt3DExtras::QCuboidMesh *cuboid = new Qt3DExtras::QCuboidMesh();

    // CuboidMesh Transform
    m_transform = new Qt3DCore::QTransform();
    m_transform->setScale(4.0f);;

    // create texture
    m_paintedTextureImage = new MyTextureImage(128, 128);
    m_paintedTextureImage->update();

    Qt3DRender::QTexture2D *tex = new Qt3DRender::QTexture2D();
    tex->addTextureImage(m_paintedTextureImage);

    Qt3DExtras::QDiffuseMapMaterial *mat = new Qt3DExtras::QDiffuseMapMaterial();
    mat->setAmbient(QColor(64, 64, 64));
    mat->setSpecular(QColor(255, 255, 255));
    mat->setDiffuse(tex);

    //Cuboid
    m_cuboidEntity = new Qt3DCore::QEntity(m_rootEntity);
    m_cuboidEntity->addComponent(cuboid);
    m_cuboidEntity->addComponent(mat);
    m_cuboidEntity->addComponent(m_transform);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &Scene::updateTimer);
    timer->start(50);
}

Scene::~Scene()
{
}

void Scene::redrawTexture()
{
    m_paintedTextureImage->update();
}

void Scene::setSize(QSize size)
{
    m_paintedTextureImage->setSize(size);
}

void Scene::updateTimer()
{
    m_transform->setRotation(QQuaternion::fromAxisAndAngle(0, 1, 0, m_angle));
    m_angle += 0.4f;
}
