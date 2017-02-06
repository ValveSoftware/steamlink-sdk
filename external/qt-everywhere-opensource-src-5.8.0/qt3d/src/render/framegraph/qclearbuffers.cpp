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

#include "qclearbuffers.h"
#include "qclearbuffers_p.h"
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
/*!
    \class Qt3DRender::QClearBuffers
    \inmodule Qt3DRender
    \since 5.7
    \ingroup framegraph
    \brief Class to clear buffers

    A Qt3DRender::QClearBuffers FrameGraph node enables clearing of the specific
    render target buffers with specific values.
 */

/*!
    \qmltype ClearBuffers
    \inqmlmodule Qt3D.Render
    \instantiates Qt3DRender::QClearBuffers
    \inherits FrameGraphNode
    \since 5.7
    \brief Class to clear buffers

    A Qt3DRender::QClearBuffers FrameGraph node enables clearing of the specific
    render target buffers with specific values.
*/

/*!
    \enum QClearBuffers::BufferType

    This enum type describes types of buffer to be cleared.
    \value None No buffers will be cleared
    \value ColorBuffer Clear color buffers
    \value DepthBuffer Clear depth buffer
    \value StencilBuffer Clear stencil buffer
    \value DepthStencilBuffer Clear depth and stencil buffers
    \value ColorDepthBuffer Clear color and depth buffers
    \value ColorDepthStencilBuffer Clear color, depth and stencil buffers
    \value AllBuffers Clear all buffers
*/

QClearBuffersPrivate::QClearBuffersPrivate()
    : QFrameGraphNodePrivate()
    , m_buffersType(QClearBuffers::None)
    , m_clearDepthValue(1.f)
    , m_clearStencilValue(0)
    , m_buffer(Q_NULLPTR)
{
}

/*!
    The constructor creates an instance with the specified \a parent.
 */
QClearBuffers::QClearBuffers(QNode *parent)
    : QFrameGraphNode(*new QClearBuffersPrivate, parent)
{
}

/*! \internal */
QClearBuffers::~QClearBuffers()
{
}

/*! \internal */
QClearBuffers::QClearBuffers(QClearBuffersPrivate &dd, QNode *parent)
    : QFrameGraphNode(dd, parent)
{
}


QClearBuffers::BufferType QClearBuffers::buffers() const
{
    Q_D(const QClearBuffers);
    return d->m_buffersType;
}

QColor QClearBuffers::clearColor() const
{
    Q_D(const QClearBuffers);
    return d->m_clearColor;
}

float QClearBuffers::clearDepthValue() const
{
    Q_D(const QClearBuffers);
    return d->m_clearDepthValue;
}

int QClearBuffers::clearStencilValue() const
{
    Q_D(const QClearBuffers);
    return d->m_clearStencilValue;
}

QRenderTargetOutput *QClearBuffers::colorBuffer() const
{
    Q_D(const QClearBuffers);
    return d->m_buffer;
}

/*!
    \property Qt3DRender::QClearBuffers::buffers
    Specifies the buffer type to be used.
 */

/*!
    \qmlproperty enumeration Qt3D.Render::ClearBuffers::buffers
    Specifies the buffer type to be used.
*/
void QClearBuffers::setBuffers(QClearBuffers::BufferType buffers)
{
    Q_D(QClearBuffers);
    if (d->m_buffersType != buffers) {
        d->m_buffersType = buffers;
        emit buffersChanged(buffers);
    }
}

/*!
    \property Qt3DRender::QClearBuffers::clearColor
    Specifies the clear color to be used.
 */
/*!
    \qmlproperty color Qt3D.Render::ClearBuffers::color
    Specifies the clear color to be used.
*/
void QClearBuffers::setClearColor(const QColor &color)
{
    Q_D(QClearBuffers);
    if (d->m_clearColor != color) {
        d->m_clearColor = color;
        emit clearColorChanged(color);
    }
}


/*!
    \property Qt3DRender::QClearBuffers::clearDepthValue
    Specifies the clear depth value to be used.
 */
/*!
    \qmlproperty real Qt3D.Render::ClearBuffers::clearDepthValue
    Specifies the clear depth value to be used.
*/
void QClearBuffers::setClearDepthValue(float clearDepthValue)
{
    Q_D(QClearBuffers);
    if (d->m_clearDepthValue != clearDepthValue) {
        if (clearDepthValue >= 0.f && clearDepthValue <= 1.f) {
            d->m_clearDepthValue = clearDepthValue;
            emit clearDepthValueChanged(clearDepthValue);
        } else qWarning() << "Invalid clear depth value";
    }
}


/*!
    \property Qt3DRender::QClearBuffers::clearStencilValue
    Specifies the stencil value to be used.
 */
/*!
    \qmlproperty int Qt3D.Render::ClearBuffers::clearStencilValue
    Specifies the stencil value to be used.
*/
void QClearBuffers::setClearStencilValue(int clearStencilValue)
{
    Q_D(QClearBuffers);
    if (d->m_clearStencilValue != clearStencilValue) {
        d->m_clearStencilValue = clearStencilValue;
        emit clearStencilValueChanged(clearStencilValue);
    }
}

/*!
    \property Qt3DRender::QClearBuffers::colorBuffer
    Specifies a specific color buffer to clear. If set to NULL (default), and
    ColorBuffer flag is set, all color buffers will be cleared.
 */
/*!
    \qmlproperty RenderTargetOutput Qt3D.Render::ClearBuffers::colorbuffer
    Specifies a specific color buffer to clear. If set to NULL (default), and
    ColorBuffer flag is set, all color buffers will be cleared.
*/
void QClearBuffers::setColorBuffer(QRenderTargetOutput *buffer)
{
    Q_D(QClearBuffers);
    if (d->m_buffer != buffer) {
        d->m_buffer = buffer;
        emit colorBufferChanged(buffer);
    }
}

Qt3DCore::QNodeCreatedChangeBasePtr QClearBuffers::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QClearBuffersData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QClearBuffers);
    data.buffersType = d->m_buffersType;
    data.clearColor = d->m_clearColor;
    data.clearDepthValue = d->m_clearDepthValue;
    data.clearStencilValue = d->m_clearStencilValue;
    data.bufferId = Qt3DCore::qIdForNode(d->m_buffer);
    return creationChange;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
