// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "base/basictypes.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/video_layer.h"
#include "cc/layers/video_layer_impl.h"
#include "cc/test/fake_video_frame_provider.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/damage_tracker.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {
namespace {

// These tests deal with compositing video.
class LayerTreeHostVideoTest : public LayerTreeTest {};

class LayerTreeHostVideoTestSetNeedsDisplay
    : public LayerTreeHostVideoTest {
 public:
  virtual void SetupTree() OVERRIDE {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(10, 10));
    root->SetIsDrawable(true);

    scoped_refptr<VideoLayer> video = VideoLayer::Create(
        &video_frame_provider_);
    video->SetPosition(gfx::PointF(3.f, 3.f));
    video->SetBounds(gfx::Size(4, 4));
    video->SetIsDrawable(true);
    root->AddChild(video);

    layer_tree_host()->SetRootLayer(root);
    layer_tree_host()->SetDeviceScaleFactor(2.f);
    LayerTreeHostVideoTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    num_draws_ = 0;
    PostSetNeedsCommitToMainThread();
  }

  virtual DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                           LayerTreeHostImpl::FrameData* frame,
                                           DrawResult draw_result) OVERRIDE {
    LayerImpl* root_layer = host_impl->active_tree()->root_layer();
    RenderSurfaceImpl* root_surface = root_layer->render_surface();
    gfx::RectF damage_rect =
        root_surface->damage_tracker()->current_damage_rect();

    switch (num_draws_) {
      case 0:
        // First frame the whole viewport is damaged.
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 20.f, 20.f).ToString(),
                  damage_rect.ToString());
        break;
      case 1:
        // Second frame the video layer is damaged.
        EXPECT_EQ(gfx::RectF(6.f, 6.f, 8.f, 8.f).ToString(),
                  damage_rect.ToString());
        EndTest();
        break;
    }

    EXPECT_EQ(DRAW_SUCCESS, draw_result);
    return draw_result;
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    VideoLayerImpl* video = static_cast<VideoLayerImpl*>(
        host_impl->active_tree()->root_layer()->children()[0]);

    if (num_draws_ == 0)
      video->SetNeedsRedraw();

    ++num_draws_;
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(2, num_draws_);
  }

 private:
  int num_draws_;

  FakeVideoFrameProvider video_frame_provider_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostVideoTestSetNeedsDisplay);

}  // namespace
}  // namespace cc
