// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "base/memory/weak_ptr.h"
#include "cc/layers/content_layer.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/vector2d_conversions.h"

namespace cc {
namespace {

class LayerTreeHostScrollTest : public LayerTreeTest {};

class LayerTreeHostScrollTestScrollSimple : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollSimple()
      : initial_scroll_(10, 20),
        second_scroll_(40, 5),
        scroll_amount_(2, -1),
        num_scrolls_(0) {}

  virtual void BeginTest() OVERRIDE {
    Layer* root_layer = layer_tree_host()->root_layer();
    scoped_refptr<Layer> scroll_layer = Layer::Create();
    root_layer->AddChild(scroll_layer);
    // Create an effective max_scroll_offset of (100, 100).
    scroll_layer->SetBounds(gfx::Size(root_layer->bounds().width() + 100,
                                      root_layer->bounds().height() + 100));
    scroll_layer->SetIsDrawable(true);
    scroll_layer->SetIsContainerForFixedPositionLayers(true);
    scroll_layer->SetScrollClipLayerId(root_layer->id());
    scroll_layer->SetScrollOffset(initial_scroll_);
    layer_tree_host()->RegisterViewportLayers(root_layer, scroll_layer, NULL);
    PostSetNeedsCommitToMainThread();
  }

  virtual void Layout() OVERRIDE {
    Layer* root = layer_tree_host()->root_layer();
    Layer* scroll_layer = root->children()[0];
    if (!layer_tree_host()->source_frame_number()) {
      EXPECT_VECTOR_EQ(initial_scroll_, scroll_layer->scroll_offset());
    } else {
      EXPECT_VECTOR_EQ(initial_scroll_ + scroll_amount_,
                       scroll_layer->scroll_offset());

      // Pretend like Javascript updated the scroll position itself.
      scroll_layer->SetScrollOffset(second_scroll_);
    }
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* root = impl->active_tree()->root_layer();
    LayerImpl* scroll_layer = root->children()[0];
    EXPECT_VECTOR_EQ(gfx::Vector2d(), scroll_layer->ScrollDelta());

    scroll_layer->SetScrollClipLayer(root->id());
    scroll_layer->SetBounds(
        gfx::Size(root->bounds().width() + 100, root->bounds().height() + 100));
    scroll_layer->ScrollBy(scroll_amount_);

    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(initial_scroll_, scroll_layer->scroll_offset());
        EXPECT_VECTOR_EQ(scroll_amount_, scroll_layer->ScrollDelta());
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        EXPECT_VECTOR_EQ(scroll_layer->scroll_offset(), second_scroll_);
        EXPECT_VECTOR_EQ(scroll_layer->ScrollDelta(), scroll_amount_);
        EndTest();
        break;
    }
  }

  virtual void ApplyScrollAndScale(const gfx::Vector2d& scroll_delta,
                                   float scale) OVERRIDE {
    num_scrolls_++;
  }

  virtual void AfterTest() OVERRIDE { EXPECT_EQ(1, num_scrolls_); }

 private:
  gfx::Vector2d initial_scroll_;
  gfx::Vector2d second_scroll_;
  gfx::Vector2d scroll_amount_;
  int num_scrolls_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestScrollSimple);

class LayerTreeHostScrollTestScrollMultipleRedraw
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollMultipleRedraw()
      : initial_scroll_(40, 10), scroll_amount_(-3, 17), num_scrolls_(0) {}

  virtual void BeginTest() OVERRIDE {
    Layer* root_layer = layer_tree_host()->root_layer();
    scroll_layer_ = Layer::Create();
    root_layer->AddChild(scroll_layer_);
    // Create an effective max_scroll_offset of (100, 100).
    scroll_layer_->SetBounds(gfx::Size(root_layer->bounds().width() + 100,
                                       root_layer->bounds().height() + 100));
    scroll_layer_->SetIsDrawable(true);
    scroll_layer_->SetIsContainerForFixedPositionLayers(true);
    scroll_layer_->SetScrollClipLayerId(root_layer->id());
    scroll_layer_->SetScrollOffset(initial_scroll_);
    layer_tree_host()->RegisterViewportLayers(root_layer, scroll_layer_, NULL);
    PostSetNeedsCommitToMainThread();
  }

  virtual void BeginCommitOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    switch (layer_tree_host()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(scroll_layer_->scroll_offset(), initial_scroll_);
        break;
      case 1:
        EXPECT_VECTOR_EQ(scroll_layer_->scroll_offset(),
                         initial_scroll_ + scroll_amount_ + scroll_amount_);
      case 2:
        EXPECT_VECTOR_EQ(scroll_layer_->scroll_offset(),
                         initial_scroll_ + scroll_amount_ + scroll_amount_);
        break;
    }
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* scroll_layer =
        impl->active_tree()->LayerById(scroll_layer_->id());
    if (impl->active_tree()->source_frame_number() == 0 &&
        impl->SourceAnimationFrameNumber() == 1) {
      // First draw after first commit.
      EXPECT_VECTOR_EQ(scroll_layer->ScrollDelta(), gfx::Vector2d());
      scroll_layer->ScrollBy(scroll_amount_);
      EXPECT_VECTOR_EQ(scroll_layer->ScrollDelta(), scroll_amount_);

      EXPECT_VECTOR_EQ(scroll_layer->scroll_offset(), initial_scroll_);
      PostSetNeedsRedrawToMainThread();
    } else if (impl->active_tree()->source_frame_number() == 0 &&
               impl->SourceAnimationFrameNumber() == 2) {
      // Second draw after first commit.
      EXPECT_EQ(scroll_layer->ScrollDelta(), scroll_amount_);
      scroll_layer->ScrollBy(scroll_amount_);
      EXPECT_VECTOR_EQ(scroll_layer->ScrollDelta(),
                       scroll_amount_ + scroll_amount_);

      EXPECT_VECTOR_EQ(scroll_layer_->scroll_offset(), initial_scroll_);
      PostSetNeedsCommitToMainThread();
    } else if (impl->active_tree()->source_frame_number() == 1) {
      // Third or later draw after second commit.
      EXPECT_GE(impl->SourceAnimationFrameNumber(), 3);
      EXPECT_VECTOR_EQ(scroll_layer_->ScrollDelta(), gfx::Vector2d());
      EXPECT_VECTOR_EQ(scroll_layer_->scroll_offset(),
                       initial_scroll_ + scroll_amount_ + scroll_amount_);
      EndTest();
    }
  }

  virtual void ApplyScrollAndScale(const gfx::Vector2d& scroll_delta,
                                   float scale) OVERRIDE {
    num_scrolls_++;
  }

  virtual void AfterTest() OVERRIDE { EXPECT_EQ(1, num_scrolls_); }

 private:
  gfx::Vector2d initial_scroll_;
  gfx::Vector2d scroll_amount_;
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

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void SetupTree() OVERRIDE {
    LayerTreeHostScrollTest::SetupTree();
    Layer* root_layer = layer_tree_host()->root_layer();
    scoped_refptr<Layer> root_scroll_layer = Layer::Create();
    root_scroll_layer->SetScrollClipLayerId(root_layer->id());
    root_scroll_layer->SetScrollOffset(initial_scroll_);
    root_scroll_layer->SetBounds(gfx::Size(200, 200));
    root_scroll_layer->SetIsDrawable(true);
    root_scroll_layer->SetIsContainerForFixedPositionLayers(true);
    root_layer->AddChild(root_scroll_layer);

    layer_tree_host()->RegisterViewportLayers(
        root_layer, root_scroll_layer, NULL);
    layer_tree_host()->SetPageScaleFactorAndLimits(1.f, 0.01f, 100.f);
  }

  virtual void WillBeginMainFrame() OVERRIDE {
    num_will_begin_main_frames_++;
    Layer* root_scroll_layer = layer_tree_host()->root_layer()->children()[0];
    switch (num_will_begin_main_frames_) {
      case 1:
        // This will not be aborted because of the initial prop changes.
        EXPECT_EQ(0, num_impl_scrolls_);
        EXPECT_EQ(0, layer_tree_host()->source_frame_number());
        EXPECT_VECTOR_EQ(root_scroll_layer->scroll_offset(), initial_scroll_);
        EXPECT_EQ(1.f, layer_tree_host()->page_scale_factor());
        break;
      case 2:
        // This commit will be aborted, and another commit will be
        // initiated from the redraw.
        EXPECT_EQ(1, num_impl_scrolls_);
        EXPECT_EQ(1, layer_tree_host()->source_frame_number());
        EXPECT_VECTOR_EQ(root_scroll_layer->scroll_offset(),
                         initial_scroll_ + impl_scroll_);
        EXPECT_EQ(impl_scale_, layer_tree_host()->page_scale_factor());
        PostSetNeedsRedrawToMainThread();
        break;
      case 3:
        // This commit will not be aborted because of the scroll change.
        EXPECT_EQ(2, num_impl_scrolls_);
        // The source frame number still increases even with the abort.
        EXPECT_EQ(2, layer_tree_host()->source_frame_number());
        EXPECT_VECTOR_EQ(root_scroll_layer->scroll_offset(),
                         initial_scroll_ + impl_scroll_ + impl_scroll_);
        EXPECT_EQ(impl_scale_ * impl_scale_,
                  layer_tree_host()->page_scale_factor());
        root_scroll_layer->SetScrollOffset(root_scroll_layer->scroll_offset() +
                                           second_main_scroll_);
        break;
      case 4:
        // This commit will also be aborted.
        EXPECT_EQ(3, num_impl_scrolls_);
        EXPECT_EQ(3, layer_tree_host()->source_frame_number());
        EXPECT_VECTOR_EQ(root_scroll_layer->scroll_offset(),
                         initial_scroll_ + impl_scroll_ + impl_scroll_ +
                             impl_scroll_ + second_main_scroll_);
        // End the test by drawing to verify this commit is also aborted.
        PostSetNeedsRedrawToMainThread();
        break;
    }
  }

  virtual void DidBeginMainFrame() OVERRIDE { num_did_begin_main_frames_++; }

  virtual void WillCommit() OVERRIDE { num_will_commits_++; }

  virtual void DidCommit() OVERRIDE { num_did_commits_++; }

  virtual void BeginCommitOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    num_impl_commits_++;
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* root_scroll_layer =
        impl->active_tree()->root_layer()->children()[0];

    if (impl->active_tree()->source_frame_number() == 0 &&
        impl->SourceAnimationFrameNumber() == 1) {
      // First draw
      EXPECT_VECTOR_EQ(root_scroll_layer->ScrollDelta(), gfx::Vector2d());
      root_scroll_layer->ScrollBy(impl_scroll_);
      EXPECT_VECTOR_EQ(root_scroll_layer->ScrollDelta(), impl_scroll_);
      EXPECT_VECTOR_EQ(root_scroll_layer->scroll_offset(), initial_scroll_);

      EXPECT_EQ(1.f, impl->active_tree()->page_scale_delta());
      EXPECT_EQ(1.f, impl->active_tree()->total_page_scale_factor());
      impl->active_tree()->SetPageScaleDelta(impl_scale_);
      EXPECT_EQ(impl_scale_, impl->active_tree()->page_scale_delta());
      EXPECT_EQ(impl_scale_, impl->active_tree()->total_page_scale_factor());

      // To simplify the testing flow, don't redraw here, just commit.
      impl->SetNeedsCommit();
    } else if (impl->active_tree()->source_frame_number() == 0 &&
               impl->SourceAnimationFrameNumber() == 2) {
      // Test a second draw after an aborted commit.
      // The scroll/scale values should be baked into the offset/scale factor
      // since the main thread consumed but aborted the begin frame.
      EXPECT_VECTOR_EQ(root_scroll_layer->ScrollDelta(), gfx::Vector2d());
      root_scroll_layer->ScrollBy(impl_scroll_);
      EXPECT_VECTOR_EQ(root_scroll_layer->ScrollDelta(), impl_scroll_);
      EXPECT_VECTOR_EQ(root_scroll_layer->scroll_offset(),
                       initial_scroll_ + impl_scroll_);

      EXPECT_EQ(1.f, impl->active_tree()->page_scale_delta());
      EXPECT_EQ(impl_scale_, impl->active_tree()->total_page_scale_factor());
      impl->active_tree()->SetPageScaleDelta(impl_scale_);
      EXPECT_EQ(impl_scale_, impl->active_tree()->page_scale_delta());
      EXPECT_EQ(impl_scale_ * impl_scale_,
                impl->active_tree()->total_page_scale_factor());

      impl->SetNeedsCommit();
    } else if (impl->active_tree()->source_frame_number() == 1) {
      // Commit for source frame 1 is aborted.
      NOTREACHED();
    } else if (impl->active_tree()->source_frame_number() == 2 &&
               impl->SourceAnimationFrameNumber() == 3) {
      // Third draw after the second full commit.
      EXPECT_EQ(root_scroll_layer->ScrollDelta(), gfx::Vector2d());
      root_scroll_layer->ScrollBy(impl_scroll_);
      impl->SetNeedsCommit();
      EXPECT_VECTOR_EQ(root_scroll_layer->ScrollDelta(), impl_scroll_);
      EXPECT_VECTOR_EQ(
          root_scroll_layer->scroll_offset(),
          initial_scroll_ + impl_scroll_ + impl_scroll_ + second_main_scroll_);
    } else if (impl->active_tree()->source_frame_number() == 2 &&
               impl->SourceAnimationFrameNumber() == 4) {
      // Final draw after the second aborted commit.
      EXPECT_VECTOR_EQ(root_scroll_layer->ScrollDelta(), gfx::Vector2d());
      EXPECT_VECTOR_EQ(root_scroll_layer->scroll_offset(),
                       initial_scroll_ + impl_scroll_ + impl_scroll_ +
                           impl_scroll_ + second_main_scroll_);
      EndTest();
    } else {
      // Commit for source frame 3 is aborted.
      NOTREACHED();
    }
  }

  virtual void ApplyScrollAndScale(const gfx::Vector2d& scroll_delta,
                                   float scale) OVERRIDE {
    num_impl_scrolls_++;
  }

  virtual void AfterTest() OVERRIDE {
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
  gfx::Vector2d initial_scroll_;
  gfx::Vector2d impl_scroll_;
  gfx::Vector2d second_main_scroll_;
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

  virtual void SetupTree() OVERRIDE {
    LayerTreeHostScrollTest::SetupTree();
    Layer* root_layer = layer_tree_host()->root_layer();
    scoped_refptr<Layer> root_scroll_layer = Layer::Create();
    root_scroll_layer->SetScrollClipLayerId(root_layer->id());
    root_scroll_layer->SetBounds(
        gfx::Size(root_layer->bounds().width() + 100,
                  root_layer->bounds().height() + 100));
    root_scroll_layer->SetIsDrawable(true);
    root_scroll_layer->SetIsContainerForFixedPositionLayers(true);
    root_layer->AddChild(root_scroll_layer);

    layer_tree_host()->RegisterViewportLayers(
        root_layer, root_scroll_layer, NULL);
    layer_tree_host()->SetPageScaleFactorAndLimits(1.f, 0.01f, 100.f);
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* root = impl->active_tree()->root_layer();
    LayerImpl* scroll_layer = root->children()[0];

    // Check that a fractional scroll delta is correctly accumulated over
    // multiple commits.
    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(scroll_layer->scroll_offset(), gfx::Vector2d(0, 0));
        EXPECT_VECTOR_EQ(scroll_layer->ScrollDelta(), gfx::Vector2d(0, 0));
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        EXPECT_VECTOR_EQ(scroll_layer->scroll_offset(),
                         gfx::ToFlooredVector2d(scroll_amount_));
        EXPECT_VECTOR_EQ(scroll_layer->ScrollDelta(),
                         gfx::Vector2dF(fmod(scroll_amount_.x(), 1.0f), 0.0f));
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        EXPECT_VECTOR_EQ(
            scroll_layer->scroll_offset(),
            gfx::ToFlooredVector2d(scroll_amount_ + scroll_amount_));
        EXPECT_VECTOR_EQ(
            scroll_layer->ScrollDelta(),
            gfx::Vector2dF(fmod(2.0f * scroll_amount_.x(), 1.0f), 0.0f));
        EndTest();
        break;
    }
    scroll_layer->ScrollBy(scroll_amount_);
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  gfx::Vector2dF scroll_amount_;
};

MULTI_THREAD_TEST_F(LayerTreeHostScrollTestFractionalScroll);

class LayerTreeHostScrollTestCaseWithChild : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestCaseWithChild()
      : initial_offset_(10, 20),
        javascript_scroll_(40, 5),
        scroll_amount_(2, -1),
        num_scrolls_(0) {}

  virtual void SetupTree() OVERRIDE {
    layer_tree_host()->SetDeviceScaleFactor(device_scale_factor_);

    scoped_refptr<Layer> root_layer = Layer::Create();
    root_layer->SetBounds(gfx::Size(10, 10));

    root_scroll_layer_ = ContentLayer::Create(&fake_content_layer_client_);
    root_scroll_layer_->SetBounds(gfx::Size(110, 110));

    root_scroll_layer_->SetPosition(gfx::Point());

    root_scroll_layer_->SetIsDrawable(true);
    root_scroll_layer_->SetScrollClipLayerId(root_layer->id());
    root_scroll_layer_->SetIsContainerForFixedPositionLayers(true);
    root_layer->AddChild(root_scroll_layer_);

    child_layer_ = ContentLayer::Create(&fake_content_layer_client_);
    child_layer_->set_did_scroll_callback(
        base::Bind(&LayerTreeHostScrollTestCaseWithChild::DidScroll,
                   base::Unretained(this)));
    child_layer_->SetBounds(gfx::Size(110, 110));

    if (scroll_child_layer_) {
      // Scrolls on the child layer will happen at 5, 5. If they are treated
      // like device pixels, and device scale factor is 2, then they will
      // be considered at 2.5, 2.5 in logical pixels, and will miss this layer.
      child_layer_->SetPosition(gfx::Point(5, 5));
    } else {
      // Adjust the child layer horizontally so that scrolls will never hit it.
      child_layer_->SetPosition(gfx::Point(60, 5));
    }

    child_layer_->SetIsDrawable(true);
    child_layer_->SetScrollClipLayerId(root_layer->id());
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
    layer_tree_host()->RegisterViewportLayers(
        root_layer, root_scroll_layer_, NULL);
    LayerTreeHostScrollTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void WillCommit() OVERRIDE {
    // Keep the test committing (otherwise the early out for no update
    // will stall the test).
    if (layer_tree_host()->source_frame_number() < 2) {
      layer_tree_host()->SetNeedsCommit();
    }
  }

  void DidScroll() {
    final_scroll_offset_ = expected_scroll_layer_->scroll_offset();
  }

  virtual void ApplyScrollAndScale(const gfx::Vector2d& scroll_delta,
                                   float scale) OVERRIDE {
    num_scrolls_++;
  }

  virtual void Layout() OVERRIDE {
    EXPECT_VECTOR_EQ(gfx::Vector2d(),
                     expected_no_scroll_layer_->scroll_offset());

    switch (layer_tree_host()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(initial_offset_,
                         expected_scroll_layer_->scroll_offset());
        break;
      case 1:
        EXPECT_VECTOR_EQ(initial_offset_ + scroll_amount_,
                         expected_scroll_layer_->scroll_offset());

        // Pretend like Javascript updated the scroll position itself.
        expected_scroll_layer_->SetScrollOffset(javascript_scroll_);
        break;
      case 2:
        EXPECT_VECTOR_EQ(javascript_scroll_ + scroll_amount_,
                         expected_scroll_layer_->scroll_offset());
        break;
    }
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* root_impl = impl->active_tree()->root_layer();
    LayerImpl* root_scroll_layer_impl = root_impl->children()[0];
    LayerImpl* child_layer_impl = root_scroll_layer_impl->children()[0];

    LayerImpl* expected_scroll_layer_impl = NULL;
    LayerImpl* expected_no_scroll_layer_impl = NULL;
    if (scroll_child_layer_) {
      expected_scroll_layer_impl = child_layer_impl;
      expected_no_scroll_layer_impl = root_scroll_layer_impl;
    } else {
      expected_scroll_layer_impl = root_scroll_layer_impl;
      expected_no_scroll_layer_impl = child_layer_impl;
    }

    EXPECT_VECTOR_EQ(gfx::Vector2d(), root_impl->ScrollDelta());
    EXPECT_VECTOR_EQ(gfx::Vector2d(),
                     expected_no_scroll_layer_impl->ScrollDelta());

    // Ensure device scale factor is affecting the layers.
    gfx::Size expected_content_bounds = gfx::ToCeiledSize(
        gfx::ScaleSize(root_scroll_layer_impl->bounds(), device_scale_factor_));
    EXPECT_SIZE_EQ(expected_content_bounds,
                   root_scroll_layer_->content_bounds());

    expected_content_bounds = gfx::ToCeiledSize(
        gfx::ScaleSize(child_layer_impl->bounds(), device_scale_factor_));
    EXPECT_SIZE_EQ(expected_content_bounds, child_layer_->content_bounds());

    switch (impl->active_tree()->source_frame_number()) {
      case 0: {
        // Gesture scroll on impl thread.
        InputHandler::ScrollStatus status = impl->ScrollBegin(
            gfx::ToCeiledPoint(expected_scroll_layer_impl->position() -
                               gfx::Vector2dF(0.5f, 0.5f)),
            InputHandler::Gesture);
        EXPECT_EQ(InputHandler::ScrollStarted, status);
        impl->ScrollBy(gfx::Point(), scroll_amount_);
        impl->ScrollEnd();

        // Check the scroll is applied as a delta.
        EXPECT_VECTOR_EQ(initial_offset_,
                         expected_scroll_layer_impl->scroll_offset());
        EXPECT_VECTOR_EQ(scroll_amount_,
                         expected_scroll_layer_impl->ScrollDelta());
        break;
      }
      case 1: {
        // Wheel scroll on impl thread.
        InputHandler::ScrollStatus status = impl->ScrollBegin(
            gfx::ToCeiledPoint(expected_scroll_layer_impl->position() +
                               gfx::Vector2dF(0.5f, 0.5f)),
            InputHandler::Wheel);
        EXPECT_EQ(InputHandler::ScrollStarted, status);
        impl->ScrollBy(gfx::Point(), scroll_amount_);
        impl->ScrollEnd();

        // Check the scroll is applied as a delta.
        EXPECT_VECTOR_EQ(javascript_scroll_,
                         expected_scroll_layer_impl->scroll_offset());
        EXPECT_VECTOR_EQ(scroll_amount_,
                         expected_scroll_layer_impl->ScrollDelta());
        break;
      }
      case 2:

        EXPECT_VECTOR_EQ(javascript_scroll_ + scroll_amount_,
                         expected_scroll_layer_impl->scroll_offset());
        EXPECT_VECTOR_EQ(gfx::Vector2d(),
                         expected_scroll_layer_impl->ScrollDelta());

        EndTest();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {
    if (scroll_child_layer_) {
      EXPECT_EQ(0, num_scrolls_);
      EXPECT_VECTOR_EQ(javascript_scroll_ + scroll_amount_,
                       final_scroll_offset_);
    } else {
      EXPECT_EQ(2, num_scrolls_);
      EXPECT_VECTOR_EQ(gfx::Vector2d(), final_scroll_offset_);
    }
  }

 protected:
  float device_scale_factor_;
  bool scroll_child_layer_;

  gfx::Vector2d initial_offset_;
  gfx::Vector2d javascript_scroll_;
  gfx::Vector2d scroll_amount_;
  int num_scrolls_;
  gfx::Vector2d final_scroll_offset_;

  FakeContentLayerClient fake_content_layer_client_;

  scoped_refptr<Layer> root_scroll_layer_;
  scoped_refptr<Layer> child_layer_;
  scoped_refptr<Layer> expected_scroll_layer_;
  scoped_refptr<Layer> expected_no_scroll_layer_;
};

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor1_ScrollChild_DirectRenderer_MainThreadPaint) {
  device_scale_factor_ = 1.f;
  scroll_child_layer_ = true;
  RunTest(true, false, false);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor1_ScrollChild_DirectRenderer_ImplSidePaint) {
  device_scale_factor_ = 1.f;
  scroll_child_layer_ = true;
  RunTest(true, false, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor1_ScrollChild_DelegatingRenderer_MainThreadPaint) {
  device_scale_factor_ = 1.f;
  scroll_child_layer_ = true;
  RunTest(true, true, false);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor1_ScrollChild_DelegatingRenderer_ImplSidePaint) {
  device_scale_factor_ = 1.f;
  scroll_child_layer_ = true;
  RunTest(true, true, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor15_ScrollChild_DirectRenderer) {
  device_scale_factor_ = 1.5f;
  scroll_child_layer_ = true;
  RunTest(true, false, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor15_ScrollChild_DelegatingRenderer) {
  device_scale_factor_ = 1.5f;
  scroll_child_layer_ = true;
  RunTest(true, true, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor2_ScrollChild_DirectRenderer) {
  device_scale_factor_ = 2.f;
  scroll_child_layer_ = true;
  RunTest(true, false, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor2_ScrollChild_DelegatingRenderer) {
  device_scale_factor_ = 2.f;
  scroll_child_layer_ = true;
  RunTest(true, true, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor1_ScrollRootScrollLayer_DirectRenderer) {
  device_scale_factor_ = 1.f;
  scroll_child_layer_ = false;
  RunTest(true, false, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor1_ScrollRootScrollLayer_DelegatingRenderer) {
  device_scale_factor_ = 1.f;
  scroll_child_layer_ = false;
  RunTest(true, true, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor15_ScrollRootScrollLayer_DirectRenderer) {
  device_scale_factor_ = 1.5f;
  scroll_child_layer_ = false;
  RunTest(true, false, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor15_ScrollRootScrollLayer_DelegatingRenderer) {
  device_scale_factor_ = 1.5f;
  scroll_child_layer_ = false;
  RunTest(true, true, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor2_ScrollRootScrollLayer_DirectRenderer_MainSidePaint) {
  device_scale_factor_ = 2.f;
  scroll_child_layer_ = false;
  RunTest(true, false, false);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor2_ScrollRootScrollLayer_DirectRenderer_ImplSidePaint) {
  device_scale_factor_ = 2.f;
  scroll_child_layer_ = false;
  RunTest(true, false, true);
}

TEST_F(LayerTreeHostScrollTestCaseWithChild,
       DeviceScaleFactor2_ScrollRootScrollLayer_DelegatingRenderer) {
  device_scale_factor_ = 2.f;
  scroll_child_layer_ = false;
  RunTest(true, true, true);
}

class ImplSidePaintingScrollTest : public LayerTreeHostScrollTest {
 public:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->impl_side_painting = true;
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    if (impl->pending_tree())
      impl->SetNeedsRedraw();
  }
};

class ImplSidePaintingScrollTestSimple : public ImplSidePaintingScrollTest {
 public:
  ImplSidePaintingScrollTestSimple()
      : initial_scroll_(10, 20),
        main_thread_scroll_(40, 5),
        impl_thread_scroll1_(2, -1),
        impl_thread_scroll2_(-3, 10),
        num_scrolls_(0) {}

  virtual void SetupTree() OVERRIDE {
    LayerTreeHostScrollTest::SetupTree();
    Layer* root_layer = layer_tree_host()->root_layer();
    scoped_refptr<Layer> root_scroll_layer = Layer::Create();
    root_scroll_layer->SetScrollClipLayerId(root_layer->id());
    root_scroll_layer->SetScrollOffset(initial_scroll_);
    root_scroll_layer->SetBounds(
        gfx::Size(root_layer->bounds().width() + 100,
                  root_layer->bounds().height() + 100));
    root_scroll_layer->SetIsDrawable(true);
    root_scroll_layer->SetIsContainerForFixedPositionLayers(true);
    root_layer->AddChild(root_scroll_layer);

    layer_tree_host()->RegisterViewportLayers(
        root_layer, root_scroll_layer, NULL);
    layer_tree_host()->SetPageScaleFactorAndLimits(1.f, 0.01f, 100.f);
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void Layout() OVERRIDE {
    Layer* root = layer_tree_host()->root_layer();
    Layer* scroll_layer = root->children()[0];
    if (!layer_tree_host()->source_frame_number()) {
      EXPECT_VECTOR_EQ(scroll_layer->scroll_offset(), initial_scroll_);
    } else {
      EXPECT_VECTOR_EQ(scroll_layer->scroll_offset(),
                       initial_scroll_ + impl_thread_scroll1_);

      // Pretend like Javascript updated the scroll position itself with a
      // change of main_thread_scroll.
      scroll_layer->SetScrollOffset(initial_scroll_ + main_thread_scroll_ +
                                    impl_thread_scroll1_);
    }
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    // We force a second draw here of the first commit before activating
    // the second commit.
    if (impl->active_tree()->source_frame_number() == 0)
      impl->SetNeedsRedraw();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    ImplSidePaintingScrollTest::DrawLayersOnThread(impl);

    LayerImpl* root = impl->active_tree()->root_layer();
    LayerImpl* scroll_layer = root->children()[0];
    LayerImpl* pending_root =
        impl->active_tree()->FindPendingTreeLayerById(root->id());

    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        if (!impl->pending_tree()) {
          impl->BlockNotifyReadyToActivateForTesting(true);
          EXPECT_VECTOR_EQ(scroll_layer->ScrollDelta(), gfx::Vector2d());
          scroll_layer->ScrollBy(impl_thread_scroll1_);

          EXPECT_VECTOR_EQ(scroll_layer->scroll_offset(), initial_scroll_);
          EXPECT_VECTOR_EQ(scroll_layer->ScrollDelta(), impl_thread_scroll1_);
          EXPECT_VECTOR_EQ(scroll_layer->sent_scroll_delta(), gfx::Vector2d());
          PostSetNeedsCommitToMainThread();

          // CommitCompleteOnThread will trigger this function again
          // and cause us to take the else clause.
        } else {
          impl->BlockNotifyReadyToActivateForTesting(false);
          ASSERT_TRUE(pending_root);
          EXPECT_EQ(impl->pending_tree()->source_frame_number(), 1);

          scroll_layer->ScrollBy(impl_thread_scroll2_);
          EXPECT_VECTOR_EQ(scroll_layer->scroll_offset(), initial_scroll_);
          EXPECT_VECTOR_EQ(scroll_layer->ScrollDelta(),
                           impl_thread_scroll1_ + impl_thread_scroll2_);
          EXPECT_VECTOR_EQ(scroll_layer->sent_scroll_delta(),
                           impl_thread_scroll1_);

          LayerImpl* pending_scroll_layer = pending_root->children()[0];
          EXPECT_VECTOR_EQ(
              pending_scroll_layer->scroll_offset(),
              initial_scroll_ + main_thread_scroll_ + impl_thread_scroll1_);
          EXPECT_VECTOR_EQ(pending_scroll_layer->ScrollDelta(),
                           impl_thread_scroll2_);
          EXPECT_VECTOR_EQ(pending_scroll_layer->sent_scroll_delta(),
                           gfx::Vector2d());
        }
        break;
      case 1:
        EXPECT_FALSE(impl->pending_tree());
        EXPECT_VECTOR_EQ(
            scroll_layer->scroll_offset(),
            initial_scroll_ + main_thread_scroll_ + impl_thread_scroll1_);
        EXPECT_VECTOR_EQ(scroll_layer->ScrollDelta(), impl_thread_scroll2_);
        EXPECT_VECTOR_EQ(scroll_layer->sent_scroll_delta(), gfx::Vector2d());
        EndTest();
        break;
    }
  }

  virtual void ApplyScrollAndScale(const gfx::Vector2d& scroll_delta,
                                   float scale) OVERRIDE {
    num_scrolls_++;
  }

  virtual void AfterTest() OVERRIDE { EXPECT_EQ(1, num_scrolls_); }

 private:
  gfx::Vector2d initial_scroll_;
  gfx::Vector2d main_thread_scroll_;
  gfx::Vector2d impl_thread_scroll1_;
  gfx::Vector2d impl_thread_scroll2_;
  int num_scrolls_;
};

MULTI_THREAD_TEST_F(ImplSidePaintingScrollTestSimple);

// This test makes sure that layers pick up scrolls that occur between
// beginning a commit and finishing a commit (aka scroll deltas not
// included in sent scroll delta) still apply to layers that don't
// push properties.
class ImplSidePaintingScrollTestImplOnlyScroll
    : public ImplSidePaintingScrollTest {
 public:
  ImplSidePaintingScrollTestImplOnlyScroll()
      : initial_scroll_(20, 10), impl_thread_scroll_(-2, 3) {}

  virtual void SetupTree() OVERRIDE {
    LayerTreeHostScrollTest::SetupTree();
    Layer* root_layer = layer_tree_host()->root_layer();
    scoped_refptr<Layer> root_scroll_layer = Layer::Create();
    root_scroll_layer->SetScrollClipLayerId(root_layer->id());
    root_scroll_layer->SetScrollOffset(initial_scroll_);
    root_scroll_layer->SetBounds(
        gfx::Size(root_layer->bounds().width() + 100,
                  root_layer->bounds().height() + 100));
    root_scroll_layer->SetIsDrawable(true);
    root_scroll_layer->SetIsContainerForFixedPositionLayers(true);
    root_layer->AddChild(root_scroll_layer);

    layer_tree_host()->RegisterViewportLayers(
        root_layer, root_scroll_layer, NULL);
    layer_tree_host()->SetPageScaleFactorAndLimits(1.f, 0.01f, 100.f);
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void WillCommit() OVERRIDE {
    Layer* root = layer_tree_host()->root_layer();
    Layer* scroll_layer = root->children()[0];
    switch (layer_tree_host()->source_frame_number()) {
      case 0:
        EXPECT_TRUE(scroll_layer->needs_push_properties());
        break;
      case 1:
        // Even if this layer doesn't need push properties, it should
        // still pick up scrolls that happen on the active layer during
        // commit.
        EXPECT_FALSE(scroll_layer->needs_push_properties());
        break;
    }
  }

  virtual void BeginCommitOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    // Scroll after the 2nd commit has started.
    if (impl->active_tree()->source_frame_number() == 0) {
      LayerImpl* active_root = impl->active_tree()->root_layer();
      LayerImpl* active_scroll_layer = active_root->children()[0];
      ASSERT_TRUE(active_root);
      ASSERT_TRUE(active_scroll_layer);
      active_scroll_layer->ScrollBy(impl_thread_scroll_);
    }
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    // We force a second draw here of the first commit before activating
    // the second commit.
    LayerImpl* active_root = impl->active_tree()->root_layer();
    LayerImpl* active_scroll_layer =
        active_root ? active_root->children()[0] : NULL;
    LayerImpl* pending_root = impl->pending_tree()->root_layer();
    LayerImpl* pending_scroll_layer = pending_root->children()[0];

    ASSERT_TRUE(pending_root);
    ASSERT_TRUE(pending_scroll_layer);
    switch (impl->pending_tree()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(pending_scroll_layer->scroll_offset(),
                         initial_scroll_);
        EXPECT_VECTOR_EQ(pending_scroll_layer->ScrollDelta(), gfx::Vector2d());
        EXPECT_VECTOR_EQ(pending_scroll_layer->sent_scroll_delta(),
                         gfx::Vector2d());
        EXPECT_FALSE(active_root);
        break;
      case 1:
        // Even though the scroll happened during the commit, both layers
        // should have the appropriate scroll delta.
        EXPECT_VECTOR_EQ(pending_scroll_layer->scroll_offset(),
                         initial_scroll_);
        EXPECT_VECTOR_EQ(pending_scroll_layer->ScrollDelta(),
                         impl_thread_scroll_);
        EXPECT_VECTOR_EQ(pending_scroll_layer->sent_scroll_delta(),
                         gfx::Vector2d());
        ASSERT_TRUE(active_root);
        EXPECT_VECTOR_EQ(active_scroll_layer->scroll_offset(), initial_scroll_);
        EXPECT_VECTOR_EQ(active_scroll_layer->ScrollDelta(),
                         impl_thread_scroll_);
        EXPECT_VECTOR_EQ(active_scroll_layer->sent_scroll_delta(),
                         gfx::Vector2d());
        break;
      case 2:
        // On the next commit, this delta should have been sent and applied.
        EXPECT_VECTOR_EQ(pending_scroll_layer->scroll_offset(),
                         initial_scroll_ + impl_thread_scroll_);
        EXPECT_VECTOR_EQ(pending_scroll_layer->ScrollDelta(), gfx::Vector2d());
        EXPECT_VECTOR_EQ(pending_scroll_layer->sent_scroll_delta(),
                         gfx::Vector2d());
        EndTest();
        break;
    }
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    ImplSidePaintingScrollTest::DrawLayersOnThread(impl);

    LayerImpl* root = impl->active_tree()->root_layer();
    LayerImpl* scroll_layer = root->children()[0];

    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_VECTOR_EQ(scroll_layer->scroll_offset(), initial_scroll_);
        EXPECT_VECTOR_EQ(scroll_layer->ScrollDelta(), gfx::Vector2d());
        EXPECT_VECTOR_EQ(scroll_layer->sent_scroll_delta(), gfx::Vector2d());
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        EXPECT_VECTOR_EQ(scroll_layer->scroll_offset(), initial_scroll_);
        EXPECT_VECTOR_EQ(scroll_layer->ScrollDelta(), impl_thread_scroll_);
        EXPECT_VECTOR_EQ(scroll_layer->sent_scroll_delta(), gfx::Vector2d());
        PostSetNeedsCommitToMainThread();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  gfx::Vector2d initial_scroll_;
  gfx::Vector2d impl_thread_scroll_;
};

MULTI_THREAD_TEST_F(ImplSidePaintingScrollTestImplOnlyScroll);

class LayerTreeHostScrollTestScrollZeroMaxScrollOffset
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestScrollZeroMaxScrollOffset() {}

  virtual void SetupTree() OVERRIDE {
    LayerTreeTest::SetupTree();
    scoped_refptr<Layer> scroll_layer = Layer::Create();
    layer_tree_host()->root_layer()->AddChild(scroll_layer);
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* root = impl->active_tree()->root_layer();
    LayerImpl* scroll_layer = root->children()[0];
    scroll_layer->SetScrollClipLayer(root->id());

    // Set max_scroll_offset = (100, 100).
    scroll_layer->SetBounds(
        gfx::Size(root->bounds().width() + 100, root->bounds().height() + 100));
    EXPECT_EQ(InputHandler::ScrollStarted,
              scroll_layer->TryScroll(gfx::PointF(0.0f, 1.0f),
                                      InputHandler::Gesture));

    // Set max_scroll_offset = (0, 0).
    scroll_layer->SetBounds(root->bounds());
    EXPECT_EQ(InputHandler::ScrollIgnored,
              scroll_layer->TryScroll(gfx::PointF(0.0f, 1.0f),
                                      InputHandler::Gesture));

    // Set max_scroll_offset = (-100, -100).
    scroll_layer->SetBounds(gfx::Size());
    EXPECT_EQ(InputHandler::ScrollIgnored,
              scroll_layer->TryScroll(gfx::PointF(0.0f, 1.0f),
                                      InputHandler::Gesture));

    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostScrollTestScrollZeroMaxScrollOffset);

class ThreadCheckingInputHandlerClient : public InputHandlerClient {
 public:
  ThreadCheckingInputHandlerClient(base::SingleThreadTaskRunner* runner,
                                   bool* received_stop_flinging)
      : task_runner_(runner), received_stop_flinging_(received_stop_flinging) {}

  virtual void WillShutdown() OVERRIDE {
    if (!received_stop_flinging_)
      ADD_FAILURE() << "WillShutdown() called before fling stopped";
  }

  virtual void Animate(base::TimeTicks time) OVERRIDE {
    if (!task_runner_->BelongsToCurrentThread())
      ADD_FAILURE() << "Animate called on wrong thread";
  }

  virtual void MainThreadHasStoppedFlinging() OVERRIDE {
    if (!task_runner_->BelongsToCurrentThread())
      ADD_FAILURE() << "MainThreadHasStoppedFlinging called on wrong thread";
    *received_stop_flinging_ = true;
  }

  virtual void DidOverscroll(const gfx::Vector2dF& accumulated_overscroll,
                             const gfx::Vector2dF& latest_overscroll_delta)
      OVERRIDE {
    if (!task_runner_->BelongsToCurrentThread())
      ADD_FAILURE() << "DidOverscroll called on wrong thread";
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
          impl_thread.message_loop_proxy().get(), &received_stop_flinging);
  FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DIRECT_3D);

  ASSERT_TRUE(impl_thread.message_loop_proxy().get());
  scoped_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  scoped_ptr<LayerTreeHost> layer_tree_host =
      LayerTreeHost::CreateThreaded(&client,
                                    shared_bitmap_manager.get(),
                                    settings,
                                    impl_thread.message_loop_proxy());

  impl_thread.message_loop_proxy()
      ->PostTask(FROM_HERE,
                 base::Bind(&BindInputHandlerOnCompositorThread,
                            layer_tree_host->GetInputHandler(),
                            base::Unretained(&input_handler_client)));

  layer_tree_host->DidStopFlinging();
  layer_tree_host.reset();
  impl_thread.Stop();
  EXPECT_TRUE(received_stop_flinging);
}

class LayerTreeHostScrollTestLayerStructureChange
    : public LayerTreeHostScrollTest {
 public:
  LayerTreeHostScrollTestLayerStructureChange()
      : scroll_destroy_whole_tree_(false) {}

  virtual void SetupTree() OVERRIDE {
    scoped_refptr<Layer> root_layer = Layer::Create();
    root_layer->SetBounds(gfx::Size(10, 10));

    Layer* root_scroll_layer =
        CreateScrollLayer(root_layer.get(), &root_scroll_layer_client_);
    CreateScrollLayer(root_layer.get(), &sibling_scroll_layer_client_);
    CreateScrollLayer(root_scroll_layer, &child_scroll_layer_client_);

    layer_tree_host()->SetRootLayer(root_layer);
    LayerTreeHostScrollTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* root = impl->active_tree()->root_layer();
    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        root->child_at(0)->SetScrollDelta(gfx::Vector2dF(5, 5));
        root->child_at(0)->child_at(0)->SetScrollDelta(gfx::Vector2dF(5, 5));
        root->child_at(1)->SetScrollDelta(gfx::Vector2dF(5, 5));
        PostSetNeedsCommitToMainThread();
        break;
      case 1:
        EndTest();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {}

  virtual void DidScroll(Layer* layer) {
    if (scroll_destroy_whole_tree_) {
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
    scoped_refptr<Layer> scroll_layer =
        ContentLayer::Create(&fake_content_layer_client_);
    scroll_layer->SetBounds(gfx::Size(110, 110));
    scroll_layer->SetPosition(gfx::Point(0, 0));
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

  FakeLayerScrollClient root_scroll_layer_client_;
  FakeLayerScrollClient sibling_scroll_layer_client_;
  FakeLayerScrollClient child_scroll_layer_client_;

  FakeContentLayerClient fake_content_layer_client_;

  bool scroll_destroy_whole_tree_;
};

TEST_F(LayerTreeHostScrollTestLayerStructureChange, ScrollDestroyLayer) {
    RunTest(true, false, false);
}

TEST_F(LayerTreeHostScrollTestLayerStructureChange, ScrollDestroyWholeTree) {
    scroll_destroy_whole_tree_ = true;
    RunTest(true, false, false);
}

}  // namespace
}  // namespace cc
