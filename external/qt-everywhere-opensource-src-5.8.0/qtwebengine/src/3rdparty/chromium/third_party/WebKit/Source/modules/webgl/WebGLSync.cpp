// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLSync.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/WebGL2RenderingContextBase.h"

namespace blink {

WebGLSync::~WebGLSync()
{
    // See the comment in WebGLObject::detachAndDeleteObject().
    detachAndDeleteObject();
}

WebGLSync::WebGLSync(WebGL2RenderingContextBase* ctx, GLsync object, GLenum objectType)
    : WebGLSharedObject(ctx)
    , m_object(object)
    , m_objectType(objectType)
{
}

void WebGLSync::deleteObjectImpl(gpu::gles2::GLES2Interface* gl)
{
    gl->DeleteSync(m_object);
    m_object = 0;
}

} // namespace blink
