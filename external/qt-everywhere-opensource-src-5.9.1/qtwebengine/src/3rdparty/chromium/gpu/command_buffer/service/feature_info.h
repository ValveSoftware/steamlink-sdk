// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_FEATURE_INFO_H_
#define GPU_COMMAND_BUFFER_SERVICE_FEATURE_INFO_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_validation.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/gpu_export.h"

namespace base {
class CommandLine;
}

namespace gl {
struct GLVersionInfo;
}

namespace gpu {
namespace gles2 {

// FeatureInfo records the features that are available for a ContextGroup.
class GPU_EXPORT FeatureInfo : public base::RefCounted<FeatureInfo> {
 public:
  struct FeatureFlags {
    FeatureFlags();

    bool chromium_framebuffer_multisample;
    bool chromium_sync_query;
    // Use glBlitFramebuffer() and glRenderbufferStorageMultisample() with
    // GL_EXT_framebuffer_multisample-style semantics, since they are exposed
    // as core GL functions on this implementation.
    bool use_core_framebuffer_multisample;
    bool multisampled_render_to_texture;
    // Use the IMG GLenum values and functions rather than EXT.
    bool use_img_for_multisampled_render_to_texture;
    bool chromium_screen_space_antialiasing;
    bool use_chromium_screen_space_antialiasing_via_shaders;
    bool oes_standard_derivatives;
    bool oes_egl_image_external;
    bool nv_egl_stream_consumer_external;
    bool oes_depth24;
    bool oes_compressed_etc1_rgb8_texture;
    bool packed_depth24_stencil8;
    bool npot_ok;
    bool enable_texture_float_linear;
    bool enable_texture_half_float_linear;
    bool angle_translated_shader_source;
    bool angle_pack_reverse_row_order;
    bool arb_texture_rectangle;
    bool angle_instanced_arrays;
    bool occlusion_query_boolean;
    bool use_arb_occlusion_query2_for_occlusion_query_boolean;
    bool use_arb_occlusion_query_for_occlusion_query_boolean;
    bool native_vertex_array_object;
    bool ext_texture_format_astc;
    bool ext_texture_format_atc;
    bool ext_texture_format_bgra8888;
    bool ext_texture_format_dxt1;
    bool ext_texture_format_dxt5;
    bool enable_shader_name_hashing;
    bool enable_samplers;
    bool ext_draw_buffers;
    bool nv_draw_buffers;
    bool ext_frag_depth;
    bool ext_shader_texture_lod;
    bool use_async_readpixels;
    bool map_buffer_range;
    bool ext_discard_framebuffer;
    bool angle_depth_texture;
    bool is_swiftshader;
    bool angle_texture_usage;
    bool ext_texture_storage;
    bool chromium_path_rendering;
    bool chromium_framebuffer_mixed_samples;
    bool blend_equation_advanced;
    bool blend_equation_advanced_coherent;
    bool ext_texture_rg;
    bool chromium_image_ycbcr_420v;
    bool chromium_image_ycbcr_422;
    bool emulate_primitive_restart_fixed_index;
    bool ext_render_buffer_format_bgra8888;
    bool ext_multisample_compatibility;
    bool ext_blend_func_extended;
    bool ext_read_format_bgra;
    bool desktop_srgb_support;
    bool arb_es3_compatibility;
    bool chromium_color_buffer_float_rgb = false;
    bool chromium_color_buffer_float_rgba = false;
    bool angle_robust_client_memory = false;
    bool khr_debug = false;
    bool chromium_bind_generates_resource = false;
    bool angle_webgl_compatibility = false;
  };

  FeatureInfo();

  // Constructor with workarounds taken from the current process's CommandLine
  explicit FeatureInfo(
      const GpuDriverBugWorkarounds& gpu_driver_bug_workarounds);

  // Constructor with workarounds taken from |command_line|.
  FeatureInfo(const base::CommandLine& command_line,
              const GpuDriverBugWorkarounds& gpu_driver_bug_workarounds);

  // Initializes the feature information. Needs a current GL context.
  bool Initialize(ContextType context_type,
                  const DisallowedFeatures& disallowed_features);

  // Helper that defaults to no disallowed features and a GLES2 context.
  bool InitializeForTesting();
  // Helper that defaults to no disallowed Features.
  bool InitializeForTesting(ContextType context_type);
  // Helper that defaults to a GLES2 context.
  bool InitializeForTesting(const DisallowedFeatures& disallowed_features);

  const Validators* validators() const {
    return &validators_;
  }

  ContextType context_type() const { return context_type_; }

  const std::string& extensions() const {
    return extensions_;
  }

  const FeatureFlags& feature_flags() const {
    return feature_flags_;
  }

  const GpuDriverBugWorkarounds& workarounds() const { return workarounds_; }

  const DisallowedFeatures& disallowed_features() const {
    return disallowed_features_;
  }

  const gl::GLVersionInfo& gl_version_info() const {
    DCHECK(gl_version_info_.get());
    return *(gl_version_info_.get());
  }

  bool IsES3Capable() const;
  void EnableES3Validators();

  bool disable_shader_translator() const { return disable_shader_translator_; }

  bool IsWebGLContext() const;
  bool IsWebGL1OrES2Context() const;
  bool IsWebGL2OrES3Context() const;

  void EnableCHROMIUMColorBufferFloatRGBA();
  void EnableCHROMIUMColorBufferFloatRGB();
  void EnableEXTColorBufferFloat();
  void EnableOESTextureFloatLinear();
  void EnableOESTextureHalfFloatLinear();

  bool ext_color_buffer_float_available() const {
    return ext_color_buffer_float_available_;
  }

  bool oes_texture_float_linear_available() const {
    return oes_texture_float_linear_available_;
  }

 private:
  friend class base::RefCounted<FeatureInfo>;
  friend class BufferManagerClientSideArraysTest;

  ~FeatureInfo();

  void AddExtensionString(const char* s);
  void InitializeBasicState(const base::CommandLine* command_line);
  void InitializeFeatures();

  Validators validators_;

  DisallowedFeatures disallowed_features_;

  ContextType context_type_;

  // The extensions string returned by glGetString(GL_EXTENSIONS);
  std::string extensions_;

  // Flags for some features
  FeatureFlags feature_flags_;

  // Flags for Workarounds.
  const GpuDriverBugWorkarounds workarounds_;

  bool ext_color_buffer_float_available_;
  bool oes_texture_float_linear_available_;
  bool oes_texture_half_float_linear_available_;

  bool disable_shader_translator_;
  std::unique_ptr<gl::GLVersionInfo> gl_version_info_;

  DISALLOW_COPY_AND_ASSIGN(FeatureInfo);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_FEATURE_INFO_H_
