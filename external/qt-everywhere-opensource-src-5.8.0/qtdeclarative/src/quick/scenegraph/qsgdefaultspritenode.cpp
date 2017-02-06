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

#include "qsgdefaultspritenode_p.h"

#include <QtQuick/QSGMaterial>
#include <QtGui/QOpenGLShaderProgram>

QT_BEGIN_NAMESPACE

struct SpriteVertex {
    float x;
    float y;
    float tx;
    float ty;
};

struct SpriteVertices {
    SpriteVertex v1;
    SpriteVertex v2;
    SpriteVertex v3;
    SpriteVertex v4;
};

class QQuickSpriteMaterial : public QSGMaterial
{
public:
    QQuickSpriteMaterial();
    ~QQuickSpriteMaterial();
    QSGMaterialType *type() const  override { static QSGMaterialType type; return &type; }
    QSGMaterialShader *createShader() const override;
    int compare(const QSGMaterial *other) const override
    {
        return this - static_cast<const QQuickSpriteMaterial *>(other);
    }

    QSGTexture *texture;

    float animT;
    float animX1;
    float animY1;
    float animX2;
    float animY2;
    float animW;
    float animH;
};

QQuickSpriteMaterial::QQuickSpriteMaterial()
    : texture(0)
    , animT(0.0f)
    , animX1(0.0f)
    , animY1(0.0f)
    , animX2(0.0f)
    , animY2(0.0f)
    , animW(1.0f)
    , animH(1.0f)
{
    setFlag(Blending, true);
}

QQuickSpriteMaterial::~QQuickSpriteMaterial()
{
    delete texture;
}

class SpriteMaterialData : public QSGMaterialShader
{
public:
    SpriteMaterialData()
        : QSGMaterialShader()
    {
        setShaderSourceFile(QOpenGLShader::Vertex, QStringLiteral(":/qt-project.org/scenegraph/shaders/sprite.vert"));
        setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/qt-project.org/scenegraph/shaders/sprite.frag"));
    }

    void updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *) Q_DECL_OVERRIDE
    {
        QQuickSpriteMaterial *m = static_cast<QQuickSpriteMaterial *>(newEffect);
        m->texture->bind();

        program()->setUniformValue(m_opacity_id, state.opacity());
        program()->setUniformValue(m_animData_id, m->animW, m->animH, m->animT);
        program()->setUniformValue(m_animPos_id, m->animX1, m->animY1, m->animX2, m->animY2);

        if (state.isMatrixDirty())
            program()->setUniformValue(m_matrix_id, state.combinedMatrix());
    }

    void initialize() Q_DECL_OVERRIDE {
        m_matrix_id = program()->uniformLocation("qt_Matrix");
        m_opacity_id = program()->uniformLocation("qt_Opacity");
        m_animData_id = program()->uniformLocation("animData");
        m_animPos_id = program()->uniformLocation("animPos");
    }

    char const *const *attributeNames() const Q_DECL_OVERRIDE {
        static const char *attr[] = {
           "vPos",
           "vTex",
            0
        };
        return attr;
    }

    int m_matrix_id;
    int m_opacity_id;
    int m_animData_id;
    int m_animPos_id;
};

QSGMaterialShader *QQuickSpriteMaterial::createShader() const
{
    return new SpriteMaterialData;
}

static QSGGeometry::Attribute Sprite_Attributes[] = {
    QSGGeometry::Attribute::create(0, 2, QSGGeometry::FloatType, true),   // pos
    QSGGeometry::Attribute::create(1, 2, QSGGeometry::FloatType),         // tex
};

static QSGGeometry::AttributeSet Sprite_AttributeSet =
{
    2, // Attribute Count
    (2+2) * sizeof(float),
    Sprite_Attributes
};

QSGDefaultSpriteNode::QSGDefaultSpriteNode()
    : m_material(new QQuickSpriteMaterial)
    , m_geometryDirty(true)
    , m_sheetSize(QSize(64, 64))
{
    // Setup geometry data
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

void QSGDefaultSpriteNode::setTexture(QSGTexture *texture)
{
    m_material->texture = texture;
    m_geometryDirty = true;
    markDirty(DirtyMaterial);
}

void QSGDefaultSpriteNode::setTime(float time)
{
    m_material->animT = time;
    markDirty(DirtyMaterial);
}

void QSGDefaultSpriteNode::setSourceA(const QPoint &source)
{
    if (m_sourceA != source) {
        m_sourceA = source;
        m_material->animX1 = static_cast<float>(source.x()) / m_sheetSize.width();
        m_material->animY1 = static_cast<float>(source.y()) / m_sheetSize.height();
        markDirty(DirtyMaterial);
    }
}

void QSGDefaultSpriteNode::setSourceB(const QPoint &source)
{
    if (m_sourceB != source) {
        m_sourceB = source;
        m_material->animX2 = static_cast<float>(source.x()) / m_sheetSize.width();
        m_material->animY2 = static_cast<float>(source.y()) / m_sheetSize.height();
        markDirty(DirtyMaterial);
    }
}

void QSGDefaultSpriteNode::setSpriteSize(const QSize &size)
{
    if (m_spriteSize != size) {
        m_spriteSize = size;
        m_material->animW = static_cast<float>(size.width()) / m_sheetSize.width();
        m_material->animH = static_cast<float>(size.height()) / m_sheetSize.height();
        markDirty(DirtyMaterial);
    }

}

void QSGDefaultSpriteNode::setSheetSize(const QSize &size)
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

void QSGDefaultSpriteNode::setSize(const QSizeF &size)
{
    if (m_size != size) {
        m_size = size;
        m_geometryDirty = true;
    }
}

void QSGDefaultSpriteNode::setFiltering(QSGTexture::Filtering filtering)
{
    m_material->texture->setFiltering(filtering);
    markDirty(DirtyMaterial);
}

void QSGDefaultSpriteNode::update()
{
    if (m_geometryDirty) {
        updateGeometry();
        m_geometryDirty = false;
    }
}

void QSGDefaultSpriteNode::updateGeometry()
{
    if (!m_material->texture)
        return;

    SpriteVertices *p = (SpriteVertices *) m_geometry->vertexData();

    QRectF texRect = m_material->texture->normalizedTextureSubRect();

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
