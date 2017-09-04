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

#include "boxentity.h"

#include <qmath.h>

BoxEntity::BoxEntity(QNode *parent)
    : Qt3DCore::QEntity(parent)
    , m_transform(new Qt3DCore::QTransform())
    , m_mesh(new Qt3DExtras::QCuboidMesh())
    , m_material(new Qt3DExtras::QPhongMaterial())
    , m_angle(0.0f)
    , m_radius(1.0f)
{
    connect(m_material, SIGNAL(diffuseChanged(const QColor &)),
            this, SIGNAL(diffuseColorChanged(const QColor &)));
    m_material->setAmbient(Qt::gray);
    m_material->setSpecular(Qt::white);
    m_material->setShininess(150.0f);

    addComponent(m_transform);
    addComponent(m_mesh);
    addComponent(m_material);
}

void BoxEntity::setDiffuseColor(const QColor &diffuseColor)
{
    m_material->setDiffuse(diffuseColor);
}

void BoxEntity::setAngle(float arg)
{
    if (m_angle == arg)
        return;

    m_angle = arg;
    emit angleChanged();
    updateTransformation();
}

void BoxEntity::setRadius(float arg)
{
    if (m_radius == arg)
        return;

    m_radius = arg;
    emit radiusChanged();
    updateTransformation();
}

QColor BoxEntity::diffuseColor()
{
    return m_material->diffuse();
}

float BoxEntity::angle() const
{
    return m_angle;
}

float BoxEntity::radius() const
{
    return m_radius;
}

void BoxEntity::updateTransformation()
{
    m_transform->setTranslation(QVector3D(qCos(m_angle) * m_radius,
                                          1.0f,
                                          qSin(m_angle) * m_radius));
}

