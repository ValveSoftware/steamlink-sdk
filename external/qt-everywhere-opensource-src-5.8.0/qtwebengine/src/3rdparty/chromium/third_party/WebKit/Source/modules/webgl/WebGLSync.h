// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLSync_h
#define WebGLSync_h

#include "modules/webgl/WebGLSharedObject.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace blink {

class WebGL2RenderingContextBase;

class WebGLSync : public WebGLSharedObject {
    DEFINE_WRAPPERTYPEINFO();
public:
    ~WebGLSync() override;

    GLsync object() const { return m_object; }

protected:
    WebGLSync(WebGL2RenderingContextBase*, GLsync, GLenum objectType);

    bool hasObject() const override { return m_object != nullptr; }
    void deleteObjectImpl(gpu::gles2::GLES2Interface*) override;

    GLenum objectType() const { return m_objectType; }

private:
    bool isSync() const override { return true; }

    GLsync m_object;
    GLenum m_objectType;
};

} // namespace blink

#endif // WebGLSync_h
