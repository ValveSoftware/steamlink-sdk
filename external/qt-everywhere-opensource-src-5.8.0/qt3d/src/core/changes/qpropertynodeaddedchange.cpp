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

#include "qpropertynodeaddedchange.h"
#include "qpropertynodeaddedchange_p.h"
#include <Qt3DCore/private/qnode_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

QPropertyNodeAddedChangePrivate::QPropertyNodeAddedChangePrivate()
    : QStaticPropertyValueAddedChangeBasePrivate()
    , m_addedNodeIdTypePair()
{
}

/*!
 * \class Qt3DCore::QPropertyNodeAddedChange
 * \inherits Qt3DCore::QStaticPropertyValueAddedChangeBase
 * \inmodule Qt3DCore
 * \brief Used to notify when a node is added to a property
 *
 */

/*!
 * \typedef Qt3DCore::QPropertyNodeAddedChangePtr
 * \relates Qt3DCore::QPropertyNodeAddedChange
 *
 * A shared pointer for QPropertyNodeAddedChange.
 */

/*!
 * Constructs a new QPropertyNodeAddedChange with \a subjectId, \a node.
 */
QPropertyNodeAddedChange::QPropertyNodeAddedChange(QNodeId subjectId, QNode *node)
    : QStaticPropertyValueAddedChangeBase(*new QPropertyNodeAddedChangePrivate, subjectId)
{
    Q_D(QPropertyNodeAddedChange);
    d->m_addedNodeIdTypePair = QNodeIdTypePair(node->id(), QNodePrivate::findStaticMetaObject(node->metaObject()));
}

/*! \internal */
QPropertyNodeAddedChange::~QPropertyNodeAddedChange()
{
}

/*!
 * \return the id of the node added to the property.
 */
QNodeId QPropertyNodeAddedChange::addedNodeId() const
{
    Q_D(const QPropertyNodeAddedChange);
    return d->m_addedNodeIdTypePair.id;
}

/*!
 * \return the meta object of the node added to the property.
 */
const QMetaObject *QPropertyNodeAddedChange::metaObject() const
{
    Q_D(const QPropertyNodeAddedChange);
    return d->m_addedNodeIdTypePair.type;
}

} // namespace Qt3DCore

QT_END_NAMESPACE
