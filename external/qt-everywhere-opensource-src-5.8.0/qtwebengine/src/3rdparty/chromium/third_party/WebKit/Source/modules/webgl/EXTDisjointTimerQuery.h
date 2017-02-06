// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTDisjointTimerQuery_h
#define EXTDisjointTimerQuery_h

#include "bindings/core/v8/ScriptValue.h"
#include "modules/webgl/WebGLExtension.h"
#include "wtf/HashMap.h"

namespace blink {

class WebGLRenderingContextBase;
class WebGLTimerQueryEXT;

class EXTDisjointTimerQuery final : public WebGLExtension {
    DEFINE_WRAPPERTYPEINFO();

public:
    static EXTDisjointTimerQuery* create(WebGLRenderingContextBase*);
    static bool supported(WebGLRenderingContextBase*);
    static const char* extensionName();

    ~EXTDisjointTimerQuery() override;
    WebGLExtensionName name() const override;

    WebGLTimerQueryEXT* createQueryEXT();
    void deleteQueryEXT(WebGLTimerQueryEXT*);
    GLboolean isQueryEXT(WebGLTimerQueryEXT*);
    void beginQueryEXT(GLenum, WebGLTimerQueryEXT*);
    void endQueryEXT(GLenum);
    void queryCounterEXT(WebGLTimerQueryEXT*, GLenum);
    ScriptValue getQueryEXT(ScriptState*, GLenum, GLenum);
    ScriptValue getQueryObjectEXT(ScriptState*, WebGLTimerQueryEXT*, GLenum);

    DECLARE_VIRTUAL_TRACE();

private:
    friend class WebGLTimerQueryEXT;
    explicit EXTDisjointTimerQuery(WebGLRenderingContextBase*);

    Member<WebGLTimerQueryEXT> m_currentElapsedQuery;
};

} // namespace blink

#endif // EXTDisjointTimerQuery_h
