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

#ifndef BARREL_H
#define BARREL_H

#include <Qt3DExtras/QNormalDiffuseSpecularMapMaterial>
#include <Qt3DRender/qtexture.h>
#include "renderableentity.h"

class Barrel : public RenderableEntity
{
public:
    Barrel(Qt3DCore::QNode *parent = 0);
    ~Barrel();

    enum DiffuseColor {
        Red = 0,
        Blue,
        Green,
        RustDiffuse,
        StainlessSteelDiffuse
    };

    enum SpecularColor {
        RustSpecular = 0,
        StainlessSteelSpecular,
        None
    };

    enum Bumps {
        NoBumps = 0,
        SoftBumps,
        MiddleBumps,
        HardBumps
    };

    void setDiffuse(DiffuseColor diffuse);
    void setSpecular(SpecularColor specular);
    void setBumps(Bumps bumps);
    void setShininess(float shininess);

    DiffuseColor diffuse() const;
    SpecularColor specular() const;
    Bumps bumps() const;
    float shininess() const;

private:
    Bumps m_bumps;
    DiffuseColor m_diffuseColor;
    SpecularColor m_specularColor;
    Qt3DExtras::QNormalDiffuseSpecularMapMaterial *m_material;
    Qt3DRender::QAbstractTexture *m_diffuseTexture;
    Qt3DRender::QAbstractTexture *m_normalTexture;
    Qt3DRender::QAbstractTexture *m_specularTexture;
    Qt3DRender::QTextureImage *m_diffuseTextureImage;
    Qt3DRender::QTextureImage *m_normalTextureImage;
    Qt3DRender::QTextureImage *m_specularTextureImage;

    void setNormalTextureSource();
    void setDiffuseTextureSource();
    void setSpecularTextureSource();

};

#endif // BARREL_H
