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

#include "qsgdistancefieldglyphnode_p.h"
#include "qsgdistancefieldglyphnode_p_p.h"
#include <QtQuick/private/qsgdistancefieldutil_p.h>
#include <QtQuick/private/qsgcontext_p.h>

QT_BEGIN_NAMESPACE

QSGDistanceFieldGlyphNode::QSGDistanceFieldGlyphNode(QSGRenderContext *context)
    : m_glyphNodeType(RootGlyphNode)
    , m_context(context)
    , m_material(0)
    , m_glyph_cache(0)
    , m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 0)
    , m_style(QQuickText::Normal)
    , m_antialiasingMode(GrayAntialiasing)
    , m_texture(0)
    , m_dirtyGeometry(false)
    , m_dirtyMaterial(false)
{
    m_geometry.setDrawingMode(GL_TRIANGLES);
    setGeometry(&m_geometry);
    setFlag(UsePreprocess);
#ifdef QSG_RUNTIME_DESCRIPTION
    qsgnode_set_description(this, QLatin1String("glyphs"));
#endif
}

QSGDistanceFieldGlyphNode::~QSGDistanceFieldGlyphNode()
{
    delete m_material;

    if (m_glyphNodeType == SubGlyphNode)
        return;

    if (m_glyph_cache) {
        m_glyph_cache->release(m_glyphs.glyphIndexes());
        m_glyph_cache->unregisterGlyphNode(this);
        m_glyph_cache->unregisterOwnerElement(ownerElement());
    }

    while (m_nodesToDelete.count())
        delete m_nodesToDelete.takeLast();
}

void QSGDistanceFieldGlyphNode::setColor(const QColor &color)
{
    m_color = color;
    if (m_material != 0) {
        m_material->setColor(color);
        markDirty(DirtyMaterial);
    } else {
        m_dirtyMaterial = true;
    }
}

void QSGDistanceFieldGlyphNode::setPreferredAntialiasingMode(AntialiasingMode mode)
{
    if (mode == m_antialiasingMode)
        return;
    m_antialiasingMode = mode;
    m_dirtyMaterial = true;
}

void QSGDistanceFieldGlyphNode::setGlyphs(const QPointF &position, const QGlyphRun &glyphs)
{
    QRawFont font = glyphs.rawFont();
    m_originalPosition = position;
    m_position = QPointF(position.x(), position.y() - font.ascent());
    m_glyphs = glyphs;

    m_dirtyGeometry = true;
    m_dirtyMaterial = true;

    QSGDistanceFieldGlyphCache *oldCache = m_glyph_cache;
    m_glyph_cache = m_context->distanceFieldGlyphCache(m_glyphs.rawFont());

    if (m_glyphNodeType == SubGlyphNode)
        return;

    if (m_glyph_cache != oldCache) {
        Q_ASSERT(ownerElement() != 0);
        if (oldCache) {
            oldCache->unregisterGlyphNode(this);
            oldCache->unregisterOwnerElement(ownerElement());
        }
        m_glyph_cache->registerGlyphNode(this);
        m_glyph_cache->registerOwnerElement(ownerElement());
    }
    m_glyph_cache->populate(glyphs.glyphIndexes());

    const QVector<quint32> glyphIndexes = m_glyphs.glyphIndexes();
    for (int i = 0; i < glyphIndexes.count(); ++i)
        m_allGlyphIndexesLookup.insert(glyphIndexes.at(i));
}

void QSGDistanceFieldGlyphNode::setStyle(QQuickText::TextStyle style)
{
    if (m_style == style)
        return;
    m_style = style;
    m_dirtyMaterial = true;
}

void QSGDistanceFieldGlyphNode::setStyleColor(const QColor &color)
{
    if (m_styleColor == color)
        return;
    m_styleColor = color;
    m_dirtyMaterial = true;
}

void QSGDistanceFieldGlyphNode::update()
{
    if (m_dirtyMaterial)
        updateMaterial();
}

void QSGDistanceFieldGlyphNode::preprocess()
{
    Q_ASSERT(m_glyph_cache);

    while (m_nodesToDelete.count())
        delete m_nodesToDelete.takeLast();

    m_glyph_cache->processPendingGlyphs();
    m_glyph_cache->update();

    if (m_dirtyGeometry)
        updateGeometry();
}

void QSGDistanceFieldGlyphNode::invalidateGlyphs(const QVector<quint32> &glyphs)
{
    if (m_dirtyGeometry)
        return;

    for (int i = 0; i < glyphs.count(); ++i) {
        if (m_allGlyphIndexesLookup.contains(glyphs.at(i))) {
            m_dirtyGeometry = true;
            return;
        }
    }
}

void QSGDistanceFieldGlyphNode::updateGeometry()
{
    Q_ASSERT(m_glyph_cache);

    // Remove previously created sub glyph nodes
    // We assume all the children are sub glyph nodes
    QSGNode *subnode = firstChild();
    while (subnode) {
        // We can't delete the node now as it might be in the preprocess list
        // It will be deleted in the next preprocess
        m_nodesToDelete.append(subnode);
        subnode = subnode->nextSibling();
    }
    removeAllChildNodes();

    QSGGeometry *g = geometry();

    Q_ASSERT(g->indexType() == GL_UNSIGNED_SHORT);

    QHash<const QSGDistanceFieldGlyphCache::Texture *, GlyphInfo> glyphsInOtherTextures;

    const QVector<quint32> indexes = m_glyphs.glyphIndexes();
    const QVector<QPointF> positions = m_glyphs.positions();
    qreal fontPixelSize = m_glyphs.rawFont().pixelSize();

    // The template parameters here are assuming that most strings are short, 64
    // characters or less.
    QVarLengthArray<QSGGeometry::TexturedPoint2D, 256> vp;
    vp.reserve(indexes.size() * 4);
    QVarLengthArray<ushort, 384> ip;
    ip.reserve(indexes.size() * 6);

    qreal maxTexMargin = m_glyph_cache->distanceFieldRadius();
    qreal fontScale = m_glyph_cache->fontScale(fontPixelSize);
    qreal margin = 2;
    qreal texMargin = margin / fontScale;
    if (texMargin > maxTexMargin) {
        texMargin = maxTexMargin;
        margin = maxTexMargin * fontScale;
    }

    for (int i = 0; i < indexes.size(); ++i) {
        const int glyphIndex = indexes.at(i);
        QSGDistanceFieldGlyphCache::TexCoord c = m_glyph_cache->glyphTexCoord(glyphIndex);

        if (c.isNull())
            continue;

        const QPointF position = positions.at(i);

        const QSGDistanceFieldGlyphCache::Texture *texture = m_glyph_cache->glyphTexture(glyphIndex);
        if (texture->textureId && !m_texture)
            m_texture = texture;

        // As we use UNSIGNED_SHORT indexing in the geometry, we overload the
        // "glyphsInOtherTextures" concept as overflow for if there are more than
        // 65536 vertices to render which would otherwise exceed the maximum index
        // size.  This will cause sub-nodes to be recursively created to handle any
        // number of glyphs.
        if (m_texture != texture || vp.size() >= 65536) {
            if (texture->textureId) {
                GlyphInfo &glyphInfo = glyphsInOtherTextures[texture];
                glyphInfo.indexes.append(glyphIndex);
                glyphInfo.positions.append(position);
            }
            continue;
        }

        QSGDistanceFieldGlyphCache::Metrics metrics = m_glyph_cache->glyphMetrics(glyphIndex, fontPixelSize);

        if (!metrics.isNull() && !c.isNull()) {
            metrics.width += margin * 2;
            metrics.height += margin * 2;
            metrics.baselineX -= margin;
            metrics.baselineY += margin;
            c.xMargin -= texMargin;
            c.yMargin -= texMargin;
            c.width += texMargin * 2;
            c.height += texMargin * 2;
        }

        qreal x = position.x() + metrics.baselineX + m_position.x();
        qreal y = position.y() - metrics.baselineY + m_position.y();

        m_boundingRect |= QRectF(x, y, metrics.width, metrics.height);

        float cx1 = x;
        float cx2 = x + metrics.width;
        float cy1 = y;
        float cy2 = y + metrics.height;

        float tx1 = c.x + c.xMargin;
        float tx2 = tx1 + c.width;
        float ty1 = c.y + c.yMargin;
        float ty2 = ty1 + c.height;

        if (m_baseLine.isNull())
            m_baseLine = position;

        int o = vp.size();

        QSGGeometry::TexturedPoint2D v1;
        v1.set(cx1, cy1, tx1, ty1);
        QSGGeometry::TexturedPoint2D v2;
        v2.set(cx2, cy1, tx2, ty1);
        QSGGeometry::TexturedPoint2D v3;
        v3.set(cx1, cy2, tx1, ty2);
        QSGGeometry::TexturedPoint2D v4;
        v4.set(cx2, cy2, tx2, ty2);
        vp.append(v1);
        vp.append(v2);
        vp.append(v3);
        vp.append(v4);

        ip.append(o + 0);
        ip.append(o + 2);
        ip.append(o + 3);
        ip.append(o + 3);
        ip.append(o + 1);
        ip.append(o + 0);
    }

    QHash<const QSGDistanceFieldGlyphCache::Texture *, GlyphInfo>::const_iterator ite = glyphsInOtherTextures.constBegin();
    while (ite != glyphsInOtherTextures.constEnd()) {
        QGlyphRun subNodeGlyphRun(m_glyphs);
        subNodeGlyphRun.setGlyphIndexes(ite->indexes);
        subNodeGlyphRun.setPositions(ite->positions);

        QSGDistanceFieldGlyphNode *subNode = new QSGDistanceFieldGlyphNode(m_context);
        subNode->setGlyphNodeType(SubGlyphNode);
        subNode->setColor(m_color);
        subNode->setStyle(m_style);
        subNode->setStyleColor(m_styleColor);
        subNode->setPreferredAntialiasingMode(m_antialiasingMode);
        subNode->setGlyphs(m_originalPosition, subNodeGlyphRun);
        subNode->update();
        subNode->updateGeometry(); // we have to explicitly call this now as preprocess won't be called before it's rendered
        appendChildNode(subNode);

        ++ite;
    }

    g->allocate(vp.size(), ip.size());
    memcpy(g->vertexDataAsTexturedPoint2D(), vp.constData(), vp.size() * sizeof(QSGGeometry::TexturedPoint2D));
    memcpy(g->indexDataAsUShort(), ip.constData(), ip.size() * sizeof(quint16));

    setBoundingRect(m_boundingRect);
    markDirty(DirtyGeometry);
    m_dirtyGeometry = false;

    m_material->setTexture(m_texture);
}

void QSGDistanceFieldGlyphNode::updateMaterial()
{
    delete m_material;

    if (m_style == QQuickText::Normal) {
        switch (m_antialiasingMode) {
        case HighQualitySubPixelAntialiasing:
            m_material = new QSGHiQSubPixelDistanceFieldTextMaterial;
            break;
        case LowQualitySubPixelAntialiasing:
            m_material = new QSGLoQSubPixelDistanceFieldTextMaterial;
            break;
        case GrayAntialiasing:
        default:
            m_material = new QSGDistanceFieldTextMaterial;
            break;
        }
    } else {
        QSGDistanceFieldStyledTextMaterial *material;
        if (m_style == QQuickText::Outline) {
            material = new QSGDistanceFieldOutlineTextMaterial;
        } else {
            QSGDistanceFieldShiftedStyleTextMaterial *sMaterial = new QSGDistanceFieldShiftedStyleTextMaterial;
            if (m_style == QQuickText::Raised)
                sMaterial->setShift(QPointF(0.0, 1.0));
            else
                sMaterial->setShift(QPointF(0.0, -1.0));
            material = sMaterial;
        }
        material->setStyleColor(m_styleColor);
        m_material = material;
    }

    m_material->setGlyphCache(m_glyph_cache);
    if (m_glyph_cache)
        m_material->setFontScale(m_glyph_cache->fontScale(m_glyphs.rawFont().pixelSize()));
    m_material->setColor(m_color);
    setMaterial(m_material);
    m_dirtyMaterial = false;
}

QT_END_NAMESPACE
