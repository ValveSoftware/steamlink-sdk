/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qsgdefaultinternalimagenode_p.h"
#include <private/qsgmaterialshader_p.h>
#include <private/qsgtexturematerial_p.h>
#include <QtGui/qopenglfunctions.h>
#include <QtCore/qmath.h>

QT_BEGIN_NAMESPACE

class SmoothTextureMaterialShader : public QSGTextureMaterialShader
{
public:
    SmoothTextureMaterialShader();

    virtual void updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect);
    virtual char const *const *attributeNames() const;

protected:
    virtual void initialize();

    int m_pixelSizeLoc;
};


QSGSmoothTextureMaterial::QSGSmoothTextureMaterial()
{
    setFlag(RequiresFullMatrixExceptTranslate, true);
    setFlag(Blending, true);
}

void QSGSmoothTextureMaterial::setTexture(QSGTexture *texture)
{
    m_texture = texture;
}

QSGMaterialType *QSGSmoothTextureMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

QSGMaterialShader *QSGSmoothTextureMaterial::createShader() const
{
    return new SmoothTextureMaterialShader;
}

SmoothTextureMaterialShader::SmoothTextureMaterialShader()
    : QSGTextureMaterialShader()
{
    setShaderSourceFile(QOpenGLShader::Vertex, QStringLiteral(":/qt-project.org/scenegraph/shaders/smoothtexture.vert"));
    setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/qt-project.org/scenegraph/shaders/smoothtexture.frag"));
}

void SmoothTextureMaterialShader::updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect)
{
    if (oldEffect == 0) {
        // The viewport is constant, so set the pixel size uniform only once.
        QRect r = state.viewportRect();
        program()->setUniformValue(m_pixelSizeLoc, 2.0f / r.width(), 2.0f / r.height());
    }
    QSGTextureMaterialShader::updateState(state, newEffect, oldEffect);
}

char const *const *SmoothTextureMaterialShader::attributeNames() const
{
    static char const *const attributes[] = {
        "vertex",
        "multiTexCoord",
        "vertexOffset",
        "texCoordOffset",
        0
    };
    return attributes;
}

void SmoothTextureMaterialShader::initialize()
{
    m_pixelSizeLoc = program()->uniformLocation("pixelSize");
    QSGTextureMaterialShader::initialize();
}

QSGDefaultInternalImageNode::QSGDefaultInternalImageNode()
{
    setMaterial(&m_materialO);
    setOpaqueMaterial(&m_material);
}

void QSGDefaultInternalImageNode::setFiltering(QSGTexture::Filtering filtering)
{
    if (m_material.filtering() == filtering)
        return;

    m_material.setFiltering(filtering);
    m_materialO.setFiltering(filtering);
    m_smoothMaterial.setFiltering(filtering);
    markDirty(DirtyMaterial);
}

void QSGDefaultInternalImageNode::setMipmapFiltering(QSGTexture::Filtering filtering)
{
    if (m_material.mipmapFiltering() == filtering)
        return;

    m_material.setMipmapFiltering(filtering);
    m_materialO.setMipmapFiltering(filtering);
    m_smoothMaterial.setMipmapFiltering(filtering);
    markDirty(DirtyMaterial);
}

void QSGDefaultInternalImageNode::setVerticalWrapMode(QSGTexture::WrapMode wrapMode)
{
    if (m_material.verticalWrapMode() == wrapMode)
        return;

    m_material.setVerticalWrapMode(wrapMode);
    m_materialO.setVerticalWrapMode(wrapMode);
    m_smoothMaterial.setVerticalWrapMode(wrapMode);
    markDirty(DirtyMaterial);
}

void QSGDefaultInternalImageNode::setHorizontalWrapMode(QSGTexture::WrapMode wrapMode)
{
    if (m_material.horizontalWrapMode() == wrapMode)
        return;

    m_material.setHorizontalWrapMode(wrapMode);
    m_materialO.setHorizontalWrapMode(wrapMode);
    m_smoothMaterial.setHorizontalWrapMode(wrapMode);
    markDirty(DirtyMaterial);
}

void QSGDefaultInternalImageNode::updateMaterialAntialiasing()
{
    if (m_antialiasing) {
        setMaterial(&m_smoothMaterial);
        setOpaqueMaterial(0);
    } else {
        setMaterial(&m_materialO);
        setOpaqueMaterial(&m_material);
    }
}

void QSGDefaultInternalImageNode::setMaterialTexture(QSGTexture *texture)
{
    m_material.setTexture(texture);
    m_materialO.setTexture(texture);
    m_smoothMaterial.setTexture(texture);
}

QSGTexture *QSGDefaultInternalImageNode::materialTexture() const
{
    return m_material.texture();
}

bool QSGDefaultInternalImageNode::updateMaterialBlending()
{
    const bool alpha = m_material.flags() & QSGMaterial::Blending;
    if (materialTexture() && alpha != materialTexture()->hasAlphaChannel()) {
        m_material.setFlag(QSGMaterial::Blending, !alpha);
        return true;
    }
    return false;
}

inline static bool isPowerOfTwo(int x)
{
    // Assumption: x >= 1
    return x == (x & -x);
}

bool QSGDefaultInternalImageNode::supportsWrap(const QSize &size) const
{
    bool wrapSupported = true;

    QOpenGLContext *ctx = QOpenGLContext::currentContext();
#ifndef QT_OPENGL_ES_2
    if (ctx->isOpenGLES())
#endif
    {
        bool npotSupported = ctx->functions()->hasOpenGLFeature(QOpenGLFunctions::NPOTTextureRepeat);
        const bool isNpot = !isPowerOfTwo(size.width()) || !isPowerOfTwo(size.height());
        wrapSupported = npotSupported || !isNpot;
    }

    return wrapSupported;
}

QT_END_NAMESPACE
