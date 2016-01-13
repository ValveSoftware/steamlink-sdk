// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPS_JS_BINDINGS_GL_CONTEXT_H_
#define MOJO_APPS_JS_BINDINGS_GL_CONTEXT_H_

#include <GLES2/gl2.h>

#include "base/memory/weak_ptr.h"
#include "gin/handle.h"
#include "gin/public/wrapper_info.h"
#include "gin/runner.h"
#include "gin/wrappable.h"
#include "mojo/bindings/js/handle.h"
#include "mojo/public/c/gles2/gles2.h"
#include "v8/include/v8.h"

namespace gin {
class Arguments;
class ArrayBufferView;
}

namespace mojo {
namespace js {
namespace gl {

// Context implements WebGLRenderingContext.
class Context : public gin::Wrappable<Context> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  // TODO(piman): draw animation frame callback.
  static gin::Handle<Context> Create(
      v8::Isolate* isolate,
      mojo::Handle handle,
      v8::Handle<v8::Function> context_lost_callback);

  static void BufferData(GLenum target, const gin::ArrayBufferView& buffer,
                         GLenum usage);
  static void CompileShader(const gin::Arguments& args, GLuint shader);
  static GLuint CreateBuffer();
  static void DrawElements(GLenum mode, GLsizei count, GLenum type,
                           uint64_t indices);
  static GLint GetAttribLocation(GLuint program, const std::string& name);
  static std::string GetProgramInfoLog(GLuint program);
  static std::string GetShaderInfoLog(GLuint shader);
  static GLint GetUniformLocation(GLuint program, const std::string& name);
  static void ShaderSource(GLuint shader, const std::string& source);
  static void UniformMatrix4fv(GLint location, GLboolean transpose,
                               const gin::ArrayBufferView& buffer);
  static void VertexAttribPointer(GLuint index, GLint size, GLenum type,
                                  GLboolean normalized, GLsizei stride,
                                  uint64_t offset);

 private:
  virtual gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) OVERRIDE;

  explicit Context(v8::Isolate* isolate,
                   mojo::Handle handle,
                   v8::Handle<v8::Function> context_lost_callback);
  virtual ~Context();

  void ContextLost();
  static void ContextLostThunk(void* closure);

  base::WeakPtr<gin::Runner> runner_;
  v8::Persistent<v8::Function> context_lost_callback_;
  MojoGLES2Context context_;
};

}  // namespace gl
}  // namespace js
}  // namespace mojo

#endif  // MOJO_APPS_JS_BINDINGS_GL_CONTEXT_H_
