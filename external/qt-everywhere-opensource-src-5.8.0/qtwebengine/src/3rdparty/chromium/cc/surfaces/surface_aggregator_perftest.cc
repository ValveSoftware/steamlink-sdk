// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "cc/debug/lap_timer.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/surfaces/surface_aggregator.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_resource_provider.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace cc {
namespace {

class EmptySurfaceFactoryClient : public SurfaceFactoryClient {
 public:
  void ReturnResources(const ReturnedResourceArray& resources) override {}
  void SetBeginFrameSource(BeginFrameSource* begin_frame_source) override {}
};

class SurfaceAggregatorPerfTest : public testing::Test {
 public:
  SurfaceAggregatorPerfTest() : factory_(&manager_, &empty_client_) {
    output_surface_ = FakeOutputSurface::CreateSoftware(
        base::WrapUnique(new SoftwareOutputDevice));
    output_surface_->BindToClient(&output_surface_client_);
    shared_bitmap_manager_.reset(new TestSharedBitmapManager);

    resource_provider_ = FakeResourceProvider::Create(
        output_surface_.get(), shared_bitmap_manager_.get());
  }

  void RunTest(int num_surfaces,
               int num_textures,
               float opacity,
               bool optimize_damage,
               bool full_damage,
               const std::string& name) {
    aggregator_.reset(new SurfaceAggregator(&manager_, resource_provider_.get(),
                                            optimize_damage));
    for (int i = 1; i <= num_surfaces; i++) {
      factory_.Create(SurfaceId(0, i, 0));
      std::unique_ptr<RenderPass> pass(RenderPass::Create());
      std::unique_ptr<DelegatedFrameData> frame_data(new DelegatedFrameData);

      SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
      for (int j = 0; j < num_textures; j++) {
        TransferableResource resource;
        resource.id = j;
        resource.is_software = true;
        frame_data->resource_list.push_back(resource);

        TextureDrawQuad* quad =
            pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
        const gfx::Rect rect(0, 0, 1, 1);
        const gfx::Rect opaque_rect;
        // Half of rects should be visible with partial damage.
        gfx::Rect visible_rect =
            j % 2 == 0 ? gfx::Rect(0, 0, 1, 1) : gfx::Rect(1, 1, 1, 1);
        bool needs_blending = false;
        bool premultiplied_alpha = false;
        const gfx::PointF uv_top_left;
        const gfx::PointF uv_bottom_right;
        SkColor background_color = SK_ColorGREEN;
        const float vertex_opacity[4] = {0.f, 0.f, 1.f, 1.f};
        bool flipped = false;
        bool nearest_neighbor = false;
        quad->SetAll(sqs, rect, opaque_rect, visible_rect, needs_blending, j,
                     gfx::Size(), premultiplied_alpha, uv_top_left,
                     uv_bottom_right, background_color, vertex_opacity, flipped,
                     nearest_neighbor, false);
      }
      sqs = pass->CreateAndAppendSharedQuadState();
      sqs->opacity = opacity;
      if (i > 1) {
        SurfaceDrawQuad* surface_quad =
            pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
        surface_quad->SetNew(sqs, gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1),
                             SurfaceId(0, i - 1, 0));
      }

      frame_data->render_pass_list.push_back(std::move(pass));
      CompositorFrame frame;
      frame.delegated_frame_data = std::move(frame_data);
      factory_.SubmitCompositorFrame(SurfaceId(0, i, 0), std::move(frame),
                                     SurfaceFactory::DrawCallback());
    }

    factory_.Create(SurfaceId(0, num_surfaces + 1, 0));
    timer_.Reset();
    do {
      std::unique_ptr<RenderPass> pass(RenderPass::Create());
      std::unique_ptr<DelegatedFrameData> frame_data(new DelegatedFrameData);

      SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
      SurfaceDrawQuad* surface_quad =
          pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
      surface_quad->SetNew(sqs, gfx::Rect(0, 0, 100, 100),
                           gfx::Rect(0, 0, 100, 100),
                           SurfaceId(0, num_surfaces, 0));

      if (full_damage)
        pass->damage_rect = gfx::Rect(0, 0, 100, 100);
      else
        pass->damage_rect = gfx::Rect(0, 0, 1, 1);

      frame_data->render_pass_list.push_back(std::move(pass));
      CompositorFrame frame;
      frame.delegated_frame_data = std::move(frame_data);
      factory_.SubmitCompositorFrame(SurfaceId(0, num_surfaces + 1, 0),
                                     std::move(frame),
                                     SurfaceFactory::DrawCallback());

      CompositorFrame aggregated =
          aggregator_->Aggregate(SurfaceId(0, num_surfaces + 1, 0));
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("aggregator_speed", "", name, timer_.LapsPerSecond(),
                           "runs/s", true);

    factory_.Destroy(SurfaceId(0, num_surfaces + 1, 0));
    for (int i = 1; i <= num_surfaces; i++)
      factory_.Destroy(SurfaceId(0, i, 0));
  }

 protected:
  SurfaceManager manager_;
  EmptySurfaceFactoryClient empty_client_;
  SurfaceFactory factory_;
  FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<OutputSurface> output_surface_;
  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager_;
  std::unique_ptr<ResourceProvider> resource_provider_;
  std::unique_ptr<SurfaceAggregator> aggregator_;
  LapTimer timer_;
};

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesOpaque) {
  RunTest(20, 100, 1.f, false, true, "many_surfaces_opaque");
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesTransparent) {
  RunTest(20, 100, .5f, false, true, "many_surfaces_transparent");
}

TEST_F(SurfaceAggregatorPerfTest, FewSurfaces) {
  RunTest(3, 1000, 1.f, false, true, "few_surfaces");
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesOpaqueDamageCalc) {
  RunTest(20, 100, 1.f, true, true, "many_surfaces_opaque_damage_calc");
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesTransparentDamageCalc) {
  RunTest(20, 100, .5f, true, true, "many_surfaces_transparent_damage_calc");
}

TEST_F(SurfaceAggregatorPerfTest, FewSurfacesDamageCalc) {
  RunTest(3, 1000, 1.f, true, true, "few_surfaces_damage_calc");
}

TEST_F(SurfaceAggregatorPerfTest, FewSurfacesAggregateDamaged) {
  RunTest(3, 1000, 1.f, true, false, "few_surfaces_aggregate_damaged");
}

}  // namespace
}  // namespace cc
