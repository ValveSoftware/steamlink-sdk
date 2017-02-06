// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h"

#include "base/strings/string_number_conversions.h"

namespace gpu {
namespace gles2 {

// Implementations of commands
error::Error GLES2DecoderPassthroughImpl::DoActiveTexture(GLenum texture) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoAttachShader(GLuint program,
                                                         GLuint shader) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindAttribLocation(
    GLuint program,
    GLuint index,
    const char* name) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindBuffer(GLenum target,
                                                       GLuint buffer) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindBufferBase(GLenum target,
                                                           GLuint index,
                                                           GLuint buffer) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindBufferRange(GLenum target,
                                                            GLuint index,
                                                            GLuint buffer,
                                                            GLintptr offset,
                                                            GLsizeiptr size) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindFramebuffer(
    GLenum target,
    GLuint framebuffer) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindRenderbuffer(
    GLenum target,
    GLuint renderbuffer) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindSampler(GLuint unit,
                                                        GLuint sampler) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindTexture(GLenum target,
                                                        GLuint texture) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindTransformFeedback(
    GLenum target,
    GLuint transformfeedback) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendColor(GLclampf red,
                                                       GLclampf green,
                                                       GLclampf blue,
                                                       GLclampf alpha) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendEquation(GLenum mode) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendEquationSeparate(
    GLenum modeRGB,
    GLenum modeAlpha) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendFunc(GLenum sfactor,
                                                      GLenum dfactor) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendFuncSeparate(GLenum srcRGB,
                                                              GLenum dstRGB,
                                                              GLenum srcAlpha,
                                                              GLenum dstAlpha) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBufferData(GLenum target,
                                                       GLsizeiptr size,
                                                       const void* data,
                                                       GLenum usage) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBufferSubData(GLenum target,
                                                          GLintptr offset,
                                                          GLsizeiptr size,
                                                          const void* data) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCheckFramebufferStatus(
    GLenum target,
    uint32_t* result) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClear(GLbitfield mask) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearBufferfi(GLenum buffer,
                                                          GLint drawbuffers,
                                                          GLfloat depth,
                                                          GLint stencil) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearBufferfv(
    GLenum buffer,
    GLint drawbuffers,
    const GLfloat* value) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearBufferiv(GLenum buffer,
                                                          GLint drawbuffers,
                                                          const GLint* value) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearBufferuiv(
    GLenum buffer,
    GLint drawbuffers,
    const GLuint* value) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearColor(GLclampf red,
                                                       GLclampf green,
                                                       GLclampf blue,
                                                       GLclampf alpha) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearDepthf(GLclampf depth) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearStencil(GLint s) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClientWaitSync(GLuint sync,
                                                           GLbitfield flags,
                                                           GLuint64 timeout,
                                                           GLenum* result) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoColorMask(GLboolean red,
                                                      GLboolean green,
                                                      GLboolean blue,
                                                      GLboolean alpha) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCompileShader(GLuint shader) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCompressedTexImage2D(
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLsizei imageSize,
    const void* data) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCompressedTexSubImage2D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLsizei imageSize,
    const void* data) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCompressedTexImage3D(
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLsizei imageSize,
    const void* data) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCompressedTexSubImage3D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLenum format,
    GLsizei imageSize,
    const void* data) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCopyBufferSubData(
    GLenum readtarget,
    GLenum writetarget,
    GLintptr readoffset,
    GLintptr writeoffset,
    GLsizeiptr size) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCopyTexImage2D(
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLint border) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCopyTexSubImage2D(GLenum target,
                                                              GLint level,
                                                              GLint xoffset,
                                                              GLint yoffset,
                                                              GLint x,
                                                              GLint y,
                                                              GLsizei width,
                                                              GLsizei height) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCopyTexSubImage3D(GLenum target,
                                                              GLint level,
                                                              GLint xoffset,
                                                              GLint yoffset,
                                                              GLint zoffset,
                                                              GLint x,
                                                              GLint y,
                                                              GLsizei width,
                                                              GLsizei height) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCreateProgram(GLuint client_id) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCreateShader(GLenum type,
                                                         GLuint client_id) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCullFace(GLenum mode) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteBuffers(
    GLsizei n,
    const GLuint* buffers) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteFramebuffers(
    GLsizei n,
    const GLuint* framebuffers) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteProgram(GLuint program) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteRenderbuffers(
    GLsizei n,
    const GLuint* renderbuffers) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteSamplers(
    GLsizei n,
    const GLuint* samplers) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteSync(GLuint sync) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteShader(GLuint shader) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteTextures(
    GLsizei n,
    const GLuint* textures) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteTransformFeedbacks(
    GLsizei n,
    const GLuint* ids) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDepthFunc(GLenum func) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDepthMask(GLboolean flag) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDepthRangef(GLclampf zNear,
                                                        GLclampf zFar) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDetachShader(GLuint program,
                                                         GLuint shader) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDisable(GLenum cap) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDisableVertexAttribArray(
    GLuint index) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDrawArrays(GLenum mode,
                                                       GLint first,
                                                       GLsizei count) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDrawElements(GLenum mode,
                                                         GLsizei count,
                                                         GLenum type,
                                                         const void* indices) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEnable(GLenum cap) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEnableVertexAttribArray(
    GLuint index) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFenceSync(GLenum condition,
                                                      GLbitfield flags,
                                                      GLuint client_id) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFinish() {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFlush() {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFramebufferRenderbuffer(
    GLenum target,
    GLenum attachment,
    GLenum renderbuffertarget,
    GLuint renderbuffer) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFramebufferTexture2D(
    GLenum target,
    GLenum attachment,
    GLenum textarget,
    GLuint texture,
    GLint level) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFramebufferTextureLayer(
    GLenum target,
    GLenum attachment,
    GLuint texture,
    GLint level,
    GLint layer) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFrontFace(GLenum mode) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenBuffers(GLsizei n,
                                                       GLuint* buffers) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenerateMipmap(GLenum target) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenFramebuffers(
    GLsizei n,
    GLuint* framebuffers) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenRenderbuffers(
    GLsizei n,
    GLuint* renderbuffers) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenSamplers(GLsizei n,
                                                        GLuint* samplers) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenTextures(GLsizei n,
                                                        GLuint* textures) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenTransformFeedbacks(GLsizei n,
                                                                  GLuint* ids) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetActiveAttrib(GLuint program,
                                                            GLuint index,
                                                            GLint* size,
                                                            GLenum* type,
                                                            std::string* name) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetActiveUniform(
    GLuint program,
    GLuint index,
    GLint* size,
    GLenum* type,
    std::string* name) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetActiveUniformBlockiv(
    GLuint program,
    GLuint index,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetActiveUniformBlockName(
    GLuint program,
    GLuint index,
    std::string* name) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetActiveUniformsiv(
    GLuint program,
    GLsizei count,
    const GLuint* indices,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetAttachedShaders(
    GLuint program,
    GLsizei maxcount,
    GLsizei* count,
    GLuint* shaders) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetAttribLocation(GLuint program,
                                                              const char* name,
                                                              GLint* result) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetBooleanv(GLenum pname,
                                                        GLsizei bufsize,
                                                        GLsizei* length,
                                                        GLboolean* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetBufferParameteri64v(
    GLenum target,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint64* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetBufferParameteriv(
    GLenum target,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetError(uint32_t* result) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetFloatv(GLenum pname,
                                                      GLsizei bufsize,
                                                      GLsizei* length,
                                                      GLfloat* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetFragDataLocation(
    GLuint program,
    const char* name,
    GLint* result) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetFramebufferAttachmentParameteriv(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetInteger64v(GLenum pname,
                                                          GLsizei bufsize,
                                                          GLsizei* length,
                                                          GLint64* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetIntegeri_v(GLenum pname,
                                                          GLuint index,
                                                          GLsizei bufsize,
                                                          GLsizei* length,
                                                          GLint* data) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetInteger64i_v(GLenum pname,
                                                            GLuint index,
                                                            GLsizei bufsize,
                                                            GLsizei* length,
                                                            GLint64* data) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetIntegerv(GLenum pname,
                                                        GLsizei bufsize,
                                                        GLsizei* length,
                                                        GLint* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetInternalformativ(GLenum target,
                                                                GLenum format,
                                                                GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei* length,
                                                                GLint* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetProgramiv(GLuint program,
                                                         GLenum pname,
                                                         GLsizei bufsize,
                                                         GLsizei* length,
                                                         GLint* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetProgramInfoLog(
    GLuint program,
    std::string* infolog) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetRenderbufferParameteriv(
    GLenum target,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetSamplerParameterfv(
    GLuint sampler,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLfloat* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetSamplerParameteriv(
    GLuint sampler,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetShaderiv(GLuint shader,
                                                        GLenum pname,
                                                        GLsizei bufsize,
                                                        GLsizei* length,
                                                        GLint* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetShaderInfoLog(
    GLuint shader,
    std::string* infolog) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetShaderPrecisionFormat(
    GLenum shadertype,
    GLenum precisiontype,
    GLint* range,
    GLint* precision) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetShaderSource(
    GLuint shader,
    std::string* source) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetString(GLenum name,
                                                      const char** result) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetSynciv(GLuint sync,
                                                      GLenum pname,
                                                      GLsizei bufsize,
                                                      GLsizei* length,
                                                      GLint* values) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetTexParameterfv(GLenum target,
                                                              GLenum pname,
                                                              GLsizei bufsize,
                                                              GLsizei* length,
                                                              GLfloat* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetTexParameteriv(GLenum target,
                                                              GLenum pname,
                                                              GLsizei bufsize,
                                                              GLsizei* length,
                                                              GLint* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetTransformFeedbackVarying(
    GLuint program,
    GLuint index,
    GLsizei* size,
    GLenum* type,
    std::string* name) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformBlockIndex(
    GLuint program,
    const char* name,
    GLint* index) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformfv(GLuint program,
                                                         GLint location,
                                                         GLsizei bufsize,
                                                         GLsizei* length,
                                                         GLfloat* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformiv(GLuint program,
                                                         GLint location,
                                                         GLsizei bufsize,
                                                         GLsizei* length,
                                                         GLint* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformuiv(GLuint program,
                                                          GLint location,
                                                          GLsizei bufsize,
                                                          GLsizei* length,
                                                          GLuint* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformIndices(
    GLuint program,
    GLsizei count,
    const char* const* names,
    GLsizei bufSize,
    GLsizei* length,
    GLuint* indices) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformLocation(
    GLuint program,
    const char* name,
    GLint* location) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetVertexAttribfv(GLuint index,
                                                              GLenum pname,
                                                              GLsizei bufsize,
                                                              GLsizei* length,
                                                              GLfloat* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetVertexAttribiv(GLuint index,
                                                              GLenum pname,
                                                              GLsizei bufsize,
                                                              GLsizei* length,
                                                              GLint* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetVertexAttribIiv(GLuint index,
                                                               GLenum pname,
                                                               GLsizei bufsize,
                                                               GLsizei* length,
                                                               GLint* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetVertexAttribIuiv(
    GLuint index,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLuint* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetVertexAttribPointerv(
    GLuint index,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLuint* pointer) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoHint(GLenum target, GLenum mode) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoInvalidateFramebuffer(
    GLenum target,
    GLsizei count,
    const GLenum* attachments) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoInvalidateSubFramebuffer(
    GLenum target,
    GLsizei count,
    const GLenum* attachments,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsBuffer(GLuint buffer,
                                                     uint32_t* result) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsEnabled(GLenum cap,
                                                      uint32_t* result) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsFramebuffer(GLuint framebuffer,
                                                          uint32_t* result) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsProgram(GLuint program,
                                                      uint32_t* result) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsRenderbuffer(GLuint renderbuffer,
                                                           uint32_t* result) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsSampler(GLuint sampler,
                                                      uint32_t* result) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsShader(GLuint shader,
                                                     uint32_t* result) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsSync(GLuint sync,
                                                   uint32_t* result) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsTexture(GLuint texture,
                                                      uint32_t* result) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsTransformFeedback(
    GLuint transformfeedback,
    uint32_t* result) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoLineWidth(GLfloat width) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoLinkProgram(GLuint program) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPauseTransformFeedback() {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPixelStorei(GLenum pname,
                                                        GLint param) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPolygonOffset(GLfloat factor,
                                                          GLfloat units) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoReadBuffer(GLenum src) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoReadPixels(GLint x,
                                                       GLint y,
                                                       GLsizei width,
                                                       GLsizei height,
                                                       GLenum format,
                                                       GLenum type,
                                                       GLsizei bufsize,
                                                       GLsizei* length,
                                                       void* pixels) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoReleaseShaderCompiler() {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoRenderbufferStorage(
    GLenum target,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoResumeTransformFeedback() {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSampleCoverage(GLclampf value,
                                                           GLboolean invert) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSamplerParameterf(GLuint sampler,
                                                              GLenum pname,
                                                              GLfloat param) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSamplerParameterfv(
    GLuint sampler,
    GLenum pname,
    const GLfloat* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSamplerParameteri(GLuint sampler,
                                                              GLenum pname,
                                                              GLint param) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSamplerParameteriv(
    GLuint sampler,
    GLenum pname,
    const GLint* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoScissor(GLint x,
                                                    GLint y,
                                                    GLsizei width,
                                                    GLsizei height) {
  glScissor(x, y, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoShaderBinary(GLsizei n,
                                                         const GLuint* shaders,
                                                         GLenum binaryformat,
                                                         const void* binary,
                                                         GLsizei length) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoShaderSource(GLuint shader,
                                                         GLsizei count,
                                                         const char** string,
                                                         const GLint* length) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilFunc(GLenum func,
                                                        GLint ref,
                                                        GLuint mask) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilFuncSeparate(GLenum face,
                                                                GLenum func,
                                                                GLint ref,
                                                                GLuint mask) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilMask(GLuint mask) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilMaskSeparate(GLenum face,
                                                                GLuint mask) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilOp(GLenum fail,
                                                      GLenum zfail,
                                                      GLenum zpass) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilOpSeparate(GLenum face,
                                                              GLenum fail,
                                                              GLenum zfail,
                                                              GLenum zpass) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexImage2D(GLenum target,
                                                       GLint level,
                                                       GLint internalformat,
                                                       GLsizei width,
                                                       GLsizei height,
                                                       GLint border,
                                                       GLenum format,
                                                       GLenum type,
                                                       GLsizei imagesize,
                                                       const void* pixels) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexImage3D(GLenum target,
                                                       GLint level,
                                                       GLint internalformat,
                                                       GLsizei width,
                                                       GLsizei height,
                                                       GLsizei depth,
                                                       GLint border,
                                                       GLenum format,
                                                       GLenum type,
                                                       GLsizei imagesize,
                                                       const void* pixels) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexParameterf(GLenum target,
                                                          GLenum pname,
                                                          GLfloat param) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexParameterfv(
    GLenum target,
    GLenum pname,
    const GLfloat* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexParameteri(GLenum target,
                                                          GLenum pname,
                                                          GLint param) {
  glTexParameteri(target, pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexParameteriv(
    GLenum target,
    GLenum pname,
    const GLint* params) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexStorage3D(GLenum target,
                                                         GLsizei levels,
                                                         GLenum internalFormat,
                                                         GLsizei width,
                                                         GLsizei height,
                                                         GLsizei depth) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexSubImage2D(GLenum target,
                                                          GLint level,
                                                          GLint xoffset,
                                                          GLint yoffset,
                                                          GLsizei width,
                                                          GLsizei height,
                                                          GLenum format,
                                                          GLenum type,
                                                          GLsizei imagesize,
                                                          const void* pixels) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexSubImage3D(GLenum target,
                                                          GLint level,
                                                          GLint xoffset,
                                                          GLint yoffset,
                                                          GLint zoffset,
                                                          GLsizei width,
                                                          GLsizei height,
                                                          GLsizei depth,
                                                          GLenum format,
                                                          GLenum type,
                                                          GLsizei imagesize,
                                                          const void* pixels) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTransformFeedbackVaryings(
    GLuint program,
    GLsizei count,
    const char** varyings,
    GLenum buffermode) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1f(GLint location,
                                                      GLfloat x) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1fv(GLint location,
                                                       GLsizei count,
                                                       const GLfloat* v) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1i(GLint location, GLint x) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1iv(GLint location,
                                                       GLsizei count,
                                                       const GLint* v) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1ui(GLint location,
                                                       GLuint x) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1uiv(GLint location,
                                                        GLsizei count,
                                                        const GLuint* v) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2f(GLint location,
                                                      GLfloat x,
                                                      GLfloat y) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2fv(GLint location,
                                                       GLsizei count,
                                                       const GLfloat* v) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2i(GLint location,
                                                      GLint x,
                                                      GLint y) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2iv(GLint location,
                                                       GLsizei count,
                                                       const GLint* v) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2ui(GLint location,
                                                       GLuint x,
                                                       GLuint y) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2uiv(GLint location,
                                                        GLsizei count,
                                                        const GLuint* v) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3f(GLint location,
                                                      GLfloat x,
                                                      GLfloat y,
                                                      GLfloat z) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3fv(GLint location,
                                                       GLsizei count,
                                                       const GLfloat* v) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3i(GLint location,
                                                      GLint x,
                                                      GLint y,
                                                      GLint z) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3iv(GLint location,
                                                       GLsizei count,
                                                       const GLint* v) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3ui(GLint location,
                                                       GLuint x,
                                                       GLuint y,
                                                       GLuint z) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3uiv(GLint location,
                                                        GLsizei count,
                                                        const GLuint* v) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4f(GLint location,
                                                      GLfloat x,
                                                      GLfloat y,
                                                      GLfloat z,
                                                      GLfloat w) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4fv(GLint location,
                                                       GLsizei count,
                                                       const GLfloat* v) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4i(GLint location,
                                                      GLint x,
                                                      GLint y,
                                                      GLint z,
                                                      GLint w) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4iv(GLint location,
                                                       GLsizei count,
                                                       const GLint* v) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4ui(GLint location,
                                                       GLuint x,
                                                       GLuint y,
                                                       GLuint z,
                                                       GLuint w) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4uiv(GLint location,
                                                        GLsizei count,
                                                        const GLuint* v) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformBlockBinding(
    GLuint program,
    GLuint index,
    GLuint binding) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix2fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat* value) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix2x3fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat* value) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix2x4fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat* value) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix3fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat* value) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix3x2fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat* value) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix3x4fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat* value) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix4fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat* value) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix4x2fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat* value) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix4x3fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat* value) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUseProgram(GLuint program) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoValidateProgram(GLuint program) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib1f(GLuint indx,
                                                           GLfloat x) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib1fv(
    GLuint indx,
    const GLfloat* values) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib2f(GLuint indx,
                                                           GLfloat x,
                                                           GLfloat y) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib2fv(
    GLuint indx,
    const GLfloat* values) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib3f(GLuint indx,
                                                           GLfloat x,
                                                           GLfloat y,
                                                           GLfloat z) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib3fv(
    GLuint indx,
    const GLfloat* values) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib4f(GLuint indx,
                                                           GLfloat x,
                                                           GLfloat y,
                                                           GLfloat z,
                                                           GLfloat w) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib4fv(
    GLuint indx,
    const GLfloat* values) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribI4i(GLuint indx,
                                                            GLint x,
                                                            GLint y,
                                                            GLint z,
                                                            GLint w) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribI4iv(
    GLuint indx,
    const GLint* values) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribI4ui(GLuint indx,
                                                             GLuint x,
                                                             GLuint y,
                                                             GLuint z,
                                                             GLuint w) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribI4uiv(
    GLuint indx,
    const GLuint* values) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribIPointer(
    GLuint indx,
    GLint size,
    GLenum type,
    GLsizei stride,
    const void* ptr) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribPointer(
    GLuint indx,
    GLint size,
    GLenum type,
    GLboolean normalized,
    GLsizei stride,
    const void* ptr) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoViewport(GLint x,
                                                     GLint y,
                                                     GLsizei width,
                                                     GLsizei height) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoWaitSync(GLuint sync,
                                                     GLbitfield flags,
                                                     GLuint64 timeout) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlitFramebufferCHROMIUM(
    GLint srcX0,
    GLint srcY0,
    GLint srcX1,
    GLint srcY1,
    GLint dstX0,
    GLint dstY0,
    GLint dstX1,
    GLint dstY1,
    GLbitfield mask,
    GLenum filter) {
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoRenderbufferStorageMultisampleCHROMIUM(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoRenderbufferStorageMultisampleEXT(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFramebufferTexture2DMultisampleEXT(
    GLenum target,
    GLenum attachment,
    GLenum textarget,
    GLuint texture,
    GLint level,
    GLsizei samples) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexStorage2DEXT(
    GLenum target,
    GLsizei levels,
    GLenum internalFormat,
    GLsizei width,
    GLsizei height) {
  glTexStorage2DEXT(target, levels, internalFormat, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenQueriesEXT(GLsizei n,
                                                          GLuint* queries) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteQueriesEXT(
    GLsizei n,
    const GLuint* queries) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoQueryCounterEXT(GLuint id,
                                                            GLenum target) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBeginQueryEXT(GLenum target,
                                                          GLuint id) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBeginTransformFeedback(
    GLenum primitivemode) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEndQueryEXT(GLenum target) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEndTransformFeedback() {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSetDisjointValueSyncCHROMIUM(
    DisjointValueSync* sync) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoInsertEventMarkerEXT(
    GLsizei length,
    const char* marker) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPushGroupMarkerEXT(
    GLsizei length,
    const char* marker) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPopGroupMarkerEXT() {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenVertexArraysOES(GLsizei n,
                                                               GLuint* arrays) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteVertexArraysOES(
    GLsizei n,
    const GLuint* arrays) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsVertexArrayOES(GLuint array,
                                                             uint32_t* result) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindVertexArrayOES(GLuint array) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSwapBuffers() {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetMaxValueInBufferCHROMIUM(
    GLuint buffer_id,
    GLsizei count,
    GLenum type,
    GLuint offset,
    uint32_t* result) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEnableFeatureCHROMIUM(
    const char* feature) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoMapBufferRange(GLenum target,
                                                           GLintptr offset,
                                                           GLsizeiptr size,
                                                           GLbitfield access,
                                                           void** ptr) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUnmapBuffer(GLenum target) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoResizeCHROMIUM(GLuint width,
                                                           GLuint height,
                                                           GLfloat scale_factor,
                                                           GLboolean alpha) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetRequestableExtensionsCHROMIUM(
    const char** extensions) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoRequestExtensionCHROMIUM(
    const char* extension) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetProgramInfoCHROMIUM(
    GLuint program,
    std::vector<uint8_t>* data) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformBlocksCHROMIUM(
    GLuint program,
    std::vector<uint8_t>* data) {
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoGetTransformFeedbackVaryingsCHROMIUM(
    GLuint program,
    std::vector<uint8_t>* data) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformsES3CHROMIUM(
    GLuint program,
    std::vector<uint8_t>* data) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetTranslatedShaderSourceANGLE(
    GLuint shader,
    std::string* source) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPostSubBufferCHROMIUM(
    GLint x,
    GLint y,
    GLint width,
    GLint height) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCopyTextureCHROMIUM(
    GLenum source_id,
    GLenum dest_id,
    GLint internalformat,
    GLenum dest_type,
    GLboolean unpack_flip_y,
    GLboolean unpack_premultiply_alpha,
    GLboolean unpack_unmultiply_alpha) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCopySubTextureCHROMIUM(
    GLenum source_id,
    GLenum dest_id,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpack_flip_y,
    GLboolean unpack_premultiply_alpha,
    GLboolean unpack_unmultiply_alpha) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCompressedCopyTextureCHROMIUM(
    GLenum source_id,
    GLenum dest_id) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDrawArraysInstancedANGLE(
    GLenum mode,
    GLint first,
    GLsizei count,
    GLsizei primcount) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDrawElementsInstancedANGLE(
    GLenum mode,
    GLsizei count,
    GLenum type,
    const void* indices,
    GLsizei primcount) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribDivisorANGLE(
    GLuint index,
    GLuint divisor) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoProduceTextureCHROMIUM(
    GLenum target,
    const GLbyte* mailbox) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoProduceTextureDirectCHROMIUM(
    GLuint texture,
    GLenum target,
    const GLbyte* mailbox) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoConsumeTextureCHROMIUM(
    GLenum target,
    const GLbyte* mailbox) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCreateAndConsumeTextureCHROMIUM(
    GLenum target,
    const GLbyte* mailbox,
    GLuint texture) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindUniformLocationCHROMIUM(
    GLuint program,
    GLint location,
    const char* name) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindTexImage2DCHROMIUM(
    GLenum target,
    GLint imageId) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoReleaseTexImage2DCHROMIUM(
    GLenum target,
    GLint imageId) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTraceBeginCHROMIUM(
    const char* category_name,
    const char* trace_name) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTraceEndCHROMIUM() {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDiscardFramebufferEXT(
    GLenum target,
    GLsizei count,
    const GLenum* attachments) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoLoseContextCHROMIUM(GLenum current,
                                                                GLenum other) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDescheduleUntilFinishedCHROMIUM() {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoInsertFenceSyncCHROMIUM(
    GLuint64 release_count) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoWaitSyncTokenCHROMIUM(
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id,
    GLuint64 release_count) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDrawBuffersEXT(GLsizei count,
                                                           const GLenum* bufs) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDiscardBackbufferCHROMIUM() {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoScheduleOverlayPlaneCHROMIUM(
    GLint plane_z_order,
    GLenum plane_transform,
    GLuint overlay_texture_id,
    GLint bounds_x,
    GLint bounds_y,
    GLint bounds_width,
    GLint bounds_height,
    GLfloat uv_x,
    GLfloat uv_y,
    GLfloat uv_width,
    GLfloat uv_height) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoScheduleCALayerCHROMIUM(
    GLuint contents_texture_id,
    const GLfloat* contents_rect,
    GLfloat opacity,
    GLuint background_color,
    GLuint edge_aa_mask,
    const GLfloat* bounds_rect,
    GLboolean is_clipped,
    const GLfloat* clip_rect,
    GLint sorting_context_id,
    const GLfloat* transform) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoScheduleCALayerInUseQueryCHROMIUM(
    GLuint n,
    const GLuint* textures) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCommitOverlayPlanesCHROMIUM() {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSwapInterval(GLint interval) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFlushDriverCachesCHROMIUM() {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoMatrixLoadfCHROMIUM(
    GLenum matrixMode,
    const GLfloat* m) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoMatrixLoadIdentityCHROMIUM(
    GLenum matrixMode) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenPathsCHROMIUM(GLuint path,
                                                             GLsizei range) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDeletePathsCHROMIUM(GLuint path,
                                                                GLsizei range) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsPathCHROMIUM(GLuint path,
                                                           uint32_t* result) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPathCommandsCHROMIUM(
    GLuint path,
    GLsizei numCommands,
    const GLubyte* commands,
    GLsizei numCoords,
    GLenum coordType,
    const GLvoid* coords,
    GLsizei coords_bufsize) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPathParameterfCHROMIUM(
    GLuint path,
    GLenum pname,
    GLfloat value) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPathParameteriCHROMIUM(
    GLuint path,
    GLenum pname,
    GLint value) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPathStencilFuncCHROMIUM(
    GLenum func,
    GLint ref,
    GLuint mask) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilFillPathCHROMIUM(
    GLuint path,
    GLenum fillMode,
    GLuint mask) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilStrokePathCHROMIUM(
    GLuint path,
    GLint reference,
    GLuint mask) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCoverFillPathCHROMIUM(
    GLuint path,
    GLenum coverMode) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCoverStrokePathCHROMIUM(
    GLuint path,
    GLenum coverMode) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilThenCoverFillPathCHROMIUM(
    GLuint path,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilThenCoverStrokePathCHROMIUM(
    GLuint path,
    GLint reference,
    GLuint mask,
    GLenum coverMode) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilFillPathInstancedCHROMIUM(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLsizei pathsBufsize,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues,
    GLsizei transformValuesBufsize) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilStrokePathInstancedCHROMIUM(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLsizei pathsBufsize,
    GLuint pathBase,
    GLint reference,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues,
    GLsizei transformValuesBufsize) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCoverFillPathInstancedCHROMIUM(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLsizei pathsBufsize,
    GLuint pathBase,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues,
    GLsizei transformValuesBufsize) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCoverStrokePathInstancedCHROMIUM(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLsizei pathsBufsize,
    GLuint pathBase,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues,
    GLsizei transformValuesBufsize) {
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoStencilThenCoverFillPathInstancedCHROMIUM(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLsizei pathsBufsize,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues,
    GLsizei transformValuesBufsize) {
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoStencilThenCoverStrokePathInstancedCHROMIUM(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLsizei pathsBufsize,
    GLuint pathBase,
    GLint reference,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues,
    GLsizei transformValuesBufsize) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindFragmentInputLocationCHROMIUM(
    GLuint program,
    GLint location,
    const char* name) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoProgramPathFragmentInputGenCHROMIUM(
    GLuint program,
    GLint location,
    GLenum genMode,
    GLint components,
    const GLfloat* coeffs,
    GLsizei coeffsBufsize) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCoverageModulationCHROMIUM(
    GLenum components) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendBarrierKHR() {
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoApplyScreenSpaceAntialiasingCHROMIUM() {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindFragDataLocationIndexedEXT(
    GLuint program,
    GLuint colorNumber,
    GLuint index,
    const char* name) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindFragDataLocationEXT(
    GLuint program,
    GLuint colorNumber,
    const char* name) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetFragDataIndexEXT(
    GLuint program,
    const char* name,
    GLint* index) {
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoUniformMatrix4fvStreamTextureMatrixCHROMIUM(
    GLint location,
    GLboolean transpose,
    const GLfloat* defaultValue) {
  return error::kNoError;
}

}  // namespace gles2
}  // namespace gpu
