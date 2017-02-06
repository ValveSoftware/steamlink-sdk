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

#include "qcomponent.h"
#include "qcomponent_p.h"
#include "qentity.h"
#include "qentity_p.h"
#include "qscene_p.h"
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

QComponentPrivate::QComponentPrivate()
    : QNodePrivate()
    , m_shareable(true)
{
}

QComponentPrivate::~QComponentPrivate()
{
}

void QComponentPrivate::addEntity(QEntity *entity)
{
    m_entities.append(entity);

    if (m_scene != nullptr && !m_scene->hasEntityForComponent(m_id, entity->id())) {
        if (!m_shareable && !m_scene->entitiesForComponent(m_id).isEmpty())
            qWarning() << "Trying to assign a non shareable component to more than one Entity";
        m_scene->addEntityForComponent(m_id, entity->id());
    }

    // TODO: Add QAddedToEntityChange to be delivered to components on the backend
}

void QComponentPrivate::removeEntity(QEntity *entity)
{
    // TODO: Add QRemovedFromEntityChange to be delivered to components on the backend

    if (m_scene != nullptr)
        m_scene->removeEntityForComponent(m_id, entity->id());

    m_entities.removeAll(entity);
}

/*!
    \class Qt3DCore::QComponent
    \inmodule Qt3DCore
    \inherits Qt3DCore::QNode
    \since 5.5

    \brief The base class of scene nodes that can be aggregated by Qt3DCore::QEntity
    instances as a component.

    A Qt3DCore::QComponent provides a vertical slice of behavior that can be assigned to and
    sometimes shared across Qt3DCore::QEntity instances.

    Qt3DCore::QComponent subclasses are often aggregated in groups that impart useful
    behavior to the aggregating entity. For example, to have an Entity that gets
    drawn by the Qt3D renderer aspect, an entity would most likely aggregate
    Qt3DCore::QTransform, Qt3DRender::QMesh, and Qt3DRender::QMaterial components.

    \sa Qt3DCore::QEntity
*/

/*!
    Constructs a new QComponent instance with \a parent as the parent.
    \note a QComponent should never be instanced directly,
    instance one of the subclasses instead.
*/
QComponent::QComponent(QNode *parent)
    : QComponent(*new QComponentPrivate, parent) {}

QComponent::~QComponent()
{
    Q_D(QComponent);

    for (QEntity *entity : qAsConst(d->m_entities)) {
        QEntityPrivate *entityPimpl = static_cast<QEntityPrivate *>(QEntityPrivate::get(entity));
        if (entityPimpl)
            entityPimpl->m_components.removeAll(this);
    }
}

/*!
    \property Qt3DCore::QComponent::isShareable
    Holds the shareable flag of the QComponent. The QComponent can be shared across several
    entities if \c{true}.
*/
bool QComponent::isShareable() const
{
    Q_D(const QComponent);
    return d->m_shareable;
}

void QComponent::setShareable(bool shareable)
{
    Q_D(QComponent);
    if (d->m_shareable != shareable) {
        d->m_shareable = shareable;
        emit shareableChanged(shareable);
    }
}

/*!
    Returns a QVector containing all the entities that reference this component.
*/
QVector<QEntity *> QComponent::entities() const
{
    Q_D(const QComponent);
    return d->m_entities;
}

/*! \internal */
QComponent::QComponent(QComponentPrivate &dd, QNode *parent)
    : QNode(dd, parent)
{
}

} // namespace Qt3DCore

/*!
    \qmltype Component3D
    \instantiates Qt3DCore::QComponent
    \inqmlmodule Qt3D.Core
    \inherits Node
    \since 5.5
    \brief Provides the base type for creating Qt 3D components.

    \TODO
*/

/*!
    \qmlproperty bool Component3D::isShareable
*/

QT_END_NAMESPACE
