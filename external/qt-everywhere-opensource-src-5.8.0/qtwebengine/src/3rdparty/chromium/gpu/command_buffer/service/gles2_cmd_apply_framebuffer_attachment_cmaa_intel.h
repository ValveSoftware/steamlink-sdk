// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_APPLY_FRAMEBUFFER_ATTACHMENT_CMAA_INTEL_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_APPLY_FRAMEBUFFER_ATTACHMENT_CMAA_INTEL_H_

#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/gpu_export.h"

namespace {
class CMAAEffect;
}

namespace gpu {
namespace gles2 {
class GLES2Decoder;
class Framebuffer;
}

// This class encapsulates the resources required to implement the
// GL_INTEL_framebuffer_CMAA extension via shaders.
//
// The CMAA Conservative Morphological Anti-Aliasing) algorithm is applied to
// all color attachments of the currently bound draw framebuffer.
//
// Reference GL_INTEL_framebuffer_CMAA for details.
class GPU_EXPORT ApplyFramebufferAttachmentCMAAINTELResourceManager {
 public:
  ApplyFramebufferAttachmentCMAAINTELResourceManager();
  ~ApplyFramebufferAttachmentCMAAINTELResourceManager();

  void Initialize(gles2::GLES2Decoder* decoder);
  void Destroy();

  // Applies the algorithm to the color attachments of the currently bound draw
  // framebuffer.
  void ApplyFramebufferAttachmentCMAAINTEL(gles2::GLES2Decoder* decoder,
                                           gles2::Framebuffer* framebuffer);

 private:
  // Applies the CMAA algorithm to a texture.
  void ApplyCMAAEffectTexture(GLuint source_texture, GLuint dest_texture);

  void OnSize(GLint width, GLint height);
  void ReleaseTextures();

  void CopyTexture(GLint source, GLint dest, bool via_fbo);
  GLuint CreateProgram(const char* defines,
                       const char* vs_source,
                       const char* fs_source);
  GLuint CreateShader(GLenum type, const char* defines, const char* source);

  bool initialized_;
  bool textures_initialized_;
  bool is_in_gamma_correct_mode_;
  bool supports_usampler_;
  bool supports_r8_image_;
  bool supports_r8_read_format_;
  bool is_gles31_compatible_;

  int frame_id_;

  GLint width_;
  GLint height_;

  GLuint copy_to_framebuffer_shader_;
  GLuint copy_to_image_shader_;
  GLuint edges0_shader_;
  GLuint edges1_shader_;
  GLuint edges_combine_shader_;
  GLuint process_and_apply_shader_;
  GLuint debug_display_edges_shader_;

  GLuint cmaa_framebuffer_;
  GLuint copy_framebuffer_;

  GLuint rgba8_texture_;
  GLuint working_color_texture_;
  GLuint edges0_texture_;
  GLuint edges1_texture_;
  GLuint mini4_edge_texture_;
  GLuint mini4_edge_depth_texture_;

  GLuint edges1_shader_result_texture_float4_slot1_;
  GLuint edges1_shader_result_texture_;
  GLuint edges_combine_shader_result_texture_float4_slot1_;
  GLuint process_and_apply_shader_result_texture_float4_slot1_;
  GLuint edges_combine_shader_result_texture_slot2_;
  GLuint copy_to_image_shader_outTexture_;

  static const char vert_str_[];
  static const char cmaa_frag_s1_[];
  static const char cmaa_frag_s2_[];
  static const char copy_frag_str_[];

  DISALLOW_COPY_AND_ASSIGN(ApplyFramebufferAttachmentCMAAINTELResourceManager);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_APPLY_FRAMEBUFFER_ATTACHMENT_CMAA_INTEL_H_
