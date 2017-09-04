/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include <QtTest/QTest>
#include <qbackendnodetester.h>
#include <Qt3DRender/qgeometryrenderer.h>
#include <Qt3DRender/qbuffer.h>
#include <private/trianglesvisitor_p.h>
#include <private/nodemanagers_p.h>
#include <private/managers_p.h>
#include <private/geometryrenderer_p.h>
#include <private/geometryrenderermanager_p.h>
#include <private/buffermanager_p.h>
#include "testrenderer.h"

using namespace Qt3DRender::Render;

class TestReader : public CoordinateReader
{
public:
    TestReader(NodeManagers *manager)
        : CoordinateReader(manager)
    {

    }
    NodeManagers *manager() const
    {
        return m_manager;
    }

    Attribute *attribute() const
    {
        return m_attribute;
    }

    Buffer *buffer() const
    {
        return m_buffer;
    }

    BufferInfo bufferInfo() const
    {
        return m_bufferInfo;
    }
    bool verifyCoordinate(uint index, QVector4D value)
    {
        return qFuzzyCompare(getCoordinate(index), value);
    }
};

class tst_CoordinateReader : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:

    void checkInitialize()
    {
        // WHEN
        QScopedPointer<NodeManagers> nodeManagers(new NodeManagers());
        TestReader reader(nodeManagers.data());

        // THEN
        QCOMPARE(reader.manager(), nodeManagers.data());
    }

    void checkSetEmptyGeometry()
    {
        QScopedPointer<NodeManagers> nodeManagers(new NodeManagers());
        QScopedPointer<Qt3DRender::QGeometryRenderer> geometryRenderer(
                    new Qt3DRender::QGeometryRenderer());
        TestReader reader(nodeManagers.data());
        TestRenderer renderer;

        GeometryRenderer *backendRenderer = nodeManagers->geometryRendererManager()
                                            ->getOrCreateResource(geometryRenderer->id());
        backendRenderer->setRenderer(&renderer);
        backendRenderer->setManager(nodeManagers->geometryRendererManager());
        simulateInitialization(geometryRenderer.data(), backendRenderer);

        // WHEN
        bool ret = reader.setGeometry(backendRenderer, QString(""));

        // THEN
        QCOMPARE(ret, false);
        QCOMPARE(reader.attribute(), nullptr);
        QCOMPARE(reader.buffer(), nullptr);
    }

    void checkSetGeometry()
    {
        QScopedPointer<NodeManagers> nodeManagers(new NodeManagers());
        Qt3DRender::QGeometry *geometry = new Qt3DRender::QGeometry();
        QScopedPointer<Qt3DRender::QGeometryRenderer> geometryRenderer(
                    new Qt3DRender::QGeometryRenderer());
        QScopedPointer<Qt3DRender::QAttribute> positionAttribute(new Qt3DRender::QAttribute());
        QScopedPointer<Qt3DRender::QBuffer> dataBuffer(new Qt3DRender::QBuffer());
        TestReader reader(nodeManagers.data());
        TestRenderer renderer;

        QByteArray data;
        data.resize(sizeof(float) * 3 * 3 * 2);
        float *dataPtr = reinterpret_cast<float *>(data.data());
        dataPtr[0] = 0;
        dataPtr[1] = 0;
        dataPtr[2] = 1.0f;
        dataPtr[3] = 1.0f;
        dataPtr[4] = 0;
        dataPtr[5] = 0;
        dataPtr[6] = 0;
        dataPtr[7] = 1.0f;
        dataPtr[8] = 0;

        dataPtr[9] = 0;
        dataPtr[10] = 0;
        dataPtr[11] = 1.0f;
        dataPtr[12] = 1.0f;
        dataPtr[13] = 0;
        dataPtr[14] = 0;
        dataPtr[15] = 0;
        dataPtr[16] = 1.0f;
        dataPtr[17] = 0;
        dataBuffer->setData(data);
        Buffer *backendBuffer = nodeManagers->bufferManager()->getOrCreateResource(dataBuffer->id());
        backendBuffer->setRenderer(&renderer);
        backendBuffer->setManager(nodeManagers->bufferManager());
        simulateInitialization(dataBuffer.data(), backendBuffer);

        positionAttribute->setBuffer(dataBuffer.data());
        positionAttribute->setName(Qt3DRender::QAttribute::defaultPositionAttributeName());
        positionAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
        positionAttribute->setVertexSize(3);
        positionAttribute->setCount(6);
        positionAttribute->setByteStride(3 * 4);
        positionAttribute->setByteOffset(0);
        positionAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
        geometry->addAttribute(positionAttribute.data());

        geometryRenderer->setGeometry(geometry);
        geometryRenderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);

        Attribute *backendAttribute = nodeManagers->attributeManager()
                                      ->getOrCreateResource(positionAttribute->id());
        backendAttribute->setRenderer(&renderer);
        simulateInitialization(positionAttribute.data(), backendAttribute);

        Geometry *backendGeometry = nodeManagers->geometryManager()
                                    ->getOrCreateResource(geometry->id());
        backendGeometry->setRenderer(&renderer);
        simulateInitialization(geometry, backendGeometry);

        GeometryRenderer *backendRenderer = nodeManagers->geometryRendererManager()
                                            ->getOrCreateResource(geometryRenderer->id());
        backendRenderer->setRenderer(&renderer);
        backendRenderer->setManager(nodeManagers->geometryRendererManager());
        simulateInitialization(geometryRenderer.data(), backendRenderer);

        // WHEN
        bool ret = reader.setGeometry(backendRenderer,
                                      Qt3DRender::QAttribute::defaultPositionAttributeName());

        // THEN
        QCOMPARE(ret, true);
        QCOMPARE(reader.attribute(), backendAttribute);
        QCOMPARE(reader.buffer(), backendBuffer);
        QCOMPARE(reader.bufferInfo().type, Qt3DRender::QAttribute::Float);
        QCOMPARE(reader.bufferInfo().dataSize, 3u);
        QCOMPARE(reader.bufferInfo().count, 6u);
        QCOMPARE(reader.bufferInfo().byteStride, 12u);
        QCOMPARE(reader.bufferInfo().byteOffset, 0u);
    }

    void testReadCoordinate()
    {
        QScopedPointer<NodeManagers> nodeManagers(new NodeManagers());
        Qt3DRender::QGeometry *geometry = new Qt3DRender::QGeometry();
        QScopedPointer<Qt3DRender::QGeometryRenderer> geometryRenderer(
                    new Qt3DRender::QGeometryRenderer());
        QScopedPointer<Qt3DRender::QAttribute> positionAttribute(new Qt3DRender::QAttribute());
        QScopedPointer<Qt3DRender::QBuffer> dataBuffer(new Qt3DRender::QBuffer());
        TestReader reader(nodeManagers.data());
        TestRenderer renderer;

        QByteArray data;
        data.resize(sizeof(float) * 3 * 3 * 2);
        float *dataPtr = reinterpret_cast<float *>(data.data());
        dataPtr[0] = 0;
        dataPtr[1] = 0;
        dataPtr[2] = 1.0f;
        dataPtr[3] = 1.0f;
        dataPtr[4] = 0;
        dataPtr[5] = 0;
        dataPtr[6] = 0;
        dataPtr[7] = 1.0f;
        dataPtr[8] = 0;

        dataPtr[9] = 0;
        dataPtr[10] = 0;
        dataPtr[11] = 1.0f;
        dataPtr[12] = 1.0f;
        dataPtr[13] = 0;
        dataPtr[14] = 0;
        dataPtr[15] = 0;
        dataPtr[16] = 1.0f;
        dataPtr[17] = 0;
        dataBuffer->setData(data);
        Buffer *backendBuffer = nodeManagers->bufferManager()->getOrCreateResource(dataBuffer->id());
        backendBuffer->setRenderer(&renderer);
        backendBuffer->setManager(nodeManagers->bufferManager());
        simulateInitialization(dataBuffer.data(), backendBuffer);

        positionAttribute->setBuffer(dataBuffer.data());
        positionAttribute->setName(Qt3DRender::QAttribute::defaultPositionAttributeName());
        positionAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
        positionAttribute->setVertexSize(3);
        positionAttribute->setCount(6);
        positionAttribute->setByteStride(3 * 4);
        positionAttribute->setByteOffset(0);
        positionAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
        geometry->addAttribute(positionAttribute.data());

        geometryRenderer->setGeometry(geometry);
        geometryRenderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);

        Attribute *backendAttribute = nodeManagers->attributeManager()
                                      ->getOrCreateResource(positionAttribute->id());
        backendAttribute->setRenderer(&renderer);
        simulateInitialization(positionAttribute.data(), backendAttribute);

        Geometry *backendGeometry = nodeManagers->geometryManager()
                                    ->getOrCreateResource(geometry->id());
        backendGeometry->setRenderer(&renderer);
        simulateInitialization(geometry, backendGeometry);

        GeometryRenderer *backendRenderer = nodeManagers->geometryRendererManager()
                                            ->getOrCreateResource(geometryRenderer->id());
        backendRenderer->setRenderer(&renderer);
        backendRenderer->setManager(nodeManagers->geometryRendererManager());
        simulateInitialization(geometryRenderer.data(), backendRenderer);

        // WHEN
        bool ret = reader.setGeometry(backendRenderer,
                                      Qt3DRender::QAttribute::defaultPositionAttributeName());

        // THEN
        QCOMPARE(ret, true);

        QVERIFY(reader.verifyCoordinate(0, QVector4D(0, 0, 1, 1)));
        QVERIFY(reader.verifyCoordinate(1, QVector4D(1, 0, 0, 1)));
        QVERIFY(reader.verifyCoordinate(2, QVector4D(0, 1, 0, 1)));
        QVERIFY(reader.verifyCoordinate(3, QVector4D(0, 0, 1, 1)));
        QVERIFY(reader.verifyCoordinate(4, QVector4D(1, 0, 0, 1)));
        QVERIFY(reader.verifyCoordinate(5, QVector4D(0, 1, 0, 1)));
    }

    void testReadCoordinateVec4()
    {
        QScopedPointer<NodeManagers> nodeManagers(new NodeManagers());
        Qt3DRender::QGeometry *geometry = new Qt3DRender::QGeometry();
        QScopedPointer<Qt3DRender::QGeometryRenderer> geometryRenderer(
                    new Qt3DRender::QGeometryRenderer());
        QScopedPointer<Qt3DRender::QAttribute> positionAttribute(new Qt3DRender::QAttribute());
        QScopedPointer<Qt3DRender::QBuffer> dataBuffer(new Qt3DRender::QBuffer());
        TestReader reader(nodeManagers.data());
        TestRenderer renderer;

        QByteArray data;
        data.resize(sizeof(float) * 4 * 3 * 2);
        float *dataPtr = reinterpret_cast<float *>(data.data());
        dataPtr[0] = 0;
        dataPtr[1] = 0;
        dataPtr[2] = 1.0f;
        dataPtr[3] = 1.0f;

        dataPtr[4] = 1.0f;
        dataPtr[5] = 0;
        dataPtr[6] = 0;
        dataPtr[7] = 1.0f;

        dataPtr[8] = 0;
        dataPtr[9] = 1.0f;
        dataPtr[10] = 0;
        dataPtr[11] = 0;

        dataPtr[12] = 0;
        dataPtr[13] = 0;
        dataPtr[14] = 1.0f;
        dataPtr[15] = 0;

        dataPtr[16] = 1.0f;
        dataPtr[17] = 0;
        dataPtr[18] = 0;
        dataPtr[19] = 0;

        dataPtr[20] = 0;
        dataPtr[21] = 1.0f;
        dataPtr[22] = 0;
        dataPtr[23] = 1.0f;

        dataBuffer->setData(data);
        Buffer *backendBuffer = nodeManagers->bufferManager()->getOrCreateResource(dataBuffer->id());
        backendBuffer->setRenderer(&renderer);
        backendBuffer->setManager(nodeManagers->bufferManager());
        simulateInitialization(dataBuffer.data(), backendBuffer);

        positionAttribute->setBuffer(dataBuffer.data());
        positionAttribute->setName(Qt3DRender::QAttribute::defaultPositionAttributeName());
        positionAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
        positionAttribute->setVertexSize(4);
        positionAttribute->setCount(6);
        positionAttribute->setByteStride(4 * 4);
        positionAttribute->setByteOffset(0);
        positionAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
        geometry->addAttribute(positionAttribute.data());

        geometryRenderer->setGeometry(geometry);
        geometryRenderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);

        Attribute *backendAttribute = nodeManagers->attributeManager()
                                      ->getOrCreateResource(positionAttribute->id());
        backendAttribute->setRenderer(&renderer);
        simulateInitialization(positionAttribute.data(), backendAttribute);

        Geometry *backendGeometry = nodeManagers->geometryManager()
                                    ->getOrCreateResource(geometry->id());
        backendGeometry->setRenderer(&renderer);
        simulateInitialization(geometry, backendGeometry);

        GeometryRenderer *backendRenderer = nodeManagers->geometryRendererManager()
                                            ->getOrCreateResource(geometryRenderer->id());
        backendRenderer->setRenderer(&renderer);
        backendRenderer->setManager(nodeManagers->geometryRendererManager());
        simulateInitialization(geometryRenderer.data(), backendRenderer);

        // WHEN
        bool ret = reader.setGeometry(backendRenderer,
                                      Qt3DRender::QAttribute::defaultPositionAttributeName());

        // THEN
        QCOMPARE(ret, true);

        QVERIFY(reader.verifyCoordinate(0, QVector4D(0, 0, 1, 1)));
        QVERIFY(reader.verifyCoordinate(1, QVector4D(1, 0, 0, 1)));
        QVERIFY(reader.verifyCoordinate(2, QVector4D(0, 1, 0, 0)));
        QVERIFY(reader.verifyCoordinate(3, QVector4D(0, 0, 1, 0)));
        QVERIFY(reader.verifyCoordinate(4, QVector4D(1, 0, 0, 0)));
        QVERIFY(reader.verifyCoordinate(5, QVector4D(0, 1, 0, 1)));
    }

    void testReadCoordinateFromAttribute()
    {
        QScopedPointer<NodeManagers> nodeManagers(new NodeManagers());
        Qt3DRender::QGeometry *geometry = new Qt3DRender::QGeometry();
        QScopedPointer<Qt3DRender::QGeometryRenderer> geometryRenderer(
                    new Qt3DRender::QGeometryRenderer());
        QScopedPointer<Qt3DRender::QAttribute> positionAttribute(new Qt3DRender::QAttribute());
        QScopedPointer<Qt3DRender::QAttribute> texcoordAttribute(new Qt3DRender::QAttribute());
        QScopedPointer<Qt3DRender::QBuffer> dataBuffer(new Qt3DRender::QBuffer());
        TestReader reader(nodeManagers.data());
        TestRenderer renderer;

        QByteArray data;
        data.resize(sizeof(float) * 3 * 3 * 2);
        float *dataPtr = reinterpret_cast<float *>(data.data());
        dataPtr[0] = 0;
        dataPtr[1] = 0;
        dataPtr[2] = 1.0f;

        dataPtr[3] = 1.0f;
        dataPtr[4] = 0;
        dataPtr[5] = 0;

        dataPtr[6] = 0;
        dataPtr[7] = 1.0f;
        dataPtr[8] = 0;

        dataPtr[9] = 0;
        dataPtr[10] = 0;
        dataPtr[11] = 1.0f;

        dataPtr[12] = 1.0f;
        dataPtr[13] = 0;
        dataPtr[14] = 0;

        dataPtr[15] = 0;
        dataPtr[16] = 1.0f;
        dataPtr[17] = 0;

        dataBuffer->setData(data);
        Buffer *backendBuffer = nodeManagers->bufferManager()->getOrCreateResource(dataBuffer->id());
        backendBuffer->setRenderer(&renderer);
        backendBuffer->setManager(nodeManagers->bufferManager());
        simulateInitialization(dataBuffer.data(), backendBuffer);

        positionAttribute->setBuffer(dataBuffer.data());
        positionAttribute->setName(Qt3DRender::QAttribute::defaultPositionAttributeName());
        positionAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
        positionAttribute->setVertexSize(3);
        positionAttribute->setCount(3);
        positionAttribute->setByteStride(3 * 4 * 2);
        positionAttribute->setByteOffset(0);
        positionAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
        geometry->addAttribute(positionAttribute.data());

        texcoordAttribute->setBuffer(dataBuffer.data());
        texcoordAttribute->setName(Qt3DRender::QAttribute::defaultTextureCoordinateAttributeName());
        texcoordAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
        texcoordAttribute->setVertexSize(3);
        texcoordAttribute->setCount(6);
        texcoordAttribute->setByteStride(3 * 4 * 2);
        texcoordAttribute->setByteOffset(3 * 4);
        texcoordAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
        geometry->addAttribute(texcoordAttribute.data());

        geometryRenderer->setGeometry(geometry);
        geometryRenderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);

        Attribute *backendAttribute = nodeManagers->attributeManager()->getOrCreateResource(
                                        positionAttribute->id());
        backendAttribute->setRenderer(&renderer);
        simulateInitialization(positionAttribute.data(), backendAttribute);

        Attribute *backendTexcoordAttribute = nodeManagers->attributeManager()
                                              ->getOrCreateResource(texcoordAttribute->id());
        backendTexcoordAttribute->setRenderer(&renderer);
        simulateInitialization(texcoordAttribute.data(), backendTexcoordAttribute);

        Geometry *backendGeometry = nodeManagers->geometryManager()
                                    ->getOrCreateResource(geometry->id());
        backendGeometry->setRenderer(&renderer);
        simulateInitialization(geometry, backendGeometry);

        GeometryRenderer *backendRenderer = nodeManagers->geometryRendererManager()
                                            ->getOrCreateResource(geometryRenderer->id());
        backendRenderer->setRenderer(&renderer);
        backendRenderer->setManager(nodeManagers->geometryRendererManager());
        simulateInitialization(geometryRenderer.data(), backendRenderer);

        // WHEN
        bool ret = reader.setGeometry(backendRenderer,
                                      Qt3DRender::QAttribute::defaultPositionAttributeName());

        // THEN
        QCOMPARE(ret, true);

        QVERIFY(reader.verifyCoordinate(0, QVector4D(0, 0, 1, 1)));
        QVERIFY(reader.verifyCoordinate(1, QVector4D(0, 1, 0, 1)));
        QVERIFY(reader.verifyCoordinate(2, QVector4D(1, 0, 0, 1)));

        // WHEN
        ret = reader.setGeometry(backendRenderer,
                                 Qt3DRender::QAttribute::defaultTextureCoordinateAttributeName());

        // THEN
        QCOMPARE(ret, true);

        QVERIFY(reader.verifyCoordinate(0, QVector4D(1, 0, 0, 1)));
        QVERIFY(reader.verifyCoordinate(1, QVector4D(0, 0, 1, 1)));
        QVERIFY(reader.verifyCoordinate(2, QVector4D(0, 1, 0, 1)));
    }

};

QTEST_MAIN(tst_CoordinateReader)

#include "tst_coordinatereader.moc"
