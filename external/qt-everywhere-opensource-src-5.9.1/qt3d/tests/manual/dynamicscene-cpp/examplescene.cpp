/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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

#include "examplescene.h"
#include "boxentity.h"

#include <QTimer>
#include <qmath.h>

ExampleScene::ExampleScene(Qt3DCore::QNode *parent)
    : Qt3DCore::QEntity(parent)
    , m_timer(new QTimer(this))
    , m_even(true)
{
    buildScene();
    QObject::connect(m_timer, SIGNAL(timeout()), SLOT(updateScene()));
    m_timer->setInterval(1200);
    m_timer->start();
}

ExampleScene::~ExampleScene()
{
    qDeleteAll(m_entities);
}

void ExampleScene::updateScene()
{
    for (int i = 0; i < m_entities.size(); ++i) {
        const bool visible = (i % 2) ^ static_cast<int>(m_even);
        m_entities[i]->setParent(visible ? this : nullptr);
    }
    m_even = !m_even;
}

void ExampleScene::buildScene()
{
    int count = 20;
    const float radius = 5.0f;

    for (int i = 0; i < count; ++i) {
        BoxEntity *entity = new BoxEntity;
        const float angle = M_PI * 2.0f * float(i) / count;
        entity->setAngle(angle);
        entity->setRadius(radius);
        entity->setDiffuseColor(QColor(qFabs(qCos(angle)) * 255, 204, 75));
        m_entities.append(entity);
    }
}

