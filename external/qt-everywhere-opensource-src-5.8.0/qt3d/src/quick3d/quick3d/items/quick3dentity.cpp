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

#include "quick3dentity_p.h"
#include <Qt3DCore/qcomponent.h>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {
namespace Quick {

/*!
    \qmltype Entity
    \instantiates Qt3DCore::QEntity
    \inherits Node
    \inqmlmodule Qt3D.Core
    \since 5.5

    \brief Entity is a \l Node subclass that can aggregate several
    \l Component3D instances that will specify its behavior.

    By itself a Entity is an empty shell. The behavior of a Entity
    object is defined by the \l Component3D objects it references. Each Qt3D
    backend aspect will be able to interpret and process an Entity by
    recognizing which components it is made up of. One aspect may decide to only
    process entities composed of a single \l Transform component whilst
    another may focus on \l MouseHandler.

    \sa Qt3D.Core::Component3D, Qt3D.Core::Transform
 */

/*!
    \qmlproperty list<Component3D> Entity::components
    Holds the list of \l Component3D instances, which define the behavior
    of the entity.
    \readonly
 */

Quick3DEntity::Quick3DEntity(QObject *parent)
    : QObject(parent)
{
}


QQmlListProperty<QComponent> Quick3DEntity::componentList()
{
    return QQmlListProperty<Qt3DCore::QComponent>(this, 0,
                                             Quick3DEntity::qmlAppendComponent,
                                             Quick3DEntity::qmlComponentsCount,
                                             Quick3DEntity::qmlComponentAt,
                                             Quick3DEntity::qmlClearComponents);
}

void Quick3DEntity::qmlAppendComponent(QQmlListProperty<QComponent> *list, QComponent *comp)
{
    if (comp == nullptr)
        return;
    Quick3DEntity *self = static_cast<Quick3DEntity *>(list->object);
    self->parentEntity()->addComponent(comp);
}

QComponent *Quick3DEntity::qmlComponentAt(QQmlListProperty<QComponent> *list, int index)
{
    Quick3DEntity *self = static_cast<Quick3DEntity *>(list->object);
    return self->parentEntity()->components().at(index);
}

int Quick3DEntity::qmlComponentsCount(QQmlListProperty<QComponent> *list)
{
    Quick3DEntity *self = static_cast<Quick3DEntity *>(list->object);
    return self->parentEntity()->components().count();
}

void Quick3DEntity::qmlClearComponents(QQmlListProperty<QComponent> *list)
{
    Quick3DEntity *self = static_cast<Quick3DEntity *>(list->object);
    const QComponentVector components = self->parentEntity()->components();
    for (QComponent *comp : components) {
        self->parentEntity()->removeComponent(comp);
    }
}

} // namespace Quick
} // namespace Qt3DCore

QT_END_NAMESPACE
