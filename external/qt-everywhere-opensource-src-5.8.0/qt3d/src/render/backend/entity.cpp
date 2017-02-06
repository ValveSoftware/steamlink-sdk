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

#include "entity_p.h"
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/qabstractlight.h>
#include <Qt3DRender/qlayer.h>
#include <Qt3DRender/qmaterial.h>
#include <Qt3DRender/qmesh.h>
#include <Qt3DRender/private/renderlogging_p.h>
#include <Qt3DRender/private/sphere_p.h>
#include <Qt3DRender/qshaderdata.h>
#include <Qt3DRender/qgeometryrenderer.h>
#include <Qt3DRender/qobjectpicker.h>
#include <Qt3DRender/qcomputecommand.h>
#include <Qt3DRender/private/geometryrenderermanager_p.h>

#include <Qt3DRender/qcameralens.h>
#include <Qt3DCore/qcomponentaddedchange.h>
#include <Qt3DCore/qcomponentremovedchange.h>
#include <Qt3DCore/qentity.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qtransform.h>
#include <Qt3DCore/private/qentity_p.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qnodecreatedchange.h>

#include <QMatrix4x4>
#include <QString>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

Entity::Entity()
    : BackendNode()
    , m_nodeManagers(nullptr)
    , m_boundingDirty(false)
{
}

Entity::~Entity()
{
    cleanup();
}

void Entity::cleanup()
{
    if (m_nodeManagers != nullptr) {
        Entity *parentEntity = parent();
        if (parentEntity != nullptr)
            parentEntity->removeChildHandle(m_handle);
        for (int i = 0; i < m_childrenHandles.size(); ++i)
            m_nodeManagers->renderNodesManager()->release(m_childrenHandles[i]);
        // We need to release using peerId otherwise the handle will be cleared
        // but would still remain in the Id to Handle table
        m_nodeManagers->worldMatrixManager()->releaseResource(peerId());

        qCDebug(Render::RenderNodes) << Q_FUNC_INFO;

    }
    m_worldTransform = HMatrix();
    // Release all component will have to perform their own release when they receive the
    // NodeDeleted notification
    // Clear components
    m_transformComponent = Qt3DCore::QNodeId();
    m_cameraComponent = Qt3DCore::QNodeId();
    m_materialComponent = Qt3DCore::QNodeId();
    m_geometryRendererComponent = Qt3DCore::QNodeId();
    m_objectPickerComponent = QNodeId();
    m_boundingVolumeDebugComponent = QNodeId();
    m_computeComponent = QNodeId();
    m_childrenHandles.clear();
    m_layerComponents.clear();
    m_shaderDataComponents.clear();
    m_lightComponents.clear();
    m_localBoundingVolume.reset();
    m_worldBoundingVolume.reset();
    m_worldBoundingVolumeWithChildren.reset();
    m_boundingDirty = false;
    QBackendNode::setEnabled(false);
}

void Entity::setParentHandle(HEntity parentHandle)
{
    Q_ASSERT(m_nodeManagers);
    // Remove ourselves from previous parent children list
    Entity *parent = m_nodeManagers->renderNodesManager()->data(parentHandle);
    if (parent != nullptr && parent->m_childrenHandles.contains(m_handle))
        parent->m_childrenHandles.remove(m_handle);
    m_parentHandle = parentHandle;
    parent = m_nodeManagers->renderNodesManager()->data(parentHandle);
    if (parent != nullptr && !parent->m_childrenHandles.contains(m_handle))
        parent->m_childrenHandles.append(m_handle);
}

void Entity::setNodeManagers(NodeManagers *manager)
{
    m_nodeManagers = manager;
}

void Entity::setHandle(HEntity handle)
{
    m_handle = handle;
}

void Entity::initializeFromPeer(const QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<QNodeCreatedChange<Qt3DCore::QEntityData>>(change);
    const auto &data = typedChange->data;

    // Note this is *not* the parentId as that is the ID of the parent QNode, which is not
    // necessarily the same as the parent QEntity (which may be further up the tree).
    const QNodeId parentEntityId = data.parentEntityId;
    qCDebug(Render::RenderNodes) << "Creating Entity id =" << peerId() << "parentId =" << parentEntityId;

    // TODO: Store string id instead and only in debug mode
    //m_objectName = peer->objectName();
    m_worldTransform = m_nodeManagers->worldMatrixManager()->getOrAcquireHandle(peerId());

    // TODO: Suboptimal -> Maybe have a Hash<QComponent, QEntityList> instead
    m_transformComponent = QNodeId();
    m_materialComponent = QNodeId();
    m_cameraComponent = QNodeId();
    m_geometryRendererComponent = QNodeId();
    m_objectPickerComponent = QNodeId();
    m_boundingVolumeDebugComponent = QNodeId();
    m_computeComponent = QNodeId();
    m_layerComponents.clear();
    m_shaderDataComponents.clear();
    m_lightComponents.clear();
    m_localBoundingVolume.reset(new Sphere(peerId()));
    m_worldBoundingVolume.reset(new Sphere(peerId()));
    m_worldBoundingVolumeWithChildren.reset(new Sphere(peerId()));

    for (const auto &idAndType : qAsConst(data.componentIdsAndTypes))
        addComponent(idAndType);

    if (!parentEntityId.isNull())
        setParentHandle(m_nodeManagers->renderNodesManager()->lookupHandle(parentEntityId));
    else
        qCDebug(Render::RenderNodes) << Q_FUNC_INFO << "No parent entity found for Entity" << peerId();
}

void Entity::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    switch (e->type()) {

    case ComponentAdded: {
        QComponentAddedChangePtr change = qSharedPointerCast<QComponentAddedChange>(e);
        const auto componentIdAndType = QNodeIdTypePair(change->componentId(), change->componentMetaObject());
        addComponent(componentIdAndType);
        qCDebug(Render::RenderNodes) << Q_FUNC_INFO << "Component Added. Id =" << change->componentId();
        break;
    }

    case ComponentRemoved: {
        QComponentRemovedChangePtr change = qSharedPointerCast<QComponentRemovedChange>(e);
        removeComponent(change->componentId());
        qCDebug(Render::RenderNodes) << Q_FUNC_INFO << "Component Removed. Id =" << change->componentId();
        break;
    }

    case PropertyValueAdded: {
        QPropertyNodeAddedChangePtr change = qSharedPointerCast<QPropertyNodeAddedChange>(e);
        if (change->metaObject()->inherits(&QEntity::staticMetaObject))
            appendChildHandle(m_nodeManagers->renderNodesManager()->lookupHandle(change->addedNodeId()));
        break;
    }

    case PropertyValueRemoved: {
        QPropertyNodeRemovedChangePtr change = qSharedPointerCast<QPropertyNodeRemovedChange>(e);
        if (change->metaObject()->inherits(&QEntity::staticMetaObject))
            removeChildHandle(m_nodeManagers->renderNodesManager()->lookupHandle(change->removedNodeId()));
        break;
    }


    default:
        break;
    }
    markDirty(AbstractRenderer::AllDirty);
    BackendNode::sceneChangeEvent(e);
}

void Entity::dump() const
{
    static int depth = 0;
    QString indent(2 * depth++, QChar::fromLatin1(' '));
    qCDebug(Backend) << indent + m_objectName;
    const auto children_ = children();
    for (const Entity *child : children_)
        child->dump();
    --depth;
}

Entity *Entity::parent() const
{
    return m_nodeManagers->renderNodesManager()->data(m_parentHandle);
}

void Entity::appendChildHandle(HEntity childHandle)
{
    if (!m_childrenHandles.contains(childHandle)) {
        m_childrenHandles.append(childHandle);
        Entity *child = m_nodeManagers->renderNodesManager()->data(childHandle);
        if (child != nullptr)
            child->m_parentHandle = m_handle;
    }
}

QVector<Entity *> Entity::children() const
{
    QVector<Entity *> childrenVector;
    childrenVector.reserve(m_childrenHandles.size());
    for (HEntity handle : m_childrenHandles) {
        Entity *child = m_nodeManagers->renderNodesManager()->data(handle);
        if (child != nullptr)
            childrenVector.append(child);
    }
    return childrenVector;
}

QMatrix4x4 *Entity::worldTransform()
{
    return m_nodeManagers->worldMatrixManager()->data(m_worldTransform);
}

const QMatrix4x4 *Entity::worldTransform() const
{
    return m_nodeManagers->worldMatrixManager()->data(m_worldTransform);
}

void Entity::addComponent(Qt3DCore::QComponent *component)
{
    // The backend element is always created when this method is called
    // If that's not the case something has gone wrong

    if (qobject_cast<Qt3DCore::QTransform*>(component) != nullptr) {
        m_transformComponent = component->id();
    } else if (qobject_cast<QCameraLens *>(component) != nullptr) {
        m_cameraComponent = component->id();
    } else if (qobject_cast<QLayer *>(component) != nullptr) {
        m_layerComponents.append(component->id());
    } else if (qobject_cast<QMaterial *>(component) != nullptr) {
        m_materialComponent = component->id();
    } else if (qobject_cast<QAbstractLight *>(component) != nullptr) {
        m_lightComponents.append(component->id());
    } else if (qobject_cast<QShaderData *>(component) != nullptr) {
        m_shaderDataComponents.append(component->id());
    } else if (qobject_cast<QGeometryRenderer *>(component) != nullptr) {
        m_geometryRendererComponent = component->id();
        m_boundingDirty = true;
    } else if (qobject_cast<QObjectPicker *>(component) != nullptr) {
        m_objectPickerComponent = component->id();
//    } else if (qobject_cast<QBoundingVolumeDebug *>(component) != nullptr) {
//        m_boundingVolumeDebugComponent = component->id();
    } else if (qobject_cast<QComputeCommand *>(component) != nullptr) {
        m_computeComponent = component->id();
    }
}

void Entity::addComponent(Qt3DCore::QNodeIdTypePair idAndType)
{
    // The backend element is always created when this method is called
    // If that's not the case something has gone wrong
    const auto type = idAndType.type;
    const auto id = idAndType.id;
    qCDebug(Render::RenderNodes) << Q_FUNC_INFO << "id =" << id << type->className();
    if (type->inherits(&Qt3DCore::QTransform::staticMetaObject)) {
        m_transformComponent = id;
    } else if (type->inherits(&QCameraLens::staticMetaObject)) {
        m_cameraComponent = id;
    } else if (type->inherits(&QLayer::staticMetaObject)) {
        m_layerComponents.append(id);
    } else if (type->inherits(&QMaterial::staticMetaObject)) {
        m_materialComponent = id;
    } else if (type->inherits(&QAbstractLight::staticMetaObject)) { // QAbstractLight subclasses QShaderData
        m_lightComponents.append(id);
    } else if (type->inherits(&QShaderData::staticMetaObject)) {
        m_shaderDataComponents.append(id);
    } else if (type->inherits(&QGeometryRenderer::staticMetaObject)) {
        m_geometryRendererComponent = id;
        m_boundingDirty = true;
    } else if (type->inherits(&QObjectPicker::staticMetaObject)) {
        m_objectPickerComponent = id;
//    } else if (type->inherits(&QBoundingVolumeDebug::staticMetaObject)) {
//        m_boundingVolumeDebugComponent = id;
    } else if (type->inherits(&QComputeCommand::staticMetaObject)) {
        m_computeComponent = id;
    }
}

void Entity::removeComponent(Qt3DCore::QNodeId nodeId)
{
    if (m_transformComponent == nodeId) {
        m_transformComponent = QNodeId();
    } else if (m_cameraComponent == nodeId) {
        m_cameraComponent = QNodeId();
    } else if (m_layerComponents.contains(nodeId)) {
        m_layerComponents.removeAll(nodeId);
    } else if (m_materialComponent == nodeId) {
        m_materialComponent = QNodeId();
    } else if (m_shaderDataComponents.contains(nodeId)) {
        m_shaderDataComponents.removeAll(nodeId);
    } else if (m_geometryRendererComponent == nodeId) {
        m_geometryRendererComponent = QNodeId();
        m_boundingDirty = true;
    } else if (m_objectPickerComponent == nodeId) {
        m_objectPickerComponent = QNodeId();
//    } else if (m_boundingVolumeDebugComponent == nodeId) {
//        m_boundingVolumeDebugComponent = QNodeId();
    } else if (m_lightComponents.contains(nodeId)) {
        m_lightComponents.removeAll(nodeId);
    } else if (m_computeComponent == nodeId) {
        m_computeComponent = QNodeId();
    }
}

bool Entity::isBoundingVolumeDirty() const
{
    return m_boundingDirty;
}

void Entity::unsetBoundingVolumeDirty()
{
    m_boundingDirty = false;
}

// Handles

template<>
HMaterial Entity::componentHandle<Material>() const
{
    return m_nodeManagers->materialManager()->lookupHandle(m_materialComponent);
}

template<>
HCamera Entity::componentHandle<CameraLens>() const
{
    return m_nodeManagers->cameraManager()->lookupHandle(m_cameraComponent);
}

template<>
HTransform Entity::componentHandle<Transform>() const
{
    return m_nodeManagers->transformManager()->lookupHandle(m_transformComponent);
}

template<>
HGeometryRenderer Entity::componentHandle<GeometryRenderer>() const
{
    return m_nodeManagers->geometryRendererManager()->lookupHandle(m_geometryRendererComponent);
}

template<>
HObjectPicker Entity::componentHandle<ObjectPicker>() const
{
    return m_nodeManagers->objectPickerManager()->lookupHandle(m_objectPickerComponent);
}

template<>
QVector<HLayer> Entity::componentsHandle<Layer>() const
{
    QVector<HLayer> layerHandles;
    layerHandles.reserve(m_layerComponents.size());
    for (QNodeId id : m_layerComponents)
        layerHandles.append(m_nodeManagers->layerManager()->lookupHandle(id));
    return layerHandles;
}

template<>
QVector<HShaderData> Entity::componentsHandle<ShaderData>() const
{
    QVector<HShaderData> shaderDataHandles;
    shaderDataHandles.reserve(m_shaderDataComponents.size());
    for (QNodeId id : m_shaderDataComponents)
        shaderDataHandles.append(m_nodeManagers->shaderDataManager()->lookupHandle(id));
    return shaderDataHandles;
}

//template<>
//HBoundingVolumeDebug Entity::componentHandle<BoundingVolumeDebug>() const
//{
//    return m_nodeManagers->boundingVolumeDebugManager()->lookupHandle(m_boundingVolumeDebugComponent);
//}

template<>
QVector<HLight> Entity::componentsHandle<Light>() const
{
    QVector<HLight> lightHandles;
    lightHandles.reserve(m_lightComponents.size());
    for (QNodeId id : m_lightComponents)
        lightHandles.append(m_nodeManagers->lightManager()->lookupHandle(id));
    return lightHandles;
}

template<>
HComputeCommand Entity::componentHandle<ComputeCommand>() const
{
    return m_nodeManagers->computeJobManager()->lookupHandle(m_computeComponent);
}

// Render components

template<>
Material *Entity::renderComponent<Material>() const
{
    return m_nodeManagers->materialManager()->lookupResource(m_materialComponent);
}

template<>
CameraLens *Entity::renderComponent<CameraLens>() const
{
    return m_nodeManagers->cameraManager()->lookupResource(m_cameraComponent);
}

template<>
Transform *Entity::renderComponent<Transform>() const
{
    return m_nodeManagers->transformManager()->lookupResource(m_transformComponent);
}

template<>
GeometryRenderer *Entity::renderComponent<GeometryRenderer>() const
{
    return m_nodeManagers->geometryRendererManager()->lookupResource(m_geometryRendererComponent);
}

template<>
ObjectPicker *Entity::renderComponent<ObjectPicker>() const
{
    return m_nodeManagers->objectPickerManager()->lookupResource(m_objectPickerComponent);
}

template<>
QVector<Layer *> Entity::renderComponents<Layer>() const
{
    QVector<Layer *> layers;
    layers.reserve(m_layerComponents.size());
    for (QNodeId id : m_layerComponents)
        layers.append(m_nodeManagers->layerManager()->lookupResource(id));
    return layers;
}

template<>
QVector<ShaderData *> Entity::renderComponents<ShaderData>() const
{
    QVector<ShaderData *> shaderDatas;
    shaderDatas.reserve(m_shaderDataComponents.size());
    for (QNodeId id : m_shaderDataComponents)
        shaderDatas.append(m_nodeManagers->shaderDataManager()->lookupResource(id));
    return shaderDatas;
}

template<>
QVector<Light *> Entity::renderComponents<Light>() const
{
    QVector<Light *> lights;
    lights.reserve(m_lightComponents.size());
    for (QNodeId id : m_lightComponents)
        lights.append(m_nodeManagers->lightManager()->lookupResource(id));
    return lights;
}

//template<>
//BoundingVolumeDebug *Entity::renderComponent<BoundingVolumeDebug>() const
//{
//    return m_nodeManagers->boundingVolumeDebugManager()->lookupResource(m_boundingVolumeDebugComponent);
//}

template<>
ComputeCommand *Entity::renderComponent<ComputeCommand>() const
{
    return m_nodeManagers->computeJobManager()->lookupResource(m_computeComponent);
}

// Uuid

template<>
Qt3DCore::QNodeId Entity::componentUuid<Transform>() const { return m_transformComponent; }

template<>
Qt3DCore::QNodeId Entity::componentUuid<CameraLens>() const { return m_cameraComponent; }

template<>
Qt3DCore::QNodeId Entity::componentUuid<Material>() const { return m_materialComponent; }

template<>
QVector<Qt3DCore::QNodeId> Entity::componentsUuid<Layer>() const { return m_layerComponents; }

template<>
QVector<Qt3DCore::QNodeId> Entity::componentsUuid<ShaderData>() const { return m_shaderDataComponents; }

template<>
Qt3DCore::QNodeId Entity::componentUuid<GeometryRenderer>() const { return m_geometryRendererComponent; }

template<>
QNodeId Entity::componentUuid<ObjectPicker>() const { return m_objectPickerComponent; }

template<>
QNodeId Entity::componentUuid<BoundingVolumeDebug>() const { return m_boundingVolumeDebugComponent; }

template<>
QNodeId Entity::componentUuid<ComputeCommand>() const { return m_computeComponent; }

template<>
QVector<Qt3DCore::QNodeId> Entity::componentsUuid<Light>() const { return m_lightComponents; }

RenderEntityFunctor::RenderEntityFunctor(AbstractRenderer *renderer, NodeManagers *manager)
    : m_nodeManagers(manager)
    , m_renderer(renderer)
{
}

Qt3DCore::QBackendNode *RenderEntityFunctor::create(const Qt3DCore::QNodeCreatedChangeBasePtr &change) const
{
    HEntity renderNodeHandle = m_nodeManagers->renderNodesManager()->getOrAcquireHandle(change->subjectId());
    Entity *entity = m_nodeManagers->renderNodesManager()->data(renderNodeHandle);
    entity->setNodeManagers(m_nodeManagers);
    entity->setHandle(renderNodeHandle);
    entity->setRenderer(m_renderer);
    return entity;
}

Qt3DCore::QBackendNode *RenderEntityFunctor::get(Qt3DCore::QNodeId id) const
{
    return m_nodeManagers->renderNodesManager()->lookupResource(id);
}

void RenderEntityFunctor::destroy(Qt3DCore::QNodeId id) const
{
    m_nodeManagers->renderNodesManager()->releaseResource(id);
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
