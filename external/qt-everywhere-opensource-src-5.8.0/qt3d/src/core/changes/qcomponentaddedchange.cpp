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

#include "qcomponentaddedchange.h"
#include "qcomponentaddedchange_p.h"
#include <Qt3DCore/qcomponent.h>
#include <Qt3DCore/qentity.h>
#include <private/qnode_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

QComponentAddedChangePrivate::QComponentAddedChangePrivate(const QEntity *entity,
                                                           const QComponent *component)
    : QSceneChangePrivate()
    , m_entityId(entity->id())
    , m_componentId(component->id())
    , m_componentMetaObject(QNodePrivate::findStaticMetaObject(component->metaObject()))
{
}

/*!
 * \class Qt3DCore::QComponentAddedChange
 * \inherits Qt3DCore::QSceneChange
 * \inmodule Qt3DCore
 * \brief The QComponentAddedChange class is used to notify when a component is added to an entity
 *
 */

/*!
 * \typedef Qt3DCore::QComponentAddedChangePtr
 * \relates Qt3DCore::QComponentAddedChange
 *
 * A shared pointer for QComponentAddedChange.
 */

/*!
 * Constructs a new QComponentAddedChange with with \a entity and  \a component.
 */
QComponentAddedChange::QComponentAddedChange(const QEntity *entity,
                                             const QComponent *component)
    : QSceneChange(*new QComponentAddedChangePrivate(entity, component),
                   ComponentAdded, entity->id())
{
}

QComponentAddedChange::~QComponentAddedChange()
{
}

/*!
  \return the id of the entity the component was added to.
 */
QNodeId QComponentAddedChange::entityId() const Q_DECL_NOTHROW
{
    Q_D(const QComponentAddedChange);
    return d->m_entityId;
}

/*!
  \return the id of the component added.
 */
QNodeId QComponentAddedChange::componentId() const Q_DECL_NOTHROW
{
    Q_D(const QComponentAddedChange);
    return d->m_componentId;
}

/*!
 * \return the metaobject.
 */
const QMetaObject *QComponentAddedChange::componentMetaObject() const Q_DECL_NOTHROW
{
    Q_D(const QComponentAddedChange);
    return d->m_componentMetaObject;
}

QT_END_NAMESPACE

} // namespace Qt3DCore

