// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_state.h"

#include <stddef.h>

#include <cmath>

#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/transform_feedback_manager.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"

namespace gpu {
namespace gles2 {

namespace {

GLuint Get2dServiceId(const TextureUnit& unit) {
  return unit.bound_texture_2d.get()
      ? unit.bound_texture_2d->service_id() : 0;
}

GLuint GetCubeServiceId(const TextureUnit& unit) {
  return unit.bound_texture_cube_map.get()
      ? unit.bound_texture_cube_map->service_id() : 0;
}

GLuint GetOesServiceId(const TextureUnit& unit) {
  return unit.bound_texture_external_oes.get()
      ? unit.bound_texture_external_oes->service_id() : 0;
}

GLuint GetArbServiceId(const TextureUnit& unit) {
  return unit.bound_texture_rectangle_arb.get()
      ? unit.bound_texture_rectangle_arb->service_id() : 0;
}

GLuint GetServiceId(const TextureUnit& unit, GLuint target) {
  switch (target) {
    case GL_TEXTURE_2D:
      return Get2dServiceId(unit);
    case GL_TEXTURE_CUBE_MAP:
      return GetCubeServiceId(unit);
    case GL_TEXTURE_RECTANGLE_ARB:
      return GetArbServiceId(unit);
    case GL_TEXTURE_EXTERNAL_OES:
      return GetOesServiceId(unit);
    default:
      NOTREACHED();
      return 0;
  }
}

bool TargetIsSupported(const FeatureInfo* feature_info, GLuint target) {
  switch (target) {
    case GL_TEXTURE_2D:
      return true;
    case GL_TEXTURE_CUBE_MAP:
      return true;
    case GL_TEXTURE_RECTANGLE_ARB:
      return feature_info->feature_flags().arb_texture_rectangle;
    case GL_TEXTURE_EXTERNAL_OES:
      return feature_info->feature_flags().oes_egl_image_external ||
             feature_info->feature_flags().nv_egl_stream_consumer_external;
    default:
      NOTREACHED();
      return false;
  }
}

GLuint GetBufferId(const Buffer* buffer) {
  if (buffer)
    return buffer->service_id();
  return 0;
}

}  // anonymous namespace.

TextureUnit::TextureUnit()
    : bind_target(GL_TEXTURE_2D) {
}

TextureUnit::TextureUnit(const TextureUnit& other) = default;

TextureUnit::~TextureUnit() {
}

bool Vec4::Equal(const Vec4& other) const {
  if (type_ != other.type_)
    return false;
  switch (type_) {
    case SHADER_VARIABLE_FLOAT:
      for (size_t ii = 0; ii < 4; ++ii) {
        if (v_[ii].float_value != other.v_[ii].float_value)
          return false;
      }
      break;
    case SHADER_VARIABLE_INT:
      for (size_t ii = 0; ii < 4; ++ii) {
        if (v_[ii].int_value != other.v_[ii].int_value)
          return false;
      }
      break;
    case SHADER_VARIABLE_UINT:
      for (size_t ii = 0; ii < 4; ++ii) {
        if (v_[ii].uint_value != other.v_[ii].uint_value)
          return false;
      }
      break;
    default:
      NOTREACHED();
      break;
  }
  return true;
}

template <>
void Vec4::GetValues<GLfloat>(GLfloat* values) const {
  DCHECK(values);
  switch (type_) {
    case SHADER_VARIABLE_FLOAT:
      for (size_t ii = 0; ii < 4; ++ii)
        values[ii] = v_[ii].float_value;
      break;
    case SHADER_VARIABLE_INT:
      for (size_t ii = 0; ii < 4; ++ii)
        values[ii] = static_cast<GLfloat>(v_[ii].int_value);
      break;
    case SHADER_VARIABLE_UINT:
      for (size_t ii = 0; ii < 4; ++ii)
        values[ii] = static_cast<GLfloat>(v_[ii].uint_value);
      break;
    default:
      NOTREACHED();
      break;
  }
}

template <>
void Vec4::GetValues<GLint>(GLint* values) const {
  DCHECK(values);
  switch (type_) {
    case SHADER_VARIABLE_FLOAT:
      for (size_t ii = 0; ii < 4; ++ii)
        values[ii] = static_cast<GLint>(v_[ii].float_value);
      break;
    case SHADER_VARIABLE_INT:
      for (size_t ii = 0; ii < 4; ++ii)
        values[ii] = v_[ii].int_value;
      break;
    case SHADER_VARIABLE_UINT:
      for (size_t ii = 0; ii < 4; ++ii)
        values[ii] = static_cast<GLint>(v_[ii].uint_value);
      break;
    default:
      NOTREACHED();
      break;
  }
}

template<>
void Vec4::GetValues<GLuint>(GLuint* values) const {
  DCHECK(values);
  switch (type_) {
    case SHADER_VARIABLE_FLOAT:
      for (size_t ii = 0; ii < 4; ++ii)
        values[ii] = static_cast<GLuint>(v_[ii].float_value);
      break;
    case SHADER_VARIABLE_INT:
      for (size_t ii = 0; ii < 4; ++ii)
        values[ii] = static_cast<GLuint>(v_[ii].int_value);
      break;
    case SHADER_VARIABLE_UINT:
      for (size_t ii = 0; ii < 4; ++ii)
        values[ii] = v_[ii].uint_value;
      break;
    default:
      NOTREACHED();
      break;
  }
}

template <>
void Vec4::SetValues<GLfloat>(const GLfloat* values) {
  DCHECK(values);
  for (size_t ii = 0; ii < 4; ++ii)
    v_[ii].float_value = values[ii];
  type_ = SHADER_VARIABLE_FLOAT;
}

template <>
void Vec4::SetValues<GLint>(const GLint* values) {
  DCHECK(values);
  for (size_t ii = 0; ii < 4; ++ii)
    v_[ii].int_value = values[ii];
  type_ = SHADER_VARIABLE_INT;
}

template <>
void Vec4::SetValues<GLuint>(const GLuint* values) {
  DCHECK(values);
  for (size_t ii = 0; ii < 4; ++ii)
    v_[ii].uint_value = values[ii];
  type_ = SHADER_VARIABLE_UINT;
}

ContextState::ContextState(FeatureInfo* feature_info,
                           ErrorStateClient* error_state_client,
                           Logger* logger)
    : active_texture_unit(0),
      bound_renderbuffer_valid(false),
      pack_reverse_row_order(false),
      ignore_cached_state(false),
      fbo_binding_for_scissor_workaround_dirty(false),
      framebuffer_srgb_(false),
      feature_info_(feature_info),
      error_state_(ErrorState::Create(error_state_client, logger)) {
  Initialize();
}

ContextState::~ContextState() {
}

void ContextState::SetLineWidthBounds(GLfloat min, GLfloat max) {
  line_width_min_ = min;
  line_width_max_ = max;
}

void ContextState::RestoreTextureUnitBindings(
    GLuint unit, const ContextState* prev_state) const {
  DCHECK_LT(unit, texture_units.size());
  const TextureUnit& texture_unit = texture_units[unit];
  GLuint service_id_2d = Get2dServiceId(texture_unit);
  GLuint service_id_cube = GetCubeServiceId(texture_unit);
  GLuint service_id_oes = GetOesServiceId(texture_unit);
  GLuint service_id_arb = GetArbServiceId(texture_unit);

  bool bind_texture_2d = true;
  bool bind_texture_cube = true;
  bool bind_texture_oes =
      feature_info_->feature_flags().oes_egl_image_external ||
      feature_info_->feature_flags().nv_egl_stream_consumer_external;
  bool bind_texture_arb = feature_info_->feature_flags().arb_texture_rectangle;

  if (prev_state) {
    const TextureUnit& prev_unit = prev_state->texture_units[unit];
    bind_texture_2d = service_id_2d != Get2dServiceId(prev_unit);
    bind_texture_cube = service_id_cube != GetCubeServiceId(prev_unit);
    bind_texture_oes =
        bind_texture_oes && service_id_oes != GetOesServiceId(prev_unit);
    bind_texture_arb =
        bind_texture_arb && service_id_arb != GetArbServiceId(prev_unit);
  }

  // Early-out if nothing has changed from the previous state.
  if (!bind_texture_2d && !bind_texture_cube
      && !bind_texture_oes && !bind_texture_arb) {
    return;
  }

  glActiveTexture(GL_TEXTURE0 + unit);
  if (bind_texture_2d) {
    glBindTexture(GL_TEXTURE_2D, service_id_2d);
  }
  if (bind_texture_cube) {
    glBindTexture(GL_TEXTURE_CUBE_MAP, service_id_cube);
  }
  if (bind_texture_oes) {
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, service_id_oes);
  }
  if (bind_texture_arb) {
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, service_id_arb);
  }
}

void ContextState::PushTextureDecompressionUnpackState() const {
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  if (bound_pixel_unpack_buffer.get()) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
  }
}

void ContextState::RestoreUnpackState() const {
  glPixelStorei(GL_UNPACK_ALIGNMENT, unpack_alignment);
  if (bound_pixel_unpack_buffer.get()) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,
                 GetBufferId(bound_pixel_unpack_buffer.get()));
    glPixelStorei(GL_UNPACK_ROW_LENGTH, unpack_row_length);
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, unpack_image_height);
  }
}

void ContextState::DoLineWidth(GLfloat width) const {
  glLineWidth(
      std::min(std::max(width, line_width_min_), line_width_max_));
}

void ContextState::RestoreBufferBindings() const {
  if (vertex_attrib_manager.get()) {
    Buffer* element_array_buffer =
        vertex_attrib_manager->element_array_buffer();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GetBufferId(element_array_buffer));
  }
  glBindBuffer(GL_ARRAY_BUFFER, GetBufferId(bound_array_buffer.get()));
  if (feature_info_->IsES3Capable()) {
    glBindBuffer(GL_COPY_READ_BUFFER,
                 GetBufferId(bound_copy_read_buffer.get()));
    glBindBuffer(GL_COPY_WRITE_BUFFER,
                 GetBufferId(bound_copy_write_buffer.get()));
    glBindBuffer(GL_PIXEL_PACK_BUFFER,
                 GetBufferId(bound_pixel_pack_buffer.get()));
    UpdatePackParameters();
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,
                 GetBufferId(bound_pixel_unpack_buffer.get()));
    UpdateUnpackParameters();
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,
                 GetBufferId(bound_transform_feedback_buffer.get()));
    glBindBuffer(GL_UNIFORM_BUFFER, GetBufferId(bound_uniform_buffer.get()));
  }
}

void ContextState::RestoreRenderbufferBindings() {
  // Require Renderbuffer rebind.
  bound_renderbuffer_valid = false;
}

void ContextState::RestoreProgramSettings(
    const ContextState* prev_state,
    bool restore_transform_feedback_bindings) const {
  bool flag = (restore_transform_feedback_bindings &&
               feature_info_->IsES3Capable());
  if (flag && prev_state) {
    if (prev_state->bound_transform_feedback.get() &&
        prev_state->bound_transform_feedback->active() &&
        !prev_state->bound_transform_feedback->paused()) {
      glPauseTransformFeedback();
    }
  }
  glUseProgram(current_program.get() ? current_program->service_id() : 0);
  if (flag) {
    if (bound_transform_feedback.get()) {
      bound_transform_feedback->DoBindTransformFeedback(GL_TRANSFORM_FEEDBACK);
    } else {
      glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    }
  }
}

void ContextState::RestoreIndexedUniformBufferBindings(
    const ContextState* prev_state) {
  if (!feature_info_->IsES3Capable())
    return;
  indexed_uniform_buffer_bindings->RestoreBindings(
      prev_state ? prev_state->indexed_uniform_buffer_bindings.get() : nullptr);
}

void ContextState::RestoreActiveTexture() const {
  glActiveTexture(GL_TEXTURE0 + active_texture_unit);
}

void ContextState::RestoreAllTextureUnitBindings(
    const ContextState* prev_state) const {
  // Restore Texture state.
  for (size_t ii = 0; ii < texture_units.size(); ++ii) {
    RestoreTextureUnitBindings(ii, prev_state);
  }
  RestoreActiveTexture();
}

void ContextState::RestoreActiveTextureUnitBinding(unsigned int target) const {
  DCHECK_LT(active_texture_unit, texture_units.size());
  const TextureUnit& texture_unit = texture_units[active_texture_unit];
  if (TargetIsSupported(feature_info_, target))
    glBindTexture(target, GetServiceId(texture_unit, target));
}

void ContextState::RestoreVertexAttribValues() const {
  for (size_t attrib = 0; attrib < vertex_attrib_manager->num_attribs();
       ++attrib) {
    switch (attrib_values[attrib].type()) {
      case SHADER_VARIABLE_FLOAT:
        {
          GLfloat v[4];
          attrib_values[attrib].GetValues(v);
          glVertexAttrib4fv(attrib, v);
        }
        break;
      case SHADER_VARIABLE_INT:
        {
          GLint v[4];
          attrib_values[attrib].GetValues(v);
          glVertexAttribI4iv(attrib, v);
        }
        break;
      case SHADER_VARIABLE_UINT:
        {
          GLuint v[4];
          attrib_values[attrib].GetValues(v);
          glVertexAttribI4uiv(attrib, v);
        }
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

void ContextState::RestoreVertexAttribArrays(
    const scoped_refptr<VertexAttribManager> attrib_manager) const {
  // This is expected to be called only for VAO with service_id 0,
  // either to restore the default VAO or a virtual VAO with service_id 0.
  GLuint vao_service_id = attrib_manager->service_id();
  DCHECK(vao_service_id == 0);

  // Bind VAO if supported.
  if (feature_info_->feature_flags().native_vertex_array_object)
    glBindVertexArrayOES(vao_service_id);

  // Restore vertex attrib arrays.
  for (size_t attrib_index = 0; attrib_index < attrib_manager->num_attribs();
       ++attrib_index) {
    const VertexAttrib* attrib = attrib_manager->GetVertexAttrib(attrib_index);

    // Restore vertex array.
    Buffer* buffer = attrib->buffer();
    GLuint buffer_service_id = buffer ? buffer->service_id() : 0;
    glBindBuffer(GL_ARRAY_BUFFER, buffer_service_id);
    const void* ptr = reinterpret_cast<const void*>(attrib->offset());
    glVertexAttribPointer(attrib_index,
                          attrib->size(),
                          attrib->type(),
                          attrib->normalized(),
                          attrib->gl_stride(),
                          ptr);

    // Restore attrib divisor if supported.
    if (feature_info_->feature_flags().angle_instanced_arrays)
      glVertexAttribDivisorANGLE(attrib_index, attrib->divisor());

    // Never touch vertex attribute 0's state (in particular, never
    // disable it) when running on desktop GL with compatibility profile
    // because it will never be re-enabled.
    if (attrib_index != 0 ||
        feature_info_->gl_version_info().BehavesLikeGLES()) {
      if (attrib->enabled()) {
        glEnableVertexAttribArray(attrib_index);
      } else {
        glDisableVertexAttribArray(attrib_index);
      }
    }
  }
}

void ContextState::RestoreVertexAttribs() const {
  // Restore Vertex Attrib Arrays
  DCHECK(vertex_attrib_manager.get());
  // Restore VAOs.
  if (feature_info_->feature_flags().native_vertex_array_object) {
    // If default VAO is still using shared id 0 instead of unique ids
    // per-context, default VAO state must be restored.
    GLuint default_vao_service_id =
        default_vertex_attrib_manager->service_id();
    if (default_vao_service_id == 0)
      RestoreVertexAttribArrays(default_vertex_attrib_manager);

    // Restore the current VAO binding, unless it's the same as the
    // default above.
    GLuint curr_vao_service_id = vertex_attrib_manager->service_id();
    if (curr_vao_service_id != 0)
      glBindVertexArrayOES(curr_vao_service_id);
  } else {
    // If native VAO isn't supported, emulated VAOs are used.
    // Restore to the currently bound VAO.
    RestoreVertexAttribArrays(vertex_attrib_manager);
  }

  // glVertexAttrib4fv aren't part of VAO state and must be restored.
  RestoreVertexAttribValues();
}

void ContextState::RestoreGlobalState(const ContextState* prev_state) const {
  InitCapabilities(prev_state);
  InitState(prev_state);
}

void ContextState::RestoreState(const ContextState* prev_state) {
  RestoreAllTextureUnitBindings(prev_state);
  RestoreVertexAttribs();
  RestoreBufferBindings();
  RestoreRenderbufferBindings();
  RestoreProgramSettings(prev_state, true);
  RestoreIndexedUniformBufferBindings(prev_state);
  RestoreGlobalState(prev_state);

  if (!prev_state) {
    if (feature_info_->feature_flags().desktop_srgb_support) {
      framebuffer_srgb_ = false;
      glDisable(GL_FRAMEBUFFER_SRGB);
    }
  } else if (framebuffer_srgb_ != prev_state->framebuffer_srgb_) {
    // FRAMEBUFFER_SRGB will be restored lazily at render time.
    framebuffer_srgb_ = prev_state->framebuffer_srgb_;
  }
}

ErrorState* ContextState::GetErrorState() {
  return error_state_.get();
}

void ContextState::EnableDisable(GLenum pname, bool enable) const {
  if (pname == GL_PRIMITIVE_RESTART_FIXED_INDEX &&
      feature_info_->feature_flags().emulate_primitive_restart_fixed_index) {
    // GLES2DecoderImpl::DoDrawElements can handle this situation
    return;
  }
  if (enable) {
    glEnable(pname);
  } else {
    glDisable(pname);
  }
}

void ContextState::UpdatePackParameters() const {
  if (!feature_info_->IsES3Capable())
    return;
  if (bound_pixel_pack_buffer.get()) {
    glPixelStorei(GL_PACK_ROW_LENGTH, pack_row_length);
  } else {
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
  }
}

void ContextState::UpdateUnpackParameters() const {
  if (!feature_info_->IsES3Capable())
    return;
  if (bound_pixel_unpack_buffer.get()) {
    glPixelStorei(GL_UNPACK_ROW_LENGTH, unpack_row_length);
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, unpack_image_height);
  } else {
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
  }
}

void ContextState::SetBoundBuffer(GLenum target, Buffer* buffer) {
  switch (target) {
    case GL_ARRAY_BUFFER:
      bound_array_buffer = buffer;
      break;
    case GL_ELEMENT_ARRAY_BUFFER:
      vertex_attrib_manager->SetElementArrayBuffer(buffer);
      break;
    case GL_COPY_READ_BUFFER:
      bound_copy_read_buffer = buffer;
      break;
    case GL_COPY_WRITE_BUFFER:
      bound_copy_write_buffer = buffer;
      break;
    case GL_PIXEL_PACK_BUFFER:
      bound_pixel_pack_buffer = buffer;
      UpdatePackParameters();
      break;
    case GL_PIXEL_UNPACK_BUFFER:
      bound_pixel_unpack_buffer = buffer;
      UpdateUnpackParameters();
      break;
    case GL_TRANSFORM_FEEDBACK_BUFFER:
      bound_transform_feedback_buffer = buffer;
      break;
    case GL_UNIFORM_BUFFER:
      bound_uniform_buffer = buffer;
      break;
    default:
      NOTREACHED();
      break;
  }
}

void ContextState::RemoveBoundBuffer(Buffer* buffer) {
  DCHECK(buffer);
  vertex_attrib_manager->Unbind(buffer);
  if (bound_array_buffer.get() == buffer) {
    bound_array_buffer = nullptr;
  }
  if (bound_copy_read_buffer.get() == buffer) {
    bound_copy_read_buffer = nullptr;
  }
  if (bound_copy_write_buffer.get() == buffer) {
    bound_copy_write_buffer = nullptr;
  }
  if (bound_pixel_pack_buffer.get() == buffer) {
    bound_pixel_pack_buffer = nullptr;
    UpdatePackParameters();
  }
  if (bound_pixel_unpack_buffer.get() == buffer) {
    bound_pixel_unpack_buffer = nullptr;
    UpdateUnpackParameters();
  }
  if (bound_transform_feedback_buffer.get() == buffer) {
    bound_transform_feedback_buffer = nullptr;
  }
  if (bound_transform_feedback.get()) {
    bound_transform_feedback->RemoveBoundBuffer(buffer);
  }
  if (bound_uniform_buffer.get() == buffer) {
    bound_uniform_buffer = nullptr;
  }
}

void ContextState::UnbindTexture(TextureRef* texture) {
  GLuint active_unit = active_texture_unit;
  for (size_t jj = 0; jj < texture_units.size(); ++jj) {
    TextureUnit& unit = texture_units[jj];
    if (unit.bound_texture_2d.get() == texture) {
      unit.bound_texture_2d = NULL;
      if (active_unit != jj) {
        glActiveTexture(GL_TEXTURE0 + jj);
        active_unit = jj;
      }
      glBindTexture(GL_TEXTURE_2D, 0);
    } else if (unit.bound_texture_cube_map.get() == texture) {
      unit.bound_texture_cube_map = NULL;
      if (active_unit != jj) {
        glActiveTexture(GL_TEXTURE0 + jj);
        active_unit = jj;
      }
      glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    } else if (unit.bound_texture_external_oes.get() == texture) {
      unit.bound_texture_external_oes = NULL;
      if (active_unit != jj) {
        glActiveTexture(GL_TEXTURE0 + jj);
        active_unit = jj;
      }
      glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
    } else if (unit.bound_texture_rectangle_arb.get() == texture) {
      unit.bound_texture_rectangle_arb = NULL;
      if (active_unit != jj) {
        glActiveTexture(GL_TEXTURE0 + jj);
        active_unit = jj;
      }
      glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
    } else if (unit.bound_texture_3d.get() == texture) {
      unit.bound_texture_3d = NULL;
      if (active_unit != jj) {
        glActiveTexture(GL_TEXTURE0 + jj);
        active_unit = jj;
      }
      glBindTexture(GL_TEXTURE_3D, 0);
    } else if (unit.bound_texture_2d_array.get() == texture) {
      unit.bound_texture_2d_array = NULL;
      if (active_unit != jj) {
        glActiveTexture(GL_TEXTURE0 + jj);
        active_unit = jj;
      }
      glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    }
  }

  if (active_unit != active_texture_unit) {
    glActiveTexture(GL_TEXTURE0 + active_texture_unit);
  }
}

void ContextState::UnbindSampler(Sampler* sampler) {
  for (size_t jj = 0; jj < sampler_units.size(); ++jj) {
    if (sampler_units[jj].get() == sampler) {
      sampler_units[jj] = nullptr;
      glBindSampler(jj, 0);
    }
  }
}

PixelStoreParams ContextState::GetPackParams() {
  DCHECK_EQ(0, pack_skip_pixels);
  DCHECK_EQ(0, pack_skip_rows);
  PixelStoreParams params;
  params.alignment = pack_alignment;
  params.row_length = pack_row_length;
  return params;
}

PixelStoreParams ContextState::GetUnpackParams(Dimension dimension) {
  DCHECK_EQ(0, unpack_skip_pixels);
  DCHECK_EQ(0, unpack_skip_rows);
  DCHECK_EQ(0, unpack_skip_images);
  PixelStoreParams params;
  params.alignment = unpack_alignment;
  params.row_length = unpack_row_length;
  if (dimension == k3D) {
    params.image_height = unpack_image_height;
  }
  return params;
}

void ContextState::EnableDisableFramebufferSRGB(bool enable) {
  if (framebuffer_srgb_ == enable)
    return;
  EnableDisable(GL_FRAMEBUFFER_SRGB, enable);
  framebuffer_srgb_ = enable;
}

void ContextState::InitStateManual(const ContextState*) const {
  // Here we always reset the states whether it's different from previous ones.
  // We have very limited states here; also, once we switch to MANGLE, MANGLE
  // will opmitize this.
  UpdatePackParameters();
  UpdateUnpackParameters();
}

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/service/context_state_impl_autogen.h"

}  // namespace gles2
}  // namespace gpu
