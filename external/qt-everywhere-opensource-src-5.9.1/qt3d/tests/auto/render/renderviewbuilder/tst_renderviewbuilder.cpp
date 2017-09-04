/****************************************************************************
**
** Copyright (C) 2016 Paul Lemire <paul.lemire350@gmail.com>
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
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qaspectjobmanager_p.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>

#include <Qt3DRender/private/renderviewbuilder_p.h>
#include <Qt3DRender/private/qrenderaspect_p.h>
#include <Qt3DRender/qviewport.h>
#include <Qt3DRender/qclearbuffers.h>
#include <Qt3DRender/qdispatchcompute.h>
#include <Qt3DRender/qfrustumculling.h>
#include <Qt3DRender/qmaterial.h>
#include <Qt3DRender/qgeometryrenderer.h>
#include <Qt3DRender/qcomputecommand.h>
#include <Qt3DRender/qlayerfilter.h>
#include <Qt3DRender/qrenderpassfilter.h>
#include <Qt3DRender/qtechniquefilter.h>
#include <Qt3DRender/qcameraselector.h>
#include <Qt3DRender/qcamera.h>
#include <Qt3DRender/qlayer.h>
#include <Qt3DRender/private/qrenderaspect_p.h>
#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/managers_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

class TestAspect : public QRenderAspect
{
public:
    TestAspect(Qt3DCore::QNode *root)
        : QRenderAspect(Qt3DRender::QRenderAspect::Synchronous)
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

    Render::Renderer *renderer() const
    {
        return static_cast<Render::Renderer *>(d_func()->m_renderer);
    }

    Render::MaterialParameterGathererJobPtr materialGathererJob() const
    {
        Render::MaterialParameterGathererJobPtr job = Render::MaterialParameterGathererJobPtr::create();
        job->setNodeManagers(nodeManagers());
        job->setRenderer(static_cast<Render::Renderer *>(d_func()->m_renderer));
        return job;
    }

    void onRegistered() { QRenderAspect::onRegistered(); }
    void onUnregistered() { QRenderAspect::onUnregistered(); }

private:
    QScopedPointer<Qt3DCore::QAspectJobManager> m_jobManager;
};

} // namespace Qt3DRender

QT_END_NAMESPACE

namespace {

Qt3DCore::QEntity *buildSimpleScene(Qt3DRender::QFrameGraphNode *fg)
{
    Qt3DCore::QEntity *root = new Qt3DCore::QEntity();

    Qt3DRender::QRenderSettings* renderSettings = new Qt3DRender::QRenderSettings();
    renderSettings->setActiveFrameGraph(fg);
    root->addComponent(renderSettings);

    // Scene
    {
        Qt3DCore::QEntity *e = new Qt3DCore::QEntity();
        Qt3DRender::QMaterial *material = new Qt3DRender::QMaterial();
        Qt3DRender::QGeometryRenderer *geometryRenderer = new Qt3DRender::QGeometryRenderer();
        e->addComponent(material);
        e->addComponent(geometryRenderer);
        e->setParent(root);
    }
    {
        Qt3DCore::QEntity *e = new Qt3DCore::QEntity();
        Qt3DRender::QMaterial *material = new Qt3DRender::QMaterial();
        Qt3DRender::QComputeCommand *computeCommand = new Qt3DRender::QComputeCommand();
        e->addComponent(material);
        e->addComponent(computeCommand);
        e->setParent(root);
    }

    return root;
}

Qt3DCore::QEntity *buildEntityFilterTestScene(Qt3DRender::QFrameGraphNode *fg, Qt3DRender::QLayer *layer)
{
    Qt3DCore::QEntity *root = new Qt3DCore::QEntity();

    Qt3DRender::QRenderSettings* renderSettings = new Qt3DRender::QRenderSettings();
    renderSettings->setActiveFrameGraph(fg);
    root->addComponent(renderSettings);

    // Scene
    for (int i = 0; i < 200; ++i) {
        Qt3DCore::QEntity *e = new Qt3DCore::QEntity();
        Qt3DRender::QMaterial *material = new Qt3DRender::QMaterial();
        Qt3DRender::QGeometryRenderer *geometryRenderer = new Qt3DRender::QGeometryRenderer();
        e->addComponent(material);
        e->addComponent(geometryRenderer);
        if (i % 2 == 0)
            e->addComponent(layer);
        e->setParent(root);
    }

    return root;
}

} // anonymous


class tst_RenderViewBuilder : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkInitialState()
    {
        // GIVEN
        Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport();
        Qt3DRender::QClearBuffers *clearBuffer = new Qt3DRender::QClearBuffers(viewport);
        Qt3DRender::TestAspect testAspect(buildSimpleScene(viewport));

        // THEN
        Qt3DRender::Render::FrameGraphNode *leafNode = testAspect.nodeManagers()->frameGraphManager()->lookupNode(clearBuffer->id());
        QVERIFY(leafNode != nullptr);

        // WHEN
        Qt3DRender::Render::RenderViewBuilder renderViewBuilder(leafNode, 0, testAspect.renderer());

        // THEN
        QCOMPARE(renderViewBuilder.renderViewIndex(), 0);
        QCOMPARE(renderViewBuilder.renderer(), testAspect.renderer());
        QVERIFY(!renderViewBuilder.renderViewJob().isNull());
        QVERIFY(!renderViewBuilder.filterEntityByLayerJob().isNull());
        QVERIFY(!renderViewBuilder.lightGathererJob().isNull());
        QVERIFY(!renderViewBuilder.renderableEntityFilterJob().isNull());
        QVERIFY(!renderViewBuilder.computableEntityFilterJob().isNull());
        QVERIFY(!renderViewBuilder.frustumCullingJob().isNull());
        QVERIFY(!renderViewBuilder.syncRenderViewInitializationJob().isNull());
        QVERIFY(!renderViewBuilder.syncFrustumCullingJob().isNull());
        QVERIFY(!renderViewBuilder.syncRenderCommandBuildingJob().isNull());
        QVERIFY(!renderViewBuilder.syncRenderViewCommandBuildersJob().isNull());
        QVERIFY(!renderViewBuilder.setClearDrawBufferIndexJob().isNull());
        QCOMPARE(renderViewBuilder.renderViewBuilderJobs().size(), Qt3DRender::Render::RenderViewBuilder::optimalJobCount());
        QCOMPARE(renderViewBuilder.materialGathererJobs().size(), Qt3DRender::Render::RenderViewBuilder::optimalJobCount());

        QCOMPARE(renderViewBuilder.buildJobHierachy().size(), 11 + 2 * Qt3DRender::Render::RenderViewBuilder::optimalJobCount());
    }

    void checkCheckJobDependencies()
    {
        // GIVEN
        Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport();
        Qt3DRender::QClearBuffers *clearBuffer = new Qt3DRender::QClearBuffers(viewport);
        Qt3DRender::TestAspect testAspect(buildSimpleScene(viewport));

        // THEN
        Qt3DRender::Render::FrameGraphNode *leafNode = testAspect.nodeManagers()->frameGraphManager()->lookupNode(clearBuffer->id());
        QVERIFY(leafNode != nullptr);

        // WHEN
        Qt3DRender::Render::RenderViewBuilder renderViewBuilder(leafNode, 0, testAspect.renderer());
        renderViewBuilder.buildJobHierachy();

        // THEN
        // Step 1
        QCOMPARE(renderViewBuilder.renderViewJob()->dependencies().size(), 0);
        QCOMPARE(renderViewBuilder.lightGathererJob()->dependencies().size(), 0);
        QCOMPARE(renderViewBuilder.renderableEntityFilterJob()->dependencies().size(),0);
        QCOMPARE(renderViewBuilder.computableEntityFilterJob()->dependencies().size(), 0);

        // Step 2
        QCOMPARE(renderViewBuilder.syncRenderViewInitializationJob()->dependencies().size(), 1);
        QCOMPARE(renderViewBuilder.syncRenderViewInitializationJob()->dependencies().first().data(), renderViewBuilder.renderViewJob().data());

        // Step 3
        QCOMPARE(renderViewBuilder.filterEntityByLayerJob()->dependencies().size(), 2);
        QVERIFY(renderViewBuilder.filterEntityByLayerJob()->dependencies().contains(renderViewBuilder.syncRenderViewInitializationJob()));
        QVERIFY(renderViewBuilder.filterEntityByLayerJob()->dependencies().contains(testAspect.renderer()->updateTreeEnabledJob()));

        QCOMPARE(renderViewBuilder.setClearDrawBufferIndexJob()->dependencies().size(), 1);
        QCOMPARE(renderViewBuilder.setClearDrawBufferIndexJob()->dependencies().first().data(), renderViewBuilder.syncRenderViewInitializationJob().data());

        QCOMPARE(renderViewBuilder.syncFrustumCullingJob()->dependencies().size(), 3);
        QVERIFY(renderViewBuilder.syncFrustumCullingJob()->dependencies().contains(renderViewBuilder.syncRenderViewInitializationJob()));
        QVERIFY(renderViewBuilder.syncFrustumCullingJob()->dependencies().contains(testAspect.renderer()->updateWorldTransformJob()));
        QVERIFY(renderViewBuilder.syncFrustumCullingJob()->dependencies().contains(testAspect.renderer()->updateShaderDataTransformJob()));

        for (const auto materialGatherer : renderViewBuilder.materialGathererJobs()) {
            QCOMPARE(materialGatherer->dependencies().size(), 2);
            QVERIFY(materialGatherer->dependencies().contains(renderViewBuilder.syncRenderViewInitializationJob()));
            QVERIFY(materialGatherer->dependencies().contains(testAspect.renderer()->filterCompatibleTechniqueJob()));
        }

        // Step 4
        QCOMPARE(renderViewBuilder.frustumCullingJob()->dependencies().size(), 2);
        QVERIFY(renderViewBuilder.frustumCullingJob()->dependencies().contains(renderViewBuilder.syncFrustumCullingJob()));
        QVERIFY(renderViewBuilder.frustumCullingJob()->dependencies().contains(testAspect.renderer()->expandBoundingVolumeJob()));

        QCOMPARE(renderViewBuilder.syncRenderCommandBuildingJob()->dependencies().size(), renderViewBuilder.materialGathererJobs().size() + 6);
        QVERIFY(renderViewBuilder.syncRenderCommandBuildingJob()->dependencies().contains(renderViewBuilder.syncRenderViewInitializationJob()));
        QVERIFY(renderViewBuilder.syncRenderCommandBuildingJob()->dependencies().contains(renderViewBuilder.renderableEntityFilterJob()));
        QVERIFY(renderViewBuilder.syncRenderCommandBuildingJob()->dependencies().contains(renderViewBuilder.computableEntityFilterJob()));
        QVERIFY(renderViewBuilder.syncRenderCommandBuildingJob()->dependencies().contains(renderViewBuilder.filterEntityByLayerJob()));
        QVERIFY(renderViewBuilder.syncRenderCommandBuildingJob()->dependencies().contains(renderViewBuilder.lightGathererJob()));
        QVERIFY(renderViewBuilder.syncRenderCommandBuildingJob()->dependencies().contains(renderViewBuilder.frustumCullingJob()));
        for (const auto materialGatherer : renderViewBuilder.materialGathererJobs()) {
            QVERIFY(renderViewBuilder.syncRenderCommandBuildingJob()->dependencies().contains(materialGatherer));
        }

        // Step 5
        for (const auto renderViewBuilderJob : renderViewBuilder.renderViewBuilderJobs()) {
            QCOMPARE(renderViewBuilderJob->dependencies().size(), 1);
            QCOMPARE(renderViewBuilderJob->dependencies().first().data(), renderViewBuilder.syncRenderCommandBuildingJob().data());
        }

        // Step 6
        QCOMPARE(renderViewBuilder.syncRenderViewCommandBuildersJob()->dependencies().size(), renderViewBuilder.renderViewBuilderJobs().size());
        for (const auto renderViewBuilderJob : renderViewBuilder.renderViewBuilderJobs()) {
            QVERIFY(renderViewBuilder.syncRenderViewCommandBuildersJob()->dependencies().contains(renderViewBuilderJob));
        }
    }

    void checkRenderViewJobExecution()
    {
        // GIVEN
        Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport();
        Qt3DRender::QClearBuffers *clearBuffer = new Qt3DRender::QClearBuffers(viewport);
        Qt3DRender::TestAspect testAspect(buildSimpleScene(viewport));

        // THEN
        Qt3DRender::Render::FrameGraphNode *leafNode = testAspect.nodeManagers()->frameGraphManager()->lookupNode(clearBuffer->id());
        QVERIFY(leafNode != nullptr);

        // WHEN
        Qt3DRender::Render::RenderViewBuilder renderViewBuilder(leafNode, 0, testAspect.renderer());
        renderViewBuilder.buildJobHierachy();
        renderViewBuilder.renderViewJob()->run();

        // THEN
        QVERIFY(renderViewBuilder.renderViewJob()->renderView() != nullptr);
    }

    void checkRenderableEntitiesFilteringExecution()
    {
        // GIVEN
        Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport();
        Qt3DRender::QClearBuffers *clearBuffer = new Qt3DRender::QClearBuffers(viewport);
        Qt3DRender::TestAspect testAspect(buildSimpleScene(viewport));

        // THEN
        Qt3DRender::Render::FrameGraphNode *leafNode = testAspect.nodeManagers()->frameGraphManager()->lookupNode(clearBuffer->id());
        QVERIFY(leafNode != nullptr);

        // WHEN
        Qt3DRender::Render::RenderViewBuilder renderViewBuilder(leafNode, 0, testAspect.renderer());
        renderViewBuilder.buildJobHierachy();
        renderViewBuilder.renderableEntityFilterJob()->run();

        // THEN
        QCOMPARE(renderViewBuilder.renderableEntityFilterJob()->filteredEntities().size(), 1);
    }

    void checkComputableEntitiesFilteringExecution()
    {
        // GIVEN
        Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport();
        Qt3DRender::QClearBuffers *clearBuffer = new Qt3DRender::QClearBuffers(viewport);
        Qt3DRender::TestAspect testAspect(buildSimpleScene(viewport));

        // THEN
        Qt3DRender::Render::FrameGraphNode *leafNode = testAspect.nodeManagers()->frameGraphManager()->lookupNode(clearBuffer->id());
        QVERIFY(leafNode != nullptr);

        // WHEN
        Qt3DRender::Render::RenderViewBuilder renderViewBuilder(leafNode, 0, testAspect.renderer());
        renderViewBuilder.buildJobHierachy();
        renderViewBuilder.computableEntityFilterJob()->run();

        // THEN
        QCOMPARE(renderViewBuilder.computableEntityFilterJob()->filteredEntities().size(), 1);
    }

    void checkSyncRenderViewInitializationExecution()
    {
        // GIVEN
        Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport();
        Qt3DRender::QClearBuffers *clearBuffer = new Qt3DRender::QClearBuffers(viewport);
        Qt3DRender::QLayerFilter *layerFilter = new Qt3DRender::QLayerFilter(clearBuffer);
        Qt3DRender::QFrustumCulling *frustumCulling = new Qt3DRender::QFrustumCulling(layerFilter);
        Qt3DRender::QTechniqueFilter *techniqueFilter = new Qt3DRender::QTechniqueFilter(frustumCulling);
        Qt3DRender::QRenderPassFilter *renderPassFilter = new Qt3DRender::QRenderPassFilter(techniqueFilter);
        Qt3DRender::QLayer *layer = new Qt3DRender::QLayer();

        layerFilter->addLayer(layer);
        Qt3DRender::TestAspect testAspect(buildSimpleScene(viewport));

        // THEN
        Qt3DRender::Render::FrameGraphNode *leafNode = testAspect.nodeManagers()->frameGraphManager()->lookupNode(renderPassFilter->id());
        QVERIFY(leafNode != nullptr);

        // WHEN
        Qt3DRender::Render::RenderViewBuilder renderViewBuilder(leafNode, 0, testAspect.renderer());
        renderViewBuilder.buildJobHierachy();

        // THEN
        QCOMPARE(renderViewBuilder.frustumCullingJob()->isActive(), false);
        QCOMPARE(renderViewBuilder.filterEntityByLayerJob()->hasLayerFilter(), false);
        QCOMPARE(renderViewBuilder.filterEntityByLayerJob()->layers().size(), 0);
        for (const auto materialGatherer : renderViewBuilder.materialGathererJobs()) {
            QVERIFY(materialGatherer->techniqueFilter() == nullptr);
            QVERIFY(materialGatherer->renderPassFilter() == nullptr);
        }

        // WHEN
        renderViewBuilder.renderViewJob()->run();
        renderViewBuilder.syncRenderViewInitializationJob()->run();

        // THEN
        QCOMPARE(renderViewBuilder.frustumCullingJob()->isActive(), true);
        QCOMPARE(renderViewBuilder.filterEntityByLayerJob()->hasLayerFilter(), true);
        QCOMPARE(renderViewBuilder.filterEntityByLayerJob()->layers().size(), 1);
        for (const auto materialGatherer : renderViewBuilder.materialGathererJobs()) {
            QVERIFY(materialGatherer->techniqueFilter() != nullptr);
            QVERIFY(materialGatherer->renderPassFilter() != nullptr);
        }
    }

    void checkSyncFrustumCullingExecution()
    {
        // GIVEN
        Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport();
        Qt3DRender::QClearBuffers *clearBuffer = new Qt3DRender::QClearBuffers(viewport);
        Qt3DRender::QFrustumCulling *frustumCulling = new Qt3DRender::QFrustumCulling(clearBuffer);
        Qt3DRender::QCameraSelector *cameraSelector = new Qt3DRender::QCameraSelector(frustumCulling);
        Qt3DRender::QCamera *camera = new Qt3DRender::QCamera();
        cameraSelector->setCamera(camera);

        Qt3DRender::TestAspect testAspect(buildSimpleScene(viewport));

        // THEN
        Qt3DRender::Render::FrameGraphNode *leafNode = testAspect.nodeManagers()->frameGraphManager()->lookupNode(cameraSelector->id());
        QVERIFY(leafNode != nullptr);

        // WHEN
        Qt3DRender::Render::RenderViewBuilder renderViewBuilder(leafNode, 0, testAspect.renderer());
        renderViewBuilder.buildJobHierachy();

        // THEN
        QCOMPARE(renderViewBuilder.frustumCullingJob()->viewProjection(), QMatrix4x4());

        // WHEN
        renderViewBuilder.renderViewJob()->run();
        renderViewBuilder.syncFrustumCullingJob()->run();

        // THEN
        QCOMPARE(renderViewBuilder.frustumCullingJob()->viewProjection(), camera->projectionMatrix() * camera->viewMatrix());
    }

    void checkRemoveEntitiesNotInSubset()
    {
        // GIVEN
        Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport();
        Qt3DRender::QClearBuffers *clearBuffer = new Qt3DRender::QClearBuffers(viewport);
        Qt3DRender::QLayerFilter *layerFilter = new Qt3DRender::QLayerFilter(clearBuffer);
        Qt3DRender::QLayer *layer = new Qt3DRender::QLayer();
        layerFilter->addLayer(layer);
        Qt3DRender::TestAspect testAspect(buildEntityFilterTestScene(viewport, layer));

        // THEN
        Qt3DRender::Render::FrameGraphNode *leafNode = testAspect.nodeManagers()->frameGraphManager()->lookupNode(layerFilter->id());
        QVERIFY(leafNode != nullptr);

        // WHEN
        Qt3DRender::Render::RenderViewBuilder renderViewBuilder(leafNode, 0, testAspect.renderer());
        renderViewBuilder.buildJobHierachy();

        renderViewBuilder.renderViewJob()->run();
        renderViewBuilder.renderableEntityFilterJob()->run();
        renderViewBuilder.syncRenderViewInitializationJob()->run();
        renderViewBuilder.filterEntityByLayerJob()->run();

        QVector<Qt3DRender::Render::Entity *> renderableEntity = renderViewBuilder.renderableEntityFilterJob()->filteredEntities();
        QVector<Qt3DRender::Render::Entity *> filteredEntity = renderViewBuilder.filterEntityByLayerJob()->filteredEntities();

        // THEN
        QCOMPARE(renderableEntity.size(), 200);
        QCOMPARE(filteredEntity.size(), 100);

        std::sort(renderableEntity.begin(), renderableEntity.end());

        // WHEN
        Qt3DRender::Render::RenderViewBuilder::removeEntitiesNotInSubset(renderableEntity, filteredEntity);

        // THEN
        QCOMPARE(renderableEntity.size(), 100);
        for (const auto entity : renderableEntity) {
            QVERIFY(filteredEntity.contains(entity));
        }
    }

};

QTEST_MAIN(tst_RenderViewBuilder)

#include "tst_renderviewbuilder.moc"
