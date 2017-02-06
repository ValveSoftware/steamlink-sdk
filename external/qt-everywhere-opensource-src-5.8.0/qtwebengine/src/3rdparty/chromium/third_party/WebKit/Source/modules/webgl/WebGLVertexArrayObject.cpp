// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLVertexArrayObject.h"

#include "modules/webgl/WebGLRenderingContextBase.h"

namespace blink {

WebGLVertexArrayObject* WebGLVertexArrayObject::create(WebGLRenderingContextBase* ctx, VaoType type)
{
    return new WebGLVertexArrayObject(ctx, type);
}

WebGLVertexArrayObject::WebGLVertexArrayObject(WebGLRenderingContextBase* ctx, VaoType type)
    : WebGLVertexArrayObjectBase(ctx, type)
{
}

} // namespace blink
