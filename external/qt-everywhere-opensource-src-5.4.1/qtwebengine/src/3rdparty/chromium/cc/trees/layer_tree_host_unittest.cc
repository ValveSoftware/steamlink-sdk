// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/synchronization/lock.h"
#include "cc/animation/timing_function.h"
#include "cc/base/swap_promise.h"
#include "cc/debug/frame_rate_counter.h"
#include "cc/layers/content_layer.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/io_surface_layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/video_layer.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/output/output_surface.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/io_surface_draw_quad.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/resource_update_queue.h"
#include "cc/test/fake_content_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_content_layer_impl.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_painted_scrollbar_layer.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/fake_scoped_ui_resource.h"
#include "cc/test/fake_video_frame_provider.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/thread_proxy.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "skia/ext/refptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/frame_time.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/vector2d_conversions.h"

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::Mock;

namespace cc {
namespace {

class LayerTreeHostTest : public LayerTreeTest {};

// Two setNeedsCommits in a row should lead to at least 1 commit and at least 1
// draw with frame 0.
class LayerTreeHostTestSetNeedsCommit1 : public LayerTreeHostTest {
 public:
  LayerTreeHostTestSetNeedsCommit1() : num_commits_(0), num_draws_(0) {}

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
    PostSetNeedsCommitToMainThread();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    num_draws_++;
    if (!impl->active_tree()->source_frame_number())
      EndTest();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    num_commits_++;
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_GE(1, num_commits_);
    EXPECT_GE(1, num_draws_);
  }

 private:
  int num_commits_;
  int num_draws_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestSetNeedsCommit1);

// A SetNeedsCommit should lead to 1 commit. Issuing a second commit after that
// first committed frame draws should lead to another commit.
class LayerTreeHostTestSetNeedsCommit2 : public LayerTreeHostTest {
 public:
  LayerTreeHostTestSetNeedsCommit2() : num_commits_(0), num_draws_(0) {}

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    ++num_draws_;
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    ++num_commits_;
    switch (num_commits_) {
      case 1:
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        EndTest();
        break;
      default:
        NOTREACHED();
    }
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(2, num_commits_);
    EXPECT_LE(1, num_draws_);
  }

 private:
  int num_commits_;
  int num_draws_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestSetNeedsCommit2);

// Verify that we pass property values in PushPropertiesTo.
class LayerTreeHostTestPushPropertiesTo : public LayerTreeHostTest {
 protected:
  virtual void SetupTree() OVERRIDE {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(10, 10));
    layer_tree_host()->SetRootLayer(root);
    LayerTreeHostTest::SetupTree();
  }

  enum Properties {
    STARTUP,
    BOUNDS,
    HIDE_LAYER_AND_SUBTREE,
    DRAWS_CONTENT,
    DONE,
  };

  virtual void BeginTest() OVERRIDE {
    index_ = STARTUP;
    PostSetNeedsCommitToMainThread();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    VerifyAfterValues(impl->active_tree()->root_layer());
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    SetBeforeValues(layer_tree_host()->root_layer());
    VerifyBeforeValues(layer_tree_host()->root_layer());

    ++index_;
    if (index_ == DONE) {
      EndTest();
      return;
    }

    SetAfterValues(layer_tree_host()->root_layer());
  }

  virtual void AfterTest() OVERRIDE {}

  void VerifyBeforeValues(Layer* layer) {
    EXPECT_EQ(gfx::Size(10, 10).ToString(), layer->bounds().ToString());
    EXPECT_FALSE(layer->hide_layer_and_subtree());
    EXPECT_FALSE(layer->DrawsContent());
  }

  void SetBeforeValues(Layer* layer) {
    layer->SetBounds(gfx::Size(10, 10));
    layer->SetHideLayerAndSubtree(false);
    layer->SetIsDrawable(false);
  }

  void VerifyAfterValues(LayerImpl* layer) {
    switch (static_cast<Properties>(index_)) {
      case STARTUP:
      case DONE:
        break;
      case BOUNDS:
        EXPECT_EQ(gfx::Size(20, 20).ToString(), layer->bounds().ToString());
        break;
      case HIDE_LAYER_AND_SUBTREE:
        EXPECT_TRUE(layer->hide_layer_and_subtree());
        break;
      case DRAWS_CONTENT:
        EXPECT_TRUE(layer->DrawsContent());
        break;
    }
  }

  void SetAfterValues(Layer* layer) {
    switch (static_cast<Properties>(index_)) {
      case STARTUP:
      case DONE:
        break;
      case BOUNDS:
        layer->SetBounds(gfx::Size(20, 20));
        break;
      case HIDE_LAYER_AND_SUBTREE:
        layer->SetHideLayerAndSubtree(true);
        break;
      case DRAWS_CONTENT:
        layer->SetIsDrawable(true);
        break;
    }
  }

  int index_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestPushPropertiesTo);

// 1 setNeedsRedraw after the first commit has completed should lead to 1
// additional draw.
class LayerTreeHostTestSetNeedsRedraw : public LayerTreeHostTest {
 public:
  LayerTreeHostTestSetNeedsRedraw() : num_commits_(0), num_draws_(0) {}

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    EXPECT_EQ(0, impl->active_tree()->source_frame_number());
    if (!num_draws_) {
      // Redraw again to verify that the second redraw doesn't commit.
      PostSetNeedsRedrawToMainThread();
    } else {
      EndTest();
    }
    num_draws_++;
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    EXPECT_EQ(0, num_draws_);
    num_commits_++;
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_GE(2, num_draws_);
    EXPECT_EQ(1, num_commits_);
  }

 private:
  int num_commits_;
  int num_draws_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestSetNeedsRedraw);

// After setNeedsRedrawRect(invalid_rect) the final damage_rect
// must contain invalid_rect.
class LayerTreeHostTestSetNeedsRedrawRect : public LayerTreeHostTest {
 public:
  LayerTreeHostTestSetNeedsRedrawRect()
      : num_draws_(0),
        bounds_(50, 50),
        invalid_rect_(10, 10, 20, 20),
        root_layer_(ContentLayer::Create(&client_)) {}

  virtual void BeginTest() OVERRIDE {
    root_layer_->SetIsDrawable(true);
    root_layer_->SetBounds(bounds_);
    layer_tree_host()->SetRootLayer(root_layer_);
    layer_tree_host()->SetViewportSize(bounds_);
    PostSetNeedsCommitToMainThread();
  }

  virtual DrawResult PrepareToDrawOnThread(
      LayerTreeHostImpl* host_impl,
      LayerTreeHostImpl::FrameData* frame_data,
      DrawResult draw_result) OVERRIDE {
    EXPECT_EQ(DRAW_SUCCESS, draw_result);

    gfx::RectF root_damage_rect;
    if (!frame_data->render_passes.empty())
      root_damage_rect = frame_data->render_passes.back()->damage_rect;

    if (!num_draws_) {
      // If this is the first frame, expect full frame damage.
      EXPECT_RECT_EQ(root_damage_rect, gfx::Rect(bounds_));
    } else {
      // Check that invalid_rect_ is indeed repainted.
      EXPECT_TRUE(root_damage_rect.Contains(invalid_rect_));
    }

    return draw_result;
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    if (!num_draws_) {
      PostSetNeedsRedrawRectToMainThread(invalid_rect_);
    } else {
      EndTest();
    }
    num_draws_++;
  }

  virtual void AfterTest() OVERRIDE { EXPECT_EQ(2, num_draws_); }

 private:
  int num_draws_;
  const gfx::Size bounds_;
  const gfx::Rect invalid_rect_;
  FakeContentLayerClient client_;
  scoped_refptr<ContentLayer> root_layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestSetNeedsRedrawRect);

class LayerTreeHostTestNoExtraCommitFromInvalidate : public LayerTreeHostTest {
 public:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->layer_transforms_should_scale_layer_contents = true;
  }

  virtual void SetupTree() OVERRIDE {
    root_layer_ = Layer::Create();
    root_layer_->SetBounds(gfx::Size(10, 20));

    scaled_layer_ = FakeContentLayer::Create(&client_);
    scaled_layer_->SetBounds(gfx::Size(1, 1));
    root_layer_->AddChild(scaled_layer_);

    layer_tree_host()->SetRootLayer(root_layer_);
    LayerTreeHostTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    if (host_impl->active_tree()->source_frame_number() == 1)
      EndTest();
  }

  virtual void DidCommit() OVERRIDE {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        // Changing the device scale factor causes a commit. It also changes
        // the content bounds of |scaled_layer_|, which should not generate
        // a second commit as a result.
        layer_tree_host()->SetDeviceScaleFactor(4.f);
        break;
      default:
        // No extra commits.
        EXPECT_EQ(2, layer_tree_host()->source_frame_number());
    }
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(gfx::Size(4, 4).ToString(),
              scaled_layer_->content_bounds().ToString());
  }

 private:
  FakeContentLayerClient client_;
  scoped_refptr<Layer> root_layer_;
  scoped_refptr<FakeContentLayer> scaled_layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestNoExtraCommitFromInvalidate);

class LayerTreeHostTestNoExtraCommitFromScrollbarInvalidate
    : public LayerTreeHostTest {
 public:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->layer_transforms_should_scale_layer_contents = true;
  }

  virtual void SetupTree() OVERRIDE {
    root_layer_ = Layer::Create();
    root_layer_->SetBounds(gfx::Size(10, 20));

    bool paint_scrollbar = true;
    bool has_thumb = false;
    scrollbar_ = FakePaintedScrollbarLayer::Create(
        paint_scrollbar, has_thumb, root_layer_->id());
    scrollbar_->SetPosition(gfx::Point(0, 10));
    scrollbar_->SetBounds(gfx::Size(10, 10));

    root_layer_->AddChild(scrollbar_);

    layer_tree_host()->SetRootLayer(root_layer_);
    LayerTreeHostTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    if (host_impl->active_tree()->source_frame_number() == 1)
      EndTest();
  }

  virtual void DidCommit() OVERRIDE {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        // Changing the device scale factor causes a commit. It also changes
        // the content bounds of |scrollbar_|, which should not generate
        // a second commit as a result.
        layer_tree_host()->SetDeviceScaleFactor(4.f);
        break;
      default:
        // No extra commits.
        EXPECT_EQ(2, layer_tree_host()->source_frame_number());
    }
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(gfx::Size(40, 40).ToString(),
              scrollbar_->content_bounds().ToString());
  }

 private:
  FakeContentLayerClient client_;
  scoped_refptr<Layer> root_layer_;
  scoped_refptr<FakePaintedScrollbarLayer> scrollbar_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostTestNoExtraCommitFromScrollbarInvalidate);

class LayerTreeHostTestSetNextCommitForcesRedraw : public LayerTreeHostTest {
 public:
  LayerTreeHostTestSetNextCommitForcesRedraw()
      : num_draws_(0),
        bounds_(50, 50),
        invalid_rect_(10, 10, 20, 20),
        root_layer_(ContentLayer::Create(&client_)) {}

  virtual void BeginTest() OVERRIDE {
    root_layer_->SetIsDrawable(true);
    root_layer_->SetBounds(bounds_);
    layer_tree_host()->SetRootLayer(root_layer_);
    layer_tree_host()->SetViewportSize(bounds_);
    PostSetNeedsCommitToMainThread();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    if (num_draws_ == 3 && host_impl->settings().impl_side_painting)
      host_impl->SetNeedsRedrawRect(invalid_rect_);
  }

  virtual DrawResult PrepareToDrawOnThread(
      LayerTreeHostImpl* host_impl,
      LayerTreeHostImpl::FrameData* frame_data,
      DrawResult draw_result) OVERRIDE {
    EXPECT_EQ(DRAW_SUCCESS, draw_result);

    gfx::RectF root_damage_rect;
    if (!frame_data->render_passes.empty())
      root_damage_rect = frame_data->render_passes.back()->damage_rect;

    switch (num_draws_) {
      case 0:
        EXPECT_RECT_EQ(gfx::Rect(bounds_), root_damage_rect);
        break;
      case 1:
      case 2:
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 0, 0), root_damage_rect);
        break;
      case 3:
        EXPECT_RECT_EQ(invalid_rect_, root_damage_rect);
        break;
      case 4:
        EXPECT_RECT_EQ(gfx::Rect(bounds_), root_damage_rect);
        break;
      default:
        NOTREACHED();
    }

    return draw_result;
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    switch (num_draws_) {
      case 0:
      case 1:
        // Cycle through a couple of empty commits to ensure we're observing the
        // right behavior
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        // Should force full frame damage on the next commit
        PostSetNextCommitForcesRedrawToMainThread();
        PostSetNeedsCommitToMainThread();
        if (host_impl->settings().impl_side_painting)
          host_impl->BlockNotifyReadyToActivateForTesting(true);
        else
          num_draws_++;
        break;
      case 3:
        host_impl->BlockNotifyReadyToActivateForTesting(false);
        break;
      default:
        EndTest();
        break;
    }
    num_draws_++;
  }

  virtual void AfterTest() OVERRIDE { EXPECT_EQ(5, num_draws_); }

 private:
  int num_draws_;
  const gfx::Size bounds_;
  const gfx::Rect invalid_rect_;
  FakeContentLayerClient client_;
  scoped_refptr<ContentLayer> root_layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestSetNextCommitForcesRedraw);

// Tests that if a layer is not drawn because of some reason in the parent then
// its damage is preserved until the next time it is drawn.
class LayerTreeHostTestUndrawnLayersDamageLater : public LayerTreeHostTest {
 public:
  LayerTreeHostTestUndrawnLayersDamageLater()
      : root_layer_(ContentLayer::Create(&client_)) {}

  virtual void SetupTree() OVERRIDE {
    root_layer_->SetIsDrawable(true);
    root_layer_->SetBounds(gfx::Size(50, 50));
    layer_tree_host()->SetRootLayer(root_layer_);

    // The initially transparent layer has a larger child layer, which is
    // not initially drawn because of the this (parent) layer.
    parent_layer_ = FakeContentLayer::Create(&client_);
    parent_layer_->SetBounds(gfx::Size(15, 15));
    parent_layer_->SetOpacity(0.0f);
    root_layer_->AddChild(parent_layer_);

    child_layer_ = FakeContentLayer::Create(&client_);
    child_layer_->SetBounds(gfx::Size(25, 25));
    parent_layer_->AddChild(child_layer_);

    LayerTreeHostTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual DrawResult PrepareToDrawOnThread(
      LayerTreeHostImpl* host_impl,
      LayerTreeHostImpl::FrameData* frame_data,
      DrawResult draw_result) OVERRIDE {
    EXPECT_EQ(DRAW_SUCCESS, draw_result);

    gfx::RectF root_damage_rect;
    if (!frame_data->render_passes.empty())
      root_damage_rect = frame_data->render_passes.back()->damage_rect;

    // The first time, the whole view needs be drawn.
    // Afterwards, just the opacity of surface_layer1 is changed a few times,
    // and each damage should be the bounding box of it and its child. If this
    // was working improperly, the damage might not include its childs bounding
    // box.
    switch (host_impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_RECT_EQ(gfx::Rect(root_layer_->bounds()), root_damage_rect);
        break;
      case 1:
      case 2:
      case 3:
        EXPECT_RECT_EQ(gfx::Rect(child_layer_->bounds()), root_damage_rect);
        break;
      default:
        NOTREACHED();
    }

    return draw_result;
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        // Test not owning the surface.
        parent_layer_->SetOpacity(1.0f);
        break;
      case 2:
        parent_layer_->SetOpacity(0.0f);
        break;
      case 3:
        // Test owning the surface.
        parent_layer_->SetOpacity(0.5f);
        parent_layer_->SetForceRenderSurface(true);
        break;
      case 4:
        EndTest();
        break;
      default:
        NOTREACHED();
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  FakeContentLayerClient client_;
  scoped_refptr<ContentLayer> root_layer_;
  scoped_refptr<FakeContentLayer> parent_layer_;
  scoped_refptr<FakeContentLayer> child_layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestUndrawnLayersDamageLater);

// Tests that if a layer is not drawn because of some reason in the parent,
// causing its content bounds to not be computed, then when it is later drawn,
// its content bounds get pushed.
class LayerTreeHostTestUndrawnLayersPushContentBoundsLater
    : public LayerTreeHostTest {
 public:
  LayerTreeHostTestUndrawnLayersPushContentBoundsLater()
      : root_layer_(Layer::Create()) {}

  virtual void SetupTree() OVERRIDE {
    root_layer_->SetIsDrawable(true);
    root_layer_->SetBounds(gfx::Size(20, 20));
    layer_tree_host()->SetRootLayer(root_layer_);

    parent_layer_ = Layer::Create();
    parent_layer_->SetBounds(gfx::Size(20, 20));
    parent_layer_->SetOpacity(0.0f);
    root_layer_->AddChild(parent_layer_);

    child_layer_ = Layer::Create();
    child_layer_->SetBounds(gfx::Size(15, 15));
    parent_layer_->AddChild(child_layer_);

    LayerTreeHostTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    LayerImpl* root = host_impl->active_tree()->root_layer();
    LayerImpl* parent = root->children()[0];
    LayerImpl* child = parent->children()[0];

    switch (host_impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_EQ(0.f, parent->opacity());
        EXPECT_EQ(gfx::SizeF(), child->content_bounds());
        break;
      case 1:
        EXPECT_EQ(1.f, parent->opacity());
        EXPECT_EQ(gfx::SizeF(15.f, 15.f), child->content_bounds());
        EndTest();
        break;
      default:
        NOTREACHED();
    }
  }

  virtual void DidCommit() OVERRIDE {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        parent_layer_->SetOpacity(1.0f);
        break;
      case 2:
        break;
      default:
        NOTREACHED();
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  scoped_refptr<Layer> root_layer_;
  scoped_refptr<Layer> parent_layer_;
  scoped_refptr<Layer> child_layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostTestUndrawnLayersPushContentBoundsLater);

class LayerTreeHostTestAbortFrameWhenInvisible : public LayerTreeHostTest {
 public:
  LayerTreeHostTestAbortFrameWhenInvisible() {}

  virtual void BeginTest() OVERRIDE {
    // Request a commit (from the main thread), Which will trigger the commit
    // flow from the impl side.
    layer_tree_host()->SetNeedsCommit();
    // Then mark ourselves as not visible before processing any more messages
    // on the main thread.
    layer_tree_host()->SetVisible(false);
    // If we make it without kicking a frame, we pass!
    EndTestAfterDelay(1);
  }

  virtual void Layout() OVERRIDE {
    ASSERT_FALSE(true);
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}
};

MULTI_THREAD_TEST_F(LayerTreeHostTestAbortFrameWhenInvisible);

// This test verifies that properties on the layer tree host are commited
// to the impl side.
class LayerTreeHostTestCommit : public LayerTreeHostTest {
 public:
  LayerTreeHostTestCommit() {}

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->SetViewportSize(gfx::Size(20, 20));
    layer_tree_host()->set_background_color(SK_ColorGRAY);

    PostSetNeedsCommitToMainThread();
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    EXPECT_EQ(gfx::Size(20, 20), impl->DrawViewportSize());
    EXPECT_EQ(SK_ColorGRAY, impl->active_tree()->background_color());

    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}
};

MULTI_THREAD_TEST_F(LayerTreeHostTestCommit);

// This test verifies that LayerTreeHostImpl's current frame time gets
// updated in consecutive frames when it doesn't draw due to tree
// activation failure.
class LayerTreeHostTestFrameTimeUpdatesAfterActivationFails
    : public LayerTreeHostTest {
 public:
  LayerTreeHostTestFrameTimeUpdatesAfterActivationFails()
      : frame_count_with_pending_tree_(0) {}

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->SetViewportSize(gfx::Size(20, 20));
    layer_tree_host()->set_background_color(SK_ColorGRAY);

    PostSetNeedsCommitToMainThread();
  }

  virtual void BeginCommitOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    EXPECT_EQ(frame_count_with_pending_tree_, 0);
    impl->BlockNotifyReadyToActivateForTesting(true);
  }

  virtual void WillBeginImplFrameOnThread(LayerTreeHostImpl* impl,
                                          const BeginFrameArgs& args) OVERRIDE {
    if (impl->pending_tree())
      frame_count_with_pending_tree_++;

    if (frame_count_with_pending_tree_ == 1) {
      EXPECT_EQ(first_frame_time_.ToInternalValue(), 0);
      first_frame_time_ = impl->CurrentFrameTimeTicks();
    } else if (frame_count_with_pending_tree_ == 2) {
      impl->BlockNotifyReadyToActivateForTesting(false);
    }
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    if (frame_count_with_pending_tree_ > 1) {
      EXPECT_NE(first_frame_time_.ToInternalValue(), 0);
      EXPECT_NE(first_frame_time_.ToInternalValue(),
                impl->CurrentFrameTimeTicks().ToInternalValue());
      EndTest();
      return;
    }

    EXPECT_FALSE(impl->settings().impl_side_painting);
    EndTest();
  }
  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    if (impl->settings().impl_side_painting)
      EXPECT_NE(frame_count_with_pending_tree_, 1);
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  int frame_count_with_pending_tree_;
  base::TimeTicks first_frame_time_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostTestFrameTimeUpdatesAfterActivationFails);

// This test verifies that LayerTreeHostImpl's current frame time gets
// updated in consecutive frames when it draws in each frame.
class LayerTreeHostTestFrameTimeUpdatesAfterDraw : public LayerTreeHostTest {
 public:
  LayerTreeHostTestFrameTimeUpdatesAfterDraw() : frame_(0) {}

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->SetViewportSize(gfx::Size(20, 20));
    layer_tree_host()->set_background_color(SK_ColorGRAY);

    PostSetNeedsCommitToMainThread();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    frame_++;
    if (frame_ == 1) {
      first_frame_time_ = impl->CurrentFrameTimeTicks();
      impl->SetNeedsRedraw();

      // Since we might use a low-resolution clock on Windows, we need to
      // make sure that the clock has incremented past first_frame_time_.
      while (first_frame_time_ == gfx::FrameTime::Now()) {
      }

      return;
    }

    EXPECT_NE(first_frame_time_, impl->CurrentFrameTimeTicks());
    EndTest();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    // Ensure there isn't a commit between the two draws, to ensure that a
    // commit isn't required for updating the current frame time. We can
    // only check for this in the multi-threaded case, since in the single-
    // threaded case there will always be a commit between consecutive draws.
    if (HasImplThread())
      EXPECT_EQ(0, frame_);
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  int frame_;
  base::TimeTicks first_frame_time_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestFrameTimeUpdatesAfterDraw);

// Verifies that StartPageScaleAnimation events propagate correctly
// from LayerTreeHost to LayerTreeHostImpl in the MT compositor.
class LayerTreeHostTestStartPageScaleAnimation : public LayerTreeHostTest {
 public:
  LayerTreeHostTestStartPageScaleAnimation() {}

  virtual void SetupTree() OVERRIDE {
    LayerTreeHostTest::SetupTree();

    if (layer_tree_host()->settings().impl_side_painting) {
      scoped_refptr<FakePictureLayer> layer =
          FakePictureLayer::Create(&client_);
      layer->set_always_update_resources(true);
      scroll_layer_ = layer;
    } else {
      scroll_layer_ = FakeContentLayer::Create(&client_);
    }

    Layer* root_layer = layer_tree_host()->root_layer();
    scroll_layer_->SetScrollClipLayerId(root_layer->id());
    scroll_layer_->SetIsContainerForFixedPositionLayers(true);
    scroll_layer_->SetBounds(gfx::Size(2 * root_layer->bounds().width(),
                                       2 * root_layer->bounds().height()));
    scroll_layer_->SetScrollOffset(gfx::Vector2d());
    layer_tree_host()->root_layer()->AddChild(scroll_layer_);
    // This test requires the page_scale and inner viewport layers to be
    // identified.
    layer_tree_host()->RegisterViewportLayers(
        root_layer, scroll_layer_.get(), NULL);
    layer_tree_host()->SetPageScaleFactorAndLimits(1.f, 0.5f, 2.f);
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void ApplyScrollAndScale(const gfx::Vector2d& scroll_delta,
                                   float scale) OVERRIDE {
    gfx::Vector2d offset = scroll_layer_->scroll_offset();
    scroll_layer_->SetScrollOffset(offset + scroll_delta);
    layer_tree_host()->SetPageScaleFactorAndLimits(scale, 0.5f, 2.f);
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    // We get one commit before the first draw, and the animation doesn't happen
    // until the second draw.
    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_EQ(1.f, impl->active_tree()->page_scale_factor());
        // We'll start an animation when we get back to the main thread.
        break;
      case 1:
        EXPECT_EQ(1.f, impl->active_tree()->page_scale_factor());
        break;
      case 2:
        EXPECT_EQ(1.25f, impl->active_tree()->page_scale_factor());
        EndTest();
        break;
      default:
        NOTREACHED();
    }
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        layer_tree_host()->StartPageScaleAnimation(
            gfx::Vector2d(), false, 1.25f, base::TimeDelta());
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {}

  FakeContentLayerClient client_;
  scoped_refptr<Layer> scroll_layer_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestStartPageScaleAnimation);

class LayerTreeHostTestSetVisible : public LayerTreeHostTest {
 public:
  LayerTreeHostTestSetVisible() : num_draws_(0) {}

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
    PostSetVisibleToMainThread(false);
    // This is suppressed while we're invisible.
    PostSetNeedsRedrawToMainThread();
    // Triggers the redraw.
    PostSetVisibleToMainThread(true);
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    EXPECT_TRUE(impl->visible());
    ++num_draws_;
    EndTest();
  }

  virtual void AfterTest() OVERRIDE { EXPECT_EQ(1, num_draws_); }

 private:
  int num_draws_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestSetVisible);

class TestOpacityChangeLayerDelegate : public ContentLayerClient {
 public:
  TestOpacityChangeLayerDelegate() : test_layer_(0) {}

  void SetTestLayer(Layer* test_layer) { test_layer_ = test_layer; }

  virtual void PaintContents(
      SkCanvas* canvas,
      const gfx::Rect& clip,
      gfx::RectF* opaque,
      ContentLayerClient::GraphicsContextStatus gc_status) OVERRIDE {
    // Set layer opacity to 0.
    if (test_layer_)
      test_layer_->SetOpacity(0.f);
  }
  virtual void DidChangeLayerCanUseLCDText() OVERRIDE {}
  virtual bool FillsBoundsCompletely() const OVERRIDE { return false; }

 private:
  Layer* test_layer_;
};

class ContentLayerWithUpdateTracking : public ContentLayer {
 public:
  static scoped_refptr<ContentLayerWithUpdateTracking> Create(
      ContentLayerClient* client) {
    return make_scoped_refptr(new ContentLayerWithUpdateTracking(client));
  }

  int PaintContentsCount() { return paint_contents_count_; }
  void ResetPaintContentsCount() { paint_contents_count_ = 0; }

  virtual bool Update(ResourceUpdateQueue* queue,
                      const OcclusionTracker<Layer>* occlusion) OVERRIDE {
    bool updated = ContentLayer::Update(queue, occlusion);
    paint_contents_count_++;
    return updated;
  }

 private:
  explicit ContentLayerWithUpdateTracking(ContentLayerClient* client)
      : ContentLayer(client), paint_contents_count_(0) {
    SetBounds(gfx::Size(10, 10));
    SetIsDrawable(true);
  }
  virtual ~ContentLayerWithUpdateTracking() {}

  int paint_contents_count_;
};

// Layer opacity change during paint should not prevent compositor resources
// from being updated during commit.
class LayerTreeHostTestOpacityChange : public LayerTreeHostTest {
 public:
  LayerTreeHostTestOpacityChange()
      : test_opacity_change_delegate_(),
        update_check_layer_(ContentLayerWithUpdateTracking::Create(
            &test_opacity_change_delegate_)) {
    test_opacity_change_delegate_.SetTestLayer(update_check_layer_.get());
  }

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->SetViewportSize(gfx::Size(10, 10));
    layer_tree_host()->root_layer()->AddChild(update_check_layer_);

    PostSetNeedsCommitToMainThread();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {
    // Update() should have been called once.
    EXPECT_EQ(1, update_check_layer_->PaintContentsCount());
  }

 private:
  TestOpacityChangeLayerDelegate test_opacity_change_delegate_;
  scoped_refptr<ContentLayerWithUpdateTracking> update_check_layer_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestOpacityChange);

class NoScaleContentLayer : public ContentLayer {
 public:
  static scoped_refptr<NoScaleContentLayer> Create(ContentLayerClient* client) {
    return make_scoped_refptr(new NoScaleContentLayer(client));
  }

  virtual void CalculateContentsScale(float ideal_contents_scale,
                                      float device_scale_factor,
                                      float page_scale_factor,
                                      float maximum_animation_contents_scale,
                                      bool animating_transform_to_screen,
                                      float* contents_scale_x,
                                      float* contents_scale_y,
                                      gfx::Size* contentBounds) OVERRIDE {
    // Skip over the ContentLayer's method to the base Layer class.
    Layer::CalculateContentsScale(ideal_contents_scale,
                                  device_scale_factor,
                                  page_scale_factor,
                                  maximum_animation_contents_scale,
                                  animating_transform_to_screen,
                                  contents_scale_x,
                                  contents_scale_y,
                                  contentBounds);
  }

 private:
  explicit NoScaleContentLayer(ContentLayerClient* client)
      : ContentLayer(client) {}
  virtual ~NoScaleContentLayer() {}
};

class LayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers
    : public LayerTreeHostTest {
 public:
  LayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers()
      : root_layer_(NoScaleContentLayer::Create(&client_)),
        child_layer_(ContentLayer::Create(&client_)) {}

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->SetViewportSize(gfx::Size(60, 60));
    layer_tree_host()->SetDeviceScaleFactor(1.5);
    EXPECT_EQ(gfx::Size(60, 60), layer_tree_host()->device_viewport_size());

    root_layer_->AddChild(child_layer_);

    root_layer_->SetIsDrawable(true);
    root_layer_->SetBounds(gfx::Size(30, 30));

    child_layer_->SetIsDrawable(true);
    child_layer_->SetPosition(gfx::Point(2, 2));
    child_layer_->SetBounds(gfx::Size(10, 10));

    layer_tree_host()->SetRootLayer(root_layer_);

    PostSetNeedsCommitToMainThread();
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    // Should only do one commit.
    EXPECT_EQ(0, impl->active_tree()->source_frame_number());
    // Device scale factor should come over to impl.
    EXPECT_NEAR(impl->device_scale_factor(), 1.5f, 0.00001f);

    // Both layers are on impl.
    ASSERT_EQ(1u, impl->active_tree()->root_layer()->children().size());

    // Device viewport is scaled.
    EXPECT_EQ(gfx::Size(60, 60), impl->DrawViewportSize());

    LayerImpl* root = impl->active_tree()->root_layer();
    LayerImpl* child = impl->active_tree()->root_layer()->children()[0];

    // Positions remain in layout pixels.
    EXPECT_EQ(gfx::Point(0, 0), root->position());
    EXPECT_EQ(gfx::Point(2, 2), child->position());

    // Compute all the layer transforms for the frame.
    LayerTreeHostImpl::FrameData frame_data;
    impl->PrepareToDraw(&frame_data);
    impl->DidDrawAllLayers(frame_data);

    const LayerImplList& render_surface_layer_list =
        *frame_data.render_surface_layer_list;

    // Both layers should be drawing into the root render surface.
    ASSERT_EQ(1u, render_surface_layer_list.size());
    ASSERT_EQ(root->render_surface(),
              render_surface_layer_list[0]->render_surface());
    ASSERT_EQ(2u, root->render_surface()->layer_list().size());

    // The root render surface is the size of the viewport.
    EXPECT_RECT_EQ(gfx::Rect(0, 0, 60, 60),
                   root->render_surface()->content_rect());

    // The content bounds of the child should be scaled.
    gfx::Size child_bounds_scaled =
        gfx::ToCeiledSize(gfx::ScaleSize(child->bounds(), 1.5));
    EXPECT_EQ(child_bounds_scaled, child->content_bounds());

    gfx::Transform scale_transform;
    scale_transform.Scale(impl->device_scale_factor(),
                          impl->device_scale_factor());

    // The root layer is scaled by 2x.
    gfx::Transform root_screen_space_transform = scale_transform;
    gfx::Transform root_draw_transform = scale_transform;

    EXPECT_EQ(root_draw_transform, root->draw_transform());
    EXPECT_EQ(root_screen_space_transform, root->screen_space_transform());

    // The child is at position 2,2, which is transformed to 3,3 after the scale
    gfx::Transform child_screen_space_transform;
    child_screen_space_transform.Translate(3.f, 3.f);
    gfx::Transform child_draw_transform = child_screen_space_transform;

    EXPECT_TRANSFORMATION_MATRIX_EQ(child_draw_transform,
                                    child->draw_transform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(child_screen_space_transform,
                                    child->screen_space_transform());

    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  FakeContentLayerClient client_;
  scoped_refptr<NoScaleContentLayer> root_layer_;
  scoped_refptr<ContentLayer> child_layer_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers);

// Verify atomicity of commits and reuse of textures.
class LayerTreeHostTestDirectRendererAtomicCommit : public LayerTreeHostTest {
 public:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->texture_id_allocation_chunk_size = 1;
    // Make sure partial texture updates are turned off.
    settings->max_partial_texture_updates = 0;
    // Linear fade animator prevents scrollbars from drawing immediately.
    settings->scrollbar_animator = LayerTreeSettings::NoAnimator;
  }

  virtual void SetupTree() OVERRIDE {
    layer_ = FakeContentLayer::Create(&client_);
    layer_->SetBounds(gfx::Size(10, 20));

    bool paint_scrollbar = true;
    bool has_thumb = false;
    scrollbar_ = FakePaintedScrollbarLayer::Create(
        paint_scrollbar, has_thumb, layer_->id());
    scrollbar_->SetPosition(gfx::Point(0, 10));
    scrollbar_->SetBounds(gfx::Size(10, 10));

    layer_->AddChild(scrollbar_);

    layer_tree_host()->SetRootLayer(layer_);
    LayerTreeHostTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    drew_frame_ = -1;
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    ASSERT_EQ(0u, layer_tree_host()->settings().max_partial_texture_updates);

    TestWebGraphicsContext3D* context = TestContext();

    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        // Number of textures should be one for each layer
        ASSERT_EQ(2u, context->NumTextures());
        // Number of textures used for commit should be one for each layer.
        EXPECT_EQ(2u, context->NumUsedTextures());
        // Verify that used texture is correct.
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(0)));
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(1)));

        context->ResetUsedTextures();
        break;
      case 1:
        // Number of textures should be one for scrollbar layer since it was
        // requested and deleted on the impl-thread, and double for the content
        // layer since its first texture is used by impl thread and cannot by
        // used for update.
        ASSERT_EQ(3u, context->NumTextures());
        // Number of textures used for commit should be one for each layer.
        EXPECT_EQ(2u, context->NumUsedTextures());
        // First textures should not have been used.
        EXPECT_FALSE(context->UsedTexture(context->TextureAt(0)));
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(1)));
        // New textures should have been used.
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(2)));
        context->ResetUsedTextures();
        break;
      case 2:
        EndTest();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    TestWebGraphicsContext3D* context = TestContext();

    if (drew_frame_ == impl->active_tree()->source_frame_number()) {
      EXPECT_EQ(0u, context->NumUsedTextures()) << "For frame " << drew_frame_;
      return;
    }
    drew_frame_ = impl->active_tree()->source_frame_number();

    // We draw/ship one texture each frame for each layer.
    EXPECT_EQ(2u, context->NumUsedTextures());
    context->ResetUsedTextures();

    if (!TestEnded())
      PostSetNeedsCommitToMainThread();
  }

  virtual void Layout() OVERRIDE {
    layer_->SetNeedsDisplay();
    scrollbar_->SetNeedsDisplay();
  }

  virtual void AfterTest() OVERRIDE {}

 protected:
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> layer_;
  scoped_refptr<FakePaintedScrollbarLayer> scrollbar_;
  int drew_frame_;
};

MULTI_THREAD_DIRECT_RENDERER_NOIMPL_TEST_F(
    LayerTreeHostTestDirectRendererAtomicCommit);

class LayerTreeHostTestDelegatingRendererAtomicCommit
    : public LayerTreeHostTestDirectRendererAtomicCommit {
 public:
  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    ASSERT_EQ(0u, layer_tree_host()->settings().max_partial_texture_updates);

    TestWebGraphicsContext3D* context = TestContext();

    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        // Number of textures should be one for each layer
        ASSERT_EQ(2u, context->NumTextures());
        // Number of textures used for commit should be one for each layer.
        EXPECT_EQ(2u, context->NumUsedTextures());
        // Verify that used texture is correct.
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(0)));
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(1)));
        context->ResetUsedTextures();
        break;
      case 1:
        // Number of textures should be doubled as the first context layer
        // texture is being used by the impl-thread and cannot be used for
        // update.  The scrollbar behavior is different direct renderer because
        // UI resource deletion with delegating renderer occurs after tree
        // activation.
        ASSERT_EQ(4u, context->NumTextures());
        // Number of textures used for commit should still be
        // one for each layer.
        EXPECT_EQ(2u, context->NumUsedTextures());
        // First textures should not have been used.
        EXPECT_FALSE(context->UsedTexture(context->TextureAt(0)));
        EXPECT_FALSE(context->UsedTexture(context->TextureAt(1)));
        // New textures should have been used.
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(2)));
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(3)));
        context->ResetUsedTextures();
        break;
      case 2:
        EndTest();
        break;
      default:
        NOTREACHED();
        break;
    }
  }
};

MULTI_THREAD_DELEGATING_RENDERER_NOIMPL_TEST_F(
    LayerTreeHostTestDelegatingRendererAtomicCommit);

static void SetLayerPropertiesForTesting(Layer* layer,
                                         Layer* parent,
                                         const gfx::Transform& transform,
                                         const gfx::Point3F& transform_origin,
                                         const gfx::PointF& position,
                                         const gfx::Size& bounds,
                                         bool opaque) {
  layer->RemoveAllChildren();
  if (parent)
    parent->AddChild(layer);
  layer->SetTransform(transform);
  layer->SetTransformOrigin(transform_origin);
  layer->SetPosition(position);
  layer->SetBounds(bounds);
  layer->SetContentsOpaque(opaque);
}

class LayerTreeHostTestAtomicCommitWithPartialUpdate
    : public LayerTreeHostTest {
 public:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->texture_id_allocation_chunk_size = 1;
    // Allow one partial texture update.
    settings->max_partial_texture_updates = 1;
    // No partial updates when impl side painting is enabled.
    settings->impl_side_painting = false;
  }

  virtual void SetupTree() OVERRIDE {
    parent_ = FakeContentLayer::Create(&client_);
    parent_->SetBounds(gfx::Size(10, 20));

    child_ = FakeContentLayer::Create(&client_);
    child_->SetPosition(gfx::Point(0, 10));
    child_->SetBounds(gfx::Size(3, 10));

    parent_->AddChild(child_);

    layer_tree_host()->SetRootLayer(parent_);
    LayerTreeHostTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        parent_->SetNeedsDisplay();
        child_->SetNeedsDisplay();
        break;
      case 2:
        // Damage part of layers.
        parent_->SetNeedsDisplayRect(gfx::RectF(0.f, 0.f, 5.f, 5.f));
        child_->SetNeedsDisplayRect(gfx::RectF(0.f, 0.f, 5.f, 5.f));
        break;
      case 3:
        child_->SetNeedsDisplay();
        layer_tree_host()->SetViewportSize(gfx::Size(10, 10));
        break;
      case 4:
        layer_tree_host()->SetViewportSize(gfx::Size(10, 20));
        break;
      case 5:
        EndTest();
        break;
      default:
        NOTREACHED() << layer_tree_host()->source_frame_number();
        break;
    }
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    ASSERT_EQ(1u, layer_tree_host()->settings().max_partial_texture_updates);

    TestWebGraphicsContext3D* context = TestContext();

    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        // Number of textures should be one for each layer.
        ASSERT_EQ(2u, context->NumTextures());
        // Number of textures used for commit should be one for each layer.
        EXPECT_EQ(2u, context->NumUsedTextures());
        // Verify that used textures are correct.
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(0)));
        EXPECT_TRUE(context->UsedTexture(context->TextureAt(1)));
        context->ResetUsedTextures();
        break;
      case 1:
        if (HasImplThread()) {
          // Number of textures should be two for each content layer.
          ASSERT_EQ(4u, context->NumTextures());
        } else {
          // In single thread we can always do partial updates, so the limit has
          // no effect.
          ASSERT_EQ(2u, context->NumTextures());
        }
        // Number of textures used for commit should be one for each content
        // layer.
        EXPECT_EQ(2u, context->NumUsedTextures());

        if (HasImplThread()) {
          // First content textures should not have been used.
          EXPECT_FALSE(context->UsedTexture(context->TextureAt(0)));
          EXPECT_FALSE(context->UsedTexture(context->TextureAt(1)));
          // New textures should have been used.
          EXPECT_TRUE(context->UsedTexture(context->TextureAt(2)));
          EXPECT_TRUE(context->UsedTexture(context->TextureAt(3)));
        } else {
          // In single thread we can always do partial updates, so the limit has
          // no effect.
          EXPECT_TRUE(context->UsedTexture(context->TextureAt(0)));
          EXPECT_TRUE(context->UsedTexture(context->TextureAt(1)));
        }

        context->ResetUsedTextures();
        break;
      case 2:
        if (HasImplThread()) {
          // Number of textures should be two for each content layer.
          ASSERT_EQ(4u, context->NumTextures());
        } else {
          // In single thread we can always do partial updates, so the limit has
          // no effect.
          ASSERT_EQ(2u, context->NumTextures());
        }
        // Number of textures used for commit should be one for each content
        // layer.
        EXPECT_EQ(2u, context->NumUsedTextures());

        if (HasImplThread()) {
          // One content layer does a partial update also.
          EXPECT_TRUE(context->UsedTexture(context->TextureAt(2)));
          EXPECT_FALSE(context->UsedTexture(context->TextureAt(3)));
        } else {
          // In single thread we can always do partial updates, so the limit has
          // no effect.
          EXPECT_TRUE(context->UsedTexture(context->TextureAt(0)));
          EXPECT_TRUE(context->UsedTexture(context->TextureAt(1)));
        }

        context->ResetUsedTextures();
        break;
      case 3:
        // No textures should be used for commit.
        EXPECT_EQ(0u, context->NumUsedTextures());

        context->ResetUsedTextures();
        break;
      case 4:
        // Number of textures used for commit should be one, for the
        // content layer.
        EXPECT_EQ(1u, context->NumUsedTextures());

        context->ResetUsedTextures();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    EXPECT_LT(impl->active_tree()->source_frame_number(), 5);

    TestWebGraphicsContext3D* context = TestContext();

    // Number of textures used for drawing should one per layer except for
    // frame 3 where the viewport only contains one layer.
    if (impl->active_tree()->source_frame_number() == 3) {
      EXPECT_EQ(1u, context->NumUsedTextures());
    } else {
      EXPECT_EQ(2u, context->NumUsedTextures())
          << "For frame " << impl->active_tree()->source_frame_number();
    }

    context->ResetUsedTextures();
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> parent_;
  scoped_refptr<FakeContentLayer> child_;
};

// Partial updates are not possible with a delegating renderer.
SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    LayerTreeHostTestAtomicCommitWithPartialUpdate);

class LayerTreeHostTestSurfaceNotAllocatedForLayersOutsideMemoryLimit
    : public LayerTreeHostTest {
 protected:
  virtual void SetupTree() OVERRIDE {
    root_layer_ = FakeContentLayer::Create(&client_);
    root_layer_->SetBounds(gfx::Size(100, 100));

    surface_layer1_ = FakeContentLayer::Create(&client_);
    surface_layer1_->SetBounds(gfx::Size(100, 100));
    surface_layer1_->SetForceRenderSurface(true);
    surface_layer1_->SetOpacity(0.5f);
    root_layer_->AddChild(surface_layer1_);

    surface_layer2_ = FakeContentLayer::Create(&client_);
    surface_layer2_->SetBounds(gfx::Size(100, 100));
    surface_layer2_->SetForceRenderSurface(true);
    surface_layer2_->SetOpacity(0.5f);
    surface_layer1_->AddChild(surface_layer2_);

    replica_layer1_ = FakeContentLayer::Create(&client_);
    surface_layer1_->SetReplicaLayer(replica_layer1_.get());

    replica_layer2_ = FakeContentLayer::Create(&client_);
    surface_layer2_->SetReplicaLayer(replica_layer2_.get());

    layer_tree_host()->SetRootLayer(root_layer_);
    LayerTreeHostTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    Renderer* renderer = host_impl->renderer();
    RenderPass::Id surface1_render_pass_id = host_impl->active_tree()
                                                 ->root_layer()
                                                 ->children()[0]
                                                 ->render_surface()
                                                 ->RenderPassId();
    RenderPass::Id surface2_render_pass_id = host_impl->active_tree()
                                                 ->root_layer()
                                                 ->children()[0]
                                                 ->children()[0]
                                                 ->render_surface()
                                                 ->RenderPassId();

    switch (host_impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_TRUE(
            renderer->HasAllocatedResourcesForTesting(surface1_render_pass_id));
        EXPECT_TRUE(
            renderer->HasAllocatedResourcesForTesting(surface2_render_pass_id));

        // Reduce the memory limit to only fit the root layer and one render
        // surface. This prevents any contents drawing into surfaces
        // from being allocated.
        host_impl->SetMemoryPolicy(ManagedMemoryPolicy(100 * 100 * 4 * 2));
        break;
      case 1:
        EXPECT_FALSE(
            renderer->HasAllocatedResourcesForTesting(surface1_render_pass_id));
        EXPECT_FALSE(
            renderer->HasAllocatedResourcesForTesting(surface2_render_pass_id));

        EndTest();
        break;
    }
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    if (layer_tree_host()->source_frame_number() < 2)
      root_layer_->SetNeedsDisplay();
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_LE(2u, root_layer_->update_count());
    EXPECT_LE(2u, surface_layer1_->update_count());
    EXPECT_LE(2u, surface_layer2_->update_count());
  }

  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> root_layer_;
  scoped_refptr<FakeContentLayer> surface_layer1_;
  scoped_refptr<FakeContentLayer> replica_layer1_;
  scoped_refptr<FakeContentLayer> surface_layer2_;
  scoped_refptr<FakeContentLayer> replica_layer2_;
};

// Surfaces don't exist with a delegated renderer.
SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_NOIMPL_TEST_F(
    LayerTreeHostTestSurfaceNotAllocatedForLayersOutsideMemoryLimit);

class EvictionTestLayer : public Layer {
 public:
  static scoped_refptr<EvictionTestLayer> Create() {
    return make_scoped_refptr(new EvictionTestLayer());
  }

  virtual bool Update(ResourceUpdateQueue*,
                      const OcclusionTracker<Layer>*) OVERRIDE;
  virtual bool DrawsContent() const OVERRIDE { return true; }

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* impl) OVERRIDE;
  virtual void SetTexturePriorities(const PriorityCalculator&) OVERRIDE;

  bool HaveBackingTexture() const {
    return texture_.get() ? texture_->have_backing_texture() : false;
  }

 private:
  EvictionTestLayer() : Layer() {}
  virtual ~EvictionTestLayer() {}

  void CreateTextureIfNeeded() {
    if (texture_)
      return;
    texture_ = PrioritizedResource::Create(
        layer_tree_host()->contents_texture_manager());
    texture_->SetDimensions(gfx::Size(10, 10), RGBA_8888);
    bitmap_.allocN32Pixels(10, 10);
  }

  scoped_ptr<PrioritizedResource> texture_;
  SkBitmap bitmap_;
};

class EvictionTestLayerImpl : public LayerImpl {
 public:
  static scoped_ptr<EvictionTestLayerImpl> Create(LayerTreeImpl* tree_impl,
                                                  int id) {
    return make_scoped_ptr(new EvictionTestLayerImpl(tree_impl, id));
  }
  virtual ~EvictionTestLayerImpl() {}

  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE {
    ASSERT_TRUE(has_texture_);
    ASSERT_NE(0u, layer_tree_impl()->resource_provider()->num_resources());
  }

  void SetHasTexture(bool has_texture) { has_texture_ = has_texture; }

 private:
  EvictionTestLayerImpl(LayerTreeImpl* tree_impl, int id)
      : LayerImpl(tree_impl, id), has_texture_(false) {}

  bool has_texture_;
};

void EvictionTestLayer::SetTexturePriorities(const PriorityCalculator&) {
  CreateTextureIfNeeded();
  if (!texture_)
    return;
  texture_->set_request_priority(PriorityCalculator::UIPriority(true));
}

bool EvictionTestLayer::Update(ResourceUpdateQueue* queue,
                               const OcclusionTracker<Layer>* occlusion) {
  CreateTextureIfNeeded();
  if (!texture_)
    return false;

  gfx::Rect full_rect(0, 0, 10, 10);
  ResourceUpdate upload = ResourceUpdate::Create(
      texture_.get(), &bitmap_, full_rect, full_rect, gfx::Vector2d());
  queue->AppendFullUpload(upload);
  return true;
}

scoped_ptr<LayerImpl> EvictionTestLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return EvictionTestLayerImpl::Create(tree_impl, layer_id_)
      .PassAs<LayerImpl>();
}

void EvictionTestLayer::PushPropertiesTo(LayerImpl* layer_impl) {
  Layer::PushPropertiesTo(layer_impl);

  EvictionTestLayerImpl* test_layer_impl =
      static_cast<EvictionTestLayerImpl*>(layer_impl);
  test_layer_impl->SetHasTexture(texture_->have_backing_texture());
}

class LayerTreeHostTestEvictTextures : public LayerTreeHostTest {
 public:
  LayerTreeHostTestEvictTextures()
      : layer_(EvictionTestLayer::Create()),
        impl_for_evict_textures_(0),
        num_commits_(0) {}

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->SetRootLayer(layer_);
    layer_tree_host()->SetViewportSize(gfx::Size(10, 20));

    gfx::Transform identity_matrix;
    SetLayerPropertiesForTesting(layer_.get(),
                                 0,
                                 identity_matrix,
                                 gfx::Point3F(0.f, 0.f, 0.f),
                                 gfx::PointF(0.f, 0.f),
                                 gfx::Size(10, 20),
                                 true);

    PostSetNeedsCommitToMainThread();
  }

  void PostEvictTextures() {
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&LayerTreeHostTestEvictTextures::EvictTexturesOnImplThread,
                   base::Unretained(this)));
  }

  void EvictTexturesOnImplThread() {
    DCHECK(impl_for_evict_textures_);
    impl_for_evict_textures_->EvictTexturesForTesting();
  }

  // Commit 1: Just commit and draw normally, then post an eviction at the end
  // that will trigger a commit.
  // Commit 2: Triggered by the eviction, let it go through and then set
  // needsCommit.
  // Commit 3: Triggered by the setNeedsCommit. In Layout(), post an eviction
  // task, which will be handled before the commit. Don't set needsCommit, it
  // should have been posted. A frame should not be drawn (note,
  // didCommitAndDrawFrame may be called anyway).
  // Commit 4: Triggered by the eviction, let it go through and then set
  // needsCommit.
  // Commit 5: Triggered by the setNeedsCommit, post an eviction task in
  // Layout(), a frame should not be drawn but a commit will be posted.
  // Commit 6: Triggered by the eviction, post an eviction task in
  // Layout(), which will be a noop, letting the commit (which recreates the
  // textures) go through and draw a frame, then end the test.
  //
  // Commits 1+2 test the eviction recovery path where eviction happens outside
  // of the beginFrame/commit pair.
  // Commits 3+4 test the eviction recovery path where eviction happens inside
  // the beginFrame/commit pair.
  // Commits 5+6 test the path where an eviction happens during the eviction
  // recovery path.
  virtual void DidCommit() OVERRIDE {
    switch (num_commits_) {
      case 1:
        EXPECT_TRUE(layer_->HaveBackingTexture());
        PostEvictTextures();
        break;
      case 2:
        EXPECT_TRUE(layer_->HaveBackingTexture());
        layer_tree_host()->SetNeedsCommit();
        break;
      case 3:
        break;
      case 4:
        EXPECT_TRUE(layer_->HaveBackingTexture());
        layer_tree_host()->SetNeedsCommit();
        break;
      case 5:
        break;
      case 6:
        EXPECT_TRUE(layer_->HaveBackingTexture());
        EndTest();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    impl_for_evict_textures_ = impl;
  }

  virtual void Layout() OVERRIDE {
    ++num_commits_;
    switch (num_commits_) {
      case 1:
      case 2:
        break;
      case 3:
        PostEvictTextures();
        break;
      case 4:
        // We couldn't check in didCommitAndDrawFrame on commit 3,
        // so check here.
        EXPECT_FALSE(layer_->HaveBackingTexture());
        break;
      case 5:
        PostEvictTextures();
        break;
      case 6:
        // We couldn't check in didCommitAndDrawFrame on commit 5,
        // so check here.
        EXPECT_FALSE(layer_->HaveBackingTexture());
        PostEvictTextures();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  FakeContentLayerClient client_;
  scoped_refptr<EvictionTestLayer> layer_;
  LayerTreeHostImpl* impl_for_evict_textures_;
  int num_commits_;
};

MULTI_THREAD_NOIMPL_TEST_F(LayerTreeHostTestEvictTextures);

class LayerTreeHostTestContinuousCommit : public LayerTreeHostTest {
 public:
  LayerTreeHostTestContinuousCommit()
      : num_commit_complete_(0), num_draw_layers_(0) {}

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->SetViewportSize(gfx::Size(10, 10));
    layer_tree_host()->root_layer()->SetBounds(gfx::Size(10, 10));

    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommit() OVERRIDE {
    if (num_draw_layers_ == 2)
      return;
    layer_tree_host()->SetNeedsCommit();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    if (num_draw_layers_ == 1)
      num_commit_complete_++;
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    num_draw_layers_++;
    if (num_draw_layers_ == 2)
      EndTest();
  }

  virtual void AfterTest() OVERRIDE {
    // Check that we didn't commit twice between first and second draw.
    EXPECT_EQ(1, num_commit_complete_);
  }

 private:
  int num_commit_complete_;
  int num_draw_layers_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestContinuousCommit);

class LayerTreeHostTestContinuousInvalidate : public LayerTreeHostTest {
 public:
  LayerTreeHostTestContinuousInvalidate()
      : num_commit_complete_(0), num_draw_layers_(0) {}

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->SetViewportSize(gfx::Size(10, 10));
    layer_tree_host()->root_layer()->SetBounds(gfx::Size(10, 10));

    content_layer_ = ContentLayer::Create(&client_);
    content_layer_->SetBounds(gfx::Size(10, 10));
    content_layer_->SetPosition(gfx::PointF(0.f, 0.f));
    content_layer_->SetIsDrawable(true);
    layer_tree_host()->root_layer()->AddChild(content_layer_);

    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    if (num_draw_layers_ == 2)
      return;
    content_layer_->SetNeedsDisplay();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    if (num_draw_layers_ == 1)
      num_commit_complete_++;
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    num_draw_layers_++;
    if (num_draw_layers_ == 2)
      EndTest();
  }

  virtual void AfterTest() OVERRIDE {
    // Check that we didn't commit twice between first and second draw.
    EXPECT_EQ(1, num_commit_complete_);
  }

 private:
  FakeContentLayerClient client_;
  scoped_refptr<Layer> content_layer_;
  int num_commit_complete_;
  int num_draw_layers_;
};

MULTI_THREAD_NOIMPL_TEST_F(LayerTreeHostTestContinuousInvalidate);

class LayerTreeHostTestDeferCommits : public LayerTreeHostTest {
 public:
  LayerTreeHostTestDeferCommits()
      : num_commits_deferred_(0), num_complete_commits_(0) {}

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DidDeferCommit() OVERRIDE {
    num_commits_deferred_++;
    layer_tree_host()->SetDeferCommits(false);
  }

  virtual void DidCommit() OVERRIDE {
    num_complete_commits_++;
    switch (num_complete_commits_) {
      case 1:
        EXPECT_EQ(0, num_commits_deferred_);
        layer_tree_host()->SetDeferCommits(true);
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        EndTest();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(1, num_commits_deferred_);
    EXPECT_EQ(2, num_complete_commits_);
  }

 private:
  int num_commits_deferred_;
  int num_complete_commits_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestDeferCommits);

class LayerTreeHostWithProxy : public LayerTreeHost {
 public:
  LayerTreeHostWithProxy(FakeLayerTreeHostClient* client,
                         const LayerTreeSettings& settings,
                         scoped_ptr<FakeProxy> proxy)
      : LayerTreeHost(client, NULL, settings) {
    proxy->SetLayerTreeHost(this);
    InitializeForTesting(proxy.PassAs<Proxy>());
  }
};

TEST(LayerTreeHostTest, LimitPartialUpdates) {
  // When partial updates are not allowed, max updates should be 0.
  {
    FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DIRECT_3D);

    scoped_ptr<FakeProxy> proxy(new FakeProxy);
    proxy->GetRendererCapabilities().allow_partial_texture_updates = false;
    proxy->SetMaxPartialTextureUpdates(5);

    LayerTreeSettings settings;
    settings.max_partial_texture_updates = 10;

    LayerTreeHostWithProxy host(&client, settings, proxy.Pass());
    host.OnCreateAndInitializeOutputSurfaceAttempted(true);

    EXPECT_EQ(0u, host.MaxPartialTextureUpdates());
  }

  // When partial updates are allowed,
  // max updates should be limited by the proxy.
  {
    FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DIRECT_3D);

    scoped_ptr<FakeProxy> proxy(new FakeProxy);
    proxy->GetRendererCapabilities().allow_partial_texture_updates = true;
    proxy->SetMaxPartialTextureUpdates(5);

    LayerTreeSettings settings;
    settings.max_partial_texture_updates = 10;

    LayerTreeHostWithProxy host(&client, settings, proxy.Pass());
    host.OnCreateAndInitializeOutputSurfaceAttempted(true);

    EXPECT_EQ(5u, host.MaxPartialTextureUpdates());
  }

  // When partial updates are allowed,
  // max updates should also be limited by the settings.
  {
    FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DIRECT_3D);

    scoped_ptr<FakeProxy> proxy(new FakeProxy);
    proxy->GetRendererCapabilities().allow_partial_texture_updates = true;
    proxy->SetMaxPartialTextureUpdates(20);

    LayerTreeSettings settings;
    settings.max_partial_texture_updates = 10;

    LayerTreeHostWithProxy host(&client, settings, proxy.Pass());
    host.OnCreateAndInitializeOutputSurfaceAttempted(true);

    EXPECT_EQ(10u, host.MaxPartialTextureUpdates());
  }
}

TEST(LayerTreeHostTest, PartialUpdatesWithGLRenderer) {
  FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DIRECT_3D);

  LayerTreeSettings settings;
  settings.max_partial_texture_updates = 4;

  scoped_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  scoped_ptr<LayerTreeHost> host = LayerTreeHost::CreateSingleThreaded(
      &client, &client, shared_bitmap_manager.get(), settings);
  host->Composite(base::TimeTicks::Now());

  EXPECT_EQ(4u, host->settings().max_partial_texture_updates);
}

TEST(LayerTreeHostTest, PartialUpdatesWithSoftwareRenderer) {
  FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DIRECT_SOFTWARE);

  LayerTreeSettings settings;
  settings.max_partial_texture_updates = 4;

  scoped_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  scoped_ptr<LayerTreeHost> host = LayerTreeHost::CreateSingleThreaded(
      &client, &client, shared_bitmap_manager.get(), settings);
  host->Composite(base::TimeTicks::Now());

  EXPECT_EQ(4u, host->settings().max_partial_texture_updates);
}

TEST(LayerTreeHostTest, PartialUpdatesWithDelegatingRendererAndGLContent) {
  FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DELEGATED_3D);

  LayerTreeSettings settings;
  settings.max_partial_texture_updates = 4;

  scoped_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  scoped_ptr<LayerTreeHost> host = LayerTreeHost::CreateSingleThreaded(
      &client, &client, shared_bitmap_manager.get(), settings);
  host->Composite(base::TimeTicks::Now());

  EXPECT_EQ(0u, host->MaxPartialTextureUpdates());
}

TEST(LayerTreeHostTest,
     PartialUpdatesWithDelegatingRendererAndSoftwareContent) {
  FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DELEGATED_SOFTWARE);

  LayerTreeSettings settings;
  settings.max_partial_texture_updates = 4;

  scoped_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  scoped_ptr<LayerTreeHost> host = LayerTreeHost::CreateSingleThreaded(
      &client, &client, shared_bitmap_manager.get(), settings);
  host->Composite(base::TimeTicks::Now());

  EXPECT_EQ(0u, host->MaxPartialTextureUpdates());
}

class LayerTreeHostTestShutdownWithOnlySomeResourcesEvicted
    : public LayerTreeHostTest {
 public:
  LayerTreeHostTestShutdownWithOnlySomeResourcesEvicted()
      : root_layer_(FakeContentLayer::Create(&client_)),
        child_layer1_(FakeContentLayer::Create(&client_)),
        child_layer2_(FakeContentLayer::Create(&client_)),
        num_commits_(0) {}

  virtual void BeginTest() OVERRIDE {
    layer_tree_host()->SetViewportSize(gfx::Size(100, 100));
    root_layer_->SetBounds(gfx::Size(100, 100));
    child_layer1_->SetBounds(gfx::Size(100, 100));
    child_layer2_->SetBounds(gfx::Size(100, 100));
    root_layer_->AddChild(child_layer1_);
    root_layer_->AddChild(child_layer2_);
    layer_tree_host()->SetRootLayer(root_layer_);
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidSetVisibleOnImplTree(LayerTreeHostImpl* host_impl,
                                       bool visible) OVERRIDE {
    if (visible) {
      // One backing should remain unevicted.
      EXPECT_EQ(
          100u * 100u * 4u * 1u,
          layer_tree_host()->contents_texture_manager()->MemoryUseBytes());
    } else {
      EXPECT_EQ(
          0u, layer_tree_host()->contents_texture_manager()->MemoryUseBytes());
    }

    // Make sure that contents textures are marked as having been
    // purged.
    EXPECT_TRUE(host_impl->active_tree()->ContentsTexturesPurged());
    // End the test in this state.
    EndTest();
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    ++num_commits_;
    switch (num_commits_) {
      case 1:
        // All three backings should have memory.
        EXPECT_EQ(
            100u * 100u * 4u * 3u,
            layer_tree_host()->contents_texture_manager()->MemoryUseBytes());
        // Set a new policy that will kick out 1 of the 3 resources.
        // Because a resource was evicted, a commit will be kicked off.
        host_impl->SetMemoryPolicy(
            ManagedMemoryPolicy(100 * 100 * 4 * 2,
                                gpu::MemoryAllocation::CUTOFF_ALLOW_EVERYTHING,
                                1000));
        break;
      case 2:
        // Only two backings should have memory.
        EXPECT_EQ(
            100u * 100u * 4u * 2u,
            layer_tree_host()->contents_texture_manager()->MemoryUseBytes());
        // Become backgrounded, which will cause 1 more resource to be
        // evicted.
        PostSetVisibleToMainThread(false);
        break;
      default:
        // No further commits should happen because this is not visible
        // anymore.
        NOTREACHED();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> root_layer_;
  scoped_refptr<FakeContentLayer> child_layer1_;
  scoped_refptr<FakeContentLayer> child_layer2_;
  int num_commits_;
};

SINGLE_AND_MULTI_THREAD_NOIMPL_TEST_F(
    LayerTreeHostTestShutdownWithOnlySomeResourcesEvicted);

class LayerTreeHostTestLCDNotification : public LayerTreeHostTest {
 public:
  class NotificationClient : public ContentLayerClient {
   public:
    NotificationClient()
        : layer_(0), paint_count_(0), lcd_notification_count_(0) {}

    void set_layer(Layer* layer) { layer_ = layer; }
    int paint_count() const { return paint_count_; }
    int lcd_notification_count() const { return lcd_notification_count_; }

    virtual void PaintContents(
        SkCanvas* canvas,
        const gfx::Rect& clip,
        gfx::RectF* opaque,
        ContentLayerClient::GraphicsContextStatus gc_status) OVERRIDE {
      ++paint_count_;
    }
    virtual void DidChangeLayerCanUseLCDText() OVERRIDE {
      ++lcd_notification_count_;
      layer_->SetNeedsDisplay();
    }
    virtual bool FillsBoundsCompletely() const OVERRIDE { return false; }

   private:
    Layer* layer_;
    int paint_count_;
    int lcd_notification_count_;
  };

  virtual void SetupTree() OVERRIDE {
    scoped_refptr<Layer> root_layer;
    if (layer_tree_host()->settings().impl_side_painting)
      root_layer = PictureLayer::Create(&client_);
    else
      root_layer = ContentLayer::Create(&client_);
    root_layer->SetIsDrawable(true);
    root_layer->SetBounds(gfx::Size(1, 1));

    layer_tree_host()->SetRootLayer(root_layer);
    client_.set_layer(root_layer.get());

    // The expecations are based on the assumption that the default
    // LCD settings are:
    EXPECT_TRUE(layer_tree_host()->settings().can_use_lcd_text);
    EXPECT_FALSE(root_layer->can_use_lcd_text());

    LayerTreeHostTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }
  virtual void AfterTest() OVERRIDE {}

  virtual void DidCommit() OVERRIDE {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        // The first update consists of one LCD notification and one paint.
        EXPECT_EQ(1, client_.lcd_notification_count());
        EXPECT_EQ(1, client_.paint_count());
        // LCD text must have been enabled on the layer.
        EXPECT_TRUE(layer_tree_host()->root_layer()->can_use_lcd_text());
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        // Since nothing changed on layer, there should be no notification
        // or paint on the second update.
        EXPECT_EQ(1, client_.lcd_notification_count());
        EXPECT_EQ(1, client_.paint_count());
        // LCD text must not have changed.
        EXPECT_TRUE(layer_tree_host()->root_layer()->can_use_lcd_text());
        // Change layer opacity that should trigger lcd notification.
        layer_tree_host()->root_layer()->SetOpacity(.5f);
        // No need to request a commit - setting opacity will do it.
        break;
      default:
        // Verify that there is no extra commit due to layer invalidation.
        EXPECT_EQ(3, layer_tree_host()->source_frame_number());
        // LCD notification count should have incremented due to
        // change in layer opacity.
        EXPECT_EQ(2, client_.lcd_notification_count());
        // Paint count should be incremented due to invalidation.
        EXPECT_EQ(2, client_.paint_count());
        // LCD text must have been disabled on the layer due to opacity.
        EXPECT_FALSE(layer_tree_host()->root_layer()->can_use_lcd_text());
        EndTest();
        break;
    }
  }

 private:
  NotificationClient client_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestLCDNotification);

// Verify that the BeginFrame notification is used to initiate rendering.
class LayerTreeHostTestBeginFrameNotification : public LayerTreeHostTest {
 public:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->begin_frame_scheduling_enabled = true;
  }

  virtual void BeginTest() OVERRIDE {
    // This will trigger a SetNeedsBeginFrame which will trigger a
    // BeginFrame.
    PostSetNeedsCommitToMainThread();
  }

  virtual DrawResult PrepareToDrawOnThread(
      LayerTreeHostImpl* host_impl,
      LayerTreeHostImpl::FrameData* frame,
      DrawResult draw_result) OVERRIDE {
    EndTest();
    return DRAW_SUCCESS;
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  base::TimeTicks frame_time_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestBeginFrameNotification);

class LayerTreeHostTestBeginFrameNotificationShutdownWhileEnabled
    : public LayerTreeHostTest {
 public:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->begin_frame_scheduling_enabled = true;
    settings->using_synchronous_renderer_compositor = true;
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    // The BeginFrame notification is turned off now but will get enabled
    // once we return. End test while it's enabled.
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&LayerTreeHostTestBeginFrameNotification::EndTest,
                   base::Unretained(this)));
  }

  virtual void AfterTest() OVERRIDE {}
};

MULTI_THREAD_TEST_F(
    LayerTreeHostTestBeginFrameNotificationShutdownWhileEnabled);

class LayerTreeHostTestAbortedCommitDoesntStall : public LayerTreeHostTest {
 protected:
  LayerTreeHostTestAbortedCommitDoesntStall()
      : commit_count_(0), commit_abort_count_(0), commit_complete_count_(0) {}

  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->begin_frame_scheduling_enabled = true;
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DidCommit() OVERRIDE {
    commit_count_++;
    if (commit_count_ == 4) {
      // After two aborted commits, request a real commit now to make sure a
      // real commit following an aborted commit will still complete and
      // end the test even when the Impl thread is idle.
      layer_tree_host()->SetNeedsCommit();
    }
  }

  virtual void BeginMainFrameAbortedOnThread(LayerTreeHostImpl* host_impl,
                                             bool did_handle) OVERRIDE {
    commit_abort_count_++;
    // Initiate another abortable commit.
    host_impl->SetNeedsCommit();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    commit_complete_count_++;
    if (commit_complete_count_ == 1) {
      // Initiate an abortable commit after the first commit.
      host_impl->SetNeedsCommit();
    } else {
      EndTest();
    }
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(commit_count_, 5);
    EXPECT_EQ(commit_abort_count_, 3);
    EXPECT_EQ(commit_complete_count_, 2);
  }

  int commit_count_;
  int commit_abort_count_;
  int commit_complete_count_;
};

class LayerTreeHostTestAbortedCommitDoesntStallSynchronousCompositor
    : public LayerTreeHostTestAbortedCommitDoesntStall {
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    LayerTreeHostTestAbortedCommitDoesntStall::InitializeSettings(settings);
    settings->using_synchronous_renderer_compositor = true;
  }
};

MULTI_THREAD_TEST_F(
    LayerTreeHostTestAbortedCommitDoesntStallSynchronousCompositor);

class LayerTreeHostTestAbortedCommitDoesntStallDisabledVsync
    : public LayerTreeHostTestAbortedCommitDoesntStall {
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    LayerTreeHostTestAbortedCommitDoesntStall::InitializeSettings(settings);
    settings->throttle_frame_production = false;
  }
};

MULTI_THREAD_TEST_F(LayerTreeHostTestAbortedCommitDoesntStallDisabledVsync);

class LayerTreeHostTestUninvertibleTransformDoesNotBlockActivation
    : public LayerTreeHostTest {
 protected:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->impl_side_painting = true;
  }

  virtual void SetupTree() OVERRIDE {
    LayerTreeHostTest::SetupTree();

    scoped_refptr<Layer> layer = PictureLayer::Create(&client_);
    layer->SetTransform(gfx::Transform(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    layer->SetBounds(gfx::Size(10, 10));
    layer_tree_host()->root_layer()->AddChild(layer);
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

  FakeContentLayerClient client_;
};

MULTI_THREAD_TEST_F(
    LayerTreeHostTestUninvertibleTransformDoesNotBlockActivation);

class LayerTreeHostTestChangeLayerPropertiesInPaintContents
    : public LayerTreeHostTest {
 public:
  class SetBoundsClient : public ContentLayerClient {
   public:
    SetBoundsClient() : layer_(0) {}

    void set_layer(Layer* layer) { layer_ = layer; }

    virtual void PaintContents(
        SkCanvas* canvas,
        const gfx::Rect& clip,
        gfx::RectF* opaque,
        ContentLayerClient::GraphicsContextStatus gc_status) OVERRIDE {
      layer_->SetBounds(gfx::Size(2, 2));
    }

    virtual void DidChangeLayerCanUseLCDText() OVERRIDE {}

    virtual bool FillsBoundsCompletely() const OVERRIDE { return false; }

   private:
    Layer* layer_;
  };

  LayerTreeHostTestChangeLayerPropertiesInPaintContents() : num_commits_(0) {}

  virtual void SetupTree() OVERRIDE {
    scoped_refptr<ContentLayer> root_layer = ContentLayer::Create(&client_);
    root_layer->SetIsDrawable(true);
    root_layer->SetBounds(gfx::Size(1, 1));

    layer_tree_host()->SetRootLayer(root_layer);
    client_.set_layer(root_layer.get());

    LayerTreeHostTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }
  virtual void AfterTest() OVERRIDE {}

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    num_commits_++;
    if (num_commits_ == 1) {
      LayerImpl* root_layer = host_impl->active_tree()->root_layer();
      EXPECT_SIZE_EQ(gfx::Size(1, 1), root_layer->bounds());
    } else {
      LayerImpl* root_layer = host_impl->active_tree()->root_layer();
      EXPECT_SIZE_EQ(gfx::Size(2, 2), root_layer->bounds());
      EndTest();
    }
  }

 private:
  SetBoundsClient client_;
  int num_commits_;
};

SINGLE_THREAD_TEST_F(LayerTreeHostTestChangeLayerPropertiesInPaintContents);

class MockIOSurfaceWebGraphicsContext3D : public TestWebGraphicsContext3D {
 public:
  MockIOSurfaceWebGraphicsContext3D() {
    test_capabilities_.gpu.iosurface = true;
    test_capabilities_.gpu.texture_rectangle = true;
  }

  virtual GLuint createTexture() OVERRIDE {
    return 1;
  }
  MOCK_METHOD1(activeTexture, void(GLenum texture));
  MOCK_METHOD2(bindTexture, void(GLenum target,
                                 GLuint texture_id));
  MOCK_METHOD3(texParameteri, void(GLenum target,
                                   GLenum pname,
                                   GLint param));
  MOCK_METHOD5(texImageIOSurface2DCHROMIUM, void(GLenum target,
                                                 GLint width,
                                                 GLint height,
                                                 GLuint ioSurfaceId,
                                                 GLuint plane));
  MOCK_METHOD4(drawElements, void(GLenum mode,
                                  GLsizei count,
                                  GLenum type,
                                  GLintptr offset));
  MOCK_METHOD1(deleteTexture, void(GLenum texture));
  MOCK_METHOD2(produceTextureCHROMIUM,
               void(GLenum target, const GLbyte* mailbox));
};

class LayerTreeHostTestIOSurfaceDrawing : public LayerTreeHostTest {
 protected:
  virtual scoped_ptr<FakeOutputSurface> CreateFakeOutputSurface(bool fallback)
      OVERRIDE {
    scoped_ptr<MockIOSurfaceWebGraphicsContext3D> mock_context_owned(
        new MockIOSurfaceWebGraphicsContext3D);
    mock_context_ = mock_context_owned.get();

    if (delegating_renderer()) {
      return FakeOutputSurface::CreateDelegating3d(
          mock_context_owned.PassAs<TestWebGraphicsContext3D>());
    } else {
      return FakeOutputSurface::Create3d(
          mock_context_owned.PassAs<TestWebGraphicsContext3D>());
    }
  }

  virtual void SetupTree() OVERRIDE {
    LayerTreeHostTest::SetupTree();

    layer_tree_host()->root_layer()->SetIsDrawable(false);

    io_surface_id_ = 9;
    io_surface_size_ = gfx::Size(6, 7);

    scoped_refptr<IOSurfaceLayer> io_surface_layer = IOSurfaceLayer::Create();
    io_surface_layer->SetBounds(gfx::Size(10, 10));
    io_surface_layer->SetIsDrawable(true);
    io_surface_layer->SetContentsOpaque(true);
    io_surface_layer->SetIOSurfaceProperties(io_surface_id_, io_surface_size_);
    layer_tree_host()->root_layer()->AddChild(io_surface_layer);
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    EXPECT_EQ(0u, host_impl->resource_provider()->num_resources());
    // In WillDraw, the IOSurfaceLayer sets up the io surface texture.

    EXPECT_CALL(*mock_context_, activeTexture(_)).Times(0);
    EXPECT_CALL(*mock_context_, bindTexture(GL_TEXTURE_RECTANGLE_ARB, 1))
        .Times(AtLeast(1));
    EXPECT_CALL(*mock_context_,
                texParameteri(
                    GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR))
        .Times(1);
    EXPECT_CALL(*mock_context_,
                texParameteri(
                    GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR))
        .Times(1);
    EXPECT_CALL(*mock_context_,
                texParameteri(GL_TEXTURE_RECTANGLE_ARB,
                              GL_TEXTURE_POOL_CHROMIUM,
                              GL_TEXTURE_POOL_UNMANAGED_CHROMIUM)).Times(1);
    EXPECT_CALL(*mock_context_,
                texParameteri(GL_TEXTURE_RECTANGLE_ARB,
                              GL_TEXTURE_WRAP_S,
                              GL_CLAMP_TO_EDGE)).Times(1);
    EXPECT_CALL(*mock_context_,
                texParameteri(GL_TEXTURE_RECTANGLE_ARB,
                              GL_TEXTURE_WRAP_T,
                              GL_CLAMP_TO_EDGE)).Times(1);

    EXPECT_CALL(*mock_context_,
                texImageIOSurface2DCHROMIUM(GL_TEXTURE_RECTANGLE_ARB,
                                            io_surface_size_.width(),
                                            io_surface_size_.height(),
                                            io_surface_id_,
                                            0)).Times(1);

    EXPECT_CALL(*mock_context_, bindTexture(_, 0)).Times(AnyNumber());
  }

  virtual DrawResult PrepareToDrawOnThread(
      LayerTreeHostImpl* host_impl,
      LayerTreeHostImpl::FrameData* frame,
      DrawResult draw_result) OVERRIDE {
    Mock::VerifyAndClearExpectations(&mock_context_);
    ResourceProvider* resource_provider = host_impl->resource_provider();
    EXPECT_EQ(1u, resource_provider->num_resources());
    CHECK_EQ(1u, frame->render_passes.size());
    CHECK_LE(1u, frame->render_passes[0]->quad_list.size());
    const DrawQuad* quad = frame->render_passes[0]->quad_list[0];
    CHECK_EQ(DrawQuad::IO_SURFACE_CONTENT, quad->material);
    const IOSurfaceDrawQuad* io_surface_draw_quad =
        IOSurfaceDrawQuad::MaterialCast(quad);
    EXPECT_SIZE_EQ(io_surface_size_, io_surface_draw_quad->io_surface_size);
    EXPECT_NE(0u, io_surface_draw_quad->io_surface_resource_id);
    EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_RECTANGLE_ARB),
              resource_provider->TargetForTesting(
                  io_surface_draw_quad->io_surface_resource_id));

    EXPECT_CALL(*mock_context_, bindTexture(GL_TEXTURE_RECTANGLE_ARB, 1))
        .Times(1);
    if (delegating_renderer()) {
      // The io surface layer's resource should be sent to the parent.
      EXPECT_CALL(*mock_context_,
                  produceTextureCHROMIUM(GL_TEXTURE_RECTANGLE_ARB, _)).Times(1);
    } else {
      // The io surface layer's texture is drawn.
      EXPECT_CALL(*mock_context_, activeTexture(GL_TEXTURE0)).Times(AtLeast(1));
      EXPECT_CALL(*mock_context_, drawElements(GL_TRIANGLES, 6, _, _))
          .Times(AtLeast(1));
    }

    return draw_result;
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    Mock::VerifyAndClearExpectations(&mock_context_);

    EXPECT_CALL(*mock_context_, deleteTexture(1)).Times(AtLeast(1));
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

  int io_surface_id_;
  MockIOSurfaceWebGraphicsContext3D* mock_context_;
  gfx::Size io_surface_size_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestIOSurfaceDrawing);

class LayerTreeHostTestNumFramesPending : public LayerTreeHostTest {
 public:
  virtual void BeginTest() OVERRIDE {
    frame_ = 0;
    PostSetNeedsCommitToMainThread();
  }

  // Round 1: commit + draw
  // Round 2: commit only (no draw/swap)
  // Round 3: draw only (no commit)

  virtual void DidCommit() OVERRIDE {
    int commit = layer_tree_host()->source_frame_number();
    switch (commit) {
      case 2:
        // Round 2 done.
        EXPECT_EQ(1, frame_);
        layer_tree_host()->SetNeedsRedraw();
        break;
    }
  }

  virtual void DidCompleteSwapBuffers() OVERRIDE {
    int commit = layer_tree_host()->source_frame_number();
    ++frame_;
    switch (frame_) {
      case 1:
        // Round 1 done.
        EXPECT_EQ(1, commit);
        layer_tree_host()->SetNeedsCommit();
        break;
      case 2:
        // Round 3 done.
        EXPECT_EQ(2, commit);
        EndTest();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {}

 protected:
  int frame_;
};

// Flaky on all platforms: http://crbug.com/327498
TEST_F(LayerTreeHostTestNumFramesPending, DISABLED_DelegatingRenderer) {
  RunTest(true, true, true);
}

TEST_F(LayerTreeHostTestNumFramesPending, DISABLED_GLRenderer) {
  RunTest(true, false, true);
}

class LayerTreeHostTestDeferredInitialize : public LayerTreeHostTest {
 public:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    // PictureLayer can only be used with impl side painting enabled.
    settings->impl_side_painting = true;
  }

  virtual void SetupTree() OVERRIDE {
    layer_ = FakePictureLayer::Create(&client_);
    // Force commits to not be aborted so new frames get drawn, otherwise
    // the renderer gets deferred initialized but nothing new needs drawing.
    layer_->set_always_update_resources(true);
    layer_tree_host()->SetRootLayer(layer_);
    LayerTreeHostTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    did_initialize_gl_ = false;
    did_release_gl_ = false;
    last_source_frame_number_drawn_ = -1;  // Never drawn.
    PostSetNeedsCommitToMainThread();
  }

  virtual scoped_ptr<FakeOutputSurface> CreateFakeOutputSurface(bool fallback)
      OVERRIDE {
    scoped_ptr<TestWebGraphicsContext3D> context3d(
        TestWebGraphicsContext3D::Create());

    return FakeOutputSurface::CreateDeferredGL(
        scoped_ptr<SoftwareOutputDevice>(new SoftwareOutputDevice),
        delegating_renderer());
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    ASSERT_TRUE(host_impl->RootLayer());
    FakePictureLayerImpl* layer_impl =
        static_cast<FakePictureLayerImpl*>(host_impl->RootLayer());

    // The same frame can be draw multiple times if new visible tiles are
    // rasterized. But we want to make sure we only post DeferredInitialize
    // and ReleaseGL once, so early out if the same frame is drawn again.
    if (last_source_frame_number_drawn_ ==
        host_impl->active_tree()->source_frame_number())
      return;

    last_source_frame_number_drawn_ =
        host_impl->active_tree()->source_frame_number();

    if (!did_initialize_gl_) {
      EXPECT_LE(1u, layer_impl->append_quads_count());
      ImplThreadTaskRunner()->PostTask(
          FROM_HERE,
          base::Bind(
              &LayerTreeHostTestDeferredInitialize::DeferredInitializeAndRedraw,
              base::Unretained(this),
              base::Unretained(host_impl)));
    } else if (did_initialize_gl_ && !did_release_gl_) {
      EXPECT_LE(2u, layer_impl->append_quads_count());
      ImplThreadTaskRunner()->PostTask(
          FROM_HERE,
          base::Bind(&LayerTreeHostTestDeferredInitialize::ReleaseGLAndRedraw,
                     base::Unretained(this),
                     base::Unretained(host_impl)));
    } else if (did_initialize_gl_ && did_release_gl_) {
      EXPECT_LE(3u, layer_impl->append_quads_count());
      EndTest();
    }
  }

  void DeferredInitializeAndRedraw(LayerTreeHostImpl* host_impl) {
    EXPECT_FALSE(did_initialize_gl_);
    // SetAndInitializeContext3D calls SetNeedsCommit.
    FakeOutputSurface* fake_output_surface =
        static_cast<FakeOutputSurface*>(host_impl->output_surface());
    scoped_refptr<TestContextProvider> context_provider =
        TestContextProvider::Create();  // Not bound to thread.
    EXPECT_TRUE(
        fake_output_surface->InitializeAndSetContext3d(context_provider));
    did_initialize_gl_ = true;
  }

  void ReleaseGLAndRedraw(LayerTreeHostImpl* host_impl) {
    EXPECT_TRUE(did_initialize_gl_);
    EXPECT_FALSE(did_release_gl_);
    // ReleaseGL calls SetNeedsCommit.
    static_cast<FakeOutputSurface*>(host_impl->output_surface())->ReleaseGL();
    did_release_gl_ = true;
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE {
    ASSERT_TRUE(result);
    DelegatedFrameData* delegated_frame_data =
        output_surface()->last_sent_frame().delegated_frame_data.get();
    if (!delegated_frame_data)
      return;

    // Return all resources immediately.
    TransferableResourceArray resources_to_return =
        output_surface()->resources_held_by_parent();

    CompositorFrameAck ack;
    for (size_t i = 0; i < resources_to_return.size(); ++i)
      output_surface()->ReturnResource(resources_to_return[i].id, &ack);
    host_impl->ReclaimResources(&ack);
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_TRUE(did_initialize_gl_);
    EXPECT_TRUE(did_release_gl_);
  }

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> layer_;
  bool did_initialize_gl_;
  bool did_release_gl_;
  int last_source_frame_number_drawn_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestDeferredInitialize);

// Test for UI Resource management.
class LayerTreeHostTestUIResource : public LayerTreeHostTest {
 public:
  LayerTreeHostTestUIResource() : num_ui_resources_(0) {}

  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->texture_id_allocation_chunk_size = 1;
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DidCommit() OVERRIDE {
    int frame = layer_tree_host()->source_frame_number();
    switch (frame) {
      case 1:
        CreateResource();
        CreateResource();
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        // Usually ScopedUIResource are deleted from the manager in their
        // destructor.  Here we just want to test that a direct call to
        // DeleteUIResource works.
        layer_tree_host()->DeleteUIResource(ui_resources_[0]->id());
        PostSetNeedsCommitToMainThread();
        break;
      case 3:
        // DeleteUIResource can be called with an invalid id.
        layer_tree_host()->DeleteUIResource(ui_resources_[0]->id());
        PostSetNeedsCommitToMainThread();
        break;
      case 4:
        CreateResource();
        CreateResource();
        PostSetNeedsCommitToMainThread();
        break;
      case 5:
        ClearResources();
        EndTest();
        break;
    }
  }

  void PerformTest(LayerTreeHostImpl* impl) {
    TestWebGraphicsContext3D* context = TestContext();

    int frame = impl->active_tree()->source_frame_number();
    switch (frame) {
      case 0:
        ASSERT_EQ(0u, context->NumTextures());
        break;
      case 1:
        // Created two textures.
        ASSERT_EQ(2u, context->NumTextures());
        break;
      case 2:
        // One texture left after one deletion.
        ASSERT_EQ(1u, context->NumTextures());
        break;
      case 3:
        // Resource manager state should not change when delete is called on an
        // invalid id.
        ASSERT_EQ(1u, context->NumTextures());
        break;
      case 4:
        // Creation after deletion: two more creates should total up to
        // three textures.
        ASSERT_EQ(3u, context->NumTextures());
        break;
    }
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    if (!layer_tree_host()->settings().impl_side_painting)
      PerformTest(impl);
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    if (layer_tree_host()->settings().impl_side_painting)
      PerformTest(impl);
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  // Must clear all resources before exiting.
  void ClearResources() {
    for (int i = 0; i < num_ui_resources_; i++)
      ui_resources_[i].reset();
  }

  void CreateResource() {
    ui_resources_[num_ui_resources_++] =
        FakeScopedUIResource::Create(layer_tree_host());
  }

  scoped_ptr<FakeScopedUIResource> ui_resources_[5];
  int num_ui_resources_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestUIResource);

class PushPropertiesCountingLayerImpl : public LayerImpl {
 public:
  static scoped_ptr<PushPropertiesCountingLayerImpl> Create(
      LayerTreeImpl* tree_impl, int id) {
    return make_scoped_ptr(new PushPropertiesCountingLayerImpl(tree_impl, id));
  }

  virtual ~PushPropertiesCountingLayerImpl() {}

  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE {
    LayerImpl::PushPropertiesTo(layer);
    push_properties_count_++;
    // Push state to the active tree because we can only access it from there.
    static_cast<PushPropertiesCountingLayerImpl*>(
        layer)->push_properties_count_ = push_properties_count_;
  }

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE {
    return PushPropertiesCountingLayerImpl::Create(tree_impl, id()).
        PassAs<LayerImpl>();
  }

  size_t push_properties_count() const { return push_properties_count_; }
  void reset_push_properties_count() { push_properties_count_ = 0; }

 private:
  size_t push_properties_count_;

  PushPropertiesCountingLayerImpl(LayerTreeImpl* tree_impl, int id)
      : LayerImpl(tree_impl, id),
        push_properties_count_(0) {
    SetBounds(gfx::Size(1, 1));
  }
};

class PushPropertiesCountingLayer : public Layer {
 public:
  static scoped_refptr<PushPropertiesCountingLayer> Create() {
    return new PushPropertiesCountingLayer();
  }

  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE {
    Layer::PushPropertiesTo(layer);
    push_properties_count_++;
    if (persist_needs_push_properties_)
      needs_push_properties_ = true;
  }

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE {
    return PushPropertiesCountingLayerImpl::Create(tree_impl, id()).
        PassAs<LayerImpl>();
  }

  size_t push_properties_count() const { return push_properties_count_; }
  void reset_push_properties_count() { push_properties_count_ = 0; }

  void set_persist_needs_push_properties(bool persist) {
    persist_needs_push_properties_ = persist;
  }

 private:
  PushPropertiesCountingLayer()
      : push_properties_count_(0), persist_needs_push_properties_(false) {
    SetBounds(gfx::Size(1, 1));
    SetIsDrawable(true);
  }
  virtual ~PushPropertiesCountingLayer() {}

  size_t push_properties_count_;
  bool persist_needs_push_properties_;
};

class LayerTreeHostTestLayersPushProperties : public LayerTreeHostTest {
 protected:
  virtual void BeginTest() OVERRIDE {
    num_commits_ = 0;
    expected_push_properties_root_ = 0;
    expected_push_properties_child_ = 0;
    expected_push_properties_grandchild_ = 0;
    expected_push_properties_child2_ = 0;
    expected_push_properties_other_root_ = 0;
    expected_push_properties_leaf_layer_ = 0;
    PostSetNeedsCommitToMainThread();
  }

  virtual void SetupTree() OVERRIDE {
    root_ = PushPropertiesCountingLayer::Create();
    child_ = PushPropertiesCountingLayer::Create();
    child2_ = PushPropertiesCountingLayer::Create();
    grandchild_ = PushPropertiesCountingLayer::Create();
    leaf_always_pushing_layer_ = PushPropertiesCountingLayer::Create();
    leaf_always_pushing_layer_->set_persist_needs_push_properties(true);

    root_->AddChild(child_);
    root_->AddChild(child2_);
    child_->AddChild(grandchild_);
    child2_->AddChild(leaf_always_pushing_layer_);

    other_root_ = PushPropertiesCountingLayer::Create();

    // Don't set the root layer here.
    LayerTreeHostTest::SetupTree();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    ++num_commits_;

    EXPECT_EQ(expected_push_properties_root_, root_->push_properties_count());
    EXPECT_EQ(expected_push_properties_child_, child_->push_properties_count());
    EXPECT_EQ(expected_push_properties_grandchild_,
              grandchild_->push_properties_count());
    EXPECT_EQ(expected_push_properties_child2_,
              child2_->push_properties_count());
    EXPECT_EQ(expected_push_properties_other_root_,
              other_root_->push_properties_count());
    EXPECT_EQ(expected_push_properties_leaf_layer_,
              leaf_always_pushing_layer_->push_properties_count());

    // The scrollbar layer always needs to be pushed.
    if (root_->layer_tree_host()) {
      EXPECT_TRUE(root_->descendant_needs_push_properties());
      EXPECT_FALSE(root_->needs_push_properties());
    }
    if (child2_->layer_tree_host()) {
      EXPECT_TRUE(child2_->descendant_needs_push_properties());
      EXPECT_FALSE(child2_->needs_push_properties());
    }
    if (leaf_always_pushing_layer_->layer_tree_host()) {
      EXPECT_FALSE(
          leaf_always_pushing_layer_->descendant_needs_push_properties());
      EXPECT_TRUE(leaf_always_pushing_layer_->needs_push_properties());
    }

    // child_ and grandchild_ don't persist their need to push properties.
    if (child_->layer_tree_host()) {
      EXPECT_FALSE(child_->descendant_needs_push_properties());
      EXPECT_FALSE(child_->needs_push_properties());
    }
    if (grandchild_->layer_tree_host()) {
      EXPECT_FALSE(grandchild_->descendant_needs_push_properties());
      EXPECT_FALSE(grandchild_->needs_push_properties());
    }

    if (other_root_->layer_tree_host()) {
      EXPECT_FALSE(other_root_->descendant_needs_push_properties());
      EXPECT_FALSE(other_root_->needs_push_properties());
    }

    switch (num_commits_) {
      case 1:
        layer_tree_host()->SetRootLayer(root_);
        // Layers added to the tree get committed.
        ++expected_push_properties_root_;
        ++expected_push_properties_child_;
        ++expected_push_properties_grandchild_;
        ++expected_push_properties_child2_;
        break;
      case 2:
        layer_tree_host()->SetNeedsCommit();
        // No layers need commit.
        break;
      case 3:
        layer_tree_host()->SetRootLayer(other_root_);
        // Layers added to the tree get committed.
        ++expected_push_properties_other_root_;
        break;
      case 4:
        layer_tree_host()->SetRootLayer(root_);
        // Layers added to the tree get committed.
        ++expected_push_properties_root_;
        ++expected_push_properties_child_;
        ++expected_push_properties_grandchild_;
        ++expected_push_properties_child2_;
        break;
      case 5:
        layer_tree_host()->SetNeedsCommit();
        // No layers need commit.
        break;
      case 6:
        child_->RemoveFromParent();
        // No layers need commit.
        break;
      case 7:
        root_->AddChild(child_);
        // Layers added to the tree get committed.
        ++expected_push_properties_child_;
        ++expected_push_properties_grandchild_;
        break;
      case 8:
        grandchild_->RemoveFromParent();
        // No layers need commit.
        break;
      case 9:
        child_->AddChild(grandchild_);
        // Layers added to the tree get committed.
        ++expected_push_properties_grandchild_;
        break;
      case 10:
        layer_tree_host()->SetViewportSize(gfx::Size(20, 20));
        // No layers need commit.
        break;
      case 11:
        layer_tree_host()->SetPageScaleFactorAndLimits(1.f, 0.8f, 1.1f);
        // No layers need commit.
        break;
      case 12:
        child_->SetPosition(gfx::Point(1, 1));
        // The modified layer needs commit
        ++expected_push_properties_child_;
        break;
      case 13:
        child2_->SetPosition(gfx::Point(1, 1));
        // The modified layer needs commit
        ++expected_push_properties_child2_;
        break;
      case 14:
        child_->RemoveFromParent();
        root_->AddChild(child_);
        // Layers added to the tree get committed.
        ++expected_push_properties_child_;
        ++expected_push_properties_grandchild_;
        break;
      case 15:
        grandchild_->SetPosition(gfx::Point(1, 1));
        // The modified layer needs commit
        ++expected_push_properties_grandchild_;
        break;
      case 16:
        // SetNeedsDisplay does not always set needs commit (so call it
        // explicitly), but is a property change.
        child_->SetNeedsDisplay();
        ++expected_push_properties_child_;
        layer_tree_host()->SetNeedsCommit();
        break;
      case 17:
        EndTest();
        break;
    }

    // The leaf layer always pushes.
    if (leaf_always_pushing_layer_->layer_tree_host())
      ++expected_push_properties_leaf_layer_;
  }

  virtual void AfterTest() OVERRIDE {}

  int num_commits_;
  FakeContentLayerClient client_;
  scoped_refptr<PushPropertiesCountingLayer> root_;
  scoped_refptr<PushPropertiesCountingLayer> child_;
  scoped_refptr<PushPropertiesCountingLayer> child2_;
  scoped_refptr<PushPropertiesCountingLayer> grandchild_;
  scoped_refptr<PushPropertiesCountingLayer> other_root_;
  scoped_refptr<PushPropertiesCountingLayer> leaf_always_pushing_layer_;
  size_t expected_push_properties_root_;
  size_t expected_push_properties_child_;
  size_t expected_push_properties_child2_;
  size_t expected_push_properties_grandchild_;
  size_t expected_push_properties_other_root_;
  size_t expected_push_properties_leaf_layer_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestLayersPushProperties);

class LayerTreeHostTestImplLayersPushProperties
    : public LayerTreeHostTestLayersPushProperties {
 protected:
  virtual void BeginTest() OVERRIDE {
    expected_push_properties_root_impl_ = 0;
    expected_push_properties_child_impl_ = 0;
    expected_push_properties_grandchild_impl_ = 0;
    expected_push_properties_child2_impl_ = 0;
    expected_push_properties_grandchild2_impl_ = 0;
    LayerTreeHostTestLayersPushProperties::BeginTest();
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    // These commits are in response to the changes made in
    // LayerTreeHostTestLayersPushProperties::DidCommitAndDrawFrame()
    switch (num_commits_) {
      case 0:
        // Tree hasn't been setup yet don't bother to check anything.
        return;
      case 1:
        // Root gets set up, Everyone is initialized.
        ++expected_push_properties_root_impl_;
        ++expected_push_properties_child_impl_;
        ++expected_push_properties_grandchild_impl_;
        ++expected_push_properties_child2_impl_;
        ++expected_push_properties_grandchild2_impl_;
        break;
      case 2:
        // Tree doesn't change but the one leaf that always pushes is pushed.
        ++expected_push_properties_grandchild2_impl_;
        break;
      case 3:
        // Root is swapped here.
        // Clear the expected push properties the tree will be rebuilt.
        expected_push_properties_root_impl_ = 0;
        expected_push_properties_child_impl_ = 0;
        expected_push_properties_grandchild_impl_ = 0;
        expected_push_properties_child2_impl_ = 0;
        expected_push_properties_grandchild2_impl_ = 0;

        // Make sure the new root is pushed.
        EXPECT_EQ(1u, static_cast<PushPropertiesCountingLayerImpl*>(
                host_impl->RootLayer())->push_properties_count());
        return;
      case 4:
        // Root is swapped back all of the layers in the tree get pushed.
        ++expected_push_properties_root_impl_;
        ++expected_push_properties_child_impl_;
        ++expected_push_properties_grandchild_impl_;
        ++expected_push_properties_child2_impl_;
        ++expected_push_properties_grandchild2_impl_;
        break;
      case 5:
        // Tree doesn't change but the one leaf that always pushes is pushed.
        ++expected_push_properties_grandchild2_impl_;
        break;
      case 6:
        // First child is removed. Structure of the tree changes here so swap
        // some of the values. child_impl becomes child2_impl.
        expected_push_properties_child_impl_ =
            expected_push_properties_child2_impl_;
        expected_push_properties_child2_impl_ = 0;
        // grandchild_impl becomes grandchild2_impl.
        expected_push_properties_grandchild_impl_ =
            expected_push_properties_grandchild2_impl_;
        expected_push_properties_grandchild2_impl_ = 0;

        // grandchild_impl is now the leaf that always pushes. It is pushed.
        ++expected_push_properties_grandchild_impl_;
        break;
      case 7:
        // The leaf that always pushes is pushed.
        ++expected_push_properties_grandchild_impl_;

        // Child is added back. New layers are initialized.
        ++expected_push_properties_grandchild2_impl_;
        ++expected_push_properties_child2_impl_;
        break;
      case 8:
        // Leaf is removed.
        expected_push_properties_grandchild2_impl_ = 0;

        // Always pushing.
        ++expected_push_properties_grandchild_impl_;
        break;
      case 9:
        // Leaf is added back
        ++expected_push_properties_grandchild2_impl_;

        // The leaf that always pushes is pushed.
        ++expected_push_properties_grandchild_impl_;
        break;
      case 10:
        // The leaf that always pushes is pushed.
        ++expected_push_properties_grandchild_impl_;
        break;
      case 11:
        // The leaf that always pushes is pushed.
        ++expected_push_properties_grandchild_impl_;
        break;
      case 12:
        // The leaf that always pushes is pushed.
        ++expected_push_properties_grandchild_impl_;

        // This child position was changed.
        ++expected_push_properties_child2_impl_;
        break;
      case 13:
        // The position of this child was changed.
        ++expected_push_properties_child_impl_;

        // The leaf that always pushes is pushed.
        ++expected_push_properties_grandchild_impl_;
        break;
      case 14:
        // Second child is removed from tree. Don't discard counts because
        // they are added back before commit.

        // The leaf that always pushes is pushed.
        ++expected_push_properties_grandchild_impl_;

        // Second child added back.
        ++expected_push_properties_child2_impl_;
        ++expected_push_properties_grandchild2_impl_;

        break;
      case 15:
        // The position of this child was changed.
        ++expected_push_properties_grandchild2_impl_;

        // The leaf that always pushes is pushed.
        ++expected_push_properties_grandchild_impl_;
        break;
      case 16:
        // Second child is invalidated with SetNeedsDisplay
        ++expected_push_properties_child2_impl_;

        // The leaf that always pushed is pushed.
        ++expected_push_properties_grandchild_impl_;
        break;
    }

    PushPropertiesCountingLayerImpl* root_impl_ = NULL;
    PushPropertiesCountingLayerImpl* child_impl_ = NULL;
    PushPropertiesCountingLayerImpl* child2_impl_ = NULL;
    PushPropertiesCountingLayerImpl* grandchild_impl_ = NULL;
    PushPropertiesCountingLayerImpl* leaf_always_pushing_layer_impl_ = NULL;

    // Pull the layers that we need from the tree assuming the same structure
    // as LayerTreeHostTestLayersPushProperties
    root_impl_ = static_cast<PushPropertiesCountingLayerImpl*>(
        host_impl->RootLayer());

    if (root_impl_ && root_impl_->children().size() > 0) {
      child_impl_ = static_cast<PushPropertiesCountingLayerImpl*>(
          root_impl_->children()[0]);

      if (child_impl_ && child_impl_->children().size() > 0)
        grandchild_impl_ = static_cast<PushPropertiesCountingLayerImpl*>(
            child_impl_->children()[0]);
    }

    if (root_impl_ && root_impl_->children().size() > 1) {
      child2_impl_ = static_cast<PushPropertiesCountingLayerImpl*>(
          root_impl_->children()[1]);

      if (child2_impl_ && child2_impl_->children().size() > 0)
        leaf_always_pushing_layer_impl_ =
            static_cast<PushPropertiesCountingLayerImpl*>(
                child2_impl_->children()[0]);
    }

    if (root_impl_)
      EXPECT_EQ(expected_push_properties_root_impl_,
                root_impl_->push_properties_count());
    if (child_impl_)
      EXPECT_EQ(expected_push_properties_child_impl_,
                child_impl_->push_properties_count());
    if (grandchild_impl_)
      EXPECT_EQ(expected_push_properties_grandchild_impl_,
                grandchild_impl_->push_properties_count());
    if (child2_impl_)
      EXPECT_EQ(expected_push_properties_child2_impl_,
                child2_impl_->push_properties_count());
    if (leaf_always_pushing_layer_impl_)
      EXPECT_EQ(expected_push_properties_grandchild2_impl_,
                leaf_always_pushing_layer_impl_->push_properties_count());
  }

  size_t expected_push_properties_root_impl_;
  size_t expected_push_properties_child_impl_;
  size_t expected_push_properties_child2_impl_;
  size_t expected_push_properties_grandchild_impl_;
  size_t expected_push_properties_grandchild2_impl_;
};

TEST_F(LayerTreeHostTestImplLayersPushProperties, DelegatingRenderer) {
  RunTestWithImplSidePainting();
}

class LayerTreeHostTestPropertyChangesDuringUpdateArePushed
    : public LayerTreeHostTest {
 protected:
  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void SetupTree() OVERRIDE {
    root_ = Layer::Create();
    root_->SetBounds(gfx::Size(1, 1));

    bool paint_scrollbar = true;
    bool has_thumb = false;
    scrollbar_layer_ = FakePaintedScrollbarLayer::Create(
        paint_scrollbar, has_thumb, root_->id());

    root_->AddChild(scrollbar_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostTest::SetupTree();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    switch (layer_tree_host()->source_frame_number()) {
      case 0:
        break;
      case 1: {
        // During update, the ignore_set_needs_commit_ bit is set to true to
        // avoid causing a second commit to be scheduled. If a property change
        // is made during this, however, it needs to be pushed in the upcoming
        // commit.
        scoped_ptr<base::AutoReset<bool> > ignore =
            scrollbar_layer_->IgnoreSetNeedsCommit();

        scrollbar_layer_->SetBounds(gfx::Size(30, 30));

        EXPECT_TRUE(scrollbar_layer_->needs_push_properties());
        EXPECT_TRUE(root_->descendant_needs_push_properties());
        layer_tree_host()->SetNeedsCommit();

        scrollbar_layer_->reset_push_properties_count();
        EXPECT_EQ(0u, scrollbar_layer_->push_properties_count());
        break;
      }
      case 2:
        EXPECT_EQ(1u, scrollbar_layer_->push_properties_count());
        EndTest();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {}

  scoped_refptr<Layer> root_;
  scoped_refptr<FakePaintedScrollbarLayer> scrollbar_layer_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestPropertyChangesDuringUpdateArePushed);

class LayerTreeHostTestCasePushPropertiesThreeGrandChildren
    : public LayerTreeHostTest {
 protected:
  virtual void BeginTest() OVERRIDE {
    expected_push_properties_root_ = 0;
    expected_push_properties_child_ = 0;
    expected_push_properties_grandchild1_ = 0;
    expected_push_properties_grandchild2_ = 0;
    expected_push_properties_grandchild3_ = 0;
    PostSetNeedsCommitToMainThread();
  }

  virtual void SetupTree() OVERRIDE {
    root_ = PushPropertiesCountingLayer::Create();
    child_ = PushPropertiesCountingLayer::Create();
    grandchild1_ = PushPropertiesCountingLayer::Create();
    grandchild2_ = PushPropertiesCountingLayer::Create();
    grandchild3_ = PushPropertiesCountingLayer::Create();

    root_->AddChild(child_);
    child_->AddChild(grandchild1_);
    child_->AddChild(grandchild2_);
    child_->AddChild(grandchild3_);

    // Don't set the root layer here.
    LayerTreeHostTest::SetupTree();
  }

  virtual void AfterTest() OVERRIDE {}

  FakeContentLayerClient client_;
  scoped_refptr<PushPropertiesCountingLayer> root_;
  scoped_refptr<PushPropertiesCountingLayer> child_;
  scoped_refptr<PushPropertiesCountingLayer> grandchild1_;
  scoped_refptr<PushPropertiesCountingLayer> grandchild2_;
  scoped_refptr<PushPropertiesCountingLayer> grandchild3_;
  size_t expected_push_properties_root_;
  size_t expected_push_properties_child_;
  size_t expected_push_properties_grandchild1_;
  size_t expected_push_properties_grandchild2_;
  size_t expected_push_properties_grandchild3_;
};

class LayerTreeHostTestPushPropertiesAddingToTreeRequiresPush
    : public LayerTreeHostTestCasePushPropertiesThreeGrandChildren {
 protected:
  virtual void DidCommitAndDrawFrame() OVERRIDE {
    int last_source_frame_number = layer_tree_host()->source_frame_number() - 1;
    switch (last_source_frame_number) {
      case 0:
        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_FALSE(root_->descendant_needs_push_properties());
        EXPECT_FALSE(child_->needs_push_properties());
        EXPECT_FALSE(child_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild1_->needs_push_properties());
        EXPECT_FALSE(grandchild1_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild2_->needs_push_properties());
        EXPECT_FALSE(grandchild2_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild3_->needs_push_properties());
        EXPECT_FALSE(grandchild3_->descendant_needs_push_properties());

        layer_tree_host()->SetRootLayer(root_);

        EXPECT_TRUE(root_->needs_push_properties());
        EXPECT_TRUE(root_->descendant_needs_push_properties());
        EXPECT_TRUE(child_->needs_push_properties());
        EXPECT_TRUE(child_->descendant_needs_push_properties());
        EXPECT_TRUE(grandchild1_->needs_push_properties());
        EXPECT_FALSE(grandchild1_->descendant_needs_push_properties());
        EXPECT_TRUE(grandchild2_->needs_push_properties());
        EXPECT_FALSE(grandchild2_->descendant_needs_push_properties());
        EXPECT_TRUE(grandchild3_->needs_push_properties());
        EXPECT_FALSE(grandchild3_->descendant_needs_push_properties());
        break;
      case 1:
        EndTest();
        break;
    }
  }
};

MULTI_THREAD_TEST_F(LayerTreeHostTestPushPropertiesAddingToTreeRequiresPush);

class LayerTreeHostTestPushPropertiesRemovingChildStopsRecursion
    : public LayerTreeHostTestCasePushPropertiesThreeGrandChildren {
 protected:
  virtual void DidCommitAndDrawFrame() OVERRIDE {
    int last_source_frame_number = layer_tree_host()->source_frame_number() - 1;
    switch (last_source_frame_number) {
      case 0:
        layer_tree_host()->SetRootLayer(root_);
        break;
      case 1:
        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_FALSE(root_->descendant_needs_push_properties());
        EXPECT_FALSE(child_->needs_push_properties());
        EXPECT_FALSE(child_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild1_->needs_push_properties());
        EXPECT_FALSE(grandchild1_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild2_->needs_push_properties());
        EXPECT_FALSE(grandchild2_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild3_->needs_push_properties());
        EXPECT_FALSE(grandchild3_->descendant_needs_push_properties());

        grandchild1_->RemoveFromParent();
        grandchild1_->SetPosition(gfx::Point(1, 1));

        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_FALSE(root_->descendant_needs_push_properties());
        EXPECT_FALSE(child_->needs_push_properties());
        EXPECT_FALSE(child_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild2_->needs_push_properties());
        EXPECT_FALSE(grandchild2_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild3_->needs_push_properties());
        EXPECT_FALSE(grandchild3_->descendant_needs_push_properties());

        child_->AddChild(grandchild1_);

        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_TRUE(root_->descendant_needs_push_properties());
        EXPECT_FALSE(child_->needs_push_properties());
        EXPECT_TRUE(child_->descendant_needs_push_properties());
        EXPECT_TRUE(grandchild1_->needs_push_properties());
        EXPECT_FALSE(grandchild1_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild2_->needs_push_properties());
        EXPECT_FALSE(grandchild2_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild3_->needs_push_properties());
        EXPECT_FALSE(grandchild3_->descendant_needs_push_properties());

        grandchild2_->SetPosition(gfx::Point(1, 1));

        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_TRUE(root_->descendant_needs_push_properties());
        EXPECT_FALSE(child_->needs_push_properties());
        EXPECT_TRUE(child_->descendant_needs_push_properties());
        EXPECT_TRUE(grandchild1_->needs_push_properties());
        EXPECT_FALSE(grandchild1_->descendant_needs_push_properties());
        EXPECT_TRUE(grandchild2_->needs_push_properties());
        EXPECT_FALSE(grandchild2_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild3_->needs_push_properties());
        EXPECT_FALSE(grandchild3_->descendant_needs_push_properties());

        // grandchild2_ will still need a push properties.
        grandchild1_->RemoveFromParent();

        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_TRUE(root_->descendant_needs_push_properties());
        EXPECT_FALSE(child_->needs_push_properties());
        EXPECT_TRUE(child_->descendant_needs_push_properties());

        // grandchild3_ does not need a push properties, so recursing should
        // no longer be needed.
        grandchild2_->RemoveFromParent();

        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_FALSE(root_->descendant_needs_push_properties());
        EXPECT_FALSE(child_->needs_push_properties());
        EXPECT_FALSE(child_->descendant_needs_push_properties());
        EndTest();
        break;
    }
  }
};

MULTI_THREAD_TEST_F(LayerTreeHostTestPushPropertiesRemovingChildStopsRecursion);

class LayerTreeHostTestPushPropertiesRemovingChildStopsRecursionWithPersistence
    : public LayerTreeHostTestCasePushPropertiesThreeGrandChildren {
 protected:
  virtual void DidCommitAndDrawFrame() OVERRIDE {
    int last_source_frame_number = layer_tree_host()->source_frame_number() - 1;
    switch (last_source_frame_number) {
      case 0:
        layer_tree_host()->SetRootLayer(root_);
        grandchild1_->set_persist_needs_push_properties(true);
        grandchild2_->set_persist_needs_push_properties(true);
        break;
      case 1:
        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_TRUE(root_->descendant_needs_push_properties());
        EXPECT_FALSE(child_->needs_push_properties());
        EXPECT_TRUE(child_->descendant_needs_push_properties());
        EXPECT_TRUE(grandchild1_->needs_push_properties());
        EXPECT_FALSE(grandchild1_->descendant_needs_push_properties());
        EXPECT_TRUE(grandchild2_->needs_push_properties());
        EXPECT_FALSE(grandchild2_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild3_->needs_push_properties());
        EXPECT_FALSE(grandchild3_->descendant_needs_push_properties());

        // grandchild2_ will still need a push properties.
        grandchild1_->RemoveFromParent();

        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_TRUE(root_->descendant_needs_push_properties());
        EXPECT_FALSE(child_->needs_push_properties());
        EXPECT_TRUE(child_->descendant_needs_push_properties());

        // grandchild3_ does not need a push properties, so recursing should
        // no longer be needed.
        grandchild2_->RemoveFromParent();

        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_FALSE(root_->descendant_needs_push_properties());
        EXPECT_FALSE(child_->needs_push_properties());
        EXPECT_FALSE(child_->descendant_needs_push_properties());
        EndTest();
        break;
    }
  }
};

MULTI_THREAD_TEST_F(
    LayerTreeHostTestPushPropertiesRemovingChildStopsRecursionWithPersistence);

class LayerTreeHostTestPushPropertiesSetPropertiesWhileOutsideTree
    : public LayerTreeHostTestCasePushPropertiesThreeGrandChildren {
 protected:
  virtual void DidCommitAndDrawFrame() OVERRIDE {
    int last_source_frame_number = layer_tree_host()->source_frame_number() - 1;
    switch (last_source_frame_number) {
      case 0:
        layer_tree_host()->SetRootLayer(root_);
        break;
      case 1:
        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_FALSE(root_->descendant_needs_push_properties());
        EXPECT_FALSE(child_->needs_push_properties());
        EXPECT_FALSE(child_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild1_->needs_push_properties());
        EXPECT_FALSE(grandchild1_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild2_->needs_push_properties());
        EXPECT_FALSE(grandchild2_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild3_->needs_push_properties());
        EXPECT_FALSE(grandchild3_->descendant_needs_push_properties());

        // Change grandchildren while their parent is not in the tree.
        child_->RemoveFromParent();
        grandchild1_->SetPosition(gfx::Point(1, 1));
        grandchild2_->SetPosition(gfx::Point(1, 1));
        root_->AddChild(child_);

        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_TRUE(root_->descendant_needs_push_properties());
        EXPECT_TRUE(child_->needs_push_properties());
        EXPECT_TRUE(child_->descendant_needs_push_properties());
        EXPECT_TRUE(grandchild1_->needs_push_properties());
        EXPECT_FALSE(grandchild1_->descendant_needs_push_properties());
        EXPECT_TRUE(grandchild2_->needs_push_properties());
        EXPECT_FALSE(grandchild2_->descendant_needs_push_properties());
        EXPECT_TRUE(grandchild3_->needs_push_properties());
        EXPECT_FALSE(grandchild3_->descendant_needs_push_properties());

        grandchild1_->RemoveFromParent();

        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_TRUE(root_->descendant_needs_push_properties());
        EXPECT_TRUE(child_->needs_push_properties());
        EXPECT_TRUE(child_->descendant_needs_push_properties());

        grandchild2_->RemoveFromParent();

        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_TRUE(root_->descendant_needs_push_properties());
        EXPECT_TRUE(child_->needs_push_properties());
        EXPECT_TRUE(child_->descendant_needs_push_properties());

        grandchild3_->RemoveFromParent();

        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_TRUE(root_->descendant_needs_push_properties());
        EXPECT_TRUE(child_->needs_push_properties());
        EXPECT_FALSE(child_->descendant_needs_push_properties());

        EndTest();
        break;
    }
  }
};

MULTI_THREAD_TEST_F(
    LayerTreeHostTestPushPropertiesSetPropertiesWhileOutsideTree);

class LayerTreeHostTestPushPropertiesSetPropertyInParentThenChild
    : public LayerTreeHostTestCasePushPropertiesThreeGrandChildren {
 protected:
  virtual void DidCommitAndDrawFrame() OVERRIDE {
    int last_source_frame_number = layer_tree_host()->source_frame_number() - 1;
    switch (last_source_frame_number) {
      case 0:
        layer_tree_host()->SetRootLayer(root_);
        break;
      case 1:
        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_FALSE(root_->descendant_needs_push_properties());
        EXPECT_FALSE(child_->needs_push_properties());
        EXPECT_FALSE(child_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild1_->needs_push_properties());
        EXPECT_FALSE(grandchild1_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild2_->needs_push_properties());
        EXPECT_FALSE(grandchild2_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild3_->needs_push_properties());
        EXPECT_FALSE(grandchild3_->descendant_needs_push_properties());

        child_->SetPosition(gfx::Point(1, 1));
        grandchild1_->SetPosition(gfx::Point(1, 1));
        grandchild2_->SetPosition(gfx::Point(1, 1));

        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_TRUE(root_->descendant_needs_push_properties());
        EXPECT_TRUE(child_->needs_push_properties());
        EXPECT_TRUE(child_->descendant_needs_push_properties());
        EXPECT_TRUE(grandchild1_->needs_push_properties());
        EXPECT_FALSE(grandchild1_->descendant_needs_push_properties());
        EXPECT_TRUE(grandchild2_->needs_push_properties());
        EXPECT_FALSE(grandchild2_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild3_->needs_push_properties());
        EXPECT_FALSE(grandchild3_->descendant_needs_push_properties());

        grandchild1_->RemoveFromParent();

        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_TRUE(root_->descendant_needs_push_properties());
        EXPECT_TRUE(child_->needs_push_properties());
        EXPECT_TRUE(child_->descendant_needs_push_properties());

        grandchild2_->RemoveFromParent();

        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_TRUE(root_->descendant_needs_push_properties());
        EXPECT_TRUE(child_->needs_push_properties());
        EXPECT_FALSE(child_->descendant_needs_push_properties());

        child_->RemoveFromParent();

        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_FALSE(root_->descendant_needs_push_properties());

        EndTest();
        break;
    }
  }
};

MULTI_THREAD_TEST_F(
    LayerTreeHostTestPushPropertiesSetPropertyInParentThenChild);

class LayerTreeHostTestPushPropertiesSetPropertyInChildThenParent
    : public LayerTreeHostTestCasePushPropertiesThreeGrandChildren {
 protected:
  virtual void DidCommitAndDrawFrame() OVERRIDE {
    int last_source_frame_number = layer_tree_host()->source_frame_number() - 1;
    switch (last_source_frame_number) {
      case 0:
        layer_tree_host()->SetRootLayer(root_);
        break;
      case 1:
        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_FALSE(root_->descendant_needs_push_properties());
        EXPECT_FALSE(child_->needs_push_properties());
        EXPECT_FALSE(child_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild1_->needs_push_properties());
        EXPECT_FALSE(grandchild1_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild2_->needs_push_properties());
        EXPECT_FALSE(grandchild2_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild3_->needs_push_properties());
        EXPECT_FALSE(grandchild3_->descendant_needs_push_properties());

        grandchild1_->SetPosition(gfx::Point(1, 1));
        grandchild2_->SetPosition(gfx::Point(1, 1));
        child_->SetPosition(gfx::Point(1, 1));

        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_TRUE(root_->descendant_needs_push_properties());
        EXPECT_TRUE(child_->needs_push_properties());
        EXPECT_TRUE(child_->descendant_needs_push_properties());
        EXPECT_TRUE(grandchild1_->needs_push_properties());
        EXPECT_FALSE(grandchild1_->descendant_needs_push_properties());
        EXPECT_TRUE(grandchild2_->needs_push_properties());
        EXPECT_FALSE(grandchild2_->descendant_needs_push_properties());
        EXPECT_FALSE(grandchild3_->needs_push_properties());
        EXPECT_FALSE(grandchild3_->descendant_needs_push_properties());

        grandchild1_->RemoveFromParent();

        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_TRUE(root_->descendant_needs_push_properties());
        EXPECT_TRUE(child_->needs_push_properties());
        EXPECT_TRUE(child_->descendant_needs_push_properties());

        grandchild2_->RemoveFromParent();

        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_TRUE(root_->descendant_needs_push_properties());
        EXPECT_TRUE(child_->needs_push_properties());
        EXPECT_FALSE(child_->descendant_needs_push_properties());

        child_->RemoveFromParent();

        EXPECT_FALSE(root_->needs_push_properties());
        EXPECT_FALSE(root_->descendant_needs_push_properties());

        EndTest();
        break;
    }
  }
};

MULTI_THREAD_TEST_F(
    LayerTreeHostTestPushPropertiesSetPropertyInChildThenParent);

// This test verifies that the tree activation callback is invoked correctly.
class LayerTreeHostTestTreeActivationCallback : public LayerTreeHostTest {
 public:
  LayerTreeHostTestTreeActivationCallback()
      : num_commits_(0), callback_count_(0) {}

  virtual void BeginTest() OVERRIDE {
    EXPECT_TRUE(HasImplThread());
    PostSetNeedsCommitToMainThread();
  }

  virtual DrawResult PrepareToDrawOnThread(
      LayerTreeHostImpl* host_impl,
      LayerTreeHostImpl::FrameData* frame_data,
      DrawResult draw_result) OVERRIDE {
    ++num_commits_;
    switch (num_commits_) {
      case 1:
        EXPECT_EQ(0, callback_count_);
        callback_count_ = 0;
        SetCallback(true);
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        EXPECT_EQ(1, callback_count_);
        callback_count_ = 0;
        SetCallback(false);
        PostSetNeedsCommitToMainThread();
        break;
      case 3:
        EXPECT_EQ(0, callback_count_);
        callback_count_ = 0;
        EndTest();
        break;
      default:
        ADD_FAILURE() << num_commits_;
        EndTest();
        break;
    }
    return LayerTreeHostTest::PrepareToDrawOnThread(
        host_impl, frame_data, draw_result);
  }

  virtual void AfterTest() OVERRIDE { EXPECT_EQ(3, num_commits_); }

  void SetCallback(bool enable) {
    output_surface()->SetTreeActivationCallback(
        enable
            ? base::Bind(
                  &LayerTreeHostTestTreeActivationCallback::ActivationCallback,
                  base::Unretained(this))
            : base::Closure());
  }

  void ActivationCallback() { ++callback_count_; }

  int num_commits_;
  int callback_count_;
};

TEST_F(LayerTreeHostTestTreeActivationCallback, DirectRenderer) {
  RunTest(true, false, true);
}

TEST_F(LayerTreeHostTestTreeActivationCallback, DelegatingRenderer) {
  RunTest(true, true, true);
}

class LayerInvalidateCausesDraw : public LayerTreeHostTest {
 public:
  LayerInvalidateCausesDraw() : num_commits_(0), num_draws_(0) {}

  virtual void BeginTest() OVERRIDE {
    ASSERT_TRUE(!!invalidate_layer_)
        << "Derived tests must set this in SetupTree";

    // One initial commit.
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    // After commit, invalidate the layer.  This should cause a commit.
    if (layer_tree_host()->source_frame_number() == 1)
      invalidate_layer_->SetNeedsDisplay();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    num_draws_++;
    if (impl->active_tree()->source_frame_number() == 1)
      EndTest();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    num_commits_++;
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_GE(2, num_commits_);
    EXPECT_GE(2, num_draws_);
  }

 protected:
  scoped_refptr<Layer> invalidate_layer_;

 private:
  int num_commits_;
  int num_draws_;
};

// VideoLayer must support being invalidated and then passing that along
// to the compositor thread, even though no resources are updated in
// response to that invalidation.
class LayerTreeHostTestVideoLayerInvalidate : public LayerInvalidateCausesDraw {
 public:
  virtual void SetupTree() OVERRIDE {
    LayerTreeHostTest::SetupTree();
    scoped_refptr<VideoLayer> video_layer = VideoLayer::Create(&provider_);
    video_layer->SetBounds(gfx::Size(10, 10));
    video_layer->SetIsDrawable(true);
    layer_tree_host()->root_layer()->AddChild(video_layer);

    invalidate_layer_ = video_layer;
  }

 private:
  FakeVideoFrameProvider provider_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestVideoLayerInvalidate);

// IOSurfaceLayer must support being invalidated and then passing that along
// to the compositor thread, even though no resources are updated in
// response to that invalidation.
class LayerTreeHostTestIOSurfaceLayerInvalidate
    : public LayerInvalidateCausesDraw {
 public:
  virtual void SetupTree() OVERRIDE {
    LayerTreeHostTest::SetupTree();
    scoped_refptr<IOSurfaceLayer> layer = IOSurfaceLayer::Create();
    layer->SetBounds(gfx::Size(10, 10));
    uint32_t fake_io_surface_id = 7;
    layer->SetIOSurfaceProperties(fake_io_surface_id, layer->bounds());
    layer->SetIsDrawable(true);
    layer_tree_host()->root_layer()->AddChild(layer);

    invalidate_layer_ = layer;
  }
};

// TODO(danakj): IOSurface layer can not be transported. crbug.com/239335
SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    LayerTreeHostTestIOSurfaceLayerInvalidate);

class LayerTreeHostTestPushHiddenLayer : public LayerTreeHostTest {
 protected:
  virtual void SetupTree() OVERRIDE {
    root_layer_ = Layer::Create();
    root_layer_->SetPosition(gfx::Point());
    root_layer_->SetBounds(gfx::Size(10, 10));

    parent_layer_ = SolidColorLayer::Create();
    parent_layer_->SetPosition(gfx::Point());
    parent_layer_->SetBounds(gfx::Size(10, 10));
    parent_layer_->SetIsDrawable(true);
    root_layer_->AddChild(parent_layer_);

    child_layer_ = SolidColorLayer::Create();
    child_layer_->SetPosition(gfx::Point());
    child_layer_->SetBounds(gfx::Size(10, 10));
    child_layer_->SetIsDrawable(true);
    parent_layer_->AddChild(child_layer_);

    layer_tree_host()->SetRootLayer(root_layer_);
    LayerTreeHostTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        // The layer type used does not need to push properties every frame.
        EXPECT_FALSE(child_layer_->needs_push_properties());

        // Change the bounds of the child layer, but make it skipped
        // by CalculateDrawProperties.
        parent_layer_->SetOpacity(0.f);
        child_layer_->SetBounds(gfx::Size(5, 5));
        break;
      case 2:
        // The bounds of the child layer were pushed to the impl side.
        EXPECT_FALSE(child_layer_->needs_push_properties());

        EndTest();
        break;
    }
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* root = impl->active_tree()->root_layer();
    LayerImpl* parent = root->children()[0];
    LayerImpl* child = parent->children()[0];

    switch (impl->active_tree()->source_frame_number()) {
      case 1:
        EXPECT_EQ(gfx::Size(5, 5).ToString(), child->bounds().ToString());
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {}

  scoped_refptr<Layer> root_layer_;
  scoped_refptr<SolidColorLayer> parent_layer_;
  scoped_refptr<SolidColorLayer> child_layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestPushHiddenLayer);

class LayerTreeHostTestUpdateLayerInEmptyViewport : public LayerTreeHostTest {
 protected:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->impl_side_painting = true;
  }

  virtual void SetupTree() OVERRIDE {
    root_layer_ = FakePictureLayer::Create(&client_);
    root_layer_->SetBounds(gfx::Size(10, 10));

    layer_tree_host()->SetRootLayer(root_layer_);
    LayerTreeHostTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    // The viewport is empty, but we still need to update layers on the main
    // thread.
    layer_tree_host()->SetViewportSize(gfx::Size(0, 0));
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommit() OVERRIDE {
    // The layer should be updated even though the viewport is empty, so we
    // are capable of drawing it on the impl tree.
    EXPECT_GT(root_layer_->update_count(), 0u);
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> root_layer_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestUpdateLayerInEmptyViewport);

class LayerTreeHostTestAbortEvictedTextures : public LayerTreeHostTest {
 public:
  LayerTreeHostTestAbortEvictedTextures()
      : num_will_begin_main_frames_(0), num_impl_commits_(0) {}

 protected:
  virtual void SetupTree() OVERRIDE {
    scoped_refptr<SolidColorLayer> root_layer = SolidColorLayer::Create();
    root_layer->SetBounds(gfx::Size(200, 200));
    root_layer->SetIsDrawable(true);

    layer_tree_host()->SetRootLayer(root_layer);
    LayerTreeHostTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void WillBeginMainFrame() OVERRIDE {
    num_will_begin_main_frames_++;
    switch (num_will_begin_main_frames_) {
      case 2:
        // Send a redraw to the compositor thread.  This will (wrongly) be
        // ignored unless aborting resets the texture state.
        layer_tree_host()->SetNeedsRedraw();
        break;
    }
  }

  virtual void BeginCommitOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    num_impl_commits_++;
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    switch (impl->SourceAnimationFrameNumber()) {
      case 1:
        // Prevent draws until commit.
        impl->active_tree()->SetContentsTexturesPurged();
        EXPECT_FALSE(impl->CanDraw());
        // Trigger an abortable commit.
        impl->SetNeedsCommit();
        break;
      case 2:
        EndTest();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {
    // Ensure that the commit was truly aborted.
    EXPECT_EQ(2, num_will_begin_main_frames_);
    EXPECT_EQ(1, num_impl_commits_);
  }

 private:
  int num_will_begin_main_frames_;
  int num_impl_commits_;
};

// Commits can only be aborted when using the thread proxy.
MULTI_THREAD_TEST_F(LayerTreeHostTestAbortEvictedTextures);

class LayerTreeHostTestMaxTransferBufferUsageBytes : public LayerTreeHostTest {
 protected:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->impl_side_painting = true;
  }

  virtual scoped_ptr<FakeOutputSurface> CreateFakeOutputSurface(bool fallback)
      OVERRIDE {
    scoped_refptr<TestContextProvider> context_provider =
        TestContextProvider::Create();
    context_provider->SetMaxTransferBufferUsageBytes(1024 * 1024);
    if (delegating_renderer())
      return FakeOutputSurface::CreateDelegating3d(context_provider);
    else
      return FakeOutputSurface::Create3d(context_provider);
  }

  virtual void SetupTree() OVERRIDE {
    scoped_refptr<FakePictureLayer> root_layer =
        FakePictureLayer::Create(&client_);
    root_layer->SetBounds(gfx::Size(6000, 6000));
    root_layer->SetIsDrawable(true);

    layer_tree_host()->SetRootLayer(root_layer);
    LayerTreeHostTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    TestWebGraphicsContext3D* context = TestContext();

    // Expect that the transfer buffer memory used is equal to the
    // MaxTransferBufferUsageBytes value set in CreateOutputSurface.
    EXPECT_EQ(1024 * 1024u, context->max_used_transfer_buffer_usage_bytes());
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  FakeContentLayerClient client_;
};

// Impl-side painting is a multi-threaded compositor feature.
MULTI_THREAD_TEST_F(LayerTreeHostTestMaxTransferBufferUsageBytes);

// Test ensuring that memory limits are sent to the prioritized resource
// manager.
class LayerTreeHostTestMemoryLimits : public LayerTreeHostTest {
 public:
  LayerTreeHostTestMemoryLimits() : num_commits_(0) {}

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void WillCommit() OVERRIDE {
    // Some commits are aborted, so increment number of attempted commits here.
    num_commits_++;
  }

  virtual void DidCommit() OVERRIDE {
    switch (num_commits_) {
      case 1:
        // Verify default values.
        EXPECT_EQ(PrioritizedResourceManager::DefaultMemoryAllocationLimit(),
                  layer_tree_host()
                      ->contents_texture_manager()
                      ->MaxMemoryLimitBytes());
        EXPECT_EQ(PriorityCalculator::AllowEverythingCutoff(),
                  layer_tree_host()
                      ->contents_texture_manager()
                      ->ExternalPriorityCutoff());
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        // The values should remain the same until the commit after the policy
        // is changed.
        EXPECT_EQ(PrioritizedResourceManager::DefaultMemoryAllocationLimit(),
                  layer_tree_host()
                      ->contents_texture_manager()
                      ->MaxMemoryLimitBytes());
        EXPECT_EQ(PriorityCalculator::AllowEverythingCutoff(),
                  layer_tree_host()
                      ->contents_texture_manager()
                      ->ExternalPriorityCutoff());
        break;
      case 3:
        // Verify values were correctly passed.
        EXPECT_EQ(16u * 1024u * 1024u,
                  layer_tree_host()
                      ->contents_texture_manager()
                      ->MaxMemoryLimitBytes());
        EXPECT_EQ(PriorityCalculator::AllowVisibleAndNearbyCutoff(),
                  layer_tree_host()
                      ->contents_texture_manager()
                      ->ExternalPriorityCutoff());
        EndTest();
        break;
      case 4:
        // Make sure no extra commits happen.
        NOTREACHED();
        break;
    }
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    switch (num_commits_) {
      case 1:
        break;
      case 2:
        // This will trigger a commit because the priority cutoff has changed.
        impl->SetMemoryPolicy(ManagedMemoryPolicy(
            16u * 1024u * 1024u,
            gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
            1000));
        break;
      case 3:
        // This will not trigger a commit because the priority cutoff has not
        // changed, and there is already enough memory for all allocations.
        impl->SetMemoryPolicy(ManagedMemoryPolicy(
            32u * 1024u * 1024u,
            gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
            1000));
        break;
      case 4:
        NOTREACHED();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  int num_commits_;
};

SINGLE_AND_MULTI_THREAD_NOIMPL_TEST_F(LayerTreeHostTestMemoryLimits);

}  // namespace

class LayerTreeHostTestSetMemoryPolicyOnLostOutputSurface
    : public LayerTreeHostTest {
 protected:
  LayerTreeHostTestSetMemoryPolicyOnLostOutputSurface()
      : first_output_surface_memory_limit_(4321234),
        second_output_surface_memory_limit_(1234321) {}

  virtual scoped_ptr<FakeOutputSurface> CreateFakeOutputSurface(bool fallback)
      OVERRIDE {
    if (!first_context_provider_) {
      first_context_provider_ = TestContextProvider::Create();
    } else {
      EXPECT_FALSE(second_context_provider_);
      second_context_provider_ = TestContextProvider::Create();
    }

    scoped_refptr<TestContextProvider> provider(second_context_provider_
                                                    ? second_context_provider_
                                                    : first_context_provider_);
    scoped_ptr<FakeOutputSurface> output_surface;
    if (delegating_renderer())
      output_surface = FakeOutputSurface::CreateDelegating3d(provider);
    else
      output_surface = FakeOutputSurface::Create3d(provider);
    output_surface->SetMemoryPolicyToSetAtBind(
        make_scoped_ptr(new ManagedMemoryPolicy(
            second_context_provider_ ? second_output_surface_memory_limit_
                                     : first_output_surface_memory_limit_,
            gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
            ManagedMemoryPolicy::kDefaultNumResourcesLimit)));
    return output_surface.Pass();
  }

  virtual void SetupTree() OVERRIDE {
    root_ = FakeContentLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));
    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    // Lost context sometimes takes two frames to recreate. The third frame
    // is sometimes aborted, so wait until the fourth frame to verify that
    // the memory has been set, and the fifth frame to end the test.
    if (layer_tree_host()->source_frame_number() < 5) {
      layer_tree_host()->SetNeedsCommit();
    } else if (layer_tree_host()->source_frame_number() == 5) {
      EndTest();
    }
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* impl,
                                   bool result) OVERRIDE {
    switch (impl->active_tree()->source_frame_number()) {
      case 1:
        EXPECT_EQ(first_output_surface_memory_limit_,
                  impl->memory_allocation_limit_bytes());
        // Lose the output surface.
        first_context_provider_->TestContext3d()->loseContextCHROMIUM(
            GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
        break;
      case 4:
        EXPECT_EQ(second_output_surface_memory_limit_,
                  impl->memory_allocation_limit_bytes());
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {}

  scoped_refptr<TestContextProvider> first_context_provider_;
  scoped_refptr<TestContextProvider> second_context_provider_;
  size_t first_output_surface_memory_limit_;
  size_t second_output_surface_memory_limit_;
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> root_;
};

// No output to copy for delegated renderers.
SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostTestSetMemoryPolicyOnLostOutputSurface);

struct TestSwapPromiseResult {
  TestSwapPromiseResult()
      : did_swap_called(false),
        did_not_swap_called(false),
        dtor_called(false),
        reason(SwapPromise::DID_NOT_SWAP_UNKNOWN) {}

  bool did_swap_called;
  bool did_not_swap_called;
  bool dtor_called;
  SwapPromise::DidNotSwapReason reason;
  base::Lock lock;
};

class TestSwapPromise : public SwapPromise {
 public:
  explicit TestSwapPromise(TestSwapPromiseResult* result) : result_(result) {}

  virtual ~TestSwapPromise() {
    base::AutoLock lock(result_->lock);
    result_->dtor_called = true;
  }

  virtual void DidSwap(CompositorFrameMetadata* metadata) OVERRIDE {
    base::AutoLock lock(result_->lock);
    EXPECT_FALSE(result_->did_swap_called);
    EXPECT_FALSE(result_->did_not_swap_called);
    result_->did_swap_called = true;
  }

  virtual void DidNotSwap(DidNotSwapReason reason) OVERRIDE {
    base::AutoLock lock(result_->lock);
    EXPECT_FALSE(result_->did_swap_called);
    EXPECT_FALSE(result_->did_not_swap_called);
    result_->did_not_swap_called = true;
    result_->reason = reason;
  }

 private:
  // Not owned.
  TestSwapPromiseResult* result_;
};

class LayerTreeHostTestBreakSwapPromise : public LayerTreeHostTest {
 protected:
  LayerTreeHostTestBreakSwapPromise()
      : commit_count_(0), commit_complete_count_(0) {}

  virtual void WillBeginMainFrame() OVERRIDE {
    ASSERT_LE(commit_count_, 2);
    scoped_ptr<SwapPromise> swap_promise(
        new TestSwapPromise(&swap_promise_result_[commit_count_]));
    layer_tree_host()->QueueSwapPromise(swap_promise.Pass());
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DidCommit() OVERRIDE {
    commit_count_++;
    if (commit_count_ == 2) {
      // This commit will finish.
      layer_tree_host()->SetNeedsCommit();
    }
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    commit_complete_count_++;
    if (commit_complete_count_ == 1) {
      // This commit will be aborted because no actual update.
      PostSetNeedsUpdateLayersToMainThread();
    } else {
      EndTest();
    }
  }

  virtual void AfterTest() OVERRIDE {
    // 3 commits are scheduled. 2 completes. 1 is aborted.
    EXPECT_EQ(commit_count_, 3);
    EXPECT_EQ(commit_complete_count_, 2);

    {
      // The first commit completes and causes swap buffer which finishes
      // the promise.
      base::AutoLock lock(swap_promise_result_[0].lock);
      EXPECT_TRUE(swap_promise_result_[0].did_swap_called);
      EXPECT_FALSE(swap_promise_result_[0].did_not_swap_called);
      EXPECT_TRUE(swap_promise_result_[0].dtor_called);
    }

    {
      // The second commit aborts.
      base::AutoLock lock(swap_promise_result_[1].lock);
      EXPECT_FALSE(swap_promise_result_[1].did_swap_called);
      EXPECT_TRUE(swap_promise_result_[1].did_not_swap_called);
      EXPECT_EQ(SwapPromise::COMMIT_FAILS, swap_promise_result_[1].reason);
      EXPECT_TRUE(swap_promise_result_[1].dtor_called);
    }

    {
      // The last commit completes but it does not cause swap buffer because
      // there is no damage in the frame data.
      base::AutoLock lock(swap_promise_result_[2].lock);
      EXPECT_FALSE(swap_promise_result_[2].did_swap_called);
      EXPECT_TRUE(swap_promise_result_[2].did_not_swap_called);
      EXPECT_EQ(SwapPromise::SWAP_FAILS, swap_promise_result_[2].reason);
      EXPECT_TRUE(swap_promise_result_[2].dtor_called);
    }
  }

  int commit_count_;
  int commit_complete_count_;
  TestSwapPromiseResult swap_promise_result_[3];
};

MULTI_THREAD_TEST_F(LayerTreeHostTestBreakSwapPromise);

class SimpleSwapPromiseMonitor : public SwapPromiseMonitor {
 public:
  SimpleSwapPromiseMonitor(LayerTreeHost* layer_tree_host,
                           LayerTreeHostImpl* layer_tree_host_impl,
                           int* set_needs_commit_count,
                           int* set_needs_redraw_count)
      : SwapPromiseMonitor(layer_tree_host, layer_tree_host_impl),
        set_needs_commit_count_(set_needs_commit_count),
        set_needs_redraw_count_(set_needs_redraw_count) {}

  virtual ~SimpleSwapPromiseMonitor() {}

  virtual void OnSetNeedsCommitOnMain() OVERRIDE {
    (*set_needs_commit_count_)++;
  }

  virtual void OnSetNeedsRedrawOnImpl() OVERRIDE {
    (*set_needs_redraw_count_)++;
  }

 private:
  int* set_needs_commit_count_;
  int* set_needs_redraw_count_;
};

class LayerTreeHostTestSimpleSwapPromiseMonitor : public LayerTreeHostTest {
 public:
  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void WillBeginMainFrame() OVERRIDE {
    int set_needs_commit_count = 0;
    int set_needs_redraw_count = 0;

    {
      scoped_ptr<SimpleSwapPromiseMonitor> swap_promise_monitor(
          new SimpleSwapPromiseMonitor(layer_tree_host(),
                                       NULL,
                                       &set_needs_commit_count,
                                       &set_needs_redraw_count));
      layer_tree_host()->SetNeedsCommit();
      EXPECT_EQ(1, set_needs_commit_count);
      EXPECT_EQ(0, set_needs_redraw_count);
    }

    // Now the monitor is destroyed, SetNeedsCommit() is no longer being
    // monitored.
    layer_tree_host()->SetNeedsCommit();
    EXPECT_EQ(1, set_needs_commit_count);
    EXPECT_EQ(0, set_needs_redraw_count);

    {
      scoped_ptr<SimpleSwapPromiseMonitor> swap_promise_monitor(
          new SimpleSwapPromiseMonitor(layer_tree_host(),
                                       NULL,
                                       &set_needs_commit_count,
                                       &set_needs_redraw_count));
      layer_tree_host()->SetNeedsUpdateLayers();
      EXPECT_EQ(2, set_needs_commit_count);
      EXPECT_EQ(0, set_needs_redraw_count);
    }

    {
      scoped_ptr<SimpleSwapPromiseMonitor> swap_promise_monitor(
          new SimpleSwapPromiseMonitor(layer_tree_host(),
                                       NULL,
                                       &set_needs_commit_count,
                                       &set_needs_redraw_count));
      layer_tree_host()->SetNeedsAnimate();
      EXPECT_EQ(3, set_needs_commit_count);
      EXPECT_EQ(0, set_needs_redraw_count);
    }

    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}
};

MULTI_THREAD_TEST_F(LayerTreeHostTestSimpleSwapPromiseMonitor);

class LayerTreeHostTestHighResRequiredAfterEvictingUIResources
    : public LayerTreeHostTest {
 protected:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->impl_side_painting = true;
  }

  virtual void SetupTree() OVERRIDE {
    LayerTreeHostTest::SetupTree();
    ui_resource_ = FakeScopedUIResource::Create(layer_tree_host());
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    host_impl->EvictAllUIResources();
    // Existence of evicted UI resources will trigger NEW_CONTENT_TAKES_PRIORITY
    // mode. Active tree should require high-res to draw after entering this
    // mode to ensure that high-res tiles are also required for a pending tree
    // to be activated.
    EXPECT_TRUE(host_impl->active_tree()->RequiresHighResToDraw());
  }

  virtual void DidCommit() OVERRIDE {
    int frame = layer_tree_host()->source_frame_number();
    switch (frame) {
      case 1:
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        ui_resource_.reset();
        EndTest();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {}

  FakeContentLayerClient client_;
  scoped_ptr<FakeScopedUIResource> ui_resource_;
};

// This test is flaky, see http://crbug.com/386199
//MULTI_THREAD_TEST_F(LayerTreeHostTestHighResRequiredAfterEvictingUIResources);

class LayerTreeHostTestGpuRasterizationDefault : public LayerTreeHostTest {
 protected:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->impl_side_painting = true;

    EXPECT_FALSE(settings->gpu_rasterization_enabled);
    EXPECT_FALSE(settings->gpu_rasterization_forced);
  }

  virtual void SetupTree() OVERRIDE {
    LayerTreeHostTest::SetupTree();

    scoped_refptr<PictureLayer> layer = PictureLayer::Create(&layer_client_);
    layer->SetBounds(gfx::Size(10, 10));
    layer->SetIsDrawable(true);
    layer_tree_host()->root_layer()->AddChild(layer);
  }

  virtual void BeginTest() OVERRIDE {
    Layer* root = layer_tree_host()->root_layer();
    PictureLayer* layer = static_cast<PictureLayer*>(root->child_at(0));
    PicturePile* pile = layer->GetPicturePileForTesting();

    // Verify default values.
    EXPECT_TRUE(root->IsSuitableForGpuRasterization());
    EXPECT_TRUE(layer->IsSuitableForGpuRasterization());
    EXPECT_TRUE(pile->is_suitable_for_gpu_rasterization());
    EXPECT_FALSE(layer_tree_host()->has_gpu_rasterization_trigger());
    EXPECT_FALSE(layer_tree_host()->UseGpuRasterization());

    // Setting gpu rasterization trigger does not enable gpu rasterization.
    layer_tree_host()->SetHasGpuRasterizationTrigger(true);
    EXPECT_TRUE(layer_tree_host()->has_gpu_rasterization_trigger());
    EXPECT_FALSE(layer_tree_host()->UseGpuRasterization());

    PostSetNeedsCommitToMainThread();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    EXPECT_FALSE(host_impl->pending_tree()->use_gpu_rasterization());
    EXPECT_FALSE(host_impl->use_gpu_rasterization());
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    EXPECT_FALSE(host_impl->active_tree()->use_gpu_rasterization());
    EXPECT_FALSE(host_impl->use_gpu_rasterization());
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

  FakeContentLayerClient layer_client_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestGpuRasterizationDefault);

class LayerTreeHostTestGpuRasterizationEnabled : public LayerTreeHostTest {
 protected:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->impl_side_painting = true;

    EXPECT_FALSE(settings->gpu_rasterization_enabled);
    settings->gpu_rasterization_enabled = true;
  }

  virtual void SetupTree() OVERRIDE {
    LayerTreeHostTest::SetupTree();

    scoped_refptr<PictureLayer> layer = PictureLayer::Create(&layer_client_);
    layer->SetBounds(gfx::Size(10, 10));
    layer->SetIsDrawable(true);
    layer_tree_host()->root_layer()->AddChild(layer);
  }

  virtual void BeginTest() OVERRIDE {
    Layer* root = layer_tree_host()->root_layer();
    PictureLayer* layer = static_cast<PictureLayer*>(root->child_at(0));
    PicturePile* pile = layer->GetPicturePileForTesting();

    // Verify default values.
    EXPECT_TRUE(root->IsSuitableForGpuRasterization());
    EXPECT_TRUE(layer->IsSuitableForGpuRasterization());
    EXPECT_TRUE(pile->is_suitable_for_gpu_rasterization());
    EXPECT_FALSE(layer_tree_host()->has_gpu_rasterization_trigger());
    EXPECT_FALSE(layer_tree_host()->UseGpuRasterization());

    // Gpu rasterization trigger is relevant.
    layer_tree_host()->SetHasGpuRasterizationTrigger(true);
    EXPECT_TRUE(layer_tree_host()->has_gpu_rasterization_trigger());
    EXPECT_TRUE(layer_tree_host()->UseGpuRasterization());

    // Content-based veto is relevant as well.
    pile->SetUnsuitableForGpuRasterizationForTesting();
    EXPECT_FALSE(pile->is_suitable_for_gpu_rasterization());
    EXPECT_FALSE(layer->IsSuitableForGpuRasterization());
    // Veto will take effect when layers are updated.
    // The results will be verified after commit is completed below.
    // Since we are manually marking picture pile as unsuitable,
    // make sure that the layer gets a chance to update.
    layer->SetNeedsDisplay();
    PostSetNeedsCommitToMainThread();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    EXPECT_FALSE(host_impl->pending_tree()->use_gpu_rasterization());
    EXPECT_FALSE(host_impl->use_gpu_rasterization());
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    EXPECT_FALSE(host_impl->active_tree()->use_gpu_rasterization());
    EXPECT_FALSE(host_impl->use_gpu_rasterization());
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

  FakeContentLayerClient layer_client_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestGpuRasterizationEnabled);

class LayerTreeHostTestGpuRasterizationForced : public LayerTreeHostTest {
 protected:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->impl_side_painting = true;

    EXPECT_FALSE(settings->gpu_rasterization_forced);
    settings->gpu_rasterization_forced = true;
  }

  virtual void SetupTree() OVERRIDE {
    LayerTreeHostTest::SetupTree();

    scoped_refptr<PictureLayer> layer = PictureLayer::Create(&layer_client_);
    layer->SetBounds(gfx::Size(10, 10));
    layer->SetIsDrawable(true);
    layer_tree_host()->root_layer()->AddChild(layer);
  }

  virtual void BeginTest() OVERRIDE {
    Layer* root = layer_tree_host()->root_layer();
    PictureLayer* layer = static_cast<PictureLayer*>(root->child_at(0));
    PicturePile* pile = layer->GetPicturePileForTesting();

    // Verify default values.
    EXPECT_TRUE(root->IsSuitableForGpuRasterization());
    EXPECT_TRUE(layer->IsSuitableForGpuRasterization());
    EXPECT_TRUE(pile->is_suitable_for_gpu_rasterization());
    EXPECT_FALSE(layer_tree_host()->has_gpu_rasterization_trigger());

    // With gpu rasterization forced, gpu rasterization trigger is irrelevant.
    EXPECT_TRUE(layer_tree_host()->UseGpuRasterization());
    layer_tree_host()->SetHasGpuRasterizationTrigger(true);
    EXPECT_TRUE(layer_tree_host()->has_gpu_rasterization_trigger());
    EXPECT_TRUE(layer_tree_host()->UseGpuRasterization());

    // Content-based veto is irrelevant as well.
    pile->SetUnsuitableForGpuRasterizationForTesting();
    EXPECT_FALSE(pile->is_suitable_for_gpu_rasterization());
    EXPECT_FALSE(layer->IsSuitableForGpuRasterization());
    // Veto will take effect when layers are updated.
    // The results will be verified after commit is completed below.
    // Since we are manually marking picture pile as unsuitable,
    // make sure that the layer gets a chance to update.
    layer->SetNeedsDisplay();
    PostSetNeedsCommitToMainThread();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    EXPECT_TRUE(host_impl->pending_tree()->use_gpu_rasterization());
    EXPECT_TRUE(host_impl->use_gpu_rasterization());
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    EXPECT_TRUE(host_impl->active_tree()->use_gpu_rasterization());
    EXPECT_TRUE(host_impl->use_gpu_rasterization());
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

  FakeContentLayerClient layer_client_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestGpuRasterizationForced);

class LayerTreeHostTestContinuousPainting : public LayerTreeHostTest {
 public:
  LayerTreeHostTestContinuousPainting()
      : num_commits_(0), num_draws_(0), bounds_(20, 20), child_layer_(NULL) {}

 protected:
  enum { kExpectedNumCommits = 10 };

  virtual void SetupTree() OVERRIDE {
    scoped_refptr<Layer> root_layer = Layer::Create();
    root_layer->SetBounds(bounds_);

    if (layer_tree_host()->settings().impl_side_painting) {
      picture_layer_ = FakePictureLayer::Create(&client_);
      child_layer_ = picture_layer_.get();
    } else {
      content_layer_ = ContentLayerWithUpdateTracking::Create(&client_);
      child_layer_ = content_layer_.get();
    }
    child_layer_->SetBounds(bounds_);
    child_layer_->SetIsDrawable(true);
    root_layer->AddChild(child_layer_);

    layer_tree_host()->SetRootLayer(root_layer);
    layer_tree_host()->SetViewportSize(bounds_);
    LayerTreeHostTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    // Wait 50x longer than expected.
    double milliseconds_per_frame =
        1000 / layer_tree_host()->settings().refresh_rate;
    EndTestAfterDelay(50 * kExpectedNumCommits * milliseconds_per_frame);
    MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(
            &LayerTreeHostTestContinuousPainting::EnableContinuousPainting,
            base::Unretained(this)));
  }

  virtual void Animate(base::TimeTicks monotonic_time) OVERRIDE {
    child_layer_->SetNeedsDisplay();
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_LE(kExpectedNumCommits, num_commits_);
    EXPECT_LE(kExpectedNumCommits, num_draws_);
    int update_count = content_layer_ ? content_layer_->PaintContentsCount()
                                      : picture_layer_->update_count();
    EXPECT_LE(kExpectedNumCommits, update_count);
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    if (++num_draws_ == kExpectedNumCommits)
      EndTest();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    ++num_commits_;
  }

 private:
  void EnableContinuousPainting() {
    LayerTreeDebugState debug_state = layer_tree_host()->debug_state();
    debug_state.continuous_painting = true;
    layer_tree_host()->SetDebugState(debug_state);
  }

  int num_commits_;
  int num_draws_;
  const gfx::Size bounds_;
  FakeContentLayerClient client_;
  scoped_refptr<ContentLayerWithUpdateTracking> content_layer_;
  scoped_refptr<FakePictureLayer> picture_layer_;
  Layer* child_layer_;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestContinuousPainting);

}  // namespace cc
