/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qscene2d.h"
#include "qscene2d_p.h"
#include "scene2d_p.h"
#include "scene2dmanager_p.h"
#include "scene2devent_p.h"

#include <Qt3DCore/qentity.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {

namespace Quick {

/*!
    \class Qt3DRender::Quick::QScene2D
    \inmodule Qt3DScene2D

    \brief This class enables rendering qml into a texture, which then can be
    used as a part of 3D scene.

    This class uses QQuickRenderControl to render the given QQuickItem into an
    offscreen surface, which is attached to a texture provided by the user. This allows the
    component to directly render into the texture without intermediate copy and the user to
    freely specify how the texture is used in the 3D scene.

    The entities using the QScene2D can be associated with the class to enable interaction
    with the item; if an entity has a QObjectPicker component, the pick events from that picker
    are sent to the QScene2D and converted to mouse events and finally sent to the item.

    \since 5.9
*/

/*!
    \qmltype Scene2D
    \inqmlmodule Qt3D.Scene2D
    \since 5.9
    \instantiates Qt3DRender::Quick::QScene2D

    \brief This type enables rendering qml into a texture, which then can be
    used as a part of 3D scene.

    This object uses RenderControl to render the given Item into an
    offscreen surface, which is attached to a texture provided by the user. This allows the
    component to directly render into the texture without intermediate copy and the user to
    freely specify how the texture is used in the 3D scene.

    The entities using the Scene2D can be associated with the type to enable interaction
    with the item; if an entity has an ObjectPicker component, the pick events from that picker
    are sent to the Scene2D and converted to mouse events and finally sent to the item.

    Usage:
    \qml
        Entity {
            id: sceneRoot

            // specify Scene2D inside the entity hierarchy
            Scene2D {
                // specify output
                output: RenderTargetOutput {
                    attachmentPoint: RenderTargetOutput.Color0
                    texture: Texture2D {
                        id: textureId
                        width: 1024
                        height: 1024
                        format: Texture.RGBA8_UNorm
                    }
                }
                // specify entities
                entities: [entityId]

                // specify rendered content
                Rectangle {
                    color: "red"
                }
            }

            Entity {
                id: entityId

                property Material material: TextureMaterial {
                    texture: textureId
                }
                property ObjectPicker picker: ObjectPicker {
                    hoverEnabled: true
                    dragEnabled: true
                }
                ...

    \endqml
 */

/*!
    \enum QScene2D::RenderPolicy

    This enum type describes types of render policies available.
    \value Continuous The Scene2D is rendering continuously. This is the default render policy.
    \value SingleShot The Scene2D renders to the texture only once after which the resources
                      allocated for rendering are released.
*/

/*!
    \qmlproperty RenderTargetOutput Qt3D.Render::Scene2D::output
    Holds the RenderTargetOutput, which specifies where the Scene2D is rendering to.
 */

/*!
    \qmlproperty enumeration Qt3D.Render::Scene2D::renderPolicy
    Holds the render policy of this Scene2D.

    \list
    \li Continuous The Scene2D is rendering continuously. This is the default render policy.
    \li SingleShot The Scene2D renders to the texture only once after which the resources
                      allocated for rendering are released.
    \endlist
 */
/*!
    \qmlproperty Item Qt3D.Render::Scene2D::item
    Holds the Item, which is rendered by Scene2D to the texture.
 */

/*!
    \qmlproperty bool Qt3D.Render::Scene2D::mouseEnabled
    Holds whether mouse events are enabled for the rendered item. The mouse events are
    generated from object picking events of the entities added to the Scene2D.
    Mouse is enabled by default.

    \note Events sent to items are delayed by one frame due to object picking
          happening in the backend.
 */
/*!
    \qmlproperty list<Entity> Qt3D.Render::Scene2D::entities
    Holds the list of entities which are associated with the Scene2D object. If the
    entities have ObjectPicker, the pick events from that entity are sent to Scene2D
    and converted to mouse events.
 */

QScene2DPrivate::QScene2DPrivate()
    : Qt3DCore::QNodePrivate()
    , m_renderManager(new Scene2DManager(this))
    , m_output(nullptr)
{
}

QScene2DPrivate::~QScene2DPrivate()
{
    m_renderManager->cleanup();
    delete m_renderManager;
}

void QScene2DPrivate::setScene(Qt3DCore::QScene *scene)
{
    Q_Q(QScene2D);
    QNodePrivate::setScene(scene);
    const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(q->id());
    change->setPropertyName("sceneInitialized");
    notifyObservers(change);
}


/*!
    The constructor creates a new QScene2D instance with the specified \a parent.
 */
QScene2D::QScene2D(Qt3DCore::QNode *parent)
    : Qt3DCore::QNode(*new QScene2DPrivate, parent)
{
}

/*!
    \property QScene2D::item
    Holds the QQuickItem, which is rendered by QScene2D to the texture.
 */
QQuickItem* QScene2D::item() const
{
    Q_D(const QScene2D);
    return d->m_renderManager->m_item;
}

void QScene2D::setItem(QQuickItem *item)
{
    Q_D(QScene2D);
    if (d->m_renderManager->m_initialized) {
        qWarning() << "Unable to set item after initialization.";
        return;
    }
    if (d->m_renderManager->m_item != item) {
        d->m_renderManager->setItem(item);
        emit itemChanged(item);
    }
}

/*!
    \property QScene2D::renderPolicy

    Holds the render policy of this Scene2D.
 */
QScene2D::RenderPolicy QScene2D::renderPolicy() const
{
    Q_D(const QScene2D);
    return d->m_renderManager->m_renderPolicy;
}

void QScene2D::setRenderPolicy(QScene2D::RenderPolicy renderPolicy)
{
    Q_D(const QScene2D);
    if (d->m_renderManager->m_renderPolicy != renderPolicy) {
        d->m_renderManager->m_renderPolicy = renderPolicy;
        emit renderPolicyChanged(renderPolicy);
    }
}

/*!
    \property QScene2D::output
    Holds the QRenderTargetOutput, which specifies where the QScene2D is
    rendering to.
 */
Qt3DRender::QRenderTargetOutput *QScene2D::output() const
{
    Q_D(const QScene2D);
    return d->m_output;
}

void QScene2D::setOutput(Qt3DRender::QRenderTargetOutput *output)
{
    Q_D(QScene2D);
    if (d->m_output != output) {
        if (d->m_output)
            d->unregisterDestructionHelper(d->m_output);
        d->m_output = output;
        if (output)
            d->registerDestructionHelper(output, &QScene2D::setOutput, d->m_output);
        emit outputChanged(output);
    }
}

Qt3DCore::QNodeCreatedChangeBasePtr QScene2D::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QScene2DData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QScene2D);
    data.renderPolicy = d->m_renderManager->m_renderPolicy;
    data.sharedObject = d->m_renderManager->m_sharedObject;
    data.output = d->m_output ? d->m_output->id() : Qt3DCore::QNodeId();
    for (Qt3DCore::QEntity *e : d->m_entities)
        data.entityIds.append(e->id());
    data.mouseEnabled = d->m_renderManager->m_mouseEnabled;
    return creationChange;
}

bool QScene2D::isMouseEnabled() const
{
    Q_D(const QScene2D);
    return d->m_renderManager->m_mouseEnabled;
}

/*!
    Retrieve entities associated with the QScene2D.
 */
QVector<Qt3DCore::QEntity*> QScene2D::entities()
{
    Q_D(const QScene2D);
    return d->m_entities;
}

/*!
    Adds an \a entity to the the QScene2D object. If the entities have QObjectPicker,
    the pick events from that entity are sent to QScene2D and converted to mouse events.
*/
void QScene2D::addEntity(Qt3DCore::QEntity *entity)
{
    Q_D(QScene2D);
    if (!d->m_entities.contains(entity)) {
        d->m_entities.append(entity);

        d->registerDestructionHelper(entity, &QScene2D::removeEntity, d->m_entities);

        if (d->m_changeArbiter != nullptr) {
            const auto change = Qt3DCore::QPropertyNodeAddedChangePtr::create(id(), entity);
            change->setPropertyName("entities");
            d->notifyObservers(change);
        }
    }
}

/*!
    Removes an \a entity from the the QScene2D object.
*/
void QScene2D::removeEntity(Qt3DCore::QEntity *entity)
{
    Q_D(QScene2D);
    if (d->m_entities.contains(entity)) {
        d->m_entities.removeAll(entity);

        d->unregisterDestructionHelper(entity);

        if (d->m_changeArbiter != nullptr) {
            const auto change = Qt3DCore::QPropertyNodeRemovedChangePtr::create(id(), entity);
            change->setPropertyName("entities");
            d->notifyObservers(change);
        }
    }
}

/*!
    \property QScene2D::mouseEnabled
    Holds whether mouse events are enabled for the rendered item. The mouse events are
    generated from object picking events of the entities added to the QScene2D.
    Mouse is enabled by default.

    \note Events are delayed by one frame due to object picking happening in the backend.
 */
void QScene2D::setMouseEnabled(bool enabled)
{
    Q_D(QScene2D);
    if (d->m_renderManager->m_mouseEnabled != enabled) {
        d->m_renderManager->m_mouseEnabled = enabled;
        emit mouseEnabledChanged(enabled);
    }
}


} // namespace Quick
} // namespace Qt3DRender

QT_END_NAMESPACE
