// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/ipc/quads_struct_traits.h"
#include "ui/gfx/mojo/transform_struct_traits.h"

namespace mojo {

cc::DrawQuad* AllocateAndConstruct(
    cc::mojom::DrawQuadStateDataView::Tag material,
    cc::QuadList* list) {
  cc::DrawQuad* quad = nullptr;
  switch (material) {
    case cc::mojom::DrawQuadStateDataView::Tag::DEBUG_BORDER_QUAD_STATE:
      quad = list->AllocateAndConstruct<cc::DebugBorderDrawQuad>();
      quad->material = cc::DrawQuad::DEBUG_BORDER;
      return quad;
    case cc::mojom::DrawQuadStateDataView::Tag::RENDER_PASS_QUAD_STATE:
      quad = list->AllocateAndConstruct<cc::RenderPassDrawQuad>();
      quad->material = cc::DrawQuad::RENDER_PASS;
      return quad;
    case cc::mojom::DrawQuadStateDataView::Tag::SOLID_COLOR_QUAD_STATE:
      quad = list->AllocateAndConstruct<cc::SolidColorDrawQuad>();
      quad->material = cc::DrawQuad::SOLID_COLOR;
      return quad;
    case cc::mojom::DrawQuadStateDataView::Tag::STREAM_VIDEO_QUAD_STATE:
      quad = list->AllocateAndConstruct<cc::StreamVideoDrawQuad>();
      quad->material = cc::DrawQuad::STREAM_VIDEO_CONTENT;
      return quad;
    case cc::mojom::DrawQuadStateDataView::Tag::SURFACE_QUAD_STATE:
      quad = list->AllocateAndConstruct<cc::SurfaceDrawQuad>();
      quad->material = cc::DrawQuad::SURFACE_CONTENT;
      return quad;
    case cc::mojom::DrawQuadStateDataView::Tag::TEXTURE_QUAD_STATE:
      quad = list->AllocateAndConstruct<cc::TextureDrawQuad>();
      quad->material = cc::DrawQuad::TEXTURE_CONTENT;
      return quad;
    case cc::mojom::DrawQuadStateDataView::Tag::TILE_QUAD_STATE:
      quad = list->AllocateAndConstruct<cc::TileDrawQuad>();
      quad->material = cc::DrawQuad::TILED_CONTENT;
      return quad;
    case cc::mojom::DrawQuadStateDataView::Tag::YUV_VIDEO_QUAD_STATE:
      quad = list->AllocateAndConstruct<cc::YUVVideoDrawQuad>();
      quad->material = cc::DrawQuad::YUV_VIDEO_CONTENT;
      return quad;
  }
  NOTREACHED();
  return nullptr;
}

// static
bool StructTraits<cc::mojom::DebugBorderQuadStateDataView, cc::DrawQuad>::Read(
    cc::mojom::DebugBorderQuadStateDataView data,
    cc::DrawQuad* out) {
  cc::DebugBorderDrawQuad* quad = static_cast<cc::DebugBorderDrawQuad*>(out);
  quad->color = data.color();
  quad->width = data.width();
  return true;
}

// static
bool StructTraits<cc::mojom::RenderPassQuadStateDataView, cc::DrawQuad>::Read(
    cc::mojom::RenderPassQuadStateDataView data,
    cc::DrawQuad* out) {
  cc::RenderPassDrawQuad* quad = static_cast<cc::RenderPassDrawQuad*>(out);
  quad->resources.ids[cc::RenderPassDrawQuad::kMaskResourceIdIndex] =
      data.mask_resource_id();
  quad->resources.count = data.mask_resource_id() ? 1 : 0;
  return data.ReadRenderPassId(&quad->render_pass_id) &&
         data.ReadMaskUvScale(&quad->mask_uv_scale) &&
         data.ReadMaskTextureSize(&quad->mask_texture_size) &&
         data.ReadFilters(&quad->filters) &&
         data.ReadFiltersScale(&quad->filters_scale) &&
         data.ReadFiltersOrigin(&quad->filters_origin) &&
         data.ReadBackgroundFilters(&quad->background_filters);
}

// static
bool StructTraits<cc::mojom::SolidColorQuadStateDataView, cc::DrawQuad>::Read(
    cc::mojom::SolidColorQuadStateDataView data,
    cc::DrawQuad* out) {
  cc::SolidColorDrawQuad* quad = static_cast<cc::SolidColorDrawQuad*>(out);
  quad->force_anti_aliasing_off = data.force_anti_aliasing_off();
  quad->color = data.color();
  return true;
}

// static
bool StructTraits<cc::mojom::StreamVideoQuadStateDataView, cc::DrawQuad>::Read(
    cc::mojom::StreamVideoQuadStateDataView data,
    cc::DrawQuad* out) {
  cc::StreamVideoDrawQuad* quad = static_cast<cc::StreamVideoDrawQuad*>(out);
  quad->resources.ids[cc::StreamVideoDrawQuad::kResourceIdIndex] =
      data.resource_id();
  quad->resources.count = 1;
  return data.ReadResourceSizeInPixels(
             &quad->overlay_resources
                  .size_in_pixels[cc::StreamVideoDrawQuad::kResourceIdIndex]) &&
         data.ReadMatrix(&quad->matrix);
}

// static
bool StructTraits<cc::mojom::SurfaceQuadStateDataView, cc::DrawQuad>::Read(
    cc::mojom::SurfaceQuadStateDataView data,
    cc::DrawQuad* out) {
  cc::SurfaceDrawQuad* quad = static_cast<cc::SurfaceDrawQuad*>(out);
  return data.ReadSurface(&quad->surface_id);
}

// static
bool StructTraits<cc::mojom::TextureQuadStateDataView, cc::DrawQuad>::Read(
    cc::mojom::TextureQuadStateDataView data,
    cc::DrawQuad* out) {
  cc::TextureDrawQuad* quad = static_cast<cc::TextureDrawQuad*>(out);
  quad->resources.ids[cc::TextureDrawQuad::kResourceIdIndex] =
      data.resource_id();
  quad->resources.count = 1;
  quad->premultiplied_alpha = data.premultiplied_alpha();
  if (!data.ReadUvTopLeft(&quad->uv_top_left) ||
      !data.ReadUvBottomRight(&quad->uv_bottom_right)) {
    return false;
  }
  quad->background_color = data.background_color();
  CArray<float> vertex_opacity_array = {4, 4, &quad->vertex_opacity[0]};
  if (!data.ReadVertexOpacity(&vertex_opacity_array))
    return false;

  quad->y_flipped = data.y_flipped();
  quad->nearest_neighbor = data.nearest_neighbor();
  quad->secure_output_only = data.secure_output_only();
  return true;
}

// static
bool StructTraits<cc::mojom::TileQuadStateDataView, cc::DrawQuad>::Read(
    cc::mojom::TileQuadStateDataView data,
    cc::DrawQuad* out) {
  cc::TileDrawQuad* quad = static_cast<cc::TileDrawQuad*>(out);
  if (!data.ReadTexCoordRect(&quad->tex_coord_rect) ||
      !data.ReadTextureSize(&quad->texture_size)) {
    return false;
  }

  quad->swizzle_contents = data.swizzle_contents();
  quad->nearest_neighbor = data.nearest_neighbor();
  quad->resources.ids[cc::TileDrawQuad::kResourceIdIndex] = data.resource_id();
  quad->resources.count = 1;
  return true;
}

cc::mojom::YUVColorSpace
EnumTraits<cc::mojom::YUVColorSpace, cc::YUVVideoDrawQuad::ColorSpace>::ToMojom(
    cc::YUVVideoDrawQuad::ColorSpace color_space) {
  switch (color_space) {
    case cc::YUVVideoDrawQuad::REC_601:
      return cc::mojom::YUVColorSpace::REC_601;
    case cc::YUVVideoDrawQuad::REC_709:
      return cc::mojom::YUVColorSpace::REC_709;
    case cc::YUVVideoDrawQuad::JPEG:
      return cc::mojom::YUVColorSpace::JPEG;
  }
  NOTREACHED();
  return cc::mojom::YUVColorSpace::JPEG;
}

// static
bool EnumTraits<cc::mojom::YUVColorSpace, cc::YUVVideoDrawQuad::ColorSpace>::
    FromMojom(cc::mojom::YUVColorSpace input,
              cc::YUVVideoDrawQuad::ColorSpace* out) {
  switch (input) {
    case cc::mojom::YUVColorSpace::REC_601:
      *out = cc::YUVVideoDrawQuad::REC_601;
      return true;
    case cc::mojom::YUVColorSpace::REC_709:
      *out = cc::YUVVideoDrawQuad::REC_709;
      return true;
    case cc::mojom::YUVColorSpace::JPEG:
      *out = cc::YUVVideoDrawQuad::JPEG;
      return true;
  }
  return false;
}

// static
bool StructTraits<cc::mojom::YUVVideoQuadStateDataView, cc::DrawQuad>::Read(
    cc::mojom::YUVVideoQuadStateDataView data,
    cc::DrawQuad* out) {
  cc::YUVVideoDrawQuad* quad = static_cast<cc::YUVVideoDrawQuad*>(out);
  if (!data.ReadYaTexCoordRect(&quad->ya_tex_coord_rect) ||
      !data.ReadUvTexCoordRect(&quad->uv_tex_coord_rect) ||
      !data.ReadYaTexSize(&quad->ya_tex_size) ||
      !data.ReadUvTexSize(&quad->uv_tex_size)) {
    return false;
  }
  quad->resources.ids[cc::YUVVideoDrawQuad::kYPlaneResourceIdIndex] =
      data.y_plane_resource_id();
  quad->resources.ids[cc::YUVVideoDrawQuad::kUPlaneResourceIdIndex] =
      data.u_plane_resource_id();
  quad->resources.ids[cc::YUVVideoDrawQuad::kVPlaneResourceIdIndex] =
      data.v_plane_resource_id();
  quad->resources.ids[cc::YUVVideoDrawQuad::kAPlaneResourceIdIndex] =
      data.a_plane_resource_id();
  static_assert(cc::YUVVideoDrawQuad::kAPlaneResourceIdIndex ==
                    cc::DrawQuad::Resources::kMaxResourceIdCount - 1,
                "The A plane resource should be the last resource ID.");
  quad->resources.count = data.a_plane_resource_id() ? 4 : 3;

  if (!data.ReadColorSpace(&quad->color_space))
    return false;
  quad->resource_offset = data.resource_offset();
  quad->resource_multiplier = data.resource_multiplier();
  quad->bits_per_channel = data.bits_per_channel();
  if (quad->bits_per_channel < cc::YUVVideoDrawQuad::kMinBitsPerChannel ||
      quad->bits_per_channel > cc::YUVVideoDrawQuad::kMaxBitsPerChannel) {
    return false;
  }
  return true;
}

// static
bool StructTraits<cc::mojom::DrawQuadDataView, cc::DrawQuad>::Read(
    cc::mojom::DrawQuadDataView data,
    cc::DrawQuad* out) {
  if (!data.ReadRect(&out->rect) || !data.ReadOpaqueRect(&out->opaque_rect) ||
      !data.ReadVisibleRect(&out->visible_rect)) {
    return false;
  }
  out->needs_blending = data.needs_blending();
  return data.ReadDrawQuadState(out);
}

}  // namespace mojo
