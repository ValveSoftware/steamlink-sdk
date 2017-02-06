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

#include "trefoilknot.h"

TrefoilKnot::TrefoilKnot(Qt3DCore::QNode *parent)
    : Qt3DCore::QEntity(parent)
    , m_mesh(new Qt3DRender::QMesh())
    , m_transform(new Qt3DCore::QTransform())
    , m_position()
    , m_scale(1.0f)
{
    m_mesh->setSource(QUrl("qrc:/assets/obj/trefoil.obj"));
    addComponent(m_mesh);
    addComponent(m_transform);
}

TrefoilKnot::~TrefoilKnot()
{
}

void TrefoilKnot::updateTransform()
{
    QMatrix4x4 m;
    m.translate(m_position);
    m.rotate(m_phi, QVector3D(1.0f, 0.0f, 0.0f));
    m.rotate(m_phi, QVector3D(0.0f, 1.0f, 0.0f));
    m.scale(m_scale);
    m_transform->setMatrix(m);
}

float TrefoilKnot::theta() const
{
    return m_theta;
}

float TrefoilKnot::phi() const
{
    return m_phi;
}

QVector3D TrefoilKnot::position() const
{
    return m_position;
}

float TrefoilKnot::scale() const
{
    return m_scale;
}

void TrefoilKnot::setTheta(float theta)
{
    if (m_theta == theta)
        return;

    m_theta = theta;
    emit thetaChanged(theta);
    updateTransform();
}

void TrefoilKnot::setPhi(float phi)
{
    if (m_phi == phi)
        return;

    m_phi = phi;
    emit phiChanged(phi);
    updateTransform();
}

void TrefoilKnot::setPosition(QVector3D position)
{
    if (m_position == position)
        return;

    m_position = position;
    emit positionChanged(position);
    updateTransform();
}

void TrefoilKnot::setScale(float scale)
{
    if (m_scale == scale)
        return;

    m_scale = scale;
    emit scaleChanged(scale);
    updateTransform();
}
