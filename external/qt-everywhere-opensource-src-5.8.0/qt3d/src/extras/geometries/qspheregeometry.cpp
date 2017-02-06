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

#include "qspheregeometry.h"
#include "qspheregeometry_p.h"
#include <Qt3DRender/qbufferdatagenerator.h>
#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/qattribute.h>

#ifndef _USE_MATH_DEFINES
# define _USE_MATH_DEFINES // For MSVC
#endif

#include <qmath.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DRender;

namespace  Qt3DExtras {

namespace {

QByteArray createSphereMeshVertexData(float radius, int rings, int slices)
{
    QByteArray bufferBytes;
    // vec3 pos, vec2 texCoord, vec3 normal, vec4 tangent
    const quint32 elementSize = 3 + 2 + 3 + 4;
    const quint32 stride = elementSize * sizeof(float);
    const int nVerts  = (slices + 1) * (rings + 1);
    bufferBytes.resize(stride * nVerts);

    float* fptr = reinterpret_cast<float*>(bufferBytes.data());

    const float dTheta = (M_PI * 2) / static_cast<float>( slices );
    const float dPhi = M_PI / static_cast<float>( rings );
    const float du = 1.0f / static_cast<float>( slices );
    const float dv = 1.0f / static_cast<float>( rings );

    // Iterate over latitudes (rings)
    for ( int lat = 0; lat < rings + 1; ++lat )
    {
        const float phi = M_PI_2 - static_cast<float>( lat ) * dPhi;
        const float cosPhi = qCos( phi );
        const float sinPhi = qSin( phi );
        const float v = 1.0f - static_cast<float>( lat ) * dv;

        // Iterate over longitudes (slices)
        for ( int lon = 0; lon < slices + 1; ++lon )
        {
            const float theta = static_cast<float>( lon ) * dTheta;
            const float cosTheta = qCos( theta );
            const float sinTheta = qSin( theta );
            const float u = static_cast<float>( lon ) * du;

            *fptr++ = radius * cosTheta * cosPhi;
            *fptr++ = radius * sinPhi;
            *fptr++ = radius * sinTheta * cosPhi;

            *fptr++ = u;
            *fptr++ = v;

            *fptr++   = cosTheta * cosPhi;
            *fptr++ = sinPhi;
            *fptr++ = sinTheta * cosPhi;

            *fptr++ = sinTheta;
            *fptr++ = 0.0;
            *fptr++ = -cosTheta;
            *fptr++ = 1.0;
        }
    }
    return bufferBytes;
}

QByteArray createSphereMeshIndexData(int rings, int slices)
{
    int faces = (slices * 2) * (rings - 2); // two tris per slice, for all middle rings
    faces += 2 * slices; // tri per slice for both top and bottom

    QByteArray indexBytes;
    const int indices = faces * 3;
    Q_ASSERT(indices < 65536);
    indexBytes.resize(indices * sizeof(quint16));
    quint16 *indexPtr = reinterpret_cast<quint16*>(indexBytes.data());

    // top cap
    {
        const int nextRingStartIndex = slices + 1;
        for ( int j = 0; j < slices; ++j )
        {
            *indexPtr++ = nextRingStartIndex + j;
            *indexPtr++ = 0;
            *indexPtr++ = nextRingStartIndex + j + 1;
        }
    }

    for ( int i = 1; i < (rings - 1); ++i )
    {
        const int ringStartIndex = i * ( slices + 1 );
        const int nextRingStartIndex = ( i + 1 ) * ( slices + 1 );

        for ( int j = 0; j < slices; ++j )
        {
            // Split the quad into two triangles
            *indexPtr++ = ringStartIndex + j;
            *indexPtr++ = ringStartIndex + j + 1;
            *indexPtr++ = nextRingStartIndex + j;
            *indexPtr++ = nextRingStartIndex + j;
            *indexPtr++ = ringStartIndex + j + 1;
            *indexPtr++ = nextRingStartIndex + j + 1;
        }
    }

    // bottom cap
    {
        const int ringStartIndex = (rings - 1) * ( slices + 1);
        const int nextRingStartIndex = (rings) * ( slices + 1);
        for ( int j = 0; j < slices; ++j )
        {
            *indexPtr++ = ringStartIndex + j + 1;
            *indexPtr++ = nextRingStartIndex;
            *indexPtr++ = ringStartIndex + j;
        }
    }

    return indexBytes;
}

} // anonymous

class SphereVertexDataFunctor : public QBufferDataGenerator
{
public:
    SphereVertexDataFunctor(int rings, int slices, float radius)
        : m_rings(rings)
        , m_slices(slices)
        , m_radius(radius)
    {
    }

    QByteArray operator ()() Q_DECL_OVERRIDE
    {
        return createSphereMeshVertexData(m_radius, m_rings, m_slices);
    }

    bool operator ==(const QBufferDataGenerator &other) const Q_DECL_OVERRIDE
    {
        const SphereVertexDataFunctor *otherFunctor = functor_cast<SphereVertexDataFunctor>(&other);
        if (otherFunctor != nullptr)
            return (otherFunctor->m_rings == m_rings &&
                    otherFunctor->m_slices == m_slices &&
                    otherFunctor->m_radius == m_radius);
        return false;
    }

    QT3D_FUNCTOR(SphereVertexDataFunctor)

private:
    int m_rings;
    int m_slices;
    float m_radius;
};

class SphereIndexDataFunctor : public QBufferDataGenerator
{
public:
    SphereIndexDataFunctor(int rings, int slices)
        : m_rings(rings)
        , m_slices(slices)
    {
    }

    QByteArray operator ()() Q_DECL_OVERRIDE
    {
        return createSphereMeshIndexData(m_rings, m_slices);
    }

    bool operator ==(const QBufferDataGenerator &other) const Q_DECL_OVERRIDE
    {
        const SphereIndexDataFunctor *otherFunctor = functor_cast<SphereIndexDataFunctor>(&other);
        if (otherFunctor != nullptr)
            return (otherFunctor->m_rings == m_rings &&
                    otherFunctor->m_slices == m_slices);
        return false;
    }

    QT3D_FUNCTOR(SphereIndexDataFunctor)

private:
    int m_rings;
    int m_slices;
};

QSphereGeometryPrivate::QSphereGeometryPrivate()
    : QGeometryPrivate()
    , m_generateTangents(false)
    , m_rings(16)
    , m_slices(16)
    , m_radius(1.0f)
    , m_positionAttribute(nullptr)
    , m_normalAttribute(nullptr)
    , m_texCoordAttribute(nullptr)
    , m_tangentAttribute(nullptr)
    , m_indexAttribute(nullptr)
    , m_vertexBuffer(nullptr)
    , m_indexBuffer(nullptr)
{
}

void QSphereGeometryPrivate::init()
{
    Q_Q(QSphereGeometry);
    m_positionAttribute = new QAttribute(q);
    m_normalAttribute = new QAttribute(q);
    m_texCoordAttribute = new QAttribute(q);
    m_tangentAttribute = new QAttribute(q);
    m_indexAttribute = new QAttribute(q);
    m_vertexBuffer = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::VertexBuffer, q);
    m_indexBuffer = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::IndexBuffer, q);

    // vec3 pos, vec2 tex, vec3 normal, vec4 tangent
    const quint32 elementSize = 3 + 2 + 3 + 4;
    const quint32 stride = elementSize * sizeof(float);
    const int nVerts = (m_slices + 1) * (m_rings + 1);
    const int faces = (m_slices * 2) * (m_rings - 2) + (2 * m_slices);

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

    m_tangentAttribute->setName(QAttribute::defaultTangentAttributeName());
    m_tangentAttribute->setVertexBaseType(QAttribute::Float);
    m_tangentAttribute->setVertexSize(4);
    m_tangentAttribute->setAttributeType(QAttribute::VertexAttribute);
    m_tangentAttribute->setBuffer(m_vertexBuffer);
    m_tangentAttribute->setByteStride(stride);
    m_tangentAttribute->setByteOffset(8 * sizeof(float));
    m_tangentAttribute->setCount(nVerts);

    m_indexAttribute->setAttributeType(QAttribute::IndexAttribute);
    m_indexAttribute->setVertexBaseType(QAttribute::UnsignedShort);
    m_indexAttribute->setBuffer(m_indexBuffer);

    m_indexAttribute->setCount(faces * 3);

    m_vertexBuffer->setDataGenerator(QSharedPointer<SphereVertexDataFunctor>::create(m_rings, m_slices, m_radius));
    m_indexBuffer->setDataGenerator(QSharedPointer<SphereIndexDataFunctor>::create(m_rings, m_slices));

    q->addAttribute(m_positionAttribute);
    q->addAttribute(m_texCoordAttribute);
    q->addAttribute(m_normalAttribute);
    if (m_generateTangents)
        q->addAttribute(m_tangentAttribute);
    q->addAttribute(m_indexAttribute);
}

/*!
 * \qmltype SphereGeometry
 * \instantiates Qt3DExtras::QSphereGeometry
 * \inqmlmodule Qt3D.Extras
 * \brief SphereGeometry allows creation of a sphere in 3D space.
 *
 * The SphereGeometry type is most commonly used internally by the SphereMesh type
 * but can also be used in custom GeometryRenderer types.
 */

/*!
 * \qmlproperty int SphereGeometry::rings
 *
 * Holds the number of rings in the sphere.
 */

/*!
 * \qmlproperty int SphereGeometry::slices
 *
 * Holds the number of slices in the sphere.
 */

/*!
 * \qmlproperty real SphereGeometry::radius
 *
 * Holds the radius of the sphere.
 */

/*!
 * \qmlproperty bool SphereGeometry::generateTangents
 *
 * Holds the value of the automatic tangent vectors generation flag.
 * Tangent vectors are orthogonal to normal vectors.
 */

/*!
 * \qmlproperty Attribute SphereGeometry::positionAttribute
 *
 * Holds the geometry position attribute.
 */

/*!
 * \qmlproperty Attribute SphereGeometry::normalAttribute
 *
 * Holds the geometry normal attribute.
 */

/*!
 * \qmlproperty Attribute SphereGeometry::texCoordAttribute
 *
 * Holds the geometry texture coordinate attribute.
 */

/*!
 * \qmlproperty Attribute SphereGeometry::tangentAttribute
 *
 * Holds the geometry tangent attribute.
 */

/*!
 * \qmlproperty Attribute SphereGeometry::indexAttribute
 *
 * Holds the geometry index attribute.
 */

/*!
 * \class Qt3DExtras::QSphereGeometry
 * \inmodule Qt3DExtras
 * \brief The QSphereGeometry class allows creation of a sphere in 3D space.
 * \since 5.7
 * \ingroup geometries
 * \inherits Qt3DRender::QGeometry
 *
 * The QSphereGeometry class is most commonly used internally by the QSphereMesh
 * but can also be used in custom Qt3DRender::QGeometryRenderer subclasses.
 */

/*!
 * Constructs a new QSphereGeometry with \a parent.
 */
QSphereGeometry::QSphereGeometry(QNode *parent)
    : QGeometry(*new QSphereGeometryPrivate(), parent)
{
    Q_D(QSphereGeometry);
    d->init();
}

/*!
 * \internal
 */
QSphereGeometry::QSphereGeometry(QSphereGeometryPrivate &dd, QNode *parent)
    : QGeometry(dd, parent)
{
    Q_D(QSphereGeometry);
    d->init();
}

/*!
 * \internal
 */
QSphereGeometry::~QSphereGeometry()
{
}

/*!
 * Updates vertices based on rings, slices, and radius properties
 */
void QSphereGeometry::updateVertices()
{
    Q_D(QSphereGeometry);
    const int nVerts = (d->m_slices + 1) * (d->m_rings + 1);
    d->m_positionAttribute->setCount(nVerts);
    d->m_texCoordAttribute->setCount(nVerts);
    d->m_normalAttribute->setCount(nVerts);
    d->m_tangentAttribute->setCount(nVerts);
    d->m_vertexBuffer->setDataGenerator(QSharedPointer<SphereVertexDataFunctor>::create(d->m_rings, d->m_slices, d->m_radius));
}

/*!
 * Updates indices based on rings and slices properties.
 */
void QSphereGeometry::updateIndices()
{
    Q_D(QSphereGeometry);
    const int faces = (d->m_slices * 2) * (d->m_rings - 2) + (2 * d->m_slices);
    d->m_indexAttribute->setCount(faces * 3);
    d->m_indexBuffer->setDataGenerator(QSharedPointer<SphereIndexDataFunctor>::create(d->m_rings, d->m_slices));

}

void QSphereGeometry::setRings(int rings)
{
    Q_D(QSphereGeometry);
    if (rings != d->m_rings) {
        d->m_rings = rings;
        updateVertices();
        updateIndices();
        emit ringsChanged(rings);
    }
}

void QSphereGeometry::setSlices(int slices)
{
    Q_D(QSphereGeometry);
    if (slices != d->m_slices) {
        d->m_slices = slices;
        updateVertices();
        updateIndices();
        emit slicesChanged(slices);
    }
}

void QSphereGeometry::setRadius(float radius)
{
    Q_D(QSphereGeometry);
    if (radius != d->m_radius) {
        d->m_radius = radius;
        updateVertices();
        emit radiusChanged(radius);
    }
}

void QSphereGeometry::setGenerateTangents(bool gen)
{
    Q_D(QSphereGeometry);
    if (d->m_generateTangents != gen) {
        if (d->m_generateTangents)
            removeAttribute(d->m_tangentAttribute);
        d->m_generateTangents = gen;
        if (d->m_generateTangents)
            addAttribute(d->m_tangentAttribute);
        emit generateTangentsChanged(gen);
    }
}

/*!
 * \property QSphereGeometry::generateTangents
 *
 * Holds the value of the automatic tangent vectors generation flag.
 * Tangent vectors are orthogonal to normal vectors.
 */
bool QSphereGeometry::generateTangents() const
{
    Q_D(const QSphereGeometry);
    return d->m_generateTangents;
}

/*!
 * \property QSphereGeometry::rings
 *
 * Holds the number of rings in the sphere.
 */
int QSphereGeometry::rings() const
{
    Q_D(const QSphereGeometry);
    return d->m_rings;
}

/*!
 * \property QSphereGeometry::slices
 *
 * Holds the number of slices in the sphere.
 */
int QSphereGeometry::slices() const
{
    Q_D(const QSphereGeometry);
    return d->m_slices;
}

/*!
 * \property QSphereGeometry::radius
 *
 * Holds the radius of the sphere.
 */
float QSphereGeometry::radius() const
{
    Q_D(const QSphereGeometry);
    return d->m_radius;
}

/*!
 * \property QSphereGeometry::positionAttribute
 *
 * Holds the geometry position attribute.
 */
QAttribute *QSphereGeometry::positionAttribute() const
{
    Q_D(const QSphereGeometry);
    return d->m_positionAttribute;
}

/*!
 * \property QSphereGeometry::normalAttribute
 *
 * Holds the geometry normal attribute.
 */
QAttribute *QSphereGeometry::normalAttribute() const
{
    Q_D(const QSphereGeometry);
    return d->m_normalAttribute;
}

/*!
 * \property QSphereGeometry::texCoordAttribute
 *
 * Holds the geometry texture coordinate attribute.
 */
QAttribute *QSphereGeometry::texCoordAttribute() const
{
    Q_D(const QSphereGeometry);
    return d->m_texCoordAttribute;
}

/*!
 * \property QSphereGeometry::tangentAttribute
 *
 * Holds the geometry tangent attribute.
 */
QAttribute *QSphereGeometry::tangentAttribute() const
{
    Q_D(const QSphereGeometry);
    return d->m_tangentAttribute;
}

/*!
 * \property QSphereGeometry::indexAttribute
 *
 * Holds the geometry index attribute.
 */
QAttribute *QSphereGeometry::indexAttribute() const
{
    Q_D(const QSphereGeometry);
    return d->m_indexAttribute;
}

} //  Qt3DExtras

QT_END_NAMESPACE

