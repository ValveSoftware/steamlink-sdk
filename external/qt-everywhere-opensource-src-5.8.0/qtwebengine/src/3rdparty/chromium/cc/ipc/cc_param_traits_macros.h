// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_CC_PARAM_TRAITS_MACROS_H_
#define CC_IPC_CC_PARAM_TRAITS_MACROS_H_

#include "cc/output/begin_frame_args.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/filter_operation.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/render_pass_id.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "cc/resources/resource_format.h"
#include "cc/resources/returned_resource.h"
#include "cc/resources/transferable_resource.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_sequence.h"
#include "ui/events/ipc/latency_info_param_traits.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CC_IPC_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(cc::DrawQuad::Material, cc::DrawQuad::MATERIAL_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(cc::FilterOperation::FilterType,
                          cc::FilterOperation::FILTER_TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(cc::ResourceFormat, cc::RESOURCE_FORMAT_MAX)

// TODO(fsamuel): This trait belongs with skia code.
IPC_ENUM_TRAITS_MAX_VALUE(SkXfermode::Mode, SkXfermode::kLastMode)
IPC_ENUM_TRAITS_MAX_VALUE(cc::YUVVideoDrawQuad::ColorSpace,
                          cc::YUVVideoDrawQuad::COLOR_SPACE_LAST)

IPC_STRUCT_TRAITS_BEGIN(cc::RenderPassId)
  IPC_STRUCT_TRAITS_MEMBER(layer_id)
  IPC_STRUCT_TRAITS_MEMBER(index)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::SurfaceSequence)
  IPC_STRUCT_TRAITS_MEMBER(id_namespace)
  IPC_STRUCT_TRAITS_MEMBER(sequence)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(material)
  IPC_STRUCT_TRAITS_MEMBER(rect)
  IPC_STRUCT_TRAITS_MEMBER(opaque_rect)
  IPC_STRUCT_TRAITS_MEMBER(visible_rect)
  IPC_STRUCT_TRAITS_MEMBER(needs_blending)
  IPC_STRUCT_TRAITS_MEMBER(resources)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::DebugBorderDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(color)
  IPC_STRUCT_TRAITS_MEMBER(width)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::RenderPassDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(render_pass_id)
  IPC_STRUCT_TRAITS_MEMBER(mask_uv_scale)
  IPC_STRUCT_TRAITS_MEMBER(mask_texture_size)
  IPC_STRUCT_TRAITS_MEMBER(filters)
  IPC_STRUCT_TRAITS_MEMBER(filters_scale)
  IPC_STRUCT_TRAITS_MEMBER(background_filters)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::SolidColorDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(color)
  IPC_STRUCT_TRAITS_MEMBER(force_anti_aliasing_off)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::StreamVideoDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(overlay_resources)
  IPC_STRUCT_TRAITS_MEMBER(matrix)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::SurfaceDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(surface_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::TextureDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(overlay_resources)
  IPC_STRUCT_TRAITS_MEMBER(premultiplied_alpha)
  IPC_STRUCT_TRAITS_MEMBER(uv_top_left)
  IPC_STRUCT_TRAITS_MEMBER(uv_bottom_right)
  IPC_STRUCT_TRAITS_MEMBER(background_color)
  IPC_STRUCT_TRAITS_MEMBER(vertex_opacity[0])
  IPC_STRUCT_TRAITS_MEMBER(vertex_opacity[1])
  IPC_STRUCT_TRAITS_MEMBER(vertex_opacity[2])
  IPC_STRUCT_TRAITS_MEMBER(vertex_opacity[3])
  IPC_STRUCT_TRAITS_MEMBER(y_flipped)
  IPC_STRUCT_TRAITS_MEMBER(nearest_neighbor)
  IPC_STRUCT_TRAITS_MEMBER(secure_output_only)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::TileDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(tex_coord_rect)
  IPC_STRUCT_TRAITS_MEMBER(texture_size)
  IPC_STRUCT_TRAITS_MEMBER(swizzle_contents)
  IPC_STRUCT_TRAITS_MEMBER(nearest_neighbor)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::YUVVideoDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(ya_tex_coord_rect)
  IPC_STRUCT_TRAITS_MEMBER(uv_tex_coord_rect)
  IPC_STRUCT_TRAITS_MEMBER(ya_tex_size)
  IPC_STRUCT_TRAITS_MEMBER(uv_tex_size)
  IPC_STRUCT_TRAITS_MEMBER(color_space)
  IPC_STRUCT_TRAITS_MEMBER(resource_offset)
  IPC_STRUCT_TRAITS_MEMBER(resource_multiplier)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::SharedQuadState)
  IPC_STRUCT_TRAITS_MEMBER(quad_to_target_transform)
  IPC_STRUCT_TRAITS_MEMBER(quad_layer_bounds)
  IPC_STRUCT_TRAITS_MEMBER(visible_quad_layer_rect)
  IPC_STRUCT_TRAITS_MEMBER(clip_rect)
  IPC_STRUCT_TRAITS_MEMBER(is_clipped)
  IPC_STRUCT_TRAITS_MEMBER(opacity)
  IPC_STRUCT_TRAITS_MEMBER(blend_mode)
  IPC_STRUCT_TRAITS_MEMBER(sorting_context_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::TransferableResource)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(format)
  IPC_STRUCT_TRAITS_MEMBER(filter)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(mailbox_holder)
  IPC_STRUCT_TRAITS_MEMBER(read_lock_fences_enabled)
  IPC_STRUCT_TRAITS_MEMBER(is_software)
  IPC_STRUCT_TRAITS_MEMBER(is_overlay_candidate)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::ReturnedResource)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(sync_token)
  IPC_STRUCT_TRAITS_MEMBER(count)
  IPC_STRUCT_TRAITS_MEMBER(lost)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::Selection<gfx::SelectionBound>)
  IPC_STRUCT_TRAITS_MEMBER(start)
  IPC_STRUCT_TRAITS_MEMBER(end)
  IPC_STRUCT_TRAITS_MEMBER(is_editable)
  IPC_STRUCT_TRAITS_MEMBER(is_empty_text_form_control)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(cc::BeginFrameArgs::BeginFrameArgsType,
                          cc::BeginFrameArgs::BEGIN_FRAME_ARGS_TYPE_MAX - 1)

IPC_STRUCT_TRAITS_BEGIN(cc::BeginFrameArgs)
  IPC_STRUCT_TRAITS_MEMBER(frame_time)
  IPC_STRUCT_TRAITS_MEMBER(deadline)
  IPC_STRUCT_TRAITS_MEMBER(interval)
  IPC_STRUCT_TRAITS_MEMBER(type)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::CompositorFrameMetadata)
  IPC_STRUCT_TRAITS_MEMBER(device_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(root_scroll_offset)
  IPC_STRUCT_TRAITS_MEMBER(page_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(scrollable_viewport_size)
  IPC_STRUCT_TRAITS_MEMBER(root_layer_size)
  IPC_STRUCT_TRAITS_MEMBER(min_page_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(max_page_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(root_overflow_x_hidden)
  IPC_STRUCT_TRAITS_MEMBER(root_overflow_y_hidden)
  IPC_STRUCT_TRAITS_MEMBER(location_bar_offset)
  IPC_STRUCT_TRAITS_MEMBER(location_bar_content_translation)
  IPC_STRUCT_TRAITS_MEMBER(root_background_color)
  IPC_STRUCT_TRAITS_MEMBER(selection)
  IPC_STRUCT_TRAITS_MEMBER(latency_info)
  IPC_STRUCT_TRAITS_MEMBER(satisfies_sequences)
  IPC_STRUCT_TRAITS_MEMBER(referenced_surfaces)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::GLFrameData)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(sub_buffer_rect)
IPC_STRUCT_TRAITS_END()

#endif  // CC_IPC_CC_PARAM_TRAITS_MACROS_H_
