// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard here.

#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/gpu_export.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/param_traits_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT GPU_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(gpu::error::Error, gpu::error::kErrorLast)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(
    gpu::CommandBufferNamespace,
    gpu::CommandBufferNamespace::INVALID,
    gpu::CommandBufferNamespace::NUM_COMMAND_BUFFER_NAMESPACES - 1)

IPC_STRUCT_TRAITS_BEGIN(gpu::Capabilities::ShaderPrecision)
  IPC_STRUCT_TRAITS_MEMBER(min_range)
  IPC_STRUCT_TRAITS_MEMBER(max_range)
  IPC_STRUCT_TRAITS_MEMBER(precision)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gpu::Capabilities::PerStagePrecisions)
  IPC_STRUCT_TRAITS_MEMBER(low_int)
  IPC_STRUCT_TRAITS_MEMBER(medium_int)
  IPC_STRUCT_TRAITS_MEMBER(high_int)
  IPC_STRUCT_TRAITS_MEMBER(low_float)
  IPC_STRUCT_TRAITS_MEMBER(medium_float)
  IPC_STRUCT_TRAITS_MEMBER(high_float)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gpu::Capabilities)
  IPC_STRUCT_TRAITS_MEMBER(vertex_shader_precisions)
  IPC_STRUCT_TRAITS_MEMBER(fragment_shader_precisions)
  IPC_STRUCT_TRAITS_MEMBER(max_combined_texture_image_units)
  IPC_STRUCT_TRAITS_MEMBER(max_cube_map_texture_size)
  IPC_STRUCT_TRAITS_MEMBER(max_fragment_uniform_vectors)
  IPC_STRUCT_TRAITS_MEMBER(max_renderbuffer_size)
  IPC_STRUCT_TRAITS_MEMBER(max_texture_image_units)
  IPC_STRUCT_TRAITS_MEMBER(max_texture_size)
  IPC_STRUCT_TRAITS_MEMBER(max_varying_vectors)
  IPC_STRUCT_TRAITS_MEMBER(max_vertex_attribs)
  IPC_STRUCT_TRAITS_MEMBER(max_vertex_texture_image_units)
  IPC_STRUCT_TRAITS_MEMBER(max_vertex_uniform_vectors)
  IPC_STRUCT_TRAITS_MEMBER(num_compressed_texture_formats)
  IPC_STRUCT_TRAITS_MEMBER(num_shader_binary_formats)
  IPC_STRUCT_TRAITS_MEMBER(bind_generates_resource_chromium)

  IPC_STRUCT_TRAITS_MEMBER(max_3d_texture_size)
  IPC_STRUCT_TRAITS_MEMBER(max_array_texture_layers)
  IPC_STRUCT_TRAITS_MEMBER(max_color_attachments)
  IPC_STRUCT_TRAITS_MEMBER(max_combined_fragment_uniform_components)
  IPC_STRUCT_TRAITS_MEMBER(max_combined_uniform_blocks)
  IPC_STRUCT_TRAITS_MEMBER(max_combined_vertex_uniform_components)
  IPC_STRUCT_TRAITS_MEMBER(max_copy_texture_chromium_size)
  IPC_STRUCT_TRAITS_MEMBER(max_draw_buffers)
  IPC_STRUCT_TRAITS_MEMBER(max_element_index)
  IPC_STRUCT_TRAITS_MEMBER(max_elements_indices)
  IPC_STRUCT_TRAITS_MEMBER(max_elements_vertices)
  IPC_STRUCT_TRAITS_MEMBER(max_fragment_input_components)
  IPC_STRUCT_TRAITS_MEMBER(max_fragment_uniform_blocks)
  IPC_STRUCT_TRAITS_MEMBER(max_fragment_uniform_components)
  IPC_STRUCT_TRAITS_MEMBER(max_program_texel_offset)
  IPC_STRUCT_TRAITS_MEMBER(max_samples)
  IPC_STRUCT_TRAITS_MEMBER(max_server_wait_timeout)
  IPC_STRUCT_TRAITS_MEMBER(max_texture_lod_bias)
  IPC_STRUCT_TRAITS_MEMBER(max_transform_feedback_interleaved_components)
  IPC_STRUCT_TRAITS_MEMBER(max_transform_feedback_separate_attribs)
  IPC_STRUCT_TRAITS_MEMBER(max_transform_feedback_separate_components)
  IPC_STRUCT_TRAITS_MEMBER(max_uniform_block_size)
  IPC_STRUCT_TRAITS_MEMBER(max_uniform_buffer_bindings)
  IPC_STRUCT_TRAITS_MEMBER(max_varying_components)
  IPC_STRUCT_TRAITS_MEMBER(max_vertex_output_components)
  IPC_STRUCT_TRAITS_MEMBER(max_vertex_uniform_blocks)
  IPC_STRUCT_TRAITS_MEMBER(max_vertex_uniform_components)
  IPC_STRUCT_TRAITS_MEMBER(min_program_texel_offset)
  IPC_STRUCT_TRAITS_MEMBER(num_extensions)
  IPC_STRUCT_TRAITS_MEMBER(num_program_binary_formats)
  IPC_STRUCT_TRAITS_MEMBER(uniform_buffer_offset_alignment)

  IPC_STRUCT_TRAITS_MEMBER(post_sub_buffer)
  IPC_STRUCT_TRAITS_MEMBER(commit_overlay_planes)
  IPC_STRUCT_TRAITS_MEMBER(egl_image_external)
  IPC_STRUCT_TRAITS_MEMBER(texture_format_atc)
  IPC_STRUCT_TRAITS_MEMBER(texture_format_bgra8888)
  IPC_STRUCT_TRAITS_MEMBER(texture_format_dxt1)
  IPC_STRUCT_TRAITS_MEMBER(texture_format_dxt5)
  IPC_STRUCT_TRAITS_MEMBER(texture_format_etc1)
  IPC_STRUCT_TRAITS_MEMBER(texture_format_etc1_npot)
  IPC_STRUCT_TRAITS_MEMBER(texture_rectangle)
  IPC_STRUCT_TRAITS_MEMBER(iosurface)
  IPC_STRUCT_TRAITS_MEMBER(texture_usage)
  IPC_STRUCT_TRAITS_MEMBER(texture_storage)
  IPC_STRUCT_TRAITS_MEMBER(discard_framebuffer)
  IPC_STRUCT_TRAITS_MEMBER(sync_query)
  IPC_STRUCT_TRAITS_MEMBER(image)
  IPC_STRUCT_TRAITS_MEMBER(future_sync_points)
  IPC_STRUCT_TRAITS_MEMBER(blend_equation_advanced)
  IPC_STRUCT_TRAITS_MEMBER(blend_equation_advanced_coherent)
  IPC_STRUCT_TRAITS_MEMBER(texture_rg)
  IPC_STRUCT_TRAITS_MEMBER(texture_half_float_linear)
  IPC_STRUCT_TRAITS_MEMBER(image_ycbcr_422)
  IPC_STRUCT_TRAITS_MEMBER(image_ycbcr_420v)
  IPC_STRUCT_TRAITS_MEMBER(render_buffer_format_bgra8888)
  IPC_STRUCT_TRAITS_MEMBER(occlusion_query_boolean)
  IPC_STRUCT_TRAITS_MEMBER(timer_queries)
  IPC_STRUCT_TRAITS_MEMBER(surfaceless)
  IPC_STRUCT_TRAITS_MEMBER(flips_vertically)
  IPC_STRUCT_TRAITS_MEMBER(disable_webgl_multisampling_color_mask_usage)
  IPC_STRUCT_TRAITS_MEMBER(disable_webgl_rgb_multisampling_usage)
  IPC_STRUCT_TRAITS_MEMBER(msaa_is_slow)
  IPC_STRUCT_TRAITS_MEMBER(chromium_image_rgb_emulation)
  IPC_STRUCT_TRAITS_MEMBER(emulate_rgb_buffer_with_rgba)

  IPC_STRUCT_TRAITS_MEMBER(major_version)
  IPC_STRUCT_TRAITS_MEMBER(minor_version)
IPC_STRUCT_TRAITS_END()
