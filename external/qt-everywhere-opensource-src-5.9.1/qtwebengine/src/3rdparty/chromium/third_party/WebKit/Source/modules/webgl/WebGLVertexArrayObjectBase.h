// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLVertexArrayObjectBase_h
#define WebGLVertexArrayObjectBase_h

#include "bindings/core/v8/TraceWrapperMember.h"
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

  WebGLBuffer* boundElementArrayBuffer() const {
    return m_boundElementArrayBuffer;
  }
  void setElementArrayBuffer(WebGLBuffer*);

  WebGLBuffer* getArrayBufferForAttrib(size_t);
  void setArrayBufferForAttrib(GLuint, WebGLBuffer*);
  void setAttribEnabled(GLuint, bool);
  bool getAttribEnabled(GLuint) const;
  bool isAllEnabledAttribBufferBound() const {
    return m_isAllEnabledAttribBufferBound;
  }
  void unbindBuffer(WebGLBuffer*);

  virtual void visitChildDOMWrappers(v8::Isolate*,
                                     const v8::Persistent<v8::Object>&);

  DECLARE_VIRTUAL_TRACE();
  DECLARE_VIRTUAL_TRACE_WRAPPERS();

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
  TraceWrapperMember<WebGLBuffer> m_boundElementArrayBuffer;
  HeapVector<TraceWrapperMember<WebGLBuffer>> m_arrayBufferList;
  Vector<bool> m_attribEnabled;
  bool m_isAllEnabledAttribBufferBound;
};

}  // namespace blink

#endif  // WebGLVertexArrayObjectBase_h
