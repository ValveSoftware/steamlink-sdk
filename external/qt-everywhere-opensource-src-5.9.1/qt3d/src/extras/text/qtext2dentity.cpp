/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "qtext2dentity.h"
#include "qtext2dentity_p.h"
#include "qtext2dmaterial_p.h"

#include <QtGui/qtextlayout.h>
#include <QtGui/qglyphrun.h>
#include <QtGui/private/qdistancefield_p.h>
#include <QtGui/private/qtextureglyphcache_p.h>
#include <QtGui/private/qfont_p.h>
#include <QtGui/private/qdistancefield_p.h>

#include <Qt3DRender/qmaterial.h>
#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/qattribute.h>
#include <Qt3DRender/qgeometry.h>
#include <Qt3DRender/qgeometryrenderer.h>

#include <Qt3DCore/private/qscene_p.h>

QT_BEGIN_NAMESPACE

namespace {

inline Q_DECL_CONSTEXPR QRectF scaleRectF(const QRectF &rect, float scale)
{
    return QRectF(rect.left() * scale, rect.top() * scale, rect.width() * scale, rect.height() * scale);
}

} // anonymous

namespace Qt3DExtras {

QHash<Qt3DCore::QScene *, QText2DEntityPrivate::CacheEntry> QText2DEntityPrivate::m_glyphCacheInstances;

QText2DEntityPrivate::QText2DEntityPrivate()
    : m_glyphCache(nullptr)
    , m_font(QLatin1String("Times"), 10)
    , m_scaledFont(QLatin1String("Times"), 10)
    , m_color(QColor(255, 255, 255, 255))
    , m_width(0.0f)
    , m_height(0.0f)
{
}

QText2DEntityPrivate::~QText2DEntityPrivate()
{
}

void QText2DEntityPrivate::setScene(Qt3DCore::QScene *scene)
{
    if (scene == m_scene)
        return;

    // Unref old glyph cache if it exists
    if (m_scene != nullptr) {
        m_glyphCache = nullptr;
        QText2DEntityPrivate::CacheEntry &entry = QText2DEntityPrivate::m_glyphCacheInstances[m_scene];
        --entry.count;
        if (entry.count == 0 && entry.glyphCache != nullptr) {
            delete entry.glyphCache;
            entry.glyphCache = nullptr;
        }
    }

    QEntityPrivate::setScene(scene);

    // Ref new glyph cache is scene is valid
    if (scene != nullptr) {
        QText2DEntityPrivate::CacheEntry &entry = QText2DEntityPrivate::m_glyphCacheInstances[scene];
        if (entry.glyphCache == nullptr) {
            entry.glyphCache = new QDistanceFieldGlyphCache();
            entry.glyphCache->setRootNode(scene->rootNode());
        }
        m_glyphCache = entry.glyphCache;
        ++entry.count;
        // Update to populate glyphCache if needed
        update();
    }
}

QText2DEntity::QText2DEntity(QNode *parent)
    : Qt3DCore::QEntity(*new QText2DEntityPrivate(), parent)
{
}

QText2DEntity::~QText2DEntity()
{
}

float QText2DEntityPrivate::computeActualScale() const
{
    // scale font based on fontScale property and given QFont
    float scale = 1.0f;
    if (m_font.pointSizeF() > 0)
        scale *= m_font.pointSizeF() / m_scaledFont.pointSizeF();
    return scale;
}

struct RenderData {
    int vertexCount = 0;
    QVector<float> vertex;
    QVector<quint16> index;
};

void QText2DEntityPrivate::setCurrentGlyphRuns(const QVector<QGlyphRun> &runs)
{
    if (runs.isEmpty())
        return;

    // For each distinct texture, we need a separate DistanceFieldTextRenderer,
    // for which we need vertex and index data
    QHash<Qt3DRender::QAbstractTexture*, RenderData> renderData;

    const float scale = computeActualScale();

    // process glyph runs
    for (const QGlyphRun &run : runs) {
        const QVector<quint32> glyphs = run.glyphIndexes();
        const QVector<QPointF> pos = run.positions();

        Q_ASSERT(glyphs.size() == pos.size());

        const bool doubleGlyphResolution = m_glyphCache->doubleGlyphResolution(run.rawFont());

        // faithfully copied from QSGDistanceFieldGlyphNode::updateGeometry()
        const float pixelSize = run.rawFont().pixelSize();
        const float fontScale = pixelSize / QT_DISTANCEFIELD_BASEFONTSIZE(doubleGlyphResolution);
        const float margin = QT_DISTANCEFIELD_RADIUS(doubleGlyphResolution) / QT_DISTANCEFIELD_SCALE(doubleGlyphResolution) * fontScale;

        for (int i = 0; i < glyphs.size(); i++) {
            const QDistanceFieldGlyphCache::Glyph &dfield = m_glyphCache->refGlyph(run.rawFont(), glyphs[i]);

            if (!dfield.texture)
                continue;

            RenderData &data = renderData[dfield.texture];

            // faithfully copied from QSGDistanceFieldGlyphNode::updateGeometry()
            QRectF metrics = scaleRectF(dfield.glyphPathBoundingRect, fontScale);
            metrics.adjust(-margin, margin, margin, 3*margin);

            const float top = 0.0f;
            const float left = 0.0f;
            const float right = m_width;
            const float bottom = m_height;

            float x1 = left + scale * (pos[i].x() + metrics.left());
            float y2 = bottom - scale * (pos[i].y() - metrics.top());
            float x2 = x1 + scale * metrics.width();
            float y1 = y2 - scale * metrics.height();

            // only draw glyphs that are at least partly visible
            if (y2 < top || x1 > right)
                continue;

            QRectF texCoords = dfield.texCoords;

            // if a glyph is only partly visible within the given rectangle,
            // cut it in half and adjust tex coords
            if (y1 < top) {
                const float insideRatio = (top - y2) / (y1 - y2);
                y1 = top;
                texCoords.setHeight(texCoords.height() * insideRatio);
            }

            // do the same thing horizontally
            if (x2 > right) {
                const float insideRatio = (right - x1) / (x2 - x1);
                x2 = right;
                texCoords.setWidth(texCoords.width() * insideRatio);
            }

            data.vertex << x1 << y1 << 0.f << texCoords.left() << texCoords.bottom();
            data.vertex << x1 << y2 << 0.f << texCoords.left() << texCoords.top();
            data.vertex << x2 << y1 << 0.f << texCoords.right() << texCoords.bottom();
            data.vertex << x2 << y2 << 0.f << texCoords.right() << texCoords.top();

            data.index << data.vertexCount << data.vertexCount+3 << data.vertexCount+1;
            data.index << data.vertexCount << data.vertexCount+2 << data.vertexCount+3;

            data.vertexCount += 4;
        }
    }

    // make sure we have the correct number of DistanceFieldTextRenderers
    // TODO: we might keep one renderer at all times, so we won't delete and
    // re-allocate one every time the text changes from an empty to a non-empty string
    // and vice-versa
    while (m_renderers.size() > renderData.size())
        delete m_renderers.takeLast();

    while (m_renderers.size() < renderData.size()) {
        DistanceFieldTextRenderer *renderer = new DistanceFieldTextRenderer(q_func());
        renderer->setColor(m_color);
        m_renderers << renderer;
    }

    Q_ASSERT(m_renderers.size() == renderData.size());

    // assign vertex data for all textures to the renderers
    int rendererIdx = 0;
    for (auto it = renderData.begin(); it != renderData.end(); ++it) {
        m_renderers[rendererIdx++]->setGlyphData(it.key(), it.value().vertex, it.value().index);
    }

    // de-ref all glyphs for previous QGlyphRuns
    for (int i = 0; i < m_currentGlyphRuns.size(); i++)
        m_glyphCache->derefGlyphs(m_currentGlyphRuns[i]);
    m_currentGlyphRuns = runs;
}

void QText2DEntityPrivate::update()
{
    if (m_glyphCache == nullptr)
        return;

    QVector<QGlyphRun> glyphRuns;

    // collect all GlyphRuns generated by the QTextLayout
    if ((m_width > 0.0f || m_height > 0.0f) && !m_text.isEmpty()) {
        QTextLayout layout(m_text, m_scaledFont);
        const float lineWidth = m_width / computeActualScale();
        float height = 0;
        layout.beginLayout();

        while (true) {
            QTextLine line = layout.createLine();
            if (!line.isValid())
                break;

            // position current line
            line.setLineWidth(lineWidth);
            line.setPosition(QPointF(0, height));
            height += line.height();

            // add glyph runs created by line
            const QList<QGlyphRun> runs = line.glyphRuns();
            for (const QGlyphRun &run : runs)
                glyphRuns << run;
        }

        layout.endLayout();
    }

    setCurrentGlyphRuns(glyphRuns);
}

QFont QText2DEntity::font() const
{
    Q_D(const QText2DEntity);
    return d->m_font;
}

void QText2DEntity::setFont(const QFont &font)
{
    Q_D(QText2DEntity);
    if (d->m_font != font) {
        // ignore the point size of the font, just make it a default value.
        // still we want to make sure that font() returns the same value
        // that was passed to setFont(), so we store it nevertheless
        d->m_font = font;
        d->m_scaledFont = font;
        d->m_scaledFont.setPointSize(10);

        emit fontChanged(font);

        if (!d->m_text.isEmpty())
            d->update();
    }
}

QColor QText2DEntity::color() const
{
    Q_D(const QText2DEntity);
    return d->m_color;
}

void QText2DEntity::setColor(const QColor &color)
{
    Q_D(QText2DEntity);
    if (d->m_color != color) {
        d->m_color = color;

        emit colorChanged(color);

        for (DistanceFieldTextRenderer *renderer : qAsConst(d->m_renderers))
            renderer->setColor(color);
    }
}

QString QText2DEntity::text() const
{
    Q_D(const QText2DEntity);
    return d->m_text;
}

void QText2DEntity::setText(const QString &text)
{
    Q_D(QText2DEntity);
    if (d->m_text != text) {
        d->m_text = text;
        emit textChanged(text);

        d->update();
    }
}

float QText2DEntity::width() const
{
    Q_D(const QText2DEntity);
    return d->m_width;
}

float QText2DEntity::height() const
{
    Q_D(const QText2DEntity);
    return d->m_height;
}

void QText2DEntity::setWidth(float width)
{
    Q_D(QText2DEntity);
    if (width != d->m_width) {
        d->m_width = width;
        emit widthChanged(width);
        d->update();
    }
}

void QText2DEntity::setHeight(float height)
{
    Q_D(QText2DEntity);
    if (height != d->m_height) {
        d->m_height = height;
        emit heightChanged(height);
        d->update();
    }
}

} // namespace Qt3DExtras

QT_END_NAMESPACE
