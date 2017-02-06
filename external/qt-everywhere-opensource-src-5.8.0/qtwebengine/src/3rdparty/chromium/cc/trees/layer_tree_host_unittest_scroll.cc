// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/animation/animation_host.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/picture_layer.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {
namespace {

std::unique_ptr<ScrollState> BeginState(const gfx::Point& point) {
  ScrollStateData scroll_state_data;
  scroll_state_data.is_beginning = true;
  scroll_state_data.position_x = point.x();
  scroll_state_data.position_y = point.y();
  std::unique_ptr<ScrollState> scroll_state(new ScrollState(scroll_state_data));
  return scroll_state;
}

std::unique_ptr<ScrollState> UpdateState(const gfx::Point& point,
                                         const gfx::Vector2dF& delta) {
  ScrollStateData scroll_state_data;
  scroll_state_data.delta_x = delta.x();
  scroll_state_data.delta_y = delta.y();
  scroll_state_data.position_x = point.x();
  scroll_state_data.position_y = point.y();
  std::unique_ptr<ScrollState> scroll_state(new ScrollState(scroll_state_data));
  return scroll_state;
}

std::unique_ptr<ScrollState> EndState() {
  ScrollStateData scroll_state_data;
  scroll_state_data.is_ending = true;
  std::unique_ptr<ScrollState> scroll_state(new ScrollState(scroll_state_data));
  return scroll_state;
}

static ScrollTree* ScrollTreeForLayer(LayerImpl* layer_impl) {
  return &layer_impl->layer_tree_impl()->property_trees()->scroll_tree;
}

class LayerTreeHostScrollTest : public LayerTreeTest {
 protected:
  void SetupTree() override {
    LayerTreeTest::SetupTree();
    Layer* root_layer = layer_tree_host()->root_layer();

    // Create an effective max_scroll_offset of (100, 100).
    gfx::Size scroll_layer_bounds(root_layer->bounds().width() + 100,
                                  root_layer->bounds().height() + 100);

    CreateVirtualViewportLayers(root_layer, root_layer->bounds(),
                                root_layer->bounds(), scroll_layer_bounds,
                                layer_tree_host());
  }
};

class LayerTreeHostScrollTestScrollSimple : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollSimple()
      : initial_scroll_(10, 20),
        second_scroll_(40, 5),
        scroll_amount_(2, -1),
        num_scrolls_(0) {}

  void BeginTest() override {
    outer_viewport_container_layer_id_ = layer_tree_host()
                                             ->outer_viewport_scroll_layer()
                                             ->scroll_clip_layer()
                                             ->id();
    layer_tree_host()->outer_viewport_scroll_layer()->SetScrollOffset(
        initial_scroll_);
    PostSetNeedsCommitToMainThread();
  }

  void UpdateLayerTreeHost() override {
    Layer* scroll_layer = layer_tree_host()->outer_viewport_scroll_layer();
    if (!layer_tree_host()->source_frame_number()) {
      EXPECT_VECTOR_EQ(initial_scroll_, scroll_layer->scroll_offset());
    } else {
      EXPECT_VECTOR_EQ(gfx::ScrollOffsetWithDelta(initial_scroll_,
                                                  scroll_amount_),
                       scroll_layer->scroll_offset());

      // Pretend like Javascript updated the scroll position itself.
      scroll_layer->SetScrollOffset(second_scroll_);
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* root = impl->active_tree()->root_layer_for_testing();
    LayerImpl* scroll_layer = impl->OuterViewportScrollLayer();
    EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(scroll_layer));

    scroll_layer->SetScrollClipLayer(outer_viewport_container_layer_id_);
    scroll_layer->SetBounds(
        gfx::Size(root->bounds().width() + 100, root->bounds().height() + 100));
    scroll_layer->ScrollBy(scroll_amount_);

    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(initial_scroll_, ScrollTreeForLayer(scroll_layer)
                                              ->GetScrollOffsetBaseForTesting(
                                                  scroll_layer->id()));
        EXPECT_VECTOR_EQ(scroll_amount_, ScrollDelta(scroll_layer));
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        EXPECT_VECTOR_EQ(second_scroll_, ScrollTreeForLayer(scroll_layer)
                                             ->GetScrollOffsetBaseForTesting(
                                                 scroll_layer->id()));
        EXPECT_VECTOR_EQ(scroll_amount_, ScrollDelta(scroll_layer));
        EndTest();
        break;
    }
  }

  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float scale,
                           float top_controls_delta) override {
    num_scrolls_++;
  }

  void AfterTest() override { EXPECT_EQ(1, num_scrolls_); }

 private:
  gfx::ScrollOffset initial_scroll_;
  gfx::ScrollOffset second_scroll_;
  gfx::Vector2dF scroll_amount_;
  int num_scrolls_;
  int outer_viewport_container_layer_id_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestScrollSimple);

class LayerTreeHostScrollTestScrollMultipleRedraw
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollMultipleRedraw()
      : initial_scroll_(40, 10), scroll_amount_(-3, 17), num_scrolls_(0) {}

  void BeginTest() override {
    scroll_layer_ = layer_tree_host()->outer_viewport_scroll_layer();
    scroll_layer_->SetScrollOffset(initial_scroll_);
    PostSetNeedsCommitToMainThread();
  }

  void BeginCommitOnThread(LayerTreeHostImpl* impl) override {
    switch (layer_tree_host()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(initial_scroll_, scroll_layer_->scroll_offset());
        break;
      case 1:
        EXPECT_VECTOR_EQ(
            gfx::ScrollOffsetWithDelta(initial_scroll_,
                                       scroll_amount_ + scroll_amount_),
            scroll_layer_->scroll_offset());
      case 2:
        EXPECT_VECTOR_EQ(
            gfx::ScrollOffsetWithDelta(initial_scroll_,
                                       scroll_amount_ + scroll_amount_),
            scroll_layer_->scroll_offset());
        break;
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* scroll_layer =
        impl->active_tree()->LayerById(scroll_layer_->id());
    if (impl->active_tree()->source_frame_number() == 0 &&
        impl->SourceAnimationFrameNumberForTesting() == 1) {
      // First draw after first commit.
      EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(scroll_layer));
      scroll_layer->ScrollBy(scroll_amount_);
      EXPECT_VECTOR_EQ(scroll_amount_, ScrollDelta(scroll_layer));

      EXPECT_VECTOR_EQ(initial_scroll_,
                       ScrollTreeForLayer(scroll_layer)
                           ->GetScrollOffsetBaseForTesting(scroll_layer->id()));
      PostSetNeedsRedrawToMainThread();
    } else if (impl->active_tree()->source_frame_number() == 0 &&
               impl->SourceAnimationFrameNumberForTesting() == 2) {
      // Second draw after first commit.
      EXPECT_EQ(ScrollDelta(scroll_layer), scroll_amount_);
      scroll_layer->ScrollBy(scroll_amount_);
      EXPECT_VECTOR_EQ(scroll_amount_ + scroll_amount_,
                       ScrollDelta(scroll_layer));

      EXPECT_VECTOR_EQ(initial_scroll_, scroll_layer_->scroll_offset());
      PostSetNeedsCommitToMainThread();
    } else if (impl->active_tree()->source_frame_number() == 1) {
      // Third or later draw after second commit.
      EXPECT_GE(impl->SourceAnimationFrameNumberForTesting(), 3u);
      EXPECT_VECTOR_EQ(
          gfx::ScrollOffsetWithDelta(initial_scroll_,
                                     scroll_amount_ + scroll_amount_),
          scroll_layer_->scroll_offset());
      EndTest();
    }
  }

  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float scale,
                           float top_controls_delta) override {
    num_scrolls_++;
  }

  void AfterTest() override { EXPECT_EQ(1, num_scrolls_); }

 private:
  gfx::ScrollOffset initial_scroll_;
  gfx::Vector2dF scroll_amount_;
  int num_scrolls_;
  scoped_refptr<Layer> scroll_layer_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestScrollMultipleRedraw);

class LayerTreeHostScrollTestScrollAbortedCommit
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollAbortedCommit()
      : initial_scroll_(50, 60),
        impl_scroll_(-3, 2),
        second_main_scroll_(14, -3),
        impl_scale_(2.f),
        num_will_begin_main_frames_(0),
        num_did_begin_main_frames_(0),
        num_will_commits_(0),
        num_did_commits_(0),
        num_impl_commits_(0),
        num_impl_scrolls_(0) {}

  void BeginTest() override {
    layer_tree_host()->outer_viewport_scroll_layer()->SetScrollOffset(
        initial_scroll_);
    PostSetNeedsCommitToMainThread();
  }

  void SetupTree() override {
    LayerTreeHostScrollTest::SetupTree();

    gfx::Size scroll_layer_bounds(200, 200);
    layer_tree_host()->outer_viewport_scroll_layer()->SetBounds(
        scroll_layer_bounds);
    layer_tree_host()->SetPageScaleFactorAndLimits(1.f, 0.01f, 100.f);
  }

  void WillBeginMainFrame() override {
    num_will_begin_main_frames_++;
    Layer* root_scroll_layer =
        layer_tree_host()->outer_viewport_scroll_layer();
    switch (num_will_begin_main_frames_) {
      case 1:
        // This will not be aborted because of the initial prop changes.
        EXPECT_EQ(0, num_impl_scrolls_);
        EXPECT_EQ(0, layer_tree_host()->source_frame_number());
        EXPECT_VECTOR_EQ(initial_scroll_, root_scroll_layer->scroll_offset());
        EXPECT_EQ(1.f, layer_tree_host()->page_scale_factor());
        break;
      case 2:
        // This commit will be aborted, and another commit will be
        // initiated from the redraw.
        EXPECT_EQ(1, num_impl_scrolls_);
        EXPECT_EQ(1, layer_tree_host()->source_frame_number());
        EXPECT_VECTOR_EQ(
            gfx::ScrollOffsetWithDelta(initial_scroll_, impl_scroll_),
            root_scroll_layer->scroll_offset());
        EXPECT_EQ(impl_scale_, layer_tree_host()->page_scale_factor());
        PostSetNeedsRedrawToMainThread();
        break;
      case 3:
        // This commit will not be aborted because of the scroll change.
        EXPECT_EQ(2, num_impl_scrolls_);
        // The source frame number still increases even with the abort.
        EXPECT_EQ(2, layer_tree_host()->source_frame_number());
        EXPECT_VECTOR_EQ(
            gfx::ScrollOffsetWithDelta(initial_scroll_,
                                       impl_scroll_ + impl_scroll_),
            root_scroll_layer->scroll_offset());
        EXPECT_EQ(impl_scale_ * impl_scale_,
                  layer_tree_host()->page_scale_factor());
        root_scroll_layer->SetScrollOffset(gfx::ScrollOffsetWithDelta(
            root_scroll_layer->scroll_offset(), second_main_scroll_));
        break;
      case 4:
        // This commit will also be aborted.
        EXPECT_EQ(3, num_impl_scrolls_);
        EXPECT_EQ(3, layer_tree_host()->source_frame_number());
        gfx::Vector2dF delta =
            impl_scroll_ + impl_scroll_ + impl_scroll_ + second_main_scroll_;
        EXPECT_VECTOR_EQ(gfx::ScrollOffsetWithDelta(initial_scroll_, delta),
                         root_scroll_layer->scroll_offset());

        // End the test by drawing to verify this commit is also aborted.
        PostSetNeedsRedrawToMainThread();
        break;
    }
  }

  void DidBeginMainFrame() override { num_did_begin_main_frames_++; }

  void WillCommit() override { num_will_commits_++; }

  void DidCommit() override { num_did_commits_++; }

  void BeginCommitOnThread(LayerTreeHostImpl* impl) override {
    num_impl_commits_++;
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* root_scroll_layer = impl->OuterViewportScrollLayer();

    if (impl->active_tree()->source_frame_number() == 0 &&
        impl->SourceAnimationFrameNumberForTesting() == 1) {
      // First draw
      EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(root_scroll_layer));
      root_scroll_layer->ScrollBy(impl_scroll_);
      EXPECT_VECTOR_EQ(impl_scroll_, ScrollDelta(root_scroll_layer));
      EXPECT_VECTOR_EQ(initial_scroll_, ScrollTreeForLayer(root_scroll_layer)
                                            ->GetScrollOffsetBaseForTesting(
                                                root_scroll_layer->id()));

      EXPECT_EQ(1.f, impl->active_tree()->page_scale_delta());
      EXPECT_EQ(1.f, impl->active_tree()->current_page_scale_factor());
      impl->active_tree()->SetPageScaleOnActiveTree(impl_scale_);
      EXPECT_EQ(impl_scale_, impl->active_tree()->page_scale_delta());
      EXPECT_EQ(impl_scale_, impl->active_tree()->current_page_scale_factor());

      // To simplify the testing flow, don't redraw here, just commit.
      impl->SetNeedsCommit();
    } else if (impl->active_tree()->source_frame_number() == 0 &&
               impl->SourceAnimationFrameNumberForTesting() == 2) {
      // Test a second draw after an aborted commit.
      // The scroll/scale values should be baked into the offset/scale factor
      // since the main thread consumed but aborted the begin frame.
      EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(root_scroll_layer));
      root_scroll_layer->ScrollBy(impl_scroll_);
      EXPECT_VECTOR_EQ(impl_scroll_, ScrollDelta(root_scroll_layer));
      EXPECT_VECTOR_EQ(
          gfx::ScrollOffsetWithDelta(initial_scroll_, impl_scroll_),
          ScrollTreeForLayer(root_scroll_layer)
              ->GetScrollOffsetBaseForTesting(root_scroll_layer->id()));

      EXPECT_EQ(1.f, impl->active_tree()->page_scale_delta());
      EXPECT_EQ(impl_scale_, impl->active_tree()->current_page_scale_factor());
      impl->active_tree()->SetPageScaleOnActiveTree(impl_scale_ * impl_scale_);
      EXPECT_EQ(impl_scale_, impl->active_tree()->page_scale_delta());
      EXPECT_EQ(impl_scale_ * impl_scale_,
                impl->active_tree()->current_page_scale_factor());

      impl->SetNeedsCommit();
    } else if (impl->active_tree()->source_frame_number() == 1) {
      // Commit for source frame 1 is aborted.
      NOTREACHED();
    } else if (impl->active_tree()->source_frame_number() == 2 &&
               impl->SourceAnimationFrameNumberForTesting() == 3) {
      // Third draw after the second full commit.
      EXPECT_EQ(ScrollDelta(root_scroll_layer), gfx::Vector2d());
      root_scroll_layer->ScrollBy(impl_scroll_);
      impl->SetNeedsCommit();
      EXPECT_VECTOR_EQ(impl_scroll_, ScrollDelta(root_scroll_layer));
      gfx::Vector2dF delta = impl_scroll_ + impl_scroll_ + second_main_scroll_;
      EXPECT_VECTOR_EQ(
          gfx::ScrollOffsetWithDelta(initial_scroll_, delta),
          ScrollTreeForLayer(root_scroll_layer)
              ->GetScrollOffsetBaseForTesting(root_scroll_layer->id()));
    } else if (impl->active_tree()->source_frame_number() == 2 &&
               impl->SourceAnimationFrameNumberForTesting() == 4) {
      // Final draw after the second aborted commit.
      EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(root_scroll_layer));
      gfx::Vector2dF delta =
          impl_scroll_ + impl_scroll_ + impl_scroll_ + second_main_scroll_;
      EXPECT_VECTOR_EQ(
          gfx::ScrollOffsetWithDelta(initial_scroll_, delta),
          ScrollTreeForLayer(root_scroll_layer)
              ->GetScrollOffsetBaseForTesting(root_scroll_layer->id()));
      EndTest();
    } else {
      // Commit for source frame 3 is aborted.
      NOTREACHED();
    }
  }

  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float scale,
                           float top_controls_delta) override {
    num_impl_scrolls_++;
  }

  void AfterTest() override {
    EXPECT_EQ(3, num_impl_scrolls_);
    // Verify that the embedder sees aborted commits as real commits.
    EXPECT_EQ(4, num_will_begin_main_frames_);
    EXPECT_EQ(4, num_did_begin_main_frames_);
    EXPECT_EQ(4, num_will_commits_);
    EXPECT_EQ(4, num_did_commits_);
    // ...but the compositor thread only sees two real ones.
    EXPECT_EQ(2, num_impl_commits_);
  }

 private:
  gfx::ScrollOffset initial_scroll_;
  gfx::Vector2dF impl_scroll_;
  gfx::Vector2dF second_main_scroll_;
  float impl_scale_;
  int num_will_begin_main_frames_;
  int num_did_begin_main_frames_;
  int num_will_commits_;
  int num_did_commits_;
  int num_impl_commits_;
  int num_impl_scrolls_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestScrollAbortedCommit);

class LayerTreeHostScrollTestFractionalScroll : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestFractionalScroll() : scroll_amount_(1.75, 0) {}

  void SetupTree() override {
    LayerTreeHostScrollTest::SetupTree();
    layer_tree_host()->SetPageScaleFactorAndLimits(1.f, 0.01f, 100.f);
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* scroll_layer = impl->OuterViewportScrollLayer();

    // Check that a fractional scroll delta is correctly accumulated over
    // multiple commits.
    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(
            gfx::Vector2d(0, 0),
            ScrollTreeForLayer(scroll_layer)
                ->GetScrollOffsetBaseForTesting(scroll_layer->id()));
        EXPECT_VECTOR_EQ(gfx::Vector2d(0, 0), ScrollDelta(scroll_layer));
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        EXPECT_VECTOR_EQ(
            gfx::ToFlooredVector2d(scroll_amount_),
            ScrollTreeForLayer(scroll_layer)
                ->GetScrollOffsetBaseForTesting(scroll_layer->id()));
        EXPECT_VECTOR_EQ(gfx::Vector2dF(fmod(scroll_amount_.x(), 1.0f), 0.0f),
                         ScrollDelta(scroll_layer));
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        EXPECT_VECTOR_EQ(
            gfx::ToFlooredVector2d(scroll_amount_ + scroll_amount_),
            ScrollTreeForLayer(scroll_layer)
                ->GetScrollOffsetBaseForTesting(scroll_layer->id()));
        EXPECT_VECTOR_EQ(
            gfx::Vector2dF(fmod(2.0f * scroll_amount_.x(), 1.0f), 0.0f),
            ScrollDelta(scroll_layer));
        EndTest();
        break;
    }
    scroll_layer->ScrollBy(scroll_amount_);
  }

  void AfterTest() override {}

 private:
  gfx::Vector2dF scroll_amount_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestFractionalScroll);

class LayerTreeHostScrollTestScrollSnapping : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollSnapping() : scroll_amount_(1.75, 0) {}

  void SetupTree() override {
    LayerTreeHostScrollTest::SetupTree();
    layer_tree_host()
        ->outer_viewport_scroll_layer()
        ->scroll_clip_layer()
        ->SetForceRenderSurfaceForTesting(true);
    gfx::Transform translate;
    translate.Translate(0.25f, 0.f);
    layer_tree_host()
        ->outer_viewport_scroll_layer()
        ->scroll_clip_layer()
        ->SetTransform(translate);
    layer_tree_host()->SetPageScaleFactorAndLimits(1.f, 0.1f, 100.f);
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* scroll_layer = impl->OuterViewportScrollLayer();
    gfx::Transform translate;

    // Check that screen space transform of the scrollable layer is correctly
    // snapped to integers.
    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_EQ(gfx::Transform(),
                  scroll_layer->draw_properties().screen_space_transform);
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        translate.Translate(-2, 0);
        EXPECT_EQ(translate,
                  scroll_layer->draw_properties().screen_space_transform);
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        translate.Translate(-3, 0);
        EXPECT_EQ(translate,
                  scroll_layer->draw_properties().screen_space_transform);
        EndTest();
        break;
    }
    scroll_layer->ScrollBy(scroll_amount_);
  }

  void AfterTest() override {}

 private:
  gfx::Vector2dF scroll_amount_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestScrollSnapping);

class LayerTreeHostScrollTestCaseWithChild : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestCaseWithChild()
      : initial_offset_(10, 20),
        javascript_scroll_(40, 5),
        scroll_amount_(2, -1),
        num_scrolls_(0) {}

  void SetupTree() override {
    layer_tree_host()->SetDeviceScaleFactor(device_scale_factor_);

    scoped_refptr<Layer> root_layer = Layer::Create();
    root_layer->SetBounds(gfx::Size(10, 10));

    root_scroll_layer_ = FakePictureLayer::Create(&fake_content_layer_client_);
    root_scroll_layer_->SetBounds(gfx::Size(110, 110));
    root_scroll_layer_->SetPosition(gfx::PointF());
    root_scroll_layer_->SetIsDrawable(true);

    CreateVirtualViewportLayers(root_layer.get(), root_scroll_layer_,
                                root_layer->bounds(), root_layer->bounds(),
                                layer_tree_host());

    child_layer_ = FakePictureLayer::Create(&fake_content_layer_client_);
    child_layer_->set_did_scroll_callback(
        base::Bind(&LayerTreeHostScrollTestCaseWithChild::DidScroll,
                   base::Unretained(this)));
    child_layer_->SetBounds(gfx::Size(110, 110));

    if (scroll_child_layer_) {
      // Scrolls on the child layer will happen at 5, 5. If they are treated
      // like device pixels, and device scale factor is 2, then they will
      // be considered at 2.5, 2.5 in logical pixels, and will miss this layer.
      child_layer_->SetPosition(gfx::PointF(5.f, 5.f));
    } else {
      // Adjust the child layer horizontally so that scrolls will never hit it.
      child_layer_->SetPosition(gfx::PointF(60.f, 5.f));
    }

    scoped_refptr<Layer> outer_container_layer =
        layer_tree_host()->outer_viewport_scroll_layer()->parent();

    child_layer_->SetIsDrawable(true);
    child_layer_->SetScrollClipLayerId(outer_container_layer->id());
    child_layer_->SetBounds(root_scroll_layer_->bounds());
    root_scroll_layer_->AddChild(child_layer_);

    if (scroll_child_layer_) {
      expected_scroll_layer_ = child_layer_;
      expected_no_scroll_layer_ = root_scroll_layer_;
    } else {
      expected_scroll_layer_ = root_scroll_layer_;
      expected_no_scroll_layer_ = child_layer_;
    }

    expected_scroll_layer_->SetScrollOffset(initial_offset_);

    layer_tree_host()->SetRootLayer(root_layer);
    LayerTreeTest::SetupTree();
    fake_content_layer_client_.set_bounds(root_layer->bounds());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void WillCommit() override {
    // Keep the test committing (otherwise the early out for no update
    // will stall the test).
    if (layer_tree_host()->source_frame_number() < 2) {
      layer_tree_host()->SetNeedsCommit();
    }
  }

  void DidScroll() {
    final_scroll_offset_ = expected_scroll_layer_->scroll_offset();
  }

  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float scale,
                           float top_controls_delta) override {
    num_scrolls_++;
  }

  void UpdateLayerTreeHost() override {
    EXPECT_VECTOR_EQ(gfx::Vector2d(),
                     expected_no_scroll_layer_->scroll_offset());

    switch (layer_tree_host()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(initial_offset_,
                         expected_scroll_layer_->scroll_offset());
        break;
      case 1:
        EXPECT_VECTOR_EQ(
            gfx::ScrollOffsetWithDelta(initial_offset_, scroll_amount_),
            expected_scroll_layer_->scroll_offset());

        // Pretend like Javascript updated the scroll position itself.
        expected_scroll_layer_->SetScrollOffset(javascript_scroll_);
        break;
      case 2:
        EXPECT_VECTOR_EQ(gfx::ScrollOffsetWithDelta(javascript_scroll_,
                                                    scroll_amount_),
                         expected_scroll_layer_->scroll_offset());
        break;
    }
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* inner_scroll = impl->InnerViewportScrollLayer();
    FakePictureLayerImpl* root_scroll_layer_impl =
        static_cast<FakePictureLayerImpl*>(impl->OuterViewportScrollLayer());
    FakePictureLayerImpl* child_layer_impl = static_cast<FakePictureLayerImpl*>(
        root_scroll_layer_impl->layer_tree_impl()->LayerById(
            child_layer_->id()));

    LayerImpl* expected_scroll_layer_impl = NULL;
    LayerImpl* expected_no_scroll_layer_impl = NULL;
    if (scroll_child_layer_) {
      expected_scroll_layer_impl = child_layer_impl;
      expected_no_scroll_layer_impl = root_scroll_layer_impl;
    } else {
      expected_scroll_layer_impl = root_scroll_layer_impl;
      expected_no_scroll_layer_impl = child_layer_impl;
    }

    EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(inner_scroll));
    EXPECT_VECTOR_EQ(gfx::Vector2d(),
                     ScrollDelta(expected_no_scroll_layer_impl));

    // Ensure device scale factor matches the active tree.
    EXPECT_EQ(device_scale_factor_, impl->active_tree()->device_scale_factor());
    switch (impl->active_tree()->source_frame_number()) {
      case 0: {
        // GESTURE scroll on impl thread. Also tests that the last scrolled
        // layer id is stored even after the scrolling ends.
        gfx::Point scroll_point =
            gfx::ToCeiledPoint(expected_scroll_layer_impl->position() -
                               gfx::Vector2dF(0.5f, 0.5f));
        InputHandler::ScrollStatus status = impl->ScrollBegin(
            BeginState(scroll_point).get(), InputHandler::TOUCHSCREEN);
        EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
        impl->ScrollBy(UpdateState(gfx::Point(), scroll_amount_).get());
        LayerImpl* scrolling_layer = impl->CurrentlyScrollingLayer();
        CHECK(scrolling_layer);
        impl->ScrollEnd(EndState().get());
        CHECK(!impl->CurrentlyScrollingLayer());
        EXPECT_EQ(scrolling_layer->id(),
                  impl->active_tree()->LastScrolledLayerId());

        // Check the scroll is applied as a delta.
        EXPECT_VECTOR_EQ(initial_offset_,
                         ScrollTreeForLayer(expected_scroll_layer_impl)
                             ->GetScrollOffsetBaseForTesting(
                                 expected_scroll_layer_impl->id()));
        EXPECT_VECTOR_EQ(scroll_amount_,
                         ScrollDelta(expected_scroll_layer_impl));
        break;
      }
      case 1: {
        // WHEEL scroll on impl thread.
        gfx::Point scroll_point =
            gfx::ToCeiledPoint(expected_scroll_layer_impl->position() +
                               gfx::Vector2dF(0.5f, 0.5f));
        InputHandler::ScrollStatus status = impl->ScrollBegin(
            BeginState(scroll_point).get(), InputHandler::WHEEL);
        EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
        impl->ScrollBy(UpdateState(gfx::Point(), scroll_amount_).get());
        impl->ScrollEnd(EndState().get());

        // Check the scroll is applied as a delta.
        EXPECT_VECTOR_EQ(javascript_scroll_,
                         ScrollTreeForLayer(expected_scroll_layer_impl)
                             ->GetScrollOffsetBaseForTesting(
                                 expected_scroll_layer_impl->id()));
        EXPECT_VECTOR_EQ(scroll_amount_,
                         ScrollDelta(expected_scroll_layer_impl));
        break;
      }
      case 2:

        EXPECT_VECTOR_EQ(
            gfx::ScrollOffsetWithDelta(javascript_scroll_, scroll_amount_),
            ScrollTreeForLayer(expected_scroll_layer_impl)
                ->GetScrollOffsetBaseForTesting(
                    expected_scroll_layer_impl->id()));
        EXPECT_VECTOR_EQ(gfx::Vector2d(),
                         ScrollDelta(expected_scroll_layer_impl));

        EndTest();
        break;
    }
  }

  void AfterTest() override {
    if (scroll_child_layer_) {
      EXPECT_EQ(0, num_scrolls_);
      EXPECT_VECTOR_EQ(gfx::ScrollOffsetWithDelta(javascript_scroll_,
                                                  scroll_amount_),
                       final_scroll_offset_);
    } else {
      EXPECT_EQ(2, num_scrolls_);
      EXPECT_VECTOR_EQ(gfx::ScrollOffset(), final_scroll_offset_);
    }
  }

 protected:
  float device_scale_factor_;
  bool scroll_child_layer_;

  gfx::ScrollOffset initial_offset_;
  gfx::ScrollOffset javascript_scroll_;
  gfx::Vector2d scroll_amount_;
  int num_scrolls_;
  gfx::ScrollOffset final_scroll_offset_;

  FakeContentLayerClient fake_content_layer_client_;

  scoped_refptr<Layer> root_scroll_layer_;
  scoped_refptr<Layer> child_layer_;
  scoped_refptr<Layer> expected_scroll_layer_;
  scoped_refptr<Layer> expected_no_scroll_layer_;
};

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor1_ScrollChild_DirectRenderer) {
  device_scale_factor_ = 1.f;
  scroll_child_layer_ = true;
  RunTest(CompositorMode::THREADED, false);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor1_ScrollChild_DelegatingRenderer) {
  device_scale_factor_ = 1.f;
  scroll_child_layer_ = true;
  RunTest(CompositorMode::THREADED, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor15_ScrollChild_DirectRenderer) {
  device_scale_factor_ = 1.5f;
  scroll_child_layer_ = true;
  RunTest(CompositorMode::THREADED, false);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor15_ScrollChild_DelegatingRenderer) {
  device_scale_factor_ = 1.5f;
  scroll_child_layer_ = true;
  RunTest(CompositorMode::THREADED, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor2_ScrollChild_DirectRenderer) {
  device_scale_factor_ = 2.f;
  scroll_child_layer_ = true;
  RunTest(CompositorMode::THREADED, false);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor2_ScrollChild_DelegatingRenderer) {
  device_scale_factor_ = 2.f;
  scroll_child_layer_ = true;
  RunTest(CompositorMode::THREADED, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor1_ScrollRootScrollLayer_DirectRenderer) {
  device_scale_factor_ = 1.f;
  scroll_child_layer_ = false;
  RunTest(CompositorMode::THREADED, false);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor1_ScrollRootScrollLayer_DelegatingRenderer) {
  device_scale_factor_ = 1.f;
  scroll_child_layer_ = false;
  RunTest(CompositorMode::THREADED, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor15_ScrollRootScrollLayer_DirectRenderer) {
  device_scale_factor_ = 1.5f;
  scroll_child_layer_ = false;
  RunTest(CompositorMode::THREADED, false);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor15_ScrollRootScrollLayer_DelegatingRenderer) {
  device_scale_factor_ = 1.5f;
  scroll_child_layer_ = false;
  RunTest(CompositorMode::THREADED, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor2_ScrollRootScrollLayer_DirectRenderer) {
  device_scale_factor_ = 2.f;
  scroll_child_layer_ = false;
  RunTest(CompositorMode::THREADED, false);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor2_ScrollRootScrollLayer_DelegatingRenderer) {
  device_scale_factor_ = 2.f;
  scroll_child_layer_ = false;
  RunTest(CompositorMode::THREADED, true);
}

class LayerTreeHostScrollTestSimple : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestSimple()
      : initial_scroll_(10, 20),
        main_thread_scroll_(40, 5),
        impl_thread_scroll1_(2, -1),
        impl_thread_scroll2_(-3, 10),
        num_scrolls_(0) {}

  void SetupTree() override {
    LayerTreeHostScrollTest::SetupTree();
    layer_tree_host()->SetPageScaleFactorAndLimits(1.f, 0.01f, 100.f);
  }

  void BeginTest() override {
    layer_tree_host()->outer_viewport_scroll_layer()->SetScrollOffset(
        initial_scroll_);
    PostSetNeedsCommitToMainThread();
  }

  void UpdateLayerTreeHost() override {
    Layer* scroll_layer = layer_tree_host()->outer_viewport_scroll_layer();
    if (!layer_tree_host()->source_frame_number()) {
      EXPECT_VECTOR_EQ(initial_scroll_, scroll_layer->scroll_offset());
    } else {
      EXPECT_VECTOR_EQ(
          scroll_layer->scroll_offset(),
          gfx::ScrollOffsetWithDelta(initial_scroll_, impl_thread_scroll1_));

      // Pretend like Javascript updated the scroll position itself with a
      // change of main_thread_scroll.
      scroll_layer->SetScrollOffset(
          gfx::ScrollOffsetWithDelta(
              initial_scroll_, main_thread_scroll_ + impl_thread_scroll1_));
    }
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* impl) override {
    // We force a second draw here of the first commit before activating
    // the second commit.
    if (impl->active_tree()->source_frame_number() == 0)
      impl->SetNeedsRedraw();
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    if (impl->pending_tree())
      impl->SetNeedsRedraw();

    LayerImpl* root = impl->active_tree()->root_layer_for_testing();
    LayerImpl* scroll_layer = impl->OuterViewportScrollLayer();
    LayerImpl* pending_root =
        impl->active_tree()->FindPendingTreeLayerById(root->id());

    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        if (!impl->pending_tree()) {
          impl->BlockNotifyReadyToActivateForTesting(true);
          EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(scroll_layer));
          scroll_layer->ScrollBy(impl_thread_scroll1_);

          EXPECT_VECTOR_EQ(initial_scroll_, ScrollTreeForLayer(scroll_layer)
                                                ->GetScrollOffsetBaseForTesting(
                                                    scroll_layer->id()));
          EXPECT_VECTOR_EQ(impl_thread_scroll1_, ScrollDelta(scroll_layer));
          PostSetNeedsCommitToMainThread();

          // CommitCompleteOnThread will trigger this function again
          // and cause us to take the else clause.
        } else {
          impl->BlockNotifyReadyToActivateForTesting(false);
          ASSERT_TRUE(pending_root);
          EXPECT_EQ(impl->pending_tree()->source_frame_number(), 1);

          scroll_layer->ScrollBy(impl_thread_scroll2_);
          EXPECT_VECTOR_EQ(initial_scroll_, ScrollTreeForLayer(scroll_layer)
                                                ->GetScrollOffsetBaseForTesting(
                                                    scroll_layer->id()));
          EXPECT_VECTOR_EQ(impl_thread_scroll1_ + impl_thread_scroll2_,
                           ScrollDelta(scroll_layer));

          LayerImpl* pending_scroll_layer =
              impl->pending_tree()->OuterViewportScrollLayer();
          EXPECT_VECTOR_EQ(
              gfx::ScrollOffsetWithDelta(
                  initial_scroll_, main_thread_scroll_ + impl_thread_scroll1_),
              ScrollTreeForLayer(pending_scroll_layer)
                  ->GetScrollOffsetBaseForTesting(pending_scroll_layer->id()));
          EXPECT_VECTOR_EQ(impl_thread_scroll2_,
                           ScrollDelta(pending_scroll_layer));
        }
        break;
      case 1:
        EXPECT_FALSE(impl->pending_tree());
        EXPECT_VECTOR_EQ(
            gfx::ScrollOffsetWithDelta(
                initial_scroll_, main_thread_scroll_ + impl_thread_scroll1_),
            ScrollTreeForLayer(scroll_layer)
                ->GetScrollOffsetBaseForTesting(scroll_layer->id()));
        EXPECT_VECTOR_EQ(impl_thread_scroll2_, ScrollDelta(scroll_layer));
        EndTest();
        break;
    }
  }

  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float scale,
                           float top_controls_delta) override {
    num_scrolls_++;
  }

  void AfterTest() override { EXPECT_EQ(1, num_scrolls_); }

 private:
  gfx::ScrollOffset initial_scroll_;
  gfx::Vector2dF main_thread_scroll_;
  gfx::Vector2dF impl_thread_scroll1_;
  gfx::Vector2dF impl_thread_scroll2_;
  int num_scrolls_;
};

// This tests scrolling on the impl side which is only possible with a thread.
MULTI_THREAD_TEST_F(LayerTreeHostScrollTestSimple);

// This test makes sure that layers pick up scrolls that occur between
// beginning a commit and finishing a commit (aka scroll deltas not
// included in sent scroll delta) still apply to layers that don't
// push properties.
class LayerTreeHostScrollTestImplOnlyScroll : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestImplOnlyScroll()
      : initial_scroll_(20, 10), impl_thread_scroll_(-2, 3), impl_scale_(2.f) {}

  void SetupTree() override {
    LayerTreeHostScrollTest::SetupTree();
    layer_tree_host()->SetPageScaleFactorAndLimits(1.f, 0.01f, 100.f);
  }

  void BeginTest() override {
    layer_tree_host()->outer_viewport_scroll_layer()->SetScrollOffset(
        initial_scroll_);
    PostSetNeedsCommitToMainThread();
  }

  void WillCommit() override {
    Layer* scroll_layer = layer_tree_host()->outer_viewport_scroll_layer();
    switch (layer_tree_host()->source_frame_number()) {
      case 0:
        EXPECT_TRUE(
            scroll_layer->layer_tree_host()->LayerNeedsPushPropertiesForTesting(
                scroll_layer));
        break;
      case 1:
        // Even if this layer doesn't need push properties, it should
        // still pick up scrolls that happen on the active layer during
        // commit.
        EXPECT_FALSE(
            scroll_layer->layer_tree_host()->LayerNeedsPushPropertiesForTesting(
                scroll_layer));
        break;
    }
  }

  void BeginCommitOnThread(LayerTreeHostImpl* impl) override {
    // Scroll after the 2nd commit has started.
    if (impl->active_tree()->source_frame_number() == 0) {
      LayerImpl* active_root = impl->active_tree()->root_layer_for_testing();
      LayerImpl* active_scroll_layer = impl->OuterViewportScrollLayer();
      ASSERT_TRUE(active_root);
      ASSERT_TRUE(active_scroll_layer);
      active_scroll_layer->ScrollBy(impl_thread_scroll_);
      impl->active_tree()->SetPageScaleOnActiveTree(impl_scale_);
    }
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* impl) override {
    // We force a second draw here of the first commit before activating
    // the second commit.
    LayerImpl* active_root = impl->active_tree()->root_layer_for_testing();
    LayerImpl* active_scroll_layer =
        active_root ? impl->OuterViewportScrollLayer() : NULL;
    LayerImpl* pending_root = impl->pending_tree()->root_layer_for_testing();
    LayerImpl* pending_scroll_layer =
        impl->pending_tree()->OuterViewportScrollLayer();

    ASSERT_TRUE(pending_root);
    ASSERT_TRUE(pending_scroll_layer);
    switch (impl->pending_tree()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(
            initial_scroll_,
            ScrollTreeForLayer(pending_scroll_layer)
                ->GetScrollOffsetBaseForTesting(pending_scroll_layer->id()));
        EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(pending_scroll_layer));
        EXPECT_FALSE(active_root);
        break;
      case 1:
        // Even though the scroll happened during the commit, both layers
        // should have the appropriate scroll delta.
        EXPECT_VECTOR_EQ(
            initial_scroll_,
            ScrollTreeForLayer(pending_scroll_layer)
                ->GetScrollOffsetBaseForTesting(pending_scroll_layer->id()));
        EXPECT_VECTOR_EQ(impl_thread_scroll_,
                         ScrollDelta(pending_scroll_layer));
        ASSERT_TRUE(active_root);
        EXPECT_VECTOR_EQ(
            initial_scroll_,
            ScrollTreeForLayer(active_scroll_layer)
                ->GetScrollOffsetBaseForTesting(active_scroll_layer->id()));
        EXPECT_VECTOR_EQ(impl_thread_scroll_, ScrollDelta(active_scroll_layer));
        break;
      case 2:
        // On the next commit, this delta should have been sent and applied.
        EXPECT_VECTOR_EQ(
            gfx::ScrollOffsetWithDelta(initial_scroll_, impl_thread_scroll_),
            ScrollTreeForLayer(pending_scroll_layer)
                ->GetScrollOffsetBaseForTesting(pending_scroll_layer->id()));
        EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(pending_scroll_layer));
        break;
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    if (impl->pending_tree())
      impl->SetNeedsRedraw();

    LayerImpl* scroll_layer = impl->OuterViewportScrollLayer();

    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(initial_scroll_, ScrollTreeForLayer(scroll_layer)
                                              ->GetScrollOffsetBaseForTesting(
                                                  scroll_layer->id()));
        EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(scroll_layer));
        EXPECT_EQ(1.f, impl->active_tree()->page_scale_delta());
        EXPECT_EQ(1.f, impl->active_tree()->current_page_scale_factor());
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        EXPECT_VECTOR_EQ(initial_scroll_, ScrollTreeForLayer(scroll_layer)
                                              ->GetScrollOffsetBaseForTesting(
                                                  scroll_layer->id()));
        EXPECT_VECTOR_EQ(impl_thread_scroll_, ScrollDelta(scroll_layer));
        EXPECT_EQ(impl_scale_, impl->active_tree()->page_scale_delta());
        EXPECT_EQ(impl_scale_,
                  impl->active_tree()->current_page_scale_factor());
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        EXPECT_EQ(1.f, impl->active_tree()->page_scale_delta());
        EXPECT_EQ(impl_scale_,
                  impl->active_tree()->current_page_scale_factor());
        EndTest();
        break;
    }
  }

  void AfterTest() override {}

 private:
  gfx::ScrollOffset initial_scroll_;
  gfx::Vector2dF impl_thread_scroll_;
  float impl_scale_;
};

// This tests scrolling on the impl side which is only possible with a thread.
MULTI_THREAD_TEST_F(LayerTreeHostScrollTestImplOnlyScroll);

class LayerTreeHostScrollTestScrollZeroMaxScrollOffset
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollZeroMaxScrollOffset() {}

  void BeginTest() override {
    outer_viewport_container_layer_id_ = layer_tree_host()
                                             ->outer_viewport_scroll_layer()
                                             ->scroll_clip_layer()
                                             ->id();
    PostSetNeedsCommitToMainThread();
  }

  void UpdateLayerTreeHost() override {
    Layer* root = layer_tree_host()->root_layer();
    Layer* scroll_layer = layer_tree_host()->outer_viewport_scroll_layer();
    switch (layer_tree_host()->source_frame_number()) {
      case 0:
        scroll_layer->SetScrollClipLayerId(outer_viewport_container_layer_id_);
        // Set max_scroll_offset = (100, 100).
        scroll_layer->SetBounds(gfx::Size(root->bounds().width() + 100,
                                          root->bounds().height() + 100));
        break;
      case 1:
        // Set max_scroll_offset = (0, 0).
        scroll_layer->SetBounds(root->bounds());
        break;
      case 2:
        // Set max_scroll_offset = (-100, -100).
        scroll_layer->SetBounds(gfx::Size());
        break;
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* scroll_layer = impl->OuterViewportScrollLayer();

    ScrollTree& scroll_tree =
        impl->active_tree()->property_trees()->scroll_tree;
    ScrollNode* scroll_node =
        scroll_tree.Node(scroll_layer->scroll_tree_index());
    InputHandler::ScrollStatus status =
        impl->TryScroll(gfx::PointF(0.0f, 1.0f), InputHandler::TOUCHSCREEN,
                        scroll_tree, scroll_node);
    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
        EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
                  status.main_thread_scrolling_reasons);
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        EXPECT_EQ(InputHandler::SCROLL_IGNORED, status.thread);
        EXPECT_EQ(MainThreadScrollingReason::kNotScrollable,
                  status.main_thread_scrolling_reasons);
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        EXPECT_EQ(InputHandler::SCROLL_IGNORED, status.thread);
        EXPECT_EQ(MainThreadScrollingReason::kNotScrollable,
                  status.main_thread_scrolling_reasons);
        EndTest();
        break;
    }
  }

  void AfterTest() override {}

 private:
  int outer_viewport_container_layer_id_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostScrollTestScrollZeroMaxScrollOffset);

class LayerTreeHostScrollTestScrollNonDrawnLayer
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollNonDrawnLayer() {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void SetupTree() override {
    LayerTreeHostScrollTest::SetupTree();
    layer_tree_host()->outer_viewport_scroll_layer()->SetIsDrawable(false);
    layer_tree_host()->outer_viewport_scroll_layer()->SetScrollOffset(
        gfx::ScrollOffset(20.f, 20.f));
    layer_tree_host()
        ->outer_viewport_scroll_layer()
        ->SetNonFastScrollableRegion(gfx::Rect(20, 20, 20, 20));
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* scroll_layer = impl->OuterViewportScrollLayer();

    ScrollTree& scroll_tree =
        impl->active_tree()->property_trees()->scroll_tree;
    ScrollNode* scroll_node =
        scroll_tree.Node(scroll_layer->scroll_tree_index());

    // Verify that the scroll layer's scroll offset is taken into account when
    // checking whether the screen space point is inside the non-fast
    // scrollable region.

    InputHandler::ScrollStatus status =
        impl->TryScroll(gfx::PointF(1.f, 1.f), InputHandler::TOUCHSCREEN,
                        scroll_tree, scroll_node);
    EXPECT_EQ(InputHandler::SCROLL_ON_MAIN_THREAD, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNonFastScrollableRegion,
              status.main_thread_scrolling_reasons);

    status = impl->TryScroll(gfx::PointF(21.f, 21.f), InputHandler::TOUCHSCREEN,
                             scroll_tree, scroll_node);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_scrolling_reasons);

    EndTest();
  }

  void AfterTest() override {}
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostScrollTestScrollNonDrawnLayer);

class LayerTreeHostScrollTestImplScrollUnderMainThreadScrollingParent
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestImplScrollUnderMainThreadScrollingParent() {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void SetupTree() override {
    LayerTreeHostScrollTest::SetupTree();
    layer_tree_host()
        ->inner_viewport_scroll_layer()
        ->AddMainThreadScrollingReasons(
            MainThreadScrollingReason::kEventHandlers);
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* inner_scroll_layer = impl->InnerViewportScrollLayer();
    LayerImpl* outer_scroll_layer = impl->OuterViewportScrollLayer();

    ScrollTree& scroll_tree =
        impl->active_tree()->property_trees()->scroll_tree;
    ScrollNode* inner_scroll_node =
        scroll_tree.Node(inner_scroll_layer->scroll_tree_index());
    ScrollNode* outer_scroll_node =
        scroll_tree.Node(outer_scroll_layer->scroll_tree_index());

    InputHandler::ScrollStatus status =
        impl->TryScroll(gfx::PointF(1.f, 1.f), InputHandler::TOUCHSCREEN,
                        scroll_tree, inner_scroll_node);
    EXPECT_EQ(InputHandler::SCROLL_ON_MAIN_THREAD, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kEventHandlers,
              status.main_thread_scrolling_reasons);

    status = impl->TryScroll(gfx::PointF(1.f, 1.f), InputHandler::TOUCHSCREEN,
                             scroll_tree, outer_scroll_node);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_scrolling_reasons);
    EndTest();
  }

  void AfterTest() override {}
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostScrollTestImplScrollUnderMainThreadScrollingParent);

class ThreadCheckingInputHandlerClient : public InputHandlerClient {
 public:
  ThreadCheckingInputHandlerClient(base::SingleThreadTaskRunner* runner,
                                   bool* received_stop_flinging)
      : task_runner_(runner), received_stop_flinging_(received_stop_flinging) {}

  void WillShutdown() override {
    if (!received_stop_flinging_)
      ADD_FAILURE() << "WillShutdown() called before fling stopped";
  }

  void Animate(base::TimeTicks time) override {
    if (!task_runner_->BelongsToCurrentThread())
      ADD_FAILURE() << "Animate called on wrong thread";
  }

  void MainThreadHasStoppedFlinging() override {
    if (!task_runner_->BelongsToCurrentThread())
      ADD_FAILURE() << "MainThreadHasStoppedFlinging called on wrong thread";
    *received_stop_flinging_ = true;
  }

  void ReconcileElasticOverscrollAndRootScroll() override {
    if (!task_runner_->BelongsToCurrentThread()) {
      ADD_FAILURE() << "ReconcileElasticOverscrollAndRootScroll called on "
                    << "wrong thread";
    }
  }

  void UpdateRootLayerStateForSynchronousInputHandler(
      const gfx::ScrollOffset& total_scroll_offset,
      const gfx::ScrollOffset& max_scroll_offset,
      const gfx::SizeF& scrollable_size,
      float page_scale_factor,
      float min_page_scale_factor,
      float max_page_scale_factor) override {
    if (!task_runner_->BelongsToCurrentThread()) {
      ADD_FAILURE() << "UpdateRootLayerStateForSynchronousInputHandler called "
                    << " on wrong thread";
    }
  }

 private:
  base::SingleThreadTaskRunner* task_runner_;
  bool* received_stop_flinging_;
};

void BindInputHandlerOnCompositorThread(
    const base::WeakPtr<InputHandler>& input_handler,
    ThreadCheckingInputHandlerClient* client) {
  input_handler->BindToClient(client);
}

TEST(LayerTreeHostFlingTest, DidStopFlingingThread) {
  base::Thread impl_thread("cc");
  ASSERT_TRUE(impl_thread.Start());

  bool received_stop_flinging = false;
  LayerTreeSettings settings;

  ThreadCheckingInputHandlerClient input_handler_client(
      impl_thread.task_runner().get(), &received_stop_flinging);
  FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DIRECT_3D);

  ASSERT_TRUE(impl_thread.task_runner());
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;

  LayerTreeHost::InitParams params;
  params.client = &client;
  params.shared_bitmap_manager = &shared_bitmap_manager;
  params.task_graph_runner = &task_graph_runner;
  params.settings = &settings;
  params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
  params.animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<LayerTreeHost> layer_tree_host =
      LayerTreeHost::CreateThreaded(impl_thread.task_runner(), &params);

  impl_thread.task_runner()->PostTask(
      FROM_HERE, base::Bind(&BindInputHandlerOnCompositorThread,
                            layer_tree_host->GetInputHandler(),
                            base::Unretained(&input_handler_client)));

  layer_tree_host->DidStopFlinging();
  layer_tree_host = nullptr;
  impl_thread.Stop();
  EXPECT_TRUE(received_stop_flinging);
}

class LayerTreeHostScrollTestLayerStructureChange
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestLayerStructureChange()
      : scroll_destroy_whole_tree_(false) {}

  void SetupTree() override {
    LayerTreeTest::SetupTree();
    Layer* root_layer = layer_tree_host()->root_layer();
    root_layer->SetBounds(gfx::Size(10, 10));

    CreateVirtualViewportLayers(root_layer, root_layer->bounds(),
                                root_layer->bounds(), root_layer->bounds(),
                                layer_tree_host());

    Layer* outer_scroll_layer =
        layer_tree_host()->outer_viewport_scroll_layer();

    Layer* root_scroll_layer =
        CreateScrollLayer(outer_scroll_layer, &root_scroll_layer_client_);
    Layer* sibling_scroll_layer =
        CreateScrollLayer(outer_scroll_layer, &sibling_scroll_layer_client_);
    Layer* child_scroll_layer =
        CreateScrollLayer(root_scroll_layer, &child_scroll_layer_client_);
    root_scroll_layer_id_ = root_scroll_layer->id();
    sibling_scroll_layer_id_ = sibling_scroll_layer->id();
    child_scroll_layer_id_ = child_scroll_layer->id();
    fake_content_layer_client_.set_bounds(root_layer->bounds());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        SetScrollOffsetDelta(
            impl->active_tree()->LayerById(root_scroll_layer_id_),
            gfx::Vector2dF(5, 5));
        SetScrollOffsetDelta(
            impl->active_tree()->LayerById(child_scroll_layer_id_),
            gfx::Vector2dF(5, 5));
        SetScrollOffsetDelta(
            impl->active_tree()->LayerById(sibling_scroll_layer_id_),
            gfx::Vector2dF(5, 5));
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        EndTest();
        break;
    }
  }

  void AfterTest() override {}

  virtual void DidScroll(Layer* layer) {
    if (scroll_destroy_whole_tree_) {
      layer_tree_host()->RegisterViewportLayers(NULL, NULL, NULL, NULL);
      layer_tree_host()->SetRootLayer(NULL);
      EndTest();
      return;
    }
    layer->RemoveFromParent();
  }

 protected:
  class FakeLayerScrollClient {
   public:
    void DidScroll() {
      owner_->DidScroll(layer_);
    }
    LayerTreeHostScrollTestLayerStructureChange* owner_;
    Layer* layer_;
  };

  Layer* CreateScrollLayer(Layer* parent, FakeLayerScrollClient* client) {
    scoped_refptr<PictureLayer> scroll_layer =
        PictureLayer::Create(&fake_content_layer_client_);
    scroll_layer->SetBounds(gfx::Size(110, 110));
    scroll_layer->SetPosition(gfx::PointF());
    scroll_layer->SetIsDrawable(true);
    scroll_layer->SetScrollClipLayerId(parent->id());
    scroll_layer->SetBounds(gfx::Size(parent->bounds().width() + 100,
                                      parent->bounds().height() + 100));
    scroll_layer->set_did_scroll_callback(base::Bind(
        &FakeLayerScrollClient::DidScroll, base::Unretained(client)));
    client->owner_ = this;
    client->layer_ = scroll_layer.get();
    parent->AddChild(scroll_layer);
    return scroll_layer.get();
  }

  static void SetScrollOffsetDelta(LayerImpl* layer_impl,
                                   const gfx::Vector2dF& delta) {
    if (layer_impl->layer_tree_impl()
            ->property_trees()
            ->scroll_tree.SetScrollOffsetDeltaForTesting(layer_impl->id(),
                                                         delta))
      layer_impl->layer_tree_impl()->DidUpdateScrollOffset(
          layer_impl->id(), layer_impl->transform_tree_index());
  }

  FakeLayerScrollClient root_scroll_layer_client_;
  FakeLayerScrollClient sibling_scroll_layer_client_;
  FakeLayerScrollClient child_scroll_layer_client_;
  int root_scroll_layer_id_;
  int sibling_scroll_layer_id_;
  int child_scroll_layer_id_;

  FakeContentLayerClient fake_content_layer_client_;

  bool scroll_destroy_whole_tree_;
};

TEST_F(LayerTreeHostScrollTestLayerStructureChange, ScrollDestroyLayer) {
  RunTest(CompositorMode::THREADED, false);
}

TEST_F(LayerTreeHostScrollTestLayerStructureChange, ScrollDestroyWholeTree) {
  scroll_destroy_whole_tree_ = true;
  RunTest(CompositorMode::THREADED, false);
}

class LayerTreeHostScrollTestScrollMFBA : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollMFBA()
      : initial_scroll_(10, 20),
        second_scroll_(40, 5),
        third_scroll_(20, 10),
        scroll_amount_(2, -1),
        num_commits_(0),
        num_scrolls_(0) {}

  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->main_frame_before_activation_enabled = true;
  }

  void BeginTest() override {
    outer_viewport_container_layer_id_ = layer_tree_host()
                                             ->outer_viewport_scroll_layer()
                                             ->scroll_clip_layer()
                                             ->id();
    layer_tree_host()->outer_viewport_scroll_layer()->SetScrollOffset(
        initial_scroll_);
    PostSetNeedsCommitToMainThread();
  }

  void StartCommitOnImpl() override {
    switch (num_commits_) {
      case 1:
        // Ask for commit here because activation (and draw) will be blocked.
        GetProxyImplForTest()->SetNeedsCommitOnImpl();
        // Block activation after second commit until third commit is ready.
        GetProxyImplForTest()->BlockNotifyReadyToActivateForTesting(true);
        break;
      case 2:
        // Unblock activation after third commit is ready.
        GetProxyImplForTest()->BlockNotifyReadyToActivateForTesting(false);
        break;
    }
    num_commits_++;
  }

  void UpdateLayerTreeHost() override {
    Layer* scroll_layer = layer_tree_host()->outer_viewport_scroll_layer();
    switch (layer_tree_host()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(initial_scroll_, scroll_layer->scroll_offset());
        break;
      case 1:
        EXPECT_VECTOR_EQ(
            gfx::ScrollOffsetWithDelta(initial_scroll_, scroll_amount_),
            scroll_layer->scroll_offset());
        // Pretend like Javascript updated the scroll position itself.
        scroll_layer->SetScrollOffset(second_scroll_);
        break;
      case 2:
        // Third frame does not see a scroll delta because we only did one
        // scroll for the second and third frames.
        EXPECT_VECTOR_EQ(second_scroll_, scroll_layer->scroll_offset());
        // Pretend like Javascript updated the scroll position itself.
        scroll_layer->SetScrollOffset(third_scroll_);
        break;
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* scroll_layer = impl->OuterViewportScrollLayer();
    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(scroll_layer));
        EXPECT_VECTOR_EQ(initial_scroll_, ScrollTreeForLayer(scroll_layer)
                                              ->GetScrollOffsetBaseForTesting(
                                                  scroll_layer->id()));
        Scroll(impl);
        EXPECT_VECTOR_EQ(scroll_amount_, ScrollDelta(scroll_layer));
        // Ask for commit after we've scrolled.
        impl->SetNeedsCommit();
        break;
      case 1:
        EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(scroll_layer));
        EXPECT_VECTOR_EQ(second_scroll_, ScrollTreeForLayer(scroll_layer)
                                             ->GetScrollOffsetBaseForTesting(
                                                 scroll_layer->id()));
        Scroll(impl);
        EXPECT_VECTOR_EQ(scroll_amount_, ScrollDelta(scroll_layer));
        break;
      case 2:
        // The scroll hasn't been consumed by the main thread.
        EXPECT_VECTOR_EQ(scroll_amount_, ScrollDelta(scroll_layer));
        EXPECT_VECTOR_EQ(third_scroll_, ScrollTreeForLayer(scroll_layer)
                                            ->GetScrollOffsetBaseForTesting(
                                                scroll_layer->id()));
        EndTest();
        break;
    }
  }

  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float scale,
                           float top_controls_delta) override {
    num_scrolls_++;
  }

  void AfterTest() override {
    EXPECT_EQ(3, num_commits_);
    EXPECT_EQ(1, num_scrolls_);
  }

 private:
  void Scroll(LayerTreeHostImpl* impl) {
    LayerImpl* root = impl->active_tree()->root_layer_for_testing();
    LayerImpl* scroll_layer = impl->OuterViewportScrollLayer();

    scroll_layer->SetScrollClipLayer(outer_viewport_container_layer_id_);
    scroll_layer->SetBounds(
        gfx::Size(root->bounds().width() + 100, root->bounds().height() + 100));
    scroll_layer->ScrollBy(scroll_amount_);
  }

  gfx::ScrollOffset initial_scroll_;
  gfx::ScrollOffset second_scroll_;
  gfx::ScrollOffset third_scroll_;
  gfx::Vector2dF scroll_amount_;
  int num_commits_;
  int num_scrolls_;
  int outer_viewport_container_layer_id_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestScrollMFBA);

class LayerTreeHostScrollTestScrollAbortedCommitMFBA
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollAbortedCommitMFBA()
      : initial_scroll_(50, 60),
        impl_scroll_(-3, 2),
        second_main_scroll_(14, -3),
        num_will_begin_main_frames_(0),
        num_did_begin_main_frames_(0),
        num_will_commits_(0),
        num_did_commits_(0),
        num_impl_commits_(0),
        num_aborted_commits_(0),
        num_impl_scrolls_(0),
        num_draws_(0) {}

  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->main_frame_before_activation_enabled = true;
  }

  void BeginTest() override {
    layer_tree_host()->outer_viewport_scroll_layer()->SetScrollOffset(
        initial_scroll_);
    PostSetNeedsCommitToMainThread();
  }

  void SetupTree() override {
    LayerTreeHostScrollTest::SetupTree();

    gfx::Size scroll_layer_bounds(200, 200);
    layer_tree_host()->outer_viewport_scroll_layer()->SetBounds(
        scroll_layer_bounds);
    layer_tree_host()->SetPageScaleFactorAndLimits(1.f, 0.01f, 100.f);
  }

  void WillBeginMainFrame() override {
    num_will_begin_main_frames_++;
    Layer* root_scroll_layer = layer_tree_host()->outer_viewport_scroll_layer();
    switch (num_will_begin_main_frames_) {
      case 1:
        // This will not be aborted because of the initial prop changes.
        EXPECT_EQ(0, num_impl_scrolls_);
        EXPECT_EQ(0, layer_tree_host()->source_frame_number());
        EXPECT_VECTOR_EQ(initial_scroll_, root_scroll_layer->scroll_offset());
        break;
      case 2:
        // This commit will not be aborted because of the scroll change.
        EXPECT_EQ(1, num_impl_scrolls_);
        EXPECT_EQ(1, layer_tree_host()->source_frame_number());
        EXPECT_VECTOR_EQ(
            gfx::ScrollOffsetWithDelta(initial_scroll_, impl_scroll_),
            root_scroll_layer->scroll_offset());
        root_scroll_layer->SetScrollOffset(gfx::ScrollOffsetWithDelta(
            root_scroll_layer->scroll_offset(), second_main_scroll_));
        break;
      case 3: {
        // This commit will be aborted.
        EXPECT_EQ(2, num_impl_scrolls_);
        // The source frame number still increases even with the abort.
        EXPECT_EQ(2, layer_tree_host()->source_frame_number());
        gfx::Vector2dF delta =
            impl_scroll_ + impl_scroll_ + second_main_scroll_;
        EXPECT_VECTOR_EQ(gfx::ScrollOffsetWithDelta(initial_scroll_, delta),
                         root_scroll_layer->scroll_offset());
        break;
      }
      case 4: {
        // This commit will also be aborted.
        EXPECT_EQ(3, num_impl_scrolls_);
        EXPECT_EQ(3, layer_tree_host()->source_frame_number());
        gfx::Vector2dF delta =
            impl_scroll_ + impl_scroll_ + impl_scroll_ + second_main_scroll_;
        EXPECT_VECTOR_EQ(gfx::ScrollOffsetWithDelta(initial_scroll_, delta),
                         root_scroll_layer->scroll_offset());
        break;
      }
    }
  }

  void DidBeginMainFrame() override { num_did_begin_main_frames_++; }

  void WillCommit() override { num_will_commits_++; }

  void DidCommit() override { num_did_commits_++; }

  void BeginCommitOnThread(LayerTreeHostImpl* impl) override {
    switch (num_impl_commits_) {
      case 1:
        // Redraw so that we keep scrolling.
        impl->SetNeedsRedraw();
        // Block activation until third commit is aborted.
        impl->BlockNotifyReadyToActivateForTesting(true);
        break;
    }
    num_impl_commits_++;
  }

  void BeginMainFrameAbortedOnThread(LayerTreeHostImpl* impl,
                                     CommitEarlyOutReason reason) override {
    switch (num_aborted_commits_) {
      case 0:
        EXPECT_EQ(2, num_impl_commits_);
        // Unblock activation when third commit is aborted.
        impl->BlockNotifyReadyToActivateForTesting(false);
        break;
      case 1:
        EXPECT_EQ(2, num_impl_commits_);
        // Redraw to end the test.
        impl->SetNeedsRedraw();
        break;
    }
    num_aborted_commits_++;
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    LayerImpl* root_scroll_layer = impl->OuterViewportScrollLayer();
    switch (impl->active_tree()->source_frame_number()) {
      case 0: {
        switch (num_impl_commits_) {
          case 1: {
            // First draw
            EXPECT_VECTOR_EQ(gfx::Vector2d(), ScrollDelta(root_scroll_layer));
            root_scroll_layer->ScrollBy(impl_scroll_);
            EXPECT_VECTOR_EQ(impl_scroll_, ScrollDelta(root_scroll_layer));
            EXPECT_VECTOR_EQ(
                initial_scroll_,
                ScrollTreeForLayer(root_scroll_layer)
                    ->GetScrollOffsetBaseForTesting(root_scroll_layer->id()));
            impl->SetNeedsCommit();
            break;
          }
          case 2: {
            // Second draw but no new active tree because activation is blocked.
            EXPECT_VECTOR_EQ(impl_scroll_, ScrollDelta(root_scroll_layer));
            root_scroll_layer->ScrollBy(impl_scroll_);
            EXPECT_VECTOR_EQ(impl_scroll_ + impl_scroll_,
                             ScrollDelta(root_scroll_layer));
            EXPECT_VECTOR_EQ(
                initial_scroll_,
                ScrollTreeForLayer(root_scroll_layer)
                    ->GetScrollOffsetBaseForTesting(root_scroll_layer->id()));
            // Ask for another commit (which will abort).
            impl->SetNeedsCommit();
            break;
          }
          default:
            NOTREACHED();
        }
        break;
      }
      case 1: {
        EXPECT_EQ(2, num_impl_commits_);
        // All scroll deltas so far should be consumed.
        EXPECT_EQ(gfx::Vector2d(), ScrollDelta(root_scroll_layer));
        switch (num_aborted_commits_) {
          case 1: {
            root_scroll_layer->ScrollBy(impl_scroll_);
            EXPECT_VECTOR_EQ(impl_scroll_, ScrollDelta(root_scroll_layer));
            gfx::Vector2dF prev_delta =
                impl_scroll_ + impl_scroll_ + second_main_scroll_;
            EXPECT_VECTOR_EQ(
                gfx::ScrollOffsetWithDelta(initial_scroll_, prev_delta),
                ScrollTreeForLayer(root_scroll_layer)
                    ->GetScrollOffsetBaseForTesting(root_scroll_layer->id()));
            // Ask for another commit (which will abort).
            impl->SetNeedsCommit();
            break;
          }
          case 2: {
            gfx::Vector2dF delta = impl_scroll_ + impl_scroll_ + impl_scroll_ +
                                   second_main_scroll_;
            EXPECT_VECTOR_EQ(
                gfx::ScrollOffsetWithDelta(initial_scroll_, delta),
                ScrollTreeForLayer(root_scroll_layer)
                    ->GetScrollOffsetBaseForTesting(root_scroll_layer->id()));
            // End test after second aborted commit (fourth commit request).
            EndTest();
            break;
          }
        }
        break;
      }
    }
    num_draws_++;
  }

  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float scale,
                           float top_controls_delta) override {
    num_impl_scrolls_++;
  }

  void AfterTest() override {
    EXPECT_EQ(3, num_impl_scrolls_);
    // Verify that the embedder sees aborted commits as real commits.
    EXPECT_EQ(4, num_will_begin_main_frames_);
    EXPECT_EQ(4, num_did_begin_main_frames_);
    EXPECT_EQ(4, num_will_commits_);
    EXPECT_EQ(4, num_did_commits_);
    // ...but the compositor thread only sees two real ones.
    EXPECT_EQ(2, num_impl_commits_);
    // ...and two aborted ones.
    EXPECT_EQ(2, num_aborted_commits_);
    // ...and four draws.
    EXPECT_EQ(4, num_draws_);
  }

 private:
  gfx::ScrollOffset initial_scroll_;
  gfx::Vector2dF impl_scroll_;
  gfx::Vector2dF second_main_scroll_;
  int num_will_begin_main_frames_;
  int num_did_begin_main_frames_;
  int num_will_commits_;
  int num_did_commits_;
  int num_impl_commits_;
  int num_aborted_commits_;
  int num_impl_scrolls_;
  int num_draws_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestScrollAbortedCommitMFBA);

}  // namespace
}  // namespace cc
