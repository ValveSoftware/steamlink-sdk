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

#include "qsgbasicinternalrectanglenode_p.h"

#include <QtCore/qmath.h>

QT_BEGIN_NAMESPACE

namespace
{
    struct Color4ub
    {
        unsigned char r, g, b, a;
    };

    Color4ub operator *(Color4ub c, float t) { c.a *= t; c.r *= t; c.g *= t; c.b *= t; return c; }
    Color4ub operator +(Color4ub a, Color4ub b) {  a.a += b.a; a.r += b.r; a.g += b.g; a.b += b.b; return a; }

    inline Color4ub colorToColor4ub(const QColor &c)
    {
        Color4ub color = { uchar(qRound(c.redF() * c.alphaF() * 255)),
                           uchar(qRound(c.greenF() * c.alphaF() * 255)),
                           uchar(qRound(c.blueF() * c.alphaF() * 255)),
                           uchar(qRound(c.alphaF() * 255))
                         };
        return color;
    }

    // Same layout as QSGGeometry::ColoredPoint2D, but uses Color4ub for convenience.
    struct Vertex
    {
        float x, y;
        Color4ub color;
        void set(float nx, float ny, Color4ub ncolor)
        {
            x = nx; y = ny; color = ncolor;
        }
    };

    struct SmoothVertex : public Vertex
    {
        float dx, dy;
        void set(float nx, float ny, Color4ub ncolor, float ndx, float ndy)
        {
            Vertex::set(nx, ny, ncolor);
            dx = ndx; dy = ndy;
        }
    };

    const QSGGeometry::AttributeSet &smoothAttributeSet()
    {
        static QSGGeometry::Attribute data[] = {
            QSGGeometry::Attribute::createWithAttributeType(0, 2, QSGGeometry::FloatType, QSGGeometry::PositionAttribute),
            QSGGeometry::Attribute::createWithAttributeType(1, 4, QSGGeometry::UnsignedByteType, QSGGeometry::ColorAttribute),
            QSGGeometry::Attribute::createWithAttributeType(2, 2, QSGGeometry::FloatType, QSGGeometry::TexCoordAttribute)
        };
        static QSGGeometry::AttributeSet attrs = { 3, sizeof(SmoothVertex), data };
        return attrs;
    }
}

QSGBasicInternalRectangleNode::QSGBasicInternalRectangleNode()
    : m_radius(0)
    , m_pen_width(0)
    , m_aligned(true)
    , m_antialiasing(false)
    , m_gradient_is_opaque(true)
    , m_dirty_geometry(false)
    , m_geometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0)
{
    setGeometry(&m_geometry);

#ifdef QSG_RUNTIME_DESCRIPTION
    qsgnode_set_description(this, QLatin1String("internalrectangle"));
#endif
}

void QSGBasicInternalRectangleNode::setRect(const QRectF &rect)
{
    if (rect == m_rect)
        return;
    m_rect = rect;
    m_dirty_geometry = true;
}

void QSGBasicInternalRectangleNode::setColor(const QColor &color)
{
    if (color == m_color)
        return;
    m_color = color;
    if (m_gradient_stops.isEmpty())
        m_dirty_geometry = true;
}

void QSGBasicInternalRectangleNode::setPenColor(const QColor &color)
{
    if (color == m_border_color)
        return;
    m_border_color = color;
    if (m_pen_width > 0)
        m_dirty_geometry = true;
}

void QSGBasicInternalRectangleNode::setPenWidth(qreal width)
{
    if (width == m_pen_width)
        return;
    m_pen_width = width;
    m_dirty_geometry = true;
}


void QSGBasicInternalRectangleNode::setGradientStops(const QGradientStops &stops)
{
    if (stops.constData() == m_gradient_stops.constData())
        return;

    m_gradient_stops = stops;

    m_gradient_is_opaque = true;
    for (int i = 0; i < stops.size(); ++i)
        m_gradient_is_opaque &= stops.at(i).second.alpha() == 0xff;
    m_dirty_geometry = true;
}

void QSGBasicInternalRectangleNode::setRadius(qreal radius)
{
    if (radius == m_radius)
        return;
    m_radius = radius;
    m_dirty_geometry = true;
}

void QSGBasicInternalRectangleNode::setAntialiasing(bool antialiasing)
{
    if (!supportsAntialiasing())
        return;

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
    m_dirty_geometry = true;
}

void QSGBasicInternalRectangleNode::setAligned(bool aligned)
{
    if (aligned == m_aligned)
        return;
    m_aligned = aligned;
    m_dirty_geometry = true;
}

void QSGBasicInternalRectangleNode::update()
{
    if (m_dirty_geometry) {
        updateGeometry();
        m_dirty_geometry = false;

        QSGNode::DirtyState state = QSGNode::DirtyGeometry;
        updateMaterialBlending(&state);
        markDirty(state);
    }
}

void QSGBasicInternalRectangleNode::updateGeometry()
{
    float width = float(m_rect.width());
    float height = float(m_rect.height());
    float penWidth = qMin(qMin(width, height) * 0.5f, float(m_pen_width));

    if (m_aligned)
        penWidth = qRound(penWidth);

    QSGGeometry *g = geometry();
    g->setDrawingMode(QSGGeometry::DrawTriangleStrip);
    int vertexStride = g->sizeOfVertex();

    union {
        Vertex *vertices;
        SmoothVertex *smoothVertices;
    };

    Color4ub fillColor = colorToColor4ub(m_color);
    Color4ub borderColor = colorToColor4ub(m_border_color);
    Color4ub transparent = { 0, 0, 0, 0 };
    const QGradientStops &stops = m_gradient_stops;

    int nextGradientStop = 0;
    float gradientPos = penWidth / height;
    while (nextGradientStop < stops.size() && stops.at(nextGradientStop).first <= gradientPos)
        ++nextGradientStop;
    int lastGradientStop = stops.size() - 1;
    float lastGradientPos = 1.0f - penWidth / height;
    while (lastGradientStop >= nextGradientStop && stops.at(lastGradientStop).first >= lastGradientPos)
        --lastGradientStop;
    int gradientIntersections = (lastGradientStop - nextGradientStop + 1);

    if (m_radius > 0) {
        // Rounded corners.

        // Radius should never exceeds half of the width or half of the height
        float radius = qMin(qMin(width, height) * 0.5f, float(m_radius));
        QRectF innerRect = m_rect;
        innerRect.adjust(radius, radius, -radius, -radius);

        float innerRadius = radius - penWidth * 1.0f;
        float outerRadius = radius;
        float delta = qMin(width, height) * 0.5f;

        // Number of segments per corner, approximately one per 3 pixels.
        int segments = qBound(3, qCeil(outerRadius * (M_PI / 6)), 18);

        /*

        --+--__
        --+--__--__
          |    --__--__
          |  seg   --__--+
        --+-__  ment  _+  \
        --+-__--__   -  \  \
              --__--+ se \  \
                  +  \  g \  \
                   \  \  m \  \
         -----------+--+  e \  \     <- gradient line
                     \  \  nt\  \
           fill       +--+----+--+
                      |  |    |  |
                         border
                  inner AA    outer AA (AA = antialiasing)

        */

        int innerVertexCount = (segments + 1) * 4 + gradientIntersections * 2;
        int outerVertexCount = (segments + 1) * 4;
        int vertexCount = innerVertexCount;
        if (m_antialiasing || penWidth)
            vertexCount += innerVertexCount;
        if (penWidth)
            vertexCount += outerVertexCount;
        if (m_antialiasing && penWidth)
            vertexCount += outerVertexCount;

        int fillIndexCount = innerVertexCount;
        int innerAAIndexCount = innerVertexCount * 2 + 2;
        int borderIndexCount = innerVertexCount * 2 + 2;
        int outerAAIndexCount = outerVertexCount * 2 + 2;
        int indexCount = 0;
        int fillHead = 0;
        int innerAAHead = 0;
        int innerAATail = 0;
        int borderHead = 0;
        int borderTail = 0;
        int outerAAHead = 0;
        int outerAATail = 0;
        bool hasFill = m_color.alpha() > 0 || !stops.isEmpty();
        if (hasFill)
            indexCount += fillIndexCount;
        if (m_antialiasing) {
            innerAATail = innerAAHead = indexCount + (innerAAIndexCount >> 1) + 1;
            indexCount += innerAAIndexCount;
        }
        if (penWidth) {
            borderTail = borderHead = indexCount + (borderIndexCount >> 1) + 1;
            indexCount += borderIndexCount;
        }
        if (m_antialiasing && penWidth) {
            outerAATail = outerAAHead = indexCount + (outerAAIndexCount >> 1) + 1;
            indexCount += outerAAIndexCount;
        }

        g->allocate(vertexCount, indexCount);
        vertices = reinterpret_cast<Vertex *>(g->vertexData());
        memset(vertices, 0, vertexCount * vertexStride);
        quint16 *indices = g->indexDataAsUShort();
        quint16 index = 0;

        float py = 0; // previous inner y-coordinate.
        float plx = 0; // previous inner left x-coordinate.
        float prx = 0; // previous inner right x-coordinate.

        float angle = 0.5f * float(M_PI) / segments;
        float cosStep = qFastCos(angle);
        float sinStep = qFastSin(angle);

        for (int part = 0; part < 2; ++part) {
            float c = 1 - part;
            float s = part;
            for (int i = 0; i <= segments; ++i) {
                float y, lx, rx;
                if (innerRadius > 0) {
                    y = (part ? innerRect.bottom() : innerRect.top()) - innerRadius * c; // current inner y-coordinate.
                    lx = innerRect.left() - innerRadius * s; // current inner left x-coordinate.
                    rx = innerRect.right() + innerRadius * s; // current inner right x-coordinate.
                    gradientPos = ((part ? innerRect.height() : 0) + radius - innerRadius * c) / height;
                } else {
                    y = (part ? innerRect.bottom() + innerRadius : innerRect.top() - innerRadius); // current inner y-coordinate.
                    lx = innerRect.left() - innerRadius; // current inner left x-coordinate.
                    rx = innerRect.right() + innerRadius; // current inner right x-coordinate.
                    gradientPos = ((part ? innerRect.height() + innerRadius : -innerRadius) + radius) / height;
                }
                float Y = (part ? innerRect.bottom() : innerRect.top()) - outerRadius * c; // current outer y-coordinate.
                float lX = innerRect.left() - outerRadius * s; // current outer left x-coordinate.
                float rX = innerRect.right() + outerRadius * s; // current outer right x-coordinate.

                while (nextGradientStop <= lastGradientStop && stops.at(nextGradientStop).first <= gradientPos) {
                    // Insert vertices at gradient stops.
                    float gy = (innerRect.top() - radius) + stops.at(nextGradientStop).first * height;
                    float t = (gy - py) / (y - py);
                    float glx = plx * (1 - t) + t * lx;
                    float grx = prx * (1 - t) + t * rx;

                    fillColor = colorToColor4ub(stops.at(nextGradientStop).second);

                    if (hasFill) {
                        indices[fillHead++] = index;
                        indices[fillHead++] = index + 1;
                    }

                    if (penWidth) {
                        --borderHead;
                        indices[borderHead] = indices[borderHead + 2];
                        indices[--borderHead] = index + 2;
                        indices[borderTail++] = index + 3;
                        indices[borderTail] = indices[borderTail - 2];
                        ++borderTail;
                    }

                    if (m_antialiasing) {
                        indices[--innerAAHead] = index + 2;
                        indices[--innerAAHead] = index;
                        indices[innerAATail++] = index + 1;
                        indices[innerAATail++] = index + 3;

                        bool lower = stops.at(nextGradientStop).first > 0.5f;
                        float dy = lower ? qMin(0.0f, height - gy - delta) : qMax(0.0f, delta - gy);
                        smoothVertices[index++].set(grx, gy, fillColor, width - grx - delta, dy);
                        smoothVertices[index++].set(glx, gy, fillColor, delta - glx, dy);
                        if (penWidth) {
                            smoothVertices[index++].set(grx, gy, borderColor, 0.49f * penWidth * s, -0.49f * penWidth * c);
                            smoothVertices[index++].set(glx, gy, borderColor, -0.49f * penWidth * s, -0.49f * penWidth * c);
                        } else {
                            dy = lower ? delta : -delta;
                            smoothVertices[index++].set(grx, gy, transparent, delta, dy);
                            smoothVertices[index++].set(glx, gy, transparent, -delta, dy);
                        }
                    } else {
                        vertices[index++].set(grx, gy, fillColor);
                        vertices[index++].set(glx, gy, fillColor);
                        if (penWidth) {
                            vertices[index++].set(grx, gy, borderColor);
                            vertices[index++].set(glx, gy, borderColor);
                        }
                    }
                    ++nextGradientStop;
                }

                if (!stops.isEmpty()) {
                    if (nextGradientStop == 0) {
                        fillColor = colorToColor4ub(stops.at(0).second);
                    } else if (nextGradientStop == stops.size()) {
                        fillColor = colorToColor4ub(stops.last().second);
                    } else {
                        const QGradientStop &prev = stops.at(nextGradientStop - 1);
                        const QGradientStop &next = stops.at(nextGradientStop);
                        float t = (gradientPos - prev.first) / (next.first - prev.first);
                        fillColor = colorToColor4ub(prev.second) * (1 - t) + colorToColor4ub(next.second) * t;
                    }
                }

                if (hasFill) {
                    indices[fillHead++] = index;
                    indices[fillHead++] = index + 1;
                }

                if (penWidth) {
                    indices[--borderHead] = index + 4;
                    indices[--borderHead] = index + 2;
                    indices[borderTail++] = index + 3;
                    indices[borderTail++] = index + 5;
                }

                if (m_antialiasing) {
                    indices[--innerAAHead] = index + 2;
                    indices[--innerAAHead] = index;
                    indices[innerAATail++] = index + 1;
                    indices[innerAATail++] = index + 3;

                    float dy = part ? qMin(0.0f, height - y - delta) : qMax(0.0f, delta - y);
                    smoothVertices[index++].set(rx, y, fillColor, width - rx - delta, dy);
                    smoothVertices[index++].set(lx, y, fillColor, delta - lx, dy);

                    dy = part ? delta : -delta;
                    if (penWidth) {
                        smoothVertices[index++].set(rx, y, borderColor, 0.49f * penWidth * s, -0.49f * penWidth * c);
                        smoothVertices[index++].set(lx, y, borderColor, -0.49f * penWidth * s, -0.49f * penWidth * c);
                        smoothVertices[index++].set(rX, Y, borderColor, -0.49f * penWidth * s, 0.49f * penWidth * c);
                        smoothVertices[index++].set(lX, Y, borderColor, 0.49f * penWidth * s, 0.49f * penWidth * c);
                        smoothVertices[index++].set(rX, Y, transparent, delta, dy);
                        smoothVertices[index++].set(lX, Y, transparent, -delta, dy);

                        indices[--outerAAHead] = index - 2;
                        indices[--outerAAHead] = index - 4;
                        indices[outerAATail++] = index - 3;
                        indices[outerAATail++] = index - 1;
                    } else {
                        smoothVertices[index++].set(rx, y, transparent, delta, dy);
                        smoothVertices[index++].set(lx, y, transparent, -delta, dy);
                    }
                } else {
                    vertices[index++].set(rx, y, fillColor);
                    vertices[index++].set(lx, y, fillColor);
                    if (penWidth) {
                        vertices[index++].set(rx, y, borderColor);
                        vertices[index++].set(lx, y, borderColor);
                        vertices[index++].set(rX, Y, borderColor);
                        vertices[index++].set(lX, Y, borderColor);
                    }
                }

                py = y;
                plx = lx;
                prx = rx;

                // Rotate
                qreal tmp = c;
                c = c * cosStep - s * sinStep;
                s = s * cosStep + tmp * sinStep;
            }
        }
        Q_ASSERT(index == vertexCount);

        // Close the triangle strips.
        if (m_antialiasing) {
            indices[--innerAAHead] = indices[innerAATail - 1];
            indices[--innerAAHead] = indices[innerAATail - 2];
            Q_ASSERT(innerAATail <= indexCount);
        }
        if (penWidth) {
            indices[--borderHead] = indices[borderTail - 1];
            indices[--borderHead] = indices[borderTail - 2];
            Q_ASSERT(borderTail <= indexCount);
        }
        if (m_antialiasing && penWidth) {
            indices[--outerAAHead] = indices[outerAATail - 1];
            indices[--outerAAHead] = indices[outerAATail - 2];
            Q_ASSERT(outerAATail == indexCount);
        }
    } else {
        // Straight corners.
        QRectF innerRect = m_rect;
        QRectF outerRect = m_rect;

        if (penWidth)
            innerRect.adjust(1.0f * penWidth, 1.0f * penWidth, -1.0f * penWidth, -1.0f * penWidth);

        float delta = qMin(width, height) * 0.5f;
        int innerVertexCount = 4 + gradientIntersections * 2;
        int outerVertexCount = 4;
        int vertexCount = innerVertexCount;
        if (m_antialiasing || penWidth)
            vertexCount += innerVertexCount;
        if (penWidth)
            vertexCount += outerVertexCount;
        if (m_antialiasing && penWidth)
            vertexCount += outerVertexCount;

        int fillIndexCount = innerVertexCount;
        int innerAAIndexCount = innerVertexCount * 2 + 2;
        int borderIndexCount = innerVertexCount * 2 + 2;
        int outerAAIndexCount = outerVertexCount * 2 + 2;
        int indexCount = 0;
        int fillHead = 0;
        int innerAAHead = 0;
        int innerAATail = 0;
        int borderHead = 0;
        int borderTail = 0;
        int outerAAHead = 0;
        int outerAATail = 0;
        bool hasFill = m_color.alpha() > 0 || !stops.isEmpty();
        if (hasFill)
            indexCount += fillIndexCount;
        if (m_antialiasing) {
            innerAATail = innerAAHead = indexCount + (innerAAIndexCount >> 1) + 1;
            indexCount += innerAAIndexCount;
        }
        if (penWidth) {
            borderTail = borderHead = indexCount + (borderIndexCount >> 1) + 1;
            indexCount += borderIndexCount;
        }
        if (m_antialiasing && penWidth) {
            outerAATail = outerAAHead = indexCount + (outerAAIndexCount >> 1) + 1;
            indexCount += outerAAIndexCount;
        }

        g->allocate(vertexCount, indexCount);
        vertices = reinterpret_cast<Vertex *>(g->vertexData());
        memset(vertices, 0, vertexCount * vertexStride);
        quint16 *indices = g->indexDataAsUShort();
        quint16 index = 0;

        float lx = innerRect.left();
        float rx = innerRect.right();
        float lX = outerRect.left();
        float rX = outerRect.right();

        for (int part = -1; part <= 1; part += 2) {
            float y = (part == 1 ? innerRect.bottom() : innerRect.top());
            float Y = (part == 1 ? outerRect.bottom() : outerRect.top());
            gradientPos = (y - innerRect.top() + penWidth) / height;

            while (nextGradientStop <= lastGradientStop && stops.at(nextGradientStop).first <= gradientPos) {
                // Insert vertices at gradient stops.
                float gy = (innerRect.top() - penWidth) + stops.at(nextGradientStop).first * height;

                fillColor = colorToColor4ub(stops.at(nextGradientStop).second);

                if (hasFill) {
                    indices[fillHead++] = index;
                    indices[fillHead++] = index + 1;
                }

                if (penWidth) {
                    --borderHead;
                    indices[borderHead] = indices[borderHead + 2];
                    indices[--borderHead] = index + 2;
                    indices[borderTail++] = index + 3;
                    indices[borderTail] = indices[borderTail - 2];
                    ++borderTail;
                }

                if (m_antialiasing) {
                    indices[--innerAAHead] = index + 2;
                    indices[--innerAAHead] = index;
                    indices[innerAATail++] = index + 1;
                    indices[innerAATail++] = index + 3;

                    bool lower = stops.at(nextGradientStop).first > 0.5f;
                    float dy = lower ? qMin(0.0f, height - gy - delta) : qMax(0.0f, delta - gy);
                    smoothVertices[index++].set(rx, gy, fillColor, width - rx - delta, dy);
                    smoothVertices[index++].set(lx, gy, fillColor, delta - lx, dy);
                    if (penWidth) {
                        smoothVertices[index++].set(rx, gy, borderColor, 0.49f * penWidth, (lower ? 0.49f : -0.49f) * penWidth);
                        smoothVertices[index++].set(lx, gy, borderColor, -0.49f * penWidth, (lower ? 0.49f : -0.49f) * penWidth);
                    } else {
                        smoothVertices[index++].set(rx, gy, transparent, delta, lower ? delta : -delta);
                        smoothVertices[index++].set(lx, gy, transparent, -delta, lower ? delta : -delta);
                    }
                } else {
                    vertices[index++].set(rx, gy, fillColor);
                    vertices[index++].set(lx, gy, fillColor);
                    if (penWidth) {
                        vertices[index++].set(rx, gy, borderColor);
                        vertices[index++].set(lx, gy, borderColor);
                    }
                }
                ++nextGradientStop;
            }

            if (!stops.isEmpty()) {
                if (nextGradientStop == 0) {
                    fillColor = colorToColor4ub(stops.at(0).second);
                } else if (nextGradientStop == stops.size()) {
                    fillColor = colorToColor4ub(stops.last().second);
                } else {
                    const QGradientStop &prev = stops.at(nextGradientStop - 1);
                    const QGradientStop &next = stops.at(nextGradientStop);
                    float t = (gradientPos - prev.first) / (next.first - prev.first);
                    fillColor = colorToColor4ub(prev.second) * (1 - t) + colorToColor4ub(next.second) * t;
                }
            }

            if (hasFill) {
                indices[fillHead++] = index;
                indices[fillHead++] = index + 1;
            }

            if (penWidth) {
                indices[--borderHead] = index + 4;
                indices[--borderHead] = index + 2;
                indices[borderTail++] = index + 3;
                indices[borderTail++] = index + 5;
            }

            if (m_antialiasing) {
                indices[--innerAAHead] = index + 2;
                indices[--innerAAHead] = index;
                indices[innerAATail++] = index + 1;
                indices[innerAATail++] = index + 3;

                float dy = part == 1 ? qMin(0.0f, height - y - delta) : qMax(0.0f, delta - y);
                smoothVertices[index++].set(rx, y, fillColor, width - rx - delta, dy);
                smoothVertices[index++].set(lx, y, fillColor, delta - lx, dy);

                if (penWidth) {
                    smoothVertices[index++].set(rx, y, borderColor, 0.49f * penWidth, 0.49f * penWidth * part);
                    smoothVertices[index++].set(lx, y, borderColor, -0.49f * penWidth, 0.49f * penWidth * part);
                    smoothVertices[index++].set(rX, Y, borderColor, -0.49f * penWidth, -0.49f * penWidth * part);
                    smoothVertices[index++].set(lX, Y, borderColor, 0.49f * penWidth, -0.49f * penWidth * part);
                    smoothVertices[index++].set(rX, Y, transparent, delta, delta * part);
                    smoothVertices[index++].set(lX, Y, transparent, -delta, delta * part);

                    indices[--outerAAHead] = index - 2;
                    indices[--outerAAHead] = index - 4;
                    indices[outerAATail++] = index - 3;
                    indices[outerAATail++] = index - 1;
                } else {
                    smoothVertices[index++].set(rx, y, transparent, delta, delta * part);
                    smoothVertices[index++].set(lx, y, transparent, -delta, delta * part);
                }
            } else {
                vertices[index++].set(rx, y, fillColor);
                vertices[index++].set(lx, y, fillColor);
                if (penWidth) {
                    vertices[index++].set(rx, y, borderColor);
                    vertices[index++].set(lx, y, borderColor);
                    vertices[index++].set(rX, Y, borderColor);
                    vertices[index++].set(lX, Y, borderColor);
                }
            }
        }
        Q_ASSERT(index == vertexCount);

        // Close the triangle strips.
        if (m_antialiasing) {
            indices[--innerAAHead] = indices[innerAATail - 1];
            indices[--innerAAHead] = indices[innerAATail - 2];
            Q_ASSERT(innerAATail <= indexCount);
        }
        if (penWidth) {
            indices[--borderHead] = indices[borderTail - 1];
            indices[--borderHead] = indices[borderTail - 2];
            Q_ASSERT(borderTail <= indexCount);
        }
        if (m_antialiasing && penWidth) {
            indices[--outerAAHead] = indices[outerAATail - 1];
            indices[--outerAAHead] = indices[outerAATail - 2];
            Q_ASSERT(outerAATail == indexCount);
        }
    }
}

QT_END_NAMESPACE
