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

#include "updatemeshtrianglelistjob_p.h"
#include <Qt3DRender/private/job_common_p.h>
#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/light_p.h>
#include <Qt3DRender/private/sphere_p.h>
#include <Qt3DRender/private/attribute_p.h>
#include <Qt3DRender/private/geometryrenderer_p.h>
#include <Qt3DRender/private/geometry_p.h>
#include <Qt3DRender/private/attribute_p.h>
#include <Qt3DRender/private/buffer_p.h>
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/buffermanager_p.h>
#include <Qt3DRender/private/geometryrenderermanager_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

UpdateMeshTriangleListJob::UpdateMeshTriangleListJob()
    : m_manager(nullptr)
{
    SET_JOB_RUN_STAT_TYPE(this, JobTypes::UpdateMeshTriangleList, 0);
}

UpdateMeshTriangleListJob::~UpdateMeshTriangleListJob()
{
}

void UpdateMeshTriangleListJob::setManagers(NodeManagers *manager)
{
    m_manager = manager;
}

void UpdateMeshTriangleListJob::run()
{
    GeometryRendererManager *geomRenderermanager = m_manager->geometryRendererManager();
    GeometryManager *geomManager = m_manager->geometryManager();
    BufferManager *bufferManager = m_manager->bufferManager();
    AttributeManager *attributeManager = m_manager->attributeManager();

    const QVector<HGeometryRenderer> handles = geomRenderermanager->activeHandles();

    for (const HGeometryRenderer handle : handles) {
        // Look if for the GeometryRender/Geometry the attributes and or buffers are dirty
        // in which case we need to recompute the triangle list
        const GeometryRenderer *geomRenderer = geomRenderermanager->data(handle);
        if (geomRenderer != nullptr) {
            const Geometry *geom = geomManager->lookupResource(geomRenderer->geometryId());
            if (geom != nullptr) {
                const Qt3DCore::QNodeId geomRendererId = geomRenderer->peerId();
                if (!geomRenderermanager->isGeometryRendererScheduledForTriangleDataRefresh(geomRendererId)) {
                    // Check if the attributes or buffers are dirty
                    bool dirty = geomRenderer->isDirty();
                    const auto attrIds = geom->attributes();
                    for (const Qt3DCore::QNodeId attrId : attrIds) {
                        const Attribute *attr = attributeManager->lookupResource(attrId);
                        if (attr != nullptr) {
                            dirty |= attr->isDirty();
                            if (!dirty) {
                                const Buffer *buffer = bufferManager->lookupResource(attr->bufferId());
                                if (buffer != nullptr)
                                    dirty = buffer->isDirty();
                            }
                            if (dirty)
                                break;
                        }
                    }
                    if (dirty)
                        m_manager->geometryRendererManager()->requestTriangleDataRefreshForGeometryRenderer(geomRendererId);
                }
            }
        }
    }
}

NodeManagers *UpdateMeshTriangleListJob::managers() const
{
    return m_manager;
}

} // Render

} // Qt3DRender

QT_END_NAMESPACE
