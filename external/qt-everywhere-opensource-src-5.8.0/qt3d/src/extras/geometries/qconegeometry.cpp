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

/*!
 * \class Qt3DExtras::QConeGeometry
 * \inmodule Qt3DExtras
 * \brief The QConeGeometry class allows creation of a cone in 3D space.
 * \since 5.7
 * \ingroup geometries
 * \inherits Qt3DRender::QGeometry
 *
 * The QConeGeometry class is most commonly used internally by the QConeMesh
 * but can also be used in custom Qt3DRender::QGeometryRenderer subclasses. The class
 * allows for creation of both a cone and a truncated cone.
 */

#ifndef _USE_MATH_DEFINES
# define _USE_MATH_DEFINES // For MSVC
#endif

#include "qconegeometry.h"
#include "qconegeometry_p.h"
#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/qbufferdatagenerator.h>
#include <Qt3DRender/qattribute.h>
#include <QVector3D>
#include <cmath>

QT_BEGIN_NAMESPACE

using namespace Qt3DRender;

namespace Qt3DExtras {

namespace {

int faceCount(int slices, int rings, int capCount)
{
    return (slices * 2) * (rings - 1) + slices * capCount;
}

int vertexCount(int slices, int rings, int capCount)
{
    return (slices + 1) * rings + capCount * (slices + 2);
}

void createSidesVertices(float *&verticesPtr,
                         int rings,
                         int slices,
                         double topRadius,
                         double bottomRadius,
                         double length)
{
    const float dY = length / static_cast<float>(rings - 1);
    const float dTheta = (M_PI * 2) / static_cast<float>(slices);

    for (int ring = 0; ring < rings; ++ring) {
        const float y = -length / 2.0f + static_cast<float>(ring) * dY;

        const float t = (y + length / 2) / length;
        const float radius = (bottomRadius * (1 - t)) + (t * topRadius);

        for (int slice = 0; slice <= slices; ++slice) {
            const float theta = static_cast<float>(slice) * dTheta;
            const float ta = std::tan((M_PI/2) - std::atan(length / (bottomRadius - topRadius)));
            const float ct = std::cos(theta);
            const float st = std::sin(theta);

            *verticesPtr++ = radius * ct;
            *verticesPtr++ = y;
            *verticesPtr++ = radius * st;

            *verticesPtr++ = (y + length / 2.0) / length;
            *verticesPtr++ = theta / (M_PI * 2);

            QVector3D n(ct, ta, st);
            n.normalize();
            *verticesPtr++ = n.x();
            *verticesPtr++ = n.y();
            *verticesPtr++ = n.z();
        }
    }
}

void createSidesIndices(quint16 *&indicesPtr, int rings, int slices)
{
    for (int ring = 0; ring < rings-1; ++ring) {
        const int ringIndexStart = ring * (slices + 1);
        const int nextRingIndexStart = (ring + 1) * (slices + 1);

        for (int slice = 0; slice <= slices; ++slice) {
            if (slice == slices)
                continue;

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
                        double topRadius,
                        double bottomRadius,
                        double length,
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


    for (int slice = 0; slice <= slices; ++slice)
    {
        const float theta = static_cast<float>(slice) * dTheta;
        const float ct = std::cos(theta);
        const float st = std::sin(theta);

        const float t = (yPosition + length / 2) / length;
        const float radius = (bottomRadius * (1 - t)) + (t * topRadius);

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
                       bool isTopCap)
{
    if ( !isTopCap ) {
        for ( int i = slices - 1 ; i >= 0 ; --i )
        {
            if ( i != 0 ) {
                *indicesPtr++ = discCenterIndex;
                *indicesPtr++ = discCenterIndex + i + 1;
                *indicesPtr++ = discCenterIndex + i;
            } else {
                *indicesPtr++ = discCenterIndex;
                *indicesPtr++ = discCenterIndex + i + 1;
                *indicesPtr++ = discCenterIndex + slices;
            }
        }
    } else {
        for ( int i = 0 ; i < slices; ++i )
        {
            if ( i != slices - 1 ) {
                *indicesPtr++ = discCenterIndex;
                *indicesPtr++ = discCenterIndex + i + 1;
                *indicesPtr++ = discCenterIndex + i + 2;
            } else {
                *indicesPtr++ = discCenterIndex;
                *indicesPtr++ = discCenterIndex + i + 1;
                *indicesPtr++ = discCenterIndex + 1;
            }
        }
    }
}

} // anonymous


class ConeVertexDataFunctor : public QBufferDataGenerator
{
public:
    ConeVertexDataFunctor(bool hasTopEndcap, bool hasBottomEndcap, int rings, int slices,
                          float topRadius, float bottomRadius, float length)
        : m_hasTopEndcap(hasTopEndcap)
        , m_hasBottomEndcap(hasBottomEndcap)
        , m_rings(rings)
        , m_slices(slices)
        , m_topRadius(topRadius)
        , m_bottomRadius(bottomRadius)
        , m_length(length)
    {}

    QByteArray operator ()() Q_DECL_OVERRIDE
    {
        const int verticesCount =
                vertexCount(m_slices, m_rings, (m_hasTopEndcap + m_hasBottomEndcap));

        // vec3 pos, vec2 texCoord, vec3 normal
        const quint32 vertexSize = (3 + 2 + 3) * sizeof(float);

        QByteArray verticesData;
        verticesData.resize(vertexSize * verticesCount);
        float *verticesPtr = reinterpret_cast<float*>(verticesData.data());

        createSidesVertices(verticesPtr, m_rings, m_slices, m_topRadius, m_bottomRadius, m_length);
        if ( m_hasTopEndcap )
            createDiscVertices(verticesPtr, m_slices, m_topRadius, m_bottomRadius, m_length, m_length * 0.5f);
        if ( m_hasBottomEndcap )
            createDiscVertices(verticesPtr, m_slices, m_topRadius, m_bottomRadius, m_length, -m_length * 0.5f);

        return verticesData;
    }

    bool operator ==(const QBufferDataGenerator &other) const Q_DECL_OVERRIDE
    {
        const ConeVertexDataFunctor *otherFunctor = functor_cast<ConeVertexDataFunctor>(&other);
        if (otherFunctor != nullptr)
            return (otherFunctor->m_hasTopEndcap == m_hasTopEndcap &&
                    otherFunctor->m_hasBottomEndcap == m_hasBottomEndcap &&
                    otherFunctor->m_rings == m_rings &&
                    otherFunctor->m_slices == m_slices &&
                    otherFunctor->m_topRadius == m_topRadius &&
                    otherFunctor->m_bottomRadius == m_bottomRadius &&
                    otherFunctor->m_length == m_length);
        return false;
    }

    QT3D_FUNCTOR(ConeVertexDataFunctor)

private:
    bool m_hasTopEndcap;
    bool m_hasBottomEndcap;
    int m_rings;
    int m_slices;
    float m_topRadius;
    float m_bottomRadius;
    float m_length;
};

class ConeIndexDataFunctor : public QBufferDataGenerator
{
public:
    ConeIndexDataFunctor(bool hasTopEndcap, bool hasBottomEndcap, int rings, int slices,
                         float length)
        : m_hasTopEndcap(hasTopEndcap)
        , m_hasBottomEndcap(hasBottomEndcap)
        , m_rings(rings)
        , m_slices(slices)
        , m_length(length)
    {
    }

    QByteArray operator ()() Q_DECL_OVERRIDE
    {
        const int facesCount = faceCount(m_slices, m_rings, (m_hasTopEndcap + m_hasBottomEndcap));

        const int indicesCount = facesCount * 3;
        const int indexSize = sizeof(quint16);
        Q_ASSERT(indicesCount < 65536);

        QByteArray indicesBytes;
        indicesBytes.resize(indicesCount * indexSize);
        quint16 *indicesPtr = reinterpret_cast<quint16*>(indicesBytes.data());

        createSidesIndices(indicesPtr, m_rings, m_slices);
        if ( m_hasTopEndcap )
            createDiscIndices(indicesPtr, m_rings * (m_slices + 1) + m_slices + 2, m_slices, true);
        if ( m_hasBottomEndcap )
            createDiscIndices(indicesPtr, m_rings * (m_slices + 1), m_slices, false);

        return indicesBytes;
    }

    bool operator ==(const QBufferDataGenerator &other) const Q_DECL_OVERRIDE
    {
        const ConeIndexDataFunctor *otherFunctor = functor_cast<ConeIndexDataFunctor>(&other);
        if (otherFunctor != nullptr)
            return (otherFunctor->m_hasTopEndcap == m_hasTopEndcap &&
                    otherFunctor->m_hasBottomEndcap == m_hasBottomEndcap &&
                    otherFunctor->m_rings == m_rings &&
                    otherFunctor->m_slices == m_slices &&
                    otherFunctor->m_length == m_length);
        return false;
    }

    QT3D_FUNCTOR(ConeIndexDataFunctor)

private:
    bool m_hasTopEndcap;
    bool m_hasBottomEndcap;
    int m_rings;
    int m_slices;
    float m_length;
};


QConeGeometryPrivate::QConeGeometryPrivate()
    : QGeometryPrivate()
    , m_hasTopEndcap(true)
    , m_hasBottomEndcap(true)
    , m_rings(16)
    , m_slices(16)
    , m_topRadius(0.0f)
    , m_bottomRadius(1.0f)
    , m_length(1.0f)
    , m_positionAttribute(nullptr)
    , m_normalAttribute(nullptr)
    , m_texCoordAttribute(nullptr)
    , m_indexAttribute(nullptr)
    , m_positionBuffer(nullptr)
    , m_vertexBuffer(nullptr)
    , m_indexBuffer(nullptr)
{
}

void QConeGeometryPrivate::init()
{
    Q_Q(QConeGeometry);
    m_positionAttribute = new QAttribute(q);
    m_normalAttribute = new QAttribute(q);
    m_texCoordAttribute = new QAttribute(q);
    m_indexAttribute = new QAttribute(q);
    m_vertexBuffer = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::VertexBuffer, q);
    m_indexBuffer = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::IndexBuffer, q);

    // vec3 pos, vec2 tex, vec3 normal
    const quint32 elementSize = 3 + 2 + 3;
    const quint32 stride = elementSize * sizeof(float);
    const int faces = faceCount(m_slices, m_rings, (m_hasTopEndcap + m_hasBottomEndcap));
    const int nVerts = vertexCount(m_slices, m_rings, (m_hasTopEndcap + m_hasBottomEndcap));

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

    m_vertexBuffer->setDataGenerator(QSharedPointer<ConeVertexDataFunctor>::create(m_hasTopEndcap, m_hasBottomEndcap, m_rings, m_slices,
                                                                                   m_topRadius, m_bottomRadius, m_length));
    m_indexBuffer->setDataGenerator(QSharedPointer<ConeIndexDataFunctor>::create(m_hasTopEndcap, m_hasBottomEndcap, m_rings, m_slices,
                                                                                 m_length));

    q->addAttribute(m_positionAttribute);
    q->addAttribute(m_texCoordAttribute);
    q->addAttribute(m_normalAttribute);
    q->addAttribute(m_indexAttribute);
}

/*!
 * \qmltype ConeGeometry
 * \instantiates Qt3DExtras::QConeGeometry
 * \inqmlmodule Qt3D.Extras
 * \brief ConeGeometry allows creation of a cone in 3D space.
 *
 * The ConeGeometry type is most commonly used internally by the ConeMesh type
 * but can also be used in custom GeometryRenderer types.
 * The ConeGeometry type allows for creation of both a cone and a truncated cone.
 */

/*!
 * \qmlproperty bool ConeGeometry::hasTopEndcap
 *
 * Determines if the cone top is capped or open.
 */

/*!
 * \qmlproperty bool ConeGeometry::hasBottomEndcap
 *
 * Determines if the cone bottom is capped or open.
 */

/*!
 * \qmlproperty int ConeGeometry::rings
 *
 * Holds the number of rings in the geometry.
 */

/*!
 * \qmlproperty int ConeGeometry::slices
 *
 * Holds the number of slices in the geometry.
 */

/*!
 * \qmlproperty real ConeGeometry::topRadius
 *
 * Holds the top radius of the cone.
 */

/*!
 * \qmlproperty real ConeGeometry::bottomRadius
 *
 * Holds the bottom radius of the cone.
 */

/*!
 * \qmlproperty real ConeGeometry::length
 *
 * Holds the length of the cone.
 */

/*!
 * \qmlproperty Attribute ConeGeometry::positionAttribute
 *
 * Holds the geometry position attribute.
 */

/*!
 * \qmlproperty Attribute ConeGeometry::normalAttribute
 *
 * Holds the geometry normal attribute.
 */

/*!
 * \qmlproperty Attribute ConeGeometry::texCoordAttribute
 *
 * Holds the geometry texture coordinate attribute.
 */

/*!
 * \qmlproperty Attribute ConeGeometry::indexAttribute
 *
 * Holds the geometry index attribute.
 */

QConeGeometry::QConeGeometry(QNode *parent)
    : QGeometry(*new QConeGeometryPrivate, parent)
{
    Q_D(QConeGeometry);
    d->init();
}

QConeGeometry::QConeGeometry(QConeGeometryPrivate &dd, QNode *parent)
    :QGeometry(dd, parent)
{
    Q_D(QConeGeometry);
    d->init();
}


/*! \internal */
QConeGeometry::~QConeGeometry()
{
}

/*!
 * Updates vertices based on geometry properties.
 */
void QConeGeometry::updateVertices()
{
    Q_D(QConeGeometry);
    const int nVerts = vertexCount(d->m_slices, d->m_rings,
                                   (d->m_hasTopEndcap + d->m_hasBottomEndcap));

    d->m_positionAttribute->setCount(nVerts);
    d->m_texCoordAttribute->setCount(nVerts);
    d->m_normalAttribute->setCount(nVerts);
    d->m_vertexBuffer->setDataGenerator(QSharedPointer<ConeVertexDataFunctor>::create(d->m_hasTopEndcap, d->m_hasBottomEndcap, d->m_rings, d->m_slices,
                                                                                      d->m_topRadius, d->m_bottomRadius, d->m_length));
}

/*!
 * Updates indices based on geometry properties.
 */
void QConeGeometry::updateIndices()
{
    Q_D(QConeGeometry);
    const int faces = faceCount(d->m_slices, d->m_rings,
                                (d->m_hasTopEndcap + d->m_hasBottomEndcap));

    d->m_indexAttribute->setCount(faces * 3);
    d->m_indexBuffer->setDataGenerator(QSharedPointer<ConeIndexDataFunctor>::create(d->m_hasTopEndcap, d->m_hasBottomEndcap, d->m_rings, d->m_slices,
                                                                                    d->m_length));
}

/*!
 * \property QConeGeometry::hasTopEndcap
 *
 * Determines if the cone top is capped or open.
 */
/*!
 * \property QConeGeometry::hasBottomEndcap
 *
 * Determines if the cone bottom is capped or open.
 */

/*!
 * \property QConeGeometry::rings
 *
 * Holds the number of rings in the geometry.
 */

/*!
 * \property QConeGeometry::slices
 *
 * Holds the number of slices in the geometry.
 */

/*!
 * \property QConeGeometry::topRadius
 *
 * Holds the top radius of the cone.
 */

/*!
 * \property QConeGeometry::bottomRadius
 *
 * Holds the bottom radius of the cone.
 */

/*!
 * \property QConeGeometry::length
 *
 * Holds the length of the cone.
 */

/*!
 * \property QConeGeometry::positionAttribute
 *
 * Holds the geometry position attribute.
 */

/*!
 * \property QConeGeometry::normalAttribute
 *
 * Holds the geometry normal attribute.
 */

/*!
 * \property QConeGeometry::texCoordAttribute
 *
 * Holds the geometry texture coordinate attribute.
 */

/*!
 * \property QConeGeometry::indexAttribute
 *
 * Holds the geometry index attribute.
 */

void QConeGeometry::setHasTopEndcap(bool hasTopEndcap)
{
    Q_D(QConeGeometry);
    if (hasTopEndcap != d->m_hasTopEndcap) {
        d->m_hasTopEndcap = hasTopEndcap;
        updateVertices();
        emit hasTopEndcapChanged(hasTopEndcap);
    }
}

void QConeGeometry::setHasBottomEndcap(bool hasBottomEndcap)
{
    Q_D(QConeGeometry);
    if (hasBottomEndcap != d->m_hasBottomEndcap) {
        d->m_hasBottomEndcap = hasBottomEndcap;
        updateVertices();
        emit hasBottomEndcapChanged(hasBottomEndcap);
    }
}

void QConeGeometry::setRings(int rings)
{
    Q_D(QConeGeometry);
    if (rings != d->m_rings) {
        d->m_rings = rings;
        updateVertices();
        updateIndices();
        emit ringsChanged(rings);
    }
}

void QConeGeometry::setSlices(int slices)
{
    Q_D(QConeGeometry);
    if (slices != d->m_slices) {
        d->m_slices = slices;
        updateVertices();
        updateIndices();
        emit slicesChanged(slices);
    }
}

void QConeGeometry::setTopRadius(float topRadius)
{
    Q_D(QConeGeometry);
    if (topRadius != d->m_topRadius) {
        d->m_topRadius = topRadius;
        updateVertices();
        emit topRadiusChanged(topRadius);
    }
}

void QConeGeometry::setBottomRadius(float bottomRadius)
{
    Q_D(QConeGeometry);
    if (bottomRadius != d->m_bottomRadius) {
        d->m_bottomRadius = bottomRadius;
        updateVertices();
        emit bottomRadiusChanged(bottomRadius);
    }
}

void QConeGeometry::setLength(float length)
{
    Q_D(QConeGeometry);
    if (length != d->m_length) {
        d->m_length = length;
        updateVertices();
        updateIndices();
        emit lengthChanged(length);
    }
}

bool QConeGeometry::hasTopEndcap() const
{
    Q_D(const QConeGeometry);
    return d->m_hasTopEndcap;
}

bool QConeGeometry::hasBottomEndcap() const
{
    Q_D(const QConeGeometry);
    return d->m_hasBottomEndcap;
}

float QConeGeometry::topRadius() const
{
    Q_D(const QConeGeometry);
    return d->m_topRadius;
}

float QConeGeometry::bottomRadius() const
{
    Q_D(const QConeGeometry);
    return d->m_bottomRadius;
}

int QConeGeometry::rings() const
{
    Q_D(const QConeGeometry);
    return d->m_rings;
}

int QConeGeometry::slices() const
{
    Q_D(const QConeGeometry);
    return d->m_slices;
}

float QConeGeometry::length() const
{
    Q_D(const QConeGeometry);
    return d->m_length;
}

QAttribute *QConeGeometry::positionAttribute() const
{
    Q_D(const QConeGeometry);
    return d->m_positionAttribute;
}

QAttribute *QConeGeometry::normalAttribute() const
{
    Q_D(const QConeGeometry);
    return d->m_normalAttribute;
}

QAttribute *QConeGeometry::texCoordAttribute() const
{
    Q_D(const QConeGeometry);
    return d->m_texCoordAttribute;
}

QAttribute *QConeGeometry::indexAttribute() const
{
    Q_D(const QConeGeometry);
    return d->m_indexAttribute;
}

} // namespace Qt3DExtras

QT_END_NAMESPACE
