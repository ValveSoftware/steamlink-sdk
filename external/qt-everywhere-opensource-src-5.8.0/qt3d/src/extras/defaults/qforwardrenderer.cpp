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

#include "qforwardrenderer.h"
#include "qforwardrenderer_p.h"

#include <Qt3DCore/qentity.h>
#include <Qt3DRender/qviewport.h>
#include <Qt3DRender/qcameraselector.h>
#include <Qt3DRender/qclearbuffers.h>
#include <Qt3DRender/qfilterkey.h>
#include <Qt3DRender/qfrustumculling.h>
#include <Qt3DRender/qrendersurfaceselector.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DRender;

namespace Qt3DExtras {

QForwardRendererPrivate::QForwardRendererPrivate()
    : QTechniqueFilterPrivate()
    , m_surfaceSelector(new QRenderSurfaceSelector)
    , m_viewport(new QViewport())
    , m_cameraSelector(new QCameraSelector())
    , m_clearBuffer(new QClearBuffers())
    , m_frustumCulling(new QFrustumCulling())
{
}

void QForwardRendererPrivate::init()
{
    Q_Q(QForwardRenderer);

    m_frustumCulling->setParent(m_clearBuffer);
    m_clearBuffer->setParent(m_cameraSelector);
    m_cameraSelector->setParent(m_viewport);
    m_viewport->setParent(m_surfaceSelector);
    m_surfaceSelector->setParent(q);

    m_viewport->setNormalizedRect(QRectF(0.0f, 0.0f, 1.0f, 1.0f));
    m_clearBuffer->setClearColor(Qt::white);
    m_clearBuffer->setBuffers(QClearBuffers::ColorDepthBuffer);

    QFilterKey *forwardRenderingStyle = new QFilterKey(q);
    forwardRenderingStyle->setName(QStringLiteral("renderingStyle"));
    forwardRenderingStyle->setValue(QStringLiteral("forward"));
    q->addMatch(forwardRenderingStyle);
}

/*!
    \class Qt3DExtras::QForwardRenderer
    \brief The QForwardRenderer provides a default \l{Qt 3D Render Framegraph}{FrameGraph}
    implementation of a forward renderer.
    \inmodule Qt3DExtras
    \since 5.7
    \inherits Qt3DRender::QTechniqueFilter

    Forward rendering is what OpenGL traditionally uses. It renders directly to the backbuffer
    one object at a time shading each one as it goes.

    QForwardRenderer is a single leaf \l{Qt 3D Render Framegraph}{FrameGraph} tree which contains
    a Qt3DRender::QViewport, a Qt3DRender::QCameraSelector, and a Qt3DRender::QClearBuffers.
    The QForwardRenderer has a default requirement filter key whose name is "renderingStyle" and
    value "forward".
    If you need to filter out your techniques, you should do so based on that filter key.

    By default the viewport occupies the whole screen and the clear color is white.
    Frustum culling is also enabled.
*/
/*!
    \qmltype ForwardRenderer
    \brief The ForwardRenderer provides a default \l{Qt 3D Render Framegraph}{FrameGraph}
    implementation of a forward renderer.
    \since 5.7
    \inqmlmodule Qt3D.Extras
    \instantiates Qt3DExtras::QForwardRenderer

    Forward rendering is what OpenGL traditionally uses. It renders directly to the backbuffer
    one object at a time shading each one as it goes.

    ForwardRenderer is a single leaf \l{Qt 3D Render Framegraph}{FrameGraph} tree which contains
    a Viewport, a CameraSelector, and a ClearBuffers.
    The ForwardRenderer has a default requirement filter key whose name is "renderingStyle" and
    value "forward".
    If you need to filter out your techniques, you should do so based on that filter key.

    By default the viewport occupies the whole screen and the clear color is white.
    Frustum culling is also enabled.
 */

QForwardRenderer::QForwardRenderer(QNode *parent)
    : QTechniqueFilter(*new QForwardRendererPrivate, parent)
{
    Q_D(QForwardRenderer);
    QObject::connect(d->m_clearBuffer, &QClearBuffers::clearColorChanged, this, &QForwardRenderer::clearColorChanged);
    QObject::connect(d->m_viewport, &QViewport::normalizedRectChanged, this, &QForwardRenderer::viewportRectChanged);
    QObject::connect(d->m_cameraSelector, &QCameraSelector::cameraChanged, this, &QForwardRenderer::cameraChanged);
    QObject::connect(d->m_surfaceSelector, &QRenderSurfaceSelector::surfaceChanged, this, &QForwardRenderer::surfaceChanged);
    QObject::connect(d->m_surfaceSelector, &QRenderSurfaceSelector::externalRenderTargetSizeChanged, this, &QForwardRenderer::externalRenderTargetSizeChanged);
    d->init();
}

QForwardRenderer::~QForwardRenderer()
{
}

void QForwardRenderer::setViewportRect(const QRectF &viewportRect)
{
    Q_D(QForwardRenderer);
    d->m_viewport->setNormalizedRect(viewportRect);
}

void QForwardRenderer::setClearColor(const QColor &clearColor)
{
    Q_D(QForwardRenderer);
    d->m_clearBuffer->setClearColor(clearColor);
}

void QForwardRenderer::setCamera(Qt3DCore::QEntity *camera)
{
    Q_D(QForwardRenderer);
    d->m_cameraSelector->setCamera(camera);
}

void QForwardRenderer::setSurface(QObject *surface)
{
    Q_D(QForwardRenderer);
    d->m_surfaceSelector->setSurface(surface);
}

void QForwardRenderer::setExternalRenderTargetSize(const QSize &size)
{
    Q_D(QForwardRenderer);
    d->m_surfaceSelector->setExternalRenderTargetSize(size);
}

/*!
    \qmlproperty rect ForwardRenderer::viewportRect

    Holds the current normalized viewport rectangle.
*/
/*!
    \property QForwardRenderer::viewportRect

    Holds the current normalized viewport rectangle.
*/
QRectF QForwardRenderer::viewportRect() const
{
    Q_D(const QForwardRenderer);
    return d->m_viewport->normalizedRect();
}

/*!
    \qmlproperty color ForwardRenderer::clearColor

    Holds the current clear color of the scene. The frame buffer is initialized to the clear color
    before rendering.
*/
/*!
    \property QForwardRenderer::clearColor

    Holds the current clear color of the scene. The frame buffer is initialized to the clear color
    before rendering.
*/
QColor QForwardRenderer::clearColor() const
{
    Q_D(const QForwardRenderer);
    return d->m_clearBuffer->clearColor();
}

/*!
    \qmlproperty Entity ForwardRenderer::camera

    Holds the current camera entity used to render the scene.

    \note A camera is an Entity that has a CameraLens as one of its components.
*/
/*!
    \property QForwardRenderer::camera

    Holds the current camera entity used to render the scene.

    \note A camera is a QEntity that has a QCameraLens as one of its components.
*/
Qt3DCore::QEntity *QForwardRenderer::camera() const
{
    Q_D(const QForwardRenderer);
    return d->m_cameraSelector->camera();
}

/*!
    \qmlproperty Object ForwardRenderer::surface

    Holds the current render surface.
*/
/*!
    \property QForwardRenderer::surface

    Holds the current render surface.
*/
QObject *QForwardRenderer::surface() const
{
    Q_D(const QForwardRenderer);
    return d->m_surfaceSelector->surface();
}

QSize QForwardRenderer::externalRenderTargetSize() const
{
    Q_D(const QForwardRenderer);
    return d->m_surfaceSelector->externalRenderTargetSize();
}

} // namespace Qt3DExtras

QT_END_NAMESPACE
