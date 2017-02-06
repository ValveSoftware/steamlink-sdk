// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h"

namespace gpu {
namespace gles2 {

// Custom Handlers
error::Error GLES2DecoderPassthroughImpl::HandleBindAttribLocationBucket(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::BindAttribLocationBucket& c =
      *static_cast<const gles2::cmds::BindAttribLocationBucket*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint index = static_cast<GLuint>(c.index);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  error::Error error = DoBindAttribLocation(program, index, name_str.c_str());
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBufferData(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::BufferData& c =
      *static_cast<const gles2::cmds::BufferData*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  uint32_t data_shm_id = static_cast<uint32_t>(c.data_shm_id);
  uint32_t data_shm_offset = static_cast<uint32_t>(c.data_shm_offset);
  GLenum usage = static_cast<GLenum>(c.usage);
  const void* data = NULL;
  if (data_shm_id != 0 || data_shm_offset != 0) {
    data = GetSharedMemoryAs<const void*>(data_shm_id, data_shm_offset, size);
    if (!data) {
      return error::kOutOfBounds;
    }
  }
  error::Error error = DoBufferData(target, size, data, usage);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleClientWaitSync(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::ClientWaitSync& c =
      *static_cast<const gles2::cmds::ClientWaitSync*>(cmd_data);
  const GLuint sync = static_cast<GLuint>(c.sync);
  const GLbitfield flags = static_cast<GLbitfield>(c.flags);
  const GLuint64 timeout = c.timeout();
  typedef cmds::ClientWaitSync::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  error::Error error = DoClientWaitSync(sync, flags, timeout, result_dst);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleCreateProgram(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::CreateProgram& c =
      *static_cast<const gles2::cmds::CreateProgram*>(cmd_data);
  GLuint client_id = static_cast<GLuint>(c.client_id);
  error::Error error = DoCreateProgram(client_id);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleCreateShader(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::CreateShader& c =
      *static_cast<const gles2::cmds::CreateShader*>(cmd_data);
  GLenum type = static_cast<GLenum>(c.type);
  GLuint client_id = static_cast<GLuint>(c.client_id);
  error::Error error = DoCreateShader(type, client_id);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleFenceSync(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::FenceSync& c =
      *static_cast<const gles2::cmds::FenceSync*>(cmd_data);
  GLenum condition = static_cast<GLenum>(c.condition);
  GLbitfield flags = static_cast<GLbitfield>(c.flags);
  GLuint client_id = static_cast<GLuint>(c.client_id);
  error::Error error = DoFenceSync(condition, flags, client_id);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDrawArrays(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::DrawArrays& c =
      *static_cast<const gles2::cmds::DrawArrays*>(cmd_data);
  GLenum mode = static_cast<GLenum>(c.mode);
  GLint first = static_cast<GLint>(c.first);
  GLsizei count = static_cast<GLsizei>(c.count);
  error::Error error = DoDrawArrays(mode, first, count);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDrawElements(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::DrawElements& c =
      *static_cast<const gles2::cmds::DrawElements*>(cmd_data);
  GLenum mode = static_cast<GLenum>(c.mode);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLenum type = static_cast<GLenum>(c.type);
  const GLvoid* indices =
      reinterpret_cast<const GLvoid*>(static_cast<uintptr_t>(c.index_offset));
  error::Error error = DoDrawElements(mode, count, type, indices);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetActiveAttrib(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetActiveAttrib& c =
      *static_cast<const gles2::cmds::GetActiveAttrib*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint index = static_cast<GLuint>(c.index);
  uint32_t name_bucket_id = c.name_bucket_id;
  typedef cmds::GetActiveAttrib::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->success != 0) {
    return error::kInvalidArguments;
  }

  std::string name;
  error::Error error =
      DoGetActiveAttrib(program, index, &result->size, &result->type, &name);
  if (error != error::kNoError) {
    return error;
  }

  result->success = 1;  // true.
  Bucket* bucket = CreateBucket(name_bucket_id);
  bucket->SetFromString(name.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetActiveUniform(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetActiveUniform& c =
      *static_cast<const gles2::cmds::GetActiveUniform*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint index = static_cast<GLuint>(c.index);
  uint32_t name_bucket_id = c.name_bucket_id;
  typedef cmds::GetActiveUniform::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->success != 0) {
    return error::kInvalidArguments;
  }

  std::string name;
  error::Error error =
      DoGetActiveUniform(program, index, &result->size, &result->type, &name);
  if (error != error::kNoError) {
    return error;
  }

  result->success = 1;  // true.
  Bucket* bucket = CreateBucket(name_bucket_id);
  bucket->SetFromString(name.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetActiveUniformBlockiv(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetActiveUniformBlockiv& c =
      *static_cast<const gles2::cmds::GetActiveUniformBlockiv*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint uniformBlockIndex = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  unsigned int buffer_size = 0;
  typedef cmds::GetActiveUniformBlockiv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, &buffer_size);
  GLint* params = result ? result->GetData() : NULL;
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei length = 0;
  error::Error error = DoGetActiveUniformBlockiv(
      program, uniformBlockIndex, pname, bufsize, &length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (length > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(length);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetActiveUniformBlockName(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetActiveUniformBlockName& c =
      *static_cast<const gles2::cmds::GetActiveUniformBlockName*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint uniformBlockIndex = static_cast<GLuint>(c.index);
  uint32_t name_bucket_id = c.name_bucket_id;
  typedef cmds::GetActiveUniformBlockName::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (*result != 0) {
    return error::kInvalidArguments;
  }

  std::string name;
  error::Error error =
      DoGetActiveUniformBlockName(program, uniformBlockIndex, &name);
  if (error != error::kNoError) {
    return error;
  }

  *result = 1;
  Bucket* bucket = CreateBucket(name_bucket_id);
  bucket->SetFromString(name.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetActiveUniformsiv(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetActiveUniformsiv& c =
      *static_cast<const gles2::cmds::GetActiveUniformsiv*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLenum pname = static_cast<GLenum>(c.pname);
  Bucket* bucket = GetBucket(c.indices_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  GLsizei uniformCount = static_cast<GLsizei>(bucket->size() / sizeof(GLuint));
  const GLuint* indices = bucket->GetDataAs<const GLuint*>(0, bucket->size());
  typedef cmds::GetActiveUniformsiv::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(uniformCount));
  GLint* params = result ? result->GetData() : NULL;
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }

  GLsizei bufsize = uniformCount;
  GLsizei length = 0;
  error::Error error = DoGetActiveUniformsiv(program, uniformCount, indices,
                                             pname, bufsize, &length, params);
  if (error != error::kNoError) {
    return error;
  }

  result->SetNumResults(length);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetAttachedShaders(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetAttachedShaders& c =
      *static_cast<const gles2::cmds::GetAttachedShaders*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  typedef cmds::GetAttachedShaders::Result Result;
  uint32_t maxCount = Result::ComputeMaxResults(c.result_size);
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, Result::ComputeSize(maxCount));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  GLsizei count = 0;
  error::Error error =
      DoGetAttachedShaders(program, maxCount, &count, result->GetData());
  if (error != error::kNoError) {
    return error;
  }

  result->SetNumResults(count);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetAttribLocation(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetAttribLocation& c =
      *static_cast<const gles2::cmds::GetAttribLocation*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  GLint* location = GetSharedMemoryAs<GLint*>(
      c.location_shm_id, c.location_shm_offset, sizeof(GLint));
  if (!location) {
    return error::kOutOfBounds;
  }
  if (*location != -1) {
    return error::kInvalidArguments;
  }
  error::Error error = DoGetAttribLocation(program, name_str.c_str(), location);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetFragDataLocation(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetFragDataLocation& c =
      *static_cast<const gles2::cmds::GetFragDataLocation*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  GLint* location = GetSharedMemoryAs<GLint*>(
      c.location_shm_id, c.location_shm_offset, sizeof(GLint));
  if (!location) {
    return error::kOutOfBounds;
  }
  if (*location != -1) {
    return error::kInvalidArguments;
  }
  error::Error error =
      DoGetFragDataLocation(program, name_str.c_str(), location);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetInternalformativ(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetInternalformativ& c =
      *static_cast<const gles2::cmds::GetInternalformativ*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum internalformat = static_cast<GLenum>(c.format);
  GLenum pname = static_cast<GLenum>(c.pname);
  unsigned int buffer_size = 0;
  typedef cmds::GetInternalformativ::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, &buffer_size);
  GLint* params = result ? result->GetData() : NULL;
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei length = 0;
  error::Error error = DoGetInternalformativ(target, internalformat, pname,
                                             bufsize, &length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (length > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(length);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetProgramInfoLog(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetProgramInfoLog& c =
      *static_cast<const gles2::cmds::GetProgramInfoLog*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  uint32_t bucket_id = static_cast<uint32_t>(c.bucket_id);

  std::string infolog;
  error::Error error = DoGetProgramInfoLog(program, &infolog);
  if (error != error::kNoError) {
    return error;
  }

  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetFromString(infolog.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetShaderInfoLog(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetShaderInfoLog& c =
      *static_cast<const gles2::cmds::GetShaderInfoLog*>(cmd_data);
  GLuint shader = static_cast<GLuint>(c.shader);
  uint32_t bucket_id = static_cast<uint32_t>(c.bucket_id);

  std::string infolog;
  error::Error error = DoGetShaderInfoLog(shader, &infolog);
  if (error != error::kNoError) {
    return error;
  }

  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetFromString(infolog.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetShaderPrecisionFormat(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetShaderPrecisionFormat& c =
      *static_cast<const gles2::cmds::GetShaderPrecisionFormat*>(cmd_data);
  GLenum shader_type = static_cast<GLenum>(c.shadertype);
  GLenum precision_type = static_cast<GLenum>(c.precisiontype);
  typedef cmds::GetShaderPrecisionFormat::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->success != 0) {
    return error::kInvalidArguments;
  }

  GLint range[2] = {0, 0};
  GLint precision = 0;
  error::Error error = DoGetShaderPrecisionFormat(shader_type, precision_type,
                                                  range, &precision);
  if (error != error::kNoError) {
    return error;
  }

  result->success = 1;  // true
  result->min_range = range[0];
  result->max_range = range[1];
  result->precision = precision;

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetShaderSource(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetShaderSource& c =
      *static_cast<const gles2::cmds::GetShaderSource*>(cmd_data);
  GLuint shader = static_cast<GLuint>(c.shader);
  uint32_t bucket_id = static_cast<uint32_t>(c.bucket_id);

  std::string source;
  error::Error error = DoGetShaderSource(shader, &source);
  if (error != error::kNoError) {
    return error;
  }

  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetFromString(source.c_str());

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetString(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetString& c =
      *static_cast<const gles2::cmds::GetString*>(cmd_data);
  GLenum name = static_cast<GLenum>(c.name);

  const char* str = nullptr;
  error::Error error = DoGetString(name, &str);
  if (error != error::kNoError) {
    return error;
  }
  if (!str) {
    return error::kOutOfBounds;
  }
  Bucket* bucket = CreateBucket(c.bucket_id);
  bucket->SetFromString(str);

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetTransformFeedbackVarying(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetTransformFeedbackVarying& c =
      *static_cast<const gles2::cmds::GetTransformFeedbackVarying*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint index = static_cast<GLuint>(c.index);
  uint32_t name_bucket_id = c.name_bucket_id;
  typedef cmds::GetTransformFeedbackVarying::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->success != 0) {
    return error::kInvalidArguments;
  }

  GLsizei size = 0;
  GLenum type = 0;
  std::string name;
  error::Error error =
      DoGetTransformFeedbackVarying(program, index, &size, &type, &name);
  if (error != error::kNoError) {
    return error;
  }

  result->success = 1;  // true.
  result->size = static_cast<int32_t>(size);
  result->type = static_cast<uint32_t>(type);
  Bucket* bucket = CreateBucket(name_bucket_id);
  bucket->SetFromString(name.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetUniformBlockIndex(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetUniformBlockIndex& c =
      *static_cast<const gles2::cmds::GetUniformBlockIndex*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  GLint* index = GetSharedMemoryAs<GLint*>(c.index_shm_id, c.index_shm_offset,
                                           sizeof(GLint));
  if (!index) {
    return error::kOutOfBounds;
  }
  if (*index != -1) {
    return error::kInvalidArguments;
  }
  error::Error error = DoGetUniformBlockIndex(program, name_str.c_str(), index);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetUniformfv(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetUniformfv& c =
      *static_cast<const gles2::cmds::GetUniformfv*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLint location = static_cast<GLint>(c.location);
  unsigned int buffer_size = 0;
  typedef cmds::GetUniformfv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, &buffer_size);
  GLfloat* params = result ? result->GetData() : NULL;
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei length = 0;
  error::Error error =
      DoGetUniformfv(program, location, bufsize, &length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (length > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(length);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetUniformiv(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetUniformiv& c =
      *static_cast<const gles2::cmds::GetUniformiv*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLint location = static_cast<GLint>(c.location);
  unsigned int buffer_size = 0;
  typedef cmds::GetUniformiv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, &buffer_size);
  GLint* params = result ? result->GetData() : NULL;
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei length = 0;
  error::Error error =
      DoGetUniformiv(program, location, bufsize, &length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (length > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(length);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetUniformuiv(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetUniformuiv& c =
      *static_cast<const gles2::cmds::GetUniformuiv*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLint location = static_cast<GLint>(c.location);
  unsigned int buffer_size = 0;
  typedef cmds::GetUniformuiv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, &buffer_size);
  GLuint* params = result ? result->GetData() : NULL;
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei length = 0;
  error::Error error =
      DoGetUniformuiv(program, location, bufsize, &length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (length > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(length);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetUniformIndices(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetUniformIndices& c =
      *static_cast<const gles2::cmds::GetUniformIndices*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  Bucket* bucket = GetBucket(c.names_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  GLsizei count = 0;
  std::vector<char*> names;
  std::vector<GLint> len;
  if (!bucket->GetAsStrings(&count, &names, &len) || count <= 0) {
    return error::kInvalidArguments;
  }
  typedef cmds::GetUniformIndices::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.indices_shm_id, c.indices_shm_offset,
      Result::ComputeSize(static_cast<size_t>(count)));
  GLuint* indices = result ? result->GetData() : NULL;
  if (indices == NULL) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  GLsizei length = 0;
  error::Error error =
      DoGetUniformIndices(program, count, &names[0], count, &length, indices);
  if (error != error::kNoError) {
    return error;
  }
  if (length != count) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(length);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetUniformLocation(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetUniformLocation& c =
      *static_cast<const gles2::cmds::GetUniformLocation*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  GLint* location = GetSharedMemoryAs<GLint*>(
      c.location_shm_id, c.location_shm_offset, sizeof(GLint));
  if (!location) {
    return error::kOutOfBounds;
  }
  if (*location != -1) {
    return error::kInvalidArguments;
  }
  error::Error error =
      DoGetUniformLocation(program, name_str.c_str(), location);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetVertexAttribPointerv(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetVertexAttribPointerv& c =
      *static_cast<const gles2::cmds::GetVertexAttribPointerv*>(cmd_data);
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  unsigned int buffer_size = 0;
  typedef cmds::GetVertexAttribPointerv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.pointer_shm_id, c.pointer_shm_offset, &buffer_size);
  GLuint* params = result ? result->GetData() : NULL;
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei length = 0;
  error::Error error =
      DoGetVertexAttribPointerv(index, pname, bufsize, &length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (length > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(length);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandlePixelStorei(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::PixelStorei& c =
      *static_cast<const gles2::cmds::PixelStorei*>(cmd_data);
  GLenum pname = static_cast<GLuint>(c.pname);
  GLint param = static_cast<GLint>(c.param);
  error::Error error = DoPixelStorei(pname, param);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleReadPixels(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::ReadPixels& c =
      *static_cast<const gles2::cmds::ReadPixels*>(cmd_data);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);

  uint8_t* pixels = nullptr;
  unsigned int buffer_size = 0;
  if (c.pixels_shm_id != 0) {
    pixels = GetSharedMemoryAndSizeAs<uint8_t*>(
        c.pixels_shm_id, c.pixels_shm_offset, &buffer_size);
    if (!pixels) {
      return error::kOutOfBounds;
    }
  }

  GLsizei bufsize = buffer_size;
  GLsizei length = 0;
  error::Error error =
      DoReadPixels(x, y, width, height, format, type, bufsize, &length, pixels);
  if (error != error::kNoError) {
    return error;
  }
  if (length > bufsize) {
    return error::kOutOfBounds;
  }

  typedef cmds::ReadPixels::Result Result;
  Result* result = nullptr;
  if (c.result_shm_id != 0) {
    result = GetSharedMemoryAs<Result*>(c.result_shm_id, c.result_shm_offset,
                                        sizeof(*result));
    if (!result) {
      return error::kOutOfBounds;
    }
    if (result->success != 0) {
      return error::kInvalidArguments;
    }
  }

  if (result) {
    result->success = 1;
    result->row_length = static_cast<uint32_t>(width);
    result->num_rows = static_cast<uint32_t>(height);
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleShaderBinary(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::ShaderBinary& c =
      *static_cast<const gles2::cmds::ShaderBinary*>(cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  GLsizei length = static_cast<GLsizei>(c.length);
  uint32_t data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  const GLuint* shaders = GetSharedMemoryAs<const GLuint*>(
      c.shaders_shm_id, c.shaders_shm_offset, data_size);
  GLenum binaryformat = static_cast<GLenum>(c.binaryformat);
  const void* binary = GetSharedMemoryAs<const void*>(
      c.binary_shm_id, c.binary_shm_offset, length);
  if (shaders == NULL || binary == NULL) {
    return error::kOutOfBounds;
  }

  error::Error error = DoShaderBinary(n, shaders, binaryformat, binary, length);
  if (error != error::kNoError) {
    return error;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleTexImage2D(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::TexImage2D& c =
      *static_cast<const gles2::cmds::TexImage2D*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint internal_format = static_cast<GLint>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);

  GLsizei imagesize = 0;
  const void* pixels = NULL;
  if (c.pixels_shm_id != 0 || c.pixels_shm_offset != 0) {
    unsigned int buffer_size = 0;
    pixels = GetSharedMemoryAndSizeAs<uint8_t*>(
        c.pixels_shm_id, c.pixels_shm_offset, &buffer_size);
    if (!pixels) {
      return error::kOutOfBounds;
    }
    imagesize = buffer_size;
  }

  error::Error error =
      DoTexImage2D(target, level, internal_format, width, height, border,
                   format, type, imagesize, pixels);
  if (error != error::kNoError) {
    return error;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleTexImage3D(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::TexImage3D& c =
      *static_cast<const gles2::cmds::TexImage3D*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint internal_format = static_cast<GLint>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLsizei depth = static_cast<GLsizei>(c.depth);
  GLint border = static_cast<GLint>(c.border);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);

  GLsizei imagesize = 0;
  const void* pixels = NULL;
  if (c.pixels_shm_id != 0 || c.pixels_shm_offset != 0) {
    unsigned int buffer_size = 0;
    pixels = GetSharedMemoryAndSizeAs<uint8_t*>(
        c.pixels_shm_id, c.pixels_shm_offset, &buffer_size);
    if (!pixels) {
      return error::kOutOfBounds;
    }
    imagesize = buffer_size;
  }

  error::Error error =
      DoTexImage3D(target, level, internal_format, width, height, depth, border,
                   format, type, imagesize, pixels);
  if (error != error::kNoError) {
    return error;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleTexSubImage2D(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::TexSubImage2D& c =
      *static_cast<const gles2::cmds::TexSubImage2D*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);

  unsigned int buffer_size = 0;
  const void* pixels = GetSharedMemoryAndSizeAs<uint8_t*>(
      c.pixels_shm_id, c.pixels_shm_offset, &buffer_size);
  if (!pixels) {
    return error::kOutOfBounds;
  }
  GLsizei imagesize = buffer_size;

  error::Error error = DoTexSubImage2D(target, level, xoffset, yoffset, width,
                                       height, format, type, imagesize, pixels);
  if (error != error::kNoError) {
    return error;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleTexSubImage3D(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::TexSubImage3D& c =
      *static_cast<const gles2::cmds::TexSubImage3D*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint zoffset = static_cast<GLint>(c.zoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLsizei depth = static_cast<GLsizei>(c.depth);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);

  unsigned int buffer_size = 0;
  const void* pixels = GetSharedMemoryAndSizeAs<uint8_t*>(
      c.pixels_shm_id, c.pixels_shm_offset, &buffer_size);
  if (!pixels) {
    return error::kOutOfBounds;
  }
  GLsizei imagesize = buffer_size;

  error::Error error =
      DoTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height,
                      depth, format, type, imagesize, pixels);
  if (error != error::kNoError) {
    return error;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniformBlockBinding(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::UniformBlockBinding& c =
      *static_cast<const gles2::cmds::UniformBlockBinding*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint index = static_cast<GLuint>(c.index);
  GLuint binding = static_cast<GLuint>(c.binding);

  error::Error error = DoUniformBlockBinding(program, index, binding);
  if (error != error::kNoError) {
    return error;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleVertexAttribIPointer(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::VertexAttribIPointer& c =
      *static_cast<const gles2::cmds::VertexAttribIPointer*>(cmd_data);
  GLuint index = static_cast<GLuint>(c.indx);
  GLint size = static_cast<GLint>(c.size);
  GLenum type = static_cast<GLenum>(c.type);
  GLsizei stride = static_cast<GLsizei>(c.stride);
  GLsizei offset = static_cast<GLsizei>(c.offset);
  const void* ptr = reinterpret_cast<const void*>(offset);

  error::Error error = DoVertexAttribIPointer(index, size, type, stride, ptr);
  if (error != error::kNoError) {
    return error;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleVertexAttribPointer(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::VertexAttribPointer& c =
      *static_cast<const gles2::cmds::VertexAttribPointer*>(cmd_data);
  GLuint index = static_cast<GLuint>(c.indx);
  GLint size = static_cast<GLint>(c.size);
  GLenum type = static_cast<GLenum>(c.type);
  GLboolean normalized = static_cast<GLenum>(c.normalized);
  GLsizei stride = static_cast<GLsizei>(c.stride);
  GLsizei offset = static_cast<GLsizei>(c.offset);
  const void* ptr = reinterpret_cast<const void*>(offset);

  error::Error error =
      DoVertexAttribPointer(index, size, type, normalized, stride, ptr);
  if (error != error::kNoError) {
    return error;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleWaitSync(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::WaitSync& c =
      *static_cast<const gles2::cmds::WaitSync*>(cmd_data);
  const GLuint sync = static_cast<GLuint>(c.sync);
  const GLbitfield flags = static_cast<GLbitfield>(c.flags);
  const GLuint64 timeout = c.timeout();

  error::Error error = DoWaitSync(sync, flags, timeout);
  if (error != error::kNoError) {
    return error;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleQueryCounterEXT(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::QueryCounterEXT& c =
      *static_cast<const gles2::cmds::QueryCounterEXT*>(cmd_data);
  GLuint id = static_cast<GLuint>(c.id);
  GLenum target = static_cast<GLenum>(c.target);

  error::Error error = DoQueryCounterEXT(id, target);
  if (error != error::kNoError) {
    return error;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBeginQueryEXT(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::BeginQueryEXT& c =
      *static_cast<const gles2::cmds::BeginQueryEXT*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint id = static_cast<GLuint>(c.id);

  error::Error error = DoBeginQueryEXT(target, id);
  if (error != error::kNoError) {
    return error;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleEndQueryEXT(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::EndQueryEXT& c =
      *static_cast<const gles2::cmds::EndQueryEXT*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);

  error::Error error = DoEndQueryEXT(target);
  if (error != error::kNoError) {
    return error;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleSetDisjointValueSyncCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::SetDisjointValueSyncCHROMIUM& c =
      *static_cast<const gles2::cmds::SetDisjointValueSyncCHROMIUM*>(cmd_data);
  DisjointValueSync* sync = GetSharedMemoryAs<DisjointValueSync*>(
      c.sync_data_shm_id, c.sync_data_shm_offset, sizeof(*sync));
  if (!sync) {
    return error::kOutOfBounds;
  }
  error::Error error = DoSetDisjointValueSyncCHROMIUM(sync);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleInsertEventMarkerEXT(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::InsertEventMarkerEXT& c =
      *static_cast<const gles2::cmds::InsertEventMarkerEXT*>(cmd_data);
  GLuint bucket_id = static_cast<GLuint>(c.bucket_id);
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string str;
  if (!bucket->GetAsString(&str)) {
    return error::kInvalidArguments;
  }
  error::Error error = DoInsertEventMarkerEXT(0, str.c_str());
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandlePushGroupMarkerEXT(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::PushGroupMarkerEXT& c =
      *static_cast<const gles2::cmds::PushGroupMarkerEXT*>(cmd_data);
  GLuint bucket_id = static_cast<GLuint>(c.bucket_id);
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string str;
  if (!bucket->GetAsString(&str)) {
    return error::kInvalidArguments;
  }
  error::Error error = DoPushGroupMarkerEXT(0, str.c_str());
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleEnableFeatureCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::EnableFeatureCHROMIUM& c =
      *static_cast<const gles2::cmds::EnableFeatureCHROMIUM*>(cmd_data);
  Bucket* bucket = GetBucket(c.bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  typedef cmds::EnableFeatureCHROMIUM::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (*result != 0) {
    return error::kInvalidArguments;
  }
  std::string feature_str;
  if (!bucket->GetAsString(&feature_str)) {
    return error::kInvalidArguments;
  }
  error::Error error = DoEnableFeatureCHROMIUM(feature_str.c_str());
  if (error != error::kNoError) {
    return error;
  }

  *result = 1;  // true.
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleMapBufferRange(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::MapBufferRange& c =
      *static_cast<const gles2::cmds::MapBufferRange*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLbitfield access = static_cast<GLbitfield>(c.access);
  GLintptr offset = static_cast<GLintptr>(c.offset);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);

  typedef cmds::MapBufferRange::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  if (*result != 0) {
    *result = 0;
    return error::kInvalidArguments;
  }
  uint8_t* mem =
      GetSharedMemoryAs<uint8_t*>(c.data_shm_id, c.data_shm_offset, size);
  if (!mem) {
    return error::kOutOfBounds;
  }

  void* ptr = nullptr;
  error::Error error = DoMapBufferRange(target, offset, size, access, &ptr);
  if (error != error::kNoError) {
    return error;
  }

  *result = 1;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUnmapBuffer(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::UnmapBuffer& c =
      *static_cast<const gles2::cmds::UnmapBuffer*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  error::Error error = DoUnmapBuffer(target);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleResizeCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::ResizeCHROMIUM& c =
      *static_cast<const gles2::cmds::ResizeCHROMIUM*>(cmd_data);
  GLuint width = static_cast<GLuint>(c.width);
  GLuint height = static_cast<GLuint>(c.height);
  GLfloat scale_factor = static_cast<GLfloat>(c.scale_factor);
  GLboolean has_alpha = static_cast<GLboolean>(c.alpha);
  error::Error error = DoResizeCHROMIUM(width, height, scale_factor, has_alpha);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleGetRequestableExtensionsCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetRequestableExtensionsCHROMIUM& c =
      *static_cast<const gles2::cmds::GetRequestableExtensionsCHROMIUM*>(
          cmd_data);
  const char* str = nullptr;
  error::Error error = DoGetRequestableExtensionsCHROMIUM(&str);
  if (error != error::kNoError) {
    return error;
  }
  if (!str) {
    return error::kOutOfBounds;
  }
  Bucket* bucket = CreateBucket(c.bucket_id);
  bucket->SetFromString(str);

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleRequestExtensionCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::RequestExtensionCHROMIUM& c =
      *static_cast<const gles2::cmds::RequestExtensionCHROMIUM*>(cmd_data);
  Bucket* bucket = GetBucket(c.bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string feature_str;
  if (!bucket->GetAsString(&feature_str)) {
    return error::kInvalidArguments;
  }
  error::Error error = DoRequestExtensionCHROMIUM(feature_str.c_str());
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetProgramInfoCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetProgramInfoCHROMIUM& c =
      *static_cast<const gles2::cmds::GetProgramInfoCHROMIUM*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);

  uint32_t bucket_id = c.bucket_id;
  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetSize(sizeof(ProgramInfoHeader));  // in case we fail.

  std::vector<uint8_t> data;
  error::Error error = DoGetProgramInfoCHROMIUM(program, &data);
  if (error != error::kNoError) {
    return error;
  }

  bucket->SetSize(data.size());
  bucket->SetData(data.data(), 0, data.size());

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetUniformBlocksCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetUniformBlocksCHROMIUM& c =
      *static_cast<const gles2::cmds::GetUniformBlocksCHROMIUM*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);

  uint32_t bucket_id = c.bucket_id;
  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetSize(sizeof(UniformBlocksHeader));  // in case we fail.

  std::vector<uint8_t> data;
  error::Error error = DoGetUniformBlocksCHROMIUM(program, &data);
  if (error != error::kNoError) {
    return error;
  }

  bucket->SetSize(data.size());
  bucket->SetData(data.data(), 0, data.size());

  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleGetTransformFeedbackVaryingsCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetTransformFeedbackVaryingsCHROMIUM& c =
      *static_cast<const gles2::cmds::GetTransformFeedbackVaryingsCHROMIUM*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);

  uint32_t bucket_id = c.bucket_id;
  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetSize(sizeof(TransformFeedbackVaryingsHeader));  // in case we fail.

  std::vector<uint8_t> data;
  error::Error error = DoGetTransformFeedbackVaryingsCHROMIUM(program, &data);
  if (error != error::kNoError) {
    return error;
  }

  bucket->SetSize(data.size());
  bucket->SetData(data.data(), 0, data.size());

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetUniformsES3CHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetUniformsES3CHROMIUM& c =
      *static_cast<const gles2::cmds::GetUniformsES3CHROMIUM*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);

  uint32_t bucket_id = c.bucket_id;
  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetSize(sizeof(UniformsES3Header));  // in case we fail.

  std::vector<uint8_t> data;
  error::Error error = DoGetUniformsES3CHROMIUM(program, &data);
  if (error != error::kNoError) {
    return error;
  }

  bucket->SetSize(data.size());
  bucket->SetData(data.data(), 0, data.size());

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetTranslatedShaderSourceANGLE(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetTranslatedShaderSourceANGLE& c =
      *static_cast<const gles2::cmds::GetTranslatedShaderSourceANGLE*>(
          cmd_data);
  GLuint shader = static_cast<GLuint>(c.shader);

  std::string source;
  error::Error error = DoGetTranslatedShaderSourceANGLE(shader, &source);
  if (error != error::kNoError) {
    return error;
  }

  Bucket* bucket = CreateBucket(c.bucket_id);
  bucket->SetFromString(source.c_str());

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandlePostSubBufferCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::PostSubBufferCHROMIUM& c =
      *static_cast<const gles2::cmds::PostSubBufferCHROMIUM*>(cmd_data);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLint width = static_cast<GLint>(c.width);
  GLint height = static_cast<GLint>(c.height);
  error::Error error = DoPostSubBufferCHROMIUM(x, y, width, height);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDrawArraysInstancedANGLE(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::DrawArraysInstancedANGLE& c =
      *static_cast<const gles2::cmds::DrawArraysInstancedANGLE*>(cmd_data);
  GLenum mode = static_cast<GLenum>(c.mode);
  GLint first = static_cast<GLint>(c.first);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLsizei primcount = static_cast<GLsizei>(c.primcount);
  error::Error error =
      DoDrawArraysInstancedANGLE(mode, first, count, primcount);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDrawElementsInstancedANGLE(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::DrawElementsInstancedANGLE& c =
      *static_cast<const gles2::cmds::DrawElementsInstancedANGLE*>(cmd_data);
  GLenum mode = static_cast<GLenum>(c.mode);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLenum type = static_cast<GLenum>(c.type);
  const GLvoid* indices =
      reinterpret_cast<const GLvoid*>(static_cast<uintptr_t>(c.index_offset));
  GLsizei primcount = static_cast<GLsizei>(c.primcount);
  error::Error error =
      DoDrawElementsInstancedANGLE(mode, count, type, indices, primcount);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleVertexAttribDivisorANGLE(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::VertexAttribDivisorANGLE& c =
      *static_cast<const gles2::cmds::VertexAttribDivisorANGLE*>(cmd_data);
  GLuint index = static_cast<GLuint>(c.index);
  GLuint divisor = static_cast<GLuint>(c.divisor);
  error::Error error = DoVertexAttribDivisorANGLE(index, divisor);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGenMailboxCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GenMailboxCHROMIUM& c =
      *static_cast<const gles2::cmds::GenMailboxCHROMIUM*>(cmd_data);
  (void)c;
  return error::kUnknownCommand;
}

error::Error
GLES2DecoderPassthroughImpl::HandleCreateAndConsumeTextureCHROMIUMImmediate(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::CreateAndConsumeTextureCHROMIUMImmediate& c = *static_cast<
      const gles2::cmds::CreateAndConsumeTextureCHROMIUMImmediate*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  uint32_t data_size;
  if (!GLES2Util::ComputeDataSize(1, sizeof(GLbyte), 64, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  const GLbyte* mailbox =
      GetImmediateDataAs<const GLbyte*>(c, data_size, immediate_data_size);
  if (mailbox == NULL) {
    return error::kOutOfBounds;
  }
  uint32_t client_id = c.client_id;
  error::Error error =
      DoCreateAndConsumeTextureCHROMIUM(target, mailbox, client_id);
  if (error != error::kNoError) {
    return error;
  }

  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleBindUniformLocationCHROMIUMBucket(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::BindUniformLocationCHROMIUMBucket& c =
      *static_cast<const gles2::cmds::BindUniformLocationCHROMIUMBucket*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLint location = static_cast<GLint>(c.location);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  error::Error error =
      DoBindUniformLocationCHROMIUM(program, location, name_str.c_str());
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleTraceBeginCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::TraceBeginCHROMIUM& c =
      *static_cast<const gles2::cmds::TraceBeginCHROMIUM*>(cmd_data);
  Bucket* category_bucket = GetBucket(c.category_bucket_id);
  Bucket* name_bucket = GetBucket(c.name_bucket_id);
  if (!category_bucket || category_bucket->size() == 0 || !name_bucket ||
      name_bucket->size() == 0) {
    return error::kInvalidArguments;
  }

  std::string category_name;
  std::string trace_name;
  if (!category_bucket->GetAsString(&category_name) ||
      !name_bucket->GetAsString(&trace_name)) {
    return error::kInvalidArguments;
  }

  error::Error error =
      DoTraceBeginCHROMIUM(category_name.c_str(), trace_name.c_str());
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDescheduleUntilFinishedCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::DescheduleUntilFinishedCHROMIUM& c =
      *static_cast<const gles2::cmds::DescheduleUntilFinishedCHROMIUM*>(
          cmd_data);
  (void)c;

  error::Error error = DoDescheduleUntilFinishedCHROMIUM();
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleInsertFenceSyncCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::InsertFenceSyncCHROMIUM& c =
      *static_cast<const gles2::cmds::InsertFenceSyncCHROMIUM*>(cmd_data);
  GLuint64 release_count = c.release_count();
  error::Error error = DoInsertFenceSyncCHROMIUM(release_count);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGenSyncTokenCHROMIUMImmediate(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GenSyncTokenCHROMIUMImmediate& c =
      *static_cast<const gles2::cmds::GenSyncTokenCHROMIUMImmediate*>(cmd_data);
  (void)c;
  return error::kUnknownCommand;
}

error::Error
GLES2DecoderPassthroughImpl::HandleGenUnverifiedSyncTokenCHROMIUMImmediate(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GenUnverifiedSyncTokenCHROMIUMImmediate& c =
      *static_cast<const gles2::cmds::GenUnverifiedSyncTokenCHROMIUMImmediate*>(
          cmd_data);
  (void)c;
  return error::kUnknownCommand;
}

error::Error
GLES2DecoderPassthroughImpl::HandleVerifySyncTokensCHROMIUMImmediate(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::VerifySyncTokensCHROMIUMImmediate& c =
      *static_cast<const gles2::cmds::VerifySyncTokensCHROMIUMImmediate*>(
          cmd_data);
  (void)c;
  return error::kUnknownCommand;
}

error::Error GLES2DecoderPassthroughImpl::HandleWaitSyncTokenCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::WaitSyncTokenCHROMIUM& c =
      *static_cast<const gles2::cmds::WaitSyncTokenCHROMIUM*>(cmd_data);
  CommandBufferNamespace namespace_id =
      static_cast<gpu::CommandBufferNamespace>(c.namespace_id);
  CommandBufferId command_buffer_id =
      CommandBufferId::FromUnsafeValue(c.command_buffer_id());
  const uint64_t release_count = c.release_count();

  const CommandBufferNamespace kMinNamespaceId =
      CommandBufferNamespace::INVALID;
  const CommandBufferNamespace kMaxNamespaceId =
      CommandBufferNamespace::NUM_COMMAND_BUFFER_NAMESPACES;
  if ((namespace_id < static_cast<int32_t>(kMinNamespaceId)) ||
      (namespace_id >= static_cast<int32_t>(kMaxNamespaceId))) {
    namespace_id = gpu::CommandBufferNamespace::INVALID;
  }

  error::Error error =
      DoWaitSyncTokenCHROMIUM(namespace_id, command_buffer_id, release_count);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDiscardBackbufferCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::DiscardBackbufferCHROMIUM& c =
      *static_cast<const gles2::cmds::DiscardBackbufferCHROMIUM*>(cmd_data);
  (void)c;
  error::Error error = DoDiscardBackbufferCHROMIUM();
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleScheduleOverlayPlaneCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::ScheduleOverlayPlaneCHROMIUM& c =
      *static_cast<const gles2::cmds::ScheduleOverlayPlaneCHROMIUM*>(cmd_data);
  GLint plane_z_order = static_cast<GLint>(c.plane_z_order);
  GLenum plane_transform = static_cast<GLenum>(c.plane_transform);
  GLuint overlay_texture_id = static_cast<GLuint>(c.overlay_texture_id);
  GLint bounds_x = static_cast<GLint>(c.bounds_x);
  GLint bounds_y = static_cast<GLint>(c.bounds_y);
  GLint bounds_width = static_cast<GLint>(c.bounds_width);
  GLint bounds_height = static_cast<GLint>(c.bounds_height);
  GLfloat uv_x = static_cast<GLfloat>(c.uv_x);
  GLfloat uv_y = static_cast<GLfloat>(c.uv_x);
  GLfloat uv_width = static_cast<GLfloat>(c.uv_x);
  GLfloat uv_height = static_cast<GLfloat>(c.uv_x);
  error::Error error = DoScheduleOverlayPlaneCHROMIUM(
      plane_z_order, plane_transform, overlay_texture_id, bounds_x, bounds_y,
      bounds_width, bounds_height, uv_x, uv_y, uv_width, uv_height);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleScheduleCALayerCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::ScheduleCALayerCHROMIUM& c =
      *static_cast<const gles2::cmds::ScheduleCALayerCHROMIUM*>(cmd_data);
  const GLfloat* mem = GetSharedMemoryAs<const GLfloat*>(c.shm_id, c.shm_offset,
                                                         28 * sizeof(GLfloat));
  if (!mem) {
    return error::kOutOfBounds;
  }
  GLuint contents_texture_id = static_cast<GLint>(c.contents_texture_id);
  const GLfloat* contents_rect = mem;
  GLfloat opacity = static_cast<GLfloat>(c.opacity);
  GLuint background_color = static_cast<GLuint>(c.background_color);
  GLuint edge_aa_mask = static_cast<GLuint>(c.edge_aa_mask);
  const GLfloat* bounds_rect = mem + 4;
  GLboolean is_clipped = static_cast<GLboolean>(c.is_clipped);
  const GLfloat* clip_rect = mem + 8;
  GLint sorting_context_id = static_cast<GLint>(c.sorting_context_id);
  const GLfloat* transform = mem + 12;
  error::Error error = DoScheduleCALayerCHROMIUM(
      contents_texture_id, contents_rect, opacity, background_color,
      edge_aa_mask, bounds_rect, is_clipped, clip_rect, sorting_context_id,
      transform);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGenPathsCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GenPathsCHROMIUM& c =
      *static_cast<const gles2::cmds::GenPathsCHROMIUM*>(cmd_data);
  GLuint path = static_cast<GLuint>(c.first_client_id);
  GLsizei range = static_cast<GLsizei>(c.range);
  error::Error error = DoGenPathsCHROMIUM(path, range);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDeletePathsCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::DeletePathsCHROMIUM& c =
      *static_cast<const gles2::cmds::DeletePathsCHROMIUM*>(cmd_data);
  GLuint path = static_cast<GLuint>(c.first_client_id);
  GLsizei range = static_cast<GLsizei>(c.range);
  error::Error error = DoDeletePathsCHROMIUM(path, range);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandlePathCommandsCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::PathCommandsCHROMIUM& c =
      *static_cast<const gles2::cmds::PathCommandsCHROMIUM*>(cmd_data);
  GLuint path = static_cast<GLuint>(c.path);
  GLsizei num_commands = static_cast<GLsizei>(c.numCommands);
  const GLubyte* commands = nullptr;
  if (num_commands > 0) {
    uint32_t commands_shm_id = static_cast<uint32_t>(c.commands_shm_id);
    uint32_t commands_shm_offset = static_cast<uint32_t>(c.commands_shm_offset);
    if (commands_shm_id != 0 || commands_shm_offset != 0) {
      commands = GetSharedMemoryAs<const GLubyte*>(
          commands_shm_id, commands_shm_offset, num_commands);
    }
    if (!commands) {
      return error::kOutOfBounds;
    }
  }
  GLsizei num_coords = static_cast<GLsizei>(c.numCoords);
  GLenum coord_type = static_cast<GLenum>(c.coordType);
  const GLvoid* coords = nullptr;
  GLsizei coords_bufsize = 0;
  if (num_coords > 0) {
    uint32_t coords_shm_id = static_cast<uint32_t>(c.coords_shm_id);
    uint32_t coords_shm_offset = static_cast<uint32_t>(c.coords_shm_offset);
    if (coords_shm_id != 0 || coords_shm_offset != 0) {
      unsigned int memory_size = 0;
      coords = GetSharedMemoryAndSizeAs<const GLvoid*>(
          coords_shm_id, coords_shm_offset, &memory_size);
      coords_bufsize = static_cast<GLsizei>(memory_size);
    }

    if (!coords) {
      return error::kOutOfBounds;
    }
  }

  error::Error error =
      DoPathCommandsCHROMIUM(path, num_commands, commands, num_coords,
                             coord_type, coords, coords_bufsize);
  if (error != error::kNoError) {
    return error;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandlePathParameterfCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::PathParameterfCHROMIUM& c =
      *static_cast<const gles2::cmds::PathParameterfCHROMIUM*>(cmd_data);
  GLuint path = static_cast<GLuint>(c.path);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLfloat value = static_cast<GLfloat>(c.value);
  error::Error error = DoPathParameterfCHROMIUM(path, pname, value);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandlePathParameteriCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::PathParameteriCHROMIUM& c =
      *static_cast<const gles2::cmds::PathParameteriCHROMIUM*>(cmd_data);
  GLuint path = static_cast<GLuint>(c.path);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint value = static_cast<GLint>(c.value);
  error::Error error = DoPathParameteriCHROMIUM(path, pname, value);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleStencilFillPathCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::StencilFillPathCHROMIUM& c =
      *static_cast<const gles2::cmds::StencilFillPathCHROMIUM*>(cmd_data);
  GLuint path = static_cast<GLuint>(c.path);
  GLenum fill_mode = static_cast<GLenum>(c.fillMode);
  GLuint mask = static_cast<GLuint>(c.mask);
  error::Error error = DoStencilFillPathCHROMIUM(path, fill_mode, mask);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleStencilStrokePathCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::StencilStrokePathCHROMIUM& c =
      *static_cast<const gles2::cmds::StencilStrokePathCHROMIUM*>(cmd_data);
  GLuint path = static_cast<GLuint>(c.path);
  GLint reference = static_cast<GLint>(c.reference);
  GLuint mask = static_cast<GLuint>(c.mask);
  error::Error error = DoStencilStrokePathCHROMIUM(path, reference, mask);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleCoverFillPathCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::CoverFillPathCHROMIUM& c =
      *static_cast<const gles2::cmds::CoverFillPathCHROMIUM*>(cmd_data);
  GLuint path = static_cast<GLuint>(c.path);
  GLenum cover_mode = static_cast<GLenum>(c.coverMode);
  error::Error error = DoCoverFillPathCHROMIUM(path, cover_mode);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleCoverStrokePathCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::CoverStrokePathCHROMIUM& c =
      *static_cast<const gles2::cmds::CoverStrokePathCHROMIUM*>(cmd_data);
  GLuint path = static_cast<GLuint>(c.path);
  GLenum cover_mode = static_cast<GLenum>(c.coverMode);
  error::Error error = DoCoverStrokePathCHROMIUM(path, cover_mode);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleStencilThenCoverFillPathCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::StencilThenCoverFillPathCHROMIUM& c =
      *static_cast<const gles2::cmds::StencilThenCoverFillPathCHROMIUM*>(
          cmd_data);
  GLuint path = static_cast<GLuint>(c.path);
  GLenum fill_mode = static_cast<GLenum>(c.fillMode);
  GLuint mask = static_cast<GLuint>(c.mask);
  GLenum cover_mode = static_cast<GLenum>(c.coverMode);
  error::Error error =
      DoStencilThenCoverFillPathCHROMIUM(path, fill_mode, mask, cover_mode);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleStencilThenCoverStrokePathCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::StencilThenCoverStrokePathCHROMIUM& c =
      *static_cast<const gles2::cmds::StencilThenCoverStrokePathCHROMIUM*>(
          cmd_data);
  GLuint path = static_cast<GLuint>(c.path);
  GLint reference = static_cast<GLint>(c.reference);
  GLuint mask = static_cast<GLuint>(c.mask);
  GLenum cover_mode = static_cast<GLenum>(c.coverMode);
  error::Error error =
      DoStencilThenCoverStrokePathCHROMIUM(path, reference, mask, cover_mode);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleStencilFillPathInstancedCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::StencilFillPathInstancedCHROMIUM& c =
      *static_cast<const gles2::cmds::StencilFillPathInstancedCHROMIUM*>(
          cmd_data);
  GLsizei num_paths = static_cast<GLsizei>(c.numPaths);
  GLenum path_name_type = static_cast<GLuint>(c.pathNameType);
  const GLvoid* paths = nullptr;
  GLsizei paths_bufsize = 0;
  if (num_paths > 0) {
    uint32_t paths_shm_id = static_cast<uint32_t>(c.paths_shm_id);
    uint32_t paths_shm_offset = static_cast<uint32_t>(c.paths_shm_offset);
    if (paths_shm_id != 0 || paths_shm_offset != 0) {
      unsigned int memory_size = 0;
      paths = GetSharedMemoryAndSizeAs<const GLvoid*>(
          paths_shm_id, paths_shm_offset, &memory_size);
      paths_bufsize = static_cast<GLsizei>(memory_size);
    }

    if (!paths) {
      return error::kOutOfBounds;
    }
  }
  GLuint path_base = static_cast<GLuint>(c.pathBase);
  GLenum fill_mode = static_cast<GLenum>(c.fillMode);
  GLuint mask = static_cast<GLuint>(c.mask);
  GLenum transform_type = static_cast<GLuint>(c.transformType);
  const GLfloat* transform_values = nullptr;
  GLsizei transform_values_bufsize = 0;
  if (c.transformValues_shm_id != 0 || c.transformValues_shm_offset != 0) {
    unsigned int memory_size = 0;
    transform_values = GetSharedMemoryAndSizeAs<const GLfloat*>(
        c.transformValues_shm_id, c.transformValues_shm_offset, &memory_size);
    transform_values_bufsize = static_cast<GLsizei>(memory_size);
  }
  if (!transform_values) {
    return error::kOutOfBounds;
  }

  error::Error error = DoStencilFillPathInstancedCHROMIUM(
      num_paths, path_name_type, paths, paths_bufsize, path_base, fill_mode,
      mask, transform_type, transform_values, transform_values_bufsize);
  if (error != error::kNoError) {
    return error;
  }

  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleStencilStrokePathInstancedCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::StencilStrokePathInstancedCHROMIUM& c =
      *static_cast<const gles2::cmds::StencilStrokePathInstancedCHROMIUM*>(
          cmd_data);
  GLsizei num_paths = static_cast<GLsizei>(c.numPaths);
  GLenum path_name_type = static_cast<GLuint>(c.pathNameType);
  const GLvoid* paths = nullptr;
  GLsizei paths_bufsize = 0;
  if (num_paths > 0) {
    uint32_t paths_shm_id = static_cast<uint32_t>(c.paths_shm_id);
    uint32_t paths_shm_offset = static_cast<uint32_t>(c.paths_shm_offset);
    if (paths_shm_id != 0 || paths_shm_offset != 0) {
      unsigned int memory_size = 0;
      paths = GetSharedMemoryAndSizeAs<const GLvoid*>(
          paths_shm_id, paths_shm_offset, &memory_size);
      paths_bufsize = static_cast<GLsizei>(memory_size);
    }

    if (!paths) {
      return error::kOutOfBounds;
    }
  }
  GLuint path_base = static_cast<GLuint>(c.pathBase);
  GLint reference = static_cast<GLint>(c.reference);
  GLuint mask = static_cast<GLuint>(c.mask);
  GLenum transform_type = static_cast<GLuint>(c.transformType);
  const GLfloat* transform_values = nullptr;
  GLsizei transform_values_bufsize = 0;
  if (c.transformValues_shm_id != 0 || c.transformValues_shm_offset != 0) {
    unsigned int memory_size = 0;
    transform_values = GetSharedMemoryAndSizeAs<const GLfloat*>(
        c.transformValues_shm_id, c.transformValues_shm_offset, &memory_size);
    transform_values_bufsize = static_cast<GLsizei>(memory_size);
  }
  if (!transform_values) {
    return error::kOutOfBounds;
  }

  error::Error error = DoStencilStrokePathInstancedCHROMIUM(
      num_paths, path_name_type, paths, paths_bufsize, path_base, reference,
      mask, transform_type, transform_values, transform_values_bufsize);
  if (error != error::kNoError) {
    return error;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleCoverFillPathInstancedCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::CoverFillPathInstancedCHROMIUM& c =
      *static_cast<const gles2::cmds::CoverFillPathInstancedCHROMIUM*>(
          cmd_data);
  GLsizei num_paths = static_cast<GLsizei>(c.numPaths);
  GLenum path_name_type = static_cast<GLuint>(c.pathNameType);
  const GLvoid* paths = nullptr;
  GLsizei paths_bufsize = 0;
  if (num_paths > 0) {
    uint32_t paths_shm_id = static_cast<uint32_t>(c.paths_shm_id);
    uint32_t paths_shm_offset = static_cast<uint32_t>(c.paths_shm_offset);
    if (paths_shm_id != 0 || paths_shm_offset != 0) {
      unsigned int memory_size = 0;
      paths = GetSharedMemoryAndSizeAs<const GLvoid*>(
          paths_shm_id, paths_shm_offset, &memory_size);
      paths_bufsize = static_cast<GLsizei>(memory_size);
    }

    if (!paths) {
      return error::kOutOfBounds;
    }
  }
  GLuint path_base = static_cast<GLuint>(c.pathBase);
  GLenum cover_mode = static_cast<GLenum>(c.coverMode);
  GLenum transform_type = static_cast<GLuint>(c.transformType);
  const GLfloat* transform_values = nullptr;
  GLsizei transform_values_bufsize = 0;
  if (c.transformValues_shm_id != 0 || c.transformValues_shm_offset != 0) {
    unsigned int memory_size = 0;
    transform_values = GetSharedMemoryAndSizeAs<const GLfloat*>(
        c.transformValues_shm_id, c.transformValues_shm_offset, &memory_size);
    transform_values_bufsize = static_cast<GLsizei>(memory_size);
  }
  if (!transform_values) {
    return error::kOutOfBounds;
  }

  error::Error error = DoCoverFillPathInstancedCHROMIUM(
      num_paths, path_name_type, paths, paths_bufsize, path_base, cover_mode,
      transform_type, transform_values, transform_values_bufsize);
  if (error != error::kNoError) {
    return error;
  }

  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleCoverStrokePathInstancedCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::CoverStrokePathInstancedCHROMIUM& c =
      *static_cast<const gles2::cmds::CoverStrokePathInstancedCHROMIUM*>(
          cmd_data);
  GLsizei num_paths = static_cast<GLsizei>(c.numPaths);
  GLenum path_name_type = static_cast<GLuint>(c.pathNameType);
  const GLvoid* paths = nullptr;
  GLsizei paths_bufsize = 0;
  if (num_paths > 0) {
    uint32_t paths_shm_id = static_cast<uint32_t>(c.paths_shm_id);
    uint32_t paths_shm_offset = static_cast<uint32_t>(c.paths_shm_offset);
    if (paths_shm_id != 0 || paths_shm_offset != 0) {
      unsigned int memory_size = 0;
      paths = GetSharedMemoryAndSizeAs<const GLvoid*>(
          paths_shm_id, paths_shm_offset, &memory_size);
      paths_bufsize = static_cast<GLsizei>(memory_size);
    }

    if (!paths) {
      return error::kOutOfBounds;
    }
  }
  GLuint path_base = static_cast<GLuint>(c.pathBase);
  GLenum cover_mode = static_cast<GLenum>(c.coverMode);
  GLenum transform_type = static_cast<GLuint>(c.transformType);
  const GLfloat* transform_values = nullptr;
  GLsizei transform_values_bufsize = 0;
  if (c.transformValues_shm_id != 0 || c.transformValues_shm_offset != 0) {
    unsigned int memory_size = 0;
    transform_values = GetSharedMemoryAndSizeAs<const GLfloat*>(
        c.transformValues_shm_id, c.transformValues_shm_offset, &memory_size);
    transform_values_bufsize = static_cast<GLsizei>(memory_size);
  }
  if (!transform_values) {
    return error::kOutOfBounds;
  }

  error::Error error = DoCoverStrokePathInstancedCHROMIUM(
      num_paths, path_name_type, paths, paths_bufsize, path_base, cover_mode,
      transform_type, transform_values, transform_values_bufsize);
  if (error != error::kNoError) {
    return error;
  }

  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleStencilThenCoverFillPathInstancedCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::StencilThenCoverFillPathInstancedCHROMIUM& c =
      *static_cast<
          const gles2::cmds::StencilThenCoverFillPathInstancedCHROMIUM*>(
          cmd_data);
  GLsizei num_paths = static_cast<GLsizei>(c.numPaths);
  GLenum path_name_type = static_cast<GLuint>(c.pathNameType);
  const GLvoid* paths = nullptr;
  GLsizei paths_bufsize = 0;
  if (num_paths > 0) {
    uint32_t paths_shm_id = static_cast<uint32_t>(c.paths_shm_id);
    uint32_t paths_shm_offset = static_cast<uint32_t>(c.paths_shm_offset);
    if (paths_shm_id != 0 || paths_shm_offset != 0) {
      unsigned int memory_size = 0;
      paths = GetSharedMemoryAndSizeAs<const GLvoid*>(
          paths_shm_id, paths_shm_offset, &memory_size);
      paths_bufsize = static_cast<GLsizei>(memory_size);
    }

    if (!paths) {
      return error::kOutOfBounds;
    }
  }
  GLuint path_base = static_cast<GLuint>(c.pathBase);
  GLenum cover_mode = static_cast<GLenum>(c.coverMode);
  GLenum fill_mode = static_cast<GLenum>(c.fillMode);
  GLuint mask = static_cast<GLuint>(c.mask);
  GLenum transform_type = static_cast<GLuint>(c.transformType);
  const GLfloat* transform_values = nullptr;
  GLsizei transform_values_bufsize = 0;
  if (c.transformValues_shm_id != 0 || c.transformValues_shm_offset != 0) {
    unsigned int memory_size = 0;
    transform_values = GetSharedMemoryAndSizeAs<const GLfloat*>(
        c.transformValues_shm_id, c.transformValues_shm_offset, &memory_size);
    transform_values_bufsize = static_cast<GLsizei>(memory_size);
  }
  if (!transform_values) {
    return error::kOutOfBounds;
  }

  error::Error error = DoStencilThenCoverFillPathInstancedCHROMIUM(
      num_paths, path_name_type, paths, paths_bufsize, path_base, cover_mode,
      fill_mode, mask, transform_type, transform_values,
      transform_values_bufsize);
  if (error != error::kNoError) {
    return error;
  }

  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleStencilThenCoverStrokePathInstancedCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::StencilThenCoverStrokePathInstancedCHROMIUM& c =
      *static_cast<
          const gles2::cmds::StencilThenCoverStrokePathInstancedCHROMIUM*>(
          cmd_data);
  GLsizei num_paths = static_cast<GLsizei>(c.numPaths);
  GLenum path_name_type = static_cast<GLuint>(c.pathNameType);
  const GLvoid* paths = nullptr;
  GLsizei paths_bufsize = 0;
  if (num_paths > 0) {
    uint32_t paths_shm_id = static_cast<uint32_t>(c.paths_shm_id);
    uint32_t paths_shm_offset = static_cast<uint32_t>(c.paths_shm_offset);
    if (paths_shm_id != 0 || paths_shm_offset != 0) {
      unsigned int memory_size = 0;
      paths = GetSharedMemoryAndSizeAs<const GLvoid*>(
          paths_shm_id, paths_shm_offset, &memory_size);
      paths_bufsize = static_cast<GLsizei>(memory_size);
    }

    if (!paths) {
      return error::kOutOfBounds;
    }
  }
  GLuint path_base = static_cast<GLuint>(c.pathBase);
  GLenum cover_mode = static_cast<GLenum>(c.coverMode);
  GLint reference = static_cast<GLint>(c.reference);
  GLuint mask = static_cast<GLuint>(c.mask);
  GLenum transform_type = static_cast<GLuint>(c.transformType);
  const GLfloat* transform_values = nullptr;
  GLsizei transform_values_bufsize = 0;
  if (c.transformValues_shm_id != 0 || c.transformValues_shm_offset != 0) {
    unsigned int memory_size = 0;
    transform_values = GetSharedMemoryAndSizeAs<const GLfloat*>(
        c.transformValues_shm_id, c.transformValues_shm_offset, &memory_size);
    transform_values_bufsize = static_cast<GLsizei>(memory_size);
  }
  if (!transform_values) {
    return error::kOutOfBounds;
  }

  error::Error error = DoStencilThenCoverStrokePathInstancedCHROMIUM(
      num_paths, path_name_type, paths, paths_bufsize, path_base, cover_mode,
      reference, mask, transform_type, transform_values,
      transform_values_bufsize);
  if (error != error::kNoError) {
    return error;
  }

  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleBindFragmentInputLocationCHROMIUMBucket(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::BindFragmentInputLocationCHROMIUMBucket& c =
      *static_cast<const gles2::cmds::BindFragmentInputLocationCHROMIUMBucket*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLint location = static_cast<GLint>(c.location);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  error::Error error =
      DoBindFragmentInputLocationCHROMIUM(program, location, name_str.c_str());
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleProgramPathFragmentInputGenCHROMIUM(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::ProgramPathFragmentInputGenCHROMIUM& c =
      *static_cast<const gles2::cmds::ProgramPathFragmentInputGenCHROMIUM*>(
          cmd_data);
  GLint program = static_cast<GLint>(c.program);
  GLint location = static_cast<GLint>(c.location);
  GLenum gen_mode = static_cast<GLint>(c.genMode);
  GLint components = static_cast<GLint>(c.components);
  const GLfloat* coeffs = nullptr;
  GLsizei coeffs_bufsize = 0;
  if (c.coeffs_shm_id != 0 || c.coeffs_shm_offset != 0) {
    unsigned int memory_size = 0;
    coeffs = GetSharedMemoryAndSizeAs<const GLfloat*>(
        c.coeffs_shm_id, c.coeffs_shm_offset, &memory_size);
    coeffs_bufsize = static_cast<GLsizei>(memory_size);
  }
  if (!coeffs) {
    return error::kOutOfBounds;
  }
  error::Error error = DoProgramPathFragmentInputGenCHROMIUM(
      program, location, gen_mode, components, coeffs, coeffs_bufsize);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleBindFragDataLocationIndexedEXTBucket(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::BindFragDataLocationIndexedEXTBucket& c =
      *static_cast<const gles2::cmds::BindFragDataLocationIndexedEXTBucket*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint colorNumber = static_cast<GLuint>(c.colorNumber);
  GLuint index = static_cast<GLuint>(c.index);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  error::Error error = DoBindFragDataLocationIndexedEXT(
      program, colorNumber, index, name_str.c_str());
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBindFragDataLocationEXTBucket(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::BindFragDataLocationEXTBucket& c =
      *static_cast<const gles2::cmds::BindFragDataLocationEXTBucket*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint colorNumber = static_cast<GLuint>(c.colorNumber);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  error::Error error =
      DoBindFragDataLocationEXT(program, colorNumber, name_str.c_str());
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetFragDataIndexEXT(
    uint32_t immediate_data_size,
    const void* cmd_data) {
  const gles2::cmds::GetFragDataIndexEXT& c =
      *static_cast<const gles2::cmds::GetFragDataIndexEXT*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  Bucket* bucket = GetBucket(c.name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  GLint* index = GetSharedMemoryAs<GLint*>(c.index_shm_id, c.index_shm_offset,
                                           sizeof(GLint));
  if (!index) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (*index != -1) {
    return error::kInvalidArguments;
  }
  error::Error error = DoGetFragDataIndexEXT(program, name_str.c_str(), index);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

}  // namespace gles2
}  // namespace gpu
