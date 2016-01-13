// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/render_pass.h"

#include "cc/base/math_util.h"
#include "cc/base/scoped_ptr_vector.h"
#include "cc/output/copy_output_request.h"
#include "cc/quads/checkerboard_draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/render_pass_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "ui/gfx/transform.h"

using cc::TestRenderPass;

namespace cc {
namespace {

struct RenderPassSize {
  // If you add a new field to this class, make sure to add it to the
  // Copy() tests.
  RenderPass::Id id;
  QuadList quad_list;
  SharedQuadStateList shared_quad_state_list;
  gfx::Transform transform_to_root_target;
  gfx::Rect output_rect;
  gfx::Rect damage_rect;
  bool has_transparent_background;
  ScopedPtrVector<CopyOutputRequest> copy_callbacks;
};

static void CompareRenderPassLists(const RenderPassList& expected_list,
                                   const RenderPassList& actual_list) {
  EXPECT_EQ(expected_list.size(), actual_list.size());
  for (size_t i = 0; i < actual_list.size(); ++i) {
    RenderPass* expected = expected_list[i];
    RenderPass* actual = actual_list[i];

    EXPECT_EQ(expected->id, actual->id);
    EXPECT_RECT_EQ(expected->output_rect, actual->output_rect);
    EXPECT_EQ(expected->transform_to_root_target,
              actual->transform_to_root_target);
    EXPECT_RECT_EQ(expected->damage_rect, actual->damage_rect);
    EXPECT_EQ(expected->has_transparent_background,
              actual->has_transparent_background);

    EXPECT_EQ(expected->shared_quad_state_list.size(),
              actual->shared_quad_state_list.size());
    EXPECT_EQ(expected->quad_list.size(), actual->quad_list.size());

    for (size_t i = 0; i < expected->quad_list.size(); ++i) {
      EXPECT_EQ(expected->quad_list[i]->rect.ToString(),
                actual->quad_list[i]->rect.ToString());
      EXPECT_EQ(
          expected->quad_list[i]->shared_quad_state->content_bounds.ToString(),
          actual->quad_list[i]->shared_quad_state->content_bounds.ToString());
    }
  }
}

TEST(RenderPassTest, CopyShouldBeIdenticalExceptIdAndQuads) {
  RenderPass::Id id(3, 2);
  gfx::Rect output_rect(45, 22, 120, 13);
  gfx::Transform transform_to_root =
      gfx::Transform(1.0, 0.5, 0.5, -0.5, -1.0, 0.0);
  gfx::Rect damage_rect(56, 123, 19, 43);
  bool has_transparent_background = true;

  scoped_ptr<TestRenderPass> pass = TestRenderPass::Create();
  pass->SetAll(id,
               output_rect,
               damage_rect,
               transform_to_root,
               has_transparent_background);
  pass->copy_requests.push_back(CopyOutputRequest::CreateEmptyRequest());

  // Stick a quad in the pass, this should not get copied.
  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(),
                       gfx::Size(),
                       gfx::Rect(),
                       gfx::Rect(),
                       false,
                       1,
                       SkXfermode::kSrcOver_Mode,
                       0);

  scoped_ptr<CheckerboardDrawQuad> checkerboard_quad =
      CheckerboardDrawQuad::Create();
  checkerboard_quad->SetNew(
      pass->shared_quad_state_list.back(), gfx::Rect(), gfx::Rect(), SkColor());
  pass->quad_list.push_back(checkerboard_quad.PassAs<DrawQuad>());

  RenderPass::Id new_id(63, 4);

  scoped_ptr<RenderPass> copy = pass->Copy(new_id);
  EXPECT_EQ(new_id, copy->id);
  EXPECT_RECT_EQ(pass->output_rect, copy->output_rect);
  EXPECT_EQ(pass->transform_to_root_target, copy->transform_to_root_target);
  EXPECT_RECT_EQ(pass->damage_rect, copy->damage_rect);
  EXPECT_EQ(pass->has_transparent_background, copy->has_transparent_background);
  EXPECT_EQ(0u, copy->quad_list.size());

  // The copy request should not be copied/duplicated.
  EXPECT_EQ(1u, pass->copy_requests.size());
  EXPECT_EQ(0u, copy->copy_requests.size());

  EXPECT_EQ(sizeof(RenderPassSize), sizeof(RenderPass));
}

TEST(RenderPassTest, CopyAllShouldBeIdentical) {
  RenderPassList pass_list;

  RenderPass::Id id(3, 2);
  gfx::Rect output_rect(45, 22, 120, 13);
  gfx::Transform transform_to_root =
      gfx::Transform(1.0, 0.5, 0.5, -0.5, -1.0, 0.0);
  gfx::Rect damage_rect(56, 123, 19, 43);
  bool has_transparent_background = true;

  scoped_ptr<TestRenderPass> pass = TestRenderPass::Create();
  pass->SetAll(id,
               output_rect,
               damage_rect,
               transform_to_root,
               has_transparent_background);

  // Two quads using one shared state.
  SharedQuadState* shared_state1 = pass->CreateAndAppendSharedQuadState();
  shared_state1->SetAll(gfx::Transform(),
                        gfx::Size(1, 1),
                        gfx::Rect(),
                        gfx::Rect(),
                        false,
                        1,
                        SkXfermode::kSrcOver_Mode,
                        0);

  scoped_ptr<CheckerboardDrawQuad> checkerboard_quad1 =
      CheckerboardDrawQuad::Create();
  checkerboard_quad1->SetNew(pass->shared_quad_state_list.back(),
                             gfx::Rect(1, 1, 1, 1),
                             gfx::Rect(1, 1, 1, 1),
                             SkColor());
  pass->quad_list.push_back(checkerboard_quad1.PassAs<DrawQuad>());

  scoped_ptr<CheckerboardDrawQuad> checkerboard_quad2 =
      CheckerboardDrawQuad::Create();
  checkerboard_quad2->SetNew(pass->shared_quad_state_list.back(),
                             gfx::Rect(2, 2, 2, 2),
                             gfx::Rect(2, 2, 2, 2),
                             SkColor());
  pass->quad_list.push_back(checkerboard_quad2.PassAs<DrawQuad>());

  // And two quads using another shared state.
  SharedQuadState* shared_state2 = pass->CreateAndAppendSharedQuadState();
  shared_state2->SetAll(gfx::Transform(),
                        gfx::Size(2, 2),
                        gfx::Rect(),
                        gfx::Rect(),
                        false,
                        1,
                        SkXfermode::kSrcOver_Mode,
                        0);

  scoped_ptr<CheckerboardDrawQuad> checkerboard_quad3 =
      CheckerboardDrawQuad::Create();
  checkerboard_quad3->SetNew(pass->shared_quad_state_list.back(),
                             gfx::Rect(3, 3, 3, 3),
                             gfx::Rect(3, 3, 3, 3),
                             SkColor());
  pass->quad_list.push_back(checkerboard_quad3.PassAs<DrawQuad>());

  scoped_ptr<CheckerboardDrawQuad> checkerboard_quad4 =
      CheckerboardDrawQuad::Create();
  checkerboard_quad4->SetNew(pass->shared_quad_state_list.back(),
                             gfx::Rect(4, 4, 4, 4),
                             gfx::Rect(4, 4, 4, 4),
                             SkColor());
  pass->quad_list.push_back(checkerboard_quad4.PassAs<DrawQuad>());

  // A second render pass with a quad.
  RenderPass::Id contrib_id(4, 1);
  gfx::Rect contrib_output_rect(10, 15, 12, 17);
  gfx::Transform contrib_transform_to_root =
      gfx::Transform(1.0, 0.5, 0.5, -0.5, -1.0, 0.0);
  gfx::Rect contrib_damage_rect(11, 16, 10, 15);
  bool contrib_has_transparent_background = true;

  scoped_ptr<TestRenderPass> contrib = TestRenderPass::Create();
  contrib->SetAll(contrib_id,
                  contrib_output_rect,
                  contrib_damage_rect,
                  contrib_transform_to_root,
                  contrib_has_transparent_background);

  SharedQuadState* contrib_shared_state =
      contrib->CreateAndAppendSharedQuadState();
  contrib_shared_state->SetAll(gfx::Transform(),
                               gfx::Size(2, 2),
                               gfx::Rect(),
                               gfx::Rect(),
                               false,
                               1,
                               SkXfermode::kSrcOver_Mode,
                               0);

  scoped_ptr<CheckerboardDrawQuad> contrib_quad =
      CheckerboardDrawQuad::Create();
  contrib_quad->SetNew(contrib->shared_quad_state_list.back(),
                       gfx::Rect(3, 3, 3, 3),
                       gfx::Rect(3, 3, 3, 3),
                       SkColor());
  contrib->quad_list.push_back(contrib_quad.PassAs<DrawQuad>());

  // And a RenderPassDrawQuad for the contributing pass.
  scoped_ptr<RenderPassDrawQuad> pass_quad = RenderPassDrawQuad::Create();
  pass_quad->SetNew(pass->shared_quad_state_list.back(),
                    contrib_output_rect,
                    contrib_output_rect,
                    contrib_id,
                    false,  // is_replica
                    0,      // mask_resource_id
                    contrib_damage_rect,
                    gfx::RectF(),  // mask_uv_rect
                    FilterOperations(),
                    FilterOperations());
  pass->quad_list.push_back(pass_quad.PassAs<DrawQuad>());

  pass_list.push_back(pass.PassAs<RenderPass>());
  pass_list.push_back(contrib.PassAs<RenderPass>());

  // Make a copy with CopyAll().
  RenderPassList copy_list;
  RenderPass::CopyAll(pass_list, &copy_list);

  CompareRenderPassLists(pass_list, copy_list);
}

TEST(RenderPassTest, CopyAllWithCulledQuads) {
  RenderPassList pass_list;

  RenderPass::Id id(3, 2);
  gfx::Rect output_rect(45, 22, 120, 13);
  gfx::Transform transform_to_root =
      gfx::Transform(1.0, 0.5, 0.5, -0.5, -1.0, 0.0);
  gfx::Rect damage_rect(56, 123, 19, 43);
  bool has_transparent_background = true;

  scoped_ptr<TestRenderPass> pass = TestRenderPass::Create();
  pass->SetAll(id,
               output_rect,
               damage_rect,
               transform_to_root,
               has_transparent_background);

  // A shared state with a quad.
  SharedQuadState* shared_state1 = pass->CreateAndAppendSharedQuadState();
  shared_state1->SetAll(gfx::Transform(),
                        gfx::Size(1, 1),
                        gfx::Rect(),
                        gfx::Rect(),
                        false,
                        1,
                        SkXfermode::kSrcOver_Mode,
                        0);

  scoped_ptr<CheckerboardDrawQuad> checkerboard_quad1 =
      CheckerboardDrawQuad::Create();
  checkerboard_quad1->SetNew(pass->shared_quad_state_list.back(),
                             gfx::Rect(1, 1, 1, 1),
                             gfx::Rect(1, 1, 1, 1),
                             SkColor());
  pass->quad_list.push_back(checkerboard_quad1.PassAs<DrawQuad>());

  // A shared state with no quads, they were culled.
  SharedQuadState* shared_state2 = pass->CreateAndAppendSharedQuadState();
  shared_state2->SetAll(gfx::Transform(),
                        gfx::Size(2, 2),
                        gfx::Rect(),
                        gfx::Rect(),
                        false,
                        1,
                        SkXfermode::kSrcOver_Mode,
                        0);

  // A second shared state with no quads.
  SharedQuadState* shared_state3 = pass->CreateAndAppendSharedQuadState();
  shared_state3->SetAll(gfx::Transform(),
                        gfx::Size(2, 2),
                        gfx::Rect(),
                        gfx::Rect(),
                        false,
                        1,
                        SkXfermode::kSrcOver_Mode,
                        0);

  // A last shared state with a quad again.
  SharedQuadState* shared_state4 = pass->CreateAndAppendSharedQuadState();
  shared_state4->SetAll(gfx::Transform(),
                        gfx::Size(2, 2),
                        gfx::Rect(),
                        gfx::Rect(),
                        false,
                        1,
                        SkXfermode::kSrcOver_Mode,
                        0);

  scoped_ptr<CheckerboardDrawQuad> checkerboard_quad2 =
      CheckerboardDrawQuad::Create();
  checkerboard_quad2->SetNew(pass->shared_quad_state_list.back(),
                             gfx::Rect(3, 3, 3, 3),
                             gfx::Rect(3, 3, 3, 3),
                             SkColor());
  pass->quad_list.push_back(checkerboard_quad2.PassAs<DrawQuad>());

  pass_list.push_back(pass.PassAs<RenderPass>());

  // Make a copy with CopyAll().
  RenderPassList copy_list;
  RenderPass::CopyAll(pass_list, &copy_list);

  CompareRenderPassLists(pass_list, copy_list);
}

}  // namespace
}  // namespace cc
