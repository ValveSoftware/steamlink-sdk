// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLTransformFeedback_h
#define WebGLTransformFeedback_h

#include "modules/webgl/WebGLProgram.h"
#include "modules/webgl/WebGLSharedPlatform3DObject.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class WebGL2RenderingContextBase;

class WebGLTransformFeedback : public WebGLSharedPlatform3DObject {
    DEFINE_WRAPPERTYPEINFO();
public:
    ~WebGLTransformFeedback() override;

    static WebGLTransformFeedback* create(WebGL2RenderingContextBase*);

    GLenum getTarget() const { return m_target; }
    void setTarget(GLenum);

    bool hasEverBeenBound() const { return object() && m_target; }

    WebGLProgram* getProgram() const { return m_program; }
    void setProgram(WebGLProgram*);

    DECLARE_TRACE();

protected:
    explicit WebGLTransformFeedback(WebGL2RenderingContextBase*);

    void deleteObjectImpl(gpu::gles2::GLES2Interface*) override;

private:
    bool isTransformFeedback() const override { return true; }

    GLenum m_target;

    Member<WebGLProgram> m_program;
};

} // namespace blink

#endif // WebGLTransformFeedback_h
