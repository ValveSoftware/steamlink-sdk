// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLTransformFeedback.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/WebGL2RenderingContextBase.h"

namespace blink {

WebGLTransformFeedback* WebGLTransformFeedback::create(WebGL2RenderingContextBase* ctx)
{
    return new WebGLTransformFeedback(ctx);
}

WebGLTransformFeedback::~WebGLTransformFeedback()
{
    // See the comment in WebGLObject::detachAndDeleteObject().
    detachAndDeleteObject();
}

WebGLTransformFeedback::WebGLTransformFeedback(WebGL2RenderingContextBase* ctx)
    : WebGLSharedPlatform3DObject(ctx)
    , m_target(0)
    , m_program(nullptr)
{
    GLuint tf;
    ctx->contextGL()->GenTransformFeedbacks(1, &tf);
    setObject(tf);
}

void WebGLTransformFeedback::deleteObjectImpl(gpu::gles2::GLES2Interface* gl)
{
    gl->DeleteTransformFeedbacks(1, &m_object);
    m_object = 0;
}

void WebGLTransformFeedback::setTarget(GLenum target)
{
    if (m_target)
        return;
    if (target == GL_TRANSFORM_FEEDBACK)
        m_target = target;
}

void WebGLTransformFeedback::setProgram(WebGLProgram* program)
{
    m_program = program;
}

DEFINE_TRACE(WebGLTransformFeedback)
{
    visitor->trace(m_program);
    WebGLSharedPlatform3DObject::trace(visitor);
}

} // namespace blink
