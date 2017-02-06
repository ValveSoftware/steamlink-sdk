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

namespace {

Qt3DCore::QEntity *buildTestScene(int layersCount,
                                  int entityCount,
                                  QVector<Qt3DCore::QNodeId> &layerIds,
                                  bool alwaysEnabled = true)
{
    Qt3DCore::QEntity *root = new Qt3DCore::QEntity();

    QVector<Qt3DRender::QLayer *> layers;
    layers.reserve(layersCount);
    layerIds.reserve(layersCount);

    for (int i = 0; i < layersCount; ++i) {
        Qt3DRender::QLayer *layer = new Qt3DRender::QLayer(root);
        layers.push_back(layer);
        layerIds.push_back(layer->id());
    }

    for (int i = 0; i < entityCount; ++i) {
        Qt3DCore::QEntity *entity = new Qt3DCore::QEntity(root);

        if (layersCount > 0)
            entity->addComponent(layers.at(qrand() % layersCount));

        if (!alwaysEnabled && i % 128 == 0)
            entity->setEnabled(false);
    }

    return root;
}

} // anonymous

class tst_BenchLayerFiltering : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    void filterEntities_data()
    {
        QTest::addColumn<Qt3DCore::QEntity *>("entitySubtree");
        QTest::addColumn<Qt3DCore::QNodeIdVector>("layerIds");
        QTest::addColumn<bool>("hasLayerFilter");


        {
            Qt3DCore::QNodeIdVector layerIds;
            Qt3DCore::QEntity *rootEntity = buildTestScene(0, 5000, layerIds);

            QTest::newRow("Filter-NoLayerFilterAllEnabled") << rootEntity
                                                            << layerIds
                                                            << (layerIds.size() != 0);
        }

        {
            Qt3DCore::QNodeIdVector layerIds;
            Qt3DCore::QEntity *rootEntity = buildTestScene(0, 5000, layerIds, false);
            QTest::newRow("Filter-NoLayerFilterSomeDisabled") << rootEntity
                                                              << layerIds
                                                              << (layerIds.size() != 0);
        }

        {
            Qt3DCore::QNodeIdVector layerIds;
            Qt3DCore::QEntity *rootEntity = buildTestScene(10, 5000, layerIds);

            QTest::newRow("FilterLayerFilterAllEnabled") << rootEntity
                                                         << layerIds
                                                         << (layerIds.size() != 0);
        }

        {
            Qt3DCore::QNodeIdVector layerIds;
            Qt3DCore::QEntity *rootEntity = buildTestScene(10, 5000, layerIds, false);

            QTest::newRow("FilterLayerFilterSomeDisabled") << rootEntity
                                                         << layerIds
                                                         << (layerIds.size() != 0);
        }

    }

    void filterEntities()
    {
        QFETCH(Qt3DCore::QEntity *, entitySubtree);
        QFETCH(Qt3DCore::QNodeIdVector, layerIds);
        QFETCH(bool, hasLayerFilter);

        // GIVEN
        QScopedPointer<Qt3DRender::TestAspect> aspect(new Qt3DRender::TestAspect(entitySubtree));

        // WHEN
        Qt3DRender::Render::FilterLayerEntityJob filterJob;
        filterJob.setHasLayerFilter(hasLayerFilter);
        filterJob.setLayers(layerIds);
        filterJob.setManager(aspect->nodeManagers());

        QBENCHMARK {
            filterJob.run();
        }
    }
};

QTEST_MAIN(tst_BenchLayerFiltering)

#include "tst_bench_layerfiltering.moc"
