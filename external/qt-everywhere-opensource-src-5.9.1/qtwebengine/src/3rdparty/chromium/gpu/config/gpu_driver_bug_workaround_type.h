// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_DRIVER_BUG_WORKAROUND_TYPE_H_
#define GPU_CONFIG_GPU_DRIVER_BUG_WORKAROUND_TYPE_H_

#include <string>

#include "gpu/gpu_export.h"

// Clang format is toggled off here so that newlines can be kept consistent
// throughout the table.
// clang-format off
#define GPU_DRIVER_BUG_WORKAROUNDS(GPU_OP)                   \
  GPU_OP(ADD_AND_TRUE_TO_LOOP_CONDITION,                     \
         add_and_true_to_loop_condition)                     \
  GPU_OP(ADJUST_SRC_DST_REGION_FOR_BLITFRAMEBUFFER,          \
         adjust_src_dst_region_for_blitframebuffer)          \
  GPU_OP(AVDA_DONT_COPY_PICTURES,                            \
         avda_dont_copy_pictures)                            \
  GPU_OP(AVOID_EGL_IMAGE_TARGET_TEXTURE_REUSE,               \
         avoid_egl_image_target_texture_reuse)               \
  GPU_OP(AVOID_ONE_COMPONENT_EGL_IMAGES,                     \
         avoid_one_component_egl_images)                     \
  GPU_OP(BROKEN_EGL_IMAGE_REF_COUNTING,                      \
         broken_egl_image_ref_counting)                      \
  GPU_OP(CLEAR_ALPHA_IN_READPIXELS,                          \
         clear_alpha_in_readpixels)                          \
  GPU_OP(CLEAR_UNIFORMS_BEFORE_FIRST_PROGRAM_USE,            \
         clear_uniforms_before_first_program_use)            \
  GPU_OP(COUNT_ALL_IN_VARYINGS_PACKING,                      \
         count_all_in_varyings_packing)                      \
  GPU_OP(CREATE_DEFAULT_GL_CONTEXT,                          \
         create_default_gl_context)                          \
  GPU_OP(DECODE_ENCODE_SRGB_FOR_GENERATEMIPMAP,              \
         decode_encode_srgb_for_generatemipmap)              \
  GPU_OP(DISABLE_ANGLE_INSTANCED_ARRAYS,                     \
         disable_angle_instanced_arrays)                     \
  GPU_OP(DISABLE_ASYNC_READPIXELS,                           \
         disable_async_readpixels)                           \
  GPU_OP(DISABLE_AV_SAMPLE_BUFFER_DISPLAY_LAYER,             \
         disable_av_sample_buffer_display_layer)             \
  GPU_OP(DISABLE_BLEND_EQUATION_ADVANCED,                    \
         disable_blend_equation_advanced)                    \
  GPU_OP(DISABLE_CHROMIUM_FRAMEBUFFER_MULTISAMPLE,           \
         disable_chromium_framebuffer_multisample)           \
  GPU_OP(DISABLE_D3D11,                                      \
         disable_d3d11)                                      \
  GPU_OP(DISABLE_DEPTH_TEXTURE,                              \
         disable_depth_texture)                              \
  GPU_OP(DISABLE_DIRECT_COMPOSITION,                         \
         disable_direct_composition)                         \
  GPU_OP(DISABLE_DISCARD_FRAMEBUFFER,                        \
         disable_discard_framebuffer)                        \
  GPU_OP(DISABLE_DXGI_ZERO_COPY_VIDEO,                       \
         disable_dxgi_zero_copy_video)                       \
  GPU_OP(DISABLE_ES3_GL_CONTEXT,                             \
         disable_es3_gl_context)                             \
  GPU_OP(DISABLE_EXT_DRAW_BUFFERS,                           \
         disable_ext_draw_buffers)                           \
  GPU_OP(DISABLE_FRAMEBUFFER_CMAA,                           \
         disable_framebuffer_cmaa)                           \
  GPU_OP(DISABLE_GL_RGB_FORMAT,                              \
         disable_gl_rgb_format)                              \
  GPU_OP(DISABLE_MULTIMONITOR_MULTISAMPLING,                 \
         disable_multimonitor_multisampling)                 \
  GPU_OP(DISABLE_NV12_DXGI_VIDEO,                            \
         disable_nv12_dxgi_video)                            \
  GPU_OP(DISABLE_OVERLAY_CA_LAYERS,                          \
         disable_overlay_ca_layers)                          \
  GPU_OP(DISABLE_POST_SUB_BUFFERS_FOR_ONSCREEN_SURFACES,     \
         disable_post_sub_buffers_for_onscreen_surfaces)     \
  GPU_OP(DISABLE_PROGRAM_CACHE,                              \
         disable_program_cache)                              \
  GPU_OP(DISABLE_PROGRAM_DISK_CACHE,                         \
         disable_program_disk_cache)                         \
  GPU_OP(DISABLE_TEXTURE_CUBE_MAP_SEAMLESS,                  \
         disable_texture_cube_map_seamless)                  \
  GPU_OP(DISABLE_TEXTURE_STORAGE,                            \
         disable_texture_storage)                            \
  GPU_OP(DISABLE_TIMESTAMP_QUERIES,                          \
         disable_timestamp_queries)                          \
  GPU_OP(DISABLE_MULTISAMPLING_COLOR_MASK_USAGE,             \
         disable_multisampling_color_mask_usage)             \
  GPU_OP(DISABLE_WEBGL_RGB_MULTISAMPLING_USAGE,              \
         disable_webgl_rgb_multisampling_usage)              \
  GPU_OP(DONT_REMOVE_INVARIANT_FOR_FRAGMENT_INPUT,           \
         dont_remove_invariant_for_fragment_input)           \
  GPU_OP(ETC1_POWER_OF_TWO_ONLY,                             \
         etc1_power_of_two_only)                             \
  GPU_OP(EMULATE_ABS_INT_FUNCTION,                           \
         emulate_abs_int_function)                           \
  GPU_OP(EMULATE_ISNAN_ON_FLOAT,                             \
         emulate_isnan_on_float)                             \
  GPU_OP(EXIT_ON_CONTEXT_LOST,                               \
         exit_on_context_lost)                               \
  GPU_OP(FORCE_CUBE_COMPLETE,                                \
         force_cube_complete)                                \
  GPU_OP(FORCE_CUBE_MAP_POSITIVE_X_ALLOCATION,               \
         force_cube_map_positive_x_allocation)               \
  GPU_OP(FORCE_DISCRETE_GPU,                                 \
         force_discrete_gpu)                                 \
  GPU_OP(FORCE_INTEGRATED_GPU,                               \
         force_integrated_gpu)                               \
  GPU_OP(GET_FRAG_DATA_INFO_BUG,                             \
         get_frag_data_info_bug)                             \
  GPU_OP(GL_CLEAR_BROKEN,                                    \
         gl_clear_broken)                                    \
  GPU_OP(IGNORE_EGL_SYNC_FAILURES,                           \
         ignore_egl_sync_failures)                           \
  GPU_OP(INIT_GL_POSITION_IN_VERTEX_SHADER,                  \
         init_gl_position_in_vertex_shader)                  \
  GPU_OP(INIT_ONE_CUBE_MAP_LEVEL_BEFORE_COPYTEXIMAGE,        \
         init_one_cube_map_level_before_copyteximage)        \
  GPU_OP(INIT_TEXTURE_MAX_ANISOTROPY,                        \
         init_texture_max_anisotropy)                        \
  GPU_OP(INIT_TWO_CUBE_MAP_LEVELS_BEFORE_COPYTEXIMAGE,       \
         init_two_cube_map_levels_before_copyteximage)       \
  GPU_OP(INIT_VERTEX_ATTRIBUTES,                             \
         init_vertex_attributes)                             \
  GPU_OP(MAX_COPY_TEXTURE_CHROMIUM_SIZE_1048576,             \
         max_copy_texture_chromium_size_1048576)             \
  GPU_OP(MAX_COPY_TEXTURE_CHROMIUM_SIZE_262144,              \
         max_copy_texture_chromium_size_262144)              \
  GPU_OP(MAX_FRAGMENT_UNIFORM_VECTORS_32,                    \
         max_fragment_uniform_vectors_32)                    \
  GPU_OP(MAX_TEXTURE_SIZE_LIMIT_4096,                        \
         max_texture_size_limit_4096)                        \
  GPU_OP(MAX_VARYING_VECTORS_16,                             \
         max_varying_vectors_16)                             \
  GPU_OP(MAX_VERTEX_UNIFORM_VECTORS_256,                     \
         max_vertex_uniform_vectors_256)                     \
  GPU_OP(MSAA_IS_SLOW,                                       \
         msaa_is_slow)                                       \
  GPU_OP(NEEDS_OFFSCREEN_BUFFER_WORKAROUND,                  \
         needs_offscreen_buffer_workaround)                  \
  GPU_OP(PACK_PARAMETERS_WORKAROUND_WITH_PACK_BUFFER,        \
         pack_parameters_workaround_with_pack_buffer)        \
  GPU_OP(REBIND_TRANSFORM_FEEDBACK_BEFORE_RESUME,            \
         rebind_transform_feedback_before_resume)            \
  GPU_OP(REGENERATE_STRUCT_NAMES,                            \
         regenerate_struct_names)                            \
  GPU_OP(REMOVE_INVARIANT_AND_CENTROID_FOR_ESSL3,            \
         remove_invariant_and_centroid_for_essl3)            \
  GPU_OP(REMOVE_POW_WITH_CONSTANT_EXPONENT,                  \
         remove_pow_with_constant_exponent)                  \
  GPU_OP(RESET_BASE_MIPMAP_LEVEL_BEFORE_TEXSTORAGE,          \
         reset_base_mipmap_level_before_texstorage)          \
  GPU_OP(RESTORE_SCISSOR_ON_FBO_CHANGE,                      \
         restore_scissor_on_fbo_change)                      \
  GPU_OP(REVERSE_POINT_SPRITE_COORD_ORIGIN,                  \
         reverse_point_sprite_coord_origin)                  \
  GPU_OP(REWRITE_DO_WHILE_LOOPS,                             \
         rewrite_do_while_loops)                             \
  GPU_OP(REWRITE_TEXELFETCHOFFSET_TO_TEXELFETCH,             \
         rewrite_texelfetchoffset_to_texelfetch)             \
  GPU_OP(SCALARIZE_VEC_AND_MAT_CONSTRUCTOR_ARGS,             \
         scalarize_vec_and_mat_constructor_args)             \
  GPU_OP(SET_TEXTURE_FILTER_BEFORE_GENERATING_MIPMAP,        \
         set_texture_filter_before_generating_mipmap)        \
  GPU_OP(SET_ZERO_LEVEL_BEFORE_GENERATING_MIPMAP,            \
         set_zero_level_before_generating_mipmap)            \
  GPU_OP(SIMULATE_OUT_OF_MEMORY_ON_LARGE_TEXTURES,           \
         simulate_out_of_memory_on_large_textures)           \
  GPU_OP(SURFACE_TEXTURE_CANT_DETACH,                        \
         surface_texture_cant_detach)                        \
  GPU_OP(SWIZZLE_RGBA_FOR_ASYNC_READPIXELS,                  \
         swizzle_rgba_for_async_readpixels)                  \
  GPU_OP(TEXSUBIMAGE_FASTER_THAN_TEXIMAGE,                   \
         texsubimage_faster_than_teximage)                   \
  GPU_OP(UNBIND_ATTACHMENTS_ON_BOUND_RENDER_FBO_DELETE,      \
         unbind_attachments_on_bound_render_fbo_delete)      \
  GPU_OP(UNBIND_EGL_CONTEXT_TO_FLUSH_DRIVER_CACHES,          \
         unbind_egl_context_to_flush_driver_caches)          \
  GPU_OP(UNBIND_FBO_ON_CONTEXT_SWITCH,                       \
         unbind_fbo_on_context_switch)                       \
  GPU_OP(UNFOLD_SHORT_CIRCUIT_AS_TERNARY_OPERATION,          \
         unfold_short_circuit_as_ternary_operation)          \
  GPU_OP(UNPACK_ALIGNMENT_WORKAROUND_WITH_UNPACK_BUFFER,     \
         unpack_alignment_workaround_with_unpack_buffer)     \
  GPU_OP(UNPACK_IMAGE_HEIGHT_WORKAROUND_WITH_UNPACK_BUFFER,  \
         unpack_image_height_workaround_with_unpack_buffer)  \
  GPU_OP(UNPACK_OVERLAPPING_ROWS_SEPARATELY_UNPACK_BUFFER,   \
         unpack_overlapping_rows_separately_unpack_buffer)   \
  GPU_OP(USE_CLIENT_SIDE_ARRAYS_FOR_STREAM_BUFFERS,          \
         use_client_side_arrays_for_stream_buffers)          \
  GPU_OP(USE_INTERMEDIARY_FOR_COPY_TEXTURE_IMAGE,            \
         use_intermediary_for_copy_texture_image)            \
  GPU_OP(USE_NON_ZERO_SIZE_FOR_CLIENT_SIDE_STREAM_BUFFERS,   \
         use_non_zero_size_for_client_side_stream_buffers)   \
  GPU_OP(USE_SHADOWED_TEX_LEVEL_PARAMS,                      \
         use_shadowed_tex_level_params)                      \
  GPU_OP(USE_UNUSED_STANDARD_SHARED_BLOCKS,                  \
         use_unused_standard_shared_blocks)                  \
  GPU_OP(USE_VIRTUALIZED_GL_CONTEXTS,                        \
         use_virtualized_gl_contexts)                        \
  GPU_OP(VALIDATE_MULTISAMPLE_BUFFER_ALLOCATION,             \
         validate_multisample_buffer_allocation)             \
  GPU_OP(WAKE_UP_GPU_BEFORE_DRAWING,                         \
         wake_up_gpu_before_drawing)                         \
  GPU_OP(USE_TESTING_GPU_DRIVER_WORKAROUND,                  \
         use_gpu_driver_workaround_for_testing)              \
// clang-format on

namespace gpu {

// Provides all types of GPU driver bug workarounds.
enum GpuDriverBugWorkaroundType {
#define GPU_OP(type, name) type,
  GPU_DRIVER_BUG_WORKAROUNDS(GPU_OP)
#undef GPU_OP
  NUMBER_OF_GPU_DRIVER_BUG_WORKAROUND_TYPES
};

GPU_EXPORT std::string GpuDriverBugWorkaroundTypeToString(
    GpuDriverBugWorkaroundType type);

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_DRIVER_BUG_WORKAROUND_TYPE_H_
