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

void calculateLocalBoundingVolume(NodeManagers *manager, Entity *node)
{
    // The Bounding volume will only be computed if the position Buffer
    // isDirty

    GeometryRenderer *gRenderer = node->renderComponent<GeometryRenderer>();
    if (gRenderer) {
        Geometry *geom = manager->lookupResource<Geometry, GeometryManager>(gRenderer->geometryId());

        if (geom) {
            Qt3DRender::Render::Attribute *pickVolumeAttribute = manager->lookupResource<Attribute, AttributeManager>(geom->boundingPositionAttribute());

            // Use the default position attribute if attribute is null
            if (!pickVolumeAttribute) {
                const auto attrIds = geom->attributes();
                for (const Qt3DCore::QNodeId attrId : attrIds) {
                    pickVolumeAttribute = manager->lookupResource<Attribute, AttributeManager>(attrId);
                    if (pickVolumeAttribute &&
                            pickVolumeAttribute->name() == QAttribute::defaultPositionAttributeName())
                        break;
                }
            }

            if (pickVolumeAttribute) {
                if (!pickVolumeAttribute
                        || pickVolumeAttribute->attributeType() != QAttribute::VertexAttribute
                        || pickVolumeAttribute->vertexBaseType() != QAttribute::Float
                        || pickVolumeAttribute->vertexSize() < 3) {
                    qWarning() << "QGeometry::boundingVolumePositionAttribute pickVolume Attribute not suited for bounding volume computation";
                    return;
                }

                Buffer *buf = manager->lookupResource<Buffer, BufferManager>(pickVolumeAttribute->bufferId());
                // No point in continuing if the positionAttribute doesn't have a suitable buffer
                if (!buf) {
                    qWarning() << "ObjectPicker pickVolume Attribute not referencing a valid buffer";
                    return;
                }

                // Buf will be set to not dirty once it's loaded
                // in a job executed after this one
                // We need to recompute the bounding volume
                // If anything in the GeometryRenderer has changed
                if (buf->isDirty() ||
                        node->isBoundingVolumeDirty() ||
                        pickVolumeAttribute->isDirty() ||
                        geom->isDirty() ||
                        gRenderer->isDirty()) {

                    const QByteArray buffer = buf->data();
                    const char *rawBuffer = buffer.constData();
                    rawBuffer += pickVolumeAttribute->byteOffset();
                    const int stride = pickVolumeAttribute->byteStride() ? pickVolumeAttribute->byteStride() : sizeof(float) * pickVolumeAttribute->vertexSize();
                    QVector<QVector3D> vertices(pickVolumeAttribute->count());

                    // TODO avoid copying the vertices
                    for (int c = 0, vC = vertices.size(); c < vC; ++c) {
                        QVector3D v;
                        const float *fptr = reinterpret_cast<const float*>(rawBuffer);
                        // TODO unwrap loop (switch?)
                        for (uint i = 0, m = qMin(pickVolumeAttribute->vertexSize(), 3U); i < m; ++i)
                            v[i] = fptr[i];
                        vertices[c] = v;
                        rawBuffer += stride;
                    }

                    node->localBoundingVolume()->initializeFromPoints(vertices);
                    node->unsetBoundingVolumeDirty();
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

