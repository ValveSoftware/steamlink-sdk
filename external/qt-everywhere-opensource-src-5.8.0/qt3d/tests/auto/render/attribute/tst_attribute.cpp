/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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
#include <Qt3DRender/private/attribute_p.h>
#include <Qt3DRender/qbuffer.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include "testrenderer.h"

class tst_Attribute : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT
private Q_SLOTS:

    void checkPeerPropertyMirroring()
    {
        // GIVEN
        Qt3DRender::Render::Attribute renderAttribute;

        Qt3DRender::QAttribute attribute;
        attribute.setAttributeType(Qt3DRender::QAttribute::IndexAttribute);
        attribute.setByteOffset(1200);
        attribute.setByteStride(883);
        attribute.setCount(427);
        attribute.setDivisor(305);
        attribute.setName(QStringLiteral("C3"));
        attribute.setVertexBaseType(Qt3DRender::QAttribute::UnsignedShort);
        attribute.setVertexSize(3);

        Qt3DRender::QBuffer buffer(Qt3DRender::QBuffer::IndexBuffer);
        buffer.setUsage(Qt3DRender::QBuffer::DynamicCopy);
        buffer.setData(QByteArrayLiteral("Corvette"));
        attribute.setBuffer(&buffer);

        // WHEN
        simulateInitialization(&attribute, &renderAttribute);

        // THEN
        QCOMPARE(renderAttribute.peerId(), attribute.id());
        QCOMPARE(renderAttribute.isDirty(), true);
        QCOMPARE(renderAttribute.vertexBaseType(), attribute.vertexBaseType());
        QCOMPARE(renderAttribute.vertexSize(), attribute.vertexSize());
        QCOMPARE(renderAttribute.attributeType(), attribute.attributeType());
        QCOMPARE(renderAttribute.byteOffset(), attribute.byteOffset());
        QCOMPARE(renderAttribute.byteStride(), attribute.byteStride());
        QCOMPARE(renderAttribute.count(), attribute.count());
        QCOMPARE(renderAttribute.divisor(), attribute.divisor());
        QCOMPARE(renderAttribute.name(), attribute.name());
        QCOMPARE(renderAttribute.bufferId(), buffer.id());
    }

    void checkInitialAndCleanedUpState()
    {
        // GIVEN
        TestRenderer renderer;
        Qt3DRender::Render::Attribute renderAttribute;
        renderAttribute.setRenderer(&renderer);

        // THEN
        QVERIFY(renderAttribute.peerId().isNull());
        QVERIFY(renderAttribute.bufferId().isNull());
        QVERIFY(renderAttribute.name().isEmpty());
        QCOMPARE(renderAttribute.isDirty(), false);
        QCOMPARE(renderAttribute.vertexBaseType(), Qt3DRender::QAttribute::Float);
        QCOMPARE(renderAttribute.vertexSize(), 1U);
        QCOMPARE(renderAttribute.attributeType(), Qt3DRender::QAttribute::VertexAttribute);
        QCOMPARE(renderAttribute.byteOffset(), 0U);
        QCOMPARE(renderAttribute.byteStride(), 0U);
        QCOMPARE(renderAttribute.count(), 0U);
        QCOMPARE(renderAttribute.divisor(), 0U);

        // GIVEN
        Qt3DRender::QAttribute attribute;
        attribute.setAttributeType(Qt3DRender::QAttribute::IndexAttribute);
        attribute.setByteOffset(1200);
        attribute.setByteStride(883);
        attribute.setCount(427);
        attribute.setDivisor(305);
        attribute.setName(QStringLiteral("C3"));
        attribute.setVertexBaseType(Qt3DRender::QAttribute::Double);
        attribute.setVertexSize(4);
        Qt3DRender::QBuffer buffer(Qt3DRender::QBuffer::IndexBuffer);
        buffer.setUsage(Qt3DRender::QBuffer::DynamicCopy);
        buffer.setData(QByteArrayLiteral("C7"));
        attribute.setBuffer(&buffer);

        // WHEN
        simulateInitialization(&attribute, &renderAttribute);
        renderAttribute.cleanup();

        // THEN
        QVERIFY(renderAttribute.bufferId().isNull());
        QVERIFY(renderAttribute.name().isEmpty());
        QCOMPARE(renderAttribute.isDirty(), false);
        QCOMPARE(renderAttribute.vertexBaseType(), Qt3DRender::QAttribute::Float);
        QCOMPARE(renderAttribute.vertexSize(), 1U);
        QCOMPARE(renderAttribute.attributeType(), Qt3DRender::QAttribute::VertexAttribute);
        QCOMPARE(renderAttribute.byteOffset(), 0U);
        QCOMPARE(renderAttribute.byteStride(), 0U);
        QCOMPARE(renderAttribute.count(), 0U);
        QCOMPARE(renderAttribute.divisor(), 0U);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        TestRenderer renderer;
        Qt3DRender::Render::Attribute renderAttribute;
        renderAttribute.setRenderer(&renderer);

        QVERIFY(!renderAttribute.isDirty());

        // WHEN
        Qt3DCore::QPropertyUpdatedChangePtr updateChange(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(static_cast<int>(Qt3DRender::QAttribute::Int));
        updateChange->setPropertyName("vertexBaseType");
        renderAttribute.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderAttribute.vertexBaseType(), Qt3DRender::QAttribute::Int);
        QVERIFY(renderAttribute.isDirty());
        QVERIFY(renderer.dirtyBits() != 0);

        renderAttribute.unsetDirty();
        renderer.resetDirty();
        QVERIFY(!renderAttribute.isDirty());
        QVERIFY(!renderer.dirtyBits());

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(3);
        updateChange->setPropertyName("vertexSize");
        renderAttribute.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderAttribute.vertexSize(), 3U);
        QVERIFY(renderAttribute.isDirty());
        QVERIFY(renderer.dirtyBits() != 0);

        renderAttribute.unsetDirty();
        renderer.resetDirty();
        QVERIFY(!renderAttribute.isDirty());

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(static_cast<int>(Qt3DRender::QAttribute::IndexAttribute));
        updateChange->setPropertyName("attributeType");
        renderAttribute.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderAttribute.attributeType(), Qt3DRender::QAttribute::IndexAttribute);
        QVERIFY(renderAttribute.isDirty());
        QVERIFY(renderer.dirtyBits() != 0);

        renderAttribute.unsetDirty();
        renderer.resetDirty();
        QVERIFY(!renderAttribute.isDirty());

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(1340);
        updateChange->setPropertyName("count");
        renderAttribute.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderAttribute.count(), 1340U);
        QVERIFY(renderAttribute.isDirty());
        QVERIFY(renderer.dirtyBits() != 0);

        renderAttribute.unsetDirty();
        renderer.resetDirty();
        QVERIFY(!renderAttribute.isDirty());

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(QStringLiteral("L88"));
        updateChange->setPropertyName("name");
        renderAttribute.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderAttribute.name(), QStringLiteral("L88"));
        QVERIFY(renderAttribute.isDirty());
        QVERIFY(renderer.dirtyBits() != 0);

        renderAttribute.unsetDirty();
        renderer.resetDirty();
        QVERIFY(!renderAttribute.isDirty());

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(555);
        updateChange->setPropertyName("byteOffset");
        renderAttribute.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderAttribute.byteOffset(), 555U);
        QVERIFY(renderAttribute.isDirty());
        QVERIFY(renderer.dirtyBits() != 0);

        renderAttribute.unsetDirty();
        renderer.resetDirty();
        QVERIFY(!renderAttribute.isDirty());

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(454);
        updateChange->setPropertyName("byteStride");
        renderAttribute.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderAttribute.byteStride(), 454U);
        QVERIFY(renderAttribute.isDirty());
        QVERIFY(renderer.dirtyBits() != 0);

        renderAttribute.unsetDirty();
        renderer.resetDirty();
        QVERIFY(!renderAttribute.isDirty());

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(1450);
        updateChange->setPropertyName("divisor");
        renderAttribute.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderAttribute.divisor(), 1450U);
        QVERIFY(renderAttribute.isDirty());
        QVERIFY(renderer.dirtyBits() != 0);

        renderAttribute.unsetDirty();
        renderer.resetDirty();
        QVERIFY(!renderAttribute.isDirty());

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        Qt3DCore::QNodeId bufferId = Qt3DCore::QNodeId::createId();
        updateChange->setValue(QVariant::fromValue(bufferId));
        updateChange->setPropertyName("buffer");
        renderAttribute.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderAttribute.bufferId(), bufferId);
        QVERIFY(renderAttribute.isDirty());
        QVERIFY(renderer.dirtyBits() != 0);

        renderAttribute.unsetDirty();
        renderer.resetDirty();
        QVERIFY(!renderAttribute.isDirty());
    }
};


QTEST_APPLESS_MAIN(tst_Attribute)

#include "tst_attribute.moc"
