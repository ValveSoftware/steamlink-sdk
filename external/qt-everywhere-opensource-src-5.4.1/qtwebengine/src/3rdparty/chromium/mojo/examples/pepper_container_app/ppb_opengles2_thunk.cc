// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "mojo/examples/pepper_container_app/graphics_3d_resource.h"
#include "mojo/examples/pepper_container_app/thunk.h"
#include "mojo/public/c/gles2/gles2.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_graphics_3d_api.h"

namespace mojo {
namespace examples {

namespace {

typedef ppapi::thunk::EnterResource<ppapi::thunk::PPB_Graphics3D_API> Enter3D;

bool IsBoundGraphics(Enter3D* enter) {
  return enter->succeeded() &&
         static_cast<Graphics3DResource*>(enter->object())->IsBoundGraphics();
}

void ActiveTexture(PP_Resource context_id, GLenum texture) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glActiveTexture(texture);
  }
}

void AttachShader(PP_Resource context_id, GLuint program, GLuint shader) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glAttachShader(program, shader);
  }
}

void BindAttribLocation(PP_Resource context_id,
                        GLuint program,
                        GLuint index,
                        const char* name) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glBindAttribLocation(program, index, name);
  }
}

void BindBuffer(PP_Resource context_id, GLenum target, GLuint buffer) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glBindBuffer(target, buffer);
  }
}

void BindFramebuffer(PP_Resource context_id,
                     GLenum target,
                     GLuint framebuffer) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glBindFramebuffer(target, framebuffer);
  }
}

void BindRenderbuffer(PP_Resource context_id,
                      GLenum target,
                      GLuint renderbuffer) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glBindRenderbuffer(target, renderbuffer);
  }
}

void BindTexture(PP_Resource context_id, GLenum target, GLuint texture) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glBindTexture(target, texture);
  }
}

void BlendColor(PP_Resource context_id,
                GLclampf red,
                GLclampf green,
                GLclampf blue,
                GLclampf alpha) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glBlendColor(red, green, blue, alpha);
  }
}

void BlendEquation(PP_Resource context_id, GLenum mode) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glBlendEquation(mode);
  }
}

void BlendEquationSeparate(PP_Resource context_id,
                           GLenum modeRGB,
                           GLenum modeAlpha) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glBlendEquationSeparate(modeRGB, modeAlpha);
  }
}

void BlendFunc(PP_Resource context_id, GLenum sfactor, GLenum dfactor) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glBlendFunc(sfactor, dfactor);
  }
}

void BlendFuncSeparate(PP_Resource context_id,
                       GLenum srcRGB,
                       GLenum dstRGB,
                       GLenum srcAlpha,
                       GLenum dstAlpha) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
  }
}

void BufferData(PP_Resource context_id,
                GLenum target,
                GLsizeiptr size,
                const void* data,
                GLenum usage) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glBufferData(target, size, data, usage);
  }
}

void BufferSubData(PP_Resource context_id,
                   GLenum target,
                   GLintptr offset,
                   GLsizeiptr size,
                   const void* data) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glBufferSubData(target, offset, size, data);
  }
}

GLenum CheckFramebufferStatus(PP_Resource context_id, GLenum target) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    return glCheckFramebufferStatus(target);
  } else {
    return 0;
  }
}

void Clear(PP_Resource context_id, GLbitfield mask) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glClear(mask);
  }
}

void ClearColor(PP_Resource context_id,
                GLclampf red,
                GLclampf green,
                GLclampf blue,
                GLclampf alpha) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glClearColor(red, green, blue, alpha);
  }
}

void ClearDepthf(PP_Resource context_id, GLclampf depth) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glClearDepthf(depth);
  }
}

void ClearStencil(PP_Resource context_id, GLint s) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glClearStencil(s);
  }
}

void ColorMask(PP_Resource context_id,
               GLboolean red,
               GLboolean green,
               GLboolean blue,
               GLboolean alpha) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glColorMask(red, green, blue, alpha);
  }
}

void CompileShader(PP_Resource context_id, GLuint shader) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glCompileShader(shader);
  }
}

void CompressedTexImage2D(PP_Resource context_id,
                          GLenum target,
                          GLint level,
                          GLenum internalformat,
                          GLsizei width,
                          GLsizei height,
                          GLint border,
                          GLsizei imageSize,
                          const void* data) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glCompressedTexImage2D(
        target, level, internalformat, width, height, border, imageSize, data);
  }
}

void CompressedTexSubImage2D(PP_Resource context_id,
                             GLenum target,
                             GLint level,
                             GLint xoffset,
                             GLint yoffset,
                             GLsizei width,
                             GLsizei height,
                             GLenum format,
                             GLsizei imageSize,
                             const void* data) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glCompressedTexSubImage2D(target,
                              level,
                              xoffset,
                              yoffset,
                              width,
                              height,
                              format,
                              imageSize,
                              data);
  }
}

void CopyTexImage2D(PP_Resource context_id,
                    GLenum target,
                    GLint level,
                    GLenum internalformat,
                    GLint x,
                    GLint y,
                    GLsizei width,
                    GLsizei height,
                    GLint border) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glCopyTexImage2D(
        target, level, internalformat, x, y, width, height, border);
  }
}

void CopyTexSubImage2D(PP_Resource context_id,
                       GLenum target,
                       GLint level,
                       GLint xoffset,
                       GLint yoffset,
                       GLint x,
                       GLint y,
                       GLsizei width,
                       GLsizei height) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
  }
}

GLuint CreateProgram(PP_Resource context_id) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    return glCreateProgram();
  } else {
    return 0;
  }
}

GLuint CreateShader(PP_Resource context_id, GLenum type) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    return glCreateShader(type);
  } else {
    return 0;
  }
}

void CullFace(PP_Resource context_id, GLenum mode) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glCullFace(mode);
  }
}

void DeleteBuffers(PP_Resource context_id, GLsizei n, const GLuint* buffers) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glDeleteBuffers(n, buffers);
  }
}

void DeleteFramebuffers(PP_Resource context_id,
                        GLsizei n,
                        const GLuint* framebuffers) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glDeleteFramebuffers(n, framebuffers);
  }
}

void DeleteProgram(PP_Resource context_id, GLuint program) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glDeleteProgram(program);
  }
}

void DeleteRenderbuffers(PP_Resource context_id,
                         GLsizei n,
                         const GLuint* renderbuffers) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glDeleteRenderbuffers(n, renderbuffers);
  }
}

void DeleteShader(PP_Resource context_id, GLuint shader) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glDeleteShader(shader);
  }
}

void DeleteTextures(PP_Resource context_id, GLsizei n, const GLuint* textures) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glDeleteTextures(n, textures);
  }
}

void DepthFunc(PP_Resource context_id, GLenum func) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glDepthFunc(func);
  }
}

void DepthMask(PP_Resource context_id, GLboolean flag) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glDepthMask(flag);
  }
}

void DepthRangef(PP_Resource context_id, GLclampf zNear, GLclampf zFar) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glDepthRangef(zNear, zFar);
  }
}

void DetachShader(PP_Resource context_id, GLuint program, GLuint shader) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glDetachShader(program, shader);
  }
}

void Disable(PP_Resource context_id, GLenum cap) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glDisable(cap);
  }
}

void DisableVertexAttribArray(PP_Resource context_id, GLuint index) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glDisableVertexAttribArray(index);
  }
}

void DrawArrays(PP_Resource context_id,
                GLenum mode,
                GLint first,
                GLsizei count) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glDrawArrays(mode, first, count);
  }
}

void DrawElements(PP_Resource context_id,
                  GLenum mode,
                  GLsizei count,
                  GLenum type,
                  const void* indices) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glDrawElements(mode, count, type, indices);
  }
}

void Enable(PP_Resource context_id, GLenum cap) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glEnable(cap);
  }
}

void EnableVertexAttribArray(PP_Resource context_id, GLuint index) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glEnableVertexAttribArray(index);
  }
}

void Finish(PP_Resource context_id) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glFinish();
  }
}

void Flush(PP_Resource context_id) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glFlush();
  }
}

void FramebufferRenderbuffer(PP_Resource context_id,
                             GLenum target,
                             GLenum attachment,
                             GLenum renderbuffertarget,
                             GLuint renderbuffer) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glFramebufferRenderbuffer(
        target, attachment, renderbuffertarget, renderbuffer);
  }
}

void FramebufferTexture2D(PP_Resource context_id,
                          GLenum target,
                          GLenum attachment,
                          GLenum textarget,
                          GLuint texture,
                          GLint level) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glFramebufferTexture2D(target, attachment, textarget, texture, level);
  }
}

void FrontFace(PP_Resource context_id, GLenum mode) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glFrontFace(mode);
  }
}

void GenBuffers(PP_Resource context_id, GLsizei n, GLuint* buffers) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGenBuffers(n, buffers);
  }
}

void GenerateMipmap(PP_Resource context_id, GLenum target) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGenerateMipmap(target);
  }
}

void GenFramebuffers(PP_Resource context_id, GLsizei n, GLuint* framebuffers) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGenFramebuffers(n, framebuffers);
  }
}

void GenRenderbuffers(PP_Resource context_id,
                      GLsizei n,
                      GLuint* renderbuffers) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGenRenderbuffers(n, renderbuffers);
  }
}

void GenTextures(PP_Resource context_id, GLsizei n, GLuint* textures) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGenTextures(n, textures);
  }
}

void GetActiveAttrib(PP_Resource context_id,
                     GLuint program,
                     GLuint index,
                     GLsizei bufsize,
                     GLsizei* length,
                     GLint* size,
                     GLenum* type,
                     char* name) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGetActiveAttrib(program, index, bufsize, length, size, type, name);
  }
}

void GetActiveUniform(PP_Resource context_id,
                      GLuint program,
                      GLuint index,
                      GLsizei bufsize,
                      GLsizei* length,
                      GLint* size,
                      GLenum* type,
                      char* name) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGetActiveUniform(program, index, bufsize, length, size, type, name);
  }
}

void GetAttachedShaders(PP_Resource context_id,
                        GLuint program,
                        GLsizei maxcount,
                        GLsizei* count,
                        GLuint* shaders) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGetAttachedShaders(program, maxcount, count, shaders);
  }
}

GLint GetAttribLocation(PP_Resource context_id,
                        GLuint program,
                        const char* name) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    return glGetAttribLocation(program, name);
  } else {
    return -1;
  }
}

void GetBooleanv(PP_Resource context_id, GLenum pname, GLboolean* params) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGetBooleanv(pname, params);
  }
}

void GetBufferParameteriv(PP_Resource context_id,
                          GLenum target,
                          GLenum pname,
                          GLint* params) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGetBufferParameteriv(target, pname, params);
  }
}

GLenum GetError(PP_Resource context_id) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    return glGetError();
  } else {
    return 0;
  }
}

void GetFloatv(PP_Resource context_id, GLenum pname, GLfloat* params) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGetFloatv(pname, params);
  }
}

void GetFramebufferAttachmentParameteriv(PP_Resource context_id,
                                         GLenum target,
                                         GLenum attachment,
                                         GLenum pname,
                                         GLint* params) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
  }
}

void GetIntegerv(PP_Resource context_id, GLenum pname, GLint* params) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGetIntegerv(pname, params);
  }
}

void GetProgramiv(PP_Resource context_id,
                  GLuint program,
                  GLenum pname,
                  GLint* params) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGetProgramiv(program, pname, params);
  }
}

void GetProgramInfoLog(PP_Resource context_id,
                       GLuint program,
                       GLsizei bufsize,
                       GLsizei* length,
                       char* infolog) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGetProgramInfoLog(program, bufsize, length, infolog);
  }
}

void GetRenderbufferParameteriv(PP_Resource context_id,
                                GLenum target,
                                GLenum pname,
                                GLint* params) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGetRenderbufferParameteriv(target, pname, params);
  }
}

void GetShaderiv(PP_Resource context_id,
                 GLuint shader,
                 GLenum pname,
                 GLint* params) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGetShaderiv(shader, pname, params);
  }
}

void GetShaderInfoLog(PP_Resource context_id,
                      GLuint shader,
                      GLsizei bufsize,
                      GLsizei* length,
                      char* infolog) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGetShaderInfoLog(shader, bufsize, length, infolog);
  }
}

void GetShaderPrecisionFormat(PP_Resource context_id,
                              GLenum shadertype,
                              GLenum precisiontype,
                              GLint* range,
                              GLint* precision) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGetShaderPrecisionFormat(shadertype, precisiontype, range, precision);
  }
}

void GetShaderSource(PP_Resource context_id,
                     GLuint shader,
                     GLsizei bufsize,
                     GLsizei* length,
                     char* source) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGetShaderSource(shader, bufsize, length, source);
  }
}

const GLubyte* GetString(PP_Resource context_id, GLenum name) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    return glGetString(name);
  } else {
    return NULL;
  }
}

void GetTexParameterfv(PP_Resource context_id,
                       GLenum target,
                       GLenum pname,
                       GLfloat* params) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGetTexParameterfv(target, pname, params);
  }
}

void GetTexParameteriv(PP_Resource context_id,
                       GLenum target,
                       GLenum pname,
                       GLint* params) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGetTexParameteriv(target, pname, params);
  }
}

void GetUniformfv(PP_Resource context_id,
                  GLuint program,
                  GLint location,
                  GLfloat* params) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGetUniformfv(program, location, params);
  }
}

void GetUniformiv(PP_Resource context_id,
                  GLuint program,
                  GLint location,
                  GLint* params) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGetUniformiv(program, location, params);
  }
}

GLint GetUniformLocation(PP_Resource context_id,
                         GLuint program,
                         const char* name) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    return glGetUniformLocation(program, name);
  } else {
    return -1;
  }
}

void GetVertexAttribfv(PP_Resource context_id,
                       GLuint index,
                       GLenum pname,
                       GLfloat* params) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGetVertexAttribfv(index, pname, params);
  }
}

void GetVertexAttribiv(PP_Resource context_id,
                       GLuint index,
                       GLenum pname,
                       GLint* params) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGetVertexAttribiv(index, pname, params);
  }
}

void GetVertexAttribPointerv(PP_Resource context_id,
                             GLuint index,
                             GLenum pname,
                             void** pointer) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glGetVertexAttribPointerv(index, pname, pointer);
  }
}

void Hint(PP_Resource context_id, GLenum target, GLenum mode) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glHint(target, mode);
  }
}

GLboolean IsBuffer(PP_Resource context_id, GLuint buffer) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    return glIsBuffer(buffer);
  } else {
    return GL_FALSE;
  }
}

GLboolean IsEnabled(PP_Resource context_id, GLenum cap) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    return glIsEnabled(cap);
  } else {
    return GL_FALSE;
  }
}

GLboolean IsFramebuffer(PP_Resource context_id, GLuint framebuffer) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    return glIsFramebuffer(framebuffer);
  } else {
    return GL_FALSE;
  }
}

GLboolean IsProgram(PP_Resource context_id, GLuint program) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    return glIsProgram(program);
  } else {
    return GL_FALSE;
  }
}

GLboolean IsRenderbuffer(PP_Resource context_id, GLuint renderbuffer) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    return glIsRenderbuffer(renderbuffer);
  } else {
    return GL_FALSE;
  }
}

GLboolean IsShader(PP_Resource context_id, GLuint shader) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    return glIsShader(shader);
  } else {
    return GL_FALSE;
  }
}

GLboolean IsTexture(PP_Resource context_id, GLuint texture) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    return glIsTexture(texture);
  } else {
    return GL_FALSE;
  }
}

void LineWidth(PP_Resource context_id, GLfloat width) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glLineWidth(width);
  }
}

void LinkProgram(PP_Resource context_id, GLuint program) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glLinkProgram(program);
  }
}

void PixelStorei(PP_Resource context_id, GLenum pname, GLint param) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glPixelStorei(pname, param);
  }
}

void PolygonOffset(PP_Resource context_id, GLfloat factor, GLfloat units) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glPolygonOffset(factor, units);
  }
}

void ReadPixels(PP_Resource context_id,
                GLint x,
                GLint y,
                GLsizei width,
                GLsizei height,
                GLenum format,
                GLenum type,
                void* pixels) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glReadPixels(x, y, width, height, format, type, pixels);
  }
}

void ReleaseShaderCompiler(PP_Resource context_id) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glReleaseShaderCompiler();
  }
}

void RenderbufferStorage(PP_Resource context_id,
                         GLenum target,
                         GLenum internalformat,
                         GLsizei width,
                         GLsizei height) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glRenderbufferStorage(target, internalformat, width, height);
  }
}

void SampleCoverage(PP_Resource context_id, GLclampf value, GLboolean invert) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glSampleCoverage(value, invert);
  }
}

void Scissor(PP_Resource context_id,
             GLint x,
             GLint y,
             GLsizei width,
             GLsizei height) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glScissor(x, y, width, height);
  }
}

void ShaderBinary(PP_Resource context_id,
                  GLsizei n,
                  const GLuint* shaders,
                  GLenum binaryformat,
                  const void* binary,
                  GLsizei length) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glShaderBinary(n, shaders, binaryformat, binary, length);
  }
}

void ShaderSource(PP_Resource context_id,
                  GLuint shader,
                  GLsizei count,
                  const char** str,
                  const GLint* length) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glShaderSource(shader, count, str, length);
  }
}

void StencilFunc(PP_Resource context_id, GLenum func, GLint ref, GLuint mask) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glStencilFunc(func, ref, mask);
  }
}

void StencilFuncSeparate(PP_Resource context_id,
                         GLenum face,
                         GLenum func,
                         GLint ref,
                         GLuint mask) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glStencilFuncSeparate(face, func, ref, mask);
  }
}

void StencilMask(PP_Resource context_id, GLuint mask) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glStencilMask(mask);
  }
}

void StencilMaskSeparate(PP_Resource context_id, GLenum face, GLuint mask) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glStencilMaskSeparate(face, mask);
  }
}

void StencilOp(PP_Resource context_id,
               GLenum fail,
               GLenum zfail,
               GLenum zpass) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glStencilOp(fail, zfail, zpass);
  }
}

void StencilOpSeparate(PP_Resource context_id,
                       GLenum face,
                       GLenum fail,
                       GLenum zfail,
                       GLenum zpass) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glStencilOpSeparate(face, fail, zfail, zpass);
  }
}

void TexImage2D(PP_Resource context_id,
                GLenum target,
                GLint level,
                GLint internalformat,
                GLsizei width,
                GLsizei height,
                GLint border,
                GLenum format,
                GLenum type,
                const void* pixels) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glTexImage2D(target,
                 level,
                 internalformat,
                 width,
                 height,
                 border,
                 format,
                 type,
                 pixels);
  }
}

void TexParameterf(PP_Resource context_id,
                   GLenum target,
                   GLenum pname,
                   GLfloat param) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glTexParameterf(target, pname, param);
  }
}

void TexParameterfv(PP_Resource context_id,
                    GLenum target,
                    GLenum pname,
                    const GLfloat* params) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glTexParameterfv(target, pname, params);
  }
}

void TexParameteri(PP_Resource context_id,
                   GLenum target,
                   GLenum pname,
                   GLint param) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glTexParameteri(target, pname, param);
  }
}

void TexParameteriv(PP_Resource context_id,
                    GLenum target,
                    GLenum pname,
                    const GLint* params) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glTexParameteriv(target, pname, params);
  }
}

void TexSubImage2D(PP_Resource context_id,
                   GLenum target,
                   GLint level,
                   GLint xoffset,
                   GLint yoffset,
                   GLsizei width,
                   GLsizei height,
                   GLenum format,
                   GLenum type,
                   const void* pixels) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glTexSubImage2D(
        target, level, xoffset, yoffset, width, height, format, type, pixels);
  }
}

void Uniform1f(PP_Resource context_id, GLint location, GLfloat x) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glUniform1f(location, x);
  }
}

void Uniform1fv(PP_Resource context_id,
                GLint location,
                GLsizei count,
                const GLfloat* v) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glUniform1fv(location, count, v);
  }
}

void Uniform1i(PP_Resource context_id, GLint location, GLint x) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glUniform1i(location, x);
  }
}

void Uniform1iv(PP_Resource context_id,
                GLint location,
                GLsizei count,
                const GLint* v) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glUniform1iv(location, count, v);
  }
}

void Uniform2f(PP_Resource context_id, GLint location, GLfloat x, GLfloat y) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glUniform2f(location, x, y);
  }
}

void Uniform2fv(PP_Resource context_id,
                GLint location,
                GLsizei count,
                const GLfloat* v) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glUniform2fv(location, count, v);
  }
}

void Uniform2i(PP_Resource context_id, GLint location, GLint x, GLint y) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glUniform2i(location, x, y);
  }
}

void Uniform2iv(PP_Resource context_id,
                GLint location,
                GLsizei count,
                const GLint* v) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glUniform2iv(location, count, v);
  }
}

void Uniform3f(PP_Resource context_id,
               GLint location,
               GLfloat x,
               GLfloat y,
               GLfloat z) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glUniform3f(location, x, y, z);
  }
}

void Uniform3fv(PP_Resource context_id,
                GLint location,
                GLsizei count,
                const GLfloat* v) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glUniform3fv(location, count, v);
  }
}

void Uniform3i(PP_Resource context_id,
               GLint location,
               GLint x,
               GLint y,
               GLint z) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glUniform3i(location, x, y, z);
  }
}

void Uniform3iv(PP_Resource context_id,
                GLint location,
                GLsizei count,
                const GLint* v) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glUniform3iv(location, count, v);
  }
}

void Uniform4f(PP_Resource context_id,
               GLint location,
               GLfloat x,
               GLfloat y,
               GLfloat z,
               GLfloat w) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glUniform4f(location, x, y, z, w);
  }
}

void Uniform4fv(PP_Resource context_id,
                GLint location,
                GLsizei count,
                const GLfloat* v) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glUniform4fv(location, count, v);
  }
}

void Uniform4i(PP_Resource context_id,
               GLint location,
               GLint x,
               GLint y,
               GLint z,
               GLint w) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glUniform4i(location, x, y, z, w);
  }
}

void Uniform4iv(PP_Resource context_id,
                GLint location,
                GLsizei count,
                const GLint* v) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glUniform4iv(location, count, v);
  }
}

void UniformMatrix2fv(PP_Resource context_id,
                      GLint location,
                      GLsizei count,
                      GLboolean transpose,
                      const GLfloat* value) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glUniformMatrix2fv(location, count, transpose, value);
  }
}

void UniformMatrix3fv(PP_Resource context_id,
                      GLint location,
                      GLsizei count,
                      GLboolean transpose,
                      const GLfloat* value) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glUniformMatrix3fv(location, count, transpose, value);
  }
}

void UniformMatrix4fv(PP_Resource context_id,
                      GLint location,
                      GLsizei count,
                      GLboolean transpose,
                      const GLfloat* value) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glUniformMatrix4fv(location, count, transpose, value);
  }
}

void UseProgram(PP_Resource context_id, GLuint program) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glUseProgram(program);
  }
}

void ValidateProgram(PP_Resource context_id, GLuint program) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glValidateProgram(program);
  }
}

void VertexAttrib1f(PP_Resource context_id, GLuint indx, GLfloat x) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glVertexAttrib1f(indx, x);
  }
}

void VertexAttrib1fv(PP_Resource context_id,
                     GLuint indx,
                     const GLfloat* values) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glVertexAttrib1fv(indx, values);
  }
}

void VertexAttrib2f(PP_Resource context_id, GLuint indx, GLfloat x, GLfloat y) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glVertexAttrib2f(indx, x, y);
  }
}

void VertexAttrib2fv(PP_Resource context_id,
                     GLuint indx,
                     const GLfloat* values) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glVertexAttrib2fv(indx, values);
  }
}

void VertexAttrib3f(PP_Resource context_id,
                    GLuint indx,
                    GLfloat x,
                    GLfloat y,
                    GLfloat z) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glVertexAttrib3f(indx, x, y, z);
  }
}

void VertexAttrib3fv(PP_Resource context_id,
                     GLuint indx,
                     const GLfloat* values) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glVertexAttrib3fv(indx, values);
  }
}

void VertexAttrib4f(PP_Resource context_id,
                    GLuint indx,
                    GLfloat x,
                    GLfloat y,
                    GLfloat z,
                    GLfloat w) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glVertexAttrib4f(indx, x, y, z, w);
  }
}

void VertexAttrib4fv(PP_Resource context_id,
                     GLuint indx,
                     const GLfloat* values) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glVertexAttrib4fv(indx, values);
  }
}

void VertexAttribPointer(PP_Resource context_id,
                         GLuint indx,
                         GLint size,
                         GLenum type,
                         GLboolean normalized,
                         GLsizei stride,
                         const void* ptr) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glVertexAttribPointer(indx, size, type, normalized, stride, ptr);
  }
}

void Viewport(PP_Resource context_id,
              GLint x,
              GLint y,
              GLsizei width,
              GLsizei height) {
  Enter3D enter(context_id, true);
  if (IsBoundGraphics(&enter)) {
    glViewport(x, y, width, height);
  }
}

}  // namespace

const PPB_OpenGLES2* GetPPB_OpenGLES2_Thunk() {
  static const struct PPB_OpenGLES2 ppb_opengles2 = {
      &ActiveTexture,                       &AttachShader,
      &BindAttribLocation,                  &BindBuffer,
      &BindFramebuffer,                     &BindRenderbuffer,
      &BindTexture,                         &BlendColor,
      &BlendEquation,                       &BlendEquationSeparate,
      &BlendFunc,                           &BlendFuncSeparate,
      &BufferData,                          &BufferSubData,
      &CheckFramebufferStatus,              &Clear,
      &ClearColor,                          &ClearDepthf,
      &ClearStencil,                        &ColorMask,
      &CompileShader,                       &CompressedTexImage2D,
      &CompressedTexSubImage2D,             &CopyTexImage2D,
      &CopyTexSubImage2D,                   &CreateProgram,
      &CreateShader,                        &CullFace,
      &DeleteBuffers,                       &DeleteFramebuffers,
      &DeleteProgram,                       &DeleteRenderbuffers,
      &DeleteShader,                        &DeleteTextures,
      &DepthFunc,                           &DepthMask,
      &DepthRangef,                         &DetachShader,
      &Disable,                             &DisableVertexAttribArray,
      &DrawArrays,                          &DrawElements,
      &Enable,                              &EnableVertexAttribArray,
      &Finish,                              &Flush,
      &FramebufferRenderbuffer,             &FramebufferTexture2D,
      &FrontFace,                           &GenBuffers,
      &GenerateMipmap,                      &GenFramebuffers,
      &GenRenderbuffers,                    &GenTextures,
      &GetActiveAttrib,                     &GetActiveUniform,
      &GetAttachedShaders,                  &GetAttribLocation,
      &GetBooleanv,                         &GetBufferParameteriv,
      &GetError,                            &GetFloatv,
      &GetFramebufferAttachmentParameteriv, &GetIntegerv,
      &GetProgramiv,                        &GetProgramInfoLog,
      &GetRenderbufferParameteriv,          &GetShaderiv,
      &GetShaderInfoLog,                    &GetShaderPrecisionFormat,
      &GetShaderSource,                     &GetString,
      &GetTexParameterfv,                   &GetTexParameteriv,
      &GetUniformfv,                        &GetUniformiv,
      &GetUniformLocation,                  &GetVertexAttribfv,
      &GetVertexAttribiv,                   &GetVertexAttribPointerv,
      &Hint,                                &IsBuffer,
      &IsEnabled,                           &IsFramebuffer,
      &IsProgram,                           &IsRenderbuffer,
      &IsShader,                            &IsTexture,
      &LineWidth,                           &LinkProgram,
      &PixelStorei,                         &PolygonOffset,
      &ReadPixels,                          &ReleaseShaderCompiler,
      &RenderbufferStorage,                 &SampleCoverage,
      &Scissor,                             &ShaderBinary,
      &ShaderSource,                        &StencilFunc,
      &StencilFuncSeparate,                 &StencilMask,
      &StencilMaskSeparate,                 &StencilOp,
      &StencilOpSeparate,                   &TexImage2D,
      &TexParameterf,                       &TexParameterfv,
      &TexParameteri,                       &TexParameteriv,
      &TexSubImage2D,                       &Uniform1f,
      &Uniform1fv,                          &Uniform1i,
      &Uniform1iv,                          &Uniform2f,
      &Uniform2fv,                          &Uniform2i,
      &Uniform2iv,                          &Uniform3f,
      &Uniform3fv,                          &Uniform3i,
      &Uniform3iv,                          &Uniform4f,
      &Uniform4fv,                          &Uniform4i,
      &Uniform4iv,                          &UniformMatrix2fv,
      &UniformMatrix3fv,                    &UniformMatrix4fv,
      &UseProgram,                          &ValidateProgram,
      &VertexAttrib1f,                      &VertexAttrib1fv,
      &VertexAttrib2f,                      &VertexAttrib2fv,
      &VertexAttrib3f,                      &VertexAttrib3fv,
      &VertexAttrib4f,                      &VertexAttrib4fv,
      &VertexAttribPointer,                 &Viewport};
  return &ppb_opengles2;
}

}  // namespace examples
}  // namespace mojo
