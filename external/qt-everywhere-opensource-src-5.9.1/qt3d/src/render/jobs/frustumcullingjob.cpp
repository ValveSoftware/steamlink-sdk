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

#include "frustumcullingjob_p.h"
#include <Qt3DRender/private/job_common_p.h>
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/entity_p.h>
#include <Qt3DRender/private/renderview_p.h>
#include <Qt3DRender/private/sphere_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

FrustumCullingJob::FrustumCullingJob()
    : Qt3DCore::QAspectJob()
    , m_root(nullptr)
    , m_active(false)
{
    SET_JOB_RUN_STAT_TYPE(this, JobTypes::FrustumCulling, 0);
}

void FrustumCullingJob::run()
{
    // Early return if not activated
    if (!m_active)
        return;

    m_visibleEntities.clear();

    const Plane planes[6] = {
        Plane(m_viewProjection.row(3) + m_viewProjection.row(0)), // Left
        Plane(m_viewProjection.row(3) - m_viewProjection.row(0)), // Right
        Plane(m_viewProjection.row(3) + m_viewProjection.row(1)), // Top
        Plane(m_viewProjection.row(3) - m_viewProjection.row(1)), // Bottom
        Plane(m_viewProjection.row(3) + m_viewProjection.row(2)), // Front
        Plane(m_viewProjection.row(3) - m_viewProjection.row(2)), // Back
    };

    cullScene(m_root, planes);
}

void FrustumCullingJob::cullScene(Entity *e, const Plane *planes)
{
    const Sphere *s = e->worldBoundingVolumeWithChildren();

    // Unrolled loop
    if (QVector3D::dotProduct(s->center(), planes[0].normal) + planes[0].d < -s->radius())
        return;
    if (QVector3D::dotProduct(s->center(), planes[1].normal) + planes[1].d < -s->radius())
        return;
    if (QVector3D::dotProduct(s->center(), planes[2].normal) + planes[2].d < -s->radius())
        return;
    if (QVector3D::dotProduct(s->center(), planes[3].normal) + planes[3].d < -s->radius())
        return;
    if (QVector3D::dotProduct(s->center(), planes[4].normal) + planes[4].d < -s->radius())
        return;
    if (QVector3D::dotProduct(s->center(), planes[5].normal) + planes[5].d < -s->radius())
        return;

    m_visibleEntities.push_back(e);

    const QVector<Entity *> children = e->children();
    for (Entity *c : children)
        cullScene(c, planes);
}

} // Render

} // Qt3DRender

QT_END_NAMESPACE
