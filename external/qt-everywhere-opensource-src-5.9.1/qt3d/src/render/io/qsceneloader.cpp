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

#include "qsceneloader.h"
#include "qsceneloader_p.h"
#include <Qt3DCore/private/qscene_p.h>

#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qentity.h>
#include <Qt3DCore/qtransform.h>
#include <Qt3DRender/qgeometryrenderer.h>
#include <Qt3DRender/qmaterial.h>
#include <Qt3DRender/qabstractlight.h>
#include <Qt3DRender/qcameralens.h>

#include <private/renderlogging_p.h>

#include <QThread>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {

/*!
    \class Qt3DRender::QSceneLoader
    \inmodule Qt3DRender
    \since 5.7
    \ingroup io

    \brief Provides the facility to load an existing Scene

    Given a 3D source file, the Qt3DRender::QSceneLoader will try to parse it and
    build a tree of Qt3DCore::QEntity objects with proper Qt3DRender::QGeometryRenderer,
    Qt3DCore::QTransform and Qt3DRender::QMaterial components.

    The loader will try to determine the best material to be used based on the properties
    of the model file. If you wish to use a custom material, you will have to traverse
    the tree and replace the default associated materials with yours.

    As the name implies, Qt3DRender::QSceneLoader loads a complete scene subtree.
    If you wish to load a single piece of geometry, you should rather use
    the Qt3DRender::QMesh instead.

    Qt3DRender::QSceneLoader internally relies on the use of plugins to support a
    wide variety of 3D file formats. \l
    {http://www.assimp.org/main_features_formats.html}{Here} is a list of formats
    that are supported by Qt3D.

    \note this component shouldn't be shared among several Qt3DCore::QEntity instances.
    Undefined behavior will result.

    \sa Qt3DRender::QMesh
    \sa Qt3DRender::QGeometryRenderer
 */

/*!
    \qmltype SceneLoader
    \inqmlmodule Qt3D.Render
    \instantiates Qt3DRender::QSceneLoader
    \inherits Component
    \since 5.7
    \brief Provides the facility to load an existing Scene

    Given a 3D source file, the SceneLoader will try to parse it and build a
    tree of Entity objects with proper GeometryRenderer, Transform and Material
    components.

    The loader will try to determine the best material to be used based on the
    properties of the model file. If you wish to use a custom material, you
    will have to traverse the tree and replace the default associated materials
    with yours.

    As the name implies, SceneLoader loads a complete scene subtree. If you
    wish to load a single piece of geometry, you should rather use the
    Mesh instead.

    SceneLoader internally relies on the use of plugins to support a wide
    variety of 3D file formats. \l
    {http://www.assimp.org/main_features_formats.html}{Here} is a list of
    formats that are supported by Qt3D.

    \note this component shouldn't be shared among several Entity instances.
    Undefined behavior will result.

    \sa Mesh
    \sa GeometryRenderer
 */

/*!
    \enum QSceneLoader::Status

    This enum identifies the state of loading
    \value None     The Qt3DRender::QSceneLoader hasn't been used yet.
    \value Loading  The Qt3DRender::QSceneLoader is currently loading the scene file.
    \value Ready    The Qt3DRender::QSceneLoader successfully loaded the scene file.
    \value Error    The Qt3DRender::QSceneLoader encountered an error while loading the scene file.
 */

/*!
    \enum QSceneLoader::ComponentType

    This enum specifies a component type.
    \value UnknownComponent Unknown component type
    \value GeometryRendererComponent Qt3DRender::QGeometryRenderer component
    \value TransformComponent Qt3DCore::QTransform component
    \value MaterialComponent Qt3DRender::QMaterial component
    \value LightComponent Qt3DRender::QAbstractLight component
    \value CameraLensComponent Qt3DRender::QCameraLens component
 */

/*!
    \qmlproperty url SceneLoader::source

    Holds the url to the source to be loaded.
 */

/*!
    \qmlproperty enumeration SceneLoader::status

    Holds the status of scene loading.
    \list
    \li SceneLoader.None
    \li SceneLoader.Loading
    \li SceneLoader.Ready
    \li SceneLoader.Error
    \endlist
    \sa Qt3DRender::QSceneLoader::Status
    \readonly
 */

/*!
    \property QSceneLoader::source

    Holds the url to the source to be loaded.
 */

/*!
    \property QSceneLoader::status

    Holds the status of scene loading.
    \list
    \li SceneLoader.None
    \li SceneLoader.Loading
    \li SceneLoader.Ready
    \li SceneLoader.Error
    \endlist
    \sa Qt3DRender::QSceneLoader::Status
 */

/*! \internal */
QSceneLoaderPrivate::QSceneLoaderPrivate()
    : QComponentPrivate()
    , m_status(QSceneLoader::None)
    , m_subTreeRoot(nullptr)
{
    m_shareable = false;
}

void QSceneLoaderPrivate::populateEntityMap(QEntity *parentEntity)
{
    // Topmost parent entity is not considered part of the scene as that is typically
    // an unnamed entity inserted by importer.
    const QNodeVector childNodes = parentEntity->childNodes();
    for (auto childNode : childNodes) {
        auto childEntity = qobject_cast<QEntity *>(childNode);
        if (childEntity) {
            m_entityMap.insert(childEntity->objectName(), childEntity);
            populateEntityMap(childEntity);
        }
    }
}

void QSceneLoaderPrivate::setStatus(QSceneLoader::Status status)
{
    if (m_status != status) {
        Q_Q(QSceneLoader);
        m_status = status;
        const bool wasBlocked = q->blockNotifications(true);
        emit q->statusChanged(status);
        q->blockNotifications(wasBlocked);
    }
}

/*!
    The constructor creates an instance with the specified \a parent.
 */
QSceneLoader::QSceneLoader(QNode *parent)
    : Qt3DCore::QComponent(*new QSceneLoaderPrivate, parent)
{
}

/*! \internal */
QSceneLoader::~QSceneLoader()
{
}

/*! \internal */
QSceneLoader::QSceneLoader(QSceneLoaderPrivate &dd, QNode *parent)
    : Qt3DCore::QComponent(dd, parent)
{
}

// Called in main thread
/*! \internal */
void QSceneLoader::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &change)
{
    Q_D(QSceneLoader);
    QPropertyUpdatedChangePtr e = qSharedPointerCast<QPropertyUpdatedChange>(change);
    if (e->type() == PropertyUpdated) {
        if (e->propertyName() == QByteArrayLiteral("scene")) {
            // If we already have a scene sub tree, delete it
            if (d->m_subTreeRoot) {
                delete d->m_subTreeRoot;
                d->m_subTreeRoot = nullptr;
            }

            // If we have successfully loaded a scene, graft it in
            auto *subTreeRoot = e->value().value<Qt3DCore::QEntity *>();
            if (subTreeRoot) {
                // Get the entity to which this component is attached
                const Qt3DCore::QNodeIdVector entities = d->m_scene->entitiesForComponent(d->m_id);
                Q_ASSERT(entities.size() == 1);
                Qt3DCore::QNodeId parentEntityId = entities.first();
                QEntity *parentEntity = qobject_cast<QEntity *>(d->m_scene->lookupNode(parentEntityId));
                subTreeRoot->setParent(parentEntity);
                d->m_subTreeRoot = subTreeRoot;
                d->populateEntityMap(d->m_subTreeRoot);
            }
        } else if (e->propertyName() == QByteArrayLiteral("status")) {
            d->setStatus(e->value().value<QSceneLoader::Status>());
        }
    }
}

QUrl QSceneLoader::source() const
{
    Q_D(const QSceneLoader);
    return d->m_source;
}

void QSceneLoader::setSource(const QUrl &arg)
{
    Q_D(QSceneLoader);
    if (d->m_source != arg) {
        d->m_entityMap.clear();
        d->m_source = arg;
        emit sourceChanged(arg);
    }
}

QSceneLoader::Status QSceneLoader::status() const
{
    Q_D(const QSceneLoader);
    return d->m_status;
}

/*!
    \qmlmethod Entity SceneLoader::entity(string entityName)
    Returns a loaded entity with the \c objectName matching the \a entityName parameter.
    If multiple entities have the same name, it is undefined which one of them is returned, but it
    will always be the same one.
*/
/*!
    Returns a loaded entity with an \c objectName matching the \a entityName parameter.
    If multiple entities have the same name, it is undefined which one of them is returned, but it
    will always be the same one.
*/
QEntity *QSceneLoader::entity(const QString &entityName) const
{
    Q_D(const QSceneLoader);
    return d->m_entityMap.value(entityName);
}

/*!
    \qmlmethod list SceneLoader::entityNames()
    Returns a list of the \c objectNames of the loaded entities.
*/
/*!
    Returns a list of the \c objectNames of the loaded entities.
*/
QStringList QSceneLoader::entityNames() const
{
    Q_D(const QSceneLoader);
    return d->m_entityMap.keys();
}

/*!
    \qmlmethod Entity SceneLoader::component(string entityName, enumeration componentType)
    Returns a component matching \a componentType of a loaded entity with an \a objectName matching
    the \a entityName.
    If the entity has multiple matching components, the first match in the component list of
    the entity is returned.
    If there is no match, an undefined item is returned.
    \list
    \li SceneLoader.UnknownComponent Unknown component type
    \li SceneLoader.GeometryRendererComponent Qt3DRender::QGeometryRenderer component
    \li SceneLoader.TransformComponent Qt3DCore::QTransform component
    \li SceneLoader.MaterialComponent Qt3DRender::QMaterial component
    \li SceneLoader.LightComponent Qt3DRender::QAbstractLight component
    \li SceneLoader.CameraLensComponent Qt3DRender::QCameraLens component
    \endlist
    \sa Qt3DRender::QSceneLoader::ComponentType
*/
/*!
    Returns a component matching \a componentType of a loaded entity with an objectName matching
    the \a entityName.
    If the entity has multiple matching components, the first match in the component list of
    the entity is returned.
    If there is no match, a null pointer is returned.
*/
QComponent *QSceneLoader::component(const QString &entityName,
                                    QSceneLoader::ComponentType componentType) const
{
    QEntity *e = entity(entityName);
    const QComponentVector components = e->components();
    for (auto component : components) {
        switch (componentType) {
        case GeometryRendererComponent:
            if (qobject_cast<Qt3DRender::QGeometryRenderer *>(component))
                return component;
            break;
        case TransformComponent:
            if (qobject_cast<Qt3DCore::QTransform *>(component))
                return component;
            break;
        case MaterialComponent:
            if (qobject_cast<Qt3DRender::QMaterial *>(component))
                return component;
            break;
        case LightComponent:
            if (qobject_cast<Qt3DRender::QAbstractLight *>(component))
                return component;
            break;
        case CameraLensComponent:
            if (qobject_cast<Qt3DRender::QCameraLens *>(component))
                return component;
            break;
        default:
            break;
        }
    }
    return nullptr;
}

/*! \internal */
void QSceneLoader::setStatus(QSceneLoader::Status status)
{
    Q_D(QSceneLoader);
    d->setStatus(status);
}

Qt3DCore::QNodeCreatedChangeBasePtr QSceneLoader::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QSceneLoaderData>::create(this);
    auto &data = creationChange->data;
    data.source = d_func()->m_source;
    return creationChange;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
