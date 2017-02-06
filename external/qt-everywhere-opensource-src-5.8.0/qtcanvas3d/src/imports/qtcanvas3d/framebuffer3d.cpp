/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
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

#include "framebuffer3d_p.h"

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

/*!
 * \qmltype Canvas3DFrameBuffer
 * \since QtCanvas3D 1.0
 * \inqmlmodule QtCanvas3D
 * \inherits Canvas3DAbstractObject
 * \brief Contains an OpenGL framebuffer.
 *
 * An uncreatable QML type that contains an OpenGL framebuffer object. You can get it by calling the
 * \l{Context3D::createFramebuffer()}{Context3D.createFramebuffer()} method.
 */

CanvasFrameBuffer::CanvasFrameBuffer(CanvasGlCommandQueue *queue, QObject *parent) :
    CanvasAbstractObject(queue, parent),
    m_framebufferId(queue->createResourceId()),
    m_texture(0)
{
    queueCommand(CanvasGlCommandQueue::glGenFramebuffers, m_framebufferId);
}

CanvasFrameBuffer::~CanvasFrameBuffer()
{
    del();
}

bool CanvasFrameBuffer::isAlive()
{
    return bool(m_framebufferId);
}

void CanvasFrameBuffer::del()
{
    if (m_framebufferId) {
        queueCommand(CanvasGlCommandQueue::glDeleteFramebuffers, m_framebufferId);
        m_framebufferId = 0;
    }
}

GLint CanvasFrameBuffer::id()
{
    return m_framebufferId;
}

void CanvasFrameBuffer::setTexture(CanvasTexture *texture)
{
    m_texture = texture;
}

CanvasTexture *CanvasFrameBuffer::texture()
{
    return m_texture;
}

QDebug operator<<(QDebug dbg, const CanvasFrameBuffer *buffer)
{
    if (buffer)
        dbg.nospace() << "Canvas3DFrameBuffer("<< buffer->name() <<", id:" << buffer->m_framebufferId << ")";
    else
        dbg.nospace() << "Canvas3DFrameBuffer("<< ((void*) buffer) <<")";
    return dbg.maybeSpace();
}

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE
