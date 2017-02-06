/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "renderviewbuilder_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

const int RenderViewBuilder::m_optimalParallelJobCount = std::max(QThread::idealThreadCount(), 2);

namespace {

class SyncRenderViewCommandBuilders
{
public:
    explicit SyncRenderViewCommandBuilders(const RenderViewInitializerJobPtr &renderViewJob,
                                           const QVector<RenderViewBuilderJobPtr> &renderViewBuilderJobs,
                                           Renderer *renderer)
        : m_renderViewJob(renderViewJob)
        , m_renderViewBuilderJobs(renderViewBuilderJobs)
        , m_renderer(renderer)
    {}

    void operator()()
    {
        // Append all the commands and sort them
        RenderView *rv = m_renderViewJob->renderView();

        int totalCommandCount = 0;
        for (const auto renderViewCommandBuilder : qAsConst(m_renderViewBuilderJobs))
            totalCommandCount += renderViewCommandBuilder->commands().size();

        QVector<RenderCommand *> commands;
        commands.reserve(totalCommandCount);

        // Reduction
        for (const auto renderViewCommandBuilder : qAsConst(m_renderViewBuilderJobs))
            commands += std::move(renderViewCommandBuilder->commands());
        rv->setCommands(commands);

        // Sort the commands
        rv->sort();

        // Enqueue our fully populated RenderView with the RenderThread
        m_renderer->enqueueRenderView(rv, m_renderViewJob->submitOrderIndex());
    }

private:
    RenderViewInitializerJobPtr m_renderViewJob;
    QVector<RenderViewBuilderJobPtr> m_renderViewBuilderJobs;
    Renderer *m_renderer;
};

class SyncFrustumCulling
{
public:
    explicit SyncFrustumCulling(const RenderViewInitializerJobPtr &renderViewJob,
                                const FrustumCullingJobPtr &frustumCulling)
        : m_renderViewJob(renderViewJob)
        , m_frustumCullingJob(frustumCulling)
    {}

    void operator()()
    {
        RenderView *rv = m_renderViewJob->renderView();

        // Update matrices now that all transforms have been updated
        rv->updateMatrices();

        // Frustum culling
        m_frustumCullingJob->setViewProjection(rv->viewProjectionMatrix());
    }

private:
    RenderViewInitializerJobPtr m_renderViewJob;
    FrustumCullingJobPtr m_frustumCullingJob;
};

class SyncRenderViewInitialization
{
public:
    explicit SyncRenderViewInitialization(const RenderViewInitializerJobPtr &renderViewJob,
                                          const FrustumCullingJobPtr &frustumCullingJob,
                                          const FilterLayerEntityJobPtr &filterEntityByLayerJob,
                                          const QVector<MaterialParameterGathererJobPtr> &materialGathererJobs,
                                          const QVector<RenderViewBuilderJobPtr> &renderViewBuilderJobs)
        : m_renderViewJob(renderViewJob)
        , m_frustumCullingJob(frustumCullingJob)
        , m_filterEntityByLayerJob(filterEntityByLayerJob)
        , m_materialGathererJobs(materialGathererJobs)
        , m_renderViewBuilderJobs(renderViewBuilderJobs)
    {}

    void operator()()
    {
        RenderView *rv = m_renderViewJob->renderView();

        // Layer filtering
        m_filterEntityByLayerJob->setHasLayerFilter(rv->hasLayerFilter());
        m_filterEntityByLayerJob->setLayers(rv->layerFilter());

        // Material Parameter building
        for (const auto materialGatherer : qAsConst(m_materialGathererJobs)) {
            materialGatherer->setRenderPassFilter(const_cast<RenderPassFilter *>(rv->renderPassFilter()));
            materialGatherer->setTechniqueFilter(const_cast<TechniqueFilter *>(rv->techniqueFilter()));
        }

        // Command builders
        for (const auto renderViewCommandBuilder : qAsConst(m_renderViewBuilderJobs))
            renderViewCommandBuilder->setRenderView(rv);

        // Set whether frustum culling is enabled or not
        m_frustumCullingJob->setActive(rv->frustumCulling());
    }

private:
    RenderViewInitializerJobPtr m_renderViewJob;
    FrustumCullingJobPtr m_frustumCullingJob;
    FilterLayerEntityJobPtr m_filterEntityByLayerJob;
    QVector<MaterialParameterGathererJobPtr> m_materialGathererJobs;
    QVector<RenderViewBuilderJobPtr> m_renderViewBuilderJobs;
};

class SyncRenderCommandBuilding
{
public:
    explicit SyncRenderCommandBuilding(const RenderViewInitializerJobPtr &renderViewJob,
                                       const FrustumCullingJobPtr &frustumCullingJob,
                                       const FilterLayerEntityJobPtr &filterEntityByLayerJob,
                                       const LightGathererPtr &lightGathererJob,
                                       const RenderableEntityFilterPtr &renderableEntityFilterJob,
                                       const ComputableEntityFilterPtr &computableEntityFilterJob,
                                       const QVector<MaterialParameterGathererJobPtr> &materialGathererJobs,
                                       const QVector<RenderViewBuilderJobPtr> &renderViewBuilderJobs)
        : m_renderViewJob(renderViewJob)
        , m_frustumCullingJob(frustumCullingJob)
        , m_filterEntityByLayerJob(filterEntityByLayerJob)
        , m_lightGathererJob(lightGathererJob)
        , m_renderableEntityFilterJob(renderableEntityFilterJob)
        , m_computableEntityFilterJob(computableEntityFilterJob)
        , m_materialGathererJobs(materialGathererJobs)
        , m_renderViewBuilderJobs(renderViewBuilderJobs)
    {}

    void operator()()
    {
        // Set the result of previous job computations
        // for final RenderCommand building
        RenderView *rv = m_renderViewJob->renderView();

        if (!rv->noDraw()) {
            // Set the light sources
            rv->setLightSources(std::move(m_lightGathererJob->lights()));

            // Remove all entities from the compute and renderable vectors that aren't in the filtered layer vector
            QVector<Entity *> filteredEntities = m_filterEntityByLayerJob->filteredEntities();

            // We sort the vector so that the removal can then be performed linearly
            if (!rv->isCompute()) {
                QVector<Entity *> renderableEntities = std::move(m_renderableEntityFilterJob->filteredEntities());
                std::sort(renderableEntities.begin(), renderableEntities.end());
                RenderViewBuilder::removeEntitiesNotInSubset(renderableEntities, filteredEntities);

                if (rv->frustumCulling())
                    RenderViewBuilder::removeEntitiesNotInSubset(renderableEntities, m_frustumCullingJob->visibleEntities());

                // Split among the number of command builders
                const int packetSize = renderableEntities.size() / RenderViewBuilder::optimalJobCount();
                for (auto i = 0, m = RenderViewBuilder::optimalJobCount(); i < m; ++i) {
                    const RenderViewBuilderJobPtr renderViewCommandBuilder = m_renderViewBuilderJobs.at(i);
                    if (i == m - 1)
                        renderViewCommandBuilder->setRenderables(renderableEntities.mid(i * packetSize, packetSize + renderableEntities.size() % m));
                    else
                        renderViewCommandBuilder->setRenderables(renderableEntities.mid(i * packetSize, packetSize));
                }
            } else {
                QVector<Entity *> computableEntities = std::move(m_computableEntityFilterJob->filteredEntities());
                std::sort(computableEntities.begin(), computableEntities.end());
                RenderViewBuilder::removeEntitiesNotInSubset(computableEntities, filteredEntities);

                // Split among the number of command builders
                const int packetSize = computableEntities.size() / RenderViewBuilder::optimalJobCount();
                for (auto i = 0, m = RenderViewBuilder::optimalJobCount(); i < m; ++i) {
                    const RenderViewBuilderJobPtr renderViewCommandBuilder = m_renderViewBuilderJobs.at(i);
                    if (i == m - 1)
                        renderViewCommandBuilder->setRenderables(computableEntities.mid(i * packetSize, packetSize + computableEntities.size() % m));
                    else
                        renderViewCommandBuilder->setRenderables(computableEntities.mid(i * packetSize, packetSize));
                }
            }

            // Reduction
            QHash<Qt3DCore::QNodeId, QVector<RenderPassParameterData>> params;
            for (const auto materialGatherer : qAsConst(m_materialGathererJobs))
                params.unite(materialGatherer->materialToPassAndParameter());
            // Set all required data on the RenderView for final processing
            rv->setMaterialParameterTable(std::move(params));
        }
    }

private:
    RenderViewInitializerJobPtr m_renderViewJob;
    FrustumCullingJobPtr m_frustumCullingJob;
    FilterLayerEntityJobPtr m_filterEntityByLayerJob;
    LightGathererPtr m_lightGathererJob;
    RenderableEntityFilterPtr m_renderableEntityFilterJob;
    ComputableEntityFilterPtr m_computableEntityFilterJob;
    QVector<MaterialParameterGathererJobPtr> m_materialGathererJobs;
    QVector<RenderViewBuilderJobPtr> m_renderViewBuilderJobs;
};

class SetClearDrawBufferIndex
{
public:
    explicit SetClearDrawBufferIndex(const RenderViewInitializerJobPtr &renderViewJob)
        : m_renderViewJob(renderViewJob)
    {}

    void operator()()
    {
        RenderView *rv = m_renderViewJob->renderView();
        QVector<ClearBufferInfo> &clearBuffersInfo = rv->specificClearColorBufferInfo();
        const AttachmentPack &attachmentPack = rv->attachmentPack();
        for (ClearBufferInfo &clearBufferInfo : clearBuffersInfo)
            clearBufferInfo.drawBufferIndex = attachmentPack.getDrawBufferIndex(clearBufferInfo.attchmentPoint);

    }

private:
    RenderViewInitializerJobPtr m_renderViewJob;
};

} // anonymous

RenderViewBuilder::RenderViewBuilder(Render::FrameGraphNode *leafNode, int renderViewIndex, Renderer *renderer)
    : m_renderViewIndex(renderViewIndex)
    , m_renderer(renderer)
    , m_renderViewJob(RenderViewInitializerJobPtr::create())
    , m_filterEntityByLayerJob(Render::FilterLayerEntityJobPtr::create())
    , m_lightGathererJob(Render::LightGathererPtr::create())
    , m_renderableEntityFilterJob(RenderableEntityFilterPtr::create())
    , m_computableEntityFilterJob(ComputableEntityFilterPtr::create())
    , m_frustumCullingJob(Render::FrustumCullingJobPtr::create())
    , m_syncFrustumCullingJob(SynchronizerJobPtr::create(SyncFrustumCulling(m_renderViewJob, m_frustumCullingJob), JobTypes::SyncFrustumCulling))
    , m_setClearDrawBufferIndexJob(SynchronizerJobPtr::create(SetClearDrawBufferIndex(m_renderViewJob), JobTypes::ClearBufferDrawIndex))
{
    // Init what we can here
    EntityManager *entityManager = m_renderer->nodeManagers()->renderNodesManager();
    m_filterEntityByLayerJob->setManager(m_renderer->nodeManagers());
    m_renderableEntityFilterJob->setManager(entityManager);
    m_computableEntityFilterJob->setManager(entityManager);
    m_frustumCullingJob->setRoot(m_renderer->sceneRoot());
    m_lightGathererJob->setManager(entityManager);
    m_renderViewJob->setRenderer(m_renderer);
    m_renderViewJob->setFrameGraphLeafNode(leafNode);
    m_renderViewJob->setSubmitOrderIndex(m_renderViewIndex);

    // RenderCommand building is the most consuming task -> split it
    // Estimate the number of jobs to create based on the number of entities
    m_renderViewBuilderJobs.reserve(RenderViewBuilder::m_optimalParallelJobCount);
    for (auto i = 0; i < RenderViewBuilder::m_optimalParallelJobCount; ++i) {
        auto renderViewCommandBuilder = Render::RenderViewBuilderJobPtr::create();
        renderViewCommandBuilder->setIndex(m_renderViewIndex);
        renderViewCommandBuilder->setRenderer(m_renderer);
        m_renderViewBuilderJobs.push_back(renderViewCommandBuilder);
    }

    // Since Material gathering is an heavy task, we split it
    const QVector<HMaterial> materialHandles = m_renderer->nodeManagers()->materialManager()->activeHandles();
    const int elementsPerJob =  materialHandles.size() / RenderViewBuilder::m_optimalParallelJobCount;
    const int lastRemaingElements = materialHandles.size() % RenderViewBuilder::m_optimalParallelJobCount;
    m_materialGathererJobs.reserve(RenderViewBuilder::m_optimalParallelJobCount);
    for (auto i = 0; i < RenderViewBuilder::m_optimalParallelJobCount; ++i) {
        auto materialGatherer = Render::MaterialParameterGathererJobPtr::create();
        materialGatherer->setNodeManagers(m_renderer->nodeManagers());
        materialGatherer->setRenderer(m_renderer);
        if (i == RenderViewBuilder::m_optimalParallelJobCount - 1)
            materialGatherer->setHandles(materialHandles.mid(i * elementsPerJob, elementsPerJob + lastRemaingElements));
        else
            materialGatherer->setHandles(materialHandles.mid(i * elementsPerJob, elementsPerJob));
        m_materialGathererJobs.push_back(materialGatherer);
    }

    m_syncRenderViewInitializationJob = SynchronizerJobPtr::create(SyncRenderViewInitialization(m_renderViewJob,
                                                                                                m_frustumCullingJob,
                                                                                                m_filterEntityByLayerJob,
                                                                                                m_materialGathererJobs,
                                                                                                m_renderViewBuilderJobs),
                                                                   JobTypes::SyncRenderViewInitialization);

    m_syncRenderCommandBuildingJob = SynchronizerJobPtr::create(SyncRenderCommandBuilding(m_renderViewJob,
                                                                                          m_frustumCullingJob,
                                                                                          m_filterEntityByLayerJob,
                                                                                          m_lightGathererJob,
                                                                                          m_renderableEntityFilterJob,
                                                                                          m_computableEntityFilterJob,
                                                                                          m_materialGathererJobs,
                                                                                          m_renderViewBuilderJobs),
                                                                JobTypes::SyncRenderViewCommandBuilding);

    m_syncRenderViewCommandBuildersJob = SynchronizerJobPtr::create(SyncRenderViewCommandBuilders(m_renderViewJob,
                                                                                                  m_renderViewBuilderJobs,
                                                                                                  m_renderer),
                                                                    JobTypes::SyncRenderViewCommandBuilder);
}

RenderViewInitializerJobPtr RenderViewBuilder::renderViewJob() const
{
    return m_renderViewJob;
}

FilterLayerEntityJobPtr RenderViewBuilder::filterEntityByLayerJob() const
{
    return m_filterEntityByLayerJob;
}

LightGathererPtr RenderViewBuilder::lightGathererJob() const
{
    return m_lightGathererJob;
}

RenderableEntityFilterPtr RenderViewBuilder::renderableEntityFilterJob() const
{
    return m_renderableEntityFilterJob;
}

ComputableEntityFilterPtr RenderViewBuilder::computableEntityFilterJob() const
{
    return m_computableEntityFilterJob;
}

FrustumCullingJobPtr RenderViewBuilder::frustumCullingJob() const
{
    return m_frustumCullingJob;
}

QVector<RenderViewBuilderJobPtr> RenderViewBuilder::renderViewBuilderJobs() const
{
    return m_renderViewBuilderJobs;
}

QVector<MaterialParameterGathererJobPtr> RenderViewBuilder::materialGathererJobs() const
{
    return m_materialGathererJobs;
}

SynchronizerJobPtr RenderViewBuilder::syncRenderViewInitializationJob() const
{
    return m_syncRenderViewInitializationJob;
}

SynchronizerJobPtr RenderViewBuilder::syncFrustumCullingJob() const
{
    return m_syncFrustumCullingJob;
}

SynchronizerJobPtr RenderViewBuilder::syncRenderCommandBuildingJob() const
{
    return m_syncRenderCommandBuildingJob;
}

SynchronizerJobPtr RenderViewBuilder::syncRenderViewCommandBuildersJob() const
{
    return m_syncRenderViewCommandBuildersJob;
}

SynchronizerJobPtr RenderViewBuilder::setClearDrawBufferIndexJob() const
{
    return m_setClearDrawBufferIndexJob;
}

QVector<Qt3DCore::QAspectJobPtr> RenderViewBuilder::buildJobHierachy() const
{
    QVector<Qt3DCore::QAspectJobPtr> jobs;

    jobs.reserve(m_materialGathererJobs.size() + m_renderViewBuilderJobs.size() + 11);

    // Set dependencies
    m_syncFrustumCullingJob->addDependency(m_renderer->updateWorldTransformJob());
    m_syncFrustumCullingJob->addDependency(m_renderer->updateShaderDataTransformJob());
    m_syncFrustumCullingJob->addDependency(m_syncRenderViewInitializationJob);

    m_frustumCullingJob->addDependency(m_renderer->expandBoundingVolumeJob());
    m_frustumCullingJob->addDependency(m_syncFrustumCullingJob);

    m_setClearDrawBufferIndexJob->addDependency(m_syncRenderViewInitializationJob);

    m_syncRenderViewInitializationJob->addDependency(m_renderViewJob);

    m_filterEntityByLayerJob->addDependency(m_syncRenderViewInitializationJob);

    m_syncRenderCommandBuildingJob->addDependency(m_syncRenderViewInitializationJob);
    for (const auto materialGatherer : qAsConst(m_materialGathererJobs)) {
        materialGatherer->addDependency(m_syncRenderViewInitializationJob);
        m_syncRenderCommandBuildingJob->addDependency(materialGatherer);
    }
    m_syncRenderCommandBuildingJob->addDependency(m_renderableEntityFilterJob);
    m_syncRenderCommandBuildingJob->addDependency(m_computableEntityFilterJob);
    m_syncRenderCommandBuildingJob->addDependency(m_filterEntityByLayerJob);
    m_syncRenderCommandBuildingJob->addDependency(m_lightGathererJob);
    m_syncRenderCommandBuildingJob->addDependency(m_frustumCullingJob);

    for (const auto renderViewCommandBuilder : qAsConst(m_renderViewBuilderJobs)) {
        renderViewCommandBuilder->addDependency(m_syncRenderCommandBuildingJob);
        m_syncRenderViewCommandBuildersJob->addDependency(renderViewCommandBuilder);
    }

    m_renderer->frameCleanupJob()->addDependency(m_syncRenderViewCommandBuildersJob);
    m_renderer->frameCleanupJob()->addDependency(m_setClearDrawBufferIndexJob);

    // Add jobs
    jobs.push_back(m_renderViewJob); // Step 1
    jobs.push_back(m_renderableEntityFilterJob); // Step 1
    jobs.push_back(m_lightGathererJob); // Step 1

    // Note: do it only if OpenGL 4.3+ available
    jobs.push_back(m_computableEntityFilterJob); // Step 1

    jobs.push_back(m_syncRenderViewInitializationJob); // Step 2

    jobs.push_back(m_syncFrustumCullingJob); // Step 3
    jobs.push_back(m_filterEntityByLayerJob); // Step 3
    jobs.push_back(m_setClearDrawBufferIndexJob); // Step 3

    for (const auto materialGatherer : qAsConst(m_materialGathererJobs)) // Step3
        jobs.push_back(materialGatherer);

    jobs.push_back(m_frustumCullingJob); // Step 4
    jobs.push_back(m_syncRenderCommandBuildingJob); // Step 4

    for (const auto renderViewCommandBuilder : qAsConst(m_renderViewBuilderJobs)) // Step 5
        jobs.push_back(renderViewCommandBuilder);

    jobs.push_back(m_syncRenderViewCommandBuildersJob); // Step 6

    return jobs;
}

Renderer *RenderViewBuilder::renderer() const
{
    return m_renderer;
}

int RenderViewBuilder::renderViewIndex() const
{
    return m_renderViewIndex;
}

int RenderViewBuilder::optimalJobCount()
{
    return RenderViewBuilder::m_optimalParallelJobCount;
}

void RenderViewBuilder::removeEntitiesNotInSubset(QVector<Entity *> &entities, QVector<Entity *> subset)
{
    // Note: assumes entities was sorted already
    std::sort(subset.begin(), subset.end());

    for (auto i = entities.size() - 1, j = subset.size() - 1; i >= 0; --i) {
        while (j >= 0 && subset.at(j) > entities.at(i))
            --j;
        if (j < 0 || entities.at(i) != subset.at(j))
            entities.removeAt(i);
    }
}

} // Render

} // Qt3DRender

QT_END_NAMESPACE
