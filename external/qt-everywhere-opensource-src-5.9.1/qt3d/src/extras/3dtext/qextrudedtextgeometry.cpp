/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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

#include "qextrudedtextgeometry.h"
#include "qextrudedtextgeometry_p.h"
#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/qbufferdatagenerator.h>
#include <Qt3DRender/qattribute.h>
#include <private/qtriangulator_p.h>
#include <qmath.h>
#include <QVector3D>
#include <QTextLayout>
#include <QTime>

QT_BEGIN_NAMESPACE

namespace Qt3DExtras {

namespace {

static float edgeSplitAngle = 90.f * 0.1f;

using IndexType = unsigned int;

struct TriangulationData {
    struct Outline {
        int begin;
        int end;
    };

    QVector<QVector3D> vertices;
    QVector<IndexType> indices;
    QVector<Outline> outlines;
    QVector<IndexType> outlineIndices;
    bool inverted;
};

TriangulationData triangulate(const QString &text, const QFont &font)
{
    TriangulationData result;
    int beginOutline = 0;

    // Initialize path with text and extract polygons
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    path.addText(0, 0, font, text);
    QList<QPolygonF> polygons = path.toSubpathPolygons(QTransform().scale(1.f, -1.f));

    // maybe glyph has no geometry
    if (polygons.size() == 0)
        return result;

    const int prevNumIndices = result.indices.size();

    // Reset path and add previously extracted polygons (which where spatially transformed)
    path = QPainterPath();
    path.setFillRule(Qt::WindingFill);
    for (QPolygonF &p : polygons)
        path.addPolygon(p);

    // Extract polylines out of the path, this allows us to retrieve indices for each glyph outline
    QPolylineSet polylines = qPolyline(path);
    QVector<IndexType> tmpIndices;
    tmpIndices.resize(polylines.indices.size());
    memcpy(tmpIndices.data(), polylines.indices.data(), polylines.indices.size() * sizeof(IndexType));

    int lastIndex = 0;
    for (const IndexType idx : tmpIndices) {
        if (idx == std::numeric_limits<IndexType>::max()) {
            const int endOutline = lastIndex;
            result.outlines.push_back({beginOutline, endOutline});
            beginOutline = endOutline;
        } else {
            result.outlineIndices.push_back(idx);
            ++lastIndex;
        }
    }

    // Triangulate path
    const QTriangleSet triangles = qTriangulate(path);

    // Append new indices to result.indices buffer
    result.indices.resize(result.indices.size() + triangles.indices.size());
    memcpy(&result.indices[prevNumIndices], triangles.indices.data(), triangles.indices.size() * sizeof(IndexType));
    for (int i = prevNumIndices, m = result.indices.size(); i < m; ++i)
        result.indices[i] += result.vertices.size();

    // Append new triangles to result.vertices
    result.vertices.reserve(triangles.vertices.size() / 2);
    for (int i = 0, m = triangles.vertices.size(); i < m; i += 2)
        result.vertices.push_back(QVector3D(triangles.vertices[i] / font.pointSizeF(), triangles.vertices[i + 1] / font.pointSizeF(), 0.0f));

    return result;
}

inline QVector3D mix(const QVector3D &a, const QVector3D &b, float ratio)
{
    return a + (b - a) * ratio;
}

} // anonymous namespace

QExtrudedTextGeometryPrivate::QExtrudedTextGeometryPrivate()
    : QGeometryPrivate()
    , m_font(QFont(QStringLiteral("Arial")))
    , m_depth(1.f)
    , m_positionAttribute(nullptr)
    , m_normalAttribute(nullptr)
    , m_indexAttribute(nullptr)
    , m_vertexBuffer(nullptr)
    , m_indexBuffer(nullptr)
{
    m_font.setPointSize(4);
}

void QExtrudedTextGeometryPrivate::init()
{
    Q_Q(QExtrudedTextGeometry);
    m_positionAttribute = new Qt3DRender::QAttribute(q);
    m_normalAttribute = new Qt3DRender::QAttribute(q);
    m_indexAttribute = new Qt3DRender::QAttribute(q);
    m_vertexBuffer = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::VertexBuffer, q);
    m_indexBuffer = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::IndexBuffer, q);

    const quint32 elementSize = 3 + 3;
    const quint32 stride = elementSize * sizeof(float);

    m_positionAttribute->setName(Qt3DRender::QAttribute::defaultPositionAttributeName());
    m_positionAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
    m_positionAttribute->setVertexSize(3);
    m_positionAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
    m_positionAttribute->setBuffer(m_vertexBuffer);
    m_positionAttribute->setByteStride(stride);
    m_positionAttribute->setByteOffset(0);
    m_positionAttribute->setCount(0);

    m_normalAttribute->setName(Qt3DRender::QAttribute::defaultNormalAttributeName());
    m_normalAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
    m_normalAttribute->setVertexSize(3);
    m_normalAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
    m_normalAttribute->setBuffer(m_vertexBuffer);
    m_normalAttribute->setByteStride(stride);
    m_normalAttribute->setByteOffset(3 * sizeof(float));
    m_normalAttribute->setCount(0);

    m_indexAttribute->setAttributeType(Qt3DRender::QAttribute::IndexAttribute);
    m_indexAttribute->setVertexBaseType(Qt3DRender::QAttribute::UnsignedInt);
    m_indexAttribute->setBuffer(m_indexBuffer);
    m_indexAttribute->setCount(0);

    q->addAttribute(m_positionAttribute);
    q->addAttribute(m_normalAttribute);
    q->addAttribute(m_indexAttribute);

    update();
}

/*!
 * \qmltype ExtrudedTextGeometry
 * \instantiates Qt3DExtras::QExtrudedTextGeometry
 * \inqmlmodule Qt3D.Extras
 * \brief ExtrudedTextGeometry allows creation of a 3D text in 3D space.
 *
 * The ExtrudedTextGeometry type is most commonly used internally by the
 * ExtrudedTextMesh type but can also be used in custom GeometryRenderer types.
 */

/*!
 * \qmlproperty QString ExtrudedTextGeometry::text
 *
 * Holds the text used for the mesh.
 */

/*!
 * \qmlproperty QFont ExtrudedTextGeometry::font
 *
 * Holds the font of the text.
 */

/*!
 * \qmlproperty float ExtrudedTextGeometry::depth
 *
 * Holds the extrusion depth of the text.
 */

/*!
 * \qmlproperty Attribute ExtrudedTextGeometry::positionAttribute
 *
 * Holds the geometry position attribute.
 */

/*!
 * \qmlproperty Attribute ExtrudedTextGeometry::normalAttribute
 *
 * Holds the geometry normal attribute.
 */

/*!
 * \qmlproperty Attribute ExtrudedTextGeometry::indexAttribute
 *
 * Holds the geometry index attribute.
 */

/*!
 * \class Qt3DExtras::QExtrudedTextGeometry
 * \inheaderfile Qt3DExtras/QExtrudedTextGeometry
 * \inmodule Qt3DExtras
 * \brief The QExtrudedTextGeometry class allows creation of a 3D extruded text
 * in 3D space.
 * \since 5.9
 * \ingroup geometries
 * \inherits Qt3DRender::QGeometry
 *
 * The QExtrudedTextGeometry class is most commonly used internally by the QText3DMesh
 * but can also be used in custom Qt3DRender::QGeometryRenderer subclasses.
 */

/*!
 * Constructs a new QExtrudedTextGeometry with \a parent.
 */
QExtrudedTextGeometry::QExtrudedTextGeometry(Qt3DCore::QNode *parent)
    : QGeometry(*new QExtrudedTextGeometryPrivate(), parent)
{
    Q_D(QExtrudedTextGeometry);
    d->init();
}

/*!
 * \internal
 */
QExtrudedTextGeometry::QExtrudedTextGeometry(QExtrudedTextGeometryPrivate &dd, Qt3DCore::QNode *parent)
    : QGeometry(dd, parent)
{
    Q_D(QExtrudedTextGeometry);
    d->init();
}

/*!
 * \internal
 */
QExtrudedTextGeometry::~QExtrudedTextGeometry()
{}

/*!
 * \internal
 * Updates vertices based on text, font, extrusionLength and smoothAngle properties.
 */
void QExtrudedTextGeometryPrivate::update()
{
    if (m_text.trimmed().isEmpty()) // save enough?
        return;

    TriangulationData data = triangulate(m_text, m_font);

    const int numVertices = data.vertices.size();
    const int numIndices = data.indices.size();

    struct Vertex {
        QVector3D position;
        QVector3D normal;
    };

    QVector<IndexType> indices;
    QVector<Vertex> vertices;

    // TODO: keep 'vertices.size()' small when extruding
    vertices.reserve(data.vertices.size() * 2);
    for (QVector3D &v : data.vertices) // front face
        vertices.push_back({ v, // vertex
                             QVector3D(0.0f, 0.0f, -1.0f) }); // normal
    for (QVector3D &v : data.vertices) // front face
        vertices.push_back({ QVector3D(v.x(), v.y(), m_depth), // vertex
                             QVector3D(0.0f, 0.0f, 1.0f) }); // normal

    for (int i = 0, verticesIndex = vertices.size(); i < data.outlines.size(); ++i) {
        const int begin = data.outlines[i].begin;
        const int end = data.outlines[i].end;
        const int verticesIndexBegin = verticesIndex;

        if (begin == end)
            continue;

        QVector3D prevNormal = QVector3D::crossProduct(
                    vertices[data.outlineIndices[end - 1] + numVertices].position - vertices[data.outlineIndices[end - 1]].position,
                vertices[data.outlineIndices[begin]].position - vertices[data.outlineIndices[end - 1]].position).normalized();

        for (int j = begin; j < end; ++j) {
            const bool isLastIndex = (j == end - 1);
            const IndexType cur = data.outlineIndices[j];
            const IndexType next = data.outlineIndices[((j - begin + 1) % (end - begin)) + begin]; // normalize, bring in range and adjust
            const QVector3D normal = QVector3D::crossProduct(vertices[cur + numVertices].position - vertices[cur].position, vertices[next].position - vertices[cur].position).normalized();

            // use smooth normals in case of a short angle
            const bool smooth = QVector3D::dotProduct(prevNormal, normal) > (90.0f - edgeSplitAngle) / 90.0f;
            const QVector3D resultNormal = smooth ? mix(prevNormal, normal, 0.5f) : normal;
            if (!smooth)             {
                vertices.push_back({vertices[cur].position,               prevNormal});
                vertices.push_back({vertices[cur + numVertices].position, prevNormal});
                verticesIndex += 2;
            }

            vertices.push_back({vertices[cur].position,               resultNormal});
            vertices.push_back({vertices[cur + numVertices].position, resultNormal});

            const int v0 = verticesIndex;
            const int v1 = verticesIndex + 1;
            const int v2 = isLastIndex ? verticesIndexBegin     : verticesIndex + 2;
            const int v3 = isLastIndex ? verticesIndexBegin + 1 : verticesIndex + 3;

            indices.push_back(v0);
            indices.push_back(v1);
            indices.push_back(v2);
            indices.push_back(v2);
            indices.push_back(v1);
            indices.push_back(v3);

            verticesIndex += 2;
            prevNormal = normal;
        }
    }

    { // upload vertices
        QByteArray data;
        data.resize(vertices.size() * sizeof(Vertex));
        memcpy(data.data(), vertices.data(), vertices.size() * sizeof(Vertex));

        m_vertexBuffer->setData(data);
        m_positionAttribute->setCount(vertices.size());
        m_normalAttribute->setCount(vertices.size());
    }

    // resize for following insertions
    const int indicesOffset = indices.size();
    indices.resize(indices.size() + numIndices * 2);

    // copy values for back faces
    IndexType *indicesFaces = indices.data() + indicesOffset;
    memcpy(indicesFaces, data.indices.data(), numIndices * sizeof(IndexType));

    // insert values for front face and flip triangles
    for (int j = 0; j < numIndices; j += 3)
    {
        indicesFaces[numIndices + j    ] = indicesFaces[j    ] + numVertices;
        indicesFaces[numIndices + j + 1] = indicesFaces[j + 2] + numVertices;
        indicesFaces[numIndices + j + 2] = indicesFaces[j + 1] + numVertices;
    }

    { // upload indices
        QByteArray data;
        data.resize(indices.size() * sizeof(IndexType));
        memcpy(data.data(), indices.data(), indices.size() * sizeof(IndexType));

        m_indexBuffer->setData(data);
        m_indexAttribute->setCount(indices.size());
    }
}

void QExtrudedTextGeometry::setText(const QString &text)
{
    Q_D(QExtrudedTextGeometry);
    if (d->m_text != text) {
        d->m_text = text;
        d->update();
        emit textChanged(text);
    }
}

void QExtrudedTextGeometry::setFont(const QFont &font)
{
    Q_D(QExtrudedTextGeometry);
    if (d->m_font != font) {
        d->m_font = font;
        d->update();
        emit fontChanged(font);
    }
}

void QExtrudedTextGeometry::setDepth(float depth)
{
    Q_D(QExtrudedTextGeometry);
    if (d->m_depth != depth) {
        d->m_depth = depth;
        d->update();
        emit depthChanged(depth);
    }
}

/*!
 * \property QString QExtrudedTextGeometry::text
 *
 * Holds the text used for the mesh.
 */
QString QExtrudedTextGeometry::text() const
{
    Q_D(const QExtrudedTextGeometry);
    return d->m_text;
}

/*!
 * \property QFont QExtrudedTextGeometry::font
 *
 * Holds the font of the text.
 */
QFont QExtrudedTextGeometry::font() const
{
    Q_D(const QExtrudedTextGeometry);
    return d->m_font;
}

/*!
 * \property float QExtrudedTextGeometry::extrusionLength
 *
 * Holds the extrusion length of the text.
 */
float QExtrudedTextGeometry::extrusionLength() const
{
    Q_D(const QExtrudedTextGeometry);
    return d->m_depth;
}

/*!
 * \property QExtrudedTextGeometry::positionAttribute
 *
 * Holds the geometry position attribute.
 */
Qt3DRender::QAttribute *QExtrudedTextGeometry::positionAttribute() const
{
    Q_D(const QExtrudedTextGeometry);
    return d->m_positionAttribute;
}

/*!
 * \property QExtrudedTextMesh::normalAttribute
 *
 * Holds the geometry normal attribute.
 */
Qt3DRender::QAttribute *QExtrudedTextGeometry::normalAttribute() const
{
    Q_D(const QExtrudedTextGeometry);
    return d->m_normalAttribute;
}

/*!
 * \property QExtrudedTextMesh::indexAttribute
 *
 * Holds the geometry index attribute.
 */
Qt3DRender::QAttribute *QExtrudedTextGeometry::indexAttribute() const
{
    Q_D(const QExtrudedTextGeometry);
    return d->m_indexAttribute;
}

} // Qt3DExtras

QT_END_NAMESPACE
