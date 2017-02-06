/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webgl/WebGLProgram.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/WebGLContextGroup.h"
#include "modules/webgl/WebGLRenderingContextBase.h"

namespace blink {

WebGLProgram* WebGLProgram::create(WebGLRenderingContextBase* ctx)
{
    return new WebGLProgram(ctx);
}

WebGLProgram::WebGLProgram(WebGLRenderingContextBase* ctx)
    : WebGLSharedPlatform3DObject(ctx)
    , m_linkStatus(false)
    , m_linkCount(0)
    , m_activeTransformFeedbackCount(0)
    , m_infoValid(true)
{
    setObject(ctx->contextGL()->CreateProgram());
}

WebGLProgram::~WebGLProgram()
{
    // These heap objects handle detachment on their own. Clear out
    // the references to prevent deleteObjectImpl() from trying to do
    // same, as we cannot safely access other heap objects from this
    // destructor.
    m_vertexShader = nullptr;
    m_fragmentShader = nullptr;

    // See the comment in WebGLObject::detachAndDeleteObject().
    detachAndDeleteObject();
}

void WebGLProgram::deleteObjectImpl(gpu::gles2::GLES2Interface* gl)
{
    gl->DeleteProgram(m_object);
    m_object = 0;
    if (m_vertexShader) {
        m_vertexShader->onDetached(gl);
        m_vertexShader = nullptr;
    }
    if (m_fragmentShader) {
        m_fragmentShader->onDetached(gl);
        m_fragmentShader = nullptr;
    }
}

bool WebGLProgram::linkStatus(WebGLRenderingContextBase* context)
{
    cacheInfoIfNeeded(context);
    return m_linkStatus;
}

void WebGLProgram::increaseLinkCount()
{
    ++m_linkCount;
    m_infoValid = false;
}

void WebGLProgram::increaseActiveTransformFeedbackCount()
{
    ++m_activeTransformFeedbackCount;
}

void WebGLProgram::decreaseActiveTransformFeedbackCount()
{
    --m_activeTransformFeedbackCount;
}

WebGLShader* WebGLProgram::getAttachedShader(GLenum type)
{
    switch (type) {
    case GL_VERTEX_SHADER:
        return m_vertexShader;
    case GL_FRAGMENT_SHADER:
        return m_fragmentShader;
    default:
        return 0;
    }
}

bool WebGLProgram::attachShader(WebGLShader* shader)
{
    if (!shader || !shader->object())
        return false;
    switch (shader->type()) {
    case GL_VERTEX_SHADER:
        if (m_vertexShader)
            return false;
        m_vertexShader = shader;
        return true;
    case GL_FRAGMENT_SHADER:
        if (m_fragmentShader)
            return false;
        m_fragmentShader = shader;
        return true;
    default:
        return false;
    }
}

bool WebGLProgram::detachShader(WebGLShader* shader)
{
    if (!shader || !shader->object())
        return false;
    switch (shader->type()) {
    case GL_VERTEX_SHADER:
        if (m_vertexShader != shader)
            return false;
        m_vertexShader = nullptr;
        return true;
    case GL_FRAGMENT_SHADER:
        if (m_fragmentShader != shader)
            return false;
        m_fragmentShader = nullptr;
        return true;
    default:
        return false;
    }
}

ScopedPersistent<v8::Array>* WebGLProgram::getPersistentCache()
{
    return &m_shaderWrappers;
}

void WebGLProgram::cacheInfoIfNeeded(WebGLRenderingContextBase* context)
{
    if (m_infoValid)
        return;
    if (!m_object)
        return;
    gpu::gles2::GLES2Interface* gl = context->contextGL();
    m_linkStatus = 0;
    gl->GetProgramiv(m_object, GL_LINK_STATUS, &m_linkStatus);
    m_infoValid = true;
}

DEFINE_TRACE(WebGLProgram)
{
    visitor->trace(m_vertexShader);
    visitor->trace(m_fragmentShader);
    WebGLSharedPlatform3DObject::trace(visitor);
}

} // namespace blink
