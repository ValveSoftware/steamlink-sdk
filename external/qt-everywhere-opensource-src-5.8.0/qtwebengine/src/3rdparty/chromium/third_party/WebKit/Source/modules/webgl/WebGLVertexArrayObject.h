// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLVertexArrayObject_h
#define WebGLVertexArrayObject_h

#include "modules/webgl/WebGLVertexArrayObjectBase.h"

namespace blink {

class WebGLVertexArrayObject final : public WebGLVertexArrayObjectBase {
    DEFINE_WRAPPERTYPEINFO();
public:
    static WebGLVertexArrayObject* create(WebGLRenderingContextBase*, VaoType);

private:
    explicit WebGLVertexArrayObject(WebGLRenderingContextBase*, VaoType);
};

} // namespace blink

#endif // WebGLVertexArrayObject_h
