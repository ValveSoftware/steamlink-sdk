// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_aggregator_test_helpers.h"

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "cc/layers/append_quads_data.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/surfaces/surface.h"
#include "cc/test/render_pass_test_common.h"
#include "cc/test/render_pass_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkXfermode.h"

namespace cc {
namespace test {

void AddTestSurfaceQuad(TestRenderPass* pass,
                        const gfx::Size& surface_size,
                        SurfaceId surface_id) {
  gfx::Transform content_to_target_transform;
  gfx::Size content_bounds = surface_size;
  gfx::Rect visible_content_rect = gfx::Rect(surface_size);
  gfx::Rect clip_rect = gfx::Rect(surface_size);
  bool is_clipped = false;
  float opacity = 1.0;
  SkXfermode::Mode blend_mode = SkXfermode::kSrcOver_Mode;

  SharedQuadState* shared_quad_state = pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(content_to_target_transform,
                            content_bounds,
                            visible_content_rect,
                            clip_rect,
                            is_clipped,
                            opacity,
                            blend_mode,
                            0);

  scoped_ptr<SurfaceDrawQuad> surface_quad = SurfaceDrawQuad::Create();
  gfx::Rect quad_rect = gfx::Rect(surface_size);
  surface_quad->SetNew(pass->shared_quad_state_list.back(),
                       gfx::Rect(surface_size),
                       gfx::Rect(surface_size),
                       surface_id);
  pass->quad_list.push_back(surface_quad.PassAs<DrawQuad>());
}
void AddTestRenderPassQuad(TestRenderPass* pass,
                           RenderPass::Id render_pass_id) {
  gfx::Rect output_rect = gfx::Rect(0, 0, 5, 5);
  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(),
                       output_rect.size(),
                       output_rect,
                       output_rect,
                       false,
                       1,
                       SkXfermode::kSrcOver_Mode,
                       0);
  scoped_ptr<RenderPassDrawQuad> quad = RenderPassDrawQuad::Create();
  quad->SetNew(shared_state,
               output_rect,
               output_rect,
               render_pass_id,
               false,
               0,
               output_rect,
               gfx::RectF(),
               FilterOperations(),
               FilterOperations());
  pass->AppendDrawQuad(quad.PassAs<DrawQuad>());
}

void AddQuadInPass(TestRenderPass* pass, Quad desc) {
  switch (desc.material) {
    case DrawQuad::SOLID_COLOR:
      AddQuad(pass, gfx::Rect(0, 0, 5, 5), desc.color);
      break;
    case DrawQuad::SURFACE_CONTENT:
      AddTestSurfaceQuad(pass, gfx::Size(5, 5), desc.surface_id);
      break;
    case DrawQuad::RENDER_PASS:
      AddTestRenderPassQuad(pass, desc.render_pass_id);
      break;
    default:
      NOTREACHED();
  }
}

void AddPasses(RenderPassList* pass_list,
               const gfx::Rect& output_rect,
               Pass* passes,
               size_t pass_count) {
  gfx::Transform root_transform;
  for (size_t i = 0; i < pass_count; ++i) {
    Pass pass = passes[i];
    TestRenderPass* test_pass =
        AddRenderPass(pass_list, pass.id, output_rect, root_transform);
    for (size_t j = 0; j < pass.quad_count; ++j) {
      AddQuadInPass(test_pass, pass.quads[j]);
    }
  }
}

void TestQuadMatchesExpectations(Quad expected_quad, DrawQuad* quad) {
  switch (expected_quad.material) {
    case DrawQuad::SOLID_COLOR: {
      ASSERT_EQ(DrawQuad::SOLID_COLOR, quad->material);

      const SolidColorDrawQuad* solid_color_quad =
          SolidColorDrawQuad::MaterialCast(quad);

      EXPECT_EQ(expected_quad.color, solid_color_quad->color);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void TestPassMatchesExpectations(Pass expected_pass, RenderPass* pass) {
  ASSERT_EQ(expected_pass.quad_count, pass->quad_list.size());
  for (size_t i = 0u; i < pass->quad_list.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Quad number %" PRIuS, i));
    TestQuadMatchesExpectations(expected_pass.quads[i], pass->quad_list.at(i));
  }
}

void TestPassesMatchExpectations(Pass* expected_passes,
                                 size_t expected_pass_count,
                                 RenderPassList* passes) {
  ASSERT_EQ(expected_pass_count, passes->size());

  for (size_t i = 0; i < passes->size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Pass number %" PRIuS, i));
    RenderPass* pass = passes->at(i);
    TestPassMatchesExpectations(expected_passes[i], pass);
  }
}

void SubmitFrame(Pass* passes, size_t pass_count, Surface* surface) {
  RenderPassList pass_list;
  AddPasses(&pass_list, gfx::Rect(surface->size()), passes, pass_count);

  scoped_ptr<DelegatedFrameData> frame_data(new DelegatedFrameData);
  pass_list.swap(frame_data->render_pass_list);

  scoped_ptr<CompositorFrame> frame(new CompositorFrame);
  frame->delegated_frame_data = frame_data.Pass();

  surface->QueueFrame(frame.Pass());
}

void QueuePassAsFrame(scoped_ptr<RenderPass> pass, Surface* surface) {
  scoped_ptr<DelegatedFrameData> delegated_frame_data(new DelegatedFrameData);
  delegated_frame_data->render_pass_list.push_back(pass.Pass());

  scoped_ptr<CompositorFrame> child_frame(new CompositorFrame);
  child_frame->delegated_frame_data = delegated_frame_data.Pass();

  surface->QueueFrame(child_frame.Pass());
}

}  // namespace test
}  // namespace cc
