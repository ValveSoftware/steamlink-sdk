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

#include "glbuffer_p.h"
#include <private/graphicscontext_p.h>

#if !defined(GL_UNIFORM_BUFFER)
#define GL_UNIFORM_BUFFER 0x8A11
#endif
#if !defined(GL_ARRAY_BUFFER)
#define GL_ARRAY_BUFFER 0x8892
#endif
#if !defined(GL_ELEMENT_ARRAY_BUFFER)
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#endif
#if !defined(GL_SHADER_STORAGE_BUFFER)
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif
#if !defined(GL_PIXEL_PACK_BUFFER)
#define GL_PIXEL_PACK_BUFFER 0x88EB
#endif
#if !defined(GL_PIXEL_UNPACK_BUFFER)
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#endif

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

// A UBO is created for each ShaderData Shader Pair
// That means a UBO is unique to a shader/shaderdata

namespace {

GLenum glBufferTypes[] = {
    GL_ARRAY_BUFFER,
    GL_UNIFORM_BUFFER,
    GL_ELEMENT_ARRAY_BUFFER,
    GL_SHADER_STORAGE_BUFFER,
    GL_PIXEL_PACK_BUFFER,
    GL_PIXEL_UNPACK_BUFFER
};

} // anonymous

GLBuffer::GLBuffer()
    : m_bufferId(~0)
    , m_isCreated(false)
    , m_bound(false)
    , m_lastTarget(GL_ARRAY_BUFFER)
{
}

bool GLBuffer::bind(GraphicsContext *ctx, Type t)
{
    if (m_bufferId == 0)
        return false;
    m_lastTarget = glBufferTypes[t];
    ctx->openGLContext()->functions()->glBindBuffer(m_lastTarget, m_bufferId);
    m_bound = true;
    return true;
}

bool GLBuffer::release(GraphicsContext *ctx)
{
    m_bound = false;
    ctx->openGLContext()->functions()->glBindBuffer(m_lastTarget, 0);
    return true;
}

bool GLBuffer::create(GraphicsContext *ctx)
{
    ctx->openGLContext()->functions()->glGenBuffers(1, &m_bufferId);
    m_isCreated = true;
    return m_bufferId != 0;
}

void GLBuffer::destroy(GraphicsContext *ctx)
{
    ctx->openGLContext()->functions()->glDeleteBuffers(1, &m_bufferId);
    m_isCreated = false;
}

void GLBuffer::allocate(GraphicsContext *ctx, uint size, bool dynamic)
{
    // Either GL_STATIC_DRAW OR GL_DYNAMIC_DRAW depending on  the use case
    // TO DO: find a way to know how a buffer/QShaderData will be used to use the right usage
    ctx->openGLContext()->functions()->glBufferData(m_lastTarget, size, NULL, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
}

void GLBuffer::allocate(GraphicsContext *ctx, const void *data, uint size, bool dynamic)
{
    ctx->openGLContext()->functions()->glBufferData(m_lastTarget, size, data, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
}

void GLBuffer::update(GraphicsContext *ctx, const void *data, uint size, int offset)
{
    ctx->openGLContext()->functions()->glBufferSubData(m_lastTarget, offset, size, data);
}

void GLBuffer::bindBufferBase(GraphicsContext *ctx, int bindingPoint, GLBuffer::Type t)
{
    ctx->bindBufferBase(glBufferTypes[t], bindingPoint, m_bufferId);
}

void GLBuffer::bindBufferBase(GraphicsContext *ctx, int bindingPoint)
{
    ctx->bindBufferBase(m_lastTarget, bindingPoint, m_bufferId);
}

} // namespace Render

} // namespace Qt3DRender

QT_END_NAMESPACE
