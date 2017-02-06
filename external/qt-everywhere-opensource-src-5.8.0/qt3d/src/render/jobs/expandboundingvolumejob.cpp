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

#include "expandboundingvolumejob_p.h"

#include <Qt3DRender/private/renderer_p.h>
#include <Qt3DRender/private/entity_p.h>
#include <Qt3DRender/private/renderlogging_p.h>
#include <Qt3DRender/private/sphere_p.h>
#include <Qt3DRender/private/job_common_p.h>

#include <QThread>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

namespace {

void expandWorldBoundingVolume(Qt3DRender::Render::Entity *node)
{
    // Go to the nodes that have the most depth
    const auto children = node->children();
    for (Entity *c : children)
        expandWorldBoundingVolume(c);

    // Then traverse back from leaf to root
    // Initialize parent bounding volume to be equal to that of the first child
    if (!children.empty()) {
        Qt3DRender::Render::Sphere *parentBoundingVolume = node->worldBoundingVolumeWithChildren();
        for (Entity *c : children)
            parentBoundingVolume->expandToContain(*c->worldBoundingVolumeWithChildren());
    }
}

}

ExpandBoundingVolumeJob::ExpandBoundingVolumeJob()
    : m_node(nullptr)
{
    SET_JOB_RUN_STAT_TYPE(this, JobTypes::ExpandBoundingVolume, 0);
}

void ExpandBoundingVolumeJob::setRoot(Entity *root)
{
    m_node = root;
}

void ExpandBoundingVolumeJob::run()
{
    // Expand worldBoundingVolumeWithChildren of each node that has children by the
    // bounding volumes of the children.

    // TODO: Implement this using a parallel_for
    qCDebug(Jobs) << "Entering" << Q_FUNC_INFO << QThread::currentThread();
    expandWorldBoundingVolume(m_node);
    qCDebug(Jobs) << "Exiting" << Q_FUNC_INFO << QThread::currentThread();
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
