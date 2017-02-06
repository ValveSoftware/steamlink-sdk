/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
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

#include "framecleanupjob_p.h"
#include <private/renderer_p.h>
#include <private/nodemanagers_p.h>
#include <private/entity_p.h>
#include <private/shaderdata_p.h>
#include <private/managers_p.h>
#include <private/texturedatamanager_p.h>
#include <private/sphere_p.h>
#include <Qt3DRender/private/job_common_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

FrameCleanupJob::FrameCleanupJob()
    : m_managers(nullptr)
    , m_root(nullptr)
{
    SET_JOB_RUN_STAT_TYPE(this, JobTypes::FrameCleanup, 0);
}

FrameCleanupJob::~FrameCleanupJob()
{
}

void FrameCleanupJob::setRoot(Entity *root)
{
    m_root = root;
}

void FrameCleanupJob::run()
{
    // mark each ShaderData clean
    ShaderData::cleanup(m_managers);

    // Debug bounding volume debug
    updateBoundingVolumesDebug(m_root);
}

void FrameCleanupJob::updateBoundingVolumesDebug(Entity *node)
{
#if 0
    BoundingVolumeDebug *debugBV = node->renderComponent<BoundingVolumeDebug>();
    if (debugBV) {
        Qt3DRender::Render::Sphere s;
        if (!debugBV->isRecursive()) {
            s = *node->worldBoundingVolume();
        } else {
            s = *node->worldBoundingVolumeWithChildren();
        }
        debugBV->setRadius(s.radius());
        debugBV->setCenter(s.center());
    }
#endif

    const auto children = node->children();
    for (Entity *c : children)
        updateBoundingVolumesDebug(c);
}

void FrameCleanupJob::setManagers(NodeManagers *managers)
{
    m_managers = managers;
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
