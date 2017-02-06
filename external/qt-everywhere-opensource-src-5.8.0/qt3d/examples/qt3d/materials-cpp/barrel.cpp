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

#include "barrel.h"

const char *diffuseColorsName[] = {
    "red",
    "blue",
    "green",
    "rust",
    "stainless_steel"
};

const char *specularColorsName[] = {
    "_rust",
    "_stainless_steel",
    ""
};

const char *bumpsName[] = {
    "no_bumps",
    "soft_bumps",
    "middle_bumps",
    "hard_bumps"
};

Barrel::Barrel(Qt3DCore::QNode *parent)
    : RenderableEntity(parent)
    , m_bumps(NoBumps)
    , m_diffuseColor(Red)
    , m_specularColor(None)
    , m_material(new Qt3DExtras::QNormalDiffuseSpecularMapMaterial())
    , m_diffuseTexture(m_material->diffuse())
    , m_normalTexture(m_material->normal())
    , m_specularTexture(m_material->specular())
    , m_diffuseTextureImage(new Qt3DRender::QTextureImage())
    , m_normalTextureImage(new Qt3DRender::QTextureImage())
    , m_specularTextureImage(new Qt3DRender::QTextureImage())
{
    mesh()->setSource(QUrl(QStringLiteral("qrc:/assets/metalbarrel/metal_barrel.obj")));
    transform()->setScale(0.03f);

    m_diffuseTexture->addTextureImage(m_diffuseTextureImage);
    m_normalTexture->addTextureImage(m_normalTextureImage);
    m_specularTexture->addTextureImage(m_specularTextureImage);

    setNormalTextureSource();
    setDiffuseTextureSource();
    setSpecularTextureSource();
    m_material->setShininess(10.0f);
    addComponent(m_material);
}

Barrel::~Barrel()
{
}

void Barrel::setDiffuse(Barrel::DiffuseColor diffuse)
{
    if (diffuse != m_diffuseColor) {
        m_diffuseColor = diffuse;
        setDiffuseTextureSource();
    }
}

void Barrel::setSpecular(Barrel::SpecularColor specular)
{
    if (specular != m_specularColor) {
        m_specularColor = specular;
        setSpecularTextureSource();
    }
}

void Barrel::setBumps(Barrel::Bumps bumps)
{
    if (bumps != m_bumps) {
        m_bumps = bumps;
        setNormalTextureSource();
    }
}

void Barrel::setShininess(float shininess)
{
    if (shininess != m_material->shininess())
        m_material->setShininess(shininess);
}

Barrel::DiffuseColor Barrel::diffuse() const
{
    return m_diffuseColor;
}

Barrel::SpecularColor Barrel::specular() const
{
    return m_specularColor;
}

Barrel::Bumps Barrel::bumps() const
{
    return m_bumps;
}

float Barrel::shininess() const
{
    return m_material->shininess();
}

void Barrel::setNormalTextureSource()
{
    m_normalTextureImage->setSource(QUrl(QStringLiteral("qrc:/assets/metalbarrel/normal_") + bumpsName[m_bumps] + QStringLiteral(".webp")));
}

void Barrel::setDiffuseTextureSource()
{
    m_diffuseTextureImage->setSource(QUrl(QStringLiteral("qrc:/assets/metalbarrel/diffus_") + diffuseColorsName[m_diffuseColor] + QStringLiteral(".webp")));
}

void Barrel::setSpecularTextureSource()
{
    m_specularTextureImage->setSource(QUrl(QStringLiteral("qrc:/assets/metalbarrel/specular") + specularColorsName[m_specularColor] + QStringLiteral(".webp")));
}
