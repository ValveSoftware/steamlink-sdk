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

#include "qfrustumculling.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
/*!
    \class Qt3DRender::QFrustumCulling
    \inmodule Qt3DRender
    \since 5.7
    \ingroup framegraph
    \brief Enable frustum culling for the FrameGraph

    A QFrustumCulling class enables frustum culling of the drawable entities based on
    the camera view and QGeometry bounds of the entities. If QFrustumCulling is present in
    the FrameGraph, only the entities whose QGeometry bounds intersect with the camera
    frustum, i.e. the view of the camera, are drawn. If QFrustumCulling is not present,
    all drawable entities will be drawn. The camera is selected by a QCameraSelector
    frame graph node in the current hierarchy. Frustum culling can save a lot of GPU
    processing time when the rendered scene is complex.

    \sa QCameraSelector
 */

/*!
    \qmltype FrustumCulling
    \inqmlmodule Qt3D.Render
    \instantiates Qt3DRender::QFrustumCulling
    \inherits FrameGraphNode
    \since 5.7
    \brief Enable frustum culling for the FrameGraph

    A FrustumCulling type enables frustum culling of the drawable entities based on
    the camera view and Geometry bounds of the entities. If FrustumCulling is present in
    the FrameGraph, only the entities whose Geometry bounds intersect with the camera
    frustum, i.e. the view of the camera, are drawn. If FrustumCulling is not present,
    all drawable entities will be drawn. The camera is selected by a CameraSelector
    frame graph node in the current hierarchy. Frustum culling can save a lot of GPU
    processing time when the rendered scene is complex.

    \sa CameraSelector
*/

/*!
    The constructor creates an instance with the specified \a parent.
 */
QFrustumCulling::QFrustumCulling(Qt3DCore::QNode *parent)
    : QFrameGraphNode(parent)
{
}

/*! \internal */
QFrustumCulling::~QFrustumCulling()
{
}

} // Qt3DRender

QT_END_NAMESPACE

