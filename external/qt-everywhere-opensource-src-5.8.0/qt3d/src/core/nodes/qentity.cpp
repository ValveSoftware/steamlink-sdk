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

#include "qentity.h"
#include "qentity_p.h"
#include "qcomponent.h"
#include "qcomponent_p.h"

#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/qcomponentaddedchange.h>
#include <Qt3DCore/qcomponentremovedchange.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include <Qt3DCore/private/corelogging_p.h>
#include <QMetaObject>
#include <QMetaProperty>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

/*!
    \class Qt3DCore::QEntity
    \inmodule Qt3DCore
    \inherits Qt3DCore::QNode
    \since 5.5

    \brief Qt3DCore::QEntity is a Qt3DCore::QNode subclass that can aggregate several
    Qt3DCore::QComponent instances that will specify its behavior.

    By itself a Qt3DCore::QEntity is an empty shell. The behavior of a Qt3DCore::QEntity
    object is defined by the Qt3DCore::QComponent objects it references. Each Qt3D
    backend aspect will be able to interpret and process an Entity by
    recognizing which components it is made up of. One aspect may decide to only
    process entities composed of a single Qt3DCore::QTransform component whilst
    another may focus on Qt3DInput::QMouseHandler.

    \sa Qt3DCore::QComponent, Qt3DCore::QTransform
 */

/*! \internal */
QEntityPrivate::QEntityPrivate()
    : QNodePrivate()
    , m_parentEntityId()
{}

/*! \internal */
QEntityPrivate::~QEntityPrivate()
{
}

/*!
    Constructs a new Qt3DCore::QEntity instance with \a parent as parent.
 */
QEntity::QEntity(QNode *parent)
    : QEntity(*new QEntityPrivate, parent) {}

/*! \internal */
QEntity::QEntity(QEntityPrivate &dd, QNode *parent)
    : QNode(dd, parent)
{
}

QEntity::~QEntity()
{
    // remove all component aggregations
    Q_D(const QEntity);
    // to avoid hammering m_components by repeated removeComponent()
    // calls below, move all contents out, so the removeOne() calls in
    // removeComponent() don't actually remove something:
    const auto components = std::move(d->m_components);
    for (QComponent *comp : components)
        removeComponent(comp);
}


/*!
    \typedef Qt3DCore::QComponentVector
    \relates Qt3DCore::QEntity

    List of QComponent pointers.
 */

/*!
    Returns the list of Qt3DCore::QComponent instances the entity is referencing.
 */
QComponentVector QEntity::components() const
{
    Q_D(const QEntity);
    return d->m_components;
}

/*!
    Adds a new reference to the component \a comp.

    \note If the Qt3DCore::QComponent has no parent, the Qt3DCore::QEntity will set
    itself as its parent thereby taking ownership of the component.
 */
void QEntity::addComponent(QComponent *comp)
{
    Q_D(QEntity);
    Q_CHECK_PTR( comp );
    qCDebug(Nodes) << Q_FUNC_INFO << comp;

    // A Component can only be aggregated once
    if (d->m_components.count(comp) != 0)
        return ;

    // We need to add it as a child of the current node if it has been declared inline
    // Or not previously added as a child of the current node so that
    // 1) The backend gets notified about it's creation
    // 2) When the current node is destroyed, it gets destroyed as well
    if (!comp->parent())
        comp->setParent(this);

    d->m_components.append(comp);

    // Ensures proper bookkeeping
    d->registerDestructionHelper(comp, &QEntity::removeComponent, d->m_components);

    if (d->m_changeArbiter) {
        const auto componentAddedChange = QComponentAddedChangePtr::create(this, comp);
        d->notifyObservers(componentAddedChange);
    }
    static_cast<QComponentPrivate *>(QComponentPrivate::get(comp))->addEntity(this);
}

/*!
    Removes the reference to \a comp.
 */
void QEntity::removeComponent(QComponent *comp)
{
    Q_CHECK_PTR(comp);
    qCDebug(Nodes) << Q_FUNC_INFO << comp;
    Q_D(QEntity);

    static_cast<QComponentPrivate *>(QComponentPrivate::get(comp))->removeEntity(this);

    if (d->m_changeArbiter) {
        const auto componentRemovedChange = QComponentRemovedChangePtr::create(this, comp);
        d->notifyObservers(componentRemovedChange);
    }

    d->m_components.removeOne(comp);

    // Remove bookkeeping connection
    d->unregisterDestructionHelper(comp);
}

/*!
    Returns the parent Qt3DCore::QEntity instance of this entity. If the
    immediate parent isn't a Qt3DCore::QEntity, this function traverses up the
    scene hierarchy until a parent Qt3DCore::QEntity is found. If no
    Qt3DCore::QEntity parent can be found, returns null.
 */
QEntity *QEntity::parentEntity() const
{
    Q_D(const QEntity);
    QNode *parentNode = QNode::parentNode();
    QEntity *parentEntity = qobject_cast<QEntity *>(parentNode);

    while (parentEntity == nullptr && parentNode != nullptr) {
        parentNode = parentNode->parentNode();
        parentEntity = qobject_cast<QEntity*>(parentNode);
    }
    if (!parentEntity) {
        if (!d->m_parentEntityId.isNull())
            d->m_parentEntityId = QNodeId();
    } else {
        if (d->m_parentEntityId != parentEntity->id())
            d->m_parentEntityId = parentEntity->id();
    }
    return parentEntity;
}

/*!
    Returns the Qt3DCore::QNodeId id of the parent Qt3DCore::QEntity instance of the
    current Qt3DCore::QEntity object. The QNodeId isNull method will return true if
    there is no Qt3DCore::QEntity parent of the current Qt3DCore::QEntity in the scene
    hierarchy.
 */
QNodeId QEntityPrivate::parentEntityId() const
{
    Q_Q(const QEntity);
    if (m_parentEntityId.isNull())
        q->parentEntity();
    return m_parentEntityId;
}

QNodeCreatedChangeBasePtr QEntity::createNodeCreationChange() const
{
    auto creationChange = QNodeCreatedChangePtr<QEntityData>::create(this);
    auto &data = creationChange->data;

    Q_D(const QEntity);
    data.parentEntityId = parentEntity() ? parentEntity()->id() : Qt3DCore::QNodeId();
    data.componentIdsAndTypes.reserve(d->m_components.size());
    const QComponentVector &components = d->m_components;
    for (QComponent *c : components) {
        const auto idAndType = QNodeIdTypePair(c->id(), QNodePrivate::findStaticMetaObject(c->metaObject()));
        data.componentIdsAndTypes.push_back(idAndType);
    }

    return creationChange;
}

} // namespace Qt3DCore

QT_END_NAMESPACE
