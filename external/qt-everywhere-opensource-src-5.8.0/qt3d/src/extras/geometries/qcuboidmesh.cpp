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

#include "qcuboidmesh.h"
#include "qcuboidgeometry.h"

QT_BEGIN_NAMESPACE

namespace  Qt3DExtras {

/*!
 * \qmltype CuboidMesh
 * \instantiates Qt3DExtras::QCuboidMesh
 * \inqmlmodule Qt3D.Extras
 * \brief A cuboid mesh.
 */

/*!
 * \qmlproperty real CuboidMesh::xExtent
 *
 * Holds the x extent of the mesh.
 */

/*!
 * \qmlproperty real CuboidMesh::yExtent
 *
 * Holds the y extent of the mesh.
 */

/*!
 * \qmlproperty real CuboidMesh::zExtent
 *
 * Holds the z extent of the mesh.
 */

/*!
 * \qmlproperty size CuboidMesh::yzMeshResolution
 *
 * Holds the y-z resolution of the mesh.
 * The width and height values of this property specify the number of vertices generated for
 * the y-z faces of the mesh.
 */

/*!
 * \qmlproperty size CuboidMesh::xzMeshResolution
 *
 * Holds the x-z resolution of the mesh.
 * The width and height values of this property specify the number of vertices generated for
 * the x-z faces of the mesh.
 */

/*!
 * \qmlproperty size CuboidMesh::xyMeshResolution
 *
 * Holds the x-y resolution of the mesh.
 * The width and height values of this property specify the number of vertices generated for
 * the x-y faces of the mesh.
 */

/*!
 * \class Qt3DExtras::QCuboidMesh
 * \inmodule Qt3DExtras
 *
 * \inherits Qt3DRender::QGeometryRenderer
 *
 * \brief A cuboid mesh.
 */

/*!
 * Constructs a new QCuboidMesh with \a parent.
 */
QCuboidMesh::QCuboidMesh(QNode *parent)
    : QGeometryRenderer(parent)
{
    QCuboidGeometry *geometry = new QCuboidGeometry(this);
    QObject::connect(geometry, &QCuboidGeometry::xExtentChanged, this, &QCuboidMesh::xExtentChanged);
    QObject::connect(geometry, &QCuboidGeometry::yExtentChanged, this, &QCuboidMesh::yExtentChanged);
    QObject::connect(geometry, &QCuboidGeometry::zExtentChanged, this, &QCuboidMesh::zExtentChanged);
    QObject::connect(geometry, &QCuboidGeometry::xyMeshResolutionChanged, this, &QCuboidMesh::xyMeshResolutionChanged);
    QObject::connect(geometry, &QCuboidGeometry::xzMeshResolutionChanged, this, &QCuboidMesh::xzMeshResolutionChanged);
    QObject::connect(geometry, &QCuboidGeometry::yzMeshResolutionChanged, this, &QCuboidMesh::yzMeshResolutionChanged);
    QGeometryRenderer::setGeometry(geometry);
}

/*! \internal */
QCuboidMesh::~QCuboidMesh()
{
}

void QCuboidMesh::setXExtent(float xExtent)
{
    static_cast<QCuboidGeometry *>(geometry())->setXExtent(xExtent);
}

/*!
 * \property QCuboidMesh::xExtent
 *
 * Holds the x extent of the mesh.
 */
float QCuboidMesh::xExtent() const
{
    return static_cast<QCuboidGeometry *>(geometry())->xExtent();
}

void QCuboidMesh::setYExtent(float yExtent)
{
    static_cast<QCuboidGeometry *>(geometry())->setYExtent(yExtent);
}

/*!
 * \property QCuboidMesh::yExtent
 *
 * Holds the y extent of the mesh.
 */
float QCuboidMesh::yExtent() const
{
    return static_cast<QCuboidGeometry *>(geometry())->yExtent();
}

void QCuboidMesh::setZExtent(float zExtent)
{
    static_cast<QCuboidGeometry *>(geometry())->setZExtent(zExtent);
}

/*!
 * \property QCuboidMesh::zExtent
 *
 * Holds the z extent of the mesh.
 */
float QCuboidMesh::zExtent() const
{
    return static_cast<QCuboidGeometry *>(geometry())->zExtent();
}

void QCuboidMesh::setYZMeshResolution(const QSize &resolution)
{
    static_cast<QCuboidGeometry *>(geometry())->setYZMeshResolution(resolution);
}

/*!
 * \property QCuboidMesh::yzMeshResolution
 *
 * Holds the y-z resolution of the mesh.
 * The width and height values of this property specify the number of vertices generated for
 * the y-z faces of the mesh.
 */
QSize QCuboidMesh::yzMeshResolution() const
{
    return static_cast<QCuboidGeometry *>(geometry())->yzMeshResolution();
}

void QCuboidMesh::setXZMeshResolution(const QSize &resolution)
{
    static_cast<QCuboidGeometry *>(geometry())->setXZMeshResolution(resolution);
}

/*!
 * \property QCuboidMesh::xzMeshResolution
 *
 * Holds the x-z resolution of the mesh.
 * The width and height values of this property specify the number of vertices generated for
 * the x-z faces of the mesh.
 */
QSize QCuboidMesh::xzMeshResolution() const
{
    return static_cast<QCuboidGeometry *>(geometry())->xzMeshResolution();
}

void QCuboidMesh::setXYMeshResolution(const QSize &resolution)
{
    static_cast<QCuboidGeometry *>(geometry())->setXYMeshResolution(resolution);
}

/*!
 * \property QCuboidMesh::xyMeshResolution
 *
 * Holds the x-y resolution of the mesh.
 * The width and height values of this property specify the number of vertices generated for
 * the x-y faces of the mesh.
 */
QSize QCuboidMesh::xyMeshResolution() const
{
    return static_cast<QCuboidGeometry *>(geometry())->xyMeshResolution();
}

} // namespace  Qt3DExtras

QT_END_NAMESPACE
