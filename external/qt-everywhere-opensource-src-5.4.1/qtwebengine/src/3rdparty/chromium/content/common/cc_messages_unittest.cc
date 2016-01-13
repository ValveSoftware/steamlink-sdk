// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cc_messages.h"

#include <string.h>

#include <algorithm>

#include "cc/output/compositor_frame.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

using cc::CheckerboardDrawQuad;
using cc::DelegatedFrameData;
using cc::DebugBorderDrawQuad;
using cc::DrawQuad;
using cc::FilterOperation;
using cc::FilterOperations;
using cc::IOSurfaceDrawQuad;
using cc::PictureDrawQuad;
using cc::RenderPass;
using cc::RenderPassDrawQuad;
using cc::ResourceProvider;
using cc::SharedQuadState;
using cc::SoftwareFrameData;
using cc::SolidColorDrawQuad;
using cc::SurfaceDrawQuad;
using cc::TextureDrawQuad;
using cc::TileDrawQuad;
using cc::TransferableResource;
using cc::StreamVideoDrawQuad;
using cc::VideoLayerImpl;
using cc::YUVVideoDrawQuad;
using gfx::Transform;

namespace content {
namespace {

class CCMessagesTest : public testing::Test {
 protected:
  void Compare(const RenderPass* a, const RenderPass* b) {
    EXPECT_EQ(a->id, b->id);
    EXPECT_EQ(a->output_rect.ToString(), b->output_rect.ToString());
    EXPECT_EQ(a->damage_rect.ToString(), b->damage_rect.ToString());
    EXPECT_EQ(a->transform_to_root_target, b->transform_to_root_target);
    EXPECT_EQ(a->has_transparent_background, b->has_transparent_background);
  }

  void Compare(const SharedQuadState* a, const SharedQuadState* b) {
    EXPECT_EQ(a->content_to_target_transform, b->content_to_target_transform);
    EXPECT_EQ(a->content_bounds, b->content_bounds);
    EXPECT_EQ(a->visible_content_rect, b->visible_content_rect);
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
      case DrawQuad::CHECKERBOARD:
        Compare(CheckerboardDrawQuad::MaterialCast(a),
                CheckerboardDrawQuad::MaterialCast(b));
        break;
      case DrawQuad::DEBUG_BORDER:
        Compare(DebugBorderDrawQuad::MaterialCast(a),
                DebugBorderDrawQuad::MaterialCast(b));
        break;
      case DrawQuad::IO_SURFACE_CONTENT:
        Compare(IOSurfaceDrawQuad::MaterialCast(a),
                IOSurfaceDrawQuad::MaterialCast(b));
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
        Compare(TileDrawQuad::MaterialCast(a),
                TileDrawQuad::MaterialCast(b));
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

  void Compare(const CheckerboardDrawQuad* a, const CheckerboardDrawQuad* b) {
    EXPECT_EQ(a->color, b->color);
  }

  void Compare(const DebugBorderDrawQuad* a, const DebugBorderDrawQuad* b) {
    EXPECT_EQ(a->color, b->color);
    EXPECT_EQ(a->width, b->width);
  }

  void Compare(const IOSurfaceDrawQuad* a, const IOSurfaceDrawQuad* b) {
    EXPECT_EQ(a->io_surface_size.ToString(), b->io_surface_size.ToString());
    EXPECT_EQ(a->io_surface_resource_id, b->io_surface_resource_id);
    EXPECT_EQ(a->orientation, b->orientation);
  }

  void Compare(const RenderPassDrawQuad* a, const RenderPassDrawQuad* b) {
    EXPECT_EQ(a->render_pass_id, b->render_pass_id);
    EXPECT_EQ(a->is_replica, b->is_replica);
    EXPECT_EQ(a->mask_resource_id, b->mask_resource_id);
    EXPECT_EQ(a->contents_changed_since_last_frame,
              b->contents_changed_since_last_frame);
    EXPECT_EQ(a->mask_uv_rect.ToString(), b->mask_uv_rect.ToString());
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
    EXPECT_EQ(a->background_filters, b->background_filters);
  }

  void Compare(const SolidColorDrawQuad* a, const SolidColorDrawQuad* b) {
    EXPECT_EQ(a->color, b->color);
    EXPECT_EQ(a->force_anti_aliasing_off, b->force_anti_aliasing_off);
  }

  void Compare(const StreamVideoDrawQuad* a, const StreamVideoDrawQuad* b) {
    EXPECT_EQ(a->resource_id, b->resource_id);
    EXPECT_EQ(a->matrix, b->matrix);
  }

  void Compare(const SurfaceDrawQuad* a, const SurfaceDrawQuad* b) {
    EXPECT_EQ(a->surface_id, b->surface_id);
  }

  void Compare(const TextureDrawQuad* a, const TextureDrawQuad* b) {
    EXPECT_EQ(a->resource_id, b->resource_id);
    EXPECT_EQ(a->premultiplied_alpha, b->premultiplied_alpha);
    EXPECT_EQ(a->uv_top_left, b->uv_top_left);
    EXPECT_EQ(a->uv_bottom_right, b->uv_bottom_right);
    EXPECT_EQ(a->background_color, b->background_color);
    EXPECT_EQ(a->vertex_opacity[0], b->vertex_opacity[0]);
    EXPECT_EQ(a->vertex_opacity[1], b->vertex_opacity[1]);
    EXPECT_EQ(a->vertex_opacity[2], b->vertex_opacity[2]);
    EXPECT_EQ(a->vertex_opacity[3], b->vertex_opacity[3]);
    EXPECT_EQ(a->flipped, b->flipped);
  }

  void Compare(const TileDrawQuad* a, const TileDrawQuad* b) {
    EXPECT_EQ(a->resource_id, b->resource_id);
    EXPECT_EQ(a->tex_coord_rect, b->tex_coord_rect);
    EXPECT_EQ(a->texture_size, b->texture_size);
    EXPECT_EQ(a->swizzle_contents, b->swizzle_contents);
  }

  void Compare(const YUVVideoDrawQuad* a, const YUVVideoDrawQuad* b) {
    EXPECT_EQ(a->tex_coord_rect, b->tex_coord_rect);
    EXPECT_EQ(a->y_plane_resource_id, b->y_plane_resource_id);
    EXPECT_EQ(a->u_plane_resource_id, b->u_plane_resource_id);
    EXPECT_EQ(a->v_plane_resource_id, b->v_plane_resource_id);
    EXPECT_EQ(a->a_plane_resource_id, b->a_plane_resource_id);
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
    EXPECT_EQ(a.mailbox_holder.sync_point, b.mailbox_holder.sync_point);
  }
};

TEST_F(CCMessagesTest, AllQuads) {
  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);

  Transform arbitrary_matrix;
  arbitrary_matrix.Scale(3, 3);
  arbitrary_matrix.Translate(-5, 20);
  arbitrary_matrix.Rotate(15);
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
  gfx::SizeF arbitrary_sizef1(15.2f, 104.6f);
  gfx::PointF arbitrary_pointf1(31.4f, 15.9f);
  gfx::PointF arbitrary_pointf2(26.5f, -35.8f);
  float arbitrary_float1 = 0.7f;
  float arbitrary_float2 = 0.3f;
  float arbitrary_float3 = 0.9f;
  float arbitrary_float_array[4] = {3.5f, 6.2f, 9.3f, 12.3f};
  bool arbitrary_bool1 = true;
  bool arbitrary_bool2 = false;
  bool arbitrary_bool3 = true;
  int arbitrary_context_id1 = 12;
  int arbitrary_context_id2 = 57;
  int arbitrary_context_id3 = -503;
  int arbitrary_int = 5;
  SkColor arbitrary_color = SkColorSetARGB(25, 36, 47, 58);
  SkXfermode::Mode arbitrary_blend_mode1 = SkXfermode::kScreen_Mode;
  SkXfermode::Mode arbitrary_blend_mode2 = SkXfermode::kLighten_Mode;
  SkXfermode::Mode arbitrary_blend_mode3 = SkXfermode::kOverlay_Mode;
  IOSurfaceDrawQuad::Orientation arbitrary_orientation =
      IOSurfaceDrawQuad::UNFLIPPED;
  RenderPass::Id arbitrary_id(10, 14);
  ResourceProvider::ResourceId arbitrary_resourceid1 = 55;
  ResourceProvider::ResourceId arbitrary_resourceid2 = 47;
  ResourceProvider::ResourceId arbitrary_resourceid3 = 23;
  ResourceProvider::ResourceId arbitrary_resourceid4 = 16;
  SkScalar arbitrary_sigma = SkFloatToScalar(2.0f);
  YUVVideoDrawQuad::ColorSpace arbitrary_color_space =
      YUVVideoDrawQuad::REC_601;

  FilterOperations arbitrary_filters1;
  arbitrary_filters1.Append(FilterOperation::CreateGrayscaleFilter(
      arbitrary_float1));
  skia::RefPtr<SkImageFilter> arbitrary_filter = skia::AdoptRef(
      SkBlurImageFilter::Create(arbitrary_sigma, arbitrary_sigma));
  arbitrary_filters1.Append(
      cc::FilterOperation::CreateReferenceFilter(arbitrary_filter));

  FilterOperations arbitrary_filters2;
  arbitrary_filters2.Append(FilterOperation::CreateBrightnessFilter(
      arbitrary_float2));

  scoped_ptr<RenderPass> pass_in = RenderPass::Create();
  pass_in->SetAll(arbitrary_id,
                  arbitrary_rect1,
                  arbitrary_rect2,
                  arbitrary_matrix,
                  arbitrary_bool1);

  SharedQuadState* shared_state1_in = pass_in->CreateAndAppendSharedQuadState();
  shared_state1_in->SetAll(arbitrary_matrix,
                           arbitrary_size1,
                           arbitrary_rect1,
                           arbitrary_rect2,
                           arbitrary_bool1,
                           arbitrary_float1,
                           arbitrary_blend_mode1,
                           arbitrary_context_id1);

  scoped_ptr<RenderPass> pass_cmp = RenderPass::Create();
  pass_cmp->SetAll(arbitrary_id,
                   arbitrary_rect1,
                   arbitrary_rect2,
                   arbitrary_matrix,
                   arbitrary_bool1);

  SharedQuadState* shared_state1_cmp =
      pass_cmp->CreateAndAppendSharedQuadState();
  shared_state1_cmp->CopyFrom(shared_state1_in);

  scoped_ptr<CheckerboardDrawQuad> checkerboard_in =
      CheckerboardDrawQuad::Create();
  checkerboard_in->SetAll(shared_state1_in,
                          arbitrary_rect1,
                          arbitrary_rect2_inside_rect1,
                          arbitrary_rect1_inside_rect1,
                          arbitrary_bool1,
                          arbitrary_color);
  scoped_ptr<DrawQuad> checkerboard_cmp = checkerboard_in->Copy(
      checkerboard_in->shared_quad_state);

  scoped_ptr<DebugBorderDrawQuad> debugborder_in =
      DebugBorderDrawQuad::Create();
  debugborder_in->SetAll(shared_state1_in,
                         arbitrary_rect3,
                         arbitrary_rect1_inside_rect3,
                         arbitrary_rect2_inside_rect3,
                         arbitrary_bool1,
                         arbitrary_color,
                         arbitrary_int);
  scoped_ptr<DrawQuad> debugborder_cmp = debugborder_in->Copy(
      debugborder_in->shared_quad_state);

  scoped_ptr<IOSurfaceDrawQuad> iosurface_in =
      IOSurfaceDrawQuad::Create();
  iosurface_in->SetAll(shared_state1_in,
                       arbitrary_rect2,
                       arbitrary_rect2_inside_rect2,
                       arbitrary_rect1_inside_rect2,
                       arbitrary_bool1,
                       arbitrary_size1,
                       arbitrary_resourceid3,
                       arbitrary_orientation);
  scoped_ptr<DrawQuad> iosurface_cmp = iosurface_in->Copy(
      iosurface_in->shared_quad_state);

  SharedQuadState* shared_state2_in = pass_in->CreateAndAppendSharedQuadState();
  shared_state2_in->SetAll(arbitrary_matrix,
                           arbitrary_size2,
                           arbitrary_rect2,
                           arbitrary_rect3,
                           arbitrary_bool1,
                           arbitrary_float2,
                           arbitrary_blend_mode2,
                           arbitrary_context_id2);
  SharedQuadState* shared_state2_cmp =
      pass_cmp->CreateAndAppendSharedQuadState();
  shared_state2_cmp->CopyFrom(shared_state2_in);

  scoped_ptr<RenderPassDrawQuad> renderpass_in =
      RenderPassDrawQuad::Create();
  renderpass_in->SetAll(shared_state2_in,
                        arbitrary_rect1,
                        arbitrary_rect2_inside_rect1,
                        arbitrary_rect1_inside_rect1,
                        arbitrary_bool1,
                        arbitrary_id,
                        arbitrary_bool2,
                        arbitrary_resourceid2,
                        arbitrary_rect1,
                        arbitrary_rectf1,
                        arbitrary_filters1,
                        arbitrary_filters2);
  scoped_ptr<RenderPassDrawQuad> renderpass_cmp = renderpass_in->Copy(
      renderpass_in->shared_quad_state, renderpass_in->render_pass_id);

  SharedQuadState* shared_state3_in = pass_in->CreateAndAppendSharedQuadState();
  shared_state3_in->SetAll(arbitrary_matrix,
                           arbitrary_size3,
                           arbitrary_rect3,
                           arbitrary_rect1,
                           arbitrary_bool1,
                           arbitrary_float3,
                           arbitrary_blend_mode3,
                           arbitrary_context_id3);
  SharedQuadState* shared_state3_cmp =
      pass_cmp->CreateAndAppendSharedQuadState();
  shared_state3_cmp->CopyFrom(shared_state3_in);

  scoped_ptr<SolidColorDrawQuad> solidcolor_in =
      SolidColorDrawQuad::Create();
  solidcolor_in->SetAll(shared_state3_in,
                        arbitrary_rect3,
                        arbitrary_rect1_inside_rect3,
                        arbitrary_rect2_inside_rect3,
                        arbitrary_bool1,
                        arbitrary_color,
                        arbitrary_bool2);
  scoped_ptr<DrawQuad> solidcolor_cmp = solidcolor_in->Copy(
      solidcolor_in->shared_quad_state);

  scoped_ptr<StreamVideoDrawQuad> streamvideo_in =
      StreamVideoDrawQuad::Create();
  streamvideo_in->SetAll(shared_state3_in,
                         arbitrary_rect2,
                         arbitrary_rect2_inside_rect2,
                         arbitrary_rect1_inside_rect2,
                         arbitrary_bool1,
                         arbitrary_resourceid2,
                         arbitrary_matrix);
  scoped_ptr<DrawQuad> streamvideo_cmp = streamvideo_in->Copy(
      streamvideo_in->shared_quad_state);

  cc::SurfaceId arbitrary_surface_id(3);
  scoped_ptr<SurfaceDrawQuad> surface_in = SurfaceDrawQuad::Create();
  surface_in->SetAll(shared_state3_in,
                     arbitrary_rect2,
                     arbitrary_rect2_inside_rect2,
                     arbitrary_rect1_inside_rect2,
                     arbitrary_bool1,
                     arbitrary_surface_id);
  scoped_ptr<DrawQuad> surface_cmp = surface_in->Copy(
      surface_in->shared_quad_state);

  scoped_ptr<TextureDrawQuad> texture_in = TextureDrawQuad::Create();
  texture_in->SetAll(shared_state3_in,
                     arbitrary_rect2,
                     arbitrary_rect2_inside_rect2,
                     arbitrary_rect1_inside_rect2,
                     arbitrary_bool1,
                     arbitrary_resourceid1,
                     arbitrary_bool2,
                     arbitrary_pointf1,
                     arbitrary_pointf2,
                     arbitrary_color,
                     arbitrary_float_array,
                     arbitrary_bool3);
  scoped_ptr<DrawQuad> texture_cmp = texture_in->Copy(
      texture_in->shared_quad_state);

  scoped_ptr<TileDrawQuad> tile_in = TileDrawQuad::Create();
  tile_in->SetAll(shared_state3_in,
                  arbitrary_rect2,
                  arbitrary_rect2_inside_rect2,
                  arbitrary_rect1_inside_rect2,
                  arbitrary_bool1,
                  arbitrary_resourceid3,
                  arbitrary_rectf1,
                  arbitrary_size1,
                  arbitrary_bool2);
  scoped_ptr<DrawQuad> tile_cmp = tile_in->Copy(
      tile_in->shared_quad_state);

  scoped_ptr<YUVVideoDrawQuad> yuvvideo_in =
      YUVVideoDrawQuad::Create();
  yuvvideo_in->SetAll(shared_state3_in,
                      arbitrary_rect1,
                      arbitrary_rect2_inside_rect1,
                      arbitrary_rect1_inside_rect1,
                      arbitrary_bool1,
                      arbitrary_rectf1,
                      arbitrary_resourceid1,
                      arbitrary_resourceid2,
                      arbitrary_resourceid3,
                      arbitrary_resourceid4,
                      arbitrary_color_space);
  scoped_ptr<DrawQuad> yuvvideo_cmp = yuvvideo_in->Copy(
      yuvvideo_in->shared_quad_state);

  pass_in->quad_list.push_back(checkerboard_in.PassAs<DrawQuad>());
  pass_in->quad_list.push_back(debugborder_in.PassAs<DrawQuad>());
  pass_in->quad_list.push_back(iosurface_in.PassAs<DrawQuad>());
  pass_in->quad_list.push_back(renderpass_in.PassAs<DrawQuad>());
  pass_in->quad_list.push_back(solidcolor_in.PassAs<DrawQuad>());
  pass_in->quad_list.push_back(streamvideo_in.PassAs<DrawQuad>());
  pass_in->quad_list.push_back(surface_in.PassAs<DrawQuad>());
  pass_in->quad_list.push_back(texture_in.PassAs<DrawQuad>());
  pass_in->quad_list.push_back(tile_in.PassAs<DrawQuad>());
  pass_in->quad_list.push_back(yuvvideo_in.PassAs<DrawQuad>());


  pass_cmp->quad_list.push_back(checkerboard_cmp.PassAs<DrawQuad>());
  pass_cmp->quad_list.push_back(debugborder_cmp.PassAs<DrawQuad>());
  pass_cmp->quad_list.push_back(iosurface_cmp.PassAs<DrawQuad>());
  pass_cmp->quad_list.push_back(renderpass_cmp.PassAs<DrawQuad>());
  pass_cmp->quad_list.push_back(solidcolor_cmp.PassAs<DrawQuad>());
  pass_cmp->quad_list.push_back(streamvideo_cmp.PassAs<DrawQuad>());
  pass_cmp->quad_list.push_back(surface_cmp.PassAs<DrawQuad>());
  pass_cmp->quad_list.push_back(texture_cmp.PassAs<DrawQuad>());
  pass_cmp->quad_list.push_back(tile_cmp.PassAs<DrawQuad>());
  pass_cmp->quad_list.push_back(yuvvideo_cmp.PassAs<DrawQuad>());

  // Make sure the in and cmp RenderPasses match.
  Compare(pass_cmp.get(), pass_in.get());
  ASSERT_EQ(3u, pass_in->shared_quad_state_list.size());
  ASSERT_EQ(10u, pass_in->quad_list.size());
  for (size_t i = 0; i < 3; ++i) {
    Compare(pass_cmp->shared_quad_state_list[i],
            pass_in->shared_quad_state_list[i]);
  }
  for (size_t i = 0; i < pass_in->quad_list.size(); ++i)
    Compare(pass_cmp->quad_list[i], pass_in->quad_list[i]);
  for (size_t i = 1; i < pass_in->quad_list.size(); ++i) {
    bool same_shared_quad_state_cmp =
        pass_cmp->quad_list[i]->shared_quad_state ==
        pass_cmp->quad_list[i - 1]->shared_quad_state;
    bool same_shared_quad_state_in =
        pass_in->quad_list[i]->shared_quad_state ==
        pass_in->quad_list[i - 1]->shared_quad_state;
    EXPECT_EQ(same_shared_quad_state_cmp, same_shared_quad_state_in);
  }

  DelegatedFrameData frame_in;
  frame_in.render_pass_list.push_back(pass_in.Pass());

  IPC::ParamTraits<DelegatedFrameData>::Write(&msg, frame_in);

  DelegatedFrameData frame_out;
  PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ParamTraits<DelegatedFrameData>::Read(&msg,
      &iter, &frame_out));

  // Make sure the out and cmp RenderPasses match.
  scoped_ptr<RenderPass> pass_out = frame_out.render_pass_list.take(
      frame_out.render_pass_list.begin());
  Compare(pass_cmp.get(), pass_out.get());
  ASSERT_EQ(3u, pass_out->shared_quad_state_list.size());
  ASSERT_EQ(10u, pass_out->quad_list.size());
  for (size_t i = 0; i < 3; ++i) {
    Compare(pass_cmp->shared_quad_state_list[i],
            pass_out->shared_quad_state_list[i]);
  }
  for (size_t i = 0; i < pass_out->quad_list.size(); ++i)
    Compare(pass_cmp->quad_list[i], pass_out->quad_list[i]);
  for (size_t i = 1; i < pass_out->quad_list.size(); ++i) {
    bool same_shared_quad_state_cmp =
        pass_cmp->quad_list[i]->shared_quad_state ==
        pass_cmp->quad_list[i - 1]->shared_quad_state;
    bool same_shared_quad_state_out =
        pass_out->quad_list[i]->shared_quad_state ==
        pass_out->quad_list[i - 1]->shared_quad_state;
    EXPECT_EQ(same_shared_quad_state_cmp, same_shared_quad_state_out);
  }
}

TEST_F(CCMessagesTest, UnusedSharedQuadStates) {
  scoped_ptr<CheckerboardDrawQuad> quad;

  scoped_ptr<RenderPass> pass_in = RenderPass::Create();
  pass_in->SetAll(RenderPass::Id(1, 1),
                  gfx::Rect(100, 100),
                  gfx::Rect(),
                  gfx::Transform(),
                  false);

  // The first SharedQuadState is used.
  SharedQuadState* shared_state1_in = pass_in->CreateAndAppendSharedQuadState();
  shared_state1_in->SetAll(gfx::Transform(),
                           gfx::Size(1, 1),
                           gfx::Rect(),
                           gfx::Rect(),
                           false,
                           1.f,
                           SkXfermode::kSrcOver_Mode,
                           0);

  quad = CheckerboardDrawQuad::Create();
  quad->SetAll(shared_state1_in,
               gfx::Rect(10, 10),
               gfx::Rect(10, 10),
               gfx::Rect(10, 10),
               false,
               SK_ColorRED);
  pass_in->quad_list.push_back(quad.PassAs<DrawQuad>());

  // The second and third SharedQuadStates are not used.
  SharedQuadState* shared_state2_in = pass_in->CreateAndAppendSharedQuadState();
  shared_state2_in->SetAll(gfx::Transform(),
                           gfx::Size(2, 2),
                           gfx::Rect(),
                           gfx::Rect(),
                           false,
                           1.f,
                           SkXfermode::kSrcOver_Mode,
                           0);

  SharedQuadState* shared_state3_in = pass_in->CreateAndAppendSharedQuadState();
  shared_state3_in->SetAll(gfx::Transform(),
                           gfx::Size(3, 3),
                           gfx::Rect(),
                           gfx::Rect(),
                           false,
                           1.f,
                           SkXfermode::kSrcOver_Mode,
                           0);

  // The fourth SharedQuadState is used.
  SharedQuadState* shared_state4_in = pass_in->CreateAndAppendSharedQuadState();
  shared_state4_in->SetAll(gfx::Transform(),
                           gfx::Size(4, 4),
                           gfx::Rect(),
                           gfx::Rect(),
                           false,
                           1.f,
                           SkXfermode::kSrcOver_Mode,
                           0);

  quad = CheckerboardDrawQuad::Create();
  quad->SetAll(shared_state4_in,
               gfx::Rect(10, 10),
               gfx::Rect(10, 10),
               gfx::Rect(10, 10),
               false,
               SK_ColorRED);
  pass_in->quad_list.push_back(quad.PassAs<DrawQuad>());

  // The fifth is not used again.
  SharedQuadState* shared_state5_in = pass_in->CreateAndAppendSharedQuadState();
  shared_state5_in->SetAll(gfx::Transform(),
                           gfx::Size(5, 5),
                           gfx::Rect(),
                           gfx::Rect(),
                           false,
                           1.f,
                           SkXfermode::kSrcOver_Mode,
                           0);

  // 5 SharedQuadStates go in.
  ASSERT_EQ(5u, pass_in->shared_quad_state_list.size());
  ASSERT_EQ(2u, pass_in->quad_list.size());

  DelegatedFrameData frame_in;
  frame_in.render_pass_list.push_back(pass_in.Pass());

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::ParamTraits<DelegatedFrameData>::Write(&msg, frame_in);

  DelegatedFrameData frame_out;
  PickleIterator iter(msg);
  EXPECT_TRUE(
      IPC::ParamTraits<DelegatedFrameData>::Read(&msg, &iter, &frame_out));

  scoped_ptr<RenderPass> pass_out =
      frame_out.render_pass_list.take(frame_out.render_pass_list.begin());

  // 2 SharedQuadStates come out. The first and fourth SharedQuadStates were
  // used by quads, and so serialized. Others were not.
  ASSERT_EQ(2u, pass_out->shared_quad_state_list.size());
  ASSERT_EQ(2u, pass_out->quad_list.size());

  EXPECT_EQ(gfx::Size(1, 1).ToString(),
            pass_out->shared_quad_state_list[0]->content_bounds.ToString());
  EXPECT_EQ(gfx::Size(4, 4).ToString(),
            pass_out->shared_quad_state_list[1]->content_bounds.ToString());
}

TEST_F(CCMessagesTest, Resources) {
  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  gfx::Size arbitrary_size(757, 1281);
  unsigned int arbitrary_uint1 = 71234838;
  unsigned int arbitrary_uint2 = 53589793;

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
  arbitrary_resource1.mailbox_holder.sync_point = arbitrary_uint1;

  TransferableResource arbitrary_resource2;
  arbitrary_resource2.id = 789132;
  arbitrary_resource2.format = cc::RGBA_4444;
  arbitrary_resource2.filter = 47;
  arbitrary_resource2.size = gfx::Size(89123, 23789);
  arbitrary_resource2.mailbox_holder.mailbox.SetName(arbitrary_mailbox2);
  arbitrary_resource2.mailbox_holder.texture_target = GL_TEXTURE_EXTERNAL_OES;
  arbitrary_resource2.mailbox_holder.sync_point = arbitrary_uint2;

  scoped_ptr<RenderPass> renderpass_in = RenderPass::Create();
  renderpass_in->SetNew(
      RenderPass::Id(1, 1), gfx::Rect(), gfx::Rect(), gfx::Transform());

  DelegatedFrameData frame_in;
  frame_in.resource_list.push_back(arbitrary_resource1);
  frame_in.resource_list.push_back(arbitrary_resource2);
  frame_in.render_pass_list.push_back(renderpass_in.Pass());

  IPC::ParamTraits<DelegatedFrameData>::Write(&msg, frame_in);

  DelegatedFrameData frame_out;
  PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ParamTraits<DelegatedFrameData>::Read(&msg,
      &iter, &frame_out));

  ASSERT_EQ(2u, frame_out.resource_list.size());
  Compare(arbitrary_resource1, frame_out.resource_list[0]);
  Compare(arbitrary_resource2, frame_out.resource_list[1]);
}

TEST_F(CCMessagesTest, LargestQuadType) {
  size_t largest = 0;

  bool done = false;
  for (int i = 0; !done; ++i) {
    switch (static_cast<DrawQuad::Material>(i)) {
      case cc::DrawQuad::CHECKERBOARD:
        largest = std::max(largest, sizeof(cc::CheckerboardDrawQuad));
        break;
      case cc::DrawQuad::DEBUG_BORDER:
        largest = std::max(largest, sizeof(cc::DebugBorderDrawQuad));
        break;
      case cc::DrawQuad::IO_SURFACE_CONTENT:
        largest = std::max(largest, sizeof(cc::IOSurfaceDrawQuad));
        break;
      case cc::DrawQuad::PICTURE_CONTENT:
        largest = std::max(largest, sizeof(cc::PictureDrawQuad));
        break;
      case cc::DrawQuad::TEXTURE_CONTENT:
        largest = std::max(largest, sizeof(cc::TextureDrawQuad));
        break;
      case cc::DrawQuad::RENDER_PASS:
        largest = std::max(largest, sizeof(cc::RenderPassDrawQuad));
        break;
      case cc::DrawQuad::SOLID_COLOR:
        largest = std::max(largest, sizeof(cc::SolidColorDrawQuad));
        break;
      case cc::DrawQuad::SURFACE_CONTENT:
        largest = std::max(largest, sizeof(cc::SurfaceDrawQuad));
        break;
      case cc::DrawQuad::TILED_CONTENT:
        largest = std::max(largest, sizeof(cc::TileDrawQuad));
        break;
      case cc::DrawQuad::STREAM_VIDEO_CONTENT:
        largest = std::max(largest, sizeof(cc::StreamVideoDrawQuad));
        break;
      case cc::DrawQuad::YUV_VIDEO_CONTENT:
        largest = std::max(largest, sizeof(cc::YUVVideoDrawQuad));
        break;
      case cc::DrawQuad::INVALID:
        break;
      default:
        done = true;
    }
  }

  // Verify the largest DrawQuad type is RenderPassDrawQuad. If this ever
  // changes, then the ReserveSizeForRenderPassWrite() method needs to be
  // updated as well to use the new largest quad.
  EXPECT_EQ(sizeof(RenderPassDrawQuad), largest);
}

TEST_F(CCMessagesTest, SoftwareFrameData) {
  cc::SoftwareFrameData frame_in;
  frame_in.id = 3;
  frame_in.size = gfx::Size(40, 20);
  frame_in.damage_rect = gfx::Rect(5, 18, 31, 44);
  frame_in.bitmap_id = cc::SharedBitmap::GenerateId();

  // Write the frame.
  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::ParamTraits<cc::SoftwareFrameData>::Write(&msg, frame_in);

  // Read the frame.
  cc::SoftwareFrameData frame_out;
  PickleIterator iter(msg);
  EXPECT_TRUE(
      IPC::ParamTraits<SoftwareFrameData>::Read(&msg, &iter, &frame_out));
  EXPECT_EQ(frame_in.id, frame_out.id);
  EXPECT_EQ(frame_in.size.ToString(), frame_out.size.ToString());
  EXPECT_EQ(frame_in.damage_rect.ToString(), frame_out.damage_rect.ToString());
  EXPECT_EQ(frame_in.bitmap_id, frame_out.bitmap_id);
}

TEST_F(CCMessagesTest, SoftwareFrameDataMaxInt) {
  SoftwareFrameData frame_in;
  frame_in.id = 3;
  frame_in.size = gfx::Size(40, 20);
  frame_in.damage_rect = gfx::Rect(5, 18, 31, 44);
  frame_in.bitmap_id = cc::SharedBitmap::GenerateId();

  // Write the SoftwareFrameData by hand, make sure it works.
  {
    IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
    IPC::WriteParam(&msg, frame_in.id);
    IPC::WriteParam(&msg, frame_in.size);
    IPC::WriteParam(&msg, frame_in.damage_rect);
    IPC::WriteParam(&msg, frame_in.bitmap_id);
    SoftwareFrameData frame_out;
    PickleIterator iter(msg);
    EXPECT_TRUE(
        IPC::ParamTraits<SoftwareFrameData>::Read(&msg, &iter, &frame_out));
  }

  // The size of the frame may overflow when multiplied together.
  int max = std::numeric_limits<int>::max();
  frame_in.size = gfx::Size(max, max);

  // If size_t is larger than int, then int*int*4 can always fit in size_t.
  bool expect_read = sizeof(size_t) >= sizeof(int) * 2;

  // Write the SoftwareFrameData with the MaxInt size, if it causes overflow it
  // should fail.
  {
    IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
    IPC::WriteParam(&msg, frame_in.id);
    IPC::WriteParam(&msg, frame_in.size);
    IPC::WriteParam(&msg, frame_in.damage_rect);
    IPC::WriteParam(&msg, frame_in.bitmap_id);
    SoftwareFrameData frame_out;
    PickleIterator iter(msg);
    EXPECT_EQ(
        expect_read,
        IPC::ParamTraits<SoftwareFrameData>::Read(&msg, &iter, &frame_out));
  }
}

}  // namespace
}  // namespace content
