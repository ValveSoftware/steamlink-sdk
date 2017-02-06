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

#include "openglvertexarrayobject_p.h"
#include <Qt3DRender/private/graphicscontext_p.h>
#include <Qt3DRender/private/renderer_p.h>
#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/managers_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

OpenGLVertexArrayObject::OpenGLVertexArrayObject()
    : m_ctx(nullptr)
    , m_specified(false)
    , m_supportsVao(false)
    , m_createdEmulatedVAO(false)
{}

void OpenGLVertexArrayObject::setGraphicsContext(GraphicsContext *ctx)
{
    m_ctx = ctx;
    m_supportsVao = m_ctx->supportsVAO();
}

void OpenGLVertexArrayObject::bind()
{
    Q_ASSERT(m_ctx);
    if (m_supportsVao) {
        Q_ASSERT(!m_vao.isNull());
        Q_ASSERT(m_vao->isCreated());
        m_vao->bind();
    } else {
        // Unbind any other VAO that may have been bound and not released correctly
        if (m_ctx->m_currentVAO != nullptr && m_ctx->m_currentVAO != this)
            m_ctx->m_currentVAO->release();

        m_ctx->m_currentVAO = this;
        // We need to specify array and vertex attributes
        for (const GraphicsContext::VAOVertexAttribute &attr : m_vertexAttributes)
            m_ctx->enableAttribute(attr);
        if (!m_indexAttribute.isNull())
            m_ctx->bindGLBuffer(m_ctx->m_renderer->nodeManagers()->glBufferManager()->data(m_indexAttribute),
                                GLBuffer::IndexBuffer);
    }
}

void OpenGLVertexArrayObject::release()
{
    Q_ASSERT(m_ctx);
    if (m_supportsVao) {
        Q_ASSERT(!m_vao.isNull());
        Q_ASSERT(m_vao->isCreated());
        m_vao->release();
    } else {
        if (m_ctx->m_currentVAO == this) {
            for (const GraphicsContext::VAOVertexAttribute &attr : m_vertexAttributes)
                m_ctx->disableAttribute(attr);
            m_ctx->m_currentVAO = nullptr;
        }
    }
}

void OpenGLVertexArrayObject::create()
{
    Q_ASSERT(m_ctx);
    if (m_supportsVao) {
        Q_ASSERT(!m_vao.isNull());
        m_vao->create();
    } else {
        m_createdEmulatedVAO = true;
    }
}

bool OpenGLVertexArrayObject::isCreated() const
{
    if (m_supportsVao) {
        Q_ASSERT(!m_vao.isNull());
        return m_vao->isCreated();
    } else {
        return m_createdEmulatedVAO;
    }
}

void OpenGLVertexArrayObject::saveVertexAttribute(const GraphicsContext::VAOVertexAttribute &attr)
{
    // Remove any vertexAttribute already at location
    for (auto i = m_vertexAttributes.size() - 1; i >= 0; --i) {
        if (m_vertexAttributes.at(i).location == attr.location) {
            m_vertexAttributes.removeAt(i);
            break;
        }
    }
    m_vertexAttributes.push_back(attr);
}


} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
