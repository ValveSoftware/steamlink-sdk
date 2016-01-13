// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_CONTEXT_STUB_WITH_EXTENSIONS_H_
#define UI_GL_GL_CONTEXT_STUB_WITH_EXTENSIONS_H_

#include "ui/gl/gl_context_stub.h"

namespace gfx {

// Lightweight GLContext stub implementation that returns a constructed
// extensions string.  We use this to create a context that we can use to
// initialize GL extensions with, without actually creating a platform context.
class GL_EXPORT GLContextStubWithExtensions : public gfx::GLContextStub {
 public:
  GLContextStubWithExtensions() {}
  virtual std::string GetExtensions() OVERRIDE;

  void AddExtensionsString(const char* extensions);
  void SetGLVersionString(const char* version_str);

 protected:
  virtual std::string GetGLVersion() OVERRIDE;

  virtual ~GLContextStubWithExtensions() {}

 private:
  std::string extensions_;
  std::string version_str_;

  DISALLOW_COPY_AND_ASSIGN(GLContextStubWithExtensions);
};

}  // namespace gfx

#endif  // UI_GL_GL_CONTEXT_STUB_WITH_EXTENSIONS_H_
