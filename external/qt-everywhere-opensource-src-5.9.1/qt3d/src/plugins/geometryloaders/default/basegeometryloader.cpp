/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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

#include "basegeometryloader_p.h"

#include <Qt3DRender/qattribute.h>
#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/qgeometry.h>

#include <Qt3DRender/private/qaxisalignedboundingbox_p.h>
#include <Qt3DRender/private/renderlogging_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

Q_LOGGING_CATEGORY(BaseGeometryLoaderLog, "Qt3D.BaseGeometryLoader", QtWarningMsg)

BaseGeometryLoader::BaseGeometryLoader()
    : m_loadTextureCoords(true)
    , m_generateTangents(true)
    , m_centerMesh(false)
    , m_geometry(nullptr)
{
}

QGeometry *BaseGeometryLoader::geometry() const
{
    return m_geometry;
}

bool BaseGeometryLoader::load(QIODevice *ioDev, const QString &subMesh)
{
    if (!doLoad(ioDev, subMesh))
        return false;

    if (m_normals.isEmpty())
        generateAveragedNormals(m_points, m_normals, m_indices);

    if (m_generateTangents && !m_texCoords.isEmpty())
        generateTangents(m_points, m_normals, m_indices, m_texCoords, m_tangents);

    if (m_centerMesh)
        center(m_points);

    qCDebug(BaseGeometryLoaderLog) << "Loaded mesh:";
    qCDebug(BaseGeometryLoaderLog) << " " << m_points.size() << "points";
    qCDebug(BaseGeometryLoaderLog) << " " << m_indices.size() / 3 << "triangles.";
    qCDebug(BaseGeometryLoaderLog) << " " << m_normals.size() << "normals";
    qCDebug(BaseGeometryLoaderLog) << " " << m_tangents.size() << "tangents ";
    qCDebug(BaseGeometryLoaderLog) << " " << m_texCoords.size() << "texture coordinates.";

    generateGeometry();

    return true;
}

void BaseGeometryLoader::generateAveragedNormals(const QVector<QVector3D>& points,
                                                 QVector<QVector3D>& normals,
                                                 const QVector<unsigned int>& faces) const
{
    for (int i = 0; i < points.size(); ++i)
        normals.append(QVector3D());

    for (int i = 0; i < faces.size(); i += 3) {
        const QVector3D &p1 = points[ faces[i]   ];
        const QVector3D &p2 = points[ faces[i+1] ];
        const QVector3D &p3 = points[ faces[i+2] ];

        const QVector3D a = p2 - p1;
        const QVector3D b = p3 - p1;
        const QVector3D n = QVector3D::crossProduct(a, b).normalized();

        normals[ faces[i]   ] += n;
        normals[ faces[i+1] ] += n;
        normals[ faces[i+2] ] += n;
    }

    for (int i = 0; i < normals.size(); ++i)
        normals[i].normalize();
}

void BaseGeometryLoader::generateGeometry()
{
    QByteArray bufferBytes;
    const int count = m_points.size();
    const quint32 elementSize = 3 + (hasTextureCoordinates() ? 2 : 0)
            + (hasNormals() ? 3 : 0)
            + (hasTangents() ? 4 : 0);
    const quint32 stride = elementSize * sizeof(float);
    bufferBytes.resize(stride * count);
    float *fptr = reinterpret_cast<float*>(bufferBytes.data());

    for (int index = 0; index < count; ++index) {
        *fptr++ = m_points.at(index).x();
        *fptr++ = m_points.at(index).y();
        *fptr++ = m_points.at(index).z();

        if (hasTextureCoordinates()) {
            *fptr++ = m_texCoords.at(index).x();
            *fptr++ = m_texCoords.at(index).y();
        }

        if (hasNormals()) {
            *fptr++ = m_normals.at(index).x();
            *fptr++ = m_normals.at(index).y();
            *fptr++ = m_normals.at(index).z();
        }

        if (hasTangents()) {
            *fptr++ = m_tangents.at(index).x();
            *fptr++ = m_tangents.at(index).y();
            *fptr++ = m_tangents.at(index).z();
            *fptr++ = m_tangents.at(index).w();
        }
    } // of buffer filling loop

    QBuffer *buf(new QBuffer(QBuffer::VertexBuffer));
    buf->setData(bufferBytes);

    if (m_geometry)
        qDebug(BaseGeometryLoaderLog, "Existing geometry instance getting overridden.");
    m_geometry = new QGeometry();

    QAttribute *positionAttribute = new QAttribute(buf, QAttribute::defaultPositionAttributeName(), QAttribute::Float, 3, count, 0, stride);
    m_geometry->addAttribute(positionAttribute);
    quint32 offset = sizeof(float) * 3;

    if (hasTextureCoordinates()) {
        QAttribute *texCoordAttribute = new QAttribute(buf, QAttribute::defaultTextureCoordinateAttributeName(),  QAttribute::Float, 2, count, offset, stride);
        m_geometry->addAttribute(texCoordAttribute);
        offset += sizeof(float) * 2;
    }

    if (hasNormals()) {
        QAttribute *normalAttribute = new QAttribute(buf, QAttribute::defaultNormalAttributeName(), QAttribute::Float, 3, count, offset, stride);
        m_geometry->addAttribute(normalAttribute);
        offset += sizeof(float) * 3;
    }

    if (hasTangents()) {
        QAttribute *tangentAttribute = new QAttribute(buf, QAttribute::defaultTangentAttributeName(),QAttribute::Float, 4, count, offset, stride);
        m_geometry->addAttribute(tangentAttribute);
        offset += sizeof(float) * 4;
    }

    QByteArray indexBytes;
    QAttribute::VertexBaseType ty;
    if (m_indices.size() < 65536) {
        // we can use USHORT
        ty = QAttribute::UnsignedShort;
        indexBytes.resize(m_indices.size() * sizeof(quint16));
        quint16 *usptr = reinterpret_cast<quint16*>(indexBytes.data());
        for (int i = 0; i < m_indices.size(); ++i)
            *usptr++ = static_cast<quint16>(m_indices.at(i));
    } else {
        // use UINT - no conversion needed, but let's ensure int is 32-bit!
        ty = QAttribute::UnsignedInt;
        Q_ASSERT(sizeof(int) == sizeof(quint32));
        indexBytes.resize(m_indices.size() * sizeof(quint32));
        memcpy(indexBytes.data(), reinterpret_cast<const char*>(m_indices.data()), indexBytes.size());
    }

    QBuffer *indexBuffer(new QBuffer(QBuffer::IndexBuffer));
    indexBuffer->setData(indexBytes);
    QAttribute *indexAttribute = new QAttribute(indexBuffer, ty, 1, m_indices.size());
    indexAttribute->setAttributeType(QAttribute::IndexAttribute);
    m_geometry->addAttribute(indexAttribute);
}

void BaseGeometryLoader::generateTangents(const QVector<QVector3D>& points,
                                          const QVector<QVector3D>& normals,
                                          const QVector<unsigned  int>& faces,
                                          const QVector<QVector2D>& texCoords,
                                          QVector<QVector4D>& tangents) const
{
    tangents.clear();
    QVector<QVector3D> tan1Accum;
    QVector<QVector3D> tan2Accum;

    tan1Accum.resize(points.size());
    tan2Accum.resize(points.size());
    tangents.resize(points.size());

    // Compute the tangent vector
    for (int i = 0; i < faces.size(); i += 3) {
        const QVector3D &p1 = points[ faces[i] ];
        const QVector3D &p2 = points[ faces[i+1] ];
        const QVector3D &p3 = points[ faces[i+2] ];

        const QVector2D &tc1 = texCoords[ faces[i] ];
        const QVector2D &tc2 = texCoords[ faces[i+1] ];
        const QVector2D &tc3 = texCoords[ faces[i+2] ];

        const QVector3D q1 = p2 - p1;
        const QVector3D q2 = p3 - p1;
        const float s1 = tc2.x() - tc1.x(), s2 = tc3.x() - tc1.x();
        const float t1 = tc2.y() - tc1.y(), t2 = tc3.y() - tc1.y();
        const float r = 1.0f / (s1 * t2 - s2 * t1);
        const QVector3D tan1((t2 * q1.x() - t1 * q2.x()) * r,
                             (t2 * q1.y() - t1 * q2.y()) * r,
                             (t2 * q1.z() - t1 * q2.z()) * r);
        const QVector3D tan2((s1 * q2.x() - s2 * q1.x()) * r,
                             (s1 * q2.y() - s2 * q1.y()) * r,
                             (s1 * q2.z() - s2 * q1.z()) * r);
        tan1Accum[ faces[i]   ] += tan1;
        tan1Accum[ faces[i+1] ] += tan1;
        tan1Accum[ faces[i+2] ] += tan1;
        tan2Accum[ faces[i]   ] += tan2;
        tan2Accum[ faces[i+1] ] += tan2;
        tan2Accum[ faces[i+2] ] += tan2;
    }

    for (int i = 0; i < points.size(); ++i) {
        const QVector3D &n = normals[i];
        const QVector3D &t1 = tan1Accum[i];
        const QVector3D &t2 = tan2Accum[i];

        // Gram-Schmidt orthogonalize
        tangents[i] = QVector4D(QVector3D(t1 - QVector3D::dotProduct(n, t1) * n).normalized(), 0.0f);

        // Store handedness in w
        tangents[i].setW((QVector3D::dotProduct(QVector3D::crossProduct(n, t1), t2) < 0.0f) ? -1.0f : 1.0f);
    }
}

void BaseGeometryLoader::center(QVector<QVector3D>& points)
{
    if (points.isEmpty())
        return;

    const QAxisAlignedBoundingBox bb(points);
    const QVector3D center = bb.center();

    // Translate center of the AABB to the origin
    for (int i = 0; i < points.size(); ++i) {
        QVector3D &point = points[i];
        point = point - center;
    }
}

} // namespace Qt3DRender

QT_END_NAMESPACE
