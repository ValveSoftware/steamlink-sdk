// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <string.h>
#include <algorithm>
#include <utility>

#include "base/macros.h"
#include "build/build_config.h"
#include "cc/ipc/cc_param_traits.h"
#include "cc/output/compositor_frame.h"
#include "cc/quads/picture_draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

using cc::DelegatedFrameData;
using cc::DebugBorderDrawQuad;
using cc::DrawQuad;
using cc::FilterOperation;
using cc::FilterOperations;
using cc::PictureDrawQuad;
using cc::RenderPass;
using cc::RenderPassId;
using cc::RenderPassDrawQuad;
using cc::ResourceId;
using cc::ResourceProvider;
using cc::SharedQuadState;
using cc::SolidColorDrawQuad;
using cc::SurfaceDrawQuad;
using cc::TextureDrawQuad;
using cc::TileDrawQuad;
using cc::TransferableResource;
using cc::StreamVideoDrawQuad;
using cc::YUVVideoDrawQuad;
using gfx::Transform;

namespace content {
namespace {

class CCParamTraitsTest : public testing::Test {
 protected:
  void Compare(const RenderPass* a, const RenderPass* b) {
    EXPECT_EQ(a->id, b->id);
    EXPECT_EQ(a->output_rect.ToString(), b->output_rect.ToString());
    EXPECT_EQ(a->damage_rect.ToString(), b->damage_rect.ToString());
    EXPECT_EQ(a->transform_to_root_target, b->transform_to_root_target);
    EXPECT_EQ(a->has_transparent_background, b->has_transparent_background);
  }

  void Compare(const SharedQuadState* a, const SharedQuadState* b) {
    EXPECT_EQ(a->quad_to_target_transform, b->quad_to_target_transform);
    EXPECT_EQ(a->quad_layer_bounds, b->quad_layer_bounds);
    EXPECT_EQ(a->visible_quad_layer_rect, b->visible_quad_layer_rect);
    EXPECT_EQ(a->clip_rect, b->clip_rect);
    EXPECT_EQ(a->is_clipped, b->is_clipped);
    EXPECT_EQ(a->opacity, b->opacity);
    EXPECT_EQ(a->blend_mode, b->blend_mode);
    EXPECT_EQ(a->sorting_context_id, b->sorting_context_id);
  }

  void Compare(const DrawQuad* a, const DrawQuad* b) {
    ASSERT_NE(DrawQuad::INVALID, a->material);
    ASSERT_EQ(a->material, b->material);
    EXPECT_EQ(a->rect.ToString(), b->rect.ToString());
    EXPECT_EQ(a->opaque_rect.ToString(), b->opaque_rect.ToString());
    EXPECT_EQ(a->visible_rect.ToString(), b->visible_rect.ToString());
    EXPECT_EQ(a->needs_blending, b->needs_blending);

    Compare(a->shared_quad_state, b->shared_quad_state);

    switch (a->material) {
      case DrawQuad::DEBUG_BORDER:
        Compare(DebugBorderDrawQuad::MaterialCast(a),
                DebugBorderDrawQuad::MaterialCast(b));
        break;
      case DrawQuad::PICTURE_CONTENT:
        Compare(PictureDrawQuad::MaterialCast(a),
                PictureDrawQuad::MaterialCast(b));
        break;
      case DrawQuad::RENDER_PASS:
        Compare(RenderPassDrawQuad::MaterialCast(a),
                RenderPassDrawQuad::MaterialCast(b));
        break;
      case DrawQuad::TEXTURE_CONTENT:
        Compare(TextureDrawQuad::MaterialCast(a),
                TextureDrawQuad::MaterialCast(b));
        break;
      case DrawQuad::TILED_CONTENT:
        Compare(TileDrawQuad::MaterialCast(a), TileDrawQuad::MaterialCast(b));
        break;
      case DrawQuad::SOLID_COLOR:
        Compare(SolidColorDrawQuad::MaterialCast(a),
                SolidColorDrawQuad::MaterialCast(b));
        break;
      case DrawQuad::STREAM_VIDEO_CONTENT:
        Compare(StreamVideoDrawQuad::MaterialCast(a),
                StreamVideoDrawQuad::MaterialCast(b));
        break;
      case DrawQuad::SURFACE_CONTENT:
        Compare(SurfaceDrawQuad::MaterialCast(a),
                SurfaceDrawQuad::MaterialCast(b));
        break;
      case DrawQuad::YUV_VIDEO_CONTENT:
        Compare(YUVVideoDrawQuad::MaterialCast(a),
                YUVVideoDrawQuad::MaterialCast(b));
        break;
      case DrawQuad::INVALID:
        break;
    }
  }

  void Compare(const DebugBorderDrawQuad* a, const DebugBorderDrawQuad* b) {
    EXPECT_EQ(a->color, b->color);
    EXPECT_EQ(a->width, b->width);
  }

  void Compare(const RenderPassDrawQuad* a, const RenderPassDrawQuad* b) {
    EXPECT_EQ(a->render_pass_id, b->render_pass_id);
    EXPECT_EQ(a->mask_resource_id(), b->mask_resource_id());
    EXPECT_EQ(a->mask_uv_scale.ToString(), b->mask_uv_scale.ToString());
    EXPECT_EQ(a->mask_texture_size.ToString(), b->mask_texture_size.ToString());
    EXPECT_EQ(a->filters.size(), b->filters.size());
    for (size_t i = 0; i < a->filters.size(); ++i) {
      if (a->filters.at(i).type() != cc::FilterOperation::REFERENCE) {
        EXPECT_EQ(a->filters.at(i), b->filters.at(i));
      } else {
        EXPECT_EQ(b->filters.at(i).type(), cc::FilterOperation::REFERENCE);
        EXPECT_EQ(a->filters.at(i).image_filter()->countInputs(),
                  b->filters.at(i).image_filter()->countInputs());
      }
    }
    EXPECT_EQ(a->filters_scale, b->filters_scale);
    EXPECT_EQ(a->background_filters, b->background_filters);
  }

  void Compare(const SolidColorDrawQuad* a, const SolidColorDrawQuad* b) {
    EXPECT_EQ(a->color, b->color);
    EXPECT_EQ(a->force_anti_aliasing_off, b->force_anti_aliasing_off);
  }

  void Compare(const StreamVideoDrawQuad* a, const StreamVideoDrawQuad* b) {
    EXPECT_EQ(a->resource_id(), b->resource_id());
    EXPECT_EQ(a->resource_size_in_pixels(), b->resource_size_in_pixels());
    EXPECT_EQ(a->matrix, b->matrix);
  }

  void Compare(const SurfaceDrawQuad* a, const SurfaceDrawQuad* b) {
    EXPECT_EQ(a->surface_id, b->surface_id);
  }

  void Compare(const TextureDrawQuad* a, const TextureDrawQuad* b) {
    EXPECT_EQ(a->resource_id(), b->resource_id());
    EXPECT_EQ(a->resource_size_in_pixels(), b->resource_size_in_pixels());
    EXPECT_EQ(a->premultiplied_alpha, b->premultiplied_alpha);
    EXPECT_EQ(a->uv_top_left, b->uv_top_left);
    EXPECT_EQ(a->uv_bottom_right, b->uv_bottom_right);
    EXPECT_EQ(a->background_color, b->background_color);
    EXPECT_EQ(a->vertex_opacity[0], b->vertex_opacity[0]);
    EXPECT_EQ(a->vertex_opacity[1], b->vertex_opacity[1]);
    EXPECT_EQ(a->vertex_opacity[2], b->vertex_opacity[2]);
    EXPECT_EQ(a->vertex_opacity[3], b->vertex_opacity[3]);
    EXPECT_EQ(a->y_flipped, b->y_flipped);
    EXPECT_EQ(a->nearest_neighbor, b->nearest_neighbor);
    EXPECT_EQ(a->secure_output_only, b->secure_output_only);
  }

  void Compare(const TileDrawQuad* a, const TileDrawQuad* b) {
    EXPECT_EQ(a->resource_id(), b->resource_id());
    EXPECT_EQ(a->tex_coord_rect, b->tex_coord_rect);
    EXPECT_EQ(a->texture_size, b->texture_size);
    EXPECT_EQ(a->swizzle_contents, b->swizzle_contents);
    EXPECT_EQ(a->nearest_neighbor, b->nearest_neighbor);
  }

  void Compare(const YUVVideoDrawQuad* a, const YUVVideoDrawQuad* b) {
    EXPECT_EQ(a->ya_tex_coord_rect, b->ya_tex_coord_rect);
    EXPECT_EQ(a->uv_tex_coord_rect, b->uv_tex_coord_rect);
    EXPECT_EQ(a->ya_tex_size, b->ya_tex_size);
    EXPECT_EQ(a->uv_tex_size, b->uv_tex_size);
    EXPECT_EQ(a->y_plane_resource_id(), b->y_plane_resource_id());
    EXPECT_EQ(a->u_plane_resource_id(), b->u_plane_resource_id());
    EXPECT_EQ(a->v_plane_resource_id(), b->v_plane_resource_id());
    EXPECT_EQ(a->a_plane_resource_id(), b->a_plane_resource_id());
    EXPECT_EQ(a->color_space, b->color_space);
  }

  void Compare(const TransferableResource& a, const TransferableResource& b) {
    EXPECT_EQ(a.id, b.id);
    EXPECT_EQ(a.format, b.format);
    EXPECT_EQ(a.filter, b.filter);
    EXPECT_EQ(a.size.ToString(), b.size.ToString());
    for (size_t i = 0; i < arraysize(a.mailbox_holder.mailbox.name); ++i) {
      EXPECT_EQ(a.mailbox_holder.mailbox.name[i],
                b.mailbox_holder.mailbox.name[i]);
    }
    EXPECT_EQ(a.mailbox_holder.texture_target, b.mailbox_holder.texture_target);
    EXPECT_EQ(a.mailbox_holder.sync_token, b.mailbox_holder.sync_token);
    EXPECT_EQ(a.is_overlay_candidate, b.is_overlay_candidate);
  }
};

TEST_F(CCParamTraitsTest, AllQuads) {
  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);

  Transform arbitrary_matrix1;
  arbitrary_matrix1.Scale(3, 3);
  arbitrary_matrix1.Translate(-5, 20);
  arbitrary_matrix1.Rotate(15);
  Transform arbitrary_matrix2;
  arbitrary_matrix2.Scale(10, -1);
  arbitrary_matrix2.Translate(20, 3);
  arbitrary_matrix2.Rotate(24);
  gfx::Rect arbitrary_rect1(-5, 9, 3, 15);
  gfx::Rect arbitrary_rect1_inside_rect1(-4, 12, 2, 8);
  gfx::Rect arbitrary_rect2_inside_rect1(-5, 11, 1, 2);
  gfx::Rect arbitrary_rect2(40, 23, 11, 7);
  gfx::Rect arbitrary_rect1_inside_rect2(44, 23, 4, 2);
  gfx::Rect arbitrary_rect2_inside_rect2(41, 25, 3, 5);
  gfx::Rect arbitrary_rect3(7, -53, 22, 19);
  gfx::Rect arbitrary_rect1_inside_rect3(10, -40, 6, 3);
  gfx::Rect arbitrary_rect2_inside_rect3(12, -51, 5, 12);
  gfx::Size arbitrary_size1(15, 19);
  gfx::Size arbitrary_size2(3, 99);
  gfx::Size arbitrary_size3(75, 1281);
  gfx::RectF arbitrary_rectf1(4.2f, -922.1f, 15.6f, 29.5f);
  gfx::RectF arbitrary_rectf2(2.1f, -411.05f, 7.8f, 14.75f);
  gfx::SizeF arbitrary_sizef1(15.2f, 104.6f);
  gfx::PointF arbitrary_pointf1(31.4f, 15.9f);
  gfx::PointF arbitrary_pointf2(26.5f, -35.8f);
  gfx::Vector2dF arbitrary_vector2df1(16.2f, -85.1f);
  gfx::Vector2dF arbitrary_vector2df2(-8.3f, 0.47f);
  float arbitrary_float1 = 0.7f;
  float arbitrary_float2 = 0.3f;
  float arbitrary_float3 = 0.9f;
  float arbitrary_float_array[4] = {3.5f, 6.2f, 9.3f, 12.3f};
  bool arbitrary_bool1 = true;
  bool arbitrary_bool2 = false;
  bool arbitrary_bool3 = true;
  bool arbitrary_bool4 = true;
  bool arbitrary_bool5 = false;
  bool arbitrary_bool6 = true;
  int arbitrary_context_id1 = 12;
  int arbitrary_context_id2 = 57;
  int arbitrary_context_id3 = -503;
  int arbitrary_int = 5;
  SkColor arbitrary_color = SkColorSetARGB(25, 36, 47, 58);
  SkXfermode::Mode arbitrary_blend_mode1 = SkXfermode::kScreen_Mode;
  SkXfermode::Mode arbitrary_blend_mode2 = SkXfermode::kLighten_Mode;
  SkXfermode::Mode arbitrary_blend_mode3 = SkXfermode::kOverlay_Mode;
  ResourceId arbitrary_resourceid1 = 55;
  ResourceId arbitrary_resourceid2 = 47;
  ResourceId arbitrary_resourceid3 = 23;
  ResourceId arbitrary_resourceid4 = 16;
  SkScalar arbitrary_sigma = SkFloatToScalar(2.0f);
  YUVVideoDrawQuad::ColorSpace arbitrary_color_space =
      YUVVideoDrawQuad::REC_601;

  RenderPassId child_id(30, 5);
  RenderPassId root_id(10, 14);

  FilterOperations arbitrary_filters1;
  arbitrary_filters1.Append(
      FilterOperation::CreateGrayscaleFilter(arbitrary_float1));
  arbitrary_filters1.Append(cc::FilterOperation::CreateReferenceFilter(
      SkBlurImageFilter::Make(arbitrary_sigma, arbitrary_sigma, nullptr)));

  FilterOperations arbitrary_filters2;
  arbitrary_filters2.Append(
      FilterOperation::CreateBrightnessFilter(arbitrary_float2));

  std::unique_ptr<RenderPass> child_pass_in = RenderPass::Create();
  child_pass_in->SetAll(child_id, arbitrary_rect2, arbitrary_rect3,
                        arbitrary_matrix2, arbitrary_bool2);

  std::unique_ptr<RenderPass> child_pass_cmp = RenderPass::Create();
  child_pass_cmp->SetAll(child_id, arbitrary_rect2, arbitrary_rect3,
                         arbitrary_matrix2, arbitrary_bool2);

  std::unique_ptr<RenderPass> pass_in = RenderPass::Create();
  pass_in->SetAll(root_id, arbitrary_rect1, arbitrary_rect2, arbitrary_matrix1,
                  arbitrary_bool1);

  SharedQuadState* shared_state1_in = pass_in->CreateAndAppendSharedQuadState();
  shared_state1_in->SetAll(arbitrary_matrix1, arbitrary_size1, arbitrary_rect1,
                           arbitrary_rect2, arbitrary_bool1, arbitrary_float1,
                           arbitrary_blend_mode1, arbitrary_context_id1);

  std::unique_ptr<RenderPass> pass_cmp = RenderPass::Create();
  pass_cmp->SetAll(root_id, arbitrary_rect1, arbitrary_rect2, arbitrary_matrix1,
                   arbitrary_bool1);

  SharedQuadState* shared_state1_cmp =
      pass_cmp->CreateAndAppendSharedQuadState();
  *shared_state1_cmp = *shared_state1_in;

  DebugBorderDrawQuad* debugborder_in =
      pass_in->CreateAndAppendDrawQuad<DebugBorderDrawQuad>();
  debugborder_in->SetAll(shared_state1_in, arbitrary_rect3,
                         arbitrary_rect1_inside_rect3,
                         arbitrary_rect2_inside_rect3, arbitrary_bool1,
                         arbitrary_color, arbitrary_int);
  pass_cmp->CopyFromAndAppendDrawQuad(debugborder_in,
                                      debugborder_in->shared_quad_state);

  SharedQuadState* shared_state2_in = pass_in->CreateAndAppendSharedQuadState();
  shared_state2_in->SetAll(arbitrary_matrix2, arbitrary_size2, arbitrary_rect2,
                           arbitrary_rect3, arbitrary_bool1, arbitrary_float2,
                           arbitrary_blend_mode2, arbitrary_context_id2);
  SharedQuadState* shared_state2_cmp =
      pass_cmp->CreateAndAppendSharedQuadState();
  *shared_state2_cmp = *shared_state2_in;

  RenderPassDrawQuad* renderpass_in =
      pass_in->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  renderpass_in->SetAll(
      shared_state2_in, arbitrary_rect1, arbitrary_rect2_inside_rect1,
      arbitrary_rect1_inside_rect1, arbitrary_bool1, child_id,
      arbitrary_resourceid2, arbitrary_vector2df1, arbitrary_size1,
      arbitrary_filters1, arbitrary_vector2df2, arbitrary_filters2);
  pass_cmp->CopyFromAndAppendRenderPassDrawQuad(
      renderpass_in, renderpass_in->shared_quad_state,
      renderpass_in->render_pass_id);

  SharedQuadState* shared_state3_in = pass_in->CreateAndAppendSharedQuadState();
  shared_state3_in->SetAll(arbitrary_matrix1, arbitrary_size3, arbitrary_rect3,
                           arbitrary_rect1, arbitrary_bool1, arbitrary_float3,
                           arbitrary_blend_mode3, arbitrary_context_id3);
  SharedQuadState* shared_state3_cmp =
      pass_cmp->CreateAndAppendSharedQuadState();
  *shared_state3_cmp = *shared_state3_in;

  SolidColorDrawQuad* solidcolor_in =
      pass_in->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  solidcolor_in->SetAll(shared_state3_in, arbitrary_rect3,
                        arbitrary_rect1_inside_rect3,
                        arbitrary_rect2_inside_rect3, arbitrary_bool1,
                        arbitrary_color, arbitrary_bool2);
  pass_cmp->CopyFromAndAppendDrawQuad(solidcolor_in,
                                      solidcolor_in->shared_quad_state);

  StreamVideoDrawQuad* streamvideo_in =
      pass_in->CreateAndAppendDrawQuad<StreamVideoDrawQuad>();
  streamvideo_in->SetAll(
      shared_state3_in, arbitrary_rect2, arbitrary_rect2_inside_rect2,
      arbitrary_rect1_inside_rect2, arbitrary_bool1, arbitrary_resourceid2,
      arbitrary_size1, arbitrary_matrix1);
  pass_cmp->CopyFromAndAppendDrawQuad(streamvideo_in,
                                      streamvideo_in->shared_quad_state);

  cc::SurfaceId arbitrary_surface_id(0, 3, 0);
  SurfaceDrawQuad* surface_in =
      pass_in->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  surface_in->SetAll(shared_state3_in, arbitrary_rect2,
                     arbitrary_rect2_inside_rect2, arbitrary_rect1_inside_rect2,
                     arbitrary_bool1, arbitrary_surface_id);
  pass_cmp->CopyFromAndAppendDrawQuad(surface_in,
                                      surface_in->shared_quad_state);

  TextureDrawQuad* texture_in =
      pass_in->CreateAndAppendDrawQuad<TextureDrawQuad>();
  texture_in->SetAll(shared_state3_in, arbitrary_rect2,
                     arbitrary_rect2_inside_rect2, arbitrary_rect1_inside_rect2,
                     arbitrary_bool1, arbitrary_resourceid1, arbitrary_size1,
                     arbitrary_bool2, arbitrary_pointf1, arbitrary_pointf2,
                     arbitrary_color, arbitrary_float_array, arbitrary_bool4,
                     arbitrary_bool5, arbitrary_bool6);
  pass_cmp->CopyFromAndAppendDrawQuad(texture_in,
                                      texture_in->shared_quad_state);

  TileDrawQuad* tile_in = pass_in->CreateAndAppendDrawQuad<TileDrawQuad>();
  tile_in->SetAll(shared_state3_in, arbitrary_rect2,
                  arbitrary_rect2_inside_rect2, arbitrary_rect1_inside_rect2,
                  arbitrary_bool1, arbitrary_resourceid3, arbitrary_rectf1,
                  arbitrary_size1, arbitrary_bool2, arbitrary_bool3);
  pass_cmp->CopyFromAndAppendDrawQuad(tile_in, tile_in->shared_quad_state);

  YUVVideoDrawQuad* yuvvideo_in =
      pass_in->CreateAndAppendDrawQuad<YUVVideoDrawQuad>();
  yuvvideo_in->SetAll(
      shared_state3_in, arbitrary_rect1, arbitrary_rect2_inside_rect1,
      arbitrary_rect1_inside_rect1, arbitrary_bool1, arbitrary_rectf1,
      arbitrary_rectf2, arbitrary_size1, arbitrary_size2, arbitrary_resourceid1,
      arbitrary_resourceid2, arbitrary_resourceid3, arbitrary_resourceid4,
      arbitrary_color_space, arbitrary_float1, arbitrary_float2);
  pass_cmp->CopyFromAndAppendDrawQuad(yuvvideo_in,
                                      yuvvideo_in->shared_quad_state);

  // Make sure the in and cmp RenderPasses match.
  Compare(child_pass_cmp.get(), child_pass_in.get());
  ASSERT_EQ(0u, child_pass_in->shared_quad_state_list.size());
  ASSERT_EQ(0u, child_pass_in->quad_list.size());
  Compare(pass_cmp.get(), pass_in.get());
  ASSERT_EQ(3u, pass_in->shared_quad_state_list.size());
  ASSERT_EQ(8u, pass_in->quad_list.size());
  for (cc::SharedQuadStateList::ConstIterator
           cmp_iterator = pass_cmp->shared_quad_state_list.begin(),
           in_iterator = pass_in->shared_quad_state_list.begin();
       in_iterator != pass_in->shared_quad_state_list.end();
       ++cmp_iterator, ++in_iterator) {
    Compare(*cmp_iterator, *in_iterator);
  }
  for (auto in_iter = pass_in->quad_list.cbegin(),
            cmp_iter = pass_cmp->quad_list.cbegin();
       in_iter != pass_in->quad_list.cend(); ++in_iter, ++cmp_iter)
    Compare(*cmp_iter, *in_iter);

  for (size_t i = 1; i < pass_in->quad_list.size(); ++i) {
    bool same_shared_quad_state_cmp =
        pass_cmp->quad_list.ElementAt(i)->shared_quad_state ==
        pass_cmp->quad_list.ElementAt(i - 1)->shared_quad_state;
    bool same_shared_quad_state_in =
        pass_in->quad_list.ElementAt(i)->shared_quad_state ==
        pass_in->quad_list.ElementAt(i - 1)->shared_quad_state;
    EXPECT_EQ(same_shared_quad_state_cmp, same_shared_quad_state_in);
  }

  DelegatedFrameData frame_in;
  frame_in.render_pass_list.push_back(std::move(child_pass_in));
  frame_in.render_pass_list.push_back(std::move(pass_in));

  IPC::ParamTraits<DelegatedFrameData>::Write(&msg, frame_in);

  DelegatedFrameData frame_out;
  base::PickleIterator iter(msg);
  EXPECT_TRUE(
      IPC::ParamTraits<DelegatedFrameData>::Read(&msg, &iter, &frame_out));

  // Make sure the out and cmp RenderPasses match.
  std::unique_ptr<RenderPass> child_pass_out =
      std::move(frame_out.render_pass_list[0]);
  Compare(child_pass_cmp.get(), child_pass_out.get());
  ASSERT_EQ(0u, child_pass_out->shared_quad_state_list.size());
  ASSERT_EQ(0u, child_pass_out->quad_list.size());
  std::unique_ptr<RenderPass> pass_out =
      std::move(frame_out.render_pass_list[1]);
  Compare(pass_cmp.get(), pass_out.get());
  ASSERT_EQ(3u, pass_out->shared_quad_state_list.size());
  ASSERT_EQ(8u, pass_out->quad_list.size());
  for (cc::SharedQuadStateList::ConstIterator
           cmp_iterator = pass_cmp->shared_quad_state_list.begin(),
           out_iterator = pass_out->shared_quad_state_list.begin();
       out_iterator != pass_out->shared_quad_state_list.end();
       ++cmp_iterator, ++out_iterator) {
    Compare(*cmp_iterator, *out_iterator);
  }
  for (auto out_iter = pass_out->quad_list.cbegin(),
            cmp_iter = pass_cmp->quad_list.cbegin();
       out_iter != pass_out->quad_list.cend(); ++out_iter, ++cmp_iter)
    Compare(*cmp_iter, *out_iter);

  for (size_t i = 1; i < pass_out->quad_list.size(); ++i) {
    bool same_shared_quad_state_cmp =
        pass_cmp->quad_list.ElementAt(i)->shared_quad_state ==
        pass_cmp->quad_list.ElementAt(i - 1)->shared_quad_state;
    bool same_shared_quad_state_out =
        pass_out->quad_list.ElementAt(i)->shared_quad_state ==
        pass_out->quad_list.ElementAt(i - 1)->shared_quad_state;
    EXPECT_EQ(same_shared_quad_state_cmp, same_shared_quad_state_out);
  }
}

TEST_F(CCParamTraitsTest, UnusedSharedQuadStates) {
  std::unique_ptr<RenderPass> pass_in = RenderPass::Create();
  pass_in->SetAll(RenderPassId(1, 1), gfx::Rect(100, 100), gfx::Rect(),
                  gfx::Transform(), false);

  // The first SharedQuadState is used.
  SharedQuadState* shared_state1_in = pass_in->CreateAndAppendSharedQuadState();
  shared_state1_in->SetAll(gfx::Transform(), gfx::Size(1, 1), gfx::Rect(),
                           gfx::Rect(), false, 1.f, SkXfermode::kSrcOver_Mode,
                           0);

  SolidColorDrawQuad* quad1 =
      pass_in->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  quad1->SetAll(shared_state1_in, gfx::Rect(10, 10), gfx::Rect(10, 10),
                gfx::Rect(10, 10), false, SK_ColorRED, false);

  // The second and third SharedQuadStates are not used.
  SharedQuadState* shared_state2_in = pass_in->CreateAndAppendSharedQuadState();
  shared_state2_in->SetAll(gfx::Transform(), gfx::Size(2, 2), gfx::Rect(),
                           gfx::Rect(), false, 1.f, SkXfermode::kSrcOver_Mode,
                           0);

  SharedQuadState* shared_state3_in = pass_in->CreateAndAppendSharedQuadState();
  shared_state3_in->SetAll(gfx::Transform(), gfx::Size(3, 3), gfx::Rect(),
                           gfx::Rect(), false, 1.f, SkXfermode::kSrcOver_Mode,
                           0);

  // The fourth SharedQuadState is used.
  SharedQuadState* shared_state4_in = pass_in->CreateAndAppendSharedQuadState();
  shared_state4_in->SetAll(gfx::Transform(), gfx::Size(4, 4), gfx::Rect(),
                           gfx::Rect(), false, 1.f, SkXfermode::kSrcOver_Mode,
                           0);

  SolidColorDrawQuad* quad2 =
      pass_in->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  quad2->SetAll(shared_state4_in, gfx::Rect(10, 10), gfx::Rect(10, 10),
                gfx::Rect(10, 10), false, SK_ColorRED, false);

  // The fifth is not used again.
  SharedQuadState* shared_state5_in = pass_in->CreateAndAppendSharedQuadState();
  shared_state5_in->SetAll(gfx::Transform(), gfx::Size(5, 5), gfx::Rect(),
                           gfx::Rect(), false, 1.f, SkXfermode::kSrcOver_Mode,
                           0);

  // 5 SharedQuadStates go in.
  ASSERT_EQ(5u, pass_in->shared_quad_state_list.size());
  ASSERT_EQ(2u, pass_in->quad_list.size());

  DelegatedFrameData frame_in;
  frame_in.render_pass_list.push_back(std::move(pass_in));

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::ParamTraits<DelegatedFrameData>::Write(&msg, frame_in);

  DelegatedFrameData frame_out;
  base::PickleIterator iter(msg);
  EXPECT_TRUE(
      IPC::ParamTraits<DelegatedFrameData>::Read(&msg, &iter, &frame_out));

  std::unique_ptr<RenderPass> pass_out =
      std::move(frame_out.render_pass_list[0]);

  // 2 SharedQuadStates come out. The first and fourth SharedQuadStates were
  // used by quads, and so serialized. Others were not.
  ASSERT_EQ(2u, pass_out->shared_quad_state_list.size());
  ASSERT_EQ(2u, pass_out->quad_list.size());

  EXPECT_EQ(gfx::Size(1, 1).ToString(),
            pass_out->shared_quad_state_list.ElementAt(0)
                ->quad_layer_bounds.ToString());
  EXPECT_EQ(gfx::Size(4, 4).ToString(),
            pass_out->shared_quad_state_list.ElementAt(1)
                ->quad_layer_bounds.ToString());
}

TEST_F(CCParamTraitsTest, Resources) {
  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  gfx::Size arbitrary_size(757, 1281);
  gpu::SyncToken arbitrary_token1(gpu::CommandBufferNamespace::GPU_IO, 0,
                                  gpu::CommandBufferId::FromUnsafeValue(0x123),
                                  71234838);
  arbitrary_token1.SetVerifyFlush();
  gpu::SyncToken arbitrary_token2(gpu::CommandBufferNamespace::GPU_IO, 0,
                                  gpu::CommandBufferId::FromUnsafeValue(0x123),
                                  53589793);
  arbitrary_token2.SetVerifyFlush();

  GLbyte arbitrary_mailbox1[GL_MAILBOX_SIZE_CHROMIUM] = {
      1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2,
      3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4,
      5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4};

  GLbyte arbitrary_mailbox2[GL_MAILBOX_SIZE_CHROMIUM] = {
      0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9,
      8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9, 8, 7,
      6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9, 8, 7};

  TransferableResource arbitrary_resource1;
  arbitrary_resource1.id = 2178312;
  arbitrary_resource1.format = cc::RGBA_8888;
  arbitrary_resource1.filter = 53;
  arbitrary_resource1.size = gfx::Size(37189, 123123);
  arbitrary_resource1.mailbox_holder.mailbox.SetName(arbitrary_mailbox1);
  arbitrary_resource1.mailbox_holder.texture_target = GL_TEXTURE_2D;
  arbitrary_resource1.mailbox_holder.sync_token = arbitrary_token1;
  arbitrary_resource1.is_overlay_candidate = true;

  TransferableResource arbitrary_resource2;
  arbitrary_resource2.id = 789132;
  arbitrary_resource2.format = cc::RGBA_4444;
  arbitrary_resource2.filter = 47;
  arbitrary_resource2.size = gfx::Size(89123, 23789);
  arbitrary_resource2.mailbox_holder.mailbox.SetName(arbitrary_mailbox2);
  arbitrary_resource2.mailbox_holder.texture_target = GL_TEXTURE_EXTERNAL_OES;
  arbitrary_resource2.mailbox_holder.sync_token = arbitrary_token2;
  arbitrary_resource2.is_overlay_candidate = false;

  std::unique_ptr<RenderPass> renderpass_in = RenderPass::Create();
  renderpass_in->SetNew(RenderPassId(1, 1), gfx::Rect(), gfx::Rect(),
                        gfx::Transform());

  DelegatedFrameData frame_in;
  frame_in.resource_list.push_back(arbitrary_resource1);
  frame_in.resource_list.push_back(arbitrary_resource2);
  frame_in.render_pass_list.push_back(std::move(renderpass_in));

  IPC::ParamTraits<DelegatedFrameData>::Write(&msg, frame_in);

  DelegatedFrameData frame_out;
  base::PickleIterator iter(msg);
  EXPECT_TRUE(
      IPC::ParamTraits<DelegatedFrameData>::Read(&msg, &iter, &frame_out));

  ASSERT_EQ(2u, frame_out.resource_list.size());
  Compare(arbitrary_resource1, frame_out.resource_list[0]);
  Compare(arbitrary_resource2, frame_out.resource_list[1]);
}

}  // namespace
}  // namespace content
