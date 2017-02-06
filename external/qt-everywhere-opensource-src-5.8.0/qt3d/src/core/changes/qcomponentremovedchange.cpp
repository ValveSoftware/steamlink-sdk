/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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

#include "qcomponentremovedchange.h"
#include "qcomponentremovedchange_p.h"
#include <Qt3DCore/qcomponent.h>
#include <Qt3DCore/qentity.h>
#include <private/qnode_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

QComponentRemovedChangePrivate::QComponentRemovedChangePrivate(const QEntity *entity,
                                                               const QComponent *component)
    : QSceneChangePrivate()
    , m_entityId(entity->id())
    , m_componentId(component->id())
    , m_componentMetaObject(QNodePrivate::findStaticMetaObject(component->metaObject()))
{
}

/*!
 * \class Qt3DCore::QComponentRemovedChange
 * \inherits Qt3DCore::QSceneChange
 * \inmodule Qt3DCore
 * \brief The QComponentRemovedChange class is used to notify when a component is removed from an entity
 *
 */

/*!
 * \typedef Qt3DCore::QComponentRemovedChangePtr
 * \relates Qt3DCore::QComponentRemovedChange
 *
 * A shared pointer for QComponentRemovedChange.
 */

/*!
 * Constructs a new QComponentRemovedChange with \a entity and  \a component.
 */
QComponentRemovedChange::QComponentRemovedChange(const QEntity *entity,
                                                 const QComponent *component)
    : QSceneChange(*new QComponentRemovedChangePrivate(entity, component),
                   ComponentRemoved, entity->id())
{
}

QComponentRemovedChange::~QComponentRemovedChange()
{
}

/*!
  \return the id of the entity the component was removed from.
 */
QNodeId QComponentRemovedChange::entityId() const Q_DECL_NOTHROW
{
    Q_D(const QComponentRemovedChange);
    return d->m_entityId;
}

/*!
  \return the id of the component removed.
 */
QNodeId QComponentRemovedChange::componentId() const Q_DECL_NOTHROW
{
    Q_D(const QComponentRemovedChange);
    return d->m_componentId;
}

/*!
 * \return the metaobject.
 */
const QMetaObject *QComponentRemovedChange::componentMetaObject() const Q_DECL_NOTHROW
{
    Q_D(const QComponentRemovedChange);
    return d->m_componentMetaObject;
}

QT_END_NAMESPACE

} // namespace Qt3DCore

