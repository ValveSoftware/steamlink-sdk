// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_copy_texture_chromium.h"

#include <stddef.h>

#include <algorithm>

#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "ui/gl/gl_implementation.h"

namespace {

const GLfloat kIdentityMatrix[16] = {1.0f, 0.0f, 0.0f, 0.0f,
                                     0.0f, 1.0f, 0.0f, 0.0f,
                                     0.0f, 0.0f, 1.0f, 0.0f,
                                     0.0f, 0.0f, 0.0f, 1.0f};

enum FragmentShaderId {
  FRAGMENT_SHADER_COPY_TEXTURE_2D,
  FRAGMENT_SHADER_COPY_TEXTURE_RECTANGLE_ARB,
  FRAGMENT_SHADER_COPY_TEXTURE_EXTERNAL_OES,
  FRAGMENT_SHADER_COPY_TEXTURE_PREMULTIPLY_ALPHA_2D,
  FRAGMENT_SHADER_COPY_TEXTURE_PREMULTIPLY_ALPHA_RECTANGLE_ARB,
  FRAGMENT_SHADER_COPY_TEXTURE_PREMULTIPLY_ALPHA_EXTERNAL_OES,
  FRAGMENT_SHADER_COPY_TEXTURE_UNPREMULTIPLY_ALPHA_2D,
  FRAGMENT_SHADER_COPY_TEXTURE_UNPREMULTIPLY_ALPHA_RECTANGLE_ARB,
  FRAGMENT_SHADER_COPY_TEXTURE_UNPREMULTIPLY_ALPHA_EXTERNAL_OES,
  NUM_FRAGMENT_SHADERS,
};

// Returns the correct fragment shader id to evaluate the copy operation for
// the premultiply alpha pixel store settings and target.
FragmentShaderId GetFragmentShaderId(bool premultiply_alpha,
                                     bool unpremultiply_alpha,
                                     GLenum target) {
  enum {
    SAMPLER_2D,
    SAMPLER_RECTANGLE_ARB,
    SAMPLER_EXTERNAL_OES,
    NUM_SAMPLERS
  };

  // bit 0: premultiply alpha
  // bit 1: unpremultiply alpha
  static FragmentShaderId shader_ids[][NUM_SAMPLERS] = {
      {
       FRAGMENT_SHADER_COPY_TEXTURE_2D,
       FRAGMENT_SHADER_COPY_TEXTURE_RECTANGLE_ARB,
       FRAGMENT_SHADER_COPY_TEXTURE_EXTERNAL_OES,
      },
      {
       FRAGMENT_SHADER_COPY_TEXTURE_PREMULTIPLY_ALPHA_2D,
       FRAGMENT_SHADER_COPY_TEXTURE_PREMULTIPLY_ALPHA_RECTANGLE_ARB,
       FRAGMENT_SHADER_COPY_TEXTURE_PREMULTIPLY_ALPHA_EXTERNAL_OES,
      },
      {
       FRAGMENT_SHADER_COPY_TEXTURE_UNPREMULTIPLY_ALPHA_2D,
       FRAGMENT_SHADER_COPY_TEXTURE_UNPREMULTIPLY_ALPHA_RECTANGLE_ARB,
       FRAGMENT_SHADER_COPY_TEXTURE_UNPREMULTIPLY_ALPHA_EXTERNAL_OES,
      },
      {
       FRAGMENT_SHADER_COPY_TEXTURE_2D,
       FRAGMENT_SHADER_COPY_TEXTURE_RECTANGLE_ARB,
       FRAGMENT_SHADER_COPY_TEXTURE_EXTERNAL_OES,
      }};

  unsigned index = (premultiply_alpha   ? (1 << 0) : 0) |
                   (unpremultiply_alpha ? (1 << 1) : 0);

  switch (target) {
    case GL_TEXTURE_2D:
      return shader_ids[index][SAMPLER_2D];
    case GL_TEXTURE_RECTANGLE_ARB:
      return shader_ids[index][SAMPLER_RECTANGLE_ARB];
    case GL_TEXTURE_EXTERNAL_OES:
      return shader_ids[index][SAMPLER_EXTERNAL_OES];
    default:
      break;
  }

  NOTREACHED();
  return shader_ids[0][SAMPLER_2D];
}

const char* kShaderPrecisionPreamble = "\
    #ifdef GL_ES\n\
    precision mediump float;\n\
    #define TexCoordPrecision mediump\n\
    #else\n\
    #define TexCoordPrecision\n\
    #endif\n";

std::string GetVertexShaderSource() {
  std::string source;

  // Preamble for core and compatibility mode.
  if (gl::GetGLImplementation() == gl::kGLImplementationDesktopGLCoreProfile) {
    source += std::string("\
        #version 150\n\
        #define ATTRIBUTE in\n\
        #define VARYING out\n");
  } else {
    source += std::string("\
        #define ATTRIBUTE attribute\n\
        #define VARYING varying\n");
  }

  // Preamble for texture precision.
  source += std::string(kShaderPrecisionPreamble);

  // Main shader source.
  source += std::string("\
      uniform vec2 u_vertex_dest_mult;\n\
      uniform vec2 u_vertex_dest_add;\n\
      uniform vec2 u_vertex_source_mult;\n\
      uniform vec2 u_vertex_source_add;\n\
      ATTRIBUTE vec2 a_position;\n\
      VARYING TexCoordPrecision vec2 v_uv;\n\
      void main(void) {\n\
        gl_Position = vec4(0, 0, 0, 1);\n\
        gl_Position.xy = a_position.xy * u_vertex_dest_mult + \
                         u_vertex_dest_add;\n\
        v_uv = a_position.xy * u_vertex_source_mult + u_vertex_source_add;\n\
      }\n");

  return source;
}

std::string GetFragmentShaderSource(bool premultiply_alpha,
                                    bool unpremultiply_alpha,
                                    GLenum target) {
  std::string source;

  // Preamble for core and compatibility mode.
  if (gl::GetGLImplementation() == gl::kGLImplementationDesktopGLCoreProfile) {
    source += std::string("\
        #version 150\n\
        out vec4 frag_color;\n\
        #define VARYING in\n\
        #define FRAGCOLOR frag_color\n\
        #define TextureLookup texture\n");
  } else {
    switch (target) {
      case GL_TEXTURE_2D:
        source += std::string("#define TextureLookup texture2D\n");
        break;
      case GL_TEXTURE_RECTANGLE_ARB:
        source += std::string("#define TextureLookup texture2DRect\n");
        break;
      case GL_TEXTURE_EXTERNAL_OES:
        source +=
            std::string("#extension GL_OES_EGL_image_external : enable\n");
        source += std::string(
            "#extension GL_NV_EGL_stream_consumer_external : enable\n");
        source += std::string("#define TextureLookup texture2D\n");
        break;
      default:
        NOTREACHED();
        break;
    }
    source += std::string("\
        #define VARYING varying\n\
        #define FRAGCOLOR gl_FragColor\n");
  }

  // Preamble for sampler type.
  switch (target) {
    case GL_TEXTURE_2D:
      source += std::string("#define SamplerType sampler2D\n");
      break;
    case GL_TEXTURE_RECTANGLE_ARB:
      source += std::string("#define SamplerType sampler2DRect\n");
      break;
    case GL_TEXTURE_EXTERNAL_OES:
      source += std::string("#define SamplerType samplerExternalOES\n");
      break;
    default:
      NOTREACHED();
      break;
  }

  // Preamble for texture precision.
  source += std::string(kShaderPrecisionPreamble);

  // Main shader source.
  source += std::string("\
      uniform SamplerType u_sampler;\n\
      uniform mat4 u_tex_coord_transform;\n\
      VARYING TexCoordPrecision vec2 v_uv;\n\
      void main(void) {\n\
        TexCoordPrecision vec4 uv = u_tex_coord_transform * vec4(v_uv, 0, 1);\n\
        FRAGCOLOR = TextureLookup(u_sampler, uv.st);\n");

  // Post-processing to premultiply or un-premultiply alpha.
  if (premultiply_alpha) {
    source += std::string("        FRAGCOLOR.rgb *= FRAGCOLOR.a;\n");
  }
  if (unpremultiply_alpha) {
    source += std::string("\
        if (FRAGCOLOR.a > 0.0)\n\
          FRAGCOLOR.rgb /= FRAGCOLOR.a;\n");
  }

  // Main function end.
  source += std::string("      }\n");

  return source;
}

void CompileShader(GLuint shader, const char* shader_source) {
  glShaderSource(shader, 1, &shader_source, 0);
  glCompileShader(shader);
#ifndef NDEBUG
  GLint compile_status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
  if (GL_TRUE != compile_status)
    DLOG(ERROR) << "CopyTextureCHROMIUM: shader compilation failure.";
#endif
}

void DeleteShader(GLuint shader) {
  if (shader)
    glDeleteShader(shader);
}

bool BindFramebufferTexture2D(GLenum target,
                              GLuint texture_id,
                              GLuint framebuffer) {
  DCHECK(target == GL_TEXTURE_2D || target == GL_TEXTURE_RECTANGLE_ARB);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(target, texture_id);
  // NVidia drivers require texture settings to be a certain way
  // or they won't report FRAMEBUFFER_COMPLETE.
  glTexParameterf(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target,
                            texture_id, 0);

#ifndef NDEBUG
  GLenum fb_status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
  if (GL_FRAMEBUFFER_COMPLETE != fb_status) {
    DLOG(ERROR) << "CopyTextureCHROMIUM: Incomplete framebuffer.";
    return false;
  }
#endif
  return true;
}

void DoCopyTexImage2D(const gpu::gles2::GLES2Decoder* decoder,
                      GLenum source_target,
                      GLuint source_id,
                      GLenum dest_target,
                      GLuint dest_id,
                      GLenum dest_internal_format,
                      GLsizei width,
                      GLsizei height,
                      GLuint framebuffer) {
  DCHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D), source_target);
  DCHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D), dest_target);
  if (BindFramebufferTexture2D(source_target, source_id, framebuffer)) {
    glBindTexture(dest_target, dest_id);
    glTexParameterf(dest_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(dest_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(dest_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(dest_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glCopyTexImage2D(dest_target, 0 /* level */, dest_internal_format,
                     0 /* x */, 0 /* y */, width, height, 0 /* border */);
  }

  decoder->RestoreTextureState(source_id);
  decoder->RestoreTextureState(dest_id);
  decoder->RestoreTextureUnitBindings(0);
  decoder->RestoreActiveTexture();
  decoder->RestoreFramebufferBindings();
}

void DoCopyTexSubImage2D(const gpu::gles2::GLES2Decoder* decoder,
                         GLenum source_target,
                         GLuint source_id,
                         GLenum dest_target,
                         GLuint dest_id,
                         GLint xoffset,
                         GLint yoffset,
                         GLint source_x,
                         GLint source_y,
                         GLsizei source_width,
                         GLsizei source_height,
                         GLuint framebuffer) {
  DCHECK(source_target == GL_TEXTURE_2D ||
         source_target == GL_TEXTURE_RECTANGLE_ARB);
  DCHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D), dest_target);
  if (BindFramebufferTexture2D(source_target, source_id, framebuffer)) {
    glBindTexture(dest_target, dest_id);
    glTexParameterf(dest_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(dest_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(dest_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(dest_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glCopyTexSubImage2D(dest_target, 0 /* level */, xoffset, yoffset,
                        source_x, source_y, source_width, source_height);
  }

  decoder->RestoreTextureState(source_id);
  decoder->RestoreTextureState(dest_id);
  decoder->RestoreTextureUnitBindings(0);
  decoder->RestoreActiveTexture();
  decoder->RestoreFramebufferBindings();
}

}  // namespace

namespace gpu {

CopyTextureCHROMIUMResourceManager::CopyTextureCHROMIUMResourceManager()
    : initialized_(false),
      vertex_shader_(0u),
      fragment_shaders_(NUM_FRAGMENT_SHADERS, 0u),
      vertex_array_object_id_(0u),
      buffer_id_(0u),
      framebuffer_(0u) {}

CopyTextureCHROMIUMResourceManager::~CopyTextureCHROMIUMResourceManager() {
  // |buffer_id_| and |framebuffer_| can be not-null because when GPU context is
  // lost, this class can be deleted without releasing resources like
  // GLES2DecoderImpl.
}

void CopyTextureCHROMIUMResourceManager::Initialize(
    const gles2::GLES2Decoder* decoder,
    const gles2::FeatureInfo::FeatureFlags& feature_flags) {
  static_assert(
      kVertexPositionAttrib == 0u,
      "kVertexPositionAttrib must be 0");
  DCHECK(!buffer_id_);
  DCHECK(!vertex_array_object_id_);
  DCHECK(!framebuffer_);
  DCHECK(programs_.empty());

  if (feature_flags.native_vertex_array_object) {
    glGenVertexArraysOES(1, &vertex_array_object_id_);
    glBindVertexArrayOES(vertex_array_object_id_);
  }

  // Initialize all of the GPU resources required to perform the copy.
  glGenBuffersARB(1, &buffer_id_);
  glBindBuffer(GL_ARRAY_BUFFER, buffer_id_);
  const GLfloat kQuadVertices[] = {-1.0f, -1.0f,
                                    1.0f, -1.0f,
                                    1.0f,  1.0f,
                                   -1.0f,  1.0f};
  glBufferData(
      GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);

  glGenFramebuffersEXT(1, &framebuffer_);

  if (vertex_array_object_id_) {
    glEnableVertexAttribArray(kVertexPositionAttrib);
    glVertexAttribPointer(kVertexPositionAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
    decoder->RestoreAllAttributes();
  }

  decoder->RestoreBufferBindings();

  initialized_ = true;
}

void CopyTextureCHROMIUMResourceManager::Destroy() {
  if (!initialized_)
    return;

  if (vertex_array_object_id_) {
    glDeleteVertexArraysOES(1, &vertex_array_object_id_);
    vertex_array_object_id_ = 0;
  }

  glDeleteFramebuffersEXT(1, &framebuffer_);
  framebuffer_ = 0;

  DeleteShader(vertex_shader_);
  std::for_each(
      fragment_shaders_.begin(), fragment_shaders_.end(), DeleteShader);

  for (ProgramMap::const_iterator it = programs_.begin(); it != programs_.end();
       ++it) {
    const ProgramInfo& info = it->second;
    glDeleteProgram(info.program);
  }

  glDeleteBuffersARB(1, &buffer_id_);
  buffer_id_ = 0;
}

void CopyTextureCHROMIUMResourceManager::DoCopyTexture(
    const gles2::GLES2Decoder* decoder,
    GLenum source_target,
    GLuint source_id,
    GLenum source_internal_format,
    GLenum dest_target,
    GLuint dest_id,
    GLenum dest_internal_format,
    GLsizei width,
    GLsizei height,
    bool flip_y,
    bool premultiply_alpha,
    bool unpremultiply_alpha) {
  bool premultiply_alpha_change = premultiply_alpha ^ unpremultiply_alpha;
  // GL_INVALID_OPERATION is generated if the currently bound framebuffer's
  // format does not contain a superset of the components required by the base
  // format of internalformat.
  // https://www.khronos.org/opengles/sdk/docs/man/xhtml/glCopyTexImage2D.xml
  bool source_format_contain_superset_of_dest_format =
      (source_internal_format == dest_internal_format &&
       source_internal_format != GL_BGRA_EXT) ||
      (source_internal_format == GL_RGBA && dest_internal_format == GL_RGB);
  // GL_TEXTURE_RECTANGLE_ARB on FBO is supported by OpenGL, not GLES2,
  // so restrict this to GL_TEXTURE_2D.
  if (source_target == GL_TEXTURE_2D && dest_target == GL_TEXTURE_2D &&
      !flip_y && !premultiply_alpha_change &&
      source_format_contain_superset_of_dest_format) {
    DoCopyTexImage2D(decoder,
                     source_target,
                     source_id,
                     dest_target,
                     dest_id,
                     dest_internal_format,
                     width,
                     height,
                     framebuffer_);
    return;
  }

  // Use kIdentityMatrix if no transform passed in.
  DoCopyTextureWithTransform(decoder, source_target, source_id, dest_target,
                             dest_id, width, height, flip_y, premultiply_alpha,
                             unpremultiply_alpha, kIdentityMatrix);
}

void CopyTextureCHROMIUMResourceManager::DoCopySubTexture(
    const gles2::GLES2Decoder* decoder,
    GLenum source_target,
    GLuint source_id,
    GLenum source_internal_format,
    GLenum dest_target,
    GLuint dest_id,
    GLenum dest_internal_format,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLsizei dest_width,
    GLsizei dest_height,
    GLsizei source_width,
    GLsizei source_height,
    bool flip_y,
    bool premultiply_alpha,
    bool unpremultiply_alpha) {
  bool premultiply_alpha_change = premultiply_alpha ^ unpremultiply_alpha;
  // GL_INVALID_OPERATION is generated if the currently bound framebuffer's
  // format does not contain a superset of the components required by the base
  // format of internalformat.
  // https://www.khronos.org/opengles/sdk/docs/man/xhtml/glCopyTexImage2D.xml
  bool source_format_contain_superset_of_dest_format =
      (source_internal_format == dest_internal_format &&
       source_internal_format != GL_BGRA_EXT) ||
      (source_internal_format == GL_RGBA && dest_internal_format == GL_RGB);
  // GL_TEXTURE_RECTANGLE_ARB on FBO is supported by OpenGL, not GLES2,
  // so restrict this to GL_TEXTURE_2D.
  if (source_target == GL_TEXTURE_2D && dest_target == GL_TEXTURE_2D &&
      !flip_y && !premultiply_alpha_change &&
      source_format_contain_superset_of_dest_format) {
    DoCopyTexSubImage2D(decoder, source_target, source_id, dest_target, dest_id,
                        xoffset, yoffset, x, y, width, height, framebuffer_);
    return;
  }

  DoCopySubTextureWithTransform(
      decoder, source_target, source_id, source_internal_format, dest_target,
      dest_id, dest_internal_format, xoffset, yoffset, x, y, width, height,
      dest_width, dest_height, source_width, source_height, flip_y,
      premultiply_alpha, unpremultiply_alpha, kIdentityMatrix);
}

void CopyTextureCHROMIUMResourceManager::DoCopySubTextureWithTransform(
    const gles2::GLES2Decoder* decoder,
    GLenum source_target,
    GLuint source_id,
    GLenum source_internal_format,
    GLenum dest_target,
    GLuint dest_id,
    GLenum dest_internal_format,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLsizei dest_width,
    GLsizei dest_height,
    GLsizei source_width,
    GLsizei source_height,
    bool flip_y,
    bool premultiply_alpha,
    bool unpremultiply_alpha,
    const GLfloat transform_matrix[16]) {
  DoCopyTextureInternal(decoder, source_target, source_id, dest_target, dest_id,
      xoffset, yoffset, x, y, width, height, dest_width, dest_height,
      source_width, source_height, flip_y, premultiply_alpha,
      unpremultiply_alpha, transform_matrix);
}

void CopyTextureCHROMIUMResourceManager::DoCopyTextureWithTransform(
    const gles2::GLES2Decoder* decoder,
    GLenum source_target,
    GLuint source_id,
    GLenum dest_target,
    GLuint dest_id,
    GLsizei width,
    GLsizei height,
    bool flip_y,
    bool premultiply_alpha,
    bool unpremultiply_alpha,
    const GLfloat transform_matrix[16]) {
  GLsizei dest_width = width;
  GLsizei dest_height = height;
  DoCopyTextureInternal(decoder, source_target, source_id, dest_target, dest_id,
                        0, 0, 0, 0, width, height, dest_width, dest_height,
                        width, height, flip_y, premultiply_alpha,
                        unpremultiply_alpha, transform_matrix);
}

void CopyTextureCHROMIUMResourceManager::DoCopyTextureInternal(
    const gles2::GLES2Decoder* decoder,
    GLenum source_target,
    GLuint source_id,
    GLenum dest_target,
    GLuint dest_id,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLsizei dest_width,
    GLsizei dest_height,
    GLsizei source_width,
    GLsizei source_height,
    bool flip_y,
    bool premultiply_alpha,
    bool unpremultiply_alpha,
    const GLfloat transform_matrix[16]) {
  DCHECK(source_target == GL_TEXTURE_2D ||
         source_target == GL_TEXTURE_RECTANGLE_ARB ||
         source_target == GL_TEXTURE_EXTERNAL_OES);
  DCHECK(dest_target == GL_TEXTURE_2D ||
         dest_target == GL_TEXTURE_RECTANGLE_ARB);
  DCHECK_GE(xoffset, 0);
  DCHECK_LE(xoffset + width, dest_width);
  DCHECK_GE(yoffset, 0);
  DCHECK_LE(yoffset + height, dest_height);
  if (dest_width == 0 || dest_height == 0 || source_width == 0 ||
      source_height == 0) {
    return;
  }

  if (!initialized_) {
    DLOG(ERROR) << "CopyTextureCHROMIUM: Uninitialized manager.";
    return;
  }

  if (vertex_array_object_id_) {
    glBindVertexArrayOES(vertex_array_object_id_);
  } else {
    if (gl::GetGLImplementation() !=
        gl::kGLImplementationDesktopGLCoreProfile) {
      decoder->ClearAllAttributes();
    }
    glEnableVertexAttribArray(kVertexPositionAttrib);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_id_);
    glVertexAttribPointer(kVertexPositionAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
  }

  FragmentShaderId fragment_shader_id = GetFragmentShaderId(
      premultiply_alpha, unpremultiply_alpha, source_target);
  DCHECK_LT(static_cast<size_t>(fragment_shader_id), fragment_shaders_.size());

  ProgramMapKey key(fragment_shader_id);
  ProgramInfo* info = &programs_[key];
  // Create program if necessary.
  if (!info->program) {
    info->program = glCreateProgram();
    if (!vertex_shader_) {
      vertex_shader_ = glCreateShader(GL_VERTEX_SHADER);
      std::string source = GetVertexShaderSource();
      CompileShader(vertex_shader_, source.c_str());
    }
    glAttachShader(info->program, vertex_shader_);
    GLuint* fragment_shader = &fragment_shaders_[fragment_shader_id];
    if (!*fragment_shader) {
      *fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
      std::string source = GetFragmentShaderSource(
          premultiply_alpha, unpremultiply_alpha, source_target);
      CompileShader(*fragment_shader, source.c_str());
    }
    glAttachShader(info->program, *fragment_shader);
    glBindAttribLocation(info->program, kVertexPositionAttrib, "a_position");
    glLinkProgram(info->program);
#ifndef NDEBUG
    GLint linked;
    glGetProgramiv(info->program, GL_LINK_STATUS, &linked);
    if (!linked)
      DLOG(ERROR) << "CopyTextureCHROMIUM: program link failure.";
#endif
    info->vertex_dest_mult_handle =
        glGetUniformLocation(info->program, "u_vertex_dest_mult");
    info->vertex_dest_add_handle =
        glGetUniformLocation(info->program, "u_vertex_dest_add");
    info->vertex_source_mult_handle =
        glGetUniformLocation(info->program, "u_vertex_source_mult");
    info->vertex_source_add_handle =
        glGetUniformLocation(info->program, "u_vertex_source_add");

    info->tex_coord_transform_handle =
        glGetUniformLocation(info->program, "u_tex_coord_transform");
    info->sampler_handle = glGetUniformLocation(info->program, "u_sampler");
  }
  glUseProgram(info->program);

  glUniformMatrix4fv(info->tex_coord_transform_handle, 1, GL_FALSE,
                     transform_matrix);

  // Note: For simplicity, the calculations in this comment block use a single
  // dimension. All calculations trivially extend to the x-y plane.
  // The target subrange in the destination texture has coordinates
  // [xoffset, xoffset + width]. The full destination texture has range
  // [0, dest_width].
  //
  // We want to find A and B such that:
  //   A * X + B = Y
  //   C * Y + D = Z
  //
  // where X = [-1, 1], Z = [xoffset, xoffset + width]
  // and C, D satisfy the relationship C * [-1, 1] + D = [0, dest_width].
  //
  // Math shows:
  //  C = D = dest_width / 2
  //  Y = [(xoffset * 2 / dest_width) - 1,
  //       (xoffset + width) * 2 / dest_width) - 1]
  //  A = width / dest_width
  //  B = (xoffset * 2 + width - dest_width) / dest_width
  glUniform2f(info->vertex_dest_mult_handle, width * 1.f / dest_width,
              height * 1.f / dest_height);
  glUniform2f(info->vertex_dest_add_handle,
              (xoffset * 2.f + width - dest_width) / dest_width,
              (yoffset * 2.f + height - dest_height) / dest_height);

  // Note: For simplicity, the calculations in this comment block use a single
  // dimension. All calculations trivially extend to the x-y plane.
  // The target subrange in the source texture has coordinates [x, x + width].
  // The full source texture has range [0, source_width]. We need to transform
  // the subrange into texture space ([0, M]), assuming that [0, source_width]
  // gets mapped to [0, M]. If source_target == GL_TEXTURE_RECTANGLE_ARB, M =
  // source_width. Otherwise, M = 1.
  //
  // We want to find A and B such that:
  //   A * X + B = Y
  //   C * Y + D = Z
  //
  // where X = [-1, 1], Z = [x, x + width]
  // and C, D satisfy the relationship C * [0, M] + D = [0, source_width].
  //
  // Math shows:
  //   D = 0
  //   C = source_width / M
  //   Y = [x * M / source_width, (x + width) * M / source_width]
  //   B = (x + w/2) * M / source_width
  //   A = (w/2) * M / source_width
  //
  // When flip_y is true, we run the same calcluation, but with Z = [x + width,
  // x]. (I'm intentionally keeping the x-plane notation, although the
  // calculation only gets applied to the y-plane).
  //
  // Math shows:
  //   D = 0
  //   C = source_width / M
  //   Y = [(x + width) * M / source_width, x * M / source_width]
  //   B = (x + w/2) * M / source_width
  //   A = (-w/2) * M / source_width
  //
  // So everything is the same but the sign of A is flipped.
  GLfloat m_x = source_target == GL_TEXTURE_RECTANGLE_ARB ? source_width : 1;
  GLfloat m_y = source_target == GL_TEXTURE_RECTANGLE_ARB ? source_height : 1;
  GLfloat sign_a = flip_y ? -1 : 1;
  glUniform2f(info->vertex_source_mult_handle, width / 2.f * m_x / source_width,
              height / 2.f * m_y / source_height * sign_a);
  glUniform2f(info->vertex_source_add_handle,
              (x + width / 2.f) * m_x / source_width,
              (y + height / 2.f) * m_y / source_height);

  if (BindFramebufferTexture2D(dest_target, dest_id, framebuffer_)) {
#ifndef NDEBUG
    // glValidateProgram of MACOSX validates FBO unlike other platforms, so
    // glValidateProgram must be called after FBO binding. crbug.com/463439
    glValidateProgram(info->program);
    GLint validation_status;
    glGetProgramiv(info->program, GL_VALIDATE_STATUS, &validation_status);
    if (GL_TRUE != validation_status) {
      DLOG(ERROR) << "CopyTextureCHROMIUM: Invalid shader.";
      return;
    }
#endif

    glUniform1i(info->sampler_handle, 0);

    glBindTexture(source_target, source_id);
    glTexParameterf(source_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(source_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(source_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(source_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    bool need_scissor =
        xoffset || yoffset || width != dest_width || height != dest_height;
    if (need_scissor) {
      glEnable(GL_SCISSOR_TEST);
      glScissor(xoffset, yoffset, width, height);
    } else {
      glDisable(GL_SCISSOR_TEST);
    }
    glViewport(0, 0, dest_width, dest_height);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  }

  decoder->RestoreAllAttributes();
  decoder->RestoreTextureState(source_id);
  decoder->RestoreTextureState(dest_id);
  decoder->RestoreTextureUnitBindings(0);
  decoder->RestoreActiveTexture();
  decoder->RestoreProgramBindings();
  decoder->RestoreBufferBindings();
  decoder->RestoreFramebufferBindings();
  decoder->RestoreGlobalState();
}

}  // namespace gpu
