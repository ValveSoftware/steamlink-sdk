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

#include "qpickingsettings.h"
#include "qpickingsettings_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
    \class Qt3DRender::QPickingSettings
    \brief The QPickingSettings class specifies how entity picking is handled.
    \since 5.7
    \inmodule Qt3DRender
    \inherits Qt3DCore::QNode

    The picking settings determine how the entity picking is handled. For more details about
    entity picking, see QObjectPicker component documentation.

    Picking is triggered by mouse events. It will cast a ray through the scene and look for
    geometry intersecting the ray.

    \sa QObjectPicker, QPickEvent, QPickTriangleEvent
 */

/*!
    \qmltype PickingSettings
    \brief The PickingSettings class specifies how entity picking is handled.
    \since 5.7
    \inqmlmodule Qt3D.Render
    \instantiates Qt3DRender::QPickingSettings

    The picking settings determine how the entity picking is handled. For more details about
    entity picking, see Qt3DRender::QObjectPicker component documentation.

    Picking is triggered by mouse events. It will cast a ray through the scene and look for
    geometry intersecting the ray.

    \sa ObjectPicker
 */

QPickingSettingsPrivate::QPickingSettingsPrivate()
    : Qt3DCore::QNodePrivate()
    , m_pickMethod(QPickingSettings::BoundingVolumePicking)
    , m_pickResultMode(QPickingSettings::NearestPick)
    , m_faceOrientationPickingMode(QPickingSettings::FrontFace)
{
}

QPickingSettings::QPickingSettings(Qt3DCore::QNode *parent)
    : Qt3DCore::QNode(*new QPickingSettingsPrivate, parent)
{
    // Block all notifications for this class as it should have been a QObject
    blockNotifications(true);
}

/*! \internal */
QPickingSettings::~QPickingSettings()
{
}

/*! \internal */
QPickingSettings::QPickingSettings(QPickingSettingsPrivate &dd, Qt3DCore::QNode *parent)
    : Qt3DCore::QNode(dd, parent)
{
}

QPickingSettings::PickMethod QPickingSettings::pickMethod() const
{
    Q_D(const QPickingSettings);
    return d->m_pickMethod;
}

QPickingSettings::PickResultMode QPickingSettings::pickResultMode() const
{
    Q_D(const QPickingSettings);
    return d->m_pickResultMode;
}

QPickingSettings::FaceOrientationPickingMode QPickingSettings::faceOrientationPickingMode() const
{
    Q_D(const QPickingSettings);
    return d->m_faceOrientationPickingMode;
}

/*!
 * \enum Qt3DRender::QPickingSettings::PickMethod
 *
 * Specifies the picking method.
 *
 * \value BoundingVolumePicking An entity is considered picked if the picking ray intersects
 * the bounding volume of the entity (default).
 * \value TrianglePicking An entity is considered picked if the picking ray intersects with
 * any triangle of the entity's mesh component.
 */

/*!
    \qmlproperty enumeration PickingSettings::pickMethod

    Holds the current pick method.

    \list
        \li PickingSettings.BoundingVolumePicking
        \li PickingSettings.TrianglePicking
    \endlist

    \sa Qt3DRender::QPickingSettings::PickMethod
*/
/*!
    \property QPickingSettings::pickMethod

    Holds the current pick method.

    By default, for performance reasons, ray casting will use bounding volume picking.
    This may however lead to unexpected results if a small object is englobed
    in the bounding sphere of a large object behind it.

    Triangle picking will produce exact results but is computationally more expensive.
*/
void QPickingSettings::setPickMethod(QPickingSettings::PickMethod pickMethod)
{
    Q_D(QPickingSettings);
    if (d->m_pickMethod == pickMethod)
        return;

    d->m_pickMethod = pickMethod;
    emit pickMethodChanged(pickMethod);
}

/*!
 * \enum Qt3DRender::QPickingSettings::PickResultMode
 *
 * Specifies what is included into the picking results.
 *
 * \value NearestPick Only the nearest entity to picking ray origin intersected by the picking ray
 * is picked (default).
 * \value AllPicks All entities that intersect the picking ray are picked.
 *
 * \sa Qt3DRender::QPickEvent
 */

/*!
    \qmlproperty enumeration PickingSettings::pickResultMode

    Holds the current pick results mode.

    \list
        \li PickingSettings.NearestPick
        \li PickingSettings.AllPicks
    \endlist

    \sa Qt3DRender::QPickingSettings::PickResultMode
*/
/*!
    \property QPickingSettings::pickResultMode

    Holds the current pick results mode.

    By default, pick results will only be produced for the entity closest to the camera.

    When setting the pick method to AllPicks, events will be triggered for all the
    entities with a QObjectPicker along the ray.

    If a QObjectPicker is assigned to an entity with multiple children, an event will
    be triggered for each child entity that intersects the ray.
*/
void QPickingSettings::setPickResultMode(QPickingSettings::PickResultMode pickResultMode)
{
    Q_D(QPickingSettings);
    if (d->m_pickResultMode == pickResultMode)
        return;

    d->m_pickResultMode = pickResultMode;
    emit pickResultModeChanged(pickResultMode);
}

/*!
    \enum Qt3DRender::QPickingSettings::FaceOrientationPickingMode

    Specifies how face orientation affects triangle picking

    \value FrontFace Only front-facing triangles will be picked (default).
    \value BackFace Only back-facing triangles will be picked.
    \value FrontAndBackFace Both front- and back-facing triangles will be picked.
*/

/*!
    \qmlproperty enumeration PickingSettings::faceOrientationPickingMode

    Specifies how face orientation affects triangle picking

    \list
        \li PickingSettings.FrontFace Only front-facing triangles will be picked (default).
        \li PickingSettings.BackFace Only back-facing triangles will be picked.
        \li PickingSettings.FrontAndBackFace Both front- and back-facing triangles will be picked.
    \endlist
*/
/*!
    \property QPickingSettings::faceOrientationPickingMode

    Specifies how face orientation affects triangle picking
*/
void QPickingSettings::setFaceOrientationPickingMode(QPickingSettings::FaceOrientationPickingMode faceOrientationPickingMode)
{
    Q_D(QPickingSettings);
    if (d->m_faceOrientationPickingMode == faceOrientationPickingMode)
        return;

    d->m_faceOrientationPickingMode = faceOrientationPickingMode;
    emit faceOrientationPickingModeChanged(faceOrientationPickingMode);
}

} // namespace Qt3Drender

QT_END_NAMESPACE
