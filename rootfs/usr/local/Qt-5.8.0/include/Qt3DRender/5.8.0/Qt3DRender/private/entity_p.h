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

#ifndef QT3DRENDER_RENDER_ENTITY_H
#define QT3DRENDER_RENDER_ENTITY_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <Qt3DRender/private/backendnode_p.h>
#include <Qt3DRender/private/renderer_p.h>
#include <Qt3DRender/private/handle_types_p.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include <Qt3DCore/private/qentity_p.h>
#include <Qt3DCore/private/qhandle_p.h>
#include <QVector>

QT_BEGIN_NAMESPACE

class QMatrix4x4;

namespace Qt3DCore {
class QNode;
class QEntity;
class QComponent;
}

namespace Qt3DRender {

class QRenderAspect;

namespace Render {

class Sphere;
class Renderer;
class NodeManagers;

class Q_AUTOTEST_EXPORT Entity : public BackendNode
{
public:
    Entity();
    ~Entity();
    void cleanup();

    void setParentHandle(HEntity parentHandle);
    void setNodeManagers(NodeManagers *manager);
    void sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e) Q_DECL_OVERRIDE;

    void dump() const;

    void  setHandle(HEntity handle);
    HEntity handle() const { return m_handle; }
    Entity *parent() const;
    HEntity parentHandle() const { return m_parentHandle; }

    void appendChildHandle(HEntity childHandle);
    void removeChildHandle(HEntity childHandle) { m_childrenHandles.removeOne(childHandle); }
    QVector<HEntity> childrenHandles() const { return m_childrenHandles; }
    QVector<Entity *> children() const;
    bool hasChildren() const { return !m_childrenHandles.empty(); }

    QMatrix4x4 *worldTransform();
    const QMatrix4x4 *worldTransform() const;
    Sphere *localBoundingVolume() const { return m_localBoundingVolume.data(); }
    Sphere *worldBoundingVolume() const { return m_worldBoundingVolume.data(); }
    Sphere *worldBoundingVolumeWithChildren() const { return m_worldBoundingVolumeWithChildren.data(); }

    void addComponent(Qt3DCore::QComponent *component);
    void addComponent(Qt3DCore::QNodeIdTypePair idAndType);
    void removeComponent(Qt3DCore::QNodeId nodeId);

    bool isBoundingVolumeDirty() const;
    void unsetBoundingVolumeDirty();

    template<class Backend, uint INDEXBITS>
    Qt3DCore::QHandle<Backend, INDEXBITS> componentHandle() const
    {
        return Qt3DCore::QHandle<Backend, INDEXBITS>();
    }

    template<class Backend, uint INDEXBITS>
    QVector<Qt3DCore::QHandle<Backend, INDEXBITS> > componentsHandle() const
    {
        return QVector<Qt3DCore::QHandle<Backend, INDEXBITS> >();
    }

    template<class Backend>
    Backend *renderComponent() const
    {
        return nullptr;
    }

    template<class Backend>
    QVector<Backend *> renderComponents() const
    {
        return QVector<Backend *>();
    }

    template<class Backend>
    Qt3DCore::QNodeId componentUuid() const
    {
        return Qt3DCore::QNodeId();
    }

    template<class Backend>
    QVector<Qt3DCore::QNodeId> componentsUuid() const
    {
        return QVector<Qt3DCore::QNodeId>();
    }

    template<typename T>
    bool containsComponentsOfType() const
    {
        return !componentUuid<T>().isNull();
    }

    template<typename T, typename Ts, typename ... Ts2>
    bool containsComponentsOfType() const
    {
        return containsComponentsOfType<T>() && containsComponentsOfType<Ts, Ts2...>();
    }


private:
    void initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change) Q_DECL_FINAL;

    NodeManagers *m_nodeManagers;
    HEntity m_handle;
    HEntity m_parentHandle;
    QVector<HEntity > m_childrenHandles;

    HMatrix m_worldTransform;
    QSharedPointer<Sphere> m_localBoundingVolume;
    QSharedPointer<Sphere> m_worldBoundingVolume;
    QSharedPointer<Sphere> m_worldBoundingVolumeWithChildren;

    // Handles to Components
    Qt3DCore::QNodeId m_transformComponent;
    Qt3DCore::QNodeId m_materialComponent;
    Qt3DCore::QNodeId m_cameraComponent;
    QVector<Qt3DCore::QNodeId> m_layerComponents;
    QVector<Qt3DCore::QNodeId> m_shaderDataComponents;
    QVector<Qt3DCore::QNodeId> m_lightComponents;
    Qt3DCore::QNodeId m_geometryRendererComponent;
    Qt3DCore::QNodeId m_objectPickerComponent;
    Qt3DCore::QNodeId m_boundingVolumeDebugComponent;
    Qt3DCore::QNodeId m_computeComponent;

    QString m_objectName;
    bool m_boundingDirty;
};

// Handles
template<>
HMaterial Entity::componentHandle<Material>() const;

template<>
HCamera Entity::componentHandle<CameraLens>() const;

template<>
HTransform Entity::componentHandle<Transform>() const;

template<>
Q_AUTOTEST_EXPORT HGeometryRenderer Entity::componentHandle<GeometryRenderer>() const;

template<>
Q_AUTOTEST_EXPORT HObjectPicker Entity::componentHandle<ObjectPicker>() const;

template<>
QVector<HLayer> Entity::componentsHandle<Layer>() const;

template<>
QVector<HShaderData> Entity::componentsHandle<ShaderData>() const;

//template<>
//Q_AUTOTEST_EXPORT HBoundingVolumeDebug Entity::componentHandle<BoundingVolumeDebug>() const;

template<>
QVector<HLight> Entity::componentsHandle<Light>() const;

template<>
Q_AUTOTEST_EXPORT HComputeCommand Entity::componentHandle<ComputeCommand>() const;

// Render components
template<>
Material *Entity::renderComponent<Material>() const;

template<>
CameraLens *Entity::renderComponent<CameraLens>() const;

template<>
Transform *Entity::renderComponent<Transform>() const;

template<>
Q_AUTOTEST_EXPORT GeometryRenderer *Entity::renderComponent<GeometryRenderer>() const;

template<>
Q_AUTOTEST_EXPORT ObjectPicker *Entity::renderComponent<ObjectPicker>() const;

template<>
QVector<Layer *> Entity::renderComponents<Layer>() const;

template<>
QVector<ShaderData *> Entity::renderComponents<ShaderData>() const;

//template<>
//Q_AUTOTEST_EXPORT BoundingVolumeDebug *Entity::renderComponent<BoundingVolumeDebug>() const;

template<>
QVector<Light *> Entity::renderComponents<Light>() const;

template<>
Q_AUTOTEST_EXPORT ComputeCommand *Entity::renderComponent<ComputeCommand>() const;

// UUid
template<>
Q_AUTOTEST_EXPORT Qt3DCore::QNodeId Entity::componentUuid<Transform>() const;

template<>
Q_AUTOTEST_EXPORT Qt3DCore::QNodeId Entity::componentUuid<CameraLens>() const;

template<>
Q_AUTOTEST_EXPORT Qt3DCore::QNodeId Entity::componentUuid<Material>() const;

template<>
Q_AUTOTEST_EXPORT QVector<Qt3DCore::QNodeId> Entity::componentsUuid<Layer>() const;

template<>
Q_AUTOTEST_EXPORT QVector<Qt3DCore::QNodeId> Entity::componentsUuid<ShaderData>() const;

template<>
Q_AUTOTEST_EXPORT Qt3DCore::QNodeId Entity::componentUuid<GeometryRenderer>() const;

template<>
Q_AUTOTEST_EXPORT Qt3DCore::QNodeId Entity::componentUuid<ObjectPicker>() const;

//template<>
//Q_AUTOTEST_EXPORT Qt3DCore::QNodeId Entity::componentUuid<BoundingVolumeDebug>() const;

template<>
Q_AUTOTEST_EXPORT Qt3DCore::QNodeId Entity::componentUuid<ComputeCommand>() const;

template<>
Q_AUTOTEST_EXPORT QVector<Qt3DCore::QNodeId> Entity::componentsUuid<Light>() const;

class RenderEntityFunctor : public Qt3DCore::QBackendNodeMapper
{
public:
    explicit RenderEntityFunctor(AbstractRenderer *renderer, NodeManagers *manager);
    Qt3DCore::QBackendNode *create(const Qt3DCore::QNodeCreatedChangeBasePtr &change) const Q_DECL_OVERRIDE;
    Qt3DCore::QBackendNode *get(Qt3DCore::QNodeId id) const Q_DECL_OVERRIDE;
    void destroy(Qt3DCore::QNodeId id) const Q_DECL_OVERRIDE;

private:
    NodeManagers *m_nodeManagers;
    AbstractRenderer *m_renderer;
};

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE

#endif // QT3DRENDER_RENDER_ENTITY_H
