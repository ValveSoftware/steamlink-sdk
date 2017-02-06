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

#include "qclipplane.h"
#include "qclipplane_p.h"
#include <Qt3DRender/private/qrenderstatecreatedchange_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
    \class Qt3DRender::QClipPlane
    \inmodule Qt3DRender
    \since 5.5
    \brief Enables an additional OpenGL clipping plane that can be in shaders
    using gl_ClipDistance

    By default, OpenGL supports up to 8 additional clipping planes.
    Qt3DCore::QClipPlane allows to enable one of these additional planes. These
    planes can then be manipulated in the shaders using gl_ClipDistance[i]
    where i varies between 0 and 7. The underlying implementation may support more
    than 8 clip planes, but it is not guaranteed.
 */

/*!
    \qmltype ClipPlane
    \instantiates Qt3DRender::QClipPlane
    \inherits RenderState
    \inqmlmodule Qt3D.Render
    \since 5.5
    \brief Enables an additional OpenGL clipping plane that can be in shaders
    using gl_ClipDistance

    By default, OpenGL supports up to 8 additional clipping planes. ClipPlane
    allows to enable one of these additional planes. These planes can then be
    manipulated in the shaders using gl_ClipDistance[i] where i varies between
    0 and 7. The underlying implementation may support more than 8 clip planes,
    but it is not guaranteed.
*/

/*!
    \qmlproperty int ClipPlane::planeIndex
    Holds the index of the plane.
    \note Usually between 0-7.
*/

/*!
    \qmlproperty vector3d ClipPlane::normal
    Holds the normal of the plane.
*/

/*!
    \qmlproperty real ClipPlane::distance
    Holds the distance of the plane from the world origin.
*/


/*!
    \property QClipPlane::planeIndex
    Holds the index of the plane.
    \note Usually between 0-7.
*/

/*!
    \property QClipPlane::normal
    Holds the normal of the plane.
*/

/*!
    \property QClipPlane::distance
    Holds the distance of the plane from the world origin.
*/


QClipPlane::QClipPlane(QNode *parent)
    : QRenderState(*new QClipPlanePrivate(), parent)
{
}

/*! \internal */
QClipPlane::~QClipPlane()
{
}

int QClipPlane::planeIndex() const
{
    Q_D(const QClipPlane);
    return d->m_planeIndex;
}

QVector3D QClipPlane::normal() const
{
    Q_D(const QClipPlane);
    return d->m_normal;
}

float QClipPlane::distance() const
{
    Q_D(const QClipPlane);
    return d->m_distance;
}

void QClipPlane::setPlaneIndex(int planeIndex)
{
    Q_D(QClipPlane);
    if (planeIndex != d->m_planeIndex) {
        d->m_planeIndex = planeIndex;
        Q_EMIT planeIndexChanged(planeIndex);
    }
}

void QClipPlane::setNormal(QVector3D normal)
{
    Q_D(QClipPlane);
    if (normal != d->m_normal) {
        d->m_normal = normal;
        Q_EMIT normalChanged(normal);
    }
}

void QClipPlane::setDistance(float distance)
{
    Q_D(QClipPlane);
    if (distance != d->m_distance) {
        d->m_distance = distance;
        Q_EMIT distanceChanged(distance);
    }
}

Qt3DCore::QNodeCreatedChangeBasePtr QClipPlane::createNodeCreationChange() const
{
    auto creationChange = QRenderStateCreatedChangePtr<QClipPlaneData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QClipPlane);
    data.normal = d->m_normal;
    data.distance = d->m_distance;
    data.planeIndex = d->m_planeIndex;
    return creationChange;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
