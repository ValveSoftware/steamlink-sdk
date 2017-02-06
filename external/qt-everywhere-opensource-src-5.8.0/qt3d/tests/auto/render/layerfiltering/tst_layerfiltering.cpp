/****************************************************************************
**
** Copyright (C) 2016 Paul Lemire
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
#include <Qt3DCore/qentity.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/private/qaspectjobmanager_p.h>

#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/entity_p.h>
#include <Qt3DRender/qrenderaspect.h>
#include <Qt3DRender/private/qrenderaspect_p.h>
#include <Qt3DRender/private/filterlayerentityjob_p.h>
#include <Qt3DRender/qlayer.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

class TestAspect : public Qt3DRender::QRenderAspect
{
public:
    TestAspect(Qt3DCore::QNode *root)
        : Qt3DRender::QRenderAspect(Qt3DRender::QRenderAspect::Synchronous)
        , m_jobManager(new Qt3DCore::QAspectJobManager())
    {
        Qt3DCore::QAbstractAspectPrivate::get(this)->m_jobManager = m_jobManager.data();
        QRenderAspect::onRegistered();

        const Qt3DCore::QNodeCreatedChangeGenerator generator(root);
        const QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = generator.creationChanges();

        for (const Qt3DCore::QNodeCreatedChangeBasePtr change : creationChanges)
            d_func()->createBackendNode(change);
    }

    ~TestAspect()
    {
        QRenderAspect::onUnregistered();
    }

    Qt3DRender::Render::NodeManagers *nodeManagers() const
    {
        return d_func()->m_renderer->nodeManagers();
    }

    void onRegistered() { QRenderAspect::onRegistered(); }
    void onUnregistered() { QRenderAspect::onUnregistered(); }

private:
    QScopedPointer<Qt3DCore::QAspectJobManager> m_jobManager;
};

} // namespace Qt3DRender

QT_END_NAMESPACE

class tst_LayerFiltering : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    void filterEntities_data()
    {
        QTest::addColumn<Qt3DCore::QEntity *>("entitySubtree");
        QTest::addColumn<Qt3DCore::QNodeIdVector>("layerIds");
        QTest::addColumn<bool>("hasLayerFilter");
        QTest::addColumn<Qt3DCore::QNodeIdVector>("expectedSelectedEntities");


        {
            Qt3DCore::QEntity *rootEntity = new Qt3DCore::QEntity();
            Qt3DCore::QEntity *childEntity1 = new Qt3DCore::QEntity(rootEntity);
            Qt3DCore::QEntity *childEntity2 = new Qt3DCore::QEntity(rootEntity);
            Qt3DCore::QEntity *childEntity3 = new Qt3DCore::QEntity(rootEntity);

            Q_UNUSED(childEntity1);
            Q_UNUSED(childEntity2);
            Q_UNUSED(childEntity3);

            QTest::newRow("EntitiesNoLayerNoLayerFilter-ShouldSelectAll") << rootEntity
                                                                          << Qt3DCore::QNodeIdVector()
                                                                          << false
                                                                          << (Qt3DCore::QNodeIdVector()
                                                                              << rootEntity->id()
                                                                              << childEntity1->id()
                                                                              << childEntity2->id()
                                                                              << childEntity3->id()
                                                                              );
        }

        {
            Qt3DCore::QEntity *rootEntity = new Qt3DCore::QEntity();
            Qt3DCore::QEntity *childEntity1 = new Qt3DCore::QEntity(rootEntity);
            Qt3DCore::QEntity *childEntity2 = new Qt3DCore::QEntity(rootEntity);
            Qt3DCore::QEntity *childEntity3 = new Qt3DCore::QEntity(rootEntity);


            Q_UNUSED(childEntity1);
            Q_UNUSED(childEntity2);
            Q_UNUSED(childEntity3);

            QTest::newRow("EntityNoLayerWithLayerFilterWithNoFilter-ShouldSelectNone") << rootEntity
                                                                                       << Qt3DCore::QNodeIdVector()
                                                                                       << true
                                                                                       << Qt3DCore::QNodeIdVector();
        }

        {
            Qt3DCore::QEntity *rootEntity = new Qt3DCore::QEntity();
            Qt3DCore::QEntity *childEntity1 = new Qt3DCore::QEntity(rootEntity);
            Qt3DCore::QEntity *childEntity2 = new Qt3DCore::QEntity(rootEntity);
            Qt3DCore::QEntity *childEntity3 = new Qt3DCore::QEntity(rootEntity);

            Q_UNUSED(childEntity1);
            Q_UNUSED(childEntity2);
            Q_UNUSED(childEntity3);

            Qt3DRender::QLayer *layer = new Qt3DRender::QLayer(rootEntity);


            QTest::newRow("NoLayerWithLayerFilterWithFilter-ShouldSelectNone") << rootEntity
                                                                               << (Qt3DCore::QNodeIdVector() << layer->id())
                                                                               << true
                                                                               << Qt3DCore::QNodeIdVector();
        }

        {
            Qt3DCore::QEntity *rootEntity = new Qt3DCore::QEntity();
            Qt3DCore::QEntity *childEntity1 = new Qt3DCore::QEntity(rootEntity);
            Qt3DCore::QEntity *childEntity2 = new Qt3DCore::QEntity(rootEntity);
            Qt3DCore::QEntity *childEntity3 = new Qt3DCore::QEntity(rootEntity);


            Qt3DRender::QLayer *layer = new Qt3DRender::QLayer(rootEntity);
            childEntity1->addComponent(layer);
            childEntity2->addComponent(layer);
            childEntity3->addComponent(layer);

            QTest::newRow("LayerWithLayerFilterWithFilter-ShouldSelectAllButRoot") << rootEntity
                                                                                   << (Qt3DCore::QNodeIdVector() << layer->id())
                                                                                   << true
                                                                                   << (Qt3DCore::QNodeIdVector() << childEntity1->id() << childEntity2->id() << childEntity3->id());
        }

        {
            Qt3DCore::QEntity *rootEntity = new Qt3DCore::QEntity();
            Qt3DCore::QEntity *childEntity1 = new Qt3DCore::QEntity(rootEntity);
            Qt3DCore::QEntity *childEntity2 = new Qt3DCore::QEntity(rootEntity);
            Qt3DCore::QEntity *childEntity3 = new Qt3DCore::QEntity(rootEntity);

            Q_UNUSED(childEntity1)

            Qt3DRender::QLayer *layer = new Qt3DRender::QLayer(rootEntity);
            Qt3DRender::QLayer *layer2 = new Qt3DRender::QLayer(rootEntity);
            childEntity2->addComponent(layer2);
            childEntity3->addComponent(layer);

            QTest::newRow("LayerWithLayerFilterWithFilter-ShouldSelectChild2And3") << rootEntity
                                                                                   << (Qt3DCore::QNodeIdVector() << layer->id() << layer2->id())
                                                                                   << true
                                                                                   << (Qt3DCore::QNodeIdVector() << childEntity2->id() << childEntity3->id());
        }

        {
            Qt3DCore::QEntity *rootEntity = new Qt3DCore::QEntity();
            Qt3DCore::QEntity *childEntity1 = new Qt3DCore::QEntity(rootEntity);
            Qt3DCore::QEntity *childEntity2 = new Qt3DCore::QEntity(rootEntity);
            Qt3DCore::QEntity *childEntity3 = new Qt3DCore::QEntity(rootEntity);


            Qt3DRender::QLayer *layer = new Qt3DRender::QLayer(rootEntity);
            Qt3DRender::QLayer *layer2 = new Qt3DRender::QLayer(rootEntity);
            childEntity1->addComponent(layer);
            childEntity2->addComponent(layer);
            childEntity3->addComponent(layer);

            QTest::newRow("LayerWithLayerFilterWithFilter-ShouldSelectNone") << rootEntity
                                                                             << (Qt3DCore::QNodeIdVector() << layer2->id())
                                                                             << true
                                                                             << Qt3DCore::QNodeIdVector();
        }

        {
            Qt3DCore::QEntity *rootEntity = new Qt3DCore::QEntity();
            Qt3DCore::QEntity *childEntity1 = new Qt3DCore::QEntity(rootEntity);
            Qt3DCore::QEntity *childEntity2 = new Qt3DCore::QEntity(rootEntity);
            Qt3DCore::QEntity *childEntity3 = new Qt3DCore::QEntity(rootEntity);

            childEntity1->setEnabled(false);

            Qt3DRender::QLayer *layer = new Qt3DRender::QLayer(rootEntity);
            childEntity1->addComponent(layer);
            childEntity2->addComponent(layer);
            childEntity3->addComponent(layer);

            QTest::newRow("LayerWithEntityDisabled-ShouldSelectOnlyEntityEnabled") << rootEntity
                                                                                   << (Qt3DCore::QNodeIdVector() << layer->id())
                                                                                   << true
                                                                                   << (Qt3DCore::QNodeIdVector() << childEntity2->id() << childEntity3->id());
        }

    }

    void filterEntities()
    {
        //QSKIP("Skipping until TestAspect can be registered");
        QFETCH(Qt3DCore::QEntity *, entitySubtree);
        QFETCH(Qt3DCore::QNodeIdVector, layerIds);
        QFETCH(bool, hasLayerFilter);
        QFETCH(Qt3DCore::QNodeIdVector, expectedSelectedEntities);

        // GIVEN
        QScopedPointer<Qt3DRender::TestAspect> aspect(new Qt3DRender::TestAspect(entitySubtree));

        // WHEN
        Qt3DRender::Render::FilterLayerEntityJob filterJob;
        filterJob.setHasLayerFilter(hasLayerFilter);
        filterJob.setLayers(layerIds);
        filterJob.setManager(aspect->nodeManagers());
        filterJob.run();

        // THEN
        const QVector<Qt3DRender::Render::Entity *> filterEntities = filterJob.filteredEntities();
        QCOMPARE(expectedSelectedEntities.size(), filterEntities.size());
        for (auto i = 0, m = expectedSelectedEntities.size(); i < m; ++i)
            QCOMPARE(expectedSelectedEntities.at(i), filterEntities.at(i)->peerId());
    }
};

QTEST_MAIN(tst_LayerFiltering)

#include "tst_layerfiltering.moc"
