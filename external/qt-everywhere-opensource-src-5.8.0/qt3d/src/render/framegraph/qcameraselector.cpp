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

#include "qcameraselector.h"
#include "qcameraselector_p.h"
#include <Qt3DCore/qentity.h>
#include <Qt3DCore/private/qentity_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
    \class Qt3DRender::QCameraSelector
    \inmodule Qt3DRender
    \since 5.5
    \ingroup framegraph

    \brief Class to allow for selection of camera to be used

    A Qt3DRender::QCameraSelector can be used to select the camera, which is used
    by the FrameGraph when drawing the entities.
 */

/*!
    \qmltype CameraSelector
    \inqmlmodule Qt3D.Render
    \instantiates Qt3DRender::QCameraSelector
    \inherits FrameGraphNode
    \since 5.5
    \brief Class to allow for selection of camera to be used

    A CameraSelector can be used to select the camera, which is used
    by the FrameGraph when drawing the entities.
*/

/*!
    \qmlproperty Entity Qt3D.Render::CameraSelector::camera

    Holds the currently selected camera.
*/

/*!
    \property Qt3DRender::QCameraSelector::camera

    Holds the currently selected camera.
*/


/*! \internal */
QCameraSelector::QCameraSelector(QCameraSelectorPrivate &dd, QNode *parent)
    : QFrameGraphNode(dd, parent)
{
}

QCameraSelectorPrivate::QCameraSelectorPrivate()
    : QFrameGraphNodePrivate()
    , m_camera(nullptr)
{
}

/*!
  The constructor creates an instance with the specified \a parent.
 */
QCameraSelector::QCameraSelector(Qt3DCore::QNode *parent)
    :   QFrameGraphNode(*new QCameraSelectorPrivate, parent)
{
}

/*! \internal */
QCameraSelector::~QCameraSelector()
{
}

void QCameraSelector::setCamera(Qt3DCore::QEntity *camera)
{
    Q_D(QCameraSelector);
    if (d->m_camera != camera) {

        if (d->m_camera)
            d->unregisterDestructionHelper(d->m_camera);

        // We need to add it as a child of the current node if it has been declared inline
        // Or not previously added as a child of the current node so that
        // 1) The backend gets notified about it's creation
        // 2) When the current node is destroyed, it gets destroyed as well
        if (camera && !camera->parent())
            camera->setParent(this);
        d->m_camera = camera;

        // Ensures proper bookkeeping
        if (d->m_camera)
            d->registerDestructionHelper(d->m_camera, &QCameraSelector::setCamera, d->m_camera);

        emit cameraChanged(camera);
    }
}

Qt3DCore::QEntity *QCameraSelector::camera() const
{
    Q_D(const QCameraSelector);
    return d->m_camera;
}

Qt3DCore::QNodeCreatedChangeBasePtr QCameraSelector::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QCameraSelectorData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QCameraSelector);
    data.cameraId = qIdForNode(d->m_camera);
    return creationChange;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
