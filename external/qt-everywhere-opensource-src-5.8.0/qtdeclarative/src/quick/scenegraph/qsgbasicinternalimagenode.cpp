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

#include "qsgbasicinternalimagenode_p.h"

#include <QtCore/qvarlengtharray.h>
#include <QtCore/qmath.h>

QT_BEGIN_NAMESPACE

namespace
{
    struct SmoothVertex
    {
        float x, y, u, v;
        float dx, dy, du, dv;
    };

    const QSGGeometry::AttributeSet &smoothAttributeSet()
    {
        static QSGGeometry::Attribute data[] = {
            QSGGeometry::Attribute::createWithAttributeType(0, 2, QSGGeometry::FloatType, QSGGeometry::PositionAttribute),
            QSGGeometry::Attribute::createWithAttributeType(1, 2, QSGGeometry::FloatType, QSGGeometry::TexCoordAttribute),
            QSGGeometry::Attribute::createWithAttributeType(2, 2, QSGGeometry::FloatType, QSGGeometry::TexCoord1Attribute),
            QSGGeometry::Attribute::createWithAttributeType(3, 2, QSGGeometry::FloatType, QSGGeometry::TexCoord2Attribute)
        };
        static QSGGeometry::AttributeSet attrs = { 4, sizeof(SmoothVertex), data };
        return attrs;
    }
}

QSGBasicInternalImageNode::QSGBasicInternalImageNode()
    : m_innerSourceRect(0, 0, 1, 1)
    , m_subSourceRect(0, 0, 1, 1)
    , m_antialiasing(false)
    , m_mirror(false)
    , m_dirtyGeometry(false)
    , m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
    , m_dynamicTexture(nullptr)
{
    setGeometry(&m_geometry);

#ifdef QSG_RUNTIME_DESCRIPTION
    qsgnode_set_description(this, QLatin1String("internalimage"));
#endif
}

void QSGBasicInternalImageNode::setTargetRect(const QRectF &rect)
{
    if (rect == m_targetRect)
        return;
    m_targetRect = rect;
    m_dirtyGeometry = true;
}

void QSGBasicInternalImageNode::setInnerTargetRect(const QRectF &rect)
{
    if (rect == m_innerTargetRect)
        return;
    m_innerTargetRect = rect;
    m_dirtyGeometry = true;
}

void QSGBasicInternalImageNode::setInnerSourceRect(const QRectF &rect)
{
    if (rect == m_innerSourceRect)
        return;
    m_innerSourceRect = rect;
    m_dirtyGeometry = true;
}

void QSGBasicInternalImageNode::setSubSourceRect(const QRectF &rect)
{
    if (rect == m_subSourceRect)
        return;
    m_subSourceRect = rect;
    m_dirtyGeometry = true;
}

void QSGBasicInternalImageNode::setTexture(QSGTexture *texture)
{
    Q_ASSERT(texture);

    setMaterialTexture(texture);
    updateMaterialBlending();

    markDirty(DirtyMaterial);

    // Because the texture can be a different part of the atlas, we need to update it...
    m_dirtyGeometry = true;
}

void QSGBasicInternalImageNode::setAntialiasing(bool antialiasing)
{
    if (antialiasing == m_antialiasing)
        return;
    m_antialiasing = antialiasing;
    if (m_antialiasing) {
        setGeometry(new QSGGeometry(smoothAttributeSet(), 0));
        setFlag(OwnsGeometry, true);
    } else {
        setGeometry(&m_geometry);
        setFlag(OwnsGeometry, false);
    }
    updateMaterialAntialiasing();
    m_dirtyGeometry = true;
}

void QSGBasicInternalImageNode::setMirror(bool mirror)
{
    if (mirror == m_mirror)
        return;
    m_mirror = mirror;
    m_dirtyGeometry = true;
}


void QSGBasicInternalImageNode::update()
{
    if (m_dirtyGeometry)
        updateGeometry();
}

void QSGBasicInternalImageNode::preprocess()
{
    bool doDirty = false;
    QSGDynamicTexture *t = qobject_cast<QSGDynamicTexture *>(materialTexture());
    if (t) {
        doDirty = t->updateTexture();
        if (doDirty) {
            // The geometry may need updating. This is expensive however, so do
            // it only when something relevant has changed.
            if (t != m_dynamicTexture
                    || t->textureSize() != m_dynamicTextureSize
                    || t->normalizedTextureSubRect() != m_dynamicTextureSubRect) {
                updateGeometry();
                m_dynamicTextureSize = t->textureSize();
                m_dynamicTextureSubRect = t->normalizedTextureSubRect();
            }
        }
    }
    m_dynamicTexture = t;

    if (updateMaterialBlending())
        doDirty = true;

    if (doDirty)
        markDirty(DirtyMaterial);
}

namespace {
    struct X { float x, tx; };
    struct Y { float y, ty; };
}

static inline void appendQuad(quint16 **indices, quint16 topLeft, quint16 topRight,
                              quint16 bottomLeft, quint16 bottomRight)
{
    *(*indices)++ = topLeft;
    *(*indices)++ = bottomLeft;
    *(*indices)++ = bottomRight;
    *(*indices)++ = bottomRight;
    *(*indices)++ = topRight;
    *(*indices)++ = topLeft;
}

QSGGeometry *QSGBasicInternalImageNode::updateGeometry(const QRectF &targetRect,
                                               const QRectF &innerTargetRect,
                                               const QRectF &sourceRect,
                                               const QRectF &innerSourceRect,
                                               const QRectF &subSourceRect,
                                               QSGGeometry *geometry,
                                               bool mirror,
                                               bool antialiasing)
{
    int floorLeft = qFloor(subSourceRect.left());
    int ceilRight = qCeil(subSourceRect.right());
    int floorTop = qFloor(subSourceRect.top());
    int ceilBottom = qCeil(subSourceRect.bottom());
    int hTiles = ceilRight - floorLeft;
    int vTiles = ceilBottom - floorTop;

    int hCells = hTiles;
    int vCells = vTiles;
    if (innerTargetRect.width() == 0)
        hCells = 0;
    if (innerTargetRect.left() != targetRect.left())
        ++hCells;
    if (innerTargetRect.right() != targetRect.right())
        ++hCells;
    if (innerTargetRect.height() == 0)
        vCells = 0;
    if (innerTargetRect.top() != targetRect.top())
        ++vCells;
    if (innerTargetRect.bottom() != targetRect.bottom())
        ++vCells;
    QVarLengthArray<X, 32> xData(2 * hCells);
    QVarLengthArray<Y, 32> yData(2 * vCells);
    X *xs = xData.data();
    Y *ys = yData.data();

    if (innerTargetRect.left() != targetRect.left()) {
        xs[0].x = targetRect.left();
        xs[0].tx = sourceRect.left();
        xs[1].x = innerTargetRect.left();
        xs[1].tx = innerSourceRect.left();
        xs += 2;
    }
    if (innerTargetRect.width() != 0) {
        xs[0].x = innerTargetRect.left();
        xs[0].tx = innerSourceRect.x() + (subSourceRect.left() - floorLeft) * innerSourceRect.width();
        ++xs;
        float b = innerTargetRect.width() / subSourceRect.width();
        float a = innerTargetRect.x() - subSourceRect.x() * b;
        for (int i = floorLeft + 1; i <= ceilRight - 1; ++i) {
            xs[0].x = xs[1].x = a + b * i;
            xs[0].tx = innerSourceRect.right();
            xs[1].tx = innerSourceRect.left();
            xs += 2;
        }
        xs[0].x = innerTargetRect.right();
        xs[0].tx = innerSourceRect.x() + (subSourceRect.right() - ceilRight + 1) * innerSourceRect.width();
        ++xs;
    }
    if (innerTargetRect.right() != targetRect.right()) {
        xs[0].x = innerTargetRect.right();
        xs[0].tx = innerSourceRect.right();
        xs[1].x = targetRect.right();
        xs[1].tx = sourceRect.right();
        xs += 2;
    }
    Q_ASSERT(xs == xData.data() + xData.size());
    if (mirror) {
        float leftPlusRight = targetRect.left() + targetRect.right();
        int count = xData.size();
        xs = xData.data();
        for (int i = 0; i < count >> 1; ++i)
            qSwap(xs[i], xs[count - 1 - i]);
        for (int i = 0; i < count; ++i)
            xs[i].x = leftPlusRight - xs[i].x;
    }

    if (innerTargetRect.top() != targetRect.top()) {
        ys[0].y = targetRect.top();
        ys[0].ty = sourceRect.top();
        ys[1].y = innerTargetRect.top();
        ys[1].ty = innerSourceRect.top();
        ys += 2;
    }
    if (innerTargetRect.height() != 0) {
        ys[0].y = innerTargetRect.top();
        ys[0].ty = innerSourceRect.y() + (subSourceRect.top() - floorTop) * innerSourceRect.height();
        ++ys;
        float b = innerTargetRect.height() / subSourceRect.height();
        float a = innerTargetRect.y() - subSourceRect.y() * b;
        for (int i = floorTop + 1; i <= ceilBottom - 1; ++i) {
            ys[0].y = ys[1].y = a + b * i;
            ys[0].ty = innerSourceRect.bottom();
            ys[1].ty = innerSourceRect.top();
            ys += 2;
        }
        ys[0].y = innerTargetRect.bottom();
        ys[0].ty = innerSourceRect.y() + (subSourceRect.bottom() - ceilBottom + 1) * innerSourceRect.height();
        ++ys;
    }
    if (innerTargetRect.bottom() != targetRect.bottom()) {
        ys[0].y = innerTargetRect.bottom();
        ys[0].ty = innerSourceRect.bottom();
        ys[1].y = targetRect.bottom();
        ys[1].ty = sourceRect.bottom();
        ys += 2;
    }
    Q_ASSERT(ys == yData.data() + yData.size());

    if (antialiasing) {
        QSGGeometry *g = geometry;
        Q_ASSERT(g);

        g->allocate(hCells * vCells * 4 + (hCells + vCells - 1) * 4,
                    hCells * vCells * 6 + (hCells + vCells) * 12);
        g->setDrawingMode(QSGGeometry::DrawTriangles);
        SmoothVertex *vertices = reinterpret_cast<SmoothVertex *>(g->vertexData());
        memset(vertices, 0, g->vertexCount() * g->sizeOfVertex());
        quint16 *indices = g->indexDataAsUShort();

        // The deltas are how much the fuzziness can reach into the image.
        // Only the border vertices are moved by the vertex shader, so the fuzziness
        // can't reach further into the image than the closest interior vertices.
        float leftDx = xData.at(1).x - xData.at(0).x;
        float rightDx = xData.at(xData.size() - 1).x - xData.at(xData.size() - 2).x;
        float topDy = yData.at(1).y - yData.at(0).y;
        float bottomDy = yData.at(yData.size() - 1).y - yData.at(yData.size() - 2).y;

        float leftDu = xData.at(1).tx - xData.at(0).tx;
        float rightDu = xData.at(xData.size() - 1).tx - xData.at(xData.size() - 2).tx;
        float topDv = yData.at(1).ty - yData.at(0).ty;
        float bottomDv = yData.at(yData.size() - 1).ty - yData.at(yData.size() - 2).ty;

        if (hCells == 1) {
            leftDx = rightDx *= 0.5f;
            leftDu = rightDu *= 0.5f;
        }
        if (vCells == 1) {
            topDy = bottomDy *= 0.5f;
            topDv = bottomDv *= 0.5f;
        }

        // This delta is how much the fuzziness can reach out from the image.
        float delta = float(qAbs(targetRect.width()) < qAbs(targetRect.height())
                            ? targetRect.width() : targetRect.height()) * 0.5f;

        quint16 index = 0;
        ys = yData.data();
        for (int j = 0; j < vCells; ++j, ys += 2) {
            xs = xData.data();
            bool isTop = j == 0;
            bool isBottom = j == vCells - 1;
            for (int i = 0; i < hCells; ++i, xs += 2) {
                bool isLeft = i == 0;
                bool isRight = i == hCells - 1;

                SmoothVertex *v = vertices + index;

                quint16 topLeft = index;
                for (int k = (isTop || isLeft ? 2 : 1); k--; ++v, ++index) {
                    v->x = xs[0].x;
                    v->u = xs[0].tx;
                    v->y = ys[0].y;
                    v->v = ys[0].ty;
                }

                quint16 topRight = index;
                for (int k = (isTop || isRight ? 2 : 1); k--; ++v, ++index) {
                    v->x = xs[1].x;
                    v->u = xs[1].tx;
                    v->y = ys[0].y;
                    v->v = ys[0].ty;
                }

                quint16 bottomLeft = index;
                for (int k = (isBottom || isLeft ? 2 : 1); k--; ++v, ++index) {
                    v->x = xs[0].x;
                    v->u = xs[0].tx;
                    v->y = ys[1].y;
                    v->v = ys[1].ty;
                }

                quint16 bottomRight = index;
                for (int k = (isBottom || isRight ? 2 : 1); k--; ++v, ++index) {
                    v->x = xs[1].x;
                    v->u = xs[1].tx;
                    v->y = ys[1].y;
                    v->v = ys[1].ty;
                }

                appendQuad(&indices, topLeft, topRight, bottomLeft, bottomRight);

                if (isTop) {
                    vertices[topLeft].dy = vertices[topRight].dy = topDy;
                    vertices[topLeft].dv = vertices[topRight].dv = topDv;
                    vertices[topLeft + 1].dy = vertices[topRight + 1].dy = -delta;
                    appendQuad(&indices, topLeft + 1, topRight + 1, topLeft, topRight);
                }

                if (isBottom) {
                    vertices[bottomLeft].dy = vertices[bottomRight].dy = -bottomDy;
                    vertices[bottomLeft].dv = vertices[bottomRight].dv = -bottomDv;
                    vertices[bottomLeft + 1].dy = vertices[bottomRight + 1].dy = delta;
                    appendQuad(&indices, bottomLeft, bottomRight, bottomLeft + 1, bottomRight + 1);
                }

                if (isLeft) {
                    vertices[topLeft].dx = vertices[bottomLeft].dx = leftDx;
                    vertices[topLeft].du = vertices[bottomLeft].du = leftDu;
                    vertices[topLeft + 1].dx = vertices[bottomLeft + 1].dx = -delta;
                    appendQuad(&indices, topLeft + 1, topLeft, bottomLeft + 1, bottomLeft);
                }

                if (isRight) {
                    vertices[topRight].dx = vertices[bottomRight].dx = -rightDx;
                    vertices[topRight].du = vertices[bottomRight].du = -rightDu;
                    vertices[topRight + 1].dx = vertices[bottomRight + 1].dx = delta;
                    appendQuad(&indices, topRight, topRight + 1, bottomRight, bottomRight + 1);
                }
            }
        }

        Q_ASSERT(index == g->vertexCount());
        Q_ASSERT(indices - g->indexCount() == g->indexData());
    } else {
        if (!geometry) {
            geometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(),
                                       hCells * vCells * 4, hCells * vCells * 6,
                                       QSGGeometry::UnsignedShortType);
        } else {
            geometry->allocate(hCells * vCells * 4, hCells * vCells * 6);
        }
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        QSGGeometry::TexturedPoint2D *vertices = geometry->vertexDataAsTexturedPoint2D();
        ys = yData.data();
        for (int j = 0; j < vCells; ++j, ys += 2) {
            xs = xData.data();
            for (int i = 0; i < hCells; ++i, xs += 2) {
                vertices[0].x = vertices[2].x = xs[0].x;
                vertices[0].tx = vertices[2].tx = xs[0].tx;
                vertices[1].x = vertices[3].x = xs[1].x;
                vertices[1].tx = vertices[3].tx = xs[1].tx;

                vertices[0].y = vertices[1].y = ys[0].y;
                vertices[0].ty = vertices[1].ty = ys[0].ty;
                vertices[2].y = vertices[3].y = ys[1].y;
                vertices[2].ty = vertices[3].ty = ys[1].ty;

                vertices += 4;
            }
        }

        quint16 *indices = geometry->indexDataAsUShort();
        for (int i = 0; i < 4 * vCells * hCells; i += 4)
            appendQuad(&indices, i, i + 1, i + 2, i + 3);
    }
    return geometry;
}

void QSGBasicInternalImageNode::updateGeometry()
{
    Q_ASSERT(!m_targetRect.isEmpty());
    const QSGTexture *t = materialTexture();
    if (!t) {
        QSGGeometry *g = geometry();
        g->allocate(4);
        g->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        memset(g->vertexData(), 0, g->sizeOfVertex() * 4);
    } else {
        QRectF sourceRect = t->normalizedTextureSubRect();

        QRectF innerSourceRect(sourceRect.x() + m_innerSourceRect.x() * sourceRect.width(),
                               sourceRect.y() + m_innerSourceRect.y() * sourceRect.height(),
                               m_innerSourceRect.width() * sourceRect.width(),
                               m_innerSourceRect.height() * sourceRect.height());

        bool hasMargins = m_targetRect != m_innerTargetRect;

        int floorLeft = qFloor(m_subSourceRect.left());
        int ceilRight = qCeil(m_subSourceRect.right());
        int floorTop = qFloor(m_subSourceRect.top());
        int ceilBottom = qCeil(m_subSourceRect.bottom());
        int hTiles = ceilRight - floorLeft;
        int vTiles = ceilBottom - floorTop;

        bool hasTiles = hTiles != 1 || vTiles != 1;
        bool fullTexture = innerSourceRect == QRectF(0, 0, 1, 1);

        // An image can be rendered as a single quad if:
        // - There are no margins, and either:
        //   - the image isn't repeated
        //   - the source rectangle fills the entire texture so that texture wrapping can be used,
        //     and NPOT is supported
        if (!hasMargins && (!hasTiles || (fullTexture && supportsWrap(t->textureSize())))) {
            QRectF sr;
            if (!fullTexture) {
                sr = QRectF(innerSourceRect.x() + (m_subSourceRect.left() - floorLeft) * innerSourceRect.width(),
                            innerSourceRect.y() + (m_subSourceRect.top() - floorTop) * innerSourceRect.height(),
                            m_subSourceRect.width() * innerSourceRect.width(),
                            m_subSourceRect.height() * innerSourceRect.height());
            } else {
                sr = QRectF(m_subSourceRect.left() - floorLeft, m_subSourceRect.top() - floorTop,
                            m_subSourceRect.width(), m_subSourceRect.height());
            }
            if (m_mirror) {
                qreal oldLeft = sr.left();
                sr.setLeft(sr.right());
                sr.setRight(oldLeft);
            }

            if (m_antialiasing) {
                QSGGeometry *g = geometry();
                Q_ASSERT(g != &m_geometry);
                g->allocate(8, 14);
                g->setDrawingMode(QSGGeometry::DrawTriangleStrip);
                SmoothVertex *vertices = reinterpret_cast<SmoothVertex *>(g->vertexData());
                float delta = float(qAbs(m_targetRect.width()) < qAbs(m_targetRect.height())
                        ? m_targetRect.width() : m_targetRect.height()) * 0.5f;
                float sx = float(sr.width() / m_targetRect.width());
                float sy = float(sr.height() / m_targetRect.height());
                for (int d = -1; d <= 1; d += 2) {
                    for (int j = 0; j < 2; ++j) {
                        for (int i = 0; i < 2; ++i, ++vertices) {
                            vertices->x = m_targetRect.x() + i * m_targetRect.width();
                            vertices->y = m_targetRect.y() + j * m_targetRect.height();
                            vertices->u = sr.x() + i * sr.width();
                            vertices->v = sr.y() + j * sr.height();
                            vertices->dx = (i == 0 ? delta : -delta) * d;
                            vertices->dy = (j == 0 ? delta : -delta) * d;
                            vertices->du = (d < 0 ? 0 : vertices->dx * sx);
                            vertices->dv = (d < 0 ? 0 : vertices->dy * sy);
                        }
                    }
                }
                Q_ASSERT(vertices - g->vertexCount() == g->vertexData());
                static const quint16 indices[] = {
                    0, 4, 1, 5, 3, 7, 2, 6, 0, 4,
                    4, 6, 5, 7
                };
                Q_ASSERT(g->sizeOfIndex() * g->indexCount() == sizeof(indices));
                memcpy(g->indexDataAsUShort(), indices, sizeof(indices));
            } else {
                m_geometry.allocate(4);
                m_geometry.setDrawingMode(QSGGeometry::DrawTriangleStrip);
                QSGGeometry::updateTexturedRectGeometry(&m_geometry, m_targetRect, sr);
            }
        } else {
            QSGGeometry *g = m_antialiasing ? geometry() : &m_geometry;
            updateGeometry(m_targetRect, m_innerTargetRect,
                           sourceRect, innerSourceRect, m_subSourceRect,
                           g, m_mirror, m_antialiasing);
        }
    }
    markDirty(DirtyGeometry);
    m_dirtyGeometry = false;
}

QT_END_NAMESPACE
