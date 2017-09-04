// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h"

#include "base/strings/string_number_conversions.h"

namespace gpu {
namespace gles2 {

namespace {

template <typename ClientType, typename ServiceType, typename GenFunction>
error::Error GenHelper(GLsizei n,
                       const volatile ClientType* client_ids,
                       ClientServiceMap<ClientType, ServiceType>* id_map,
                       GenFunction gen_function) {
  std::vector<ClientType> client_ids_copy(client_ids, client_ids + n);
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (id_map->GetServiceID(client_ids_copy[ii], nullptr)) {
      return error::kInvalidArguments;
    }
  }
  if (!CheckUniqueAndNonNullIds(n, client_ids_copy.data())) {
    return error::kInvalidArguments;
  }

  std::vector<ServiceType> service_ids(n, 0);
  gen_function(n, service_ids.data());
  for (GLsizei ii = 0; ii < n; ++ii) {
    id_map->SetIDMapping(client_ids_copy[ii], service_ids[ii]);
  }

  return error::kNoError;
}

template <typename ClientType, typename ServiceType, typename GenFunction>
error::Error CreateHelper(ClientType client_id,
                          ClientServiceMap<ClientType, ServiceType>* id_map,
                          GenFunction create_function) {
  if (id_map->GetServiceID(client_id, nullptr)) {
    return error::kInvalidArguments;
  }
  ServiceType service_id = create_function();
  id_map->SetIDMapping(client_id, service_id);
  return error::kNoError;
}

template <typename ClientType, typename ServiceType, typename DeleteFunction>
error::Error DeleteHelper(GLsizei n,
                          const volatile ClientType* client_ids,
                          ClientServiceMap<ClientType, ServiceType>* id_map,
                          DeleteFunction delete_function) {
  std::vector<ServiceType> service_ids(n, 0);
  for (GLsizei ii = 0; ii < n; ++ii) {
    ClientType client_id = client_ids[ii];
    service_ids[ii] = id_map->GetServiceIDOrInvalid(client_id);
    id_map->RemoveClientID(client_id);
  }

  delete_function(n, service_ids.data());

  return error::kNoError;
}

template <typename ClientType, typename ServiceType, typename DeleteFunction>
error::Error DeleteHelper(ClientType client_id,
                          ClientServiceMap<ClientType, ServiceType>* id_map,
                          DeleteFunction delete_function) {
  delete_function(id_map->GetServiceIDOrInvalid(client_id));
  id_map->RemoveClientID(client_id);
  return error::kNoError;
}

template <typename ClientType, typename ServiceType, typename GenFunction>
ServiceType GetServiceID(ClientType client_id,
                         ClientServiceMap<ClientType, ServiceType>* id_map,
                         bool create_if_missing,
                         GenFunction gen_function) {
  ServiceType service_id = id_map->invalid_service_id();
  if (id_map->GetServiceID(client_id, &service_id)) {
    return service_id;
  }

  if (create_if_missing) {
    service_id = gen_function();
    id_map->SetIDMapping(client_id, service_id);
    return service_id;
  }

  return id_map->invalid_service_id();
}

GLuint GetTextureServiceID(GLuint client_id,
                           PassthroughResources* resources,
                           bool create_if_missing) {
  return GetServiceID(client_id, &resources->texture_id_map, create_if_missing,
                      []() {
                        GLuint service_id = 0;
                        glGenTextures(1, &service_id);
                        return service_id;
                      });
}

GLuint GetBufferServiceID(GLuint client_id,
                          PassthroughResources* resources,
                          bool create_if_missing) {
  return GetServiceID(client_id, &resources->buffer_id_map, create_if_missing,
                      []() {
                        GLuint service_id = 0;
                        glGenBuffersARB(1, &service_id);
                        return service_id;
                      });
}

GLuint GetRenderbufferServiceID(GLuint client_id,
                                PassthroughResources* resources,
                                bool create_if_missing) {
  return GetServiceID(client_id, &resources->renderbuffer_id_map,
                      create_if_missing, []() {
                        GLuint service_id = 0;
                        glGenRenderbuffersEXT(1, &service_id);
                        return service_id;
                      });
}

GLuint GetFramebufferServiceID(GLuint client_id,
                               ClientServiceMap<GLuint, GLuint>* id_map,
                               bool create_if_missing) {
  return GetServiceID(client_id, id_map, create_if_missing, []() {
    GLuint service_id = 0;
    glGenFramebuffersEXT(1, &service_id);
    return service_id;
  });
}

GLuint GetTransformFeedbackServiceID(GLuint client_id,
                                     ClientServiceMap<GLuint, GLuint>* id_map) {
  return id_map->GetServiceIDOrInvalid(client_id);
}

GLuint GetVertexArrayServiceID(GLuint client_id,
                               ClientServiceMap<GLuint, GLuint>* id_map) {
  return id_map->GetServiceIDOrInvalid(client_id);
}

GLuint GetProgramServiceID(GLuint client_id, PassthroughResources* resources) {
  return resources->program_id_map.GetServiceIDOrInvalid(client_id);
}

GLuint GetShaderServiceID(GLuint client_id, PassthroughResources* resources) {
  return resources->shader_id_map.GetServiceIDOrInvalid(client_id);
}

GLuint GetQueryServiceID(GLuint client_id,
                         ClientServiceMap<GLuint, GLuint>* id_map) {
  return id_map->GetServiceIDOrInvalid(client_id);
}

GLuint GetSamplerServiceID(GLuint client_id, PassthroughResources* resources) {
  return resources->sampler_id_map.GetServiceIDOrInvalid(client_id);
}

GLsync GetSyncServiceID(GLuint client_id, PassthroughResources* resources) {
  return reinterpret_cast<GLsync>(
      resources->sync_id_map.GetServiceIDOrInvalid(client_id));
}

template <typename T>
void InsertValueIntoBuffer(std::vector<uint8_t>* data,
                           const T& value,
                           size_t offset) {
  DCHECK_LE(offset + sizeof(T), data->size());
  memcpy(data->data() + offset, &value, sizeof(T));
}

template <typename T>
void AppendValueToBuffer(std::vector<uint8_t>* data, const T& value) {
  size_t old_size = data->size();
  data->resize(old_size + sizeof(T));
  memcpy(data->data() + old_size, &value, sizeof(T));
}

void AppendStringToBuffer(std::vector<uint8_t>* data,
                          const char* str,
                          size_t len) {
  size_t old_size = data->size();
  data->resize(old_size + len);
  memcpy(data->data() + old_size, str, len);
}

}  // anonymous namespace

// Implementations of commands
error::Error GLES2DecoderPassthroughImpl::DoActiveTexture(GLenum texture) {
  glActiveTexture(texture);
  active_texture_unit_ = static_cast<size_t>(texture) - GL_TEXTURE0;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoAttachShader(GLuint program,
                                                         GLuint shader) {
  glAttachShader(GetProgramServiceID(program, resources_),
                 GetShaderServiceID(shader, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindAttribLocation(
    GLuint program,
    GLuint index,
    const char* name) {
  glBindAttribLocation(GetProgramServiceID(program, resources_), index, name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindBuffer(GLenum target,
                                                       GLuint buffer) {
  glBindBuffer(
      target, GetBufferServiceID(buffer, resources_, bind_generates_resource_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindBufferBase(GLenum target,
                                                           GLuint index,
                                                           GLuint buffer) {
  glBindBufferBase(target, index, GetBufferServiceID(buffer, resources_,
                                                     bind_generates_resource_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindBufferRange(GLenum target,
                                                            GLuint index,
                                                            GLuint buffer,
                                                            GLintptr offset,
                                                            GLsizeiptr size) {
  glBindBufferRange(target, index, GetBufferServiceID(buffer, resources_,
                                                      bind_generates_resource_),
                    offset, size);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindFramebuffer(
    GLenum target,
    GLuint framebuffer) {
  glBindFramebufferEXT(
      target, GetFramebufferServiceID(framebuffer, &framebuffer_id_map_,
                                      bind_generates_resource_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindRenderbuffer(
    GLenum target,
    GLuint renderbuffer) {
  glBindRenderbufferEXT(target,
                        GetRenderbufferServiceID(renderbuffer, resources_,
                                                 bind_generates_resource_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindSampler(GLuint unit,
                                                        GLuint sampler) {
  glBindSampler(unit, GetSamplerServiceID(sampler, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindTexture(GLenum target,
                                                        GLuint texture) {
  glBindTexture(target, GetTextureServiceID(texture, resources_,
                                            bind_generates_resource_));
  if (target == GL_TEXTURE_2D &&
      active_texture_unit_ < bound_textures_.size()) {
    bound_textures_[active_texture_unit_] = texture;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindTransformFeedback(
    GLenum target,
    GLuint transformfeedback) {
  glBindTransformFeedback(
      target, GetTransformFeedbackServiceID(transformfeedback,
                                            &transform_feedback_id_map_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendColor(GLclampf red,
                                                       GLclampf green,
                                                       GLclampf blue,
                                                       GLclampf alpha) {
  glBlendColor(red, green, blue, alpha);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendEquation(GLenum mode) {
  glBlendEquation(mode);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendEquationSeparate(
    GLenum modeRGB,
    GLenum modeAlpha) {
  glBlendEquationSeparate(modeRGB, modeAlpha);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendFunc(GLenum sfactor,
                                                      GLenum dfactor) {
  glBlendFunc(sfactor, dfactor);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendFuncSeparate(GLenum srcRGB,
                                                              GLenum dstRGB,
                                                              GLenum srcAlpha,
                                                              GLenum dstAlpha) {
  glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBufferData(GLenum target,
                                                       GLsizeiptr size,
                                                       const void* data,
                                                       GLenum usage) {
  glBufferData(target, size, data, usage);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBufferSubData(GLenum target,
                                                          GLintptr offset,
                                                          GLsizeiptr size,
                                                          const void* data) {
  glBufferSubData(target, offset, size, data);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCheckFramebufferStatus(
    GLenum target,
    uint32_t* result) {
  *result = glCheckFramebufferStatusEXT(target);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClear(GLbitfield mask) {
  glClear(mask);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearBufferfi(GLenum buffer,
                                                          GLint drawbuffers,
                                                          GLfloat depth,
                                                          GLint stencil) {
  glClearBufferfi(buffer, drawbuffers, depth, stencil);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearBufferfv(
    GLenum buffer,
    GLint drawbuffers,
    const volatile GLfloat* value) {
  glClearBufferfv(buffer, drawbuffers, const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearBufferiv(
    GLenum buffer,
    GLint drawbuffers,
    const volatile GLint* value) {
  glClearBufferiv(buffer, drawbuffers, const_cast<const GLint*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearBufferuiv(
    GLenum buffer,
    GLint drawbuffers,
    const volatile GLuint* value) {
  glClearBufferuiv(buffer, drawbuffers, const_cast<const GLuint*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearColor(GLclampf red,
                                                       GLclampf green,
                                                       GLclampf blue,
                                                       GLclampf alpha) {
  glClearColor(red, green, blue, alpha);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearDepthf(GLclampf depth) {
  glClearDepthf(depth);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearStencil(GLint s) {
  glClearStencil(s);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClientWaitSync(GLuint sync,
                                                           GLbitfield flags,
                                                           GLuint64 timeout,
                                                           GLenum* result) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoColorMask(GLboolean red,
                                                      GLboolean green,
                                                      GLboolean blue,
                                                      GLboolean alpha) {
  glColorMask(red, green, blue, alpha);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCompileShader(GLuint shader) {
  glCompileShader(GetShaderServiceID(shader, resources_));
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
  glCompressedTexImage2D(target, level, internalformat, width, height, border,
                         imageSize, data);
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
  glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height,
                            format, imageSize, data);
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
  glCompressedTexImage3D(target, level, internalformat, width, height, depth,
                         border, imageSize, data);
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
  glCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width,
                            height, depth, format, imageSize, data);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCopyBufferSubData(
    GLenum readtarget,
    GLenum writetarget,
    GLintptr readoffset,
    GLintptr writeoffset,
    GLsizeiptr size) {
  glCopyBufferSubData(readtarget, writetarget, readoffset, writeoffset, size);
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
  glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
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
  glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
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
  glCopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width,
                      height);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCreateProgram(GLuint client_id) {
  return CreateHelper(client_id, &resources_->program_id_map,
                      []() { return glCreateProgram(); });
}

error::Error GLES2DecoderPassthroughImpl::DoCreateShader(GLenum type,
                                                         GLuint client_id) {
  return CreateHelper(client_id, &resources_->shader_id_map,
                      [type]() { return glCreateShader(type); });
}

error::Error GLES2DecoderPassthroughImpl::DoCullFace(GLenum mode) {
  glCullFace(mode);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteBuffers(
    GLsizei n,
    const volatile GLuint* buffers) {
  return DeleteHelper(
      n, buffers, &resources_->buffer_id_map,
      [](GLsizei n, GLuint* buffers) { glDeleteBuffersARB(n, buffers); });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteFramebuffers(
    GLsizei n,
    const volatile GLuint* framebuffers) {
  return DeleteHelper(n, framebuffers, &framebuffer_id_map_,
                      [](GLsizei n, GLuint* framebuffers) {
                        glDeleteFramebuffersEXT(n, framebuffers);
                      });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteProgram(GLuint program) {
  return DeleteHelper(program, &resources_->program_id_map,
                      [](GLuint program) { glDeleteProgram(program); });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteRenderbuffers(
    GLsizei n,
    const volatile GLuint* renderbuffers) {
  return DeleteHelper(n, renderbuffers, &resources_->renderbuffer_id_map,
                      [](GLsizei n, GLuint* renderbuffers) {
                        glDeleteRenderbuffersEXT(n, renderbuffers);
                      });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteSamplers(
    GLsizei n,
    const volatile GLuint* samplers) {
  return DeleteHelper(
      n, samplers, &resources_->sampler_id_map,
      [](GLsizei n, GLuint* samplers) { glDeleteSamplers(n, samplers); });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteSync(GLuint sync) {
  return DeleteHelper(sync, &resources_->sync_id_map, [](uintptr_t sync) {
    glDeleteSync(reinterpret_cast<GLsync>(sync));
  });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteShader(GLuint shader) {
  return DeleteHelper(shader, &resources_->shader_id_map,
                      [](GLuint shader) { glDeleteShader(shader); });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteTextures(
    GLsizei n,
    const volatile GLuint* textures) {
  // Textures that are currently associated with a mailbox are stored in the
  // texture_object_map_ and are deleted automatically when they are
  // unreferenced.  Only delete textures that are not in this map.
  std::vector<GLuint> non_mailbox_client_ids;
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = textures[ii];
    auto texture_object_iter = resources_->texture_object_map.find(client_id);
    if (texture_object_iter == resources_->texture_object_map.end()) {
      // Delete with DeleteHelper
      non_mailbox_client_ids.push_back(client_id);
    } else {
      // Deleted when unreferenced
      resources_->texture_id_map.RemoveClientID(client_id);
      resources_->texture_object_map.erase(client_id);
    }
  }
  return DeleteHelper(
      non_mailbox_client_ids.size(), non_mailbox_client_ids.data(),
      &resources_->texture_id_map,
      [](GLsizei n, GLuint* textures) { glDeleteTextures(n, textures); });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteTransformFeedbacks(
    GLsizei n,
    const volatile GLuint* ids) {
  return DeleteHelper(n, ids, &transform_feedback_id_map_,
                      [](GLsizei n, GLuint* transform_feedbacks) {
                        glDeleteTransformFeedbacks(n, transform_feedbacks);
                      });
}

error::Error GLES2DecoderPassthroughImpl::DoDepthFunc(GLenum func) {
  glDepthFunc(func);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDepthMask(GLboolean flag) {
  glDepthMask(flag);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDepthRangef(GLclampf zNear,
                                                        GLclampf zFar) {
  glDepthRangef(zNear, zFar);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDetachShader(GLuint program,
                                                         GLuint shader) {
  glDetachShader(GetProgramServiceID(program, resources_),
                 GetShaderServiceID(shader, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDisable(GLenum cap) {
  glDisable(cap);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDisableVertexAttribArray(
    GLuint index) {
  glDisableVertexAttribArray(index);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDrawArrays(GLenum mode,
                                                       GLint first,
                                                       GLsizei count) {
  glDrawArrays(mode, first, count);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDrawElements(GLenum mode,
                                                         GLsizei count,
                                                         GLenum type,
                                                         const void* indices) {
  glDrawElements(mode, count, type, indices);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEnable(GLenum cap) {
  glEnable(cap);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEnableVertexAttribArray(
    GLuint index) {
  glEnableVertexAttribArray(index);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFenceSync(GLenum condition,
                                                      GLbitfield flags,
                                                      GLuint client_id) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFinish() {
  glFinish();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFlush() {
  glFlush();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFlushMappedBufferRange(
    GLenum target,
    GLintptr offset,
    GLsizeiptr size) {
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFramebufferRenderbuffer(
    GLenum target,
    GLenum attachment,
    GLenum renderbuffertarget,
    GLuint renderbuffer) {
  glFramebufferRenderbufferEXT(
      target, attachment, renderbuffertarget,
      GetRenderbufferServiceID(renderbuffer, resources_, false));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFramebufferTexture2D(
    GLenum target,
    GLenum attachment,
    GLenum textarget,
    GLuint texture,
    GLint level) {
  glFramebufferTexture2DEXT(target, attachment, textarget,
                            GetTextureServiceID(texture, resources_, false),
                            level);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFramebufferTextureLayer(
    GLenum target,
    GLenum attachment,
    GLuint texture,
    GLint level,
    GLint layer) {
  glFramebufferTextureLayer(target, attachment,
                            GetTextureServiceID(texture, resources_, false),
                            level, layer);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFrontFace(GLenum mode) {
  glFrontFace(mode);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenBuffers(
    GLsizei n,
    volatile GLuint* buffers) {
  return GenHelper(
      n, buffers, &resources_->buffer_id_map,
      [](GLsizei n, GLuint* buffers) { glGenBuffersARB(n, buffers); });
}

error::Error GLES2DecoderPassthroughImpl::DoGenerateMipmap(GLenum target) {
  glGenerateMipmapEXT(target);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenFramebuffers(
    GLsizei n,
    volatile GLuint* framebuffers) {
  return GenHelper(n, framebuffers, &framebuffer_id_map_,
                   [](GLsizei n, GLuint* framebuffers) {
                     glGenFramebuffersEXT(n, framebuffers);
                   });
}

error::Error GLES2DecoderPassthroughImpl::DoGenRenderbuffers(
    GLsizei n,
    volatile GLuint* renderbuffers) {
  return GenHelper(n, renderbuffers, &resources_->renderbuffer_id_map,
                   [](GLsizei n, GLuint* renderbuffers) {
                     glGenRenderbuffersEXT(n, renderbuffers);
                   });
}

error::Error GLES2DecoderPassthroughImpl::DoGenSamplers(
    GLsizei n,
    volatile GLuint* samplers) {
  return GenHelper(
      n, samplers, &resources_->sampler_id_map,
      [](GLsizei n, GLuint* samplers) { glGenSamplers(n, samplers); });
}

error::Error GLES2DecoderPassthroughImpl::DoGenTextures(
    GLsizei n,
    volatile GLuint* textures) {
  return GenHelper(
      n, textures, &resources_->texture_id_map,
      [](GLsizei n, GLuint* textures) { glGenTextures(n, textures); });
}

error::Error GLES2DecoderPassthroughImpl::DoGenTransformFeedbacks(
    GLsizei n,
    volatile GLuint* ids) {
  return GenHelper(n, ids, &transform_feedback_id_map_,
                   [](GLsizei n, GLuint* transform_feedbacks) {
                     glGenTransformFeedbacks(n, transform_feedbacks);
                   });
}

error::Error GLES2DecoderPassthroughImpl::DoGetActiveAttrib(GLuint program,
                                                            GLuint index,
                                                            GLint* size,
                                                            GLenum* type,
                                                            std::string* name) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetActiveUniform(
    GLuint program,
    GLuint index,
    GLint* size,
    GLenum* type,
    std::string* name) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetActiveUniformBlockiv(
    GLuint program,
    GLuint index,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  glGetActiveUniformBlockivRobustANGLE(GetProgramServiceID(program, resources_),
                                       index, pname, bufSize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetActiveUniformBlockName(
    GLuint program,
    GLuint index,
    std::string* name) {
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetAttachedShaders(
    GLuint program,
    GLsizei maxcount,
    GLsizei* count,
    GLuint* shaders) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetAttribLocation(GLuint program,
                                                              const char* name,
                                                              GLint* result) {
  *result = glGetAttribLocation(GetProgramServiceID(program, resources_), name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetBooleanv(GLenum pname,
                                                        GLsizei bufsize,
                                                        GLsizei* length,
                                                        GLboolean* params) {
  glGetBooleanvRobustANGLE(pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetBufferParameteri64v(
    GLenum target,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint64* params) {
  glGetBufferParameteri64vRobustANGLE(target, pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetBufferParameteriv(
    GLenum target,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint* params) {
  glGetBufferParameterivRobustANGLE(target, pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetError(uint32_t* result) {
  *result = glGetError();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetFloatv(GLenum pname,
                                                      GLsizei bufsize,
                                                      GLsizei* length,
                                                      GLfloat* params) {
  glGetFloatvRobustANGLE(pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetFragDataLocation(
    GLuint program,
    const char* name,
    GLint* result) {
  *result =
      glGetFragDataLocation(GetProgramServiceID(program, resources_), name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetFramebufferAttachmentParameteriv(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint* params) {
  glGetFramebufferAttachmentParameterivRobustANGLE(target, attachment, pname,
                                                   bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetInteger64v(GLenum pname,
                                                          GLsizei bufsize,
                                                          GLsizei* length,
                                                          GLint64* params) {
  glGetInteger64vRobustANGLE(pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetIntegeri_v(GLenum pname,
                                                          GLuint index,
                                                          GLsizei bufsize,
                                                          GLsizei* length,
                                                          GLint* data) {
  glGetIntegeri_vRobustANGLE(pname, index, bufsize, length, data);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetInteger64i_v(GLenum pname,
                                                            GLuint index,
                                                            GLsizei bufsize,
                                                            GLsizei* length,
                                                            GLint64* data) {
  glGetInteger64i_vRobustANGLE(pname, index, bufsize, length, data);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetIntegerv(GLenum pname,
                                                        GLsizei bufsize,
                                                        GLsizei* length,
                                                        GLint* params) {
  glGetIntegervRobustANGLE(pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetInternalformativ(GLenum target,
                                                                GLenum format,
                                                                GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei* length,
                                                                GLint* params) {
  glGetInternalformativRobustANGLE(target, format, pname, bufSize, length,
                                   params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetProgramiv(GLuint program,
                                                         GLenum pname,
                                                         GLsizei bufsize,
                                                         GLsizei* length,
                                                         GLint* params) {
  glGetProgramivRobustANGLE(GetProgramServiceID(program, resources_), pname,
                            bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetProgramInfoLog(
    GLuint program,
    std::string* infolog) {
  GLint info_log_len = 0;
  glGetProgramiv(GetProgramServiceID(program, resources_), GL_INFO_LOG_LENGTH,
                 &info_log_len);

  std::vector<char> buffer(info_log_len, 0);
  glGetProgramInfoLog(GetProgramServiceID(program, resources_), info_log_len,
                      nullptr, buffer.data());
  *infolog = std::string(buffer.data());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetRenderbufferParameteriv(
    GLenum target,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint* params) {
  glGetRenderbufferParameterivRobustANGLE(target, pname, bufsize, length,
                                          params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetSamplerParameterfv(
    GLuint sampler,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLfloat* params) {
  glGetSamplerParameterfvRobustANGLE(GetSamplerServiceID(sampler, resources_),
                                     pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetSamplerParameteriv(
    GLuint sampler,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint* params) {
  glGetSamplerParameterivRobustANGLE(sampler, pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetShaderiv(GLuint shader,
                                                        GLenum pname,
                                                        GLsizei bufsize,
                                                        GLsizei* length,
                                                        GLint* params) {
  glGetShaderivRobustANGLE(GetShaderServiceID(shader, resources_), pname,
                           bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetShaderInfoLog(
    GLuint shader,
    std::string* infolog) {
  GLuint service_id = GetShaderServiceID(shader, resources_);
  GLint info_log_len = 0;
  glGetShaderiv(service_id, GL_INFO_LOG_LENGTH, &info_log_len);
  std::vector<char> buffer(info_log_len, 0);
  glGetShaderInfoLog(service_id, info_log_len, nullptr, buffer.data());
  *infolog = std::string(buffer.data());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetShaderPrecisionFormat(
    GLenum shadertype,
    GLenum precisiontype,
    GLint* range,
    GLint* precision) {
  glGetShaderPrecisionFormat(shadertype, precisiontype, range, precision);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetShaderSource(
    GLuint shader,
    std::string* source) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetString(GLenum name,
                                                      const char** result) {
  switch (name) {
    case GL_VERSION:
      *result = GetServiceVersionString(feature_info_.get());
      break;
    case GL_SHADING_LANGUAGE_VERSION:
      *result = GetServiceShadingLanguageVersionString(feature_info_.get());
      break;
    case GL_RENDERER:
      *result = GetServiceRendererString(feature_info_.get());
      break;
    case GL_VENDOR:
      *result = GetServiceVendorString(feature_info_.get());
      break;
    case GL_EXTENSIONS:
      *result = extension_string_.c_str();
      break;
    default:
      *result = reinterpret_cast<const char*>(glGetString(name));
      break;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetSynciv(GLuint sync,
                                                      GLenum pname,
                                                      GLsizei bufsize,
                                                      GLsizei* length,
                                                      GLint* values) {
  glGetSynciv(GetSyncServiceID(sync, resources_), pname, bufsize, length,
              values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetTexParameterfv(GLenum target,
                                                              GLenum pname,
                                                              GLsizei bufsize,
                                                              GLsizei* length,
                                                              GLfloat* params) {
  glGetTexParameterfvRobustANGLE(target, pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetTexParameteriv(GLenum target,
                                                              GLenum pname,
                                                              GLsizei bufsize,
                                                              GLsizei* length,
                                                              GLint* params) {
  glGetTexParameterivRobustANGLE(target, pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetTransformFeedbackVarying(
    GLuint program,
    GLuint index,
    GLsizei* size,
    GLenum* type,
    std::string* name) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformBlockIndex(
    GLuint program,
    const char* name,
    GLint* index) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformfv(GLuint program,
                                                         GLint location,
                                                         GLsizei bufsize,
                                                         GLsizei* length,
                                                         GLfloat* params) {
  // GetUniform*RobustANGLE entry points expect bufsize in bytes like the entry
  // points in GL_EXT_robustness
  glGetUniformfvRobustANGLE(GetProgramServiceID(program, resources_), location,
                            bufsize * sizeof(*params), length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformiv(GLuint program,
                                                         GLint location,
                                                         GLsizei bufsize,
                                                         GLsizei* length,
                                                         GLint* params) {
  // GetUniform*RobustANGLE entry points expect bufsize in bytes like the entry
  // points in GL_EXT_robustness
  glGetUniformivRobustANGLE(GetProgramServiceID(program, resources_), location,
                            bufsize * sizeof(*params), length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformuiv(GLuint program,
                                                          GLint location,
                                                          GLsizei bufsize,
                                                          GLsizei* length,
                                                          GLuint* params) {
  // GetUniform*RobustANGLE entry points expect bufsize in bytes like the entry
  // points in GL_EXT_robustness
  glGetUniformuivRobustANGLE(GetProgramServiceID(program, resources_), location,
                             bufsize * sizeof(*params), length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformIndices(
    GLuint program,
    GLsizei count,
    const char* const* names,
    GLsizei bufSize,
    GLsizei* length,
    GLuint* indices) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformLocation(
    GLuint program,
    const char* name,
    GLint* location) {
  *location =
      glGetUniformLocation(GetProgramServiceID(program, resources_), name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetVertexAttribfv(GLuint index,
                                                              GLenum pname,
                                                              GLsizei bufsize,
                                                              GLsizei* length,
                                                              GLfloat* params) {
  glGetVertexAttribfvRobustANGLE(index, pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetVertexAttribiv(GLuint index,
                                                              GLenum pname,
                                                              GLsizei bufsize,
                                                              GLsizei* length,
                                                              GLint* params) {
  glGetVertexAttribivRobustANGLE(index, pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetVertexAttribIiv(GLuint index,
                                                               GLenum pname,
                                                               GLsizei bufsize,
                                                               GLsizei* length,
                                                               GLint* params) {
  glGetVertexAttribIivRobustANGLE(index, pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetVertexAttribIuiv(
    GLuint index,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLuint* params) {
  glGetVertexAttribIuivRobustANGLE(index, pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetVertexAttribPointerv(
    GLuint index,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLuint* pointer) {
  std::array<void*, 1> temp_pointers{{nullptr}};
  GLsizei temp_length = 0;
  glGetVertexAttribPointervRobustANGLE(
      index, pname, static_cast<GLsizei>(temp_pointers.size()), &temp_length,
      temp_pointers.data());
  DCHECK(temp_length >= 0 &&
         temp_length <= static_cast<GLsizei>(temp_pointers.size()) &&
         temp_length <= bufsize);
  for (GLsizei ii = 0; ii < temp_length; ii++) {
    pointer[ii] =
        static_cast<GLuint>(reinterpret_cast<uintptr_t>(temp_pointers[ii]));
  }
  *length = temp_length;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoHint(GLenum target, GLenum mode) {
  glHint(target, mode);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoInvalidateFramebuffer(
    GLenum target,
    GLsizei count,
    const volatile GLenum* attachments) {
  std::vector<GLenum> attachments_copy(attachments, attachments + count);
  glInvalidateFramebuffer(target, count, attachments_copy.data());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoInvalidateSubFramebuffer(
    GLenum target,
    GLsizei count,
    const volatile GLenum* attachments,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height) {
  std::vector<GLenum> attachments_copy(attachments, attachments + count);
  glInvalidateSubFramebuffer(target, count, attachments_copy.data(), x, y,
                             width, height);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsBuffer(GLuint buffer,
                                                     uint32_t* result) {
  NOTIMPLEMENTED();
  *result = glIsBuffer(GetBufferServiceID(buffer, resources_, false));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsEnabled(GLenum cap,
                                                      uint32_t* result) {
  *result = glIsEnabled(cap);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsFramebuffer(GLuint framebuffer,
                                                          uint32_t* result) {
  *result = glIsFramebufferEXT(
      GetFramebufferServiceID(framebuffer, &framebuffer_id_map_, false));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsProgram(GLuint program,
                                                      uint32_t* result) {
  *result = glIsProgram(GetProgramServiceID(program, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsRenderbuffer(GLuint renderbuffer,
                                                           uint32_t* result) {
  NOTIMPLEMENTED();
  *result = glIsRenderbufferEXT(
      GetRenderbufferServiceID(renderbuffer, resources_, false));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsSampler(GLuint sampler,
                                                      uint32_t* result) {
  *result = glIsSampler(GetSamplerServiceID(sampler, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsShader(GLuint shader,
                                                     uint32_t* result) {
  *result = glIsShader(GetShaderServiceID(shader, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsSync(GLuint sync,
                                                   uint32_t* result) {
  *result = glIsSync(GetSyncServiceID(sync, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsTexture(GLuint texture,
                                                      uint32_t* result) {
  *result = glIsTexture(GetTextureServiceID(texture, resources_, false));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsTransformFeedback(
    GLuint transformfeedback,
    uint32_t* result) {
  *result = glIsTransformFeedback(GetTransformFeedbackServiceID(
      transformfeedback, &transform_feedback_id_map_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoLineWidth(GLfloat width) {
  glLineWidth(width);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoLinkProgram(GLuint program) {
  glLinkProgram(GetProgramServiceID(program, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPauseTransformFeedback() {
  glPauseTransformFeedback();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPixelStorei(GLenum pname,
                                                        GLint param) {
  glPixelStorei(pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPolygonOffset(GLfloat factor,
                                                          GLfloat units) {
  glPolygonOffset(factor, units);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoReadBuffer(GLenum src) {
  glReadBuffer(src);
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
  glReadPixelsRobustANGLE(x, y, width, height, format, type, bufsize, length,
                          pixels);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoReleaseShaderCompiler() {
  glReleaseShaderCompiler();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoRenderbufferStorage(
    GLenum target,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  glRenderbufferStorageEXT(target, internalformat, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoResumeTransformFeedback() {
  glResumeTransformFeedback();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSampleCoverage(GLclampf value,
                                                           GLboolean invert) {
  glSampleCoverage(value, invert);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSamplerParameterf(GLuint sampler,
                                                              GLenum pname,
                                                              GLfloat param) {
  glSamplerParameterf(GetSamplerServiceID(sampler, resources_), pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSamplerParameterfv(
    GLuint sampler,
    GLenum pname,
    const volatile GLfloat* params) {
  std::array<GLfloat, 1> params_copy{{params[0]}};
  glSamplerParameterfvRobustANGLE(
      GetSamplerServiceID(sampler, resources_), pname,
      static_cast<GLsizei>(params_copy.size()), params_copy.data());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSamplerParameteri(GLuint sampler,
                                                              GLenum pname,
                                                              GLint param) {
  glSamplerParameteri(GetSamplerServiceID(sampler, resources_), pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSamplerParameteriv(
    GLuint sampler,
    GLenum pname,
    const volatile GLint* params) {
  std::array<GLint, 1> params_copy{{params[0]}};
  glSamplerParameterivRobustANGLE(
      GetSamplerServiceID(sampler, resources_), pname,
      static_cast<GLsizei>(params_copy.size()), params_copy.data());
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
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoShaderSource(GLuint shader,
                                                         GLsizei count,
                                                         const char** string,
                                                         const GLint* length) {
  glShaderSource(GetShaderServiceID(shader, resources_), count, string, length);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilFunc(GLenum func,
                                                        GLint ref,
                                                        GLuint mask) {
  glStencilFunc(func, ref, mask);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilFuncSeparate(GLenum face,
                                                                GLenum func,
                                                                GLint ref,
                                                                GLuint mask) {
  glStencilFuncSeparate(face, func, ref, mask);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilMask(GLuint mask) {
  glStencilMask(mask);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilMaskSeparate(GLenum face,
                                                                GLuint mask) {
  glStencilMaskSeparate(face, mask);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilOp(GLenum fail,
                                                      GLenum zfail,
                                                      GLenum zpass) {
  glStencilOp(fail, zfail, zpass);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilOpSeparate(GLenum face,
                                                              GLenum fail,
                                                              GLenum zfail,
                                                              GLenum zpass) {
  glStencilOpSeparate(face, fail, zfail, zpass);
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
  glTexImage2DRobustANGLE(target, level, internalformat, width, height, border,
                          format, type, imagesize, pixels);
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
  glTexImage3DRobustANGLE(target, level, internalformat, width, height, depth,
                          border, format, type, imagesize, pixels);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexParameterf(GLenum target,
                                                          GLenum pname,
                                                          GLfloat param) {
  glTexParameterf(target, pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexParameterfv(
    GLenum target,
    GLenum pname,
    const volatile GLfloat* params) {
  std::array<GLfloat, 1> params_copy{{params[0]}};
  glTexParameterfvRobustANGLE(target, pname,
                              static_cast<GLsizei>(params_copy.size()),
                              params_copy.data());
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
    const volatile GLint* params) {
  std::array<GLint, 1> params_copy{{params[0]}};
  glTexParameterivRobustANGLE(target, pname,
                              static_cast<GLsizei>(params_copy.size()),
                              params_copy.data());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexStorage3D(GLenum target,
                                                         GLsizei levels,
                                                         GLenum internalFormat,
                                                         GLsizei width,
                                                         GLsizei height,
                                                         GLsizei depth) {
  glTexStorage3D(target, levels, internalFormat, width, height, depth);
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
  glTexSubImage2DRobustANGLE(target, level, xoffset, yoffset, width, height,
                             format, type, imagesize, pixels);
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
  glTexSubImage3DRobustANGLE(target, level, xoffset, yoffset, zoffset, width,
                             height, depth, format, type, imagesize, pixels);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTransformFeedbackVaryings(
    GLuint program,
    GLsizei count,
    const char** varyings,
    GLenum buffermode) {
  glTransformFeedbackVaryings(GetProgramServiceID(program, resources_), count,
                              varyings, buffermode);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1f(GLint location,
                                                      GLfloat x) {
  glUniform1f(location, x);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1fv(
    GLint location,
    GLsizei count,
    const volatile GLfloat* v) {
  glUniform1fv(location, count, const_cast<const GLfloat*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1i(GLint location, GLint x) {
  glUniform1i(location, x);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1iv(
    GLint location,
    GLsizei count,
    const volatile GLint* v) {
  glUniform1iv(location, count, const_cast<const GLint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1ui(GLint location,
                                                       GLuint x) {
  glUniform1ui(location, x);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1uiv(
    GLint location,
    GLsizei count,
    const volatile GLuint* v) {
  glUniform1uiv(location, count, const_cast<const GLuint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2f(GLint location,
                                                      GLfloat x,
                                                      GLfloat y) {
  glUniform2f(location, x, y);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2fv(
    GLint location,
    GLsizei count,
    const volatile GLfloat* v) {
  glUniform2fv(location, count, const_cast<const GLfloat*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2i(GLint location,
                                                      GLint x,
                                                      GLint y) {
  glUniform2i(location, x, y);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2iv(
    GLint location,
    GLsizei count,
    const volatile GLint* v) {
  glUniform2iv(location, count, const_cast<const GLint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2ui(GLint location,
                                                       GLuint x,
                                                       GLuint y) {
  glUniform2ui(location, x, y);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2uiv(
    GLint location,
    GLsizei count,
    const volatile GLuint* v) {
  glUniform2uiv(location, count, const_cast<const GLuint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3f(GLint location,
                                                      GLfloat x,
                                                      GLfloat y,
                                                      GLfloat z) {
  glUniform3f(location, x, y, z);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3fv(
    GLint location,
    GLsizei count,
    const volatile GLfloat* v) {
  glUniform3fv(location, count, const_cast<const GLfloat*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3i(GLint location,
                                                      GLint x,
                                                      GLint y,
                                                      GLint z) {
  glUniform3i(location, x, y, z);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3iv(
    GLint location,
    GLsizei count,
    const volatile GLint* v) {
  glUniform3iv(location, count, const_cast<const GLint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3ui(GLint location,
                                                       GLuint x,
                                                       GLuint y,
                                                       GLuint z) {
  glUniform3ui(location, x, y, z);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3uiv(
    GLint location,
    GLsizei count,
    const volatile GLuint* v) {
  glUniform3uiv(location, count, const_cast<const GLuint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4f(GLint location,
                                                      GLfloat x,
                                                      GLfloat y,
                                                      GLfloat z,
                                                      GLfloat w) {
  glUniform4f(location, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4fv(
    GLint location,
    GLsizei count,
    const volatile GLfloat* v) {
  glUniform4fv(location, count, const_cast<const GLfloat*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4i(GLint location,
                                                      GLint x,
                                                      GLint y,
                                                      GLint z,
                                                      GLint w) {
  glUniform4i(location, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4iv(
    GLint location,
    GLsizei count,
    const volatile GLint* v) {
  glUniform4iv(location, count, const_cast<const GLint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4ui(GLint location,
                                                       GLuint x,
                                                       GLuint y,
                                                       GLuint z,
                                                       GLuint w) {
  glUniform4ui(location, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4uiv(
    GLint location,
    GLsizei count,
    const volatile GLuint* v) {
  glUniform4uiv(location, count, const_cast<const GLuint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformBlockBinding(
    GLuint program,
    GLuint index,
    GLuint binding) {
  glUniformBlockBinding(GetProgramServiceID(program, resources_), index,
                        binding);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix2fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  glUniformMatrix2fv(location, count, transpose,
                     const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix2x3fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  glUniformMatrix2x3fv(location, count, transpose,
                       const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix2x4fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  glUniformMatrix2x4fv(location, count, transpose,
                       const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix3fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  glUniformMatrix3fv(location, count, transpose,
                     const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix3x2fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  glUniformMatrix3x2fv(location, count, transpose,
                       const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix3x4fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  glUniformMatrix3x4fv(location, count, transpose,
                       const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix4fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  glUniformMatrix4fv(location, count, transpose,
                     const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix4x2fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  glUniformMatrix4x2fv(location, count, transpose,
                       const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix4x3fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  glUniformMatrix4x3fv(location, count, transpose,
                       const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUseProgram(GLuint program) {
  glUseProgram(GetProgramServiceID(program, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoValidateProgram(GLuint program) {
  glValidateProgram(GetProgramServiceID(program, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib1f(GLuint indx,
                                                           GLfloat x) {
  glVertexAttrib1f(indx, x);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib1fv(
    GLuint indx,
    const volatile GLfloat* values) {
  glVertexAttrib1fv(indx, const_cast<const GLfloat*>(values));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib2f(GLuint indx,
                                                           GLfloat x,
                                                           GLfloat y) {
  glVertexAttrib2f(indx, x, y);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib2fv(
    GLuint indx,
    const volatile GLfloat* values) {
  glVertexAttrib2fv(indx, const_cast<const GLfloat*>(values));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib3f(GLuint indx,
                                                           GLfloat x,
                                                           GLfloat y,
                                                           GLfloat z) {
  glVertexAttrib3f(indx, x, y, z);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib3fv(
    GLuint indx,
    const volatile GLfloat* values) {
  glVertexAttrib3fv(indx, const_cast<const GLfloat*>(values));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib4f(GLuint indx,
                                                           GLfloat x,
                                                           GLfloat y,
                                                           GLfloat z,
                                                           GLfloat w) {
  glVertexAttrib4f(indx, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib4fv(
    GLuint indx,
    const volatile GLfloat* values) {
  glVertexAttrib4fv(indx, const_cast<const GLfloat*>(values));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribI4i(GLuint indx,
                                                            GLint x,
                                                            GLint y,
                                                            GLint z,
                                                            GLint w) {
  glVertexAttribI4i(indx, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribI4iv(
    GLuint indx,
    const volatile GLint* values) {
  glVertexAttribI4iv(indx, const_cast<const GLint*>(values));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribI4ui(GLuint indx,
                                                             GLuint x,
                                                             GLuint y,
                                                             GLuint z,
                                                             GLuint w) {
  glVertexAttribI4ui(indx, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribI4uiv(
    GLuint indx,
    const volatile GLuint* values) {
  glVertexAttribI4uiv(indx, const_cast<const GLuint*>(values));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribIPointer(
    GLuint indx,
    GLint size,
    GLenum type,
    GLsizei stride,
    const void* ptr) {
  glVertexAttribIPointer(indx, size, type, stride, ptr);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribPointer(
    GLuint indx,
    GLint size,
    GLenum type,
    GLboolean normalized,
    GLsizei stride,
    const void* ptr) {
  glVertexAttribPointer(indx, size, type, normalized, stride, ptr);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoViewport(GLint x,
                                                     GLint y,
                                                     GLsizei width,
                                                     GLsizei height) {
  glViewport(x, y, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoWaitSync(GLuint sync,
                                                     GLbitfield flags,
                                                     GLuint64 timeout) {
  NOTIMPLEMENTED();
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
  glBlitFramebufferANGLE(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1,
                         mask, filter);
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoRenderbufferStorageMultisampleCHROMIUM(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  glRenderbufferStorageMultisampleANGLE(target, samples, internalformat, width,
                                        height);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoRenderbufferStorageMultisampleEXT(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  glRenderbufferStorageMultisampleANGLE(target, samples, internalformat, width,
                                        height);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFramebufferTexture2DMultisampleEXT(
    GLenum target,
    GLenum attachment,
    GLenum textarget,
    GLuint texture,
    GLint level,
    GLsizei samples) {
  NOTIMPLEMENTED();
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

error::Error GLES2DecoderPassthroughImpl::DoGenQueriesEXT(
    GLsizei n,
    volatile GLuint* queries) {
  return GenHelper(n, queries, &query_id_map_, [](GLsizei n, GLuint* queries) {
    glGenQueries(n, queries);
  });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteQueriesEXT(
    GLsizei n,
    const volatile GLuint* queries) {
  return DeleteHelper(
      n, queries, &query_id_map_,
      [](GLsizei n, GLuint* queries) { glDeleteQueries(n, queries); });
}

error::Error GLES2DecoderPassthroughImpl::DoQueryCounterEXT(GLuint id,
                                                            GLenum target) {
  glQueryCounter(GetQueryServiceID(id, &query_id_map_), target);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBeginQueryEXT(GLenum target,
                                                          GLuint id) {
  // TODO(geofflang): Track active queries
  glBeginQuery(target, GetQueryServiceID(id, &query_id_map_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBeginTransformFeedback(
    GLenum primitivemode) {
  glBeginTransformFeedback(primitivemode);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEndQueryEXT(GLenum target) {
  // TODO(geofflang): Track active queries
  glEndQuery(target);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEndTransformFeedback() {
  glEndTransformFeedback();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSetDisjointValueSyncCHROMIUM(
    DisjointValueSync* sync) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoInsertEventMarkerEXT(
    GLsizei length,
    const char* marker) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPushGroupMarkerEXT(
    GLsizei length,
    const char* marker) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPopGroupMarkerEXT() {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenVertexArraysOES(
    GLsizei n,
    volatile GLuint* arrays) {
  return GenHelper(
      n, arrays, &vertex_array_id_map_,
      [](GLsizei n, GLuint* arrays) { glGenVertexArraysOES(n, arrays); });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteVertexArraysOES(
    GLsizei n,
    const volatile GLuint* arrays) {
  return DeleteHelper(
      n, arrays, &vertex_array_id_map_,
      [](GLsizei n, GLuint* arrays) { glDeleteVertexArraysOES(n, arrays); });
}

error::Error GLES2DecoderPassthroughImpl::DoIsVertexArrayOES(GLuint array,
                                                             uint32_t* result) {
  *result =
      glIsVertexArrayOES(GetVertexArrayServiceID(array, &vertex_array_id_map_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindVertexArrayOES(GLuint array) {
  glBindVertexArrayOES(GetVertexArrayServiceID(array, &vertex_array_id_map_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSwapBuffers() {
  gfx::SwapResult result = surface_->SwapBuffers();
  if (result == gfx::SwapResult::SWAP_FAILED) {
    LOG(ERROR) << "Context lost because SwapBuffers failed.";
  }
  // TODO(geofflang): force the context loss?
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetMaxValueInBufferCHROMIUM(
    GLuint buffer_id,
    GLsizei count,
    GLenum type,
    GLuint offset,
    uint32_t* result) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEnableFeatureCHROMIUM(
    const char* feature) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoMapBufferRange(GLenum target,
                                                           GLintptr offset,
                                                           GLsizeiptr size,
                                                           GLbitfield access,
                                                           void** ptr) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUnmapBuffer(GLenum target) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoResizeCHROMIUM(GLuint width,
                                                           GLuint height,
                                                           GLfloat scale_factor,
                                                           GLboolean alpha) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetRequestableExtensionsCHROMIUM(
    const char** extensions) {
  *extensions = "";
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoRequestExtensionCHROMIUM(
    const char* extension) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetProgramInfoCHROMIUM(
    GLuint program,
    std::vector<uint8_t>* data) {
  GLuint service_program = 0;
  if (!resources_->program_id_map.GetServiceID(program, &service_program)) {
    return error::kNoError;
  }

  GLint num_attributes = 0;
  glGetProgramiv(service_program, GL_ACTIVE_ATTRIBUTES, &num_attributes);

  GLint num_uniforms = 0;
  glGetProgramiv(service_program, GL_ACTIVE_UNIFORMS, &num_uniforms);

  data->resize(sizeof(ProgramInfoHeader) +
                   ((num_attributes + num_uniforms) * sizeof(ProgramInput)),
               0);

  GLint link_status = 0;
  glGetProgramiv(service_program, GL_LINK_STATUS, &link_status);

  ProgramInfoHeader header;
  header.link_status = link_status;
  header.num_attribs = num_attributes;
  header.num_uniforms = num_uniforms;
  InsertValueIntoBuffer(data, header, 0);

  GLint active_attribute_max_length = 0;
  glGetProgramiv(service_program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,
                 &active_attribute_max_length);

  std::vector<char> attrib_name_buf(active_attribute_max_length, 0);
  for (GLint attrib_index = 0; attrib_index < num_attributes; attrib_index++) {
    GLsizei length = 0;
    GLint size = 0;
    GLenum type = GL_NONE;
    glGetActiveAttrib(service_program, attrib_index, attrib_name_buf.size(),
                      &length, &size, &type, attrib_name_buf.data());

    ProgramInput input;
    input.size = size;
    input.type = type;

    int32_t location =
        glGetAttribLocation(service_program, attrib_name_buf.data());
    input.location_offset = data->size();
    AppendValueToBuffer(data, location);

    input.name_offset = data->size();
    input.name_length = length;
    AppendStringToBuffer(data, attrib_name_buf.data(), length);

    InsertValueIntoBuffer(
        data, input,
        sizeof(ProgramInfoHeader) + (attrib_index * sizeof(ProgramInput)));
  }

  GLint active_uniform_max_length = 0;
  glGetProgramiv(service_program, GL_ACTIVE_UNIFORM_MAX_LENGTH,
                 &active_uniform_max_length);

  std::vector<char> uniform_name_buf(active_uniform_max_length, 0);
  for (GLint uniform_index = 0; uniform_index < num_uniforms; uniform_index++) {
    GLsizei length = 0;
    GLint size = 0;
    GLenum type = GL_NONE;
    glGetActiveUniform(service_program, uniform_index, uniform_name_buf.size(),
                       &length, &size, &type, uniform_name_buf.data());

    ProgramInput input;
    input.size = size;
    input.type = type;

    input.location_offset = data->size();
    int32_t base_location =
        glGetUniformLocation(service_program, uniform_name_buf.data());
    AppendValueToBuffer(data, base_location);

    GLSLArrayName parsed_service_name(uniform_name_buf.data());
    if (size > 1 || parsed_service_name.IsArrayName()) {
      for (GLint location_index = 1; location_index < size; location_index++) {
        std::string array_element_name = parsed_service_name.base_name() + "[" +
                                         base::IntToString(location_index) +
                                         "]";
        int32_t element_location =
            glGetUniformLocation(service_program, array_element_name.c_str());
        AppendValueToBuffer(data, element_location);
      }
    }

    input.name_offset = data->size();
    input.name_length = length;
    AppendStringToBuffer(data, uniform_name_buf.data(), length);

    InsertValueIntoBuffer(data, input, sizeof(ProgramInfoHeader) +
                                           ((num_attributes + uniform_index) *
                                            sizeof(ProgramInput)));
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformBlocksCHROMIUM(
    GLuint program,
    std::vector<uint8_t>* data) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoGetTransformFeedbackVaryingsCHROMIUM(
    GLuint program,
    std::vector<uint8_t>* data) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformsES3CHROMIUM(
    GLuint program,
    std::vector<uint8_t>* data) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetTranslatedShaderSourceANGLE(
    GLuint shader,
    std::string* source) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSwapBuffersWithDamageCHROMIUM(
    GLint x,
    GLint y,
    GLint width,
    GLint height) {
  gfx::SwapResult result = surface_->PostSubBuffer(x, y, width, height);
  if (result == gfx::SwapResult::SWAP_FAILED) {
    LOG(ERROR) << "Context lost because PostSubBuffer failed.";
  }
  // TODO(geofflang): force the context loss?
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
  glCopyTextureCHROMIUM(GetTextureServiceID(source_id, resources_, false),
                        GetTextureServiceID(dest_id, resources_, false),
                        internalformat, dest_type, unpack_flip_y,
                        unpack_premultiply_alpha, unpack_unmultiply_alpha);
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
  glCopySubTextureCHROMIUM(GetTextureServiceID(source_id, resources_, false),
                           GetTextureServiceID(dest_id, resources_, false),
                           xoffset, yoffset, x, y, width, height, unpack_flip_y,
                           unpack_premultiply_alpha, unpack_unmultiply_alpha);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCompressedCopyTextureCHROMIUM(
    GLenum source_id,
    GLenum dest_id) {
  glCompressedCopyTextureCHROMIUM(
      GetTextureServiceID(source_id, resources_, false),
      GetTextureServiceID(dest_id, resources_, false));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDrawArraysInstancedANGLE(
    GLenum mode,
    GLint first,
    GLsizei count,
    GLsizei primcount) {
  glDrawArraysInstancedANGLE(mode, first, count, primcount);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDrawElementsInstancedANGLE(
    GLenum mode,
    GLsizei count,
    GLenum type,
    const void* indices,
    GLsizei primcount) {
  glDrawElementsInstancedANGLE(mode, count, type, indices, primcount);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribDivisorANGLE(
    GLuint index,
    GLuint divisor) {
  glVertexAttribDivisorANGLE(index, divisor);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoProduceTextureCHROMIUM(
    GLenum target,
    const volatile GLbyte* mailbox) {
  // TODO(geofflang): validation

  GLuint texture_client_id = bound_textures_[active_texture_unit_];
  scoped_refptr<TexturePassthrough> texture;

  auto texture_object_iter =
      resources_->texture_object_map.find(texture_client_id);
  if (texture_object_iter != resources_->texture_object_map.end()) {
    texture = texture_object_iter->second.get();
  } else {
    GLuint service_id =
        GetTextureServiceID(texture_client_id, resources_, false);
    texture = new TexturePassthrough(service_id);
    resources_->texture_object_map.insert(
        std::make_pair(texture_client_id, texture));
  }

  const Mailbox& mb = Mailbox::FromVolatile(
      *reinterpret_cast<const volatile Mailbox*>(mailbox));
  mailbox_manager_->ProduceTexture(mb, texture.get());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoProduceTextureDirectCHROMIUM(
    GLuint texture_client_id,
    GLenum target,
    const volatile GLbyte* mailbox) {
  // TODO(geofflang): validation

  scoped_refptr<TexturePassthrough> texture;
  auto texture_object_iter =
      resources_->texture_object_map.find(texture_client_id);
  if (texture_object_iter != resources_->texture_object_map.end()) {
    texture = texture_object_iter->second.get();
  } else {
    GLuint service_id =
        GetTextureServiceID(texture_client_id, resources_, false);
    texture = new TexturePassthrough(service_id);
    resources_->texture_object_map.insert(
        std::make_pair(texture_client_id, texture));
  }

  const Mailbox& mb = Mailbox::FromVolatile(
      *reinterpret_cast<const volatile Mailbox*>(mailbox));
  mailbox_manager_->ProduceTexture(mb, texture.get());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoConsumeTextureCHROMIUM(
    GLenum target,
    const volatile GLbyte* mailbox) {
  // TODO(geofflang): validation

  const Mailbox& mb = Mailbox::FromVolatile(
      *reinterpret_cast<const volatile Mailbox*>(mailbox));
  scoped_refptr<TexturePassthrough> texture = static_cast<TexturePassthrough*>(
      group_->mailbox_manager()->ConsumeTexture(mb));
  if (texture == nullptr) {
    // TODO(geofflang): error, missing mailbox
    return error::kNoError;
  }

  GLuint client_id = bound_textures_[active_texture_unit_];
  resources_->texture_id_map.SetIDMapping(client_id, texture->service_id());
  resources_->texture_object_map.erase(client_id);
  resources_->texture_object_map.insert(std::make_pair(client_id, texture));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCreateAndConsumeTextureINTERNAL(
    GLenum target,
    GLuint texture_client_id,
    const volatile GLbyte* mailbox) {
  // TODO(geofflang): validation

  if (resources_->texture_id_map.GetServiceID(texture_client_id, nullptr)) {
    return error::kInvalidArguments;
  }
  const Mailbox& mb = Mailbox::FromVolatile(
      *reinterpret_cast<const volatile Mailbox*>(mailbox));
  scoped_refptr<TexturePassthrough> texture = static_cast<TexturePassthrough*>(
      group_->mailbox_manager()->ConsumeTexture(mb));
  if (texture == nullptr) {
    // TODO(geofflang): error, missing mailbox
    return error::kNoError;
  }

  resources_->texture_id_map.SetIDMapping(texture_client_id,
                                          texture->service_id());
  resources_->texture_object_map.erase(texture_client_id);
  resources_->texture_object_map.insert(
      std::make_pair(texture_client_id, texture));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindUniformLocationCHROMIUM(
    GLuint program,
    GLint location,
    const char* name) {
  glBindUniformLocationCHROMIUM(GetProgramServiceID(program, resources_),
                                location, name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindTexImage2DCHROMIUM(
    GLenum target,
    GLint imageId) {
  // TODO(geofflang): error handling
  gl::GLImage* image = image_manager_->LookupImage(imageId);
  if (!image->BindTexImage(target)) {
    image->CopyTexImage(target);
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoReleaseTexImage2DCHROMIUM(
    GLenum target,
    GLint imageId) {
  // TODO(geofflang): error handling
  gl::GLImage* image = image_manager_->LookupImage(imageId);
  image->ReleaseTexImage(target);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTraceBeginCHROMIUM(
    const char* category_name,
    const char* trace_name) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTraceEndCHROMIUM() {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDiscardFramebufferEXT(
    GLenum target,
    GLsizei count,
    const volatile GLenum* attachments) {
  std::vector<GLenum> attachments_copy(attachments, attachments + count);
  glDiscardFramebufferEXT(target, count, attachments_copy.data());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoLoseContextCHROMIUM(GLenum current,
                                                                GLenum other) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDescheduleUntilFinishedCHROMIUM() {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoInsertFenceSyncCHROMIUM(
    GLuint64 release_count) {
  if (!fence_sync_release_callback_.is_null())
    fence_sync_release_callback_.Run(release_count);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoWaitSyncTokenCHROMIUM(
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id,
    GLuint64 release_count) {
  if (wait_fence_sync_callback_.is_null()) {
    return error::kNoError;
  }
  return wait_fence_sync_callback_.Run(namespace_id, command_buffer_id,
                                       release_count)
             ? error::kNoError
             : error::kDeferCommandUntilLater;
}

error::Error GLES2DecoderPassthroughImpl::DoDrawBuffersEXT(
    GLsizei count,
    const volatile GLenum* bufs) {
  std::vector<GLenum> bufs_copy(bufs, bufs + count);
  glDrawBuffersARB(count, bufs_copy.data());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDiscardBackbufferCHROMIUM() {
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoScheduleCALayerSharedStateCHROMIUM(
    GLfloat opacity,
    GLboolean is_clipped,
    const GLfloat* clip_rect,
    GLint sorting_context_id,
    const GLfloat* transform) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoScheduleCALayerCHROMIUM(
    GLuint contents_texture_id,
    const GLfloat* contents_rect,
    GLuint background_color,
    GLuint edge_aa_mask,
    const GLfloat* bounds_rect) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoScheduleCALayerInUseQueryCHROMIUM(
    GLuint n,
    const volatile GLuint* textures) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCommitOverlayPlanesCHROMIUM() {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSwapInterval(GLint interval) {
  context_->SetSwapInterval(interval);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFlushDriverCachesCHROMIUM() {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoMatrixLoadfCHROMIUM(
    GLenum matrixMode,
    const volatile GLfloat* m) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoMatrixLoadIdentityCHROMIUM(
    GLenum matrixMode) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenPathsCHROMIUM(GLuint path,
                                                             GLsizei range) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDeletePathsCHROMIUM(GLuint path,
                                                                GLsizei range) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsPathCHROMIUM(GLuint path,
                                                           uint32_t* result) {
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPathParameterfCHROMIUM(
    GLuint path,
    GLenum pname,
    GLfloat value) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPathParameteriCHROMIUM(
    GLuint path,
    GLenum pname,
    GLint value) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPathStencilFuncCHROMIUM(
    GLenum func,
    GLint ref,
    GLuint mask) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilFillPathCHROMIUM(
    GLuint path,
    GLenum fillMode,
    GLuint mask) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilStrokePathCHROMIUM(
    GLuint path,
    GLint reference,
    GLuint mask) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCoverFillPathCHROMIUM(
    GLuint path,
    GLenum coverMode) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCoverStrokePathCHROMIUM(
    GLuint path,
    GLenum coverMode) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilThenCoverFillPathCHROMIUM(
    GLuint path,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilThenCoverStrokePathCHROMIUM(
    GLuint path,
    GLint reference,
    GLuint mask,
    GLenum coverMode) {
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindFragmentInputLocationCHROMIUM(
    GLuint program,
    GLint location,
    const char* name) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoProgramPathFragmentInputGenCHROMIUM(
    GLuint program,
    GLint location,
    GLenum genMode,
    GLint components,
    const GLfloat* coeffs,
    GLsizei coeffsBufsize) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCoverageModulationCHROMIUM(
    GLenum components) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendBarrierKHR() {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoApplyScreenSpaceAntialiasingCHROMIUM() {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindFragDataLocationIndexedEXT(
    GLuint program,
    GLuint colorNumber,
    GLuint index,
    const char* name) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindFragDataLocationEXT(
    GLuint program,
    GLuint colorNumber,
    const char* name) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetFragDataIndexEXT(
    GLuint program,
    const char* name,
    GLint* index) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoUniformMatrix4fvStreamTextureMatrixCHROMIUM(
    GLint location,
    GLboolean transpose,
    const volatile GLfloat* defaultValue) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

}  // namespace gles2
}  // namespace gpu
