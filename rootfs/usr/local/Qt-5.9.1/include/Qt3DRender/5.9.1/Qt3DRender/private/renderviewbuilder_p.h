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

#ifndef QT3DRENDER_RENDER_RENDERVIEWBUILDER_H
#define QT3DRENDER_RENDER_RENDERVIEWBUILDER_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <functional>
#include <Qt3DCore/qaspectjob.h>
#include <Qt3DRender/private/filterentitybycomponentjob_p.h>
#include <Qt3DRender/private/filterlayerentityjob_p.h>
#include <Qt3DRender/private/genericlambdajob_p.h>
#include <Qt3DRender/private/materialparametergathererjob_p.h>
#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/renderviewbuilderjob_p.h>
#include <Qt3DRender/private/renderview_p.h>
#include <Qt3DRender/private/frustumcullingjob_p.h>
#include <Qt3DRender/private/lightgatherer_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

class Renderer;

using SynchronizerJobPtr = GenericLambdaJobPtr<std::function<void()>>;
using ComputableEntityFilterPtr = FilterEntityByComponentJobPtr<Render::ComputeCommand, Render::Material>;
using RenderableEntityFilterPtr = FilterEntityByComponentJobPtr<Render::GeometryRenderer, Render::Material>;

class Q_AUTOTEST_EXPORT RenderViewBuilder
{
public:
    explicit RenderViewBuilder(Render::FrameGraphNode *leafNode, int renderViewIndex, Renderer *renderer);

    RenderViewInitializerJobPtr renderViewJob() const;
    FilterLayerEntityJobPtr filterEntityByLayerJob() const;
    LightGathererPtr lightGathererJob() const;
    RenderableEntityFilterPtr renderableEntityFilterJob() const;
    ComputableEntityFilterPtr computableEntityFilterJob() const;
    FrustumCullingJobPtr frustumCullingJob() const;
    QVector<RenderViewBuilderJobPtr> renderViewBuilderJobs() const;
    QVector<MaterialParameterGathererJobPtr> materialGathererJobs() const;
    SynchronizerJobPtr syncRenderViewInitializationJob() const;
    SynchronizerJobPtr syncFrustumCullingJob() const;
    SynchronizerJobPtr syncRenderCommandBuildingJob() const;
    SynchronizerJobPtr syncRenderViewCommandBuildersJob() const;
    SynchronizerJobPtr setClearDrawBufferIndexJob() const;

    QVector<Qt3DCore::QAspectJobPtr> buildJobHierachy() const;

    Renderer *renderer() const;
    int renderViewIndex() const;

    static int optimalJobCount();
    static void removeEntitiesNotInSubset(QVector<Entity *> &entities, QVector<Entity *> subset);

private:
    const int m_renderViewIndex;
    Renderer *m_renderer;

    RenderViewInitializerJobPtr m_renderViewJob;
    FilterLayerEntityJobPtr m_filterEntityByLayerJob;
    LightGathererPtr m_lightGathererJob;
    RenderableEntityFilterPtr m_renderableEntityFilterJob;
    ComputableEntityFilterPtr m_computableEntityFilterJob;
    FrustumCullingJobPtr m_frustumCullingJob;
    QVector<RenderViewBuilderJobPtr> m_renderViewBuilderJobs;
    QVector<MaterialParameterGathererJobPtr> m_materialGathererJobs;

    SynchronizerJobPtr m_syncRenderViewInitializationJob;
    SynchronizerJobPtr m_syncFrustumCullingJob;
    SynchronizerJobPtr m_syncRenderCommandBuildingJob;
    SynchronizerJobPtr m_syncRenderViewCommandBuildersJob;
    SynchronizerJobPtr m_setClearDrawBufferIndexJob;

    static const int m_optimalParallelJobCount;
};

} // Render

} // Qt3DRender

QT_END_NAMESPACE

#endif // QT3DRENDER_RENDER_RENDERVIEWBUILDER_H
