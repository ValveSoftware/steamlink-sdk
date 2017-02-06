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

#include "renderbuffer3d_p.h"

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

/*!
 * \qmltype Canvas3DRenderBuffer
 * \since QtCanvas3D 1.0
 * \inqmlmodule QtCanvas3D
 * \inherits Canvas3DAbstractObject
 * \brief Contains an OpenGL renderbuffer.
 *
 * An uncreatable QML type that contains an OpenGL renderbuffer. You can get it by calling
 * the \l{Context3D::createRenderbuffer()}{Context3D.createRenderbuffer()} method.
 */

CanvasRenderBuffer::CanvasRenderBuffer(CanvasGlCommandQueue *queue,
                                       bool initSecondaryId, QObject *parent) :
    CanvasAbstractObject(queue, parent),
    m_renderbufferId(queue->createResourceId()),
    m_secondaryId(0)
{
    queueCommand(CanvasGlCommandQueue::glGenRenderbuffers, m_renderbufferId);
    if (initSecondaryId) {
        m_secondaryId = queue->createResourceId();
        queueCommand(CanvasGlCommandQueue::glGenRenderbuffers, m_secondaryId);
    }
}


CanvasRenderBuffer::~CanvasRenderBuffer()
{
    del();
}

bool CanvasRenderBuffer::isAlive()
{
    return bool(m_renderbufferId);
}

void CanvasRenderBuffer::del()
{
    if (m_renderbufferId) {
        queueCommand(CanvasGlCommandQueue::glDeleteRenderbuffers, m_renderbufferId);
        if (m_secondaryId) {
            queueCommand(CanvasGlCommandQueue::glDeleteRenderbuffers, m_secondaryId);
            m_secondaryId = 0;
        }
        m_renderbufferId = 0;
    }
}

GLint CanvasRenderBuffer::id()
{
    return m_renderbufferId;
}

GLint CanvasRenderBuffer::secondaryId()
{
    return m_secondaryId;
}

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE
