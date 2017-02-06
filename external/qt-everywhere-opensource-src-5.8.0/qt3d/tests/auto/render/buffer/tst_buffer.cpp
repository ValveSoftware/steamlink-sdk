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
#include <Qt3DRender/private/buffer_p.h>
#include <Qt3DRender/private/qbuffer_p.h>
#include <Qt3DRender/private/buffermanager_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qbackendnode_p.h>
#include "testpostmanarbiter.h"
#include "testrenderer.h"

class TestFunctor : public Qt3DRender::QBufferDataGenerator
{
public:
    explicit TestFunctor(int size)
        : m_size(size)
    {}

    QByteArray operator ()() Q_DECL_FINAL
    {
        return QByteArrayLiteral("454");
    }

    bool operator ==(const Qt3DRender::QBufferDataGenerator &other) const Q_DECL_FINAL
    {
        const TestFunctor *otherFunctor = functor_cast<TestFunctor>(&other);
        if (otherFunctor != nullptr)
            return otherFunctor->m_size == m_size;
        return false;
    }

    QT3D_FUNCTOR(TestFunctor)

private:
    int m_size;
};

class tst_RenderBuffer : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT
private Q_SLOTS:

    void checkPeerPropertyMirroring()
    {
        // GIVEN
        Qt3DRender::Render::Buffer renderBuffer;
        Qt3DRender::QBuffer buffer(Qt3DRender::QBuffer::IndexBuffer);
        Qt3DRender::Render::BufferManager bufferManager;
        buffer.setUsage(Qt3DRender::QBuffer::DynamicCopy);
        buffer.setData(QByteArrayLiteral("Corvette"));
        buffer.setDataGenerator(Qt3DRender::QBufferDataGeneratorPtr(new TestFunctor(883)));

        // WHEN
        renderBuffer.setManager(&bufferManager);
        simulateInitialization(&buffer, &renderBuffer);

        // THEN
        QCOMPARE(renderBuffer.peerId(), buffer.id());
        QCOMPARE(renderBuffer.isDirty(), true);
        QCOMPARE(renderBuffer.type(), buffer.type());
        QCOMPARE(renderBuffer.usage(), buffer.usage());
        QCOMPARE(renderBuffer.data(), buffer.data());
        QCOMPARE(renderBuffer.dataGenerator(), buffer.dataGenerator());
        QVERIFY(*renderBuffer.dataGenerator() == *buffer.dataGenerator());
    }

    void checkInitialAndCleanedUpState()
    {
        // GIVEN
        Qt3DRender::Render::Buffer renderBuffer;
        Qt3DRender::Render::BufferManager bufferManager;
        TestRenderer renderer;

        // THEN
        QCOMPARE(renderBuffer.isDirty(), false);
        QCOMPARE(renderBuffer.type(), Qt3DRender::QBuffer::VertexBuffer);
        QCOMPARE(renderBuffer.usage(), Qt3DRender::QBuffer::StaticDraw);
        QVERIFY(renderBuffer.data().isEmpty());
        QVERIFY(renderBuffer.peerId().isNull());
        QVERIFY(renderBuffer.dataGenerator().isNull());
        QVERIFY(renderBuffer.pendingBufferUpdates().empty());

        // GIVEN
        Qt3DRender::QBuffer buffer(Qt3DRender::QBuffer::IndexBuffer);
        buffer.setUsage(Qt3DRender::QBuffer::DynamicCopy);
        buffer.setData(QByteArrayLiteral("C7"));
        buffer.setDataGenerator(Qt3DRender::QBufferDataGeneratorPtr(new TestFunctor(73)));

        // WHEN
        renderBuffer.setManager(&bufferManager);
        renderBuffer.setRenderer(&renderer);
        simulateInitialization(&buffer, &renderBuffer);

        Qt3DCore::QPropertyUpdatedChangePtr updateChange(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        Qt3DRender::QBufferUpdate updateData;
        updateData.offset = 2;
        updateData.data = QByteArrayLiteral("LS5");
        updateChange->setValue(QVariant::fromValue(updateData));
        updateChange->setPropertyName("updateData");
        renderBuffer.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderBuffer.type(), Qt3DRender::QBuffer::IndexBuffer);
        QCOMPARE(renderBuffer.usage(), Qt3DRender::QBuffer::DynamicCopy);
        QCOMPARE(renderBuffer.isDirty(), true);
        QCOMPARE(renderBuffer.data(), QByteArrayLiteral("C7"));
        QVERIFY(!renderBuffer.dataGenerator().isNull());
        QVERIFY(!renderBuffer.pendingBufferUpdates().empty());

        // WHEN
        renderBuffer.cleanup();

        // THEN
        QCOMPARE(renderBuffer.isDirty(), false);
        QCOMPARE(renderBuffer.type(), Qt3DRender::QBuffer::VertexBuffer);
        QCOMPARE(renderBuffer.usage(), Qt3DRender::QBuffer::StaticDraw);
        QVERIFY(renderBuffer.data().isEmpty());
        QVERIFY(renderBuffer.dataGenerator().isNull());
        QVERIFY(renderBuffer.pendingBufferUpdates().empty());
    }

    void checkPropertyChanges()
    {
        // GIVEN
        TestRenderer renderer;
        Qt3DRender::Render::Buffer renderBuffer;
        renderBuffer.setRenderer(&renderer);

        // THEN
        QVERIFY(renderBuffer.type() != Qt3DRender::QBuffer::IndexBuffer);
        QVERIFY(renderBuffer.data().isEmpty());
        QVERIFY(renderBuffer.usage() != Qt3DRender::QBuffer::DynamicRead);
        QVERIFY(!renderBuffer.isDirty());

        // WHEN
        Qt3DCore::QPropertyUpdatedChangePtr updateChange(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(static_cast<int>(Qt3DRender::QBuffer::IndexBuffer));
        updateChange->setPropertyName("type");
        renderBuffer.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderBuffer.type(), Qt3DRender::QBuffer::IndexBuffer);
        QVERIFY(renderer.dirtyBits() != 0);
        QVERIFY(renderBuffer.isDirty());

        renderBuffer.unsetDirty();
        QVERIFY(!renderBuffer.isDirty());

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(static_cast<int>(Qt3DRender::QBuffer::DynamicRead));
        updateChange->setPropertyName("usage");
        renderBuffer.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderBuffer.usage(), Qt3DRender::QBuffer::DynamicRead);
        QVERIFY(renderBuffer.isDirty());

        renderBuffer.unsetDirty();
        QVERIFY(!renderBuffer.isDirty());

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(QByteArrayLiteral("LS9"));
        updateChange->setPropertyName("data");
        renderBuffer.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderBuffer.data(), QByteArrayLiteral("LS9"));
        QVERIFY(renderBuffer.isDirty());

        renderBuffer.unsetDirty();
        QVERIFY(!renderBuffer.isDirty());

        // WHEN
        Qt3DRender::QBufferDataGeneratorPtr functor(new TestFunctor(355));
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(QVariant::fromValue(functor));
        updateChange->setPropertyName("dataGenerator");
        renderBuffer.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderBuffer.dataGenerator(), functor);
        QVERIFY(renderBuffer.isDirty());

        renderBuffer.unsetDirty();
        QVERIFY(!renderBuffer.isDirty());

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(true);
        updateChange->setPropertyName("syncData");
        renderBuffer.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderBuffer.isSyncData(), true);
        QVERIFY(!renderBuffer.isDirty());

        // WHEN
        TestArbiter arbiter;
        Qt3DCore::QBackendNodePrivate::get(&renderBuffer)->setArbiter(&arbiter);
        renderBuffer.executeFunctor();

        // THEN
        QCOMPARE(arbiter.events.count(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "data");
        QCOMPARE(change->value().toByteArray(), QByteArrayLiteral("454"));

        arbiter.events.clear();

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        Qt3DRender::QBufferUpdate updateData;
        updateData.offset = 2;
        updateData.data = QByteArrayLiteral("LS5");
        updateChange->setValue(QVariant::fromValue(updateData));
        updateChange->setPropertyName("updateData");
        renderBuffer.sceneChangeEvent(updateChange);

        // THEN
        QVERIFY(!renderBuffer.pendingBufferUpdates().empty());
        QVERIFY(renderBuffer.isDirty());

        renderBuffer.unsetDirty();
        QVERIFY(!renderBuffer.isDirty());
    }
};


QTEST_APPLESS_MAIN(tst_RenderBuffer)

#include "tst_buffer.moc"
