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

#include "filterlayerentityjob_p.h"
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/entity_p.h>
#include <Qt3DRender/private/job_common_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

namespace {
int layerFilterJobCounter = 0;
} // anonymous

FilterLayerEntityJob::FilterLayerEntityJob()
    : Qt3DCore::QAspectJob()
    , m_manager(nullptr)
{
    SET_JOB_RUN_STAT_TYPE(this, JobTypes::LayerFiltering, layerFilterJobCounter++);
}

void FilterLayerEntityJob::run()
{

    m_filteredEntities.clear();
    if (m_hasLayerFilter) { // LayerFilter set -> filter
        LayerManager *layerManager = m_manager->layerManager();

        // Remove layerIds which are not active/enabled
        for (auto i = m_layerIds.size() - 1; i >= 0; --i) {
            Layer *backendLayer = layerManager->lookupResource(m_layerIds.at(i));
            if (backendLayer == nullptr || !backendLayer->isEnabled())
                m_layerIds.removeAt(i);
        }

        filterLayerAndEntity();
    } else { // No LayerFilter set -> retrieve all
        selectAllEntities();
    }
}

// Note: we assume that m_layerIds contains only enabled layers
// -> meaning that if an Entity references such a layer, it's enabled
void FilterLayerEntityJob::filterLayerAndEntity()
{
    EntityManager *entityManager = m_manager->renderNodesManager();
    const QVector<HEntity> handles = entityManager->activeHandles();

    for (const HEntity handle : handles) {
        Entity *entity = entityManager->data(handle);

        if (!entity->isEnabled())
            continue;

        const Qt3DCore::QNodeIdVector entityLayers = entity->componentsUuid<Layer>();

        // An Entity is positively filtered if it contains at least one Layer component with the same id as the
        // layers selected by the LayerFilter

        for (const Qt3DCore::QNodeId id : entityLayers) {
            if (m_layerIds.contains(id)) {
                m_filteredEntities.push_back(entity);
                break;
            }
        }
    }
}

// No layer filter -> retrieve all entities
void FilterLayerEntityJob::selectAllEntities()
{
    EntityManager *entityManager = m_manager->renderNodesManager();
    const QVector<HEntity> handles = entityManager->activeHandles();

    m_filteredEntities.reserve(handles.size());
    for (const HEntity handle : handles) {
        Entity *e = entityManager->data(handle);
        if (e->isEnabled())
            m_filteredEntities.push_back(e);
    }
}

} // Render

} // Qt3DRender

QT_END_NAMESPACE
