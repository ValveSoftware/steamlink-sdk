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

#include "buffer3d_p.h"

#include <QDebug>
#include <QString>

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

/*!
 * \qmltype Canvas3DBuffer
 * \since QtCanvas3D 1.0
 * \inqmlmodule QtCanvas3D
 * \inherits Canvas3DAbstractObject
 * \brief Contains an OpenGL buffer.
 *
 * An uncreatable QML type that contains an OpenGL buffer. You can get it by calling the
 * \l{Context3D::createBuffer()}{Context3D.createBuffer()} method.
 */

CanvasBuffer::CanvasBuffer() :
    CanvasAbstractObject(0, 0),
    m_bufferId(0),
    m_bindTarget(CanvasBuffer::UNINITIALIZED)
{
    // Compiler says we need the default constructor, but it is unclear why
}

CanvasBuffer::CanvasBuffer(CanvasGlCommandQueue *queue, QObject *parent) :
    CanvasAbstractObject(queue, parent),
    m_bufferId(queue->createResourceId()),
    m_bindTarget(CanvasBuffer::UNINITIALIZED)
{
    queueCommand(CanvasGlCommandQueue::glGenBuffers, m_bufferId);
}

CanvasBuffer::CanvasBuffer(const CanvasBuffer& other) :
    CanvasAbstractObject(other.commandQueue(), 0), // Copying a QObject, leave it parentless..
    m_bufferId(other.m_bufferId),
    m_bindTarget(other.m_bindTarget)
{
}

CanvasBuffer::~CanvasBuffer()
{
    // Crashes on exit as V4VM does it's final cleanup without checking of ownerships
    del();
}

void CanvasBuffer::del()
{
    if (m_bufferId)
        queueCommand(CanvasGlCommandQueue::glDeleteBuffers, m_bufferId);
    m_bufferId = 0;
}

bool CanvasBuffer::isAlive()
{
    return bool(m_bufferId);
}

CanvasBuffer::bindTarget CanvasBuffer::target()
{
    return m_bindTarget;
}

void CanvasBuffer::setTarget(bindTarget bindPoint)
{
    //Q_ASSERT(m_bindTarget == CanvasBuffer::UNINITIALIZED);

    m_bindTarget = bindPoint;
}

GLint CanvasBuffer::id()
{
    return m_bufferId;
}

QDebug operator<<(QDebug dbg, const CanvasBuffer *buffer)
{
    if (buffer)
        dbg.nospace() << "Canvas3DBuffer("<< buffer->name() <<", id:" << buffer->m_bufferId << ")";
    else
        dbg.nospace() << "Canvas3DBuffer("<< ((void*) buffer) <<")";
    return dbg.maybeSpace();
}

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE
