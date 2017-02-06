// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLSampler.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/WebGL2RenderingContextBase.h"

namespace blink {

WebGLSampler* WebGLSampler::create(WebGL2RenderingContextBase* ctx)
{
    return new WebGLSampler(ctx);
}

WebGLSampler::~WebGLSampler()
{
    // See the comment in WebGLObject::detachAndDeleteObject().
    detachAndDeleteObject();
}

WebGLSampler::WebGLSampler(WebGL2RenderingContextBase* ctx)
    : WebGLSharedPlatform3DObject(ctx)
{
    GLuint sampler;
    ctx->contextGL()->GenSamplers(1, &sampler);
    setObject(sampler);
}

void WebGLSampler::deleteObjectImpl(gpu::gles2::GLES2Interface* gl)
{
    gl->DeleteSamplers(1, &m_object);
    m_object = 0;
}

} // namespace blink
