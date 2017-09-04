/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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

#include "qlevelofdetailswitch.h"
#include "qlevelofdetail_p.h"
#include "qglobal.h"
#include <Qt3DCore/QEntity>
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
    \class Qt3DRender::QLevelOfDetailSwitch
    \inmodule Qt3DRender
    \inherits Qt3DRender::QLevelOfDetail
    \since 5.9
    \brief Provides a way of enabling child entities based on distance or screen size.

    This component is assigned to an entity. When the entity changes distance relative
    to the camera, the QLevelOfDetailSwitch will disable all the child entities except
    the one matching index Qt3DRender::QLevelOfDetailSwitch::currentIndex.
*/

/*!
    \qmltype LevelOfDetailSwitch
    \instantiates Qt3DRender::QLevelOfDetailSwitch
    \inherits Component3D
    \inqmlmodule Qt3D.Render
    \since 5.9
    \brief Provides a way of enabling child entities based on distance or screen size.

    This component is assigned to an entity. When the entity changes distance relative
    to the camera, the LevelOfDetailSwitch will disable all the child entities except
    the one matching index \l currentIndex.

    \sa LevelOfDetail
*/

/*!
    \qmlproperty int LevelOfDetailSwitch::currentIndex

    The index of the presently selected child entity.
*/

/*! \fn Qt3DRender::QLevelOfDetailSwitch::QLevelOfDetailSwitch(Qt3DCore::QNode *parent)
  Constructs a new QLevelOfDetailSwitch with the specified \a parent.
 */
QLevelOfDetailSwitch::QLevelOfDetailSwitch(QNode *parent)
    : QLevelOfDetail(parent)
{
    Q_D(QLevelOfDetail);
    d->m_currentIndex = -1;
}

/*! \internal */
QLevelOfDetailSwitch::~QLevelOfDetailSwitch()
{
}

/*! \internal */
QLevelOfDetailSwitch::QLevelOfDetailSwitch(QLevelOfDetailPrivate &dd, QNode *parent)
    : QLevelOfDetail(dd, parent)
{
}

/*! \internal */
void QLevelOfDetailSwitch::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &change)
{
    Q_D(QLevelOfDetail);
    Qt3DCore::QPropertyUpdatedChangePtr e = qSharedPointerCast<Qt3DCore::QPropertyUpdatedChange>(change);
    if (e->type() == Qt3DCore::PropertyUpdated) {
        if (e->propertyName() == QByteArrayLiteral("currentIndex")) {
            int ndx = e->value().value<int>();
            d->m_currentIndex = ndx;
            emit currentIndexChanged(ndx);

            int entityIndex = 0;
            const auto entities = this->entities();
            for (Qt3DCore::QEntity *entity : entities) {
                const auto childNodes = entity->childNodes();
                for (Qt3DCore::QNode *childNode : childNodes) {
                    Qt3DCore::QEntity *childEntity = qobject_cast<Qt3DCore::QEntity *>(childNode);
                    if (childEntity) {
                        childEntity->setEnabled(entityIndex == ndx);
                        entityIndex++;
                    }
                }
                break; // only work on the first entity, LOD should not be shared
            }
        }
    }
}

} // namespace Qt3DRender

QT_END_NAMESPACE
