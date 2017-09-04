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

#include "qlevelofdetailboundingsphere.h"
#include <QSharedData>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

class QLevelOfDetailBoundingSpherePrivate: public QSharedData
{
public:
    QLevelOfDetailBoundingSpherePrivate()
        : QSharedData()
        , m_radius(0.0f)
    {}

    QLevelOfDetailBoundingSpherePrivate(QVector3D center, float radius)
        : QSharedData()
        , m_center(center)
        , m_radius(radius)
    {}

    ~QLevelOfDetailBoundingSpherePrivate()
    {}

    QVector3D m_center;
    float m_radius;
};

/*!
    \class Qt3DRender::QLevelOfDetailBoundingSphere
    \inmodule Qt3DRender
    \since 5.9
    \brief The QLevelOfDetailBoundingSphere class provides a simple spherical volume, defined by it's center and radius.
*/

/*!
    \qmltype LevelOfDetail
    \instantiates Qt3DRender::QLevelOfDetailBoundingSphere
    \inherits Component3D
    \inqmlmodule Qt3D.Render
    \brief The LevelOfDetailBoundingSphere class provides a simple spherical volume, defined by it's center and radius.
*/

/*!
 * \qmlproperty QVector3D LevelOfDetailBoundingSphere::center
 *
 * Specifies the center of the bounding sphere
 */

/*!
 * \property QLevelOfDetailBoundingSphere::center
 *
 * Specifies the center of the bounding sphere
 */

/*!
 * \qmlproperty qreal LevelOfDetailBoundingSphere::radius
 *
 * Specifies the radius of the bounding sphere
 */

/*!
 * \property QLevelOfDetailBoundingSphere::radius
 *
 * Specifies the radius of the bounding sphere
 */

/*! \fn Qt3DRender::QLevelOfDetailBoundingSphere::QLevelOfDetailBoundingSphere(const QVector3D &center = QVector3D(), float radius = -1.0f)
  Constructs a new QLevelOfDetailBoundingSphere with the specified \a center and \a radius.
 */


QLevelOfDetailBoundingSphere::QLevelOfDetailBoundingSphere(QVector3D center, float radius)
    : d_ptr(new QLevelOfDetailBoundingSpherePrivate(center, radius))
{
}

QLevelOfDetailBoundingSphere::QLevelOfDetailBoundingSphere(const QLevelOfDetailBoundingSphere &other)
    : d_ptr(other.d_ptr)
{
}

QLevelOfDetailBoundingSphere::~QLevelOfDetailBoundingSphere()
{
}

QLevelOfDetailBoundingSphere &QLevelOfDetailBoundingSphere::operator =(const QLevelOfDetailBoundingSphere &other)
{
    d_ptr = other.d_ptr;
    return *this;
}

QVector3D QLevelOfDetailBoundingSphere::center() const
{
    return d_ptr->m_center;
}

float QLevelOfDetailBoundingSphere::radius() const
{
    return d_ptr->m_radius;
}

bool QLevelOfDetailBoundingSphere::isEmpty() const
{
    return d_ptr->m_radius <= 0.0f;
}

bool QLevelOfDetailBoundingSphere::operator ==(const QLevelOfDetailBoundingSphere &other) const
{
    return d_ptr->m_center == other.center() && other.d_ptr->m_radius == other.radius();
}

bool QLevelOfDetailBoundingSphere::operator !=(const QLevelOfDetailBoundingSphere &other) const
{
    return !(*this == other);
}

} // namespace Qt3DRender

QT_END_NAMESPACE
