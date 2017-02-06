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

#include "houseplant.h"
#include <Qt3DRender/qtexture.h>

const char *potNames[] = {
    "cross",
    "square",
    "triangle",
    "sphere"
};

const char *plantNames[] = {
    "bamboo",
    "palm",
    "pine",
    "spikes",
    "shrub"
};


HousePlant::HousePlant(Qt3DCore::QNode *parent)
    : Qt3DCore::QEntity(parent)
    , m_pot(new RenderableEntity(this))
    , m_plant(new RenderableEntity(m_pot))
    , m_cover(new RenderableEntity(m_pot))
    , m_potMaterial(new Qt3DExtras::QNormalDiffuseMapMaterial())
    , m_plantMaterial(new Qt3DExtras::QNormalDiffuseMapAlphaMaterial())
    , m_coverMaterial(new Qt3DExtras::QNormalDiffuseMapMaterial())
    , m_potImage(new Qt3DRender::QTextureImage())
    , m_potNormalImage(new Qt3DRender::QTextureImage())
    , m_plantImage(new Qt3DRender::QTextureImage())
    , m_plantNormalImage(new Qt3DRender::QTextureImage())
    , m_coverImage(new Qt3DRender::QTextureImage())
    , m_coverNormalImage(new Qt3DRender::QTextureImage())
    , m_plantType(Bamboo)
    , m_potShape(Cross)
{
    m_pot->transform()->setScale(0.03f);
    m_pot->addComponent(m_potMaterial);
    m_plant->addComponent(m_plantMaterial);
    m_cover->addComponent(m_coverMaterial);

    m_potMaterial->diffuse()->addTextureImage(m_potImage);
    m_potMaterial->normal()->addTextureImage(m_potNormalImage);
    m_plantMaterial->diffuse()->addTextureImage(m_plantImage);
    m_plantMaterial->normal()->addTextureImage(m_plantNormalImage);
    m_coverMaterial->diffuse()->addTextureImage(m_coverImage);
    m_coverMaterial->normal()->addTextureImage(m_coverNormalImage);

    updatePlantType();
    updatePotShape();

    m_coverImage->setSource(QUrl(QStringLiteral("qrc:/assets/houseplants/cover.webp")));
    m_coverNormalImage->setSource(QUrl(QStringLiteral("qrc:/assets/houseplants/cover_normal.webp")));
    m_potImage->setSource(QUrl(QStringLiteral("qrc:/assets/houseplants/pot.webp")));
    m_potNormalImage->setSource(QUrl(QStringLiteral("qrc:/assets/houseplants/pot_normal.webp")));

    m_potMaterial->setShininess(10.0f);
    m_potMaterial->setSpecular(QColor::fromRgbF(0.75f, 0.75f, 0.75f, 1.0f));

    m_plantMaterial->setShininess(10.0f);

    m_coverMaterial->setSpecular(QColor::fromRgbF(0.05f, 0.05f, 0.05f, 1.0f));
    m_coverMaterial->setShininess(5.0f);
}

HousePlant::~HousePlant()
{
}

void HousePlant::setPotShape(HousePlant::PotShape shape)
{
    if (shape != m_potShape) {
        m_potShape = shape;
        updatePotShape();
    }
}

void HousePlant::setPlantType(HousePlant::Plant plant)
{
    if (plant != m_plantType) {
        m_plantType = plant;
        updatePlantType();
    }
}

HousePlant::PotShape HousePlant::potShape() const
{
    return m_potShape;
}

HousePlant::Plant HousePlant::plantType() const
{
    return m_plantType;
}

void HousePlant::setPosition(const QVector3D &pos)
{
    m_pot->transform()->setTranslation(pos);
}

void HousePlant::setScale(float scale)
{
    m_pot->transform()->setScale(scale);
}

QVector3D HousePlant::position() const
{
    return m_pot->transform()->translation();
}

float HousePlant::scale() const
{
    return m_pot->transform()->scale();
}

void HousePlant::updatePotShape()
{
    m_pot->mesh()->setSource(QUrl(QStringLiteral("qrc:/assets/houseplants/") + potNames[m_potShape] + QStringLiteral("-pot.obj")));
    m_plant->mesh()->setSource(QUrl(QStringLiteral("qrc:/assets/houseplants/") + potNames[m_potShape] + QStringLiteral("-") + plantNames[m_plantType] + QStringLiteral(".obj")));
}

void HousePlant::updatePlantType()
{
    m_plant->mesh()->setSource(QUrl(QStringLiteral("qrc:/assets/houseplants/") + potNames[m_potShape] + QStringLiteral("-") + plantNames[m_plantType] + QStringLiteral(".obj")));

    m_plantImage->setSource(QUrl(QStringLiteral("qrc:/assets/houseplants/") + plantNames[m_plantType] + QStringLiteral(".webp")));
    m_plantNormalImage->setSource(QUrl(QStringLiteral("qrc:/assets/houseplants/") + plantNames[m_plantType] + QStringLiteral("_normal.webp")));
}

