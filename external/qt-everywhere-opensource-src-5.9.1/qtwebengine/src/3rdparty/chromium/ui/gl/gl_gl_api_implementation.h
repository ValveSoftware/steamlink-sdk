// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_GL_API_IMPLEMENTATION_H_
#define UI_GL_GL_GL_API_IMPLEMENTATION_H_

#include <vector>

#include "base/compiler_specific.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_export.h"

namespace base {
class CommandLine;
}

namespace gl {

class GLContext;
struct GLVersionInfo;

GL_EXPORT void InitializeStaticGLBindingsGL();
void InitializeDynamicGLBindingsGL(GLContext* context);
GL_EXPORT void InitializeDebugGLBindingsGL();
void InitializeNullDrawGLBindingsGL();
// TODO(danakj): Remove this when all test suites are using null-draw.
bool HasInitializedNullDrawGLBindingsGL();
bool SetNullDrawGLBindingsEnabledGL(bool enabled);
GL_EXPORT void ClearGLBindingsGL();
void SetGLToRealGLApi();
void SetGLToStubGLApi();
void SetGLApi(GLApi* api);
void SetGLApiToNoContext();
GLApi* GetCurrentGLApi();
GL_EXPORT void SetStubGLApi(GLApi* api);
const GLVersionInfo* GetGLVersionInfo();

class GL_EXPORT GLApiBase : public GLApi {
 public:
  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "gl_bindings_api_autogen_gl.h"

 protected:
  GLApiBase();
  ~GLApiBase() override;
  void InitializeBase(DriverGL* driver);

  DriverGL* driver_;
};

// Implemenents the GL API by calling directly into the driver.
class GL_EXPORT RealGLApi : public GLApiBase {
 public:
  RealGLApi();
  ~RealGLApi() override;
  void Initialize(DriverGL* driver);
  void InitializeWithCommandLine(DriverGL* driver,
                                 base::CommandLine* command_line);

  void glGetIntegervFn(GLenum pname, GLint* params) override;
  const GLubyte* glGetStringFn(GLenum name) override;
  const GLubyte* glGetStringiFn(GLenum name, GLuint index) override;

  void InitializeFilteredExtensions();

 private:
  // Filtered GL_EXTENSIONS we return to glGetString(i) calls.
  std::vector<std::string> disabled_exts_;
  std::vector<std::string> filtered_exts_;
  std::string filtered_exts_str_;

#if DCHECK_IS_ON()
  bool filtered_exts_initialized_;
#endif
};

// Inserts a TRACE for every GL call.
class TraceGLApi : public GLApi {
 public:
  TraceGLApi(GLApi* gl_api) : gl_api_(gl_api) { }
  ~TraceGLApi() override;

  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "gl_bindings_api_autogen_gl.h"

 private:
  GLApi* gl_api_;
};

// Catches incorrect usage when GL calls are made without a current context.
class NoContextGLApi : public GLApi {
 public:
  NoContextGLApi();
  ~NoContextGLApi() override;

  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "gl_bindings_api_autogen_gl.h"
};

}  // namespace gl

#endif  // UI_GL_GL_GL_API_IMPLEMENTATION_H_
