/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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

#include <QGuiApplication>

#include <Qt3DCore/QEntity>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QCameraLens>
#include <Qt3DCore/QTransform>
#include <Qt3DCore/QAspectEngine>

#include <Qt3DInput/QInputAspect>

#include <Qt3DRender/QRenderStateSet>
#include <Qt3DRender/QRenderAspect>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/QPerVertexColorMaterial>

#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QGeometry>
#include <Qt3DRender/QAttribute>
#include <Qt3DRender/QBuffer>

#include <QPropertyAnimation>
#include <Qt3DExtras/qt3dwindow.h>
#include <Qt3DExtras/qorbitcameracontroller.h>

namespace {

struct IndirectElementDrawBuffer{ // Element Indirect
    uint count;
    uint instancesCount;
    uint firstIndex;
    uint baseVertex;
    uint baseInstance;
};

struct IndirectArrayDrawBuffer{ // Array Indirect
    uint count;
    uint instancesCount;
    uint first;
    uint baseInstance;
};

} // anonymous

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    Qt3DExtras::Qt3DWindow view;
    view.defaultFrameGraph()->setClearColor(QColor::fromRgbF(0.0, 0.5, 1.0, 1.0));

    // Root entity
    Qt3DCore::QEntity *rootEntity = new Qt3DCore::QEntity();

    // Camera
    Qt3DRender::QCamera *cameraEntity = view.camera();

    cameraEntity->lens()->setPerspectiveProjection(45.0f, 16.0f/9.0f, 0.1f, 1000.0f);
    cameraEntity->setPosition(QVector3D(0, 0, 40.0f));
    cameraEntity->setUpVector(QVector3D(0, 1, 0));
    cameraEntity->setViewCenter(QVector3D(0, 0, 0));

    // For camera controls
    Qt3DExtras::QOrbitCameraController *camController = new Qt3DExtras::QOrbitCameraController(rootEntity);
    camController->setCamera(cameraEntity);

    // Material
    Qt3DRender::QMaterial *material = new Qt3DExtras::QPerVertexColorMaterial(rootEntity);

    // vec3 for position
    // vec3 for colors
    // vec3 for normals

    /*          2
               /|\
              / | \
             / /3\ \
             0/___\ 1
    */
    // Vertices
    QVector3D v0(-1.0f, 0.0f, -1.0f);
    QVector3D v1(1.0f, 0.0f, -1.0f);
    QVector3D v2(0.0f, 1.0f, 0.0f);
    QVector3D v3(0.0f, 0.0f, 1.0f);

    // Faces Normals
    QVector3D n023 = QVector3D::normal(v0, v2, v3);
    QVector3D n012 = QVector3D::normal(v0, v1, v2);
    QVector3D n310 = QVector3D::normal(v3, v1, v0);
    QVector3D n132 = QVector3D::normal(v1, v3, v2);

    // Vector Normals
    QVector3D n0 = QVector3D(n023 + n012 + n310).normalized();
    QVector3D n1 = QVector3D(n132 + n012 + n310).normalized();
    QVector3D n2 = QVector3D(n132 + n012 + n023).normalized();
    QVector3D n3 = QVector3D(n132 + n310 + n023).normalized();

    // Colors
    QVector3D red(1.0f, 0.0f, 0.0f);
    QVector3D green(0.0f, 1.0f, 0.0f);
    QVector3D blue(0.0f, 0.0f, 1.0f);
    QVector3D white(1.0f, 1.0f, 1.0f);

    QVector<QVector3D> vertices = QVector<QVector3D>()
            << v0 << n0 << red
            << v1 << n1 << blue
            << v2 << n2 << green
            << v3 << n3 << white;

    //     DrawElementIndirect
    {
        Qt3DCore::QEntity *customDrawElementIndirectEntity = new Qt3DCore::QEntity(rootEntity);

        // Transform
        Qt3DCore::QTransform *transform = new Qt3DCore::QTransform;
        transform->setScale(8.0f);
        transform->setTranslation(QVector3D(-10.0f, 0.0, 0.0f));

        // Custom Mesh (TetraHedron)
        Qt3DRender::QGeometryRenderer *customMeshRenderer = new Qt3DRender::QGeometryRenderer;
        Qt3DRender::QGeometry *customGeometry = new Qt3DRender::QGeometry(customMeshRenderer);

        Qt3DRender::QBuffer *vertexDataBuffer = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::VertexBuffer, customGeometry);
        Qt3DRender::QBuffer *indexDataBuffer = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::IndexBuffer, customGeometry);
        Qt3DRender::QBuffer *indirectDrawDataBuffer = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::DrawIndirectBuffer, customGeometry);

        // 4 distinct vertices
        QByteArray vertexBufferData;
        vertexBufferData.resize(4 * (3 + 3 + 3) * sizeof(float));

        float *rawVertexArray = reinterpret_cast<float *>(vertexBufferData.data());
        int idx = 0;

        Q_FOREACH (const QVector3D &v, vertices) {
            rawVertexArray[idx++] = v.x();
            rawVertexArray[idx++] = v.y();
            rawVertexArray[idx++] = v.z();
        }

        // Indices (12)
        QByteArray indexBufferData;
        indexBufferData.resize(4 * 3 * sizeof(ushort));
        ushort *rawIndexArray = reinterpret_cast<ushort *>(indexBufferData.data());

        // Front
        rawIndexArray[0] = 0;
        rawIndexArray[1] = 1;
        rawIndexArray[2] = 2;
        // Bottom
        rawIndexArray[3] = 3;
        rawIndexArray[4] = 1;
        rawIndexArray[5] = 0;
        // Left
        rawIndexArray[6] = 0;
        rawIndexArray[7] = 2;
        rawIndexArray[8] = 3;
        // Right
        rawIndexArray[9] = 1;
        rawIndexArray[10] = 3;
        rawIndexArray[11] = 2;


        QByteArray indirectBufferData;
        indirectBufferData.resize(sizeof(IndirectElementDrawBuffer));
        IndirectElementDrawBuffer *indirectStruct = reinterpret_cast<IndirectElementDrawBuffer *>(indirectBufferData.data());
        indirectStruct->count = 12U;
        indirectStruct->firstIndex = 0U;
        indirectStruct->baseInstance = 0U;
        indirectStruct->baseVertex = 0U;
        indirectStruct->instancesCount = 1U;

        vertexDataBuffer->setData(vertexBufferData);
        indexDataBuffer->setData(indexBufferData);
        indirectDrawDataBuffer->setData(indirectBufferData);

        // Attributes
        Qt3DRender::QAttribute *positionAttribute = new Qt3DRender::QAttribute();
        positionAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
        positionAttribute->setBuffer(vertexDataBuffer);
        positionAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
        positionAttribute->setVertexSize(3);
        positionAttribute->setByteOffset(0);
        positionAttribute->setByteStride(9 * sizeof(float));
        positionAttribute->setCount(4);
        positionAttribute->setName(Qt3DRender::QAttribute::defaultPositionAttributeName());

        Qt3DRender::QAttribute *normalAttribute = new Qt3DRender::QAttribute();
        normalAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
        normalAttribute->setBuffer(vertexDataBuffer);
        normalAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
        normalAttribute->setVertexSize(3);
        normalAttribute->setByteOffset(3 * sizeof(float));
        normalAttribute->setByteStride(9 * sizeof(float));
        normalAttribute->setCount(4);
        normalAttribute->setName(Qt3DRender::QAttribute::defaultNormalAttributeName());

        Qt3DRender::QAttribute *colorAttribute = new Qt3DRender::QAttribute();
        colorAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
        colorAttribute->setBuffer(vertexDataBuffer);
        colorAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
        colorAttribute->setVertexSize(3);
        colorAttribute->setByteOffset(6 * sizeof(float));
        colorAttribute->setByteStride(9 * sizeof(float));
        colorAttribute->setCount(4);
        colorAttribute->setName(Qt3DRender::QAttribute::defaultColorAttributeName());

        Qt3DRender::QAttribute *indexAttribute = new Qt3DRender::QAttribute();
        indexAttribute->setAttributeType(Qt3DRender::QAttribute::IndexAttribute);
        indexAttribute->setBuffer(indexDataBuffer);
        indexAttribute->setVertexBaseType(Qt3DRender::QAttribute::UnsignedShort);
        indexAttribute->setVertexSize(1);
        indexAttribute->setByteOffset(0);
        indexAttribute->setByteStride(0);
        indexAttribute->setCount(12);

        Qt3DRender::QAttribute *indirectAttribute = new Qt3DRender::QAttribute();
        indirectAttribute->setAttributeType(Qt3DRender::QAttribute::DrawIndirectAttribute);
        indirectAttribute->setBuffer(indirectDrawDataBuffer);
        indirectAttribute->setByteOffset(0);
        indirectAttribute->setByteStride(0);
        indirectAttribute->setCount(1);

        customGeometry->addAttribute(positionAttribute);
        customGeometry->addAttribute(normalAttribute);
        customGeometry->addAttribute(colorAttribute);
        customGeometry->addAttribute(indexAttribute);
        customGeometry->addAttribute(indirectAttribute);

        customMeshRenderer->setInstanceCount(1);
        customMeshRenderer->setIndexOffset(0);
        customMeshRenderer->setFirstInstance(0);
        customMeshRenderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);
        customMeshRenderer->setGeometry(customGeometry);
        // 4 faces of 3 points
        customMeshRenderer->setVertexCount(12);

        customDrawElementIndirectEntity->addComponent(customMeshRenderer);
        customDrawElementIndirectEntity->addComponent(transform);
        customDrawElementIndirectEntity->addComponent(material);
    }

    // DrawArrayIndirect
    {
        Qt3DCore::QEntity *customDrawArrayIndirectEntity = new Qt3DCore::QEntity(rootEntity);

        // Transform
        Qt3DCore::QTransform *transform = new Qt3DCore::QTransform;
        transform->setScale(8.0f);
        transform->setTranslation(QVector3D(10.0f, 0.0, 0.0f));

        // Custom Mesh (TetraHedron)
        Qt3DRender::QGeometryRenderer *customMeshRenderer = new Qt3DRender::QGeometryRenderer;
        Qt3DRender::QGeometry *customGeometry = new Qt3DRender::QGeometry(customMeshRenderer);

        Qt3DRender::QBuffer *vertexDataBuffer = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::VertexBuffer, customGeometry);
        Qt3DRender::QBuffer *indirectDrawDataBuffer = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::DrawIndirectBuffer, customGeometry);


        QVector<QVector3D> orderedPositionVertices;
        // 12 vertices with position, normal, color
        orderedPositionVertices.reserve(12 * (1 + 1 + 1));

        orderedPositionVertices << v0 << n0 << red;
        orderedPositionVertices << v1 << n1 << red;
        orderedPositionVertices << v2 << n2 << red;

        orderedPositionVertices << v3 << n3 << green;
        orderedPositionVertices << v1 << n1 << green;
        orderedPositionVertices << v0 << n0 << green;

        orderedPositionVertices << v0 << n0 << blue;
        orderedPositionVertices << v2 << n2 << blue;
        orderedPositionVertices << v3 << n3 << blue;

        orderedPositionVertices << v1 << n1 << white;
        orderedPositionVertices << v3 << n3 << white;
        orderedPositionVertices << v2 << n2 << white;

        QByteArray vertexBufferData;
        vertexBufferData.resize(orderedPositionVertices.size() * sizeof(QVector3D));
        memcpy(vertexBufferData.data(), orderedPositionVertices.data(), orderedPositionVertices.size() * sizeof(QVector3D));

        QByteArray indirectBufferData;
        indirectBufferData.resize(sizeof(IndirectArrayDrawBuffer));
        IndirectArrayDrawBuffer *indirectStruct = reinterpret_cast<IndirectArrayDrawBuffer *>(indirectBufferData.data());
        indirectStruct->count = 12U;
        indirectStruct->baseInstance = 0U;
        indirectStruct->instancesCount = 1U;
        indirectStruct->first = 0U;

        vertexDataBuffer->setData(vertexBufferData);
        indirectDrawDataBuffer->setData(indirectBufferData);

        // Attributes
        Qt3DRender::QAttribute *positionAttribute = new Qt3DRender::QAttribute();
        positionAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
        positionAttribute->setBuffer(vertexDataBuffer);
        positionAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
        positionAttribute->setVertexSize(3);
        positionAttribute->setByteOffset(0);
        positionAttribute->setByteStride(9 * sizeof(float));
        positionAttribute->setCount(12);
        positionAttribute->setName(Qt3DRender::QAttribute::defaultPositionAttributeName());

        Qt3DRender::QAttribute *normalAttribute = new Qt3DRender::QAttribute();
        normalAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
        normalAttribute->setBuffer(vertexDataBuffer);
        normalAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
        normalAttribute->setVertexSize(3);
        normalAttribute->setByteOffset(3 * sizeof(float));
        normalAttribute->setByteStride(9 * sizeof(float));
        normalAttribute->setCount(12);
        normalAttribute->setName(Qt3DRender::QAttribute::defaultNormalAttributeName());

        Qt3DRender::QAttribute *colorAttribute = new Qt3DRender::QAttribute();
        colorAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
        colorAttribute->setBuffer(vertexDataBuffer);
        colorAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
        colorAttribute->setVertexSize(3);
        colorAttribute->setByteOffset(6 * sizeof(float));
        colorAttribute->setByteStride(9 * sizeof(float));
        colorAttribute->setCount(12);
        colorAttribute->setName(Qt3DRender::QAttribute::defaultColorAttributeName());

        Qt3DRender::QAttribute *indirectAttribute = new Qt3DRender::QAttribute();
        indirectAttribute->setAttributeType(Qt3DRender::QAttribute::DrawIndirectAttribute);
        indirectAttribute->setBuffer(indirectDrawDataBuffer);
        indirectAttribute->setByteOffset(0);
        indirectAttribute->setByteStride(0);
        indirectAttribute->setCount(1);

        customGeometry->addAttribute(positionAttribute);
        customGeometry->addAttribute(normalAttribute);
        customGeometry->addAttribute(colorAttribute);
        customGeometry->addAttribute(indirectAttribute);

        customMeshRenderer->setInstanceCount(1);
        customMeshRenderer->setIndexOffset(0);
        customMeshRenderer->setFirstInstance(0);
        customMeshRenderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);
        customMeshRenderer->setGeometry(customGeometry);
        // 4 faces of 3 points
        customMeshRenderer->setVertexCount(12);

        customDrawArrayIndirectEntity->addComponent(customMeshRenderer);
        customDrawArrayIndirectEntity->addComponent(transform);
        customDrawArrayIndirectEntity->addComponent(material);
    }

    view.setRootEntity(rootEntity);
    view.show();

    return app.exec();
}
