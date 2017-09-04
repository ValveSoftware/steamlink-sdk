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

#include "qnodraw.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
    \class Qt3DRender::QNoDraw
    \inmodule Qt3DRender

    \brief When a Qt3DRender::QNoDraw node is present in a FrameGraph branch, this
    prevents the renderer from rendering any primitive.

    Qt3DRender::QNoDraw should be used when the FrameGraph needs to set up some render
    states or clear some buffers without requiring any mesh to be drawn. It has
    the same effect as having a Qt3DRender::QRenderPassFilter that matches none of
    available Qt3DRender::QRenderPass instances of the scene without the overhead cost
    of actually performing the filtering.

    When disabled, a Qt3DRender::QNoDraw node won't prevent the scene from
    being rendered. Toggling the enabled property is therefore a way to make a
    Qt3DRender::QNoDraw active or inactive.

    Qt3DRender::QNoDraw is usually used as a child of a
    Qt3DRendeR::QClearBuffers node to prevent from drawing the scene when there
    are multiple render passes.

    \code
    Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport();
    Qt3DRender::QCameraSelector *cameraSelector = new Qt3DRender::QCameraSelector(viewport);

    Qt3DRender::QClearBuffers *clearBuffers = new Qt3DRender::QClearBuffers(cameraSelector);
    clearBuffers->setBuffers(Qt3DRender::QClearBuffers::ColorDepthBuffer);

    Qt3DRender::QNoDraw *noDraw = new Qt3DRender::QNoDraw(clearBuffers);

    Qt3DRender::QRenderPassFilter *mainPass = new Qt3DRender::QRenderPassFilter(cameraSelector);
    ....
    Qt3DRender::QRenderPassFilter *previewPass = new Qt3DRender::QRenderPassFilter(cameraSelector);
    ....
    \endcode

    \since 5.5
 */

/*!
    \qmltype NoDraw
    \instantiates Qt3DRender::QNoDraw
    \inherits FrameGraphNode
    \inqmlmodule Qt3D.Render
    \since 5.5
    \brief When a NoDraw node is present in a FrameGraph branch, this
     prevents the renderer from rendering any primitive.

    NoDraw should be used when the FrameGraph needs to set up some render
    states or clear some buffers without requiring any mesh to be drawn. It has
    the same effect as having a Qt3DRender::QRenderPassFilter that matches none
    of available Qt3DRender::QRenderPass instances of the scene without the
    overhead cost of actually performing the filtering.

    When disabled, a NoDraw node won't prevent the scene from being rendered.
    Toggling the enabled property is therefore a way to make a NoDraw active or
    inactive.

    NoDraw is usually used as a child of a ClearBuffers node to prevent from
    drawing the scene when there are multiple render passes.

    \code

    Viewport {
        CameraSelector {
            ClearBuffers {
                buffers: ClearBuffers.ColorDepthBuffer
                NoDraw { } // Prevents from drawing anything
            }
            RenderPassFilter {
                ...
            }
            RenderPassFilter {
                ...
            }
        }
    }

    \endcode
*/

/*!
  The constructor creates an instance with the specified \a parent.
 */
QNoDraw::QNoDraw(QNode *parent)
    : QFrameGraphNode(parent)
{
}

/*! \internal */
QNoDraw::~QNoDraw()
{
}

} // namespace Qt3DRender

QT_END_NAMESPACE
