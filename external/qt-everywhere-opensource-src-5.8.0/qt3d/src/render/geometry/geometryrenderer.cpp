/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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

#include "geometryrenderer_p.h"
#include <Qt3DRender/private/geometryrenderermanager_p.h>
#include <Qt3DRender/private/qboundingvolume_p.h>
#include <Qt3DRender/private/qgeometryrenderer_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qtypedpropertyupdatechange_p.h>
#include <QtCore/qcoreapplication.h>

#include <memory>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

GeometryRenderer::GeometryRenderer()
    : BackendNode(ReadWrite)
    , m_instanceCount(0)
    , m_vertexCount(0)
    , m_indexOffset(0)
    , m_firstInstance(0)
    , m_firstVertex(0)
    , m_restartIndexValue(-1)
    , m_verticesPerPatch(0)
    , m_primitiveRestartEnabled(false)
    , m_primitiveType(QGeometryRenderer::Triangles)
    , m_dirty(false)
    , m_manager(nullptr)
{
}

GeometryRenderer::~GeometryRenderer()
{
}

void GeometryRenderer::cleanup()
{
    BackendNode::setEnabled(false);
    m_instanceCount = 0;
    m_vertexCount = 0;
    m_indexOffset = 0;
    m_firstInstance = 0;
    m_firstVertex = 0;
    m_restartIndexValue = -1;
    m_verticesPerPatch = 0;
    m_primitiveRestartEnabled = false;
    m_primitiveType = QGeometryRenderer::Triangles;
    m_geometryId = Qt3DCore::QNodeId();
    m_dirty = false;
    m_geometryFactory.reset();
    qDeleteAll(m_triangleVolumes);
    m_triangleVolumes.clear();
}

void GeometryRenderer::setManager(GeometryRendererManager *manager)
{
    m_manager = manager;
}

void GeometryRenderer::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QGeometryRendererData>>(change);
    const auto &data = typedChange->data;
    m_geometryId = data.geometryId;
    m_instanceCount = data.instanceCount;
    m_vertexCount = data.vertexCount;
    m_indexOffset = data.indexOffset;
    m_firstInstance = data.firstInstance;
    m_firstVertex = data.firstVertex;
    m_restartIndexValue = data.restartIndexValue;
    m_verticesPerPatch = data.verticesPerPatch;
    m_primitiveRestartEnabled = data.primitiveRestart;
    m_primitiveType = data.primitiveType;

    Q_ASSERT(m_manager);
    m_geometryFactory = data.geometryFactory;
    if (m_geometryFactory)
        m_manager->addDirtyGeometryRenderer(peerId());

    m_dirty = true;
}

void GeometryRenderer::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    switch (e->type()) {
    case PropertyUpdated: {
        QPropertyUpdatedChangePtr propertyChange = qSharedPointerCast<QPropertyUpdatedChange>(e);
        QByteArray propertyName = propertyChange->propertyName();

        if (propertyName == QByteArrayLiteral("instanceCount")) {
            m_instanceCount = propertyChange->value().value<int>();
            m_dirty = true;
        } else if (propertyName == QByteArrayLiteral("vertexCount")) {
            m_vertexCount = propertyChange->value().value<int>();
            m_dirty = true;
        } else if (propertyName == QByteArrayLiteral("indexOffset")) {
            m_indexOffset = propertyChange->value().value<int>();
            m_dirty = true;
        } else if (propertyName == QByteArrayLiteral("firstInstance")) {
            m_firstInstance = propertyChange->value().value<int>();
            m_dirty = true;
        } else if (propertyName == QByteArrayLiteral("firstVertex")) {
            m_firstVertex = propertyChange->value().value<int>();
            m_dirty = true;
        } else if (propertyName == QByteArrayLiteral("restartIndexValue")) {
            m_restartIndexValue = propertyChange->value().value<int>();
            m_dirty = true;
        } else if (propertyName == QByteArrayLiteral("verticesPerPatch")) {
            m_verticesPerPatch = propertyChange->value().value<int>();
            m_dirty = true;
        } else if (propertyName == QByteArrayLiteral("primitiveRestartEnabled")) {
            m_primitiveRestartEnabled = propertyChange->value().value<bool>();
            m_dirty = true;
        } else if (propertyName == QByteArrayLiteral("primitiveType")) {
            m_primitiveType = static_cast<QGeometryRenderer::PrimitiveType>(propertyChange->value().value<int>());
            m_dirty = true;
        } else if (propertyName == QByteArrayLiteral("geometryFactory")) {
            QGeometryFactoryPtr newFunctor = propertyChange->value().value<QGeometryFactoryPtr>();
            m_dirty |= !(newFunctor && m_geometryFactory && *newFunctor == *m_geometryFactory);
            m_geometryFactory = newFunctor;
            if (m_geometryFactory && m_manager != nullptr)
                m_manager->addDirtyGeometryRenderer(peerId());
        } else if (propertyName == QByteArrayLiteral("geometry")) {
            m_geometryId = propertyChange->value().value<Qt3DCore::QNodeId>();
            m_dirty = true;
        }
        break;
    }

    default:
        break;
    }

    markDirty(AbstractRenderer::AllDirty);

    BackendNode::sceneChangeEvent(e);

    // Add to dirty list in manager
}

void GeometryRenderer::executeFunctor()
{
    Q_ASSERT(m_geometryFactory);
    std::unique_ptr<QGeometry> geometry((*m_geometryFactory)());
    if (!geometry)
        return;

    // Move the QGeometry object to the main thread and notify the
    // corresponding QGeometryRenderer
    const auto appThread = QCoreApplication::instance()->thread();
    geometry->moveToThread(appThread);

    auto e = QGeometryChangePtr::create(peerId());
    e->setDeliveryFlags(Qt3DCore::QSceneChange::Nodes);
    e->setPropertyName("geometry");
    e->data = std::move(geometry);
    notifyObservers(e);

    // TODO: Maybe we could also send a status to help troubleshoot errors
}

void GeometryRenderer::unsetDirty()
{
    m_dirty = false;
}


void GeometryRenderer::setTriangleVolumes(const QVector<QBoundingVolume *> &volumes)
{
    qDeleteAll(m_triangleVolumes);
    m_triangleVolumes = volumes;
}

QVector<QBoundingVolume *> GeometryRenderer::triangleData() const
{
    return m_triangleVolumes;
}

GeometryRendererFunctor::GeometryRendererFunctor(AbstractRenderer *renderer, GeometryRendererManager *manager)
    : m_manager(manager)
    , m_renderer(renderer)
{
}

Qt3DCore::QBackendNode *GeometryRendererFunctor::create(const Qt3DCore::QNodeCreatedChangeBasePtr &change) const
{
    GeometryRenderer *geometryRenderer = m_manager->getOrCreateResource(change->subjectId());
    geometryRenderer->setManager(m_manager);
    geometryRenderer->setRenderer(m_renderer);
    return geometryRenderer;
}

Qt3DCore::QBackendNode *GeometryRendererFunctor::get(Qt3DCore::QNodeId id) const
{
    return m_manager->lookupResource(id);
}

void GeometryRendererFunctor::destroy(Qt3DCore::QNodeId id) const
{
    m_manager->releaseResource(id);
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
