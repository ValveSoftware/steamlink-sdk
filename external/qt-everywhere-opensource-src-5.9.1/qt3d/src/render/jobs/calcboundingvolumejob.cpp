/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
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

#include "calcboundingvolumejob_p.h"

#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/entity_p.h>
#include <Qt3DRender/private/renderlogging_p.h>
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/geometryrenderer_p.h>
#include <Qt3DRender/private/geometry_p.h>
#include <Qt3DRender/private/buffermanager_p.h>
#include <Qt3DRender/private/attribute_p.h>
#include <Qt3DRender/private/buffer_p.h>
#include <Qt3DRender/private/sphere_p.h>
#include <Qt3DRender/private/buffervisitor_p.h>

#include <QtCore/qmath.h>
#include <QtConcurrent/QtConcurrent>
#include <Qt3DRender/private/job_common_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

namespace {

void calculateLocalBoundingVolume(NodeManagers *manager, Entity *node);

struct UpdateBoundFunctor {
    NodeManagers *manager;

    void operator ()(Qt3DRender::Render::Entity *node)
    {
        calculateLocalBoundingVolume(manager, node);
    }
};

class BoundingVolumeCalculator
{
public:
    BoundingVolumeCalculator(NodeManagers *manager) : m_manager(manager) { }

    const Sphere& result() { return m_volume; }

    bool apply(Qt3DRender::Render::Attribute *positionAttribute)
    {
        FindExtremePoints findExtremePoints(m_manager);
        if (!findExtremePoints.apply(positionAttribute))
            return false;

        // Calculate squared distance for the pairs of points
        const float xDist2 = (findExtremePoints.xMaxPt - findExtremePoints.xMinPt).lengthSquared();
        const float yDist2 = (findExtremePoints.yMaxPt - findExtremePoints.yMinPt).lengthSquared();
        const float zDist2 = (findExtremePoints.zMaxPt - findExtremePoints.zMinPt).lengthSquared();

        // Select most distant pair
        QVector3D p = findExtremePoints.xMinPt;
        QVector3D q = findExtremePoints.xMaxPt;
        if (yDist2 > xDist2 && yDist2 > zDist2) {
            p = findExtremePoints.yMinPt;
            q = findExtremePoints.yMaxPt;
        }
        if (zDist2 > xDist2 && zDist2 > yDist2) {
            p = findExtremePoints.zMinPt;
            q = findExtremePoints.zMaxPt;
        }

        const QVector3D c = 0.5f * (p + q);
        m_volume.setCenter(c);
        m_volume.setRadius((q - c).length());

        ExpandSphere expandSphere(m_manager, m_volume);
        if (!expandSphere.apply(positionAttribute))
            return false;

        return true;
    }

private:
    Sphere m_volume;
    NodeManagers *m_manager;

    class FindExtremePoints : public Buffer3fVisitor
    {
    public:
        FindExtremePoints(NodeManagers *manager)
            : Buffer3fVisitor(manager)
            , xMin(0.0f), xMax(0.0f), yMin(0.0f), yMax(0.0f), zMin(0.0f), zMax(0.0f)
        { }

        float xMin, xMax, yMin, yMax, zMin, zMax;
        QVector3D xMinPt, xMaxPt, yMinPt, yMaxPt, zMinPt, zMaxPt;

        void visit(uint ndx, float x, float y, float z) override
        {
            if (ndx) {
                if (x < xMin) {
                    xMin = x;
                    xMinPt = QVector3D(x, y, z);
                }
                if (x > xMax) {
                    xMax = x;
                    xMaxPt = QVector3D(x, y, z);
                }
                if (y < yMin) {
                    yMin = y;
                    yMinPt = QVector3D(x, y, z);
                }
                if (y > yMax) {
                    yMax = y;
                    yMaxPt = QVector3D(x, y, z);
                }
                if (z < zMin) {
                    zMin = z;
                    zMinPt = QVector3D(x, y, z);
                }
                if (z > zMax) {
                    zMax = z;
                    zMaxPt = QVector3D(x, y, z);
                }
            } else {
                xMin = xMax = x;
                yMin = yMax = y;
                zMin = zMax = z;
                xMinPt = xMaxPt = yMinPt = yMaxPt = zMinPt = zMaxPt = QVector3D(x, y, z);
            }
        }
    };

    class ExpandSphere : public Buffer3fVisitor
    {
    public:
        ExpandSphere(NodeManagers *manager, Sphere& volume)
            : Buffer3fVisitor(manager), m_volume(volume)
        { }

        Sphere& m_volume;
        void visit(uint ndx, float x, float y, float z) override
        {
            Q_UNUSED(ndx);
            m_volume.expandToContain(QVector3D(x, y, z));
        }
    };
};

void calculateLocalBoundingVolume(NodeManagers *manager, Entity *node)
{
    // The Bounding volume will only be computed if the position Buffer
    // isDirty

    if (!node->isTreeEnabled())
        return;

    GeometryRenderer *gRenderer = node->renderComponent<GeometryRenderer>();
    if (gRenderer) {
        Geometry *geom = manager->lookupResource<Geometry, GeometryManager>(gRenderer->geometryId());

        if (geom) {
            Qt3DRender::Render::Attribute *positionAttribute = manager->lookupResource<Attribute, AttributeManager>(geom->boundingPositionAttribute());

            // Use the default position attribute if attribute is null
            if (!positionAttribute) {
                const auto attrIds = geom->attributes();
                for (const Qt3DCore::QNodeId attrId : attrIds) {
                    positionAttribute = manager->lookupResource<Attribute, AttributeManager>(attrId);
                    if (positionAttribute &&
                            positionAttribute->name() == QAttribute::defaultPositionAttributeName())
                        break;
                }
            }

            if (!positionAttribute
                    || positionAttribute->attributeType() != QAttribute::VertexAttribute
                    || positionAttribute->vertexBaseType() != QAttribute::Float
                    || positionAttribute->vertexSize() < 3) {
                qWarning() << "QGeometry::boundingVolumePositionAttribute position Attribute not suited for bounding volume computation";
                return;
            }

            if (positionAttribute) {
                Buffer *buf = manager->lookupResource<Buffer, BufferManager>(positionAttribute->bufferId());
                // No point in continuing if the positionAttribute doesn't have a suitable buffer
                if (!buf) {
                    qWarning() << "ObjectPicker position Attribute not referencing a valid buffer";
                    return;
                }

                // Buf will be set to not dirty once it's loaded
                // in a job executed after this one
                // We need to recompute the bounding volume
                // If anything in the GeometryRenderer has changed
                if (buf->isDirty() ||
                        node->isBoundingVolumeDirty() ||
                        positionAttribute->isDirty() ||
                        geom->isDirty() ||
                        gRenderer->isDirty()) {

                    BoundingVolumeCalculator reader(manager);
                    if (reader.apply(positionAttribute)) {
                        node->localBoundingVolume()->setCenter(reader.result().center());
                        node->localBoundingVolume()->setRadius(reader.result().radius());
                        node->unsetBoundingVolumeDirty();
                    }
                }
            }
        }
    }

    const QVector<Qt3DRender::Render::Entity *> children = node->children();
    if (children.size() > 1) {
        UpdateBoundFunctor functor;
        functor.manager = manager;
        QtConcurrent::blockingMap(children, functor);
    } else {
        const auto children = node->children();
        for (Entity *child : children)
            calculateLocalBoundingVolume(manager, child);
    }
}

} // anonymous

CalculateBoundingVolumeJob::CalculateBoundingVolumeJob()
    : m_manager(nullptr)
    , m_node(nullptr)
{
    SET_JOB_RUN_STAT_TYPE(this, JobTypes::CalcBoundingVolume, 0);
}

void CalculateBoundingVolumeJob::run()
{
    calculateLocalBoundingVolume(m_manager, m_node);
}

void CalculateBoundingVolumeJob::setRoot(Entity *node)
{
    m_node = node;
}

void CalculateBoundingVolumeJob::setManagers(NodeManagers *manager)
{
    m_manager = manager;
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE

