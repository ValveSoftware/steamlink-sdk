// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_QUADS_STRUCT_TRAITS_H_
#define CC_IPC_QUADS_STRUCT_TRAITS_H_

#include "cc/ipc/filter_operation_struct_traits.h"
#include "cc/ipc/filter_operations_struct_traits.h"
#include "cc/ipc/quads.mojom.h"
#include "cc/ipc/render_pass_id_struct_traits.h"
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

template <>
struct EnumTraits<cc::mojom::Material, cc::DrawQuad::Material> {
  static cc::mojom::Material ToMojom(cc::DrawQuad::Material material);
  static bool FromMojom(cc::mojom::Material input, cc::DrawQuad::Material* out);
};

template <>
struct StructTraits<cc::mojom::DebugBorderQuadState, cc::DrawQuad> {
  static bool IsNull(const cc::DrawQuad& input) {
    return input.material != cc::DrawQuad::DEBUG_BORDER;
  }

  static void SetToNull(cc::DrawQuad* output) {
    // There is nothing to deserialize here if the DrawQuad is not a
    // DebugBorderDrawQuad. If it is, then this should not be called.
    DCHECK_NE(cc::DrawQuad::DEBUG_BORDER, output->material);
  }

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
struct StructTraits<cc::mojom::RenderPassQuadState, cc::DrawQuad> {
  static bool IsNull(const cc::DrawQuad& quad) {
    return quad.material != cc::DrawQuad::RENDER_PASS;
  }

  static void SetToNull(cc::DrawQuad* output) {
    // There is nothing to deserialize here if the DrawQuad is not a
    // RenderPassDrawQuad. If it is, then this should not be called.
    DCHECK_NE(cc::DrawQuad::RENDER_PASS, output->material);
  }

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
struct StructTraits<cc::mojom::SolidColorQuadState, cc::DrawQuad> {
  static bool IsNull(const cc::DrawQuad& input) {
    return input.material != cc::DrawQuad::SOLID_COLOR;
  }

  static void SetToNull(cc::DrawQuad* output) {
    // There is nothing to deserialize here if the DrawQuad is not a
    // SolidColorDrawQuad. If it is, then this should not be called.
    DCHECK_NE(cc::DrawQuad::SOLID_COLOR, output->material);
  }

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
struct StructTraits<cc::mojom::StreamVideoQuadState, cc::DrawQuad> {
  static bool IsNull(const cc::DrawQuad& input) {
    return input.material != cc::DrawQuad::STREAM_VIDEO_CONTENT;
  }

  static void SetToNull(cc::DrawQuad* output) {
    // There is nothing to deserialize here if the DrawQuad is not a
    // StreamVideoDrawQuad. If it is, then this should not be called.
    DCHECK_NE(cc::DrawQuad::STREAM_VIDEO_CONTENT, output->material);
  }

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
struct StructTraits<cc::mojom::SurfaceQuadState, cc::DrawQuad> {
  static bool IsNull(const cc::DrawQuad& input) {
    return input.material != cc::DrawQuad::SURFACE_CONTENT;
  }

  static void SetToNull(cc::DrawQuad* output) {
    // There is nothing to deserialize here if the DrawQuad is not a
    // SurfaceDrawQuad. If it is, then this should not be called.
    DCHECK_NE(cc::DrawQuad::SURFACE_CONTENT, output->material);
  }

  static const cc::SurfaceId& surface(const cc::DrawQuad& input) {
    const cc::SurfaceDrawQuad* quad = cc::SurfaceDrawQuad::MaterialCast(&input);
    return quad->surface_id;
  }

  static bool Read(cc::mojom::SurfaceQuadStateDataView data, cc::DrawQuad* out);
};

template <>
struct StructTraits<cc::mojom::TextureQuadState, cc::DrawQuad> {
  static bool IsNull(const cc::DrawQuad& input) {
    return input.material != cc::DrawQuad::TEXTURE_CONTENT;
  }

  static void SetToNull(cc::DrawQuad* output) {
    // There is nothing to deserialize here if the DrawQuad is not a
    // TextureContentDrawQuad. If it is, then this should not be called.
    DCHECK_NE(cc::DrawQuad::TEXTURE_CONTENT, output->material);
  }

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
struct StructTraits<cc::mojom::TileQuadState, cc::DrawQuad> {
  static bool IsNull(const cc::DrawQuad& input) {
    return input.material != cc::DrawQuad::TILED_CONTENT;
  }

  static void SetToNull(cc::DrawQuad* output) {
    // There is nothing to deserialize here if the DrawQuad is not a
    // TileDrawQuad. If it is, then this should not be called.
    DCHECK_NE(cc::DrawQuad::TILED_CONTENT, output->material);
  }

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
struct StructTraits<cc::mojom::YUVVideoQuadState, cc::DrawQuad> {
  static bool IsNull(const cc::DrawQuad& input) {
    return input.material != cc::DrawQuad::YUV_VIDEO_CONTENT;
  }

  static void SetToNull(cc::DrawQuad* output) {
    // There is nothing to deserialize here if the DrawQuad is not a
    // YUVVideoDrawQuad. If it is, then this should not be called.
    DCHECK_NE(cc::DrawQuad::YUV_VIDEO_CONTENT, output->material);
  }

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

  static bool Read(cc::mojom::YUVVideoQuadStateDataView data,
                   cc::DrawQuad* out);
};

template <>
struct StructTraits<cc::mojom::DrawQuad, cc::DrawQuad> {
  static cc::DrawQuad::Material material(const cc::DrawQuad& quad) {
    return quad.material;
  }

  static const gfx::Rect& rect(const cc::DrawQuad& quad) { return quad.rect; }

  static const gfx::Rect& opaque_rect(const cc::DrawQuad& quad) {
    return quad.opaque_rect;
  }

  static const gfx::Rect& visible_rect(const cc::DrawQuad& quad) {
    return quad.visible_rect;
  }

  static bool needs_blending(const cc::DrawQuad& quad) {
    return quad.needs_blending;
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

  static const cc::DrawQuad& yuv_video_quad_state(const cc::DrawQuad& data) {
    return data;
  }

  static bool Read(cc::mojom::DrawQuadDataView data, cc::DrawQuad* out);
};

struct QuadListArray {
  // This is the expected size of the array.
  size_t size;
  cc::QuadList* list;
};

template <>
struct ArrayTraits<QuadListArray> {
  using Element = cc::DrawQuad;
  using Iterator = cc::QuadList::Iterator;
  using ConstIterator = cc::QuadList::ConstIterator;

  static ConstIterator GetBegin(const QuadListArray& input) {
    return input.list->begin();
  }
  static Iterator GetBegin(QuadListArray& input) { return input.list->begin(); }
  static void AdvanceIterator(ConstIterator& iterator) { ++iterator; }
  static void AdvanceIterator(Iterator& iterator) { ++iterator; }
  static const Element& GetValue(ConstIterator& iterator) { return **iterator; }
  static Element& GetValue(Iterator& iterator) { return **iterator; }
  static size_t GetSize(const QuadListArray& input) {
    return input.list->size();
  }
  static bool Resize(QuadListArray& input, size_t size) {
    return input.size == size;
  }
};

template <>
struct StructTraits<cc::mojom::QuadList, cc::QuadList> {
  static void* SetUpContext(const cc::QuadList& quad_list);
  static void TearDownContext(const cc::QuadList& quad_list, void* context);
  static const mojo::Array<cc::DrawQuad::Material>& quad_types(
      const cc::QuadList& quad_list,
      void* context);
  static QuadListArray quads(const cc::QuadList& quad_list) {
    return {quad_list.size(), const_cast<cc::QuadList*>(&quad_list)};
  }
  static bool Read(cc::mojom::QuadListDataView data, cc::QuadList* out);
};

}  // namespace mojo

#endif  // CC_IPC_QUADS_STRUCT_TRAITS_H_
