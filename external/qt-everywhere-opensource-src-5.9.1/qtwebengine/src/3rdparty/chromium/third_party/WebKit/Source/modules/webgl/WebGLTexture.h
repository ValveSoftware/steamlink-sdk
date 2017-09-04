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

#ifndef WebGLTexture_h
#define WebGLTexture_h

#include "modules/webgl/WebGLSharedPlatform3DObject.h"

namespace blink {

class WebGLTexture final : public WebGLSharedPlatform3DObject {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~WebGLTexture() override;

  static WebGLTexture* create(WebGLRenderingContextBase*);

  void setTarget(GLenum);

  GLenum getTarget() const { return m_target; }

  static GLenum getValidFormatForInternalFormat(GLenum);

  bool hasEverBeenBound() const { return object() && m_target; }

  static GLint computeLevelCount(GLsizei width, GLsizei height, GLsizei depth);

 private:
  explicit WebGLTexture(WebGLRenderingContextBase*);

  void deleteObjectImpl(gpu::gles2::GLES2Interface*) override;

  bool isTexture() const override { return true; }

  int mapTargetToIndex(GLenum) const;

  GLenum m_target;
};

}  // namespace blink

#endif  // WebGLTexture_h
