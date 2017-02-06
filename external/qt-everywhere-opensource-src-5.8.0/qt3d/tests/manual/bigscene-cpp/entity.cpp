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

#include "entity.h"

#include <Qt3DExtras/QCylinderMesh>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DCore/QTransform>
#include <QMatrix4x4>

Entity::Entity(Qt3DCore::QNode *parent)
    : QEntity(parent)
    , m_transform(new Qt3DCore::QTransform())
    , m_mesh(new Qt3DExtras::QCylinderMesh())
    , m_material(new Qt3DExtras::QPhongMaterial())
{
    m_mesh->setRings(50.0f);
    m_mesh->setSlices(30.0f);
    m_mesh->setRadius(2.5f);
    m_mesh->setLength(5.0f);

    addComponent(m_mesh);
    addComponent(m_transform);
    addComponent(m_material);
}

void Entity::updateTransform()
{
    QMatrix4x4 m;
    m.translate(m_position);
    m.rotate(m_phi, QVector3D(1.0f, 0.0f, 0.0f));
    m.rotate(m_theta, QVector3D(0.0f, 0.0f, 1.0f));
    m_transform->setMatrix(m);
}

float Entity::theta() const
{
    return m_theta;
}

float Entity::phi() const
{
    return m_phi;
}

QVector3D Entity::position() const
{
    return m_position;
}

QColor Entity::diffuseColor() const
{
    return m_material->diffuse();
}

void Entity::setTheta(float theta)
{
    if (m_theta == theta)
        return;

    m_theta = theta;
    emit thetaChanged(theta);
    updateTransform();
}

void Entity::setPhi(float phi)
{
    if (m_phi == phi)
        return;

    m_phi = phi;
    emit phiChanged(phi);
    updateTransform();
}

void Entity::setPosition(QVector3D position)
{
    if (m_position == position)
        return;

    m_position = position;
    emit positionChanged(position);
    updateTransform();
}

void Entity::setDiffuseColor(QColor diffuseColor)
{
    if (m_material->diffuse() == diffuseColor)
        return;

    m_material->setDiffuse(diffuseColor);
    emit diffuseColorChanged(diffuseColor);
}
