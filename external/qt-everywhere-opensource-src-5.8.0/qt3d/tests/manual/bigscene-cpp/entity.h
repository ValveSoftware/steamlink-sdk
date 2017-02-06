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

#ifndef ENTITY_H
#define ENTITY_H

#include <Qt3DCore/QEntity>
#include <QtGui/QColor>
#include <QtGui/QVector3D>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {
class QTransform;
}

namespace Qt3DExtras {
class QCylinderMesh;
class QPhongMaterial;
}

QT_END_NAMESPACE

class Entity : public Qt3DCore::QEntity
{
    Q_OBJECT
    Q_PROPERTY(float theta READ theta WRITE setTheta NOTIFY thetaChanged)
    Q_PROPERTY(float phi READ phi WRITE setPhi NOTIFY phiChanged)
    Q_PROPERTY(QVector3D position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QColor diffuseColor READ diffuseColor WRITE setDiffuseColor NOTIFY diffuseColorChanged)

public:
    Entity(Qt3DCore::QNode *parent = 0);

    float theta() const;
    float phi() const;
    QVector3D position() const;
    QColor diffuseColor() const;

public slots:
    void setTheta(float theta);
    void setPhi(float phi);
    void setPosition(QVector3D position);
    void setDiffuseColor(QColor diffuseColor);

signals:
    void thetaChanged(float theta);
    void phiChanged(float phi);
    void positionChanged(QVector3D position);
    void diffuseColorChanged(QColor diffuseColor);

private:
    void updateTransform();

private:
    Qt3DCore::QTransform *m_transform;
    Qt3DExtras::QCylinderMesh *m_mesh;
    Qt3DExtras::QPhongMaterial *m_material;
    float m_theta;
    float m_phi;
    QVector3D m_position;
};

#endif // ENTITY_H
