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

#include "modules/webgl/WebGLObject.h"

#include "modules/webgl/WebGLRenderingContextBase.h"

namespace blink {

WebGLObject::WebGLObject(WebGLRenderingContextBase*)
    : m_attachmentCount(0)
    , m_deleted(false)
{
}

WebGLObject::~WebGLObject()
{
    // Verify that platform objects have been explicitly deleted.
    ASSERT(m_deleted);
}

void WebGLObject::deleteObject(gpu::gles2::GLES2Interface* gl)
{
    m_deleted = true;
    if (!hasObject())
        return;

    if (!hasGroupOrContext())
        return;

    if (!m_attachmentCount) {
        if (!gl)
            gl = getAGLInterface();
        if (gl) {
            deleteObjectImpl(gl);
            // Ensure the inherited class no longer claims to have a valid object
            ASSERT(!hasObject());
        }
    }
}

void WebGLObject::detach()
{
    m_attachmentCount = 0; // Make sure OpenGL resource is deleted.
}

void WebGLObject::detachAndDeleteObject()
{
    // To ensure that all platform objects are deleted after being detached,
    // this method does them together.
    //
    // The individual WebGL destructors need to call detachAndDeleteObject()
    // rather than do it based on Oilpan GC.
    detach();
    deleteObject(nullptr);
}

void WebGLObject::onDetached(gpu::gles2::GLES2Interface* gl)
{
    if (m_attachmentCount)
        --m_attachmentCount;
    if (m_deleted)
        deleteObject(gl);
}

} // namespace blink
