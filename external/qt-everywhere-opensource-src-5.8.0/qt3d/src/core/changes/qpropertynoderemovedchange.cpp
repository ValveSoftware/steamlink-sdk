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

#include "qpropertynoderemovedchange.h"
#include "qpropertynoderemovedchange_p.h"
#include <Qt3DCore/private/qnode_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

QPropertyNodeRemovedChangePrivate::QPropertyNodeRemovedChangePrivate()
    : QStaticPropertyValueRemovedChangeBasePrivate()
    , m_propertyName(nullptr)
    , m_removedNodeIdTypePair()
{
}

/*!
 * \class Qt3DCore::QPropertyNodeRemovedChange
 * \inherits Qt3DCore::QStaticPropertyValueRemovedChangeBase
 * \inmodule Qt3DCore
 * \brief The QPropertyNodeRemovedChange class is used to notify when a node is removed from a property
 *
 */


/*!
 * \typedef Qt3DCore::QPropertyNodeRemovedChangePtr
 * \relates Qt3DCore::QPropertyNodeRemovedChange
 *
 * A shared pointer for QPropertyNodeRemovedChange.
 */

/*!
 * Constructs a new QPropertyNodeRemovedChange with \a subjectId, \a node.
 */
QPropertyNodeRemovedChange::QPropertyNodeRemovedChange(QNodeId subjectId, QNode *node)
    : QStaticPropertyValueRemovedChangeBase(*new QPropertyNodeRemovedChangePrivate, subjectId)
{
    Q_D(QPropertyNodeRemovedChange);
    d->m_removedNodeIdTypePair = QNodeIdTypePair(node->id(), QNodePrivate::get(node)->m_typeInfo);
}

/*! \internal */
QPropertyNodeRemovedChange::~QPropertyNodeRemovedChange()
{
}

/*!
 * \return the id of the node removed to the property.
 */
QNodeId QPropertyNodeRemovedChange::removedNodeId() const
{
    Q_D(const QPropertyNodeRemovedChange);
    return d->m_removedNodeIdTypePair.id;
}

/*!
 * \return the metaObject of the node removed to the property.
 */
const QMetaObject *QPropertyNodeRemovedChange::metaObject() const
{
    Q_D(const QPropertyNodeRemovedChange);
    return d->m_removedNodeIdTypePair.type;
}

} // namespace Qt3DCore

QT_END_NAMESPACE
