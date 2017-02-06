// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLVertexArrayObjectBase_h
#define WebGLVertexArrayObjectBase_h

#include "bindings/core/v8/ScopedPersistent.h"
#include "modules/webgl/WebGLBuffer.h"
#include "modules/webgl/WebGLContextObject.h"
#include "platform/heap/Handle.h"

namespace blink {

class WebGLVertexArrayObjectBase : public WebGLContextObject {
public:
    enum VaoType {
        VaoTypeDefault,
        VaoTypeUser,
    };

    ~WebGLVertexArrayObjectBase() override;

    GLuint object() const { return m_object; }

    bool isDefaultObject() const { return m_type == VaoTypeDefault; }

    bool hasEverBeenBound() const { return object() && m_hasEverBeenBound; }
    void setHasEverBeenBound() { m_hasEverBeenBound = true; }

    WebGLBuffer* boundElementArrayBuffer() const { return m_boundElementArrayBuffer; }
    void setElementArrayBuffer(WebGLBuffer*);

    WebGLBuffer* getArrayBufferForAttrib(size_t);
    void setArrayBufferForAttrib(GLuint, WebGLBuffer*);
    void setAttribEnabled(GLuint, bool);
    bool getAttribEnabled(GLuint) const;
    bool isAllEnabledAttribBufferBound() const { return m_isAllEnabledAttribBufferBound; }
    void unbindBuffer(WebGLBuffer*);

    ScopedPersistent<v8::Array>* getPersistentCache();

    DECLARE_VIRTUAL_TRACE();

protected:
    WebGLVertexArrayObjectBase(WebGLRenderingContextBase*, VaoType);

private:
    void dispatchDetached(gpu::gles2::GLES2Interface*);
    bool hasObject() const override { return m_object != 0; }
    void deleteObjectImpl(gpu::gles2::GLES2Interface*) override;

    void updateAttribBufferBoundStatus();

    GLuint m_object;

    VaoType m_type;
    bool m_hasEverBeenBound;
    bool m_destructionInProgress;
    Member<WebGLBuffer> m_boundElementArrayBuffer;
    HeapVector<Member<WebGLBuffer>> m_arrayBufferList;
    Vector<bool> m_attribEnabled;
    bool m_isAllEnabledAttribBufferBound;

    // For preserving the wrappers of WebGLBuffer objects latched in
    // via vertexAttribPointer calls.
    ScopedPersistent<v8::Array> m_arrayBufferWrappers;
};

} // namespace blink

#endif // WebGLVertexArrayObjectBase_h
