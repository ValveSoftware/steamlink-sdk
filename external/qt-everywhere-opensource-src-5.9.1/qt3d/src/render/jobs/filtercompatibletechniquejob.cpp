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

#include "filtercompatibletechniquejob_p.h"
#include <Qt3DRender/private/techniquemanager_p.h>
#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/renderer_p.h>
#include <Qt3DRender/private/job_common_p.h>
#include <Qt3DRender/private/graphicscontext_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

FilterCompatibleTechniqueJob::FilterCompatibleTechniqueJob()
    : m_manager(nullptr)
    , m_renderer(nullptr)
{
    SET_JOB_RUN_STAT_TYPE(this, JobTypes::FilterCompatibleTechniques, 0);
}

void FilterCompatibleTechniqueJob::setManager(TechniqueManager *manager)
{
    m_manager = manager;
}

TechniqueManager *FilterCompatibleTechniqueJob::manager() const
{
    return m_manager;
}

void FilterCompatibleTechniqueJob::setRenderer(Renderer *renderer)
{
    m_renderer = renderer;
}

Renderer *FilterCompatibleTechniqueJob::renderer() const
{
    return m_renderer;
}

void FilterCompatibleTechniqueJob::run()
{
    Q_ASSERT(m_manager != nullptr && m_renderer != nullptr);

    if (m_renderer->isRunning() && m_renderer->graphicsContext()->isInitialized()) {
        const QVector<Qt3DCore::QNodeId> dirtyTechniqueIds = m_manager->takeDirtyTechniques();
        for (const Qt3DCore::QNodeId techniqueId : dirtyTechniqueIds) {
            Technique *technique = m_manager->lookupResource(techniqueId);
            if (Q_LIKELY(technique != nullptr))
                technique->setCompatibleWithRenderer((*m_renderer->contextInfo() == *technique->graphicsApiFilter()));
        }
    }
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
