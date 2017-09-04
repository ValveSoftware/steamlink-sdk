/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebGLFramebuffer_h
#define WebGLFramebuffer_h

#include "bindings/core/v8/ScriptWrappableVisitor.h"
#include "modules/webgl/WebGLContextObject.h"
#include "modules/webgl/WebGLSharedObject.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace blink {

class WebGLRenderbuffer;
class WebGLTexture;

class WebGLFramebuffer final : public WebGLContextObject {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class WebGLAttachment : public GarbageCollectedFinalized<WebGLAttachment> {
   public:
    virtual ~WebGLAttachment();

    virtual WebGLSharedObject* object() const = 0;
    virtual bool isSharedObject(WebGLSharedObject*) const = 0;
    virtual bool valid() const = 0;
    virtual void onDetached(gpu::gles2::GLES2Interface*) = 0;
    virtual void attach(gpu::gles2::GLES2Interface*,
                        GLenum target,
                        GLenum attachment) = 0;
    virtual void unattach(gpu::gles2::GLES2Interface*,
                          GLenum target,
                          GLenum attachment) = 0;

    DEFINE_INLINE_VIRTUAL_TRACE() {}

   protected:
    WebGLAttachment();
  };

  ~WebGLFramebuffer() override;

  static WebGLFramebuffer* create(WebGLRenderingContextBase*);

  GLuint object() const { return m_object; }

  void setAttachmentForBoundFramebuffer(GLenum target,
                                        GLenum attachment,
                                        GLenum texTarget,
                                        WebGLTexture*,
                                        GLint level,
                                        GLint layer);
  void setAttachmentForBoundFramebuffer(GLenum target,
                                        GLenum attachment,
                                        WebGLRenderbuffer*);
  // If an object is attached to the currently bound framebuffer, remove it.
  void removeAttachmentFromBoundFramebuffer(GLenum target, WebGLSharedObject*);
  WebGLSharedObject* getAttachmentObject(GLenum) const;

  // WebGL 1 specific:
  //   1) can't allow depth_stencil for depth/stencil attachments, and vice
  //      versa.
  //   2) no conflicting DEPTH/STENCIL/DEPTH_STENCIL attachments.
  GLenum checkDepthStencilStatus(const char** reason) const;

  bool hasEverBeenBound() const { return object() && m_hasEverBeenBound; }

  void setHasEverBeenBound() { m_hasEverBeenBound = true; }

  bool hasStencilBuffer() const;

  // Wrapper for drawBuffersEXT/drawBuffersARB to work around a driver bug.
  void drawBuffers(const Vector<GLenum>& bufs);

  GLenum getDrawBuffer(GLenum);

  void readBuffer(const GLenum colorBuffer) { m_readBuffer = colorBuffer; }

  GLenum getReadBuffer() const { return m_readBuffer; }

  virtual void visitChildDOMWrappers(v8::Isolate*,
                                     const v8::Persistent<v8::Object>&);

  DECLARE_VIRTUAL_TRACE();
  DECLARE_VIRTUAL_TRACE_WRAPPERS();

 protected:
  explicit WebGLFramebuffer(WebGLRenderingContextBase*);

  bool hasObject() const override { return m_object != 0; }
  void deleteObjectImpl(gpu::gles2::GLES2Interface*) override;

 private:
  WebGLAttachment* getAttachment(GLenum attachment) const;

  // Check if the framebuffer is currently bound.
  bool isBound(GLenum target) const;

  // Check if a new drawBuffers call should be issued. This is called when we
  // add or remove an attachment.
  void drawBuffersIfNecessary(bool force);

  void setAttachmentInternal(GLenum target,
                             GLenum attachment,
                             GLenum texTarget,
                             WebGLTexture*,
                             GLint level,
                             GLint layer);
  void setAttachmentInternal(GLenum target,
                             GLenum attachment,
                             WebGLRenderbuffer*);
  // If a given attachment point for the currently bound framebuffer is not
  // null, remove the attached object.
  void removeAttachmentInternal(GLenum target, GLenum attachment);

  void commitWebGL1DepthStencilIfConsistent(GLenum target);

  GLuint m_object;

  typedef HeapHashMap<GLenum, TraceWrapperMember<WebGLAttachment>>
      AttachmentMap;

  AttachmentMap m_attachments;
  bool m_destructionInProgress;

  bool m_hasEverBeenBound;
  bool m_webGL1DepthStencilConsistent;

  Vector<GLenum> m_drawBuffers;
  Vector<GLenum> m_filteredDrawBuffers;

  GLenum m_readBuffer;
};

}  // namespace blink

#endif  // WebGLFramebuffer_h
