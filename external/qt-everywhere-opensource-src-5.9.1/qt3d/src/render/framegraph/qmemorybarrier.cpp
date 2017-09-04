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

#include "qmemorybarrier.h"
#include "qmemorybarrier_p.h"
#include <Qt3DRender/qframegraphnodecreatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
    \class Qt3DRender::QMemoryBarrier
    \inmodule Qt3DRender
    \since 5.9
    \ingroup framegraph
    \brief Class to emplace a memory barrier

    A Qt3DRender::QMemoryBarrier FrameGraph node is used to emplace a specific
    memory barrier at a specific time of the rendering. This is required to
    properly synchronize drawing and compute commands on the GPU.

    The barrier defines the ordering of memory operations issued by a prior
    command. This means that if command1 is manipulating a buffer that is to be
    used as a vertex attribute buffer in a following command2, then the memory
    barrier should be placed after command1 and setting the appropriate barrier
    type for vertex attribute buffer.

    When a QMemoryBarrier node is found in a FrameGraph branch, the barrier
    will be enforced prior to any draw or compute command even if these are
    defined deeper in the branch.

    For OpenGL rendering, this page gives more info about the
    \l {https://www.opengl.org/wiki/Memory_Model}{Memory Model}
 */

/*!
    \qmltype MemoryBarrier
    \inqmlmodule Qt3D.Render
    \instantiates Qt3DRender::QMemoryBarrier
    \inherits FrameGraphNode
    \since 5.9
    \brief Class to place a memory barrier

    A MemoryBarrier FrameGraph node is used to emplace a specific
    memory barrier at a specific time of the rendering. This is required to
    properly synchronize drawing and compute commands on the GPU.

    The barrier defines the ordering of memory operations issued by a prior
    command. This means that if command1 is manipulating a buffer that is to be
    used as a vertex attribute buffer in a following command2, then the memory
    barrier should be placed after command1 and setting the appropriate barrier
    type for vertex attribute buffer.

    When a QMemoryBarrier node is found in a FrameGraph branch, the barrier
    will be enforced prior to any draw or compute command even if these are
    defined deeper in the branch.

    For OpenGL rendering, this page gives more info about the
    \l {https://www.opengl.org/wiki/Memory_Model}{Memory Model}
*/

/*!
    \enum QMemoryBarrier::Operation

    This enum type describes types of buffer to be cleared.
    \value None
    \value ElementArray
    \value Uniform
    \value TextureFetch
    \value ShaderImageAccess
    \value Command
    \value PixelBuffer
    \value TextureUpdate
    \value BufferUpdate
    \value FrameBuffer
    \value TransformFeedback
    \value AtomicCounter
    \value ShaderStorage
    \value QueryBuffer
    \value VertexAttributeArray
    \value All
*/


QMemoryBarrierPrivate::QMemoryBarrierPrivate()
    : QFrameGraphNodePrivate()
    , m_waitOperations(QMemoryBarrier::None)
{
}

QMemoryBarrier::QMemoryBarrier(Qt3DCore::QNode *parent)
    : QFrameGraphNode(*new QMemoryBarrierPrivate(), parent)
{
}

QMemoryBarrier::~QMemoryBarrier()
{
}

void QMemoryBarrier::setWaitOperations(QMemoryBarrier::Operations waitOperations)
{
    Q_D(QMemoryBarrier);
    if (waitOperations != d->m_waitOperations) {
        d->m_waitOperations = waitOperations;
        emit waitOperationsChanged(waitOperations);
        d->notifyPropertyChange("waitOperations", QVariant::fromValue(waitOperations));
    }
}

QMemoryBarrier::Operations QMemoryBarrier::waitOperations() const
{
    Q_D(const QMemoryBarrier);
    return d->m_waitOperations;
}

QMemoryBarrier::QMemoryBarrier(QMemoryBarrierPrivate &dd, Qt3DCore::QNode *parent)
    : QFrameGraphNode(dd, parent)
{
}

Qt3DCore::QNodeCreatedChangeBasePtr QMemoryBarrier::createNodeCreationChange() const
{
    auto creationChange = QFrameGraphNodeCreatedChangePtr<QMemoryBarrierData>::create(this);
    QMemoryBarrierData &data = creationChange->data;
    Q_D(const QMemoryBarrier);
    data.waitOperations = d->m_waitOperations;
    return creationChange;
}


} // Qt3DRender

QT_END_NAMESPACE
