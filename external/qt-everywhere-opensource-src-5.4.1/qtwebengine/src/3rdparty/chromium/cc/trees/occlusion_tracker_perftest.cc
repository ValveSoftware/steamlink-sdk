// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/occlusion_tracker.h"

#include "base/time/time.h"
#include "cc/debug/lap_timer.h"
#include "cc/layers/layer_iterator.h"
#include "cc/layers/solid_color_layer_impl.h"
#include "cc/test/fake_layer_tree_host_impl_client.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/fake_rendering_stats_instrumentation.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace cc {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class OcclusionTrackerPerfTest : public testing::Test {
 public:
  OcclusionTrackerPerfTest()
      : timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval),
        impl_(&proxy_) {}
  void CreateHost() {
    LayerTreeSettings settings;
    shared_bitmap_manager_.reset(new TestSharedBitmapManager());
    host_impl_ = LayerTreeHostImpl::Create(
        settings, &client_, &proxy_, &stats_, shared_bitmap_manager_.get(), 1);
    host_impl_->InitializeRenderer(
        FakeOutputSurface::Create3d().PassAs<OutputSurface>());

    scoped_ptr<LayerImpl> root_layer = LayerImpl::Create(active_tree(), 1);
    active_tree()->SetRootLayer(root_layer.Pass());
  }

  LayerTreeImpl* active_tree() { return host_impl_->active_tree(); }

  void SetTestName(const std::string& name) { test_name_ = name; }

  void PrintResults() {
    CHECK(!test_name_.empty()) << "Must SetTestName() before AfterTest().";
    perf_test::PrintResult("occlusion_tracker_time",
                           "",
                           test_name_,
                           1000 * timer_.MsPerLap(),
                           "us",
                           true);
  }

 protected:
  LapTimer timer_;
  std::string test_name_;
  FakeLayerTreeHostImplClient client_;
  FakeProxy proxy_;
  DebugScopedSetImplThread impl_;
  FakeRenderingStatsInstrumentation stats_;
  scoped_ptr<SharedBitmapManager> shared_bitmap_manager_;
  scoped_ptr<LayerTreeHostImpl> host_impl_;
};

TEST_F(OcclusionTrackerPerfTest, UnoccludedContentRect_FullyOccluded) {
  SetTestName("unoccluded_content_rect_fully_occluded");

  gfx::Rect viewport_rect(768, 1038);
  OcclusionTracker<LayerImpl> tracker(viewport_rect);

  CreateHost();
  host_impl_->SetViewportSize(viewport_rect.size());

  scoped_ptr<SolidColorLayerImpl> opaque_layer =
      SolidColorLayerImpl::Create(active_tree(), 2);
  opaque_layer->SetBackgroundColor(SK_ColorRED);
  opaque_layer->SetContentsOpaque(true);
  opaque_layer->SetDrawsContent(true);
  opaque_layer->SetBounds(viewport_rect.size());
  opaque_layer->SetContentBounds(viewport_rect.size());
  active_tree()->root_layer()->AddChild(opaque_layer.PassAs<LayerImpl>());

  active_tree()->UpdateDrawProperties();
  const LayerImplList& rsll = active_tree()->RenderSurfaceLayerList();
  ASSERT_EQ(1u, rsll.size());
  EXPECT_EQ(1u, rsll[0]->render_surface()->layer_list().size());

  LayerIterator<LayerImpl> begin = LayerIterator<LayerImpl>::Begin(&rsll);
  LayerIterator<LayerImpl> end = LayerIterator<LayerImpl>::End(&rsll);

  LayerIteratorPosition<LayerImpl> pos = begin;

  // The opaque_layer adds occlusion over the whole viewport.
  tracker.EnterLayer(pos);
  tracker.LeaveLayer(pos);

  gfx::Transform transform_to_target;
  transform_to_target.Translate(0, 96);

  do {
    for (int x = 0; x < viewport_rect.width(); x += 256) {
      for (int y = 0; y < viewport_rect.height(); y += 256) {
        gfx::Rect query_content_rect(x, y, 256, 256);
        gfx::Rect unoccluded = tracker.UnoccludedContentRect(
            query_content_rect, transform_to_target);
        // Sanity test that we're not hitting early outs.
        bool expect_empty =
            query_content_rect.right() <= viewport_rect.width() &&
            query_content_rect.bottom() + 96 <= viewport_rect.height();
        CHECK_EQ(expect_empty, unoccluded.IsEmpty())
            << query_content_rect.ToString();
      }
    }

    timer_.NextLap();
  } while (!timer_.HasTimeLimitExpired());

  ++begin;
  LayerIteratorPosition<LayerImpl> next = begin;
  EXPECT_EQ(active_tree()->root_layer(), next.current_layer);

  ++begin;
  EXPECT_EQ(end, begin);

  PrintResults();
}

TEST_F(OcclusionTrackerPerfTest, UnoccludedContentRect_10OpaqueLayers) {
  static const int kNumOpaqueLayers = 10;
  SetTestName("unoccluded_content_rect_10_opaque_layers");

  gfx::Rect viewport_rect(768, 1038);
  OcclusionTracker<LayerImpl> tracker(viewport_rect);

  CreateHost();
  host_impl_->SetViewportSize(viewport_rect.size());

  for (int i = 0; i < kNumOpaqueLayers; ++i) {
    scoped_ptr<SolidColorLayerImpl> opaque_layer =
        SolidColorLayerImpl::Create(active_tree(), 2 + i);
    opaque_layer->SetBackgroundColor(SK_ColorRED);
    opaque_layer->SetContentsOpaque(true);
    opaque_layer->SetDrawsContent(true);
    opaque_layer->SetBounds(
        gfx::Size(viewport_rect.width() / 2, viewport_rect.height() / 2));
    opaque_layer->SetContentBounds(
        gfx::Size(viewport_rect.width() / 2, viewport_rect.height() / 2));
    opaque_layer->SetPosition(gfx::Point(i, i));
    active_tree()->root_layer()->AddChild(opaque_layer.PassAs<LayerImpl>());
  }

  active_tree()->UpdateDrawProperties();
  const LayerImplList& rsll = active_tree()->RenderSurfaceLayerList();
  ASSERT_EQ(1u, rsll.size());
  EXPECT_EQ(static_cast<size_t>(kNumOpaqueLayers),
            rsll[0]->render_surface()->layer_list().size());

  LayerIterator<LayerImpl> begin = LayerIterator<LayerImpl>::Begin(&rsll);
  LayerIterator<LayerImpl> end = LayerIterator<LayerImpl>::End(&rsll);

  // The opaque_layers add occlusion.
  for (int i = 0; i < kNumOpaqueLayers - 1; ++i) {
    LayerIteratorPosition<LayerImpl> pos = begin;
    tracker.EnterLayer(pos);
    tracker.LeaveLayer(pos);
    ++begin;
  }
  LayerIteratorPosition<LayerImpl> pos = begin;
  tracker.EnterLayer(pos);
  tracker.LeaveLayer(pos);

  gfx::Transform transform_to_target;
  transform_to_target.Translate(0, 96);

  do {
    for (int x = 0; x < viewport_rect.width(); x += 256) {
      for (int y = 0; y < viewport_rect.height(); y += 256) {
        gfx::Rect query_content_rect(x, y, 256, 256);
        gfx::Rect unoccluded = tracker.UnoccludedContentRect(
            query_content_rect, transform_to_target);
      }
    }

    timer_.NextLap();
  } while (!timer_.HasTimeLimitExpired());

  ++begin;
  LayerIteratorPosition<LayerImpl> next = begin;
  EXPECT_EQ(active_tree()->root_layer(), next.current_layer);

  ++begin;
  EXPECT_EQ(end, begin);

  PrintResults();
}

}  // namespace
}  // namespace cc
