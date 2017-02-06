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
#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>

#include <Qt3DRender/qgeometryrenderer.h>
#include <Qt3DRender/qgeometryfactory.h>
#include <Qt3DRender/qgeometry.h>
#include <Qt3DRender/qattribute.h>
#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/private/qgeometryrenderer_p.h>

#include <Qt3DCore/QPropertyUpdatedChange>
#include <Qt3DCore/QPropertyNodeAddedChange>
#include <Qt3DCore/QPropertyNodeRemovedChange>

#include "testpostmanarbiter.h"

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

class tst_QGeometryRenderer: public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkCloning_data()
    {
        QTest::addColumn<Qt3DRender::QGeometryRenderer *>("geometryRenderer");

        Qt3DRender::QGeometryRenderer *defaultConstructed = new Qt3DRender::QGeometryRenderer();
        QTest::newRow("defaultConstructed") << defaultConstructed ;

        Qt3DRender::QGeometryRenderer *geometry1 = new Qt3DRender::QGeometryRenderer();
        geometry1->setGeometry(new Qt3DRender::QGeometry());
        geometry1->setInstanceCount(1);
        geometry1->setIndexOffset(0);
        geometry1->setFirstInstance(55);
        geometry1->setRestartIndexValue(-1);
        geometry1->setPrimitiveRestartEnabled(false);
        geometry1->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);
        geometry1->setVertexCount(15);
        geometry1->setVerticesPerPatch(2);
        geometry1->setGeometryFactory(Qt3DRender::QGeometryFactoryPtr(new TestFactory(383)));
        QTest::newRow("triangle") << geometry1;

        Qt3DRender::QGeometryRenderer *geometry2 = new Qt3DRender::QGeometryRenderer();
        geometry2->setGeometry(new Qt3DRender::QGeometry());
        geometry2->setInstanceCount(200);
        geometry2->setIndexOffset(58);
        geometry2->setFirstInstance(10);
        geometry2->setRestartIndexValue(65535);
        geometry2->setVertexCount(2056);
        geometry2->setPrimitiveRestartEnabled(true);
        geometry2->setVerticesPerPatch(3);
        geometry2->setPrimitiveType(Qt3DRender::QGeometryRenderer::Lines);
        geometry2->setGeometryFactory(Qt3DRender::QGeometryFactoryPtr(new TestFactory(305)));
        QTest::newRow("lines with restart") << geometry2;
    }

    void checkCloning()
    {
        // GIVEN
        QFETCH(Qt3DRender::QGeometryRenderer *, geometryRenderer);

        // WHEN
        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(geometryRenderer);
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

        // THEN
        QCOMPARE(creationChanges.size(), 1 + (geometryRenderer->geometry() ? 1 : 0));

        const Qt3DCore::QNodeCreatedChangePtr<Qt3DRender::QGeometryRendererData> creationChangeData =
                qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QGeometryRendererData>>(creationChanges.first());
        const Qt3DRender::QGeometryRendererData &cloneData = creationChangeData->data;

        QCOMPARE(geometryRenderer->id(), creationChangeData->subjectId());
        QCOMPARE(geometryRenderer->isEnabled(), creationChangeData->isNodeEnabled());
        QCOMPARE(geometryRenderer->metaObject(), creationChangeData->metaObject());

        QCOMPARE(cloneData.instanceCount, geometryRenderer->instanceCount());
        QCOMPARE(cloneData.vertexCount, geometryRenderer->vertexCount());
        QCOMPARE(cloneData.indexOffset, geometryRenderer->indexOffset());
        QCOMPARE(cloneData.firstInstance, geometryRenderer->firstInstance());
        QCOMPARE(cloneData.restartIndexValue, geometryRenderer->restartIndexValue());
        QCOMPARE(cloneData.primitiveRestart, geometryRenderer->primitiveRestartEnabled());
        QCOMPARE(cloneData.primitiveType, geometryRenderer->primitiveType());
        QCOMPARE(cloneData.verticesPerPatch, geometryRenderer->verticesPerPatch());

        if (geometryRenderer->geometry() != nullptr)
            QCOMPARE(cloneData.geometryId, geometryRenderer->geometry()->id());

        QCOMPARE(cloneData.geometryFactory, geometryRenderer->geometryFactory());
        if (geometryRenderer->geometryFactory())
            QVERIFY(*cloneData.geometryFactory == *geometryRenderer->geometryFactory());
    }

    void checkPropertyUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        QScopedPointer<Qt3DRender::QGeometryRenderer> geometryRenderer(new Qt3DRender::QGeometryRenderer());
        arbiter.setArbiterOnNode(geometryRenderer.data());

        // WHEN
        geometryRenderer->setInstanceCount(256);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "instanceCount");
        QCOMPARE(change->value().value<int>(), 256);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        geometryRenderer->setVertexCount(1340);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "vertexCount");
        QCOMPARE(change->value().value<int>(), 1340);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        geometryRenderer->setIndexOffset(883);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "indexOffset");
        QCOMPARE(change->value().value<int>(), 883);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        geometryRenderer->setFirstInstance(1200);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "firstInstance");
        QCOMPARE(change->value().value<int>(), 1200);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        geometryRenderer->setRestartIndexValue(65535);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "restartIndexValue");
        QCOMPARE(change->value().value<int>(), 65535);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        geometryRenderer->setVerticesPerPatch(2);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "verticesPerPatch");
        QCOMPARE(change->value().toInt(), 2);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        geometryRenderer->setPrimitiveRestartEnabled(true);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "primitiveRestartEnabled");
        QCOMPARE(change->value().value<bool>(), true);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        geometryRenderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Patches);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "primitiveType");
        QCOMPARE(change->value().value<int>(), static_cast<int>(Qt3DRender::QGeometryRenderer::Patches));
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        Qt3DRender::QGeometryFactoryPtr factory(new TestFactory(555));
        geometryRenderer->setGeometryFactory(factory);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "geometryFactory");
        QCOMPARE(change->value().value<Qt3DRender::QGeometryFactoryPtr>(), factory);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        Qt3DRender::QGeometry geom;
        geometryRenderer->setGeometry(&geom);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr nodeAddedChange = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(nodeAddedChange->propertyName(), "geometry");
        QCOMPARE(nodeAddedChange->value().value<Qt3DCore::QNodeId>(), geom.id());
        QCOMPARE(nodeAddedChange->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        Qt3DRender::QGeometry geom2;
        geometryRenderer->setGeometry(&geom2);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr nodeRemovedChange = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(nodeRemovedChange->propertyName(), "geometry");
        QCOMPARE(nodeRemovedChange->value().value<Qt3DCore::QNodeId>(), geom2.id());
        QCOMPARE(nodeRemovedChange->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();
    }

    void checkGeometryBookkeeping()
    {
        // GIVEN
        QScopedPointer<Qt3DRender::QGeometryRenderer> geometryRenderer(new Qt3DRender::QGeometryRenderer);
        {
            // WHEN
            Qt3DRender::QGeometry geometry;
            geometryRenderer->setGeometry(&geometry);

            // THEN
            QCOMPARE(geometry.parent(), geometryRenderer.data());
            QCOMPARE(geometryRenderer->geometry(), &geometry);
        }
        // THEN (Should not crash and parameter be unset)
        QVERIFY(geometryRenderer->geometry() == nullptr);

        {
            // WHEN
            Qt3DRender::QGeometryRenderer someOtherGeometryRenderer;
            QScopedPointer<Qt3DRender::QGeometry> geometry(new Qt3DRender::QGeometry(&someOtherGeometryRenderer));
            geometryRenderer->setGeometry(geometry.data());

            // THEN
            QCOMPARE(geometry->parent(), &someOtherGeometryRenderer);
            QCOMPARE(geometryRenderer->geometry(), geometry.data());

            // WHEN
            geometryRenderer.reset();
            geometry.reset();

            // THEN Should not crash when the geometry is destroyed (tests for failed removal of destruction helper)
        }
    }
};

QTEST_MAIN(tst_QGeometryRenderer)

#include "tst_qgeometryrenderer.moc"
