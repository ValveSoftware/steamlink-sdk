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

#ifndef HOUSEPLANT_H
#define HOUSEPLANT_H

#include "renderableentity.h"
#include <QEntity>
#include <Qt3DExtras/QNormalDiffuseMapAlphaMaterial>
#include <Qt3DExtras/QNormalDiffuseMapMaterial>
#include <QTextureImage>

class HousePlant : public Qt3DCore::QEntity
{
    Q_OBJECT
public:
    explicit HousePlant(Qt3DCore::QNode *parent = 0);
    ~HousePlant();

    enum PotShape {
        Cross = 0,
        Square,
        Triangle,
        Sphere
    };

    enum Plant {
        Bamboo = 0,
        Palm,
        Pine,
        Spikes,
        Shrub
    };

    void setPotShape(PotShape shape);
    void setPlantType(Plant plant);

    PotShape potShape() const;
    Plant plantType() const;

    void setPosition(const QVector3D &pos);
    void setScale(float scale);

    QVector3D position() const;
    float scale() const;

private:
    RenderableEntity *m_pot;
    RenderableEntity *m_plant;
    RenderableEntity *m_cover;

    Qt3DExtras::QNormalDiffuseMapMaterial *m_potMaterial;
    Qt3DExtras::QNormalDiffuseMapAlphaMaterial *m_plantMaterial;
    Qt3DExtras::QNormalDiffuseMapMaterial *m_coverMaterial;

    Qt3DRender::QTextureImage *m_potImage;
    Qt3DRender::QTextureImage *m_potNormalImage;
    Qt3DRender::QTextureImage *m_plantImage;
    Qt3DRender::QTextureImage *m_plantNormalImage;
    Qt3DRender::QTextureImage *m_coverImage;
    Qt3DRender::QTextureImage *m_coverNormalImage;

    Plant m_plantType;
    PotShape m_potShape;

    void updatePotShape();
    void updatePlantType();
};

#endif // HOUSEPLANT_H
