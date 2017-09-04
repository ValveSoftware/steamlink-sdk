/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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

#include "updatelevelofdetailjob_p.h"
#include <Qt3DRender/QLevelOfDetail>
#include <Qt3DRender/private/job_common_p.h>
#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/sphere_p.h>

QT_BEGIN_NAMESPACE

namespace
{

template <unsigned N>
double approxRollingAverage(double avg, double input) {
    avg -= avg / N;
    avg += input / N;
    return avg;
}

}
namespace Qt3DRender {

namespace Render {

UpdateLevelOfDetailJob::UpdateLevelOfDetailJob()
    : m_manager(nullptr)
    , m_frameGraphRoot(nullptr)
    , m_root(nullptr)
    , m_filterValue(0.)
{
    SET_JOB_RUN_STAT_TYPE(this, JobTypes::UpdateLevelOfDetail, 0);
}

UpdateLevelOfDetailJob::~UpdateLevelOfDetailJob()
{
}

void UpdateLevelOfDetailJob::setRoot(Entity *root)
{
    m_root = root;
}

void UpdateLevelOfDetailJob::setManagers(NodeManagers *manager)
{
    m_manager = manager;
}

void UpdateLevelOfDetailJob::setFrameGraphRoot(FrameGraphNode *frameGraphRoot)
{
    m_frameGraphRoot = frameGraphRoot;
}

void UpdateLevelOfDetailJob::run()
{
    Q_ASSERT(m_frameGraphRoot && m_root && m_manager);
    updateEntityLod(m_root);
}

bool UpdateLevelOfDetailJob::viewMatrixForCamera(const Qt3DCore::QNodeId &cameraId,
                                                 QMatrix4x4 &viewMatrix,
                                                 QMatrix4x4 &projectionMatrix) const
{
    Render::CameraLens *lens = nullptr;
    Entity *camNode = m_manager->renderNodesManager()->lookupResource(cameraId);
    if (camNode != nullptr &&
            (lens = camNode->renderComponent<CameraLens>()) != nullptr &&
            lens->isEnabled()) {
        viewMatrix = *camNode->worldTransform();
        projectionMatrix = lens->projection();
        return true;
    }
    return false;
}

QRect UpdateLevelOfDetailJob::windowViewport(const QSize &area, const QRectF &relativeViewport) const
{
    if (area.isValid()) {
        const int areaWidth = area.width();
        const int areaHeight = area.height();
        return QRect(relativeViewport.x() * areaWidth,
                     (1.0 - relativeViewport.y() - relativeViewport.height()) * areaHeight,
                     relativeViewport.width() * areaWidth,
                     relativeViewport.height() * areaHeight);
    }
    return relativeViewport.toRect();
}

void UpdateLevelOfDetailJob::updateEntityLod(Entity *entity)
{
    if (!entity->isEnabled())
        return; // skip disabled sub-trees, since their bounding box is probably not valid anyway

    QVector<LevelOfDetail *> lods = entity->renderComponents<LevelOfDetail>();
    if (!lods.empty()) {
        LevelOfDetail* lod = lods.front();  // other lods are ignored

        if (lod->isEnabled() && !lod->thresholds().isEmpty()) {
            switch (lod->thresholdType()) {
            case QLevelOfDetail::DistanceToCameraThreshold:
                updateEntityLodByDistance(entity, lod);
                break;
            case QLevelOfDetail::ProjectedScreenPixelSizeThreshold:
                updateEntityLodByScreenArea(entity, lod);
                break;
            default:
                Q_ASSERT(false);
                break;
            }
        }
    }

    const auto children = entity->children();
    for (Qt3DRender::Render::Entity *child : children)
        updateEntityLod(child);
}

void UpdateLevelOfDetailJob::updateEntityLodByDistance(Entity *entity, LevelOfDetail *lod)
{
    QMatrix4x4 viewMatrix;
    QMatrix4x4 projectionMatrix;
    if (!viewMatrixForCamera(lod->camera(), viewMatrix, projectionMatrix))
        return;

    const QVector<qreal> thresholds = lod->thresholds();
    QVector3D center = lod->center();
    if (lod->hasBoundingVolumeOverride() || entity->worldBoundingVolume() == nullptr) {
        center = *entity->worldTransform() * center;
    } else {
        center = entity->worldBoundingVolume()->center();
    }

    const QVector3D tcenter = viewMatrix * center;
    const float dist = tcenter.length();
    const int n = thresholds.size();
    for (int i=0; i<n; ++i) {
        if (dist <= thresholds[i] || i == n -1) {
            m_filterValue = approxRollingAverage<30>(m_filterValue, i);
            i = qBound(0, static_cast<int>(qRound(m_filterValue)), n - 1);
            if (lod->currentIndex() != i)
                lod->setCurrentIndex(i);
            break;
        }
    }
}

void UpdateLevelOfDetailJob::updateEntityLodByScreenArea(Entity *entity, LevelOfDetail *lod)
{
    QMatrix4x4 viewMatrix;
    QMatrix4x4 projectionMatrix;
    if (!viewMatrixForCamera(lod->camera(), viewMatrix, projectionMatrix))
        return;

    PickingUtils::ViewportCameraAreaGatherer vcaGatherer(lod->camera());
    const QVector<PickingUtils::ViewportCameraAreaTriplet> vcaTriplets = vcaGatherer.gather(m_frameGraphRoot);
    if (vcaTriplets.isEmpty())
        return;

    const PickingUtils::ViewportCameraAreaTriplet &vca = vcaTriplets.front();

    const QVector<qreal> thresholds = lod->thresholds();
    Sphere bv(lod->center(), lod->radius());
    if (!lod->hasBoundingVolumeOverride() && entity->worldBoundingVolume() != nullptr) {
        bv = *(entity->worldBoundingVolume());
    } else {
        bv.transform(*entity->worldTransform());
    }

    bv.transform(projectionMatrix * viewMatrix);
    const float sideLength = bv.radius() * 2.f;
    float area = vca.viewport.width() * sideLength * vca.viewport.height() * sideLength;

    const QRect r = windowViewport(vca.area, vca.viewport);
    area =  std::sqrt(area * r.width() * r.height());

    const int n = thresholds.size();
    for (int i = 0; i < n; ++i) {
        if (thresholds[i] < area || i == n -1) {
            m_filterValue = approxRollingAverage<30>(m_filterValue, i);
            i = qBound(0, static_cast<int>(qRound(m_filterValue)), n - 1);
            if (lod->currentIndex() != i)
                lod->setCurrentIndex(i);
            break;
        }
    }
}

} // Render

} // Qt3DRender

QT_END_NAMESPACE
