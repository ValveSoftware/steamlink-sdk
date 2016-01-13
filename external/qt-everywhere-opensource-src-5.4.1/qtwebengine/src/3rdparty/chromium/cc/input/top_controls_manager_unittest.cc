// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/top_controls_manager.h"

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "cc/input/top_controls_manager_client.h"
#include "cc/layers/layer_impl.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/frame_time.h"
#include "ui/gfx/vector2d_f.h"

namespace cc {
namespace {

static const float kTopControlsHeight = 100;

class MockTopControlsManagerClient : public TopControlsManagerClient {
 public:
  MockTopControlsManagerClient(float top_controls_show_threshold,
                               float top_controls_hide_threshold)
      : host_impl_(&proxy_, &shared_bitmap_manager_),
        redraw_needed_(false),
        update_draw_properties_needed_(false),
        top_controls_show_threshold_(top_controls_show_threshold),
        top_controls_hide_threshold_(top_controls_hide_threshold) {
    active_tree_ = LayerTreeImpl::create(&host_impl_);
    root_scroll_layer_ = LayerImpl::Create(active_tree_.get(), 1);
  }

  virtual ~MockTopControlsManagerClient() {}

  virtual void DidChangeTopControlsPosition() OVERRIDE {
    redraw_needed_ = true;
    update_draw_properties_needed_ = true;
  }

  virtual bool HaveRootScrollLayer() const OVERRIDE {
    return true;
  }

  LayerImpl* rootScrollLayer() {
    return root_scroll_layer_.get();
  }

  TopControlsManager* manager() {
    if (!manager_) {
      manager_ = TopControlsManager::Create(this,
                                            kTopControlsHeight,
                                            top_controls_show_threshold_,
                                            top_controls_hide_threshold_);
    }
    return manager_.get();
  }

 private:
  FakeImplProxy proxy_;
  TestSharedBitmapManager shared_bitmap_manager_;
  FakeLayerTreeHostImpl host_impl_;
  scoped_ptr<LayerTreeImpl> active_tree_;
  scoped_ptr<LayerImpl> root_scroll_layer_;
  scoped_ptr<TopControlsManager> manager_;
  bool redraw_needed_;
  bool update_draw_properties_needed_;

  float top_controls_show_threshold_;
  float top_controls_hide_threshold_;
};

TEST(TopControlsManagerTest, EnsureScrollThresholdApplied) {
  MockTopControlsManagerClient client(0.5f, 0.5f);
  TopControlsManager* manager = client.manager();

  manager->ScrollBegin();

  // Scroll down to hide the controls entirely.
  manager->ScrollBy(gfx::Vector2dF(0.f, 30.f));
  EXPECT_EQ(-30.f, manager->controls_top_offset());

  manager->ScrollBy(gfx::Vector2dF(0.f, 30.f));
  EXPECT_EQ(-60.f, manager->controls_top_offset());

  manager->ScrollBy(gfx::Vector2dF(0.f, 100.f));
  EXPECT_EQ(-100.f, manager->controls_top_offset());

  // Scroll back up a bit and ensure the controls don't move until we cross
  // the threshold.
  manager->ScrollBy(gfx::Vector2dF(0.f, -10.f));
  EXPECT_EQ(-100.f, manager->controls_top_offset());

  manager->ScrollBy(gfx::Vector2dF(0.f, -50.f));
  EXPECT_EQ(-100.f, manager->controls_top_offset());

  // After hitting the threshold, further scrolling up should result in the top
  // controls showing.
  manager->ScrollBy(gfx::Vector2dF(0.f, -10.f));
  EXPECT_EQ(-90.f, manager->controls_top_offset());

  manager->ScrollBy(gfx::Vector2dF(0.f, -50.f));
  EXPECT_EQ(-40.f, manager->controls_top_offset());

  // Reset the scroll threshold by going further up the page than the initial
  // threshold.
  manager->ScrollBy(gfx::Vector2dF(0.f, -100.f));
  EXPECT_EQ(0.f, manager->controls_top_offset());

  // See that scrolling down the page now will result in the controls hiding.
  manager->ScrollBy(gfx::Vector2dF(0.f, 20.f));
  EXPECT_EQ(-20.f, manager->controls_top_offset());

  manager->ScrollEnd();
}

TEST(TopControlsManagerTest, PartialShownHideAnimation) {
  MockTopControlsManagerClient client(0.5f, 0.5f);
  TopControlsManager* manager = client.manager();
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, 300.f));
  EXPECT_EQ(-100.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
  manager->ScrollEnd();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, -15.f));
  EXPECT_EQ(-85.f, manager->controls_top_offset());
  EXPECT_EQ(15.f, manager->content_top_offset());
  manager->ScrollEnd();

  EXPECT_TRUE(manager->animation());

  base::TimeTicks time = gfx::FrameTime::Now();
  float previous_offset = manager->controls_top_offset();
  while (manager->animation()) {
    time = base::TimeDelta::FromMicroseconds(100) + time;
    manager->Animate(time);
    EXPECT_LT(manager->controls_top_offset(), previous_offset);
    previous_offset = manager->controls_top_offset();
  }
  EXPECT_FALSE(manager->animation());
  EXPECT_EQ(-100.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
}

TEST(TopControlsManagerTest, PartialShownShowAnimation) {
  MockTopControlsManagerClient client(0.5f, 0.5f);
  TopControlsManager* manager = client.manager();
  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, 300.f));
  EXPECT_EQ(-100.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
  manager->ScrollEnd();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, -70.f));
  EXPECT_EQ(-30.f, manager->controls_top_offset());
  EXPECT_EQ(70.f, manager->content_top_offset());
  manager->ScrollEnd();

  EXPECT_TRUE(manager->animation());

  base::TimeTicks time = gfx::FrameTime::Now();
  float previous_offset = manager->controls_top_offset();
  while (manager->animation()) {
    time = base::TimeDelta::FromMicroseconds(100) + time;
    manager->Animate(time);
    EXPECT_GT(manager->controls_top_offset(), previous_offset);
    previous_offset = manager->controls_top_offset();
  }
  EXPECT_FALSE(manager->animation());
  EXPECT_EQ(0.f, manager->controls_top_offset());
  EXPECT_EQ(100.f, manager->content_top_offset());
}

TEST(TopControlsManagerTest, PartialHiddenWithAmbiguousThresholdShows) {
  MockTopControlsManagerClient client(0.25f, 0.25f);
  TopControlsManager* manager = client.manager();

  manager->ScrollBegin();

  manager->ScrollBy(gfx::Vector2dF(0.f, 20.f));
  EXPECT_EQ(-20.f, manager->controls_top_offset());
  EXPECT_EQ(80.f, manager->content_top_offset());

  manager->ScrollEnd();
  EXPECT_TRUE(manager->animation());

  base::TimeTicks time = gfx::FrameTime::Now();
  float previous_offset = manager->controls_top_offset();
  while (manager->animation()) {
    time = base::TimeDelta::FromMicroseconds(100) + time;
    manager->Animate(time);
    EXPECT_GT(manager->controls_top_offset(), previous_offset);
    previous_offset = manager->controls_top_offset();
  }
  EXPECT_FALSE(manager->animation());
  EXPECT_EQ(0.f, manager->controls_top_offset());
  EXPECT_EQ(100.f, manager->content_top_offset());
}

TEST(TopControlsManagerTest, PartialHiddenWithAmbiguousThresholdHides) {
  MockTopControlsManagerClient client(0.25f, 0.25f);
  TopControlsManager* manager = client.manager();

  manager->ScrollBegin();

  manager->ScrollBy(gfx::Vector2dF(0.f, 30.f));
  EXPECT_EQ(-30.f, manager->controls_top_offset());
  EXPECT_EQ(70.f, manager->content_top_offset());

  manager->ScrollEnd();
  EXPECT_TRUE(manager->animation());

  base::TimeTicks time = gfx::FrameTime::Now();
  float previous_offset = manager->controls_top_offset();
  while (manager->animation()) {
    time = base::TimeDelta::FromMicroseconds(100) + time;
    manager->Animate(time);
    EXPECT_LT(manager->controls_top_offset(), previous_offset);
    previous_offset = manager->controls_top_offset();
  }
  EXPECT_FALSE(manager->animation());
  EXPECT_EQ(-100.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
}

TEST(TopControlsManagerTest, PartialShownWithAmbiguousThresholdHides) {
  MockTopControlsManagerClient client(0.25f, 0.25f);
  TopControlsManager* manager = client.manager();

  manager->ScrollBy(gfx::Vector2dF(0.f, 200.f));
  EXPECT_EQ(-100.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());

  manager->ScrollBegin();

  manager->ScrollBy(gfx::Vector2dF(0.f, -20.f));
  EXPECT_EQ(-80.f, manager->controls_top_offset());
  EXPECT_EQ(20.f, manager->content_top_offset());

  manager->ScrollEnd();
  EXPECT_TRUE(manager->animation());

  base::TimeTicks time = gfx::FrameTime::Now();
  float previous_offset = manager->controls_top_offset();
  while (manager->animation()) {
    time = base::TimeDelta::FromMicroseconds(100) + time;
    manager->Animate(time);
    EXPECT_LT(manager->controls_top_offset(), previous_offset);
    previous_offset = manager->controls_top_offset();
  }
  EXPECT_FALSE(manager->animation());
  EXPECT_EQ(-100.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
}

TEST(TopControlsManagerTest, PartialShownWithAmbiguousThresholdShows) {
  MockTopControlsManagerClient client(0.25f, 0.25f);
  TopControlsManager* manager = client.manager();

  manager->ScrollBy(gfx::Vector2dF(0.f, 200.f));
  EXPECT_EQ(-100.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());

  manager->ScrollBegin();

  manager->ScrollBy(gfx::Vector2dF(0.f, -30.f));
  EXPECT_EQ(-70.f, manager->controls_top_offset());
  EXPECT_EQ(30.f, manager->content_top_offset());

  manager->ScrollEnd();
  EXPECT_TRUE(manager->animation());

  base::TimeTicks time = gfx::FrameTime::Now();
  float previous_offset = manager->controls_top_offset();
  while (manager->animation()) {
    time = base::TimeDelta::FromMicroseconds(100) + time;
    manager->Animate(time);
    EXPECT_GT(manager->controls_top_offset(), previous_offset);
    previous_offset = manager->controls_top_offset();
  }
  EXPECT_FALSE(manager->animation());
  EXPECT_EQ(0.f, manager->controls_top_offset());
  EXPECT_EQ(100.f, manager->content_top_offset());
}

TEST(TopControlsManagerTest, PinchIgnoresScroll) {
  MockTopControlsManagerClient client(0.5f, 0.5f);
  TopControlsManager* manager = client.manager();

  // Hide the controls.
  manager->ScrollBegin();
  EXPECT_EQ(0.f, manager->controls_top_offset());

  manager->ScrollBy(gfx::Vector2dF(0.f, 300.f));
  EXPECT_EQ(-100.f, manager->controls_top_offset());

  manager->PinchBegin();
  EXPECT_EQ(-100.f, manager->controls_top_offset());

  // Scrolls are ignored during pinch.
  manager->ScrollBy(gfx::Vector2dF(0.f, -15.f));
  EXPECT_EQ(-100.f, manager->controls_top_offset());
  manager->PinchEnd();
  EXPECT_EQ(-100.f, manager->controls_top_offset());

  // Scrolls should no long be ignored.
  manager->ScrollBy(gfx::Vector2dF(0.f, -15.f));
  EXPECT_EQ(-85.f, manager->controls_top_offset());
  EXPECT_EQ(15.f, manager->content_top_offset());
  manager->ScrollEnd();

  EXPECT_TRUE(manager->animation());
}

TEST(TopControlsManagerTest, PinchBeginStartsAnimationIfNecessary) {
  MockTopControlsManagerClient client(0.5f, 0.5f);
  TopControlsManager* manager = client.manager();

  manager->ScrollBegin();
  manager->ScrollBy(gfx::Vector2dF(0.f, 300.f));
  EXPECT_EQ(-100.f, manager->controls_top_offset());

  manager->PinchBegin();
  EXPECT_FALSE(manager->animation());

  manager->PinchEnd();
  EXPECT_FALSE(manager->animation());

  manager->ScrollBy(gfx::Vector2dF(0.f, -15.f));
  EXPECT_EQ(-85.f, manager->controls_top_offset());
  EXPECT_EQ(15.f, manager->content_top_offset());

  manager->PinchBegin();
  EXPECT_TRUE(manager->animation());

  base::TimeTicks time = base::TimeTicks::Now();
  float previous_offset = manager->controls_top_offset();
  while (manager->animation()) {
    time = base::TimeDelta::FromMicroseconds(100) + time;
    manager->Animate(time);
    EXPECT_LT(manager->controls_top_offset(), previous_offset);
    previous_offset = manager->controls_top_offset();
  }
  EXPECT_FALSE(manager->animation());

  manager->PinchEnd();
  EXPECT_FALSE(manager->animation());

  manager->ScrollBy(gfx::Vector2dF(0.f, -55.f));
  EXPECT_EQ(-45.f, manager->controls_top_offset());
  EXPECT_EQ(55.f, manager->content_top_offset());
  EXPECT_FALSE(manager->animation());

  manager->ScrollEnd();
  EXPECT_TRUE(manager->animation());

  time = base::TimeTicks::Now();
  previous_offset = manager->controls_top_offset();
  while (manager->animation()) {
    time = base::TimeDelta::FromMicroseconds(100) + time;
    manager->Animate(time);
    EXPECT_GT(manager->controls_top_offset(), previous_offset);
    previous_offset = manager->controls_top_offset();
  }
  EXPECT_FALSE(manager->animation());
  EXPECT_EQ(0.f, manager->controls_top_offset());
}

}  // namespace
}  // namespace cc
