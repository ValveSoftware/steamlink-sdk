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

#include "qsgd3d12spritenode_p.h"
#include "qsgd3d12material_p.h"

#include "vs_sprite.hlslh"
#include "ps_sprite.hlslh"

QT_BEGIN_NAMESPACE

struct SpriteVertex
{
    float x;
    float y;
    float tx;
    float ty;
};

struct SpriteVertices
{
    SpriteVertex v1;
    SpriteVertex v2;
    SpriteVertex v3;
    SpriteVertex v4;
};

class QSGD3D12SpriteMaterial : public QSGD3D12Material
{
public:
    QSGD3D12SpriteMaterial();
    ~QSGD3D12SpriteMaterial();

    QSGMaterialType *type() const override { static QSGMaterialType type; return &type; }

    int compare(const QSGMaterial *other) const override
    {
        return this - static_cast<const QSGD3D12SpriteMaterial *>(other);
    }

    int constantBufferSize() const override;
    void preparePipeline(QSGD3D12PipelineState *pipelineState) override;
    UpdateResults updatePipeline(const QSGD3D12MaterialRenderState &state,
                                 QSGD3D12PipelineState *pipelineState,
                                 ExtraState *extraState,
                                 quint8 *constantBuffer) override;

    QSGTexture *texture;

    float animT;
    float animX1;
    float animY1;
    float animX2;
    float animY2;
    float animW;
    float animH;
};

QSGD3D12SpriteMaterial::QSGD3D12SpriteMaterial()
    : texture(nullptr),
      animT(0.0f),
      animX1(0.0f),
      animY1(0.0f),
      animX2(0.0f),
      animY2(0.0f),
      animW(1.0f),
      animH(1.0f)
{
    setFlag(Blending, true);
}

QSGD3D12SpriteMaterial::~QSGD3D12SpriteMaterial()
{
    delete texture;
}

static const int SPRITE_CB_SIZE_0 = 16 * sizeof(float); // float4x4
static const int SPRITE_CB_SIZE_1 = 4 * sizeof(float); // float4
static const int SPRITE_CB_SIZE_2 = 3 * sizeof(float); // float3
static const int SPRITE_CB_SIZE_3 = sizeof(float); // float
static const int SPRITE_CB_SIZE = SPRITE_CB_SIZE_0 + SPRITE_CB_SIZE_1 + SPRITE_CB_SIZE_2 + SPRITE_CB_SIZE_3;

int QSGD3D12SpriteMaterial::constantBufferSize() const
{
    return QSGD3D12Engine::alignedConstantBufferSize(SPRITE_CB_SIZE);
}

void QSGD3D12SpriteMaterial::preparePipeline(QSGD3D12PipelineState *pipelineState)
{
    pipelineState->shaders.vs = g_VS_Sprite;
    pipelineState->shaders.vsSize = sizeof(g_VS_Sprite);
    pipelineState->shaders.ps = g_PS_Sprite;
    pipelineState->shaders.psSize = sizeof(g_PS_Sprite);

    pipelineState->shaders.rootSig.textureViewCount = 1;
}

QSGD3D12Material::UpdateResults QSGD3D12SpriteMaterial::updatePipeline(const QSGD3D12MaterialRenderState &state,
                                                                       QSGD3D12PipelineState *,
                                                                       ExtraState *,
                                                                       quint8 *constantBuffer)
{
    QSGD3D12Material::UpdateResults r = UpdatedConstantBuffer;
    quint8 *p = constantBuffer;

    if (state.isMatrixDirty())
        memcpy(p, state.combinedMatrix().constData(), SPRITE_CB_SIZE_0);
    p += SPRITE_CB_SIZE_0;

    {
        const float v[] = { animX1, animY1, animX2, animY2 };
        memcpy(p, v, SPRITE_CB_SIZE_1);
    }
    p += SPRITE_CB_SIZE_1;

    {
        const float v[] = { animW, animH, animT };
        memcpy(p, v, SPRITE_CB_SIZE_2);
    }
    p += SPRITE_CB_SIZE_2;

    if (state.isOpacityDirty()) {
        const float opacity = state.opacity();
        memcpy(p, &opacity, SPRITE_CB_SIZE_3);
    }

    texture->bind();

    return r;
}

static QSGGeometry::Attribute Sprite_Attributes[] = {
    QSGGeometry::Attribute::createWithAttributeType(0, 2, QSGGeometry::FloatType, QSGGeometry::PositionAttribute),
    QSGGeometry::Attribute::createWithAttributeType(1, 2, QSGGeometry::FloatType, QSGGeometry::TexCoordAttribute),
};

static QSGGeometry::AttributeSet Sprite_AttributeSet = { 2, 4 * sizeof(float), Sprite_Attributes };

QSGD3D12SpriteNode::QSGD3D12SpriteNode()
    : m_material(new QSGD3D12SpriteMaterial)
    , m_geometryDirty(true)
    , m_sheetSize(QSize(64, 64))
{
    m_geometry = new QSGGeometry(Sprite_AttributeSet, 4, 6);
    m_geometry->setDrawingMode(QSGGeometry::DrawTriangles);

    quint16 *indices = m_geometry->indexDataAsUShort();
    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;
    indices[3] = 1;
    indices[4] = 3;
    indices[5] = 2;

    setGeometry(m_geometry);
    setMaterial(m_material);
    setFlag(OwnsGeometry, true);
    setFlag(OwnsMaterial, true);
}

void QSGD3D12SpriteNode::setTexture(QSGTexture *texture)
{
    m_material->texture = texture;
    m_geometryDirty = true;
    markDirty(DirtyMaterial);
}

void QSGD3D12SpriteNode::setTime(float time)
{
    m_material->animT = time;
    markDirty(DirtyMaterial);
}

void QSGD3D12SpriteNode::setSourceA(const QPoint &source)
{
    if (m_sourceA != source) {
        m_sourceA = source;
        m_material->animX1 = static_cast<float>(source.x()) / m_sheetSize.width();
        m_material->animY1 = static_cast<float>(source.y()) / m_sheetSize.height();
        markDirty(DirtyMaterial);
    }
}

void QSGD3D12SpriteNode::setSourceB(const QPoint &source)
{
    if (m_sourceB != source) {
        m_sourceB = source;
        m_material->animX2 = static_cast<float>(source.x()) / m_sheetSize.width();
        m_material->animY2 = static_cast<float>(source.y()) / m_sheetSize.height();
        markDirty(DirtyMaterial);
    }
}

void QSGD3D12SpriteNode::setSpriteSize(const QSize &size)
{
    if (m_spriteSize != size) {
        m_spriteSize = size;
        m_material->animW = static_cast<float>(size.width()) / m_sheetSize.width();
        m_material->animH = static_cast<float>(size.height()) / m_sheetSize.height();
        markDirty(DirtyMaterial);
    }
}

void QSGD3D12SpriteNode::setSheetSize(const QSize &size)
{
    if (m_sheetSize != size) {
        m_sheetSize = size;

        // Update all dependent properties
        m_material->animX1 = static_cast<float>(m_sourceA.x()) / m_sheetSize.width();
        m_material->animY1 = static_cast<float>(m_sourceA.y()) / m_sheetSize.height();
        m_material->animX2 = static_cast<float>(m_sourceB.x()) / m_sheetSize.width();
        m_material->animY2 = static_cast<float>(m_sourceB.y()) / m_sheetSize.height();
        m_material->animW = static_cast<float>(m_spriteSize.width()) / m_sheetSize.width();
        m_material->animH = static_cast<float>(m_spriteSize.height()) / m_sheetSize.height();

        markDirty(DirtyMaterial);
    }
}

void QSGD3D12SpriteNode::setSize(const QSizeF &size)
{
    if (m_size != size) {
        m_size = size;
        m_geometryDirty = true;
    }
}

void QSGD3D12SpriteNode::setFiltering(QSGTexture::Filtering filtering)
{
    m_material->texture->setFiltering(filtering);
    markDirty(DirtyMaterial);
}

void QSGD3D12SpriteNode::update()
{
    if (m_geometryDirty) {
        m_geometryDirty = false;
        updateGeometry();
    }
}

void QSGD3D12SpriteNode::updateGeometry()
{
    if (!m_material->texture)
        return;

    SpriteVertices *p = static_cast<SpriteVertices *>(m_geometry->vertexData());
    const QRectF texRect = m_material->texture->normalizedTextureSubRect();

    p->v1.tx = texRect.topLeft().x();
    p->v1.ty = texRect.topLeft().y();

    p->v2.tx = texRect.topRight().x();
    p->v2.ty = texRect.topRight().y();

    p->v3.tx = texRect.bottomLeft().x();
    p->v3.ty = texRect.bottomLeft().y();

    p->v4.tx = texRect.bottomRight().x();
    p->v4.ty = texRect.bottomRight().y();

    p->v1.x = 0;
    p->v1.y = 0;

    p->v2.x = m_size.width();
    p->v2.y = 0;

    p->v3.x = 0;
    p->v3.y = m_size.height();

    p->v4.x = m_size.width();
    p->v4.y = m_size.height();

    markDirty(DirtyGeometry);
}

QT_END_NAMESPACE
