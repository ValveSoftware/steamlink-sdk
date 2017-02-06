/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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

#include "qgeometry.h"
#include "qgeometryfactory.h"
#include "qgeometry_p.h"
#include <private/qnode_p.h>
#include <Qt3DRender/qattribute.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {

QGeometryFactory::~QGeometryFactory()
{
}

QGeometryPrivate::QGeometryPrivate()
    : QNodePrivate(),
      m_boundingVolumePositionAttribute(nullptr)
{
}

QGeometryPrivate::~QGeometryPrivate()
{
}

/*!
 * \qmltype Geometry
 * \instantiates Qt3DRender::QGeometry
 * \inqmlmodule Qt3D.Render
 */

/*!
 * \qmlproperty QAttribute Geometry::boundingVolumePositionAttribute
 *
 * Holds the property used to compute the bounding volume.
 */

/*!
 * \class Qt3DRender::QGeometry
 * \inmodule Qt3DRender
 *
 * \inherits Qt3DCore::QNode
 *
 */

/*!
 * \typedef Qt3DRender::QAttributeList
 * \relates Qt3DRender::QGeometry
 *
 * A vector of {QAttribute}s.
 */

/*!
 * Constructs a new QGeometry with \a parent.
 */
QGeometry::QGeometry(QNode *parent)
    : QGeometry(*new QGeometryPrivate(), parent) {}

/*!
 * \internal
 */
QGeometry::~QGeometry()
{
}

/*!
 * \internal
 */
QGeometry::QGeometry(QGeometryPrivate &dd, QNode *parent)
    : QNode(dd, parent)
{
}

/*!
 * Adds an \a attribute to this geometry.
 */
void QGeometry::addAttribute(QAttribute *attribute)
{
    Q_ASSERT(attribute);
    Q_D(QGeometry);
    if (!d->m_attributes.contains(attribute)) {
        d->m_attributes.append(attribute);

        // Ensures proper bookkeeping
        d->registerDestructionHelper(attribute, &QGeometry::removeAttribute, d->m_attributes);

        // We need to add it as a child of the current node if it has been declared inline
        // Or not previously added as a child of the current node so that
        // 1) The backend gets notified about it's creation
        // 2) When the current node is destroyed, it gets destroyed as well
        if (!attribute->parent())
            attribute->setParent(this);

        if (d->m_changeArbiter != nullptr) {
            const auto change = QPropertyNodeAddedChangePtr::create(id(), attribute);
            change->setPropertyName("attribute");
            d->notifyObservers(change);
        }
    }
}

/*!
 * Removes the given \a attribute from this geometry.
 */
void QGeometry::removeAttribute(QAttribute *attribute)
{
    Q_ASSERT(attribute);
    Q_D(QGeometry);
    if (d->m_changeArbiter != nullptr) {
        const auto change = QPropertyNodeRemovedChangePtr::create(id(), attribute);
        change->setPropertyName("attribute");
        d->notifyObservers(change);
    }
    d->m_attributes.removeOne(attribute);
    // Remove bookkeeping connection
    d->unregisterDestructionHelper(attribute);
}

void QGeometry::setBoundingVolumePositionAttribute(QAttribute *boundingVolumePositionAttribute)
{
    Q_D(QGeometry);
    if (d->m_boundingVolumePositionAttribute != boundingVolumePositionAttribute) {
        d->m_boundingVolumePositionAttribute = boundingVolumePositionAttribute;
        emit boundingVolumePositionAttributeChanged(boundingVolumePositionAttribute);
    }
}

/*!
 * \property QGeometry::boundingVolumePositionAttribute
 *
 * Holds atribute used to compute the bounding volume .
 */
QAttribute *QGeometry::boundingVolumePositionAttribute() const
{
    Q_D(const QGeometry);
    return d->m_boundingVolumePositionAttribute;
}

/*!
 * \return the list of attributes in this geometry.
 */
QVector<QAttribute *> QGeometry::attributes() const
{
    Q_D(const QGeometry);
    return d->m_attributes;
}

Qt3DCore::QNodeCreatedChangeBasePtr QGeometry::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QGeometryData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QGeometry);
    data.attributeIds = qIdsForNodes(d->m_attributes);
    data.boundingVolumePositionAttributeId = qIdForNode(d->m_boundingVolumePositionAttribute);
    return creationChange;
}

} // namespace Qt3DRender

QT_END_NAMESPACE

#include "moc_qgeometry.cpp"
