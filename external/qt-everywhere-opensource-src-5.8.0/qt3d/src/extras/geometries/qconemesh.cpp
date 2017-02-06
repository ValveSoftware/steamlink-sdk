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

#ifndef _USE_MATH_DEFINES
# define _USE_MATH_DEFINES // For MSVC
#endif

#include "qconemesh.h"
#include "qconegeometry.h"
#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/qbufferdatagenerator.h>
#include <Qt3DRender/qattribute.h>
#include <qmath.h>
#include <QVector3D>

QT_BEGIN_NAMESPACE

namespace Qt3DExtras {

/*!
 * \qmltype ConeMesh
 * \instantiates Qt3DExtras::QConeMesh
 * \inqmlmodule Qt3D.Extras
 * \brief A conical mesh.
 */

/*!
 * \qmlproperty int ConeMesh::rings
 *
 * Holds the number of rings in the mesh.
 */

/*!
 * \qmlproperty int ConeMesh::slices
 *
 * Holds the number of slices in the mesh.
 */

/*!
 * \qmlproperty bool ConeMesh::hasTopEndcap
 *
 * Determines if the cone top is capped or open.
 */

/*!
 * \qmlproperty bool ConeMesh::hasBottomEndcap
 *
 * Determines if the cone bottom is capped or open.
 */

/*!
 * \qmlproperty real ConeMesh::topRadius
 *
 * Holds the top radius of the cone.
 */

/*!
 * \qmlproperty real ConeMesh::bottomRadius
 *
 * Holds the bottom radius of the cone.
 */

/*!
 * \qmlproperty real ConeMesh::length
 *
 * Holds the length of the cone.
 */

/*!
 * \class Qt3DExtras::QConeMesh
 * \inmodule Qt3DExtras
 *
 * \inherits Qt3DRender::QGeometryRenderer
 *
 * \brief A conical mesh.
 */

QConeMesh::QConeMesh(QNode *parent)
    : QGeometryRenderer(parent)
{
    QConeGeometry *geometry = new QConeGeometry(this);
    QObject::connect(geometry, &QConeGeometry::hasTopEndcapChanged, this, &QConeMesh::hasTopEndcapChanged);
    QObject::connect(geometry, &QConeGeometry::hasBottomEndcapChanged, this, &QConeMesh::hasBottomEndcapChanged);
    QObject::connect(geometry, &QConeGeometry::topRadiusChanged, this, &QConeMesh::topRadiusChanged);
    QObject::connect(geometry, &QConeGeometry::bottomRadiusChanged, this, &QConeMesh::bottomRadiusChanged);
    QObject::connect(geometry, &QConeGeometry::ringsChanged, this, &QConeMesh::ringsChanged);
    QObject::connect(geometry, &QConeGeometry::slicesChanged, this, &QConeMesh::slicesChanged);
    QObject::connect(geometry, &QConeGeometry::lengthChanged, this, &QConeMesh::lengthChanged);

    QGeometryRenderer::setGeometry(geometry);
}

/*! \internal */
QConeMesh::~QConeMesh()
{
}

void QConeMesh::setHasTopEndcap(bool hasTopEndcap)
{
    static_cast<QConeGeometry *>(geometry())->setHasTopEndcap(hasTopEndcap);
}

void QConeMesh::setHasBottomEndcap(bool hasBottomEndcap)
{
    static_cast<QConeGeometry *>(geometry())->setHasBottomEndcap(hasBottomEndcap);
}

void QConeMesh::setTopRadius(float topRadius)
{
    static_cast<QConeGeometry *>(geometry())->setTopRadius(topRadius);
}

void QConeMesh::setBottomRadius(float bottomRadius)
{
    static_cast<QConeGeometry *>(geometry())->setBottomRadius(bottomRadius);
}

void QConeMesh::setRings(int rings)
{
    static_cast<QConeGeometry *>(geometry())->setRings(rings);
}

void QConeMesh::setSlices(int slices)
{
    static_cast<QConeGeometry *>(geometry())->setSlices(slices);
}

void QConeMesh::setLength(float length)
{
    static_cast<QConeGeometry *>(geometry())->setLength(length);
}

/*!
 * \property QConeMesh::hasTopEndcap
 *
 * Determines if the cone top is capped or open.
 */
bool QConeMesh::hasTopEndcap() const
{
    return static_cast<QConeGeometry *>(geometry())->hasTopEndcap();
}

/*!
 * \property QConeMesh::hasBottomEndcap
 *
 * Determines if the cone bottom is capped or open.
 */
bool QConeMesh::hasBottomEndcap() const
{
    return static_cast<QConeGeometry *>(geometry())->hasBottomEndcap();
}

/*!
 * \property QConeMesh::topRadius
 *
 * Holds the top radius of the cone.
 */
float QConeMesh::topRadius() const
{
    return static_cast<QConeGeometry *>(geometry())->topRadius();
}

/*!
 * \property QConeMesh::bottomRadius
 *
 * Holds the bottom radius of the cone.
 */
float QConeMesh::bottomRadius() const
{
    return static_cast<QConeGeometry *>(geometry())->bottomRadius();
}

/*!
 * \property QConeMesh::rings
 *
 * Holds the number of rings in the mesh.
 */
int QConeMesh::rings() const
{
    return static_cast<QConeGeometry *>(geometry())->rings();
}

/*!
 * \property QConeMesh::slices
 *
 * Holds the number of slices in the mesh.
 */
int QConeMesh::slices() const
{
    return static_cast<QConeGeometry *>(geometry())->slices();
}

/*!
 * \property QConeMesh::length
 *
 * Holds the length of the cone.
 */
float QConeMesh::length() const
{
    return static_cast<QConeGeometry *>(geometry())->length();
}

} // namespace Qt3DExtras

QT_END_NAMESPACE
