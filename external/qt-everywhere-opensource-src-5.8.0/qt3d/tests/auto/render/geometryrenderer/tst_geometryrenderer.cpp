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
#include <Qt3DRender/private/geometryrenderer_p.h>
#include <Qt3DRender/private/geometryrenderermanager_p.h>
#include <Qt3DRender/qgeometry.h>
#include <Qt3DRender/qgeometryfactory.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include "testrenderer.h"

class TestFactory : public Qt3DRender::QGeometryFactory
{
public:
    explicit TestFactory(int size)
        : m_size(size)
    {}

    Qt3DRender::QGeometry *operator ()() Q_DECL_FINAL
    {
        return nullptr;
    }

    bool operator ==(const Qt3DRender::QGeometryFactory &other) const Q_DECL_FINAL
    {
        const TestFactory *otherFactory = functor_cast<TestFactory>(&other);
        if (otherFactory != nullptr)
            return otherFactory->m_size == m_size;
        return false;
    }

    QT3D_FUNCTOR(TestFactory)

    private:
        int m_size;
};

class DummyGeometry : public Qt3DRender::QGeometry
{
    Q_OBJECT
public:
    DummyGeometry(Qt3DCore::QNode *parent = nullptr)
        : Qt3DRender::QGeometry(parent)
    {}
};

class tst_RenderGeometryRenderer : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT
private Q_SLOTS:

    void checkPeerPropertyMirroring()
    {
        // GIVEN
        Qt3DRender::Render::GeometryRenderer renderGeometryRenderer;
        Qt3DRender::QGeometryRenderer geometryRenderer;
        Qt3DRender::QGeometry geometry;
        Qt3DRender::QGeometryFactoryPtr factory(new TestFactory(1200));
        Qt3DRender::Render::GeometryRendererManager geometryRendererManager;

        geometryRenderer.setInstanceCount(1584);
        geometryRenderer.setVertexCount(1609);
        geometryRenderer.setIndexOffset(750);
        geometryRenderer.setFirstInstance(883);
        geometryRenderer.setRestartIndexValue(65536);
        geometryRenderer.setPrimitiveRestartEnabled(true);
        geometryRenderer.setPrimitiveType(Qt3DRender::QGeometryRenderer::Patches);
        geometryRenderer.setGeometry(&geometry);
        geometryRenderer.setGeometryFactory(factory);
        geometryRenderer.setEnabled(false);

        // WHEN
        renderGeometryRenderer.setManager(&geometryRendererManager);
        simulateInitialization(&geometryRenderer, &renderGeometryRenderer);

        // THEN
        QCOMPARE(renderGeometryRenderer.peerId(), geometryRenderer.id());
        QCOMPARE(renderGeometryRenderer.isDirty(), true);
        QCOMPARE(renderGeometryRenderer.instanceCount(), geometryRenderer.instanceCount());
        QCOMPARE(renderGeometryRenderer.vertexCount(), geometryRenderer.vertexCount());
        QCOMPARE(renderGeometryRenderer.indexOffset(), geometryRenderer.indexOffset());
        QCOMPARE(renderGeometryRenderer.firstInstance(), geometryRenderer.firstInstance());
        QCOMPARE(renderGeometryRenderer.restartIndexValue(), geometryRenderer.restartIndexValue());
        QCOMPARE(renderGeometryRenderer.primitiveRestartEnabled(), geometryRenderer.primitiveRestartEnabled());
        QCOMPARE(renderGeometryRenderer.primitiveType(), geometryRenderer.primitiveType());
        QCOMPARE(renderGeometryRenderer.geometryId(), geometry.id());
        QCOMPARE(renderGeometryRenderer.geometryFactory(), factory);
        QCOMPARE(renderGeometryRenderer.isEnabled(), false);
        QVERIFY(*renderGeometryRenderer.geometryFactory() == *factory);
    }

    void checkInitialAndCleanedUpState()
    {
        // GIVEN
        Qt3DRender::Render::GeometryRenderer renderGeometryRenderer;
        Qt3DRender::Render::GeometryRendererManager geometryRendererManager;

        // THEN
        QVERIFY(renderGeometryRenderer.peerId().isNull());
        QVERIFY(renderGeometryRenderer.geometryId().isNull());
        QCOMPARE(renderGeometryRenderer.isDirty(), false);
        QCOMPARE(renderGeometryRenderer.instanceCount(), 0);
        QCOMPARE(renderGeometryRenderer.vertexCount(), 0);
        QCOMPARE(renderGeometryRenderer.indexOffset(), 0);
        QCOMPARE(renderGeometryRenderer.firstInstance(), 0);
        QCOMPARE(renderGeometryRenderer.restartIndexValue(), -1);
        QCOMPARE(renderGeometryRenderer.primitiveRestartEnabled(), false);
        QCOMPARE(renderGeometryRenderer.primitiveType(), Qt3DRender::QGeometryRenderer::Triangles);
        QVERIFY(renderGeometryRenderer.geometryFactory().isNull());
        QVERIFY(!renderGeometryRenderer.isEnabled());

        // GIVEN
        Qt3DRender::QGeometryRenderer geometryRenderer;
        Qt3DRender::QGeometry geometry;
        Qt3DRender::QGeometryFactoryPtr factory(new TestFactory(1200));


        geometryRenderer.setInstanceCount(454);
        geometryRenderer.setVertexCount(350);
        geometryRenderer.setIndexOffset(427);
        geometryRenderer.setFirstInstance(383);
        geometryRenderer.setRestartIndexValue(555);
        geometryRenderer.setPrimitiveRestartEnabled(true);
        geometryRenderer.setPrimitiveType(Qt3DRender::QGeometryRenderer::Patches);
        geometryRenderer.setGeometry(&geometry);
        geometryRenderer.setGeometryFactory(factory);
        geometryRenderer.setEnabled(true);

        // WHEN
        renderGeometryRenderer.setManager(&geometryRendererManager);
        simulateInitialization(&geometryRenderer, &renderGeometryRenderer);
        renderGeometryRenderer.cleanup();

        // THEN
        QVERIFY(renderGeometryRenderer.geometryId().isNull());
        QCOMPARE(renderGeometryRenderer.isDirty(), false);
        QCOMPARE(renderGeometryRenderer.instanceCount(), 0);
        QCOMPARE(renderGeometryRenderer.vertexCount(), 0);
        QCOMPARE(renderGeometryRenderer.indexOffset(), 0);
        QCOMPARE(renderGeometryRenderer.firstInstance(), 0);
        QCOMPARE(renderGeometryRenderer.restartIndexValue(), -1);
        QCOMPARE(renderGeometryRenderer.primitiveRestartEnabled(), false);
        QCOMPARE(renderGeometryRenderer.primitiveType(), Qt3DRender::QGeometryRenderer::Triangles);
        QVERIFY(renderGeometryRenderer.geometryFactory().isNull());
        QVERIFY(!renderGeometryRenderer.isEnabled());
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DRender::Render::GeometryRenderer renderGeometryRenderer;
        TestRenderer renderer;
        renderGeometryRenderer.setRenderer(&renderer);

        QVERIFY(!renderGeometryRenderer.isDirty());

        // WHEN
        Qt3DCore::QPropertyUpdatedChangePtr updateChange(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("instanceCount");
        updateChange->setValue(2);
        renderGeometryRenderer.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderGeometryRenderer.instanceCount(), 2);
        QVERIFY(renderGeometryRenderer.isDirty());
        QVERIFY(renderer.dirtyBits() != 0);

        renderGeometryRenderer.unsetDirty();
        QVERIFY(!renderGeometryRenderer.isDirty());

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("vertexCount");
        updateChange->setValue(56);
        renderGeometryRenderer.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderGeometryRenderer.vertexCount(), 56);
        QVERIFY(renderGeometryRenderer.isDirty());

        renderGeometryRenderer.unsetDirty();
        QVERIFY(!renderGeometryRenderer.isDirty());

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("indexOffset");
        updateChange->setValue(65);
        renderGeometryRenderer.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderGeometryRenderer.indexOffset(), 65);
        QVERIFY(renderGeometryRenderer.isDirty());

        renderGeometryRenderer.unsetDirty();
        QVERIFY(!renderGeometryRenderer.isDirty());

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("firstInstance");
        updateChange->setValue(82);
        renderGeometryRenderer.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderGeometryRenderer.firstInstance(), 82);
        QVERIFY(renderGeometryRenderer.isDirty());

        renderGeometryRenderer.unsetDirty();
        QVERIFY(!renderGeometryRenderer.isDirty());

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("restartIndexValue");
        updateChange->setValue(46);
        renderGeometryRenderer.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderGeometryRenderer.restartIndexValue(), 46);
        QVERIFY(renderGeometryRenderer.isDirty());

        renderGeometryRenderer.unsetDirty();
        QVERIFY(!renderGeometryRenderer.isDirty());

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("primitiveRestartEnabled");
        updateChange->setValue(true);
        renderGeometryRenderer.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderGeometryRenderer.primitiveRestartEnabled(), true);
        QVERIFY(renderGeometryRenderer.isDirty());

        renderGeometryRenderer.unsetDirty();
        QVERIFY(!renderGeometryRenderer.isDirty());

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("primitiveType");
        updateChange->setValue(static_cast<int>(Qt3DRender::QGeometryRenderer::LineLoop));
        renderGeometryRenderer.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderGeometryRenderer.primitiveType(), Qt3DRender::QGeometryRenderer::LineLoop);
        QVERIFY(renderGeometryRenderer.isDirty());

        renderGeometryRenderer.unsetDirty();
        QVERIFY(!renderGeometryRenderer.isDirty());

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setPropertyName("geometryFactory");
        Qt3DRender::QGeometryFactoryPtr factory(new TestFactory(1450));
        updateChange->setValue(QVariant::fromValue(factory));
        renderGeometryRenderer.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderGeometryRenderer.geometryFactory(), factory);
        QVERIFY(renderGeometryRenderer.isDirty());

        renderGeometryRenderer.unsetDirty();
        QVERIFY(!renderGeometryRenderer.isDirty());

        // WHEN
        DummyGeometry geometry;
        const Qt3DCore::QNodeId geometryId = geometry.id();
        const auto nodeAddedChange = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
        nodeAddedChange->setPropertyName("geometry");
        nodeAddedChange->setValue(QVariant::fromValue(geometryId));
        renderGeometryRenderer.sceneChangeEvent(nodeAddedChange);

        // THEN
        QCOMPARE(renderGeometryRenderer.geometryId(), geometryId);
        QVERIFY(renderGeometryRenderer.isDirty());

        renderGeometryRenderer.unsetDirty();
        QVERIFY(!renderGeometryRenderer.isDirty());

        // WHEN
        const auto nodeRemovedChange = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
        nodeRemovedChange->setPropertyName("geometry");
        nodeRemovedChange->setValue(QVariant::fromValue(Qt3DCore::QNodeId()));
        renderGeometryRenderer.sceneChangeEvent(nodeRemovedChange);

        // THEN
        QCOMPARE(renderGeometryRenderer.geometryId(), Qt3DCore::QNodeId());
        QVERIFY(renderGeometryRenderer.isDirty());

        renderGeometryRenderer.unsetDirty();
        QVERIFY(!renderGeometryRenderer.isDirty());

        // WHEN
        updateChange.reset(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(QVariant::fromValue(true));
        updateChange->setPropertyName("enabled");
        renderGeometryRenderer.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderGeometryRenderer.isEnabled(), true);
        QVERIFY(!renderGeometryRenderer.isDirty());
    }
};

QTEST_APPLESS_MAIN(tst_RenderGeometryRenderer)

#include "tst_geometryrenderer.moc"
