/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef GEOMETRYTESTHELPER_H
#define GEOMETRYTESTHELPER_H

#include <Qt3DRender/qattribute.h>
#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/qbufferdatagenerator.h>
#include <Qt3DRender/qgeometry.h>

inline void generateGeometry(Qt3DRender::QGeometry &geometry)
{
    // Get all attributes
    const QVector<Qt3DRender::QAttribute *> attributes = geometry.attributes();

    // Get all unique data generators from the buffers referenced by the attributes
    QHash<Qt3DRender::QBufferDataGeneratorPtr, Qt3DRender::QBuffer *> dataGenerators;
    for (const auto attribute : attributes) {
        const auto dataGenerator = attribute->buffer()->dataGenerator();
        if (!dataGenerators.contains(dataGenerator))
            dataGenerators.insert(dataGenerator, attribute->buffer());
    }

    // Generate data for each buffer
    const auto end = dataGenerators.end();
    for (auto it = dataGenerators.begin(); it != end; ++it) {
        Qt3DRender::QBufferDataGeneratorPtr dataGenerator = it.key();
        const QByteArray data = (*dataGenerator)();

        Qt3DRender::QBuffer *buffer = it.value();
        buffer->setData(data);
    }
}

template<typename IndexType>
IndexType extractIndexData(Qt3DRender::QAttribute *attribute, int index)
{
    // Get the raw data
    const IndexType *typedData = reinterpret_cast<const IndexType *>(attribute->buffer()->data().constData());

    // Offset into the data taking stride and offset into account
    const IndexType indexValue = *(typedData + index);
    return indexValue;
}

template<typename VertexType, typename IndexType>
VertexType extractVertexData(Qt3DRender::QAttribute *attribute, IndexType index)
{
    // Get the raw data
    const char *rawData = attribute->buffer()->data().constData();

    // Offset into the data taking stride and offset into account
    const char *vertexData = rawData + (index * attribute->byteStride() + attribute->byteOffset());

    // Construct vertex from the typed data
    VertexType vertex;
    const Qt3DRender::QAttribute::VertexBaseType type = attribute->vertexBaseType();
    switch (type)
    {
    case Qt3DRender::QAttribute::Float: {
        const float *typedVertexData = reinterpret_cast<const float *>(vertexData);
        const int components = attribute->vertexSize();
        for (int i = 0; i < components; ++i)
            vertex[i] = typedVertexData[i];
        break;

        // TODO: Handle other types as needed
    }

    default:
        qWarning() << "Unhandled type";
        Q_UNREACHABLE();
        break;
    }

    return vertex;
}

#endif // GEOMETRYTESTHELPER_H
