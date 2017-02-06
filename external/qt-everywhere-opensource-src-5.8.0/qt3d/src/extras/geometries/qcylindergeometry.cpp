/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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

#ifndef _USE_MATH_DEFINES
# define _USE_MATH_DEFINES // For MSVC
#endif

#include "qcylindergeometry.h"
#include "qcylindergeometry_p.h"
#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/qbufferdatagenerator.h>
#include <Qt3DRender/qattribute.h>
#include <qmath.h>
#include <QVector3D>

QT_BEGIN_NAMESPACE

using namespace Qt3DRender;

namespace Qt3DExtras {

namespace {

int faceCount(int slices, int rings)
{
    return (slices * 2) * (rings - 1) // two tris per side, for each pair of adjacent rings
            + slices * 2; // two caps
}

int vertexCount(int slices, int rings)
{
    return (slices + 1) * rings + 2 * (slices + 1) + 2;
}

void createSidesVertices(float *&verticesPtr,
                         int rings,
                         int slices,
                         double radius,
                         double length)
{
    const float dY = length / static_cast<float>(rings - 1);
    const float dTheta = (M_PI * 2) / static_cast<float>(slices);

    for (int ring = 0; ring < rings; ++ring) {
        const float y = -length / 2.0f + static_cast<float>(ring) * dY;

        for (int slice = 0; slice <= slices; ++slice) {
            const float theta = static_cast<float>(slice) * dTheta;
            const float ct = qCos(theta);
            const float st = qSin(theta);

            *verticesPtr++ = radius * ct;
            *verticesPtr++ = y;
            *verticesPtr++ = radius * st;

            *verticesPtr++ = (y + length / 2.0) / length;
            *verticesPtr++ = theta / (M_PI * 2);

            QVector3D n(ct, 0.0f, st);
            n.normalize();
            *verticesPtr++ = n.x();
            *verticesPtr++ = n.y();
            *verticesPtr++ = n.z();
        }
    }
}

void createSidesIndices(quint16 *&indicesPtr, int rings, int slices)
{
    for (int ring = 0; ring < rings - 1; ++ring) {
        const int ringIndexStart = ring * (slices + 1);
        const int nextRingIndexStart = (ring + 1) * (slices + 1);

        for (int slice = 0; slice < slices; ++slice) {
            const int nextSlice = slice + 1;

            *indicesPtr++ = (ringIndexStart + slice);
            *indicesPtr++ = (nextRingIndexStart + slice);
            *indicesPtr++ = (ringIndexStart + nextSlice);
            *indicesPtr++ = (ringIndexStart + nextSlice);
            *indicesPtr++ = (nextRingIndexStart + slice);
            *indicesPtr++ = (nextRingIndexStart + nextSlice);
        }
    }
}

void createDiscVertices(float *&verticesPtr,
                        int slices,
                        double radius,
                        double yPosition)
{
    const float dTheta = (M_PI * 2) / static_cast<float>(slices);
    const double yNormal = (yPosition < 0.0f) ? -1.0f : 1.0f;

    *verticesPtr++ = 0.0f;
    *verticesPtr++ = yPosition;
    *verticesPtr++ = 0.0f;

    *verticesPtr++ = 1.0f;
    *verticesPtr++ = 0.0f;

    *verticesPtr++ = 0.0f;
    *verticesPtr++ = yNormal;
    *verticesPtr++ = 0.0f;

    for (int slice = 0; slice <= slices; ++slice) {
        const float theta = static_cast<float>(slice) * dTheta;
        const float ct = qCos(theta);
        const float st = qSin(theta);

        *verticesPtr++ = radius * ct;
        *verticesPtr++ = yPosition;
        *verticesPtr++ = radius * st;

        *verticesPtr++ = 1.0f;
        *verticesPtr++ = theta / (M_PI * 2);

        *verticesPtr++ = 0.0f;
        *verticesPtr++ = yNormal;
        *verticesPtr++ = 0.0f;
    }
}

void createDiscIndices(quint16 *&indicesPtr,
                       int discCenterIndex,
                       int slices,
                       double yPosition)
{
    const double yNormal = (yPosition < 0.0f) ? -1.0f : 1.0f;

    for (int slice = 0; slice < slices; ++slice) {
        const int nextSlice = slice + 1;

        *indicesPtr++ = discCenterIndex;
        *indicesPtr++ = (discCenterIndex + 1 + nextSlice);
        *indicesPtr++ = (discCenterIndex + 1 + slice);

        if (yNormal < 0.0f)
            qSwap(*(indicesPtr -1), *(indicesPtr - 2));
    }
}

} // anonymous


class CylinderVertexDataFunctor : public QBufferDataGenerator
{
public:
    CylinderVertexDataFunctor(int rings, int slices, float radius, float length)
        : m_rings(rings)
        , m_slices(slices)
        , m_radius(radius)
        , m_length(length)
    {}

    QByteArray operator ()() Q_DECL_OVERRIDE
    {
        const int verticesCount = vertexCount(m_slices, m_rings);
        // vec3 pos, vec2 texCoord, vec3 normal
        const quint32 vertexSize = (3 + 2 + 3) * sizeof(float);

        QByteArray verticesData;
        verticesData.resize(vertexSize * verticesCount);
        float *verticesPtr = reinterpret_cast<float*>(verticesData.data());

        createSidesVertices(verticesPtr, m_rings, m_slices, m_radius, m_length);
        createDiscVertices(verticesPtr, m_slices, m_radius, -m_length * 0.5f);
        createDiscVertices(verticesPtr, m_slices, m_radius, m_length * 0.5f);

        return verticesData;
    }

    bool operator ==(const QBufferDataGenerator &other) const Q_DECL_OVERRIDE
    {
        const CylinderVertexDataFunctor *otherFunctor = functor_cast<CylinderVertexDataFunctor>(&other);
        if (otherFunctor != nullptr)
            return (otherFunctor->m_rings == m_rings &&
                    otherFunctor->m_slices == m_slices &&
                    otherFunctor->m_radius == m_radius &&
                    otherFunctor->m_length == m_length);
        return false;
    }

    QT3D_FUNCTOR(CylinderVertexDataFunctor)

private:
    int m_rings;
    int m_slices;
    float m_radius;
    float m_length;
};

class CylinderIndexDataFunctor : public QBufferDataGenerator
{
public:
    CylinderIndexDataFunctor(int rings, int slices, float length)
        : m_rings(rings)
        , m_slices(slices)
        , m_length(length)
    {
    }

    QByteArray operator ()() Q_DECL_OVERRIDE
    {
        const int facesCount = faceCount(m_slices, m_rings);
        const int indicesCount = facesCount * 3;
        const int indexSize = sizeof(quint16);
        Q_ASSERT(indicesCount < 65536);

        QByteArray indicesBytes;
        indicesBytes.resize(indicesCount * indexSize);
        quint16 *indicesPtr = reinterpret_cast<quint16*>(indicesBytes.data());

        createSidesIndices(indicesPtr, m_rings, m_slices);
        createDiscIndices(indicesPtr, m_rings * (m_slices + 1), m_slices, -m_length * 0.5);
        createDiscIndices(indicesPtr, m_rings * (m_slices + 1) + m_slices + 2, m_slices, m_length * 0.5);
        Q_ASSERT(indicesPtr == (reinterpret_cast<quint16*>(indicesBytes.data()) + indicesCount));

        return indicesBytes;
    }

    bool operator ==(const QBufferDataGenerator &other) const Q_DECL_OVERRIDE
    {
        const CylinderIndexDataFunctor *otherFunctor = functor_cast<CylinderIndexDataFunctor>(&other);
        if (otherFunctor != nullptr)
            return (otherFunctor->m_rings == m_rings &&
                    otherFunctor->m_slices == m_slices &&
                    otherFunctor->m_length == m_length);
        return false;
    }

    QT3D_FUNCTOR(CylinderIndexDataFunctor)

private:
    int m_rings;
    int m_slices;
    float m_length;
};


QCylinderGeometryPrivate::QCylinderGeometryPrivate()
    : QGeometryPrivate()
    , m_rings(16)
    , m_slices(16)
    , m_radius(1.0f)
    , m_length(1.0f)
    , m_positionAttribute(nullptr)
    , m_normalAttribute(nullptr)
    , m_texCoordAttribute(nullptr)
    , m_indexAttribute(nullptr)
    , m_vertexBuffer(nullptr)
    , m_indexBuffer(nullptr)
{
}

void QCylinderGeometryPrivate::init()
{
    Q_Q(QCylinderGeometry);
    m_positionAttribute = new QAttribute(q);
    m_normalAttribute = new QAttribute(q);
    m_texCoordAttribute = new QAttribute(q);
    m_indexAttribute = new QAttribute(q);
    m_vertexBuffer = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::VertexBuffer, q);
    m_indexBuffer = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::IndexBuffer, q);

    // vec3 pos, vec2 tex, vec3 normal
    const quint32 elementSize = 3 + 2 + 3;
    const quint32 stride = elementSize * sizeof(float);
    const int nVerts = vertexCount(m_slices, m_rings);
    const int faces = faceCount(m_slices, m_rings);

    m_positionAttribute->setName(QAttribute::defaultPositionAttributeName());
    m_positionAttribute->setVertexBaseType(QAttribute::Float);
    m_positionAttribute->setVertexSize(3);
    m_positionAttribute->setAttributeType(QAttribute::VertexAttribute);
    m_positionAttribute->setBuffer(m_vertexBuffer);
    m_positionAttribute->setByteStride(stride);
    m_positionAttribute->setCount(nVerts);

    m_texCoordAttribute->setName(QAttribute::defaultTextureCoordinateAttributeName());
    m_texCoordAttribute->setVertexBaseType(QAttribute::Float);
    m_texCoordAttribute->setVertexSize(2);
    m_texCoordAttribute->setAttributeType(QAttribute::VertexAttribute);
    m_texCoordAttribute->setBuffer(m_vertexBuffer);
    m_texCoordAttribute->setByteStride(stride);
    m_texCoordAttribute->setByteOffset(3 * sizeof(float));
    m_texCoordAttribute->setCount(nVerts);

    m_normalAttribute->setName(QAttribute::defaultNormalAttributeName());
    m_normalAttribute->setVertexBaseType(QAttribute::Float);
    m_normalAttribute->setVertexSize(3);
    m_normalAttribute->setAttributeType(QAttribute::VertexAttribute);
    m_normalAttribute->setBuffer(m_vertexBuffer);
    m_normalAttribute->setByteStride(stride);
    m_normalAttribute->setByteOffset(5 * sizeof(float));
    m_normalAttribute->setCount(nVerts);

    m_indexAttribute->setAttributeType(QAttribute::IndexAttribute);
    m_indexAttribute->setVertexBaseType(QAttribute::UnsignedShort);
    m_indexAttribute->setBuffer(m_indexBuffer);

    m_indexAttribute->setCount(faces * 3);

    m_vertexBuffer->setDataGenerator(QSharedPointer<CylinderVertexDataFunctor>::create(m_rings, m_slices, m_radius, m_length));
    m_indexBuffer->setDataGenerator(QSharedPointer<CylinderIndexDataFunctor>::create(m_rings, m_slices, m_length));

    q->addAttribute(m_positionAttribute);
    q->addAttribute(m_texCoordAttribute);
    q->addAttribute(m_normalAttribute);
    q->addAttribute(m_indexAttribute);
}

/*!
 * \qmltype CylinderGeometry
 * \instantiates Qt3DExtras::QCylinderGeometry
 * \inqmlmodule Qt3D.Extras
 * \brief CylinderGeometry allows creation of a cylinder in 3D space.
 *
 * The CylinderGeometry type is most commonly used internally by the CylinderMesh type
 * but can also be used in custom GeometryRenderer types.
 */

/*!
 * \qmlproperty int CylinderGeometry::rings
 *
 * Holds the number of rings in the cylinder.
 */

/*!
 * \qmlproperty int CylinderGeometry::slices
 *
 * Holds the number of slices in the cylinder.
 */

/*!
 * \qmlproperty real CylinderGeometry::radius
 *
 * Holds the radius of the cylinder.
 */

/*!
 * \qmlproperty real CylinderGeometry::length
 *
 * Holds the length of the cylinder.
 */

/*!
 * \qmlproperty Attribute CylinderGeometry::positionAttribute
 *
 * Holds the geometry position attribute.
 */

/*!
 * \qmlproperty Attribute CylinderGeometry::normalAttribute
 *
 * Holds the geometry normal attribute.
 */

/*!
 * \qmlproperty Attribute CylinderGeometry::texCoordAttribute
 *
 * Holds the geometry texture coordinate attribute.
 */

/*!
 * \qmlproperty Attribute CylinderGeometry::indexAttribute
 *
 * Holds the geometry index attribute.
 */

/*!
 * \class Qt3DExtras::QCylinderGeometry
 * \inmodule Qt3DExtras
 * \brief The QCylinderGeometry class allows creation of a cylinder in 3D space.
 * \since 5.7
 * \ingroup geometries
 * \inherits Qt3DRender::QGeometry
 *
 * The QCylinderGeometry class is most commonly used internally by the QCylinderMesh
 * but can also be used in custom Qt3DRender::QGeometryRenderer subclasses.
 */

/*!
 * Constructs a new QCylinderMesh with \a parent.
 */
QCylinderGeometry::QCylinderGeometry(QNode *parent)
    : QGeometry(*new QCylinderGeometryPrivate, parent)
{
    Q_D(QCylinderGeometry);
    d->init();
}

/*!
 * \internal
 */
QCylinderGeometry::QCylinderGeometry(QCylinderGeometryPrivate &dd, QNode *parent)
    :QGeometry(dd, parent)
{
    Q_D(QCylinderGeometry);
    d->init();
}

/*!
 * \internal
 */
QCylinderGeometry::~QCylinderGeometry()
{
}

/*!
 * Updates the vertices based on rings, slices, and length properties.
 */
void QCylinderGeometry::updateVertices()
{
    Q_D(QCylinderGeometry);
    const int nVerts = vertexCount(d->m_slices, d->m_rings);
    d->m_positionAttribute->setCount(nVerts);
    d->m_texCoordAttribute->setCount(nVerts);
    d->m_normalAttribute->setCount(nVerts);

    d->m_vertexBuffer->setDataGenerator(QSharedPointer<CylinderVertexDataFunctor>::create(d->m_rings, d->m_slices, d->m_radius, d->m_length));
}

/*!
 * Updates the indices based on rings, slices, and length properties.
 */
void QCylinderGeometry::updateIndices()
{
    Q_D(QCylinderGeometry);
    const int faces = faceCount(d->m_slices, d->m_rings);
    d->m_indexAttribute->setCount(faces * 3);
    d->m_indexBuffer->setDataGenerator(QSharedPointer<CylinderIndexDataFunctor>::create(d->m_rings, d->m_slices, d->m_length));
}

void QCylinderGeometry::setRings(int rings)
{
    Q_D(QCylinderGeometry);
    if (rings != d->m_rings) {
        d->m_rings = rings;
        updateVertices();
        updateIndices();
        emit ringsChanged(rings);
    }
}

void QCylinderGeometry::setSlices(int slices)
{
    Q_D(QCylinderGeometry);
    if (slices != d->m_slices) {
        d->m_slices = slices;
        updateVertices();
        updateIndices();
        emit slicesChanged(slices);
    }
}

void QCylinderGeometry::setRadius(float radius)
{
    Q_D(QCylinderGeometry);
    if (radius != d->m_radius) {
        d->m_radius = radius;
        updateVertices();
        emit radiusChanged(radius);
    }
}

void QCylinderGeometry::setLength(float length)
{
    Q_D(QCylinderGeometry);
    if (length != d->m_length) {
        d->m_length = length;
        updateVertices();
        updateIndices();
        emit lengthChanged(length);
    }
}

/*!
 * \property QCylinderGeometry::rings
 *
 * Holds the number of rings in the cylinder.
 */
int QCylinderGeometry::rings() const
{
    Q_D(const QCylinderGeometry);
    return d->m_rings;
}

/*!
 * \property QCylinderGeometry::slices
 *
 * Holds the number of slices in the cylinder.
 */
int QCylinderGeometry::slices() const
{
    Q_D(const QCylinderGeometry);
    return d->m_slices;
}

/*!
 * \property QCylinderGeometry::radius
 *
 * Holds the radius of the cylinder.
 */
float QCylinderGeometry::radius() const
{
    Q_D(const QCylinderGeometry);
    return d->m_radius;
}

/*!
 * \property QCylinderGeometry::length
 *
 * Holds the length of the cylinder.
 */
float QCylinderGeometry::length() const
{
    Q_D(const QCylinderGeometry);
    return d->m_length;
}

/*!
 * \property QCylinderGeometry::positionAttribute
 *
 * Holds the geometry position attribute.
 */
QAttribute *QCylinderGeometry::positionAttribute() const
{
    Q_D(const QCylinderGeometry);
    return d->m_positionAttribute;
}

/*!
 * \property QCylinderGeometry::normalAttribute
 *
 * Holds the geometry normal attribute.
 */
QAttribute *QCylinderGeometry::normalAttribute() const
{
    Q_D(const QCylinderGeometry);
    return d->m_normalAttribute;
}

/*!
 * \property QCylinderGeometry::texCoordAttribute
 *
 * Holds the geometry texture coordinate attribute.
 */
QAttribute *QCylinderGeometry::texCoordAttribute() const
{
    Q_D(const QCylinderGeometry);
    return d->m_texCoordAttribute;
}

/*!
 * \property QCylinderGeometry::indexAttribute
 *
 * Holds the geometry index attribute.
 */
QAttribute *QCylinderGeometry::indexAttribute() const
{
    Q_D(const QCylinderGeometry);
    return d->m_indexAttribute;
}

} // namespace Qt3DExtras

QT_END_NAMESPACE
