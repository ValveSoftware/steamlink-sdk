/****************************************************************************
**
** Copyright (C) 2016 Paul Lemire
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

#include "materialparametergathererjob_p.h"
#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/renderpassfilternode_p.h>
#include <Qt3DRender/private/techniquefilternode_p.h>
#include <Qt3DRender/private/job_common_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

namespace {

int materialParameterGathererCounter = 0;
const int likelyNumberOfParameters = 24;

} // anonymous

MaterialParameterGathererJob::MaterialParameterGathererJob()
    : Qt3DCore::QAspectJob()
    , m_manager(nullptr)
    , m_techniqueFilter(nullptr)
    , m_renderPassFilter(nullptr)
    , m_renderer(nullptr)
{
    SET_JOB_RUN_STAT_TYPE(this, JobTypes::MaterialParameterGathering, materialParameterGathererCounter++);
}

// TechniqueFilter / RenderPassFilter

// Parameters from Material/Effect/Technique

// Note: we could maybe improve that by having a smart update when we detect
// that a parameter value has changed. That might require way more book keeping
// which might make this solution a bit too complex

// The fact that this can now be performed in parallel should already provide a big
// improvement
void MaterialParameterGathererJob::run()
{
    for (const HMaterial materialHandle : qAsConst(m_handles)) {
        Material *material = m_manager->materialManager()->data(materialHandle);

        if (Q_UNLIKELY(!material->isEnabled()))
            continue;

        Effect *effect = m_manager->effectManager()->lookupResource(material->effect());
        Technique *technique = findTechniqueForEffect(m_renderer, m_techniqueFilter, effect);

        if (Q_LIKELY(technique != nullptr)) {
            RenderPassList passes = findRenderPassesForTechnique(m_manager, m_renderPassFilter, technique);
            if (Q_LIKELY(passes.size() > 0)) {
                // Order set:
                // 1 Pass Filter
                // 2 Technique Filter
                // 3 Material
                // 4 Effect
                // 5 Technique
                // 6 RenderPass

                // Add Parameters define in techniqueFilter and passFilter
                // passFilter have priority over techniqueFilter

                ParameterInfoList parameters;
                // Doing the reserve allows a gain of 0.5ms on some of the demo examples
                parameters.reserve(likelyNumberOfParameters);

                if (m_renderPassFilter)
                    parametersFromParametersProvider(&parameters, m_manager->parameterManager(),
                                                     m_renderPassFilter);
                if (m_techniqueFilter)
                    parametersFromParametersProvider(&parameters, m_manager->parameterManager(),
                                                     m_techniqueFilter);
                // Get the parameters for our selected rendering setup (override what was defined in the technique/pass filter)
                parametersFromMaterialEffectTechnique(&parameters, m_manager->parameterManager(), material, effect, technique);

                for (RenderPass *renderPass : passes) {
                    ParameterInfoList globalParameters = parameters;
                    parametersFromParametersProvider(&globalParameters, m_manager->parameterManager(), renderPass);
                    m_parameters[material->peerId()].push_back({renderPass, globalParameters});
                }
            }
        }
    }
}

} // Render

} // Qt3DRender

QT_END_NAMESPACE
