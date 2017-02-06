// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/ipc/quads_struct_traits.h"
#include "ui/gfx/mojo/transform_struct_traits.h"

namespace mojo {

namespace {

bool ReadDrawQuad(cc::mojom::DrawQuadDataView data, cc::DrawQuad* quad) {
  cc::DrawQuad::Material material;
  if (!data.ReadMaterial(&material) || material != quad->material) {
    return false;
  }
  if (!data.ReadRect(&quad->rect) || !data.ReadOpaqueRect(&quad->opaque_rect) ||
      !data.ReadVisibleRect(&quad->visible_rect)) {
    return false;
  }
  quad->needs_blending = data.needs_blending();
  return true;
}

bool AllocateAndConstruct(cc::DrawQuad::Material material, cc::QuadList* list) {
  cc::DrawQuad* quad = nullptr;
  switch (material) {
    case cc::DrawQuad::INVALID:
      break;
    case cc::DrawQuad::DEBUG_BORDER:
      quad = list->AllocateAndConstruct<cc::DebugBorderDrawQuad>();
      break;
    case cc::DrawQuad::PICTURE_CONTENT:
      quad = list->AllocateAndConstruct<cc::PictureDrawQuad>();
      break;
    case cc::DrawQuad::RENDER_PASS:
      quad = list->AllocateAndConstruct<cc::RenderPassDrawQuad>();
      break;
    case cc::DrawQuad::SOLID_COLOR:
      quad = list->AllocateAndConstruct<cc::SolidColorDrawQuad>();
      break;
    case cc::DrawQuad::STREAM_VIDEO_CONTENT:
      quad = list->AllocateAndConstruct<cc::StreamVideoDrawQuad>();
      break;
    case cc::DrawQuad::SURFACE_CONTENT:
      quad = list->AllocateAndConstruct<cc::SurfaceDrawQuad>();
      break;
    case cc::DrawQuad::TEXTURE_CONTENT:
      quad = list->AllocateAndConstruct<cc::TextureDrawQuad>();
      break;
    case cc::DrawQuad::TILED_CONTENT:
      quad = list->AllocateAndConstruct<cc::TileDrawQuad>();
      break;
    case cc::DrawQuad::YUV_VIDEO_CONTENT:
      quad = list->AllocateAndConstruct<cc::YUVVideoDrawQuad>();
      break;
  }
  if (quad)
    quad->material = material;
  return quad != nullptr;
}

}  // namespace

// static
cc::mojom::Material
EnumTraits<cc::mojom::Material, cc::DrawQuad::Material>::ToMojom(
    cc::DrawQuad::Material material) {
  switch (material) {
    case cc::DrawQuad::INVALID:
      return cc::mojom::Material::INVALID;
    case cc::DrawQuad::DEBUG_BORDER:
      return cc::mojom::Material::DEBUG_BORDER;
    case cc::DrawQuad::PICTURE_CONTENT:
      return cc::mojom::Material::PICTURE_CONTENT;
    case cc::DrawQuad::RENDER_PASS:
      return cc::mojom::Material::RENDER_PASS;
    case cc::DrawQuad::SOLID_COLOR:
      return cc::mojom::Material::SOLID_COLOR;
    case cc::DrawQuad::STREAM_VIDEO_CONTENT:
      return cc::mojom::Material::STREAM_VIDEO_CONTENT;
    case cc::DrawQuad::SURFACE_CONTENT:
      return cc::mojom::Material::SURFACE_CONTENT;
    case cc::DrawQuad::TEXTURE_CONTENT:
      return cc::mojom::Material::TEXTURE_CONTENT;
    case cc::DrawQuad::TILED_CONTENT:
      return cc::mojom::Material::TILED_CONTENT;
    case cc::DrawQuad::YUV_VIDEO_CONTENT:
      return cc::mojom::Material::YUV_VIDEO_CONTENT;
  }
  return cc::mojom::Material::INVALID;
}

// static
bool EnumTraits<cc::mojom::Material, cc::DrawQuad::Material>::FromMojom(
    cc::mojom::Material input,
    cc::DrawQuad::Material* out) {
  switch (input) {
    case cc::mojom::Material::INVALID:
      *out = cc::DrawQuad::INVALID;
      return true;
    case cc::mojom::Material::DEBUG_BORDER:
      *out = cc::DrawQuad::DEBUG_BORDER;
      return true;
    case cc::mojom::Material::PICTURE_CONTENT:
      *out = cc::DrawQuad::PICTURE_CONTENT;
      return true;
    case cc::mojom::Material::RENDER_PASS:
      *out = cc::DrawQuad::RENDER_PASS;
      return true;
    case cc::mojom::Material::SOLID_COLOR:
      *out = cc::DrawQuad::SOLID_COLOR;
      return true;
    case cc::mojom::Material::STREAM_VIDEO_CONTENT:
      *out = cc::DrawQuad::STREAM_VIDEO_CONTENT;
      return true;
    case cc::mojom::Material::SURFACE_CONTENT:
      *out = cc::DrawQuad::SURFACE_CONTENT;
      return true;
    case cc::mojom::Material::TEXTURE_CONTENT:
      *out = cc::DrawQuad::TEXTURE_CONTENT;
      return true;
    case cc::mojom::Material::TILED_CONTENT:
      *out = cc::DrawQuad::TILED_CONTENT;
      return true;
    case cc::mojom::Material::YUV_VIDEO_CONTENT:
      *out = cc::DrawQuad::YUV_VIDEO_CONTENT;
      return true;
  }
  return false;
}

// static
bool StructTraits<cc::mojom::DebugBorderQuadState, cc::DrawQuad>::Read(
    cc::mojom::DebugBorderQuadStateDataView data,
    cc::DrawQuad* out) {
  cc::DebugBorderDrawQuad* quad = static_cast<cc::DebugBorderDrawQuad*>(out);
  quad->color = data.color();
  quad->width = data.width();
  return true;
}

// static
bool StructTraits<cc::mojom::RenderPassQuadState, cc::DrawQuad>::Read(
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
         data.ReadBackgroundFilters(&quad->background_filters);
}

// static
bool StructTraits<cc::mojom::SolidColorQuadState, cc::DrawQuad>::Read(
    cc::mojom::SolidColorQuadStateDataView data,
    cc::DrawQuad* out) {
  cc::SolidColorDrawQuad* quad = static_cast<cc::SolidColorDrawQuad*>(out);
  quad->force_anti_aliasing_off = data.force_anti_aliasing_off();
  quad->color = data.color();
  return true;
}

// static
bool StructTraits<cc::mojom::StreamVideoQuadState, cc::DrawQuad>::Read(
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
bool StructTraits<cc::mojom::SurfaceQuadState, cc::DrawQuad>::Read(
    cc::mojom::SurfaceQuadStateDataView data,
    cc::DrawQuad* out) {
  cc::SurfaceDrawQuad* quad = static_cast<cc::SurfaceDrawQuad*>(out);
  return data.ReadSurface(&quad->surface_id);
}

// static
bool StructTraits<cc::mojom::TextureQuadState, cc::DrawQuad>::Read(
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
bool StructTraits<cc::mojom::TileQuadState, cc::DrawQuad>::Read(
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
bool StructTraits<cc::mojom::YUVVideoQuadState, cc::DrawQuad>::Read(
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
  return true;
}

// static
bool StructTraits<cc::mojom::DrawQuad, cc::DrawQuad>::Read(
    cc::mojom::DrawQuadDataView data,
    cc::DrawQuad* out) {
  if (!ReadDrawQuad(data, out))
    return false;
  switch (data.material()) {
    case cc::mojom::Material::INVALID:
      break;
    case cc::mojom::Material::DEBUG_BORDER:
      return data.ReadDebugBorderQuadState(out);
    case cc::mojom::Material::PICTURE_CONTENT:
      // TODO(fsamuel): Implement PictureDrawQuad
      // serialization/deserialization.
      break;
    case cc::mojom::Material::RENDER_PASS:
      return data.ReadRenderPassQuadState(out);
    case cc::mojom::Material::SOLID_COLOR:
      return data.ReadSolidColorQuadState(out);
    case cc::mojom::Material::STREAM_VIDEO_CONTENT:
      return data.ReadStreamVideoQuadState(out);
    case cc::mojom::Material::SURFACE_CONTENT:
      return data.ReadSurfaceQuadState(out);
    case cc::mojom::Material::TEXTURE_CONTENT:
      return data.ReadTextureQuadState(out);
    case cc::mojom::Material::TILED_CONTENT:
      return data.ReadTileQuadState(out);
    case cc::mojom::Material::YUV_VIDEO_CONTENT:
      return data.ReadYuvVideoQuadState(out);
  }
  NOTREACHED();
  return false;
}

// static
void* StructTraits<cc::mojom::QuadList, cc::QuadList>::SetUpContext(
    const cc::QuadList& quad_list) {
  mojo::Array<cc::DrawQuad::Material>* materials =
      new mojo::Array<cc::DrawQuad::Material>(quad_list.size());
  for (auto it = quad_list.begin(); it != quad_list.end(); ++it)
    (*materials)[it.index()] = it->material;
  return materials;
}

// static
void StructTraits<cc::mojom::QuadList, cc::QuadList>::TearDownContext(
    const cc::QuadList& quad_list,
    void* context) {
  // static_cast to ensure the destructor is called.
  delete static_cast<mojo::Array<cc::DrawQuad::Material>*>(context);
}

// static
const mojo::Array<cc::DrawQuad::Material>&
StructTraits<cc::mojom::QuadList, cc::QuadList>::quad_types(
    const cc::QuadList& quad_list,
    void* context) {
  return *static_cast<mojo::Array<cc::DrawQuad::Material>*>(context);
}

// static
bool StructTraits<cc::mojom::QuadList, cc::QuadList>::Read(
    cc::mojom::QuadListDataView data,
    cc::QuadList* out) {
  // TODO(fsamuel): Once we have ArrayTraits DataViews we can delete
  // this field. This field exists so we can pre-allocate DrawQuads
  // in the QuadList according to their material.
  mojo::Array<cc::DrawQuad::Material> materials;
  if (!data.ReadQuadTypes(&materials))
    return false;
  for (size_t i = 0; i < materials.size(); ++i) {
    if (!AllocateAndConstruct(materials[i], out))
      return false;
  }
  // The materials array and the quads array are expected to be the same size.
  // If they are not, then deserialization will fail.
  QuadListArray quad_list_array = {materials.size(), out};
  return data.ReadQuads(&quad_list_array);
}

}  // namespace mojo
