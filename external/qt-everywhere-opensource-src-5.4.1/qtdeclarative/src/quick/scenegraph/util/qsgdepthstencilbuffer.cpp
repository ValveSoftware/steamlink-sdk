/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qsgdepthstencilbuffer_p.h"

QT_BEGIN_NAMESPACE

QSGDepthStencilBuffer::QSGDepthStencilBuffer(QOpenGLContext *context, const Format &format)
    : m_functions(context)
    , m_manager(0)
    , m_format(format)
    , m_depthBuffer(0)
    , m_stencilBuffer(0)
{
    // 'm_manager' is set by QSGDepthStencilBufferManager::insertBuffer().
}

QSGDepthStencilBuffer::~QSGDepthStencilBuffer()
{
    if (m_manager)
        m_manager->m_buffers.remove(m_format);
}

void QSGDepthStencilBuffer::attach()
{
    m_functions.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                          GL_RENDERBUFFER, m_depthBuffer);
    m_functions.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                          GL_RENDERBUFFER, m_stencilBuffer);
}

void QSGDepthStencilBuffer::detach()
{
    m_functions.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                          GL_RENDERBUFFER, 0);
    m_functions.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                          GL_RENDERBUFFER, 0);
}

#ifndef GL_DEPTH24_STENCIL8_OES
#define GL_DEPTH24_STENCIL8_OES 0x88F0
#endif

#ifndef GL_DEPTH_COMPONENT24_OES
#define GL_DEPTH_COMPONENT24_OES 0x81A6
#endif

QSGDefaultDepthStencilBuffer::QSGDefaultDepthStencilBuffer(QOpenGLContext *context, const Format &format)
    : QSGDepthStencilBuffer(context, format)
{
    const GLsizei width = format.size.width();
    const GLsizei height = format.size.height();

    if (format.attachments == (DepthAttachment | StencilAttachment)
            && m_functions.hasOpenGLExtension(QOpenGLExtensions::PackedDepthStencil))
    {
        m_functions.glGenRenderbuffers(1, &m_depthBuffer);
        m_functions.glBindRenderbuffer(GL_RENDERBUFFER, m_depthBuffer);
        if (format.samples && m_functions.hasOpenGLExtension(QOpenGLExtensions::FramebufferMultisample)) {
#if defined(QT_OPENGL_ES_2)
            m_functions.glRenderbufferStorageMultisample(GL_RENDERBUFFER, format.samples,
                GL_DEPTH24_STENCIL8_OES, width, height);
#else
            m_functions.glRenderbufferStorageMultisample(GL_RENDERBUFFER, format.samples,
                GL_DEPTH24_STENCIL8, width, height);
#endif
        } else {
#if defined(QT_OPENGL_ES_2)
            m_functions.glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, width, height);
#else
            m_functions.glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
#endif
        }
        m_stencilBuffer = m_depthBuffer;
    }
    if (!m_depthBuffer && (format.attachments & DepthAttachment)) {
        m_functions.glGenRenderbuffers(1, &m_depthBuffer);
        m_functions.glBindRenderbuffer(GL_RENDERBUFFER, m_depthBuffer);
        GLenum internalFormat = GL_DEPTH_COMPONENT;
        if (context->isOpenGLES())
            internalFormat = m_functions.hasOpenGLExtension(QOpenGLExtensions::Depth24)
                ? GL_DEPTH_COMPONENT24_OES : GL_DEPTH_COMPONENT16;
        if (format.samples && m_functions.hasOpenGLExtension(QOpenGLExtensions::FramebufferMultisample)) {
            m_functions.glRenderbufferStorageMultisample(GL_RENDERBUFFER, format.samples,
                internalFormat, width, height);
        } else {
            m_functions.glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, width, height);
        }
    }
    if (!m_stencilBuffer && (format.attachments & StencilAttachment)) {
        m_functions.glGenRenderbuffers(1, &m_stencilBuffer);
        m_functions.glBindRenderbuffer(GL_RENDERBUFFER, m_stencilBuffer);
#ifdef QT_OPENGL_ES
        const GLenum internalFormat = GL_STENCIL_INDEX8;
#else
        const GLenum internalFormat = context->isOpenGLES() ? GL_STENCIL_INDEX8 : GL_STENCIL_INDEX;
#endif
        if (format.samples && m_functions.hasOpenGLExtension(QOpenGLExtensions::FramebufferMultisample)) {
            m_functions.glRenderbufferStorageMultisample(GL_RENDERBUFFER, format.samples,
                internalFormat, width, height);
        } else {
            m_functions.glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, width, height);
        }
    }
}

QSGDefaultDepthStencilBuffer::~QSGDefaultDepthStencilBuffer()
{
    free();
}

void QSGDefaultDepthStencilBuffer::free()
{
    if (m_depthBuffer)
        m_functions.glDeleteRenderbuffers(1, &m_depthBuffer);
    if (m_stencilBuffer && m_stencilBuffer != m_depthBuffer)
        m_functions.glDeleteRenderbuffers(1, &m_stencilBuffer);
    m_depthBuffer = m_stencilBuffer = 0;
}


QSGDepthStencilBufferManager::~QSGDepthStencilBufferManager()
{
    for (Hash::const_iterator it = m_buffers.constBegin(); it != m_buffers.constEnd(); ++it) {
        QSGDepthStencilBuffer *buffer = it.value().data();
        buffer->free();
        buffer->m_manager = 0;
    }
}

QSharedPointer<QSGDepthStencilBuffer> QSGDepthStencilBufferManager::bufferForFormat(const QSGDepthStencilBuffer::Format &fmt)
{
    Hash::const_iterator it = m_buffers.constFind(fmt);
    if (it != m_buffers.constEnd())
        return it.value().toStrongRef();
    return QSharedPointer<QSGDepthStencilBuffer>();
}

void QSGDepthStencilBufferManager::insertBuffer(const QSharedPointer<QSGDepthStencilBuffer> &buffer)
{
    Q_ASSERT(buffer->m_manager == 0);
    Q_ASSERT(!m_buffers.contains(buffer->m_format));
    buffer->m_manager = this;
    m_buffers.insert(buffer->m_format, buffer.toWeakRef());
}

uint qHash(const QSGDepthStencilBuffer::Format &format)
{
    return qHash(qMakePair(format.size.width(), format.size.height()))
            ^ (uint(format.samples) << 12) ^ (uint(format.attachments) << 28);
}

QT_END_NAMESPACE
