// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLVertexArrayObjectBase.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/WebGLRenderingContextBase.h"

namespace blink {

WebGLVertexArrayObjectBase::WebGLVertexArrayObjectBase(WebGLRenderingContextBase* ctx, VaoType type)
    : WebGLContextObject(ctx)
    , m_object(0)
    , m_type(type)
    , m_hasEverBeenBound(false)
    , m_destructionInProgress(false)
    , m_boundElementArrayBuffer(nullptr)
    , m_isAllEnabledAttribBufferBound(true)
{
    m_arrayBufferList.resize(ctx->maxVertexAttribs());
    m_attribEnabled.resize(ctx->maxVertexAttribs());
    for (size_t i = 0; i < m_attribEnabled.size(); ++i) {
        m_attribEnabled[i] = false;
    }

    switch (m_type) {
    case VaoTypeDefault:
        break;
    default:
        context()->contextGL()->GenVertexArraysOES(1, &m_object);
        break;
    }
}

WebGLVertexArrayObjectBase::~WebGLVertexArrayObjectBase()
{
    m_destructionInProgress = true;

    // Delete the platform framebuffer resource, in case
    // where this vertex array object isn't detached when it and
    // the WebGLRenderingContextBase object it is registered with
    // are both finalized.
    detachAndDeleteObject();
}

void WebGLVertexArrayObjectBase::dispatchDetached(gpu::gles2::GLES2Interface* gl)
{
    if (m_boundElementArrayBuffer)
        m_boundElementArrayBuffer->onDetached(gl);

    for (size_t i = 0; i < m_arrayBufferList.size(); ++i) {
        if (m_arrayBufferList[i])
            m_arrayBufferList[i]->onDetached(gl);
    }
}

void WebGLVertexArrayObjectBase::deleteObjectImpl(gpu::gles2::GLES2Interface* gl)
{
    switch (m_type) {
    case VaoTypeDefault:
        break;
    default:
        gl->DeleteVertexArraysOES(1, &m_object);
        m_object = 0;
        break;
    }

    // Member<> objects must not be accessed during the destruction,
    // since they could have been already finalized.
    // The finalizers of these objects will handle their detachment
    // by themselves.
    if (!m_destructionInProgress)
        dispatchDetached(gl);
}

void WebGLVertexArrayObjectBase::setElementArrayBuffer(WebGLBuffer* buffer)
{
    if (buffer)
        buffer->onAttached();
    if (m_boundElementArrayBuffer)
        m_boundElementArrayBuffer->onDetached(context()->contextGL());
    m_boundElementArrayBuffer = buffer;
}

WebGLBuffer* WebGLVertexArrayObjectBase::getArrayBufferForAttrib(size_t index)
{
    DCHECK(index < context()->maxVertexAttribs());
    return m_arrayBufferList[index].get();
}

void WebGLVertexArrayObjectBase::setArrayBufferForAttrib(GLuint index, WebGLBuffer* buffer)
{
    if (buffer)
        buffer->onAttached();
    if (m_arrayBufferList[index])
        m_arrayBufferList[index]->onDetached(context()->contextGL());

    m_arrayBufferList[index] = buffer;
    updateAttribBufferBoundStatus();
}

void WebGLVertexArrayObjectBase::setAttribEnabled(GLuint index, bool enabled)
{
    DCHECK(index < context()->maxVertexAttribs());
    m_attribEnabled[index] = enabled;
    updateAttribBufferBoundStatus();
}

bool WebGLVertexArrayObjectBase::getAttribEnabled(GLuint index) const
{
    DCHECK(index < context()->maxVertexAttribs());
    return m_attribEnabled[index];
}

void WebGLVertexArrayObjectBase::updateAttribBufferBoundStatus()
{
    m_isAllEnabledAttribBufferBound = true;
    for (size_t i = 0; i < m_attribEnabled.size(); ++i) {
        if (m_attribEnabled[i] && !m_arrayBufferList[i]) {
            m_isAllEnabledAttribBufferBound = false;
            return;
        }
    }
}

void WebGLVertexArrayObjectBase::unbindBuffer(WebGLBuffer* buffer)
{
    if (m_boundElementArrayBuffer == buffer) {
        m_boundElementArrayBuffer->onDetached(context()->contextGL());
        m_boundElementArrayBuffer = nullptr;
    }

    for (size_t i = 0; i < m_arrayBufferList.size(); ++i) {
        if (m_arrayBufferList[i] == buffer) {
            m_arrayBufferList[i]->onDetached(context()->contextGL());
            m_arrayBufferList[i] = nullptr;
        }
    }
    updateAttribBufferBoundStatus();
}

ScopedPersistent<v8::Array>* WebGLVertexArrayObjectBase::getPersistentCache()
{
    return &m_arrayBufferWrappers;
}

DEFINE_TRACE(WebGLVertexArrayObjectBase)
{
    visitor->trace(m_boundElementArrayBuffer);
    visitor->trace(m_arrayBufferList);
    WebGLContextObject::trace(visitor);
}

} // namespace blink
