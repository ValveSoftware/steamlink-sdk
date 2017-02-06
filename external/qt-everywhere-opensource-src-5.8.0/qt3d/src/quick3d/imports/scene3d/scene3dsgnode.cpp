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

#include "scene3dsgnode_p.h"
#include "scene3dlogging_p.h"

#include <QtCore/qthread.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
    \class Qt3DCore::Scene3DSGNode
    \internal

    \brief The Qt3DCore::Scene3DSGNode class is a simple QSGeometryNode subclass that
    uses a Qt3DCore::Scene3DMaterial

    The Qt3DCore::Scene3DSGNode allows to render a simple rectangle textured with a
    texture using premultiplied alpha.
 */
Scene3DSGNode::Scene3DSGNode()
    : QSGGeometryNode()
    , m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
{
    setMaterial(&m_material);
    setOpaqueMaterial(&m_opaqueMaterial);
    setGeometry(&m_geometry);
    qCDebug(Scene3D) << Q_FUNC_INFO << QThread::currentThread();
}

Scene3DSGNode::~Scene3DSGNode()
{
    qCDebug(Scene3D) << Q_FUNC_INFO << QThread::currentThread();
    // The Scene3DSGNode is deleted by the QSGRenderThread when the SceneGraph
    // is terminated.
}

void Scene3DSGNode::setRect(const QRectF &rect)
{
    if (rect != m_rect) {
        m_rect = rect;
        // Map the item's bounding rect to normalized texture coordinates
        const QRectF sourceRect(0.0f, 1.0f, 1.0f, -1.0f);
        QSGGeometry::updateTexturedRectGeometry(&m_geometry, m_rect, sourceRect);
        markDirty(DirtyGeometry);
    }
}

} // namespace Qt3DRender

QT_END_NAMESPACE
