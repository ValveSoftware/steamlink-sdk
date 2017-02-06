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

#include "qtransform.h"
#include "qtransform_p.h"
#include "qmath3d_p.h"

#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

QTransformPrivate::QTransformPrivate()
    : QComponentPrivate()
    , m_rotation()
    , m_scale(1.0f, 1.0f, 1.0f)
    , m_translation()
    , m_eulerRotationAngles()
    , m_matrixDirty(false)
{
    m_shareable = false;
}

QTransformPrivate::~QTransformPrivate()
{
}

/*!
 * \qmltype Transform
 * \inqmlmodule Qt3D.Core
 * \inherits Component3D
 * \instantiates Qt3DCore::QComponent
 * \since 5.6
 * \brief Used to perform transforms on meshes
 *
 * The Transform component is not shareable between multiple Entity's.
 */

/*!
 * \qmlproperty matrix4x4 Transform::matrix
 *
 * Holds the matrix4x4 of the transform.
 */

/*!
 * \qmlproperty real Transform::rotationX
 *
 * Holds the x rotation of the transform as Euler angle.
 */

/*!
 * \qmlproperty real Transform::rotationY
 *
 * Holds the y rotation of the transform as Euler angle.
 */

/*!
 * \qmlproperty real Transform::rotationZ
 *
 * Holds the z rotation of the transform as Euler angle.
 */

/*!
 * \qmlproperty vector3d Transform::scale3D
 *
 * Holds the scale of the transform as vector3d.
 */

/*!
 * \qmlproperty float Transform::scale
 *
 * Holds the uniform scale of the transform. If the scale has been set with scale3D, holds
 * the x value only.
 */

/*!
 * \qmlproperty quaternion Transform::rotation
 *
 * Holds the rotation of the transform as quaternion.
 */

/*!
 * \qmlproperty vector3d Transform::translation
 *
 * Holds the translation of the transform as vector3d.
 */

/*!
 * \qmlmethod quaternion Transform::fromAxisAndAngle(vector3d axis, real angle)
 * Creates a quaternion from \a axis and \a angle.
 * \return the resulting quaternion.
 */

/*!
 * \qmlmethod quaternion Transform::fromAxisAndAngle(real x, real y, real z, real angle)
 * Creates a quaternion from \a x, \a y, \a z, and \a angle.
 * \return the resulting quaternion.
 */

/*!
 * \qmlmethod quaternion Transform::fromAxesAndAngles(vector3d axis1, real angle1,
 *                                                    vector3d axis2, real angle2)
 * Creates a quaternion from \a axis1, \a angle1, \a axis2, and \a angle2.
 * \return the resulting quaternion.
 */

/*!
 * \qmlmethod quaternion Transform::fromAxesAndAngles(vector3d axis1, real angle1,
 *                                                    vector3d axis2, real angle2,
 *                                                    vector3d axis3, real angle3)
 * Creates a quaternion from \a axis1, \a angle1, \a axis2, \a angle2, \a axis3, and \a angle3.
 * \return the resulting quaternion.
 */

/*!
 * \qmlmethod quaternion Transform::fromEulerAngles(vector3d eulerAngles)
 * Creates a quaternion from \a eulerAngles.
 * \return the resulting quaternion.
 */

/*!
 * \qmlmethod quaternion Transform::fromEulerAngles(real pitch, real yaw, real roll)
 * Creates a quaternion from \a pitch, \a yaw, and \a roll.
 * \return the resulting quaternion.
 */

/*!
 * \qmlmethod matrix4x4 Transform::rotateAround(vector3d point, real angle, vector3d axis)
 * Creates a rotation matrix from \a axis and \a angle around \a point.
 * \return the resulting matrix4x4.
 */

/*!
 * \class Qt3DCore::QTransform
 * \inmodule Qt3DCore
 * \inherits Qt3DCore::QComponent
 * \since 5.6
 * \brief Used to perform transforms on meshes
 *
 * The QTransform component is not shareable between multiple QEntity's.
 */

/*!
 * Constructs a new QTransform with \a parent.
 */
QTransform::QTransform(QNode *parent)
    : QComponent(*new QTransformPrivate, parent)
{
}

/*!
 * \internal
 */
QTransform::~QTransform()
{
}

/*!
 * \internal
 */
QTransform::QTransform(QTransformPrivate &dd, QNode *parent)
    : QComponent(dd, parent)
{
}

void QTransform::setMatrix(const QMatrix4x4 &m)
{
    Q_D(QTransform);
    if (m != matrix()) {
        d->m_matrix = m;
        d->m_matrixDirty = false;

        QVector3D s;
        QVector3D t;
        QQuaternion r;
        decomposeQMatrix4x4(m, t, r, s);
        d->m_scale = s;
        d->m_rotation = r;
        d->m_translation = t;
        d->m_eulerRotationAngles = d->m_rotation.toEulerAngles();
        emit scale3DChanged(s);
        emit rotationChanged(r);
        emit translationChanged(t);

        const bool wasBlocked = blockNotifications(true);
        emit matrixChanged();
        emit scaleChanged(d->m_scale.x());
        emit rotationXChanged(d->m_eulerRotationAngles.x());
        emit rotationYChanged(d->m_eulerRotationAngles.y());
        emit rotationZChanged(d->m_eulerRotationAngles.z());
        blockNotifications(wasBlocked);
    }
}

void QTransform::setRotationX(float rotationX)
{
    Q_D(QTransform);

    if (d->m_eulerRotationAngles.x() == rotationX)
        return;

    d->m_eulerRotationAngles.setX(rotationX);
    QQuaternion rotation = QQuaternion::fromEulerAngles(d->m_eulerRotationAngles);
    if (rotation != d->m_rotation) {
        d->m_rotation = rotation;
        d->m_matrixDirty = true;
        emit rotationChanged(rotation);
    }

    const bool wasBlocked = blockNotifications(true);
    emit rotationXChanged(rotationX);
    emit matrixChanged();
    blockNotifications(wasBlocked);
}

void QTransform::setRotationY(float rotationY)
{
    Q_D(QTransform);

    if (d->m_eulerRotationAngles.y() == rotationY)
        return;

    d->m_eulerRotationAngles.setY(rotationY);
    QQuaternion rotation = QQuaternion::fromEulerAngles(d->m_eulerRotationAngles);
    if (rotation != d->m_rotation) {
        d->m_rotation = rotation;
        d->m_matrixDirty = true;
        emit rotationChanged(rotation);
    }

    const bool wasBlocked = blockNotifications(true);
    emit rotationYChanged(rotationY);
    emit matrixChanged();
    blockNotifications(wasBlocked);
}

void QTransform::setRotationZ(float rotationZ)
{
    Q_D(QTransform);
    if (d->m_eulerRotationAngles.z() == rotationZ)
        return;

    d->m_eulerRotationAngles.setZ(rotationZ);
    QQuaternion rotation = QQuaternion::fromEulerAngles(d->m_eulerRotationAngles);
    if (rotation != d->m_rotation) {
        d->m_rotation = rotation;
        d->m_matrixDirty = true;
        emit rotationChanged(rotation);
    }

    const bool wasBlocked = blockNotifications(true);
    emit rotationZChanged(rotationZ);
    emit matrixChanged();
    blockNotifications(wasBlocked);
}

/*!
 * \property Qt3DCore::QTransform::matrix
 *
 * Holds the QMatrix4x4 of the transform.
 */
QMatrix4x4 QTransform::matrix() const
{
    Q_D(const QTransform);
    if (d->m_matrixDirty) {
        composeQMatrix4x4(d->m_translation, d->m_rotation, d->m_scale, d->m_matrix);
        d->m_matrixDirty = false;
    }
    return d->m_matrix;
}

/*!
 * \property Qt3DCore::QTransform::rotationX
 *
 * Holds the x rotation of the transform as Euler angle.
 */
float QTransform::rotationX() const
{
    Q_D(const QTransform);
    return d->m_eulerRotationAngles.x();
}

/*!
 * \property Qt3DCore::QTransform::rotationY
 *
 * Holds the y rotation of the transform as Euler angle.
 */
float QTransform::rotationY() const
{
    Q_D(const QTransform);
    return d->m_eulerRotationAngles.y();
}

/*!
 * \property Qt3DCore::QTransform::rotationZ
 *
 * Holds the z rotation of the transform as Euler angle.
 */
float QTransform::rotationZ() const
{
    Q_D(const QTransform);
    return d->m_eulerRotationAngles.z();
}

void QTransform::setScale3D(const QVector3D &scale)
{
    Q_D(QTransform);
    if (scale != d->m_scale) {
        d->m_scale = scale;
        d->m_matrixDirty = true;
        emit scale3DChanged(scale);

        const bool wasBlocked = blockNotifications(true);
        emit matrixChanged();
        blockNotifications(wasBlocked);
    }
}

/*!
 * \property Qt3DCore::QTransform::scale3D
 *
 * Holds the scale of the transform as QVector3D.
 */
QVector3D QTransform::scale3D() const
{
    Q_D(const QTransform);
    return d->m_scale;
}

void QTransform::setScale(float scale)
{
    Q_D(QTransform);
    if (scale != d->m_scale.x()) {
        setScale3D(QVector3D(scale, scale, scale));

        const bool wasBlocked = blockNotifications(true);
        emit scaleChanged(scale);
        blockNotifications(wasBlocked);
    }
}

/*!
 * \property Qt3DCore::QTransform::scale
 *
 * Holds the uniform scale of the transform. If the scale has been set with setScale3D, holds
 * the x value only.
 */
float QTransform::scale() const
{
    Q_D(const QTransform);
    return d->m_scale.x();
}

void QTransform::setRotation(const QQuaternion &rotation)
{
    Q_D(QTransform);
    if (rotation != d->m_rotation) {
        d->m_rotation = rotation;
        const QVector3D oldRotation = d->m_eulerRotationAngles;
        d->m_eulerRotationAngles = d->m_rotation.toEulerAngles();
        d->m_matrixDirty = true;
        emit rotationChanged(rotation);

        const bool wasBlocked = blockNotifications(true);
        emit matrixChanged();
        if (d->m_eulerRotationAngles.x() != oldRotation.x())
            emit rotationXChanged(d->m_eulerRotationAngles.x());
        if (d->m_eulerRotationAngles.y() != oldRotation.y())
            emit rotationYChanged(d->m_eulerRotationAngles.y());
        if (d->m_eulerRotationAngles.z() != oldRotation.z())
            emit rotationZChanged(d->m_eulerRotationAngles.z());
        blockNotifications(wasBlocked);
    }
}

/*!
 * \property Qt3DCore::QTransform::rotation
 *
 * Holds the rotation of the transform as QQuaternion.
 */
QQuaternion QTransform::rotation() const
{
    Q_D(const QTransform);
    return d->m_rotation;
}

void QTransform::setTranslation(const QVector3D &translation)
{
    Q_D(QTransform);
    if (translation != d->m_translation) {
        d->m_translation = translation;
        d->m_matrixDirty = true;
        emit translationChanged(translation);

        const bool wasBlocked = blockNotifications(true);
        emit matrixChanged();
        blockNotifications(wasBlocked);
    }
}

/*!
 * \property Qt3DCore::QTransform::translation
 *
 * Holds the translation of the transform as QVector3D.
 */
QVector3D QTransform::translation() const
{
    Q_D(const QTransform);
    return d->m_translation;
}

/*!
 * Creates a QQuaternion from \a axis and \a angle.
 * \return the resulting QQuaternion.
 */
QQuaternion QTransform::fromAxisAndAngle(const QVector3D &axis, float angle)
{
    return QQuaternion::fromAxisAndAngle(axis, angle);
}

/*!
 * Creates a QQuaternion from \a x, \a y, \a z, and \a angle.
 * \return the resulting QQuaternion.
 */
QQuaternion QTransform::fromAxisAndAngle(float x, float y, float z, float angle)
{
    return QQuaternion::fromAxisAndAngle(x, y, z, angle);
}

/*!
 * Creates a QQuaternion from \a axis1, \a angle1, \a axis2, and \a angle2.
 * \return the resulting QQuaternion.
 */
QQuaternion QTransform::fromAxesAndAngles(const QVector3D &axis1, float angle1,
                                          const QVector3D &axis2, float angle2)
{
    const QQuaternion q1 = QQuaternion::fromAxisAndAngle(axis1, angle1);
    const QQuaternion q2 = QQuaternion::fromAxisAndAngle(axis2, angle2);
    return q2 * q1;
}

/*!
 * Creates a QQuaternion from \a axis1, \a angle1, \a axis2, \a angle2, \a axis3, and \a angle3.
 * \return the resulting QQuaternion.
 */
QQuaternion QTransform::fromAxesAndAngles(const QVector3D &axis1, float angle1,
                                          const QVector3D &axis2, float angle2,
                                          const QVector3D &axis3, float angle3)
{
    const QQuaternion q1 = QQuaternion::fromAxisAndAngle(axis1, angle1);
    const QQuaternion q2 = QQuaternion::fromAxisAndAngle(axis2, angle2);
    const QQuaternion q3 = QQuaternion::fromAxisAndAngle(axis3, angle3);
    return q3 * q2 * q1;
}

/*!
 * Creates a QQuaternion from \a eulerAngles.
 * \return the resulting QQuaternion.
 */
QQuaternion QTransform::fromEulerAngles(const QVector3D &eulerAngles)
{
    return QQuaternion::fromEulerAngles(eulerAngles);
}

/*!
 * Creates a QQuaternion from \a pitch, \a yaw, and \a roll.
 * \return the resulting QQuaternion.
 */
QQuaternion QTransform::fromEulerAngles(float pitch, float yaw, float roll)
{
    return QQuaternion::fromEulerAngles(pitch, yaw, roll);
}

/*!
 * Creates a rotation matrix from \a axis and \a angle around \a point.
 * \return the resulting QMatrix4x4.
 */
QMatrix4x4 QTransform::rotateAround(const QVector3D &point, float angle, const QVector3D &axis)
{
    QMatrix4x4 m;
    m.translate(point);
    m.rotate(angle, axis);
    m.translate(-point);
    return m;
}

QNodeCreatedChangeBasePtr QTransform::createNodeCreationChange() const
{
    auto creationChange = QNodeCreatedChangePtr<QTransformData>::create(this);
    auto &data = creationChange->data;

    Q_D(const QTransform);
    data.rotation = d->m_rotation;
    data.scale = d->m_scale;
    data.translation = d->m_translation;

    return creationChange;
}

} // namespace Qt3DCore

QT_END_NAMESPACE

#include "moc_qtransform.cpp"
