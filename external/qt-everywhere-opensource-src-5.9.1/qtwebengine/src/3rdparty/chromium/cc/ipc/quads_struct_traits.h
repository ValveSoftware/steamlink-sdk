// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_QUADS_STRUCT_TRAITS_H_
#define CC_IPC_QUADS_STRUCT_TRAITS_H_

#include "cc/ipc/filter_operation_struct_traits.h"
#include "cc/ipc/filter_operations_struct_traits.h"
#include "cc/ipc/quads.mojom-shared.h"
#include "cc/ipc/render_pass_id_struct_traits.h"
#include "cc/ipc/shared_quad_state_struct_traits.h"
#include "cc/ipc/surface_id_struct_traits.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/picture_draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace mojo {

cc::DrawQuad* AllocateAndConstruct(
    cc::mojom::DrawQuadStateDataView::Tag material,
    cc::QuadList* list);

template <>
struct UnionTraits<cc::mojom::DrawQuadStateDataView, cc::DrawQuad> {
  static cc::mojom::DrawQuadStateDataView::Tag GetTag(
      const cc::DrawQuad& quad) {
    switch (quad.material) {
      case cc::DrawQuad::INVALID:
        break;
      case cc::DrawQuad::DEBUG_BORDER:
        return cc::mojom::DrawQuadStateDataView::Tag::DEBUG_BORDER_QUAD_STATE;
      case cc::DrawQuad::PICTURE_CONTENT:
        break;
      case cc::DrawQuad::RENDER_PASS:
        return cc::mojom::DrawQuadStateDataView::Tag::RENDER_PASS_QUAD_STATE;
      case cc::DrawQuad::SOLID_COLOR:
        return cc::mojom::DrawQuadStateDataView::Tag::SOLID_COLOR_QUAD_STATE;
      case cc::DrawQuad::STREAM_VIDEO_CONTENT:
        return cc::mojom::DrawQuadStateDataView::Tag::STREAM_VIDEO_QUAD_STATE;
      case cc::DrawQuad::SURFACE_CONTENT:
        return cc::mojom::DrawQuadStateDataView::Tag::SURFACE_QUAD_STATE;
      case cc::DrawQuad::TEXTURE_CONTENT:
        return cc::mojom::DrawQuadStateDataView::Tag::TEXTURE_QUAD_STATE;
      case cc::DrawQuad::TILED_CONTENT:
        return cc::mojom::DrawQuadStateDataView::Tag::TILE_QUAD_STATE;
      case cc::DrawQuad::YUV_VIDEO_CONTENT:
        return cc::mojom::DrawQuadStateDataView::Tag::YUV_VIDEO_QUAD_STATE;
    }
    NOTREACHED();
    return cc::mojom::DrawQuadStateDataView::Tag::DEBUG_BORDER_QUAD_STATE;
  }

  static const cc::DrawQuad& debug_border_quad_state(const cc::DrawQuad& quad) {
    return quad;
  }

  static const cc::DrawQuad& render_pass_quad_state(const cc::DrawQuad& quad) {
    return quad;
  }

  static const cc::DrawQuad& solid_color_quad_state(const cc::DrawQuad& quad) {
    return quad;
  }

  static const cc::DrawQuad& surface_quad_state(const cc::DrawQuad& quad) {
    return quad;
  }

  static const cc::DrawQuad& texture_quad_state(const cc::DrawQuad& quad) {
    return quad;
  }

  static const cc::DrawQuad& tile_quad_state(const cc::DrawQuad& quad) {
    return quad;
  }

  static const cc::DrawQuad& stream_video_quad_state(const cc::DrawQuad& quad) {
    return quad;
  }

  static const cc::DrawQuad& yuv_video_quad_state(const cc::DrawQuad& quad) {
    return quad;
  }

  static bool Read(cc::mojom::DrawQuadStateDataView data, cc::DrawQuad* out) {
    switch (data.tag()) {
      case cc::mojom::DrawQuadStateDataView::Tag::DEBUG_BORDER_QUAD_STATE:
        return data.ReadDebugBorderQuadState(out);
      case cc::mojom::DrawQuadStateDataView::Tag::RENDER_PASS_QUAD_STATE:
        return data.ReadRenderPassQuadState(out);
      case cc::mojom::DrawQuadStateDataView::Tag::SOLID_COLOR_QUAD_STATE:
        return data.ReadSolidColorQuadState(out);
      case cc::mojom::DrawQuadStateDataView::Tag::SURFACE_QUAD_STATE:
        return data.ReadSurfaceQuadState(out);
      case cc::mojom::DrawQuadStateDataView::Tag::TEXTURE_QUAD_STATE:
        return data.ReadTextureQuadState(out);
      case cc::mojom::DrawQuadStateDataView::Tag::TILE_QUAD_STATE:
        return data.ReadTileQuadState(out);
      case cc::mojom::DrawQuadStateDataView::Tag::STREAM_VIDEO_QUAD_STATE:
        return data.ReadStreamVideoQuadState(out);
      case cc::mojom::DrawQuadStateDataView::Tag::YUV_VIDEO_QUAD_STATE:
        return data.ReadYuvVideoQuadState(out);
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct StructTraits<cc::mojom::DebugBorderQuadStateDataView, cc::DrawQuad> {
  static uint32_t color(const cc::DrawQuad& input) {
    const cc::DebugBorderDrawQuad* quad =
        cc::DebugBorderDrawQuad::MaterialCast(&input);
    return quad->color;
  }

  static int32_t width(const cc::DrawQuad& input) {
    const cc::DebugBorderDrawQuad* quad =
        cc::DebugBorderDrawQuad::MaterialCast(&input);
    return quad->width;
  }

  static bool Read(cc::mojom::DebugBorderQuadStateDataView data,
                   cc::DrawQuad* out);
};

template <>
struct StructTraits<cc::mojom::RenderPassQuadStateDataView, cc::DrawQuad> {
  static const cc::RenderPassId& render_pass_id(const cc::DrawQuad& input) {
    const cc::RenderPassDrawQuad* quad =
        cc::RenderPassDrawQuad::MaterialCast(&input);
    return quad->render_pass_id;
  }

  static uint32_t mask_resource_id(const cc::DrawQuad& input) {
    const cc::RenderPassDrawQuad* quad =
        cc::RenderPassDrawQuad::MaterialCast(&input);
    return quad->mask_resource_id();
  }

  static const gfx::Vector2dF& mask_uv_scale(const cc::DrawQuad& input) {
    const cc::RenderPassDrawQuad* quad =
        cc::RenderPassDrawQuad::MaterialCast(&input);
    return quad->mask_uv_scale;
  }

  static const gfx::Size& mask_texture_size(const cc::DrawQuad& input) {
    const cc::RenderPassDrawQuad* quad =
        cc::RenderPassDrawQuad::MaterialCast(&input);
    return quad->mask_texture_size;
  }

  static const cc::FilterOperations& filters(const cc::DrawQuad& input) {
    const cc::RenderPassDrawQuad* quad =
        cc::RenderPassDrawQuad::MaterialCast(&input);
    return quad->filters;
  }

  static const gfx::Vector2dF& filters_scale(const cc::DrawQuad& input) {
    const cc::RenderPassDrawQuad* quad =
        cc::RenderPassDrawQuad::MaterialCast(&input);
    return quad->filters_scale;
  }

  static const gfx::PointF& filters_origin(const cc::DrawQuad& input) {
    const cc::RenderPassDrawQuad* quad =
        cc::RenderPassDrawQuad::MaterialCast(&input);
    return quad->filters_origin;
  }

  static const cc::FilterOperations& background_filters(
      const cc::DrawQuad& input) {
    const cc::RenderPassDrawQuad* quad =
        cc::RenderPassDrawQuad::MaterialCast(&input);
    return quad->background_filters;
  }

  static bool Read(cc::mojom::RenderPassQuadStateDataView data,
                   cc::DrawQuad* out);
};

template <>
struct StructTraits<cc::mojom::SolidColorQuadStateDataView, cc::DrawQuad> {
  static uint32_t color(const cc::DrawQuad& input) {
    const cc::SolidColorDrawQuad* quad =
        cc::SolidColorDrawQuad::MaterialCast(&input);
    return quad->color;
  }

  static bool force_anti_aliasing_off(const cc::DrawQuad& input) {
    const cc::SolidColorDrawQuad* quad =
        cc::SolidColorDrawQuad::MaterialCast(&input);
    return quad->force_anti_aliasing_off;
  }

  static bool Read(cc::mojom::SolidColorQuadStateDataView data,
                   cc::DrawQuad* out);
};

template <>
struct StructTraits<cc::mojom::StreamVideoQuadStateDataView, cc::DrawQuad> {
  static uint32_t resource_id(const cc::DrawQuad& input) {
    const cc::StreamVideoDrawQuad* quad =
        cc::StreamVideoDrawQuad::MaterialCast(&input);
    return quad->resources.ids[cc::StreamVideoDrawQuad::kResourceIdIndex];
  }

  static const gfx::Size& resource_size_in_pixels(const cc::DrawQuad& input) {
    const cc::StreamVideoDrawQuad* quad =
        cc::StreamVideoDrawQuad::MaterialCast(&input);
    return quad->overlay_resources
        .size_in_pixels[cc::StreamVideoDrawQuad::kResourceIdIndex];
  }

  static const gfx::Transform& matrix(const cc::DrawQuad& input) {
    const cc::StreamVideoDrawQuad* quad =
        cc::StreamVideoDrawQuad::MaterialCast(&input);
    return quad->matrix;
  }

  static bool Read(cc::mojom::StreamVideoQuadStateDataView data,
                   cc::DrawQuad* out);
};

template <>
struct StructTraits<cc::mojom::SurfaceQuadStateDataView, cc::DrawQuad> {
  static const cc::SurfaceId& surface(const cc::DrawQuad& input) {
    const cc::SurfaceDrawQuad* quad = cc::SurfaceDrawQuad::MaterialCast(&input);
    return quad->surface_id;
  }

  static bool Read(cc::mojom::SurfaceQuadStateDataView data, cc::DrawQuad* out);
};

template <>
struct StructTraits<cc::mojom::TextureQuadStateDataView, cc::DrawQuad> {
  static uint32_t resource_id(const cc::DrawQuad& input) {
    const cc::TextureDrawQuad* quad = cc::TextureDrawQuad::MaterialCast(&input);
    return quad->resource_id();
  }

  static bool premultiplied_alpha(const cc::DrawQuad& input) {
    const cc::TextureDrawQuad* quad = cc::TextureDrawQuad::MaterialCast(&input);
    return quad->premultiplied_alpha;
  }

  static const gfx::PointF& uv_top_left(const cc::DrawQuad& input) {
    const cc::TextureDrawQuad* quad = cc::TextureDrawQuad::MaterialCast(&input);
    return quad->uv_top_left;
  }

  static const gfx::PointF& uv_bottom_right(const cc::DrawQuad& input) {
    const cc::TextureDrawQuad* quad = cc::TextureDrawQuad::MaterialCast(&input);
    return quad->uv_bottom_right;
  }

  static uint32_t background_color(const cc::DrawQuad& input) {
    const cc::TextureDrawQuad* quad = cc::TextureDrawQuad::MaterialCast(&input);
    return quad->background_color;
  }

  static CArray<float> vertex_opacity(const cc::DrawQuad& input) {
    const cc::TextureDrawQuad* quad = cc::TextureDrawQuad::MaterialCast(&input);
    return {4, 4, const_cast<float*>(&quad->vertex_opacity[0])};
  }

  static bool y_flipped(const cc::DrawQuad& input) {
    const cc::TextureDrawQuad* quad = cc::TextureDrawQuad::MaterialCast(&input);
    return quad->y_flipped;
  }

  static bool nearest_neighbor(const cc::DrawQuad& input) {
    const cc::TextureDrawQuad* quad = cc::TextureDrawQuad::MaterialCast(&input);
    return quad->nearest_neighbor;
  }

  static bool secure_output_only(const cc::DrawQuad& input) {
    const cc::TextureDrawQuad* quad = cc::TextureDrawQuad::MaterialCast(&input);
    return quad->secure_output_only;
  }

  static bool Read(cc::mojom::TextureQuadStateDataView data, cc::DrawQuad* out);
};

template <>
struct StructTraits<cc::mojom::TileQuadStateDataView, cc::DrawQuad> {
  static const gfx::RectF& tex_coord_rect(const cc::DrawQuad& input) {
    const cc::TileDrawQuad* quad = cc::TileDrawQuad::MaterialCast(&input);
    return quad->tex_coord_rect;
  }

  static const gfx::Size& texture_size(const cc::DrawQuad& input) {
    const cc::TileDrawQuad* quad = cc::TileDrawQuad::MaterialCast(&input);
    return quad->texture_size;
  }

  static bool swizzle_contents(const cc::DrawQuad& input) {
    const cc::TileDrawQuad* quad = cc::TileDrawQuad::MaterialCast(&input);
    return quad->swizzle_contents;
  }

  static bool nearest_neighbor(const cc::DrawQuad& input) {
    const cc::TileDrawQuad* quad = cc::TileDrawQuad::MaterialCast(&input);
    return quad->nearest_neighbor;
  }

  static uint32_t resource_id(const cc::DrawQuad& input) {
    const cc::TileDrawQuad* quad = cc::TileDrawQuad::MaterialCast(&input);
    return quad->resource_id();
  }

  static bool Read(cc::mojom::TileQuadStateDataView data, cc::DrawQuad* out);
};

template <>
struct EnumTraits<cc::mojom::YUVColorSpace, cc::YUVVideoDrawQuad::ColorSpace> {
  static cc::mojom::YUVColorSpace ToMojom(
      cc::YUVVideoDrawQuad::ColorSpace color_space);
  static bool FromMojom(cc::mojom::YUVColorSpace input,
                        cc::YUVVideoDrawQuad::ColorSpace* out);
};

template <>
struct StructTraits<cc::mojom::YUVVideoQuadStateDataView, cc::DrawQuad> {
  static const gfx::RectF& ya_tex_coord_rect(const cc::DrawQuad& input) {
    const cc::YUVVideoDrawQuad* quad =
        cc::YUVVideoDrawQuad::MaterialCast(&input);
    return quad->ya_tex_coord_rect;
  }

  static const gfx::RectF& uv_tex_coord_rect(const cc::DrawQuad& input) {
    const cc::YUVVideoDrawQuad* quad =
        cc::YUVVideoDrawQuad::MaterialCast(&input);
    return quad->uv_tex_coord_rect;
  }

  static const gfx::Size& ya_tex_size(const cc::DrawQuad& input) {
    const cc::YUVVideoDrawQuad* quad =
        cc::YUVVideoDrawQuad::MaterialCast(&input);
    return quad->ya_tex_size;
  }

  static const gfx::Size& uv_tex_size(const cc::DrawQuad& input) {
    const cc::YUVVideoDrawQuad* quad =
        cc::YUVVideoDrawQuad::MaterialCast(&input);
    return quad->uv_tex_size;
  }

  static uint32_t y_plane_resource_id(const cc::DrawQuad& input) {
    const cc::YUVVideoDrawQuad* quad =
        cc::YUVVideoDrawQuad::MaterialCast(&input);
    return quad->y_plane_resource_id();
  }

  static uint32_t u_plane_resource_id(const cc::DrawQuad& input) {
    const cc::YUVVideoDrawQuad* quad =
        cc::YUVVideoDrawQuad::MaterialCast(&input);
    return quad->u_plane_resource_id();
  }

  static uint32_t v_plane_resource_id(const cc::DrawQuad& input) {
    const cc::YUVVideoDrawQuad* quad =
        cc::YUVVideoDrawQuad::MaterialCast(&input);
    return quad->v_plane_resource_id();
  }

  static uint32_t a_plane_resource_id(const cc::DrawQuad& input) {
    const cc::YUVVideoDrawQuad* quad =
        cc::YUVVideoDrawQuad::MaterialCast(&input);
    return quad->a_plane_resource_id();
  }

  static cc::YUVVideoDrawQuad::ColorSpace color_space(
      const cc::DrawQuad& input) {
    const cc::YUVVideoDrawQuad* quad =
        cc::YUVVideoDrawQuad::MaterialCast(&input);
    return quad->color_space;
  }

  static float resource_offset(const cc::DrawQuad& input) {
    const cc::YUVVideoDrawQuad* quad =
        cc::YUVVideoDrawQuad::MaterialCast(&input);
    return quad->resource_offset;
  }

  static float resource_multiplier(const cc::DrawQuad& input) {
    const cc::YUVVideoDrawQuad* quad =
        cc::YUVVideoDrawQuad::MaterialCast(&input);
    return quad->resource_multiplier;
  }

  static uint32_t bits_per_channel(const cc::DrawQuad& input) {
    const cc::YUVVideoDrawQuad* quad =
        cc::YUVVideoDrawQuad::MaterialCast(&input);
    return quad->bits_per_channel;
  }

  static bool Read(cc::mojom::YUVVideoQuadStateDataView data,
                   cc::DrawQuad* out);
};

struct DrawQuadWithSharedQuadState {
  const cc::DrawQuad* quad;
  const cc::SharedQuadState* shared_quad_state;
};

template <>
struct StructTraits<cc::mojom::DrawQuadDataView, DrawQuadWithSharedQuadState> {
  static const gfx::Rect& rect(const DrawQuadWithSharedQuadState& input) {
    return input.quad->rect;
  }

  static const gfx::Rect& opaque_rect(
      const DrawQuadWithSharedQuadState& input) {
    return input.quad->opaque_rect;
  }

  static const gfx::Rect& visible_rect(
      const DrawQuadWithSharedQuadState& input) {
    return input.quad->visible_rect;
  }

  static bool needs_blending(const DrawQuadWithSharedQuadState& input) {
    return input.quad->needs_blending;
  }

  static OptSharedQuadState sqs(const DrawQuadWithSharedQuadState& input) {
    return {input.shared_quad_state};
  }

  static const cc::DrawQuad& draw_quad_state(
      const DrawQuadWithSharedQuadState& input) {
    return *input.quad;
  }
};

// This StructTraits is only used for deserialization within RenderPasses.
template <>
struct StructTraits<cc::mojom::DrawQuadDataView, cc::DrawQuad> {
  static bool Read(cc::mojom::DrawQuadDataView data, cc::DrawQuad* out);
};

template <>
struct ArrayTraits<cc::QuadList> {
  using Element = DrawQuadWithSharedQuadState;
  struct ConstIterator {
    explicit ConstIterator(const cc::QuadList::ConstIterator& it)
        : it(it), last_shared_quad_state(nullptr) {}

    cc::QuadList::ConstIterator it;
    const cc::SharedQuadState* last_shared_quad_state;
  };

  static ConstIterator GetBegin(const cc::QuadList& input) {
    return ConstIterator(input.begin());
  }

  static void AdvanceIterator(ConstIterator& iterator) {
    iterator.last_shared_quad_state = (*iterator.it)->shared_quad_state;
    ++iterator.it;
  }

  static Element GetValue(ConstIterator& iterator) {
    DrawQuadWithSharedQuadState dq = {*iterator.it, nullptr};
    // Only serialize the SharedQuadState if we haven't seen it before and
    // therefore have not already serialized it.
    const cc::SharedQuadState* current_sqs = (*iterator.it)->shared_quad_state;
    if (current_sqs != iterator.last_shared_quad_state)
      dq.shared_quad_state = current_sqs;
    return dq;
  }

  static size_t GetSize(const cc::QuadList& input) { return input.size(); }
};

}  // namespace mojo

#endif  // CC_IPC_QUADS_STRUCT_TRAITS_H_
