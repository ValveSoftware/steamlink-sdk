// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/compositor/blimp_compositor_manager.h"

#include "base/memory/ptr_util.h"
#include "cc/proto/compositor_message.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/gesture_detection/motion_event_generic.h"

using testing::_;
using testing::InSequence;
using testing::Sequence;

namespace blimp {
namespace client {

class MockRenderWidgetFeature : public RenderWidgetFeature {
 public:
  MOCK_METHOD3(SendCompositorMessage,
               void(const int,
                    const int,
                    const cc::proto::CompositorMessage&));
  MOCK_METHOD3(SendInputEvent, void(const int,
                                    const int,
                                    const blink::WebInputEvent&));
  MOCK_METHOD2(SetDelegate, void(int, RenderWidgetFeatureDelegate*));
  MOCK_METHOD1(RemoveDelegate, void(const int));
};

class MockBlimpCompositor : public BlimpCompositor {
 public :
  explicit MockBlimpCompositor(const int render_widget_id)
  : BlimpCompositor(render_widget_id, nullptr) {
  }

  MOCK_METHOD1(SetVisible, void(bool));
  MOCK_METHOD1(SetAcceleratedWidget, void(gfx::AcceleratedWidget));
  MOCK_METHOD0(ReleaseAcceleratedWidget, void());
  MOCK_METHOD1(OnTouchEvent, bool(const ui::MotionEvent& motion_event));

  void OnCompositorMessageReceived(
      std::unique_ptr<cc::proto::CompositorMessage> message) override {
    MockableOnCompositorMessageReceived(*message);
  }
  MOCK_METHOD1(MockableOnCompositorMessageReceived,
               void(const cc::proto::CompositorMessage&));
};

class BlimpCompositorManagerForTesting : public BlimpCompositorManager {
 public:
  explicit BlimpCompositorManagerForTesting(
      RenderWidgetFeature* render_widget_feature)
      : BlimpCompositorManager(render_widget_feature, nullptr) {}

  using BlimpCompositorManager::GetCompositor;

  std::unique_ptr<BlimpCompositor> CreateBlimpCompositor(
      int render_widget_id,
      BlimpCompositorClient* client) override {
    return base::WrapUnique(new MockBlimpCompositor(render_widget_id));
  }
};

class BlimpCompositorManagerTest : public testing::Test {
 public:
  void SetUp() override {
    EXPECT_CALL(render_widget_feature_, SetDelegate(_, _)).Times(1);
    EXPECT_CALL(render_widget_feature_, RemoveDelegate(_)).Times(1);

    compositor_manager_.reset(
        new BlimpCompositorManagerForTesting(&render_widget_feature_));
  }

  void TearDown() override {
    compositor_manager_.reset();
  }

  void SetUpCompositors() {
    delegate()->OnRenderWidgetCreated(1);
    delegate()->OnRenderWidgetCreated(2);

    mock_compositor1_ = static_cast<MockBlimpCompositor*>
      (compositor_manager_->GetCompositor(1));
    mock_compositor2_ = static_cast<MockBlimpCompositor*>
      (compositor_manager_->GetCompositor(2));

    EXPECT_NE(mock_compositor1_, nullptr);
    EXPECT_NE(mock_compositor2_, nullptr);

    EXPECT_EQ(mock_compositor1_->render_widget_id(), 1);
    EXPECT_EQ(mock_compositor2_->render_widget_id(), 2);
  }

  RenderWidgetFeature::RenderWidgetFeatureDelegate* delegate() const {
    DCHECK(compositor_manager_);
    return static_cast<RenderWidgetFeature::RenderWidgetFeatureDelegate*>
        (compositor_manager_.get());
  }

  std::unique_ptr<BlimpCompositorManagerForTesting> compositor_manager_;
  MockRenderWidgetFeature render_widget_feature_;
  MockBlimpCompositor* mock_compositor1_;
  MockBlimpCompositor* mock_compositor2_;
};

TEST_F(BlimpCompositorManagerTest, ForwardsMessagesToCorrectCompositor) {
  SetUpCompositors();

  // Ensure that the compositor messages for a render widget are forwarded to
  // the correct compositor.
  EXPECT_CALL(*mock_compositor1_,
              MockableOnCompositorMessageReceived(_)).Times(2);
  EXPECT_CALL(*mock_compositor2_,
              MockableOnCompositorMessageReceived(_)).Times(1);
  EXPECT_CALL(*mock_compositor1_,
                SetVisible(false)).Times(1);
  EXPECT_CALL(*mock_compositor1_,
                SetAcceleratedWidget(gfx::kNullAcceleratedWidget)).Times(1);

  delegate()->OnCompositorMessageReceived(
      1, base::WrapUnique(new cc::proto::CompositorMessage));
  delegate()->OnRenderWidgetInitialized(1);
  delegate()->OnCompositorMessageReceived(
      2, base::WrapUnique(new cc::proto::CompositorMessage));
  delegate()->OnCompositorMessageReceived(
      1, base::WrapUnique(new cc::proto::CompositorMessage));

  delegate()->OnRenderWidgetDeleted(1);
  EXPECT_EQ(compositor_manager_->GetCompositor(1), nullptr);
}

TEST_F(BlimpCompositorManagerTest, ForwardsViewEventsToCorrectCompositor) {
  InSequence sequence;
  SetUpCompositors();

  EXPECT_CALL(*mock_compositor1_, SetVisible(true));
  EXPECT_CALL(*mock_compositor1_, SetAcceleratedWidget(
      gfx::kNullAcceleratedWidget));
  EXPECT_CALL(*mock_compositor1_, OnTouchEvent(_));
  EXPECT_CALL(*mock_compositor1_, SetVisible(false));
  EXPECT_CALL(*mock_compositor1_, ReleaseAcceleratedWidget());

  EXPECT_CALL(*mock_compositor2_, SetVisible(true));
  EXPECT_CALL(*mock_compositor2_, SetAcceleratedWidget(
      gfx::kNullAcceleratedWidget));
  EXPECT_CALL(*mock_compositor2_, SetVisible(false));
  EXPECT_CALL(*mock_compositor2_, ReleaseAcceleratedWidget());

  // Make the compositor manager visible and give it the accelerated widget
  // while we don't have any render widget initialized.
  compositor_manager_->SetVisible(true);
  compositor_manager_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);

  // Initialize the first render widget. This should propagate the visibility,
  // the accelerated widget and the touch events to the corresponding
  // compositor.
  delegate()->OnRenderWidgetInitialized(1);
  compositor_manager_->OnTouchEvent(
      ui::MotionEventGeneric(ui::MotionEvent::Action::ACTION_NONE,
                             base::TimeTicks::Now(), ui::PointerProperties()));

  // Now initialize the second render widget. This should swap the compositors
  // and make the first one invisible and release the accelerated widget.
  delegate()->OnRenderWidgetInitialized(2);

  // Now make the compositor manager invisible and release the accelerated
  // widget from it. This should make the current compositor invisible and
  // release the widget.
  compositor_manager_->SetVisible(false);
  compositor_manager_->ReleaseAcceleratedWidget();

  // Destroy all the widgets. We should not be receiving any calls for the view
  // events forwarded after this.
  delegate()->OnRenderWidgetDeleted(1);
  delegate()->OnRenderWidgetDeleted(2);

  compositor_manager_->SetVisible(true);
}

}  // namespace client
}  // namespace blimp
