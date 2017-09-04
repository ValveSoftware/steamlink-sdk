// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_pointer_action.h"
#include "content/browser/renderer_host/input/synthetic_touch_pointer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebMouseEvent;
using blink::WebTouchPoint;

namespace content {

namespace {

class MockSyntheticPointerActionTarget : public SyntheticGestureTarget {
 public:
  MockSyntheticPointerActionTarget() {}
  ~MockSyntheticPointerActionTarget() override {}

  // SyntheticGestureTarget:
  void SetNeedsFlush() override { NOTIMPLEMENTED(); }

  base::TimeDelta PointerAssumedStoppedTime() const override {
    NOTIMPLEMENTED();
    return base::TimeDelta();
  }

  float GetTouchSlopInDips() const override {
    NOTIMPLEMENTED();
    return 0.0f;
  }

  float GetMinScalingSpanInDips() const override {
    NOTIMPLEMENTED();
    return 0.0f;
  }

  WebInputEvent::Type type() const { return type_; }

 protected:
  WebInputEvent::Type type_;
};

class MockSyntheticPointerTouchActionTarget
    : public MockSyntheticPointerActionTarget {
 public:
  MockSyntheticPointerTouchActionTarget() {}
  ~MockSyntheticPointerTouchActionTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    ASSERT_TRUE(WebInputEvent::isTouchEventType(event.type));
    const WebTouchEvent& touch_event = static_cast<const WebTouchEvent&>(event);
    type_ = touch_event.type;
    for (size_t i = 0; i < touch_event.touchesLength; ++i) {
      indexes_[i] = touch_event.touches[i].id;
      positions_[i] = gfx::PointF(touch_event.touches[i].position);
      states_[i] = touch_event.touches[i].state;
    }
    touch_length_ = touch_event.touchesLength;
  }

  SyntheticGestureParams::GestureSourceType
  GetDefaultSyntheticGestureSourceType() const override {
    return SyntheticGestureParams::TOUCH_INPUT;
  }

  gfx::PointF positions(int index) const { return positions_[index]; }
  int indexes(int index) const { return indexes_[index]; }
  WebTouchPoint::State states(int index) { return states_[index]; }
  unsigned touch_length() const { return touch_length_; }

 private:
  gfx::PointF positions_[WebTouchEvent::kTouchesLengthCap];
  unsigned touch_length_;
  int indexes_[WebTouchEvent::kTouchesLengthCap];
  WebTouchPoint::State states_[WebTouchEvent::kTouchesLengthCap];
};

class MockSyntheticPointerMouseActionTarget
    : public MockSyntheticPointerActionTarget {
 public:
  MockSyntheticPointerMouseActionTarget() {}
  ~MockSyntheticPointerMouseActionTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    ASSERT_TRUE(WebInputEvent::isMouseEventType(event.type));
    const WebMouseEvent& mouse_event = static_cast<const WebMouseEvent&>(event);
    type_ = mouse_event.type;
    position_ = gfx::PointF(mouse_event.x, mouse_event.y);
    clickCount_ = mouse_event.clickCount;
    button_ = mouse_event.button;
  }

  SyntheticGestureParams::GestureSourceType
  GetDefaultSyntheticGestureSourceType() const override {
    return SyntheticGestureParams::MOUSE_INPUT;
  }

  gfx::PointF position() const { return position_; }
  int clickCount() const { return clickCount_; }
  WebMouseEvent::Button button() const { return button_; }

 private:
  gfx::PointF position_;
  int clickCount_;
  WebMouseEvent::Button button_;
};

class SyntheticPointerActionTest : public testing::Test {
 public:
  SyntheticPointerActionTest() {
    action_param_list_.reset(new std::vector<SyntheticPointerActionParams>());
    std::fill(index_map_.begin(), index_map_.end(), -1);
    num_success_ = 0;
    num_failure_ = 0;
  }
  ~SyntheticPointerActionTest() override {}

 protected:
  template <typename MockGestureTarget>
  void CreateSyntheticPointerActionTarget() {
    target_.reset(new MockGestureTarget());
    synthetic_pointer_ = SyntheticPointer::Create(
        target_->GetDefaultSyntheticGestureSourceType());
  }

  void ForwardSyntheticPointerAction() {
    pointer_action_.reset(new SyntheticPointerAction(
        std::move(action_param_list_), synthetic_pointer_.get(), &index_map_));

    SyntheticGesture::Result result = pointer_action_->ForwardInputEvents(
        base::TimeTicks::Now(), target_.get());

    if (result == SyntheticGesture::GESTURE_FINISHED)
      num_success_++;
    else
      num_failure_++;
  }

  int num_success_;
  int num_failure_;
  std::unique_ptr<MockSyntheticPointerActionTarget> target_;
  std::unique_ptr<SyntheticGesture> pointer_action_;
  std::unique_ptr<SyntheticPointer> synthetic_pointer_;
  std::unique_ptr<std::vector<SyntheticPointerActionParams>> action_param_list_;
  SyntheticPointerAction::IndexMap index_map_;
};

TEST_F(SyntheticPointerActionTest, PointerTouchAction) {
  CreateSyntheticPointerActionTarget<MockSyntheticPointerTouchActionTarget>();

  // Send a touch press for one finger.
  SyntheticPointerActionParams params0 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  params0.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params0.set_index(0);
  params0.set_position(gfx::PointF(54, 89));
  action_param_list_->push_back(params0);
  ForwardSyntheticPointerAction();

  MockSyntheticPointerTouchActionTarget* pointer_touch_target =
      static_cast<MockSyntheticPointerTouchActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchStart);
  EXPECT_EQ(pointer_touch_target->indexes(0), 0);
  EXPECT_EQ(index_map_[0], 0);
  EXPECT_EQ(pointer_touch_target->positions(0), gfx::PointF(54, 89));
  EXPECT_EQ(pointer_touch_target->states(0), WebTouchPoint::StatePressed);
  ASSERT_EQ(pointer_touch_target->touch_length(), 1U);

  // Send a touch move for the first finger and a touch press for the second
  // finger.
  action_param_list_.reset(new std::vector<SyntheticPointerActionParams>());
  params0.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  params0.set_position(gfx::PointF(133, 156));
  SyntheticPointerActionParams params1 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  params1.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params1.set_index(1);
  params1.set_position(gfx::PointF(79, 132));
  action_param_list_->push_back(params0);
  action_param_list_->push_back(params1);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
  // The type of the SyntheticWebTouchEvent is the action of the last finger.
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchStart);
  EXPECT_EQ(pointer_touch_target->indexes(0), 0);
  EXPECT_EQ(index_map_[0], 0);
  EXPECT_EQ(pointer_touch_target->positions(0), gfx::PointF(133, 156));
  EXPECT_EQ(pointer_touch_target->states(0), WebTouchPoint::StateMoved);
  EXPECT_EQ(pointer_touch_target->indexes(1), 1);
  EXPECT_EQ(index_map_[1], 1);
  EXPECT_EQ(pointer_touch_target->positions(1), gfx::PointF(79, 132));
  EXPECT_EQ(pointer_touch_target->states(1), WebTouchPoint::StatePressed);
  ASSERT_EQ(pointer_touch_target->touch_length(), 2U);

  // Send a touch move for the second finger.
  action_param_list_.reset(new std::vector<SyntheticPointerActionParams>());
  params1.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  params1.set_position(gfx::PointF(87, 253));
  action_param_list_->push_back(params1);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(3, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchMove);
  EXPECT_EQ(pointer_touch_target->indexes(1), 1);
  EXPECT_EQ(index_map_[1], 1);
  EXPECT_EQ(pointer_touch_target->positions(1), gfx::PointF(87, 253));
  EXPECT_EQ(pointer_touch_target->states(1), WebTouchPoint::StateMoved);
  ASSERT_EQ(pointer_touch_target->touch_length(), 2U);

  // Send touch releases for both fingers.
  action_param_list_.reset(new std::vector<SyntheticPointerActionParams>());
  params0.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  params1.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  action_param_list_->push_back(params0);
  action_param_list_->push_back(params1);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(4, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchEnd);
  EXPECT_EQ(pointer_touch_target->indexes(0), 0);
  EXPECT_EQ(index_map_[0], -1);
  EXPECT_EQ(pointer_touch_target->states(0), WebTouchPoint::StateReleased);
  EXPECT_EQ(pointer_touch_target->indexes(1), 1);
  EXPECT_EQ(index_map_[1], -1);
  EXPECT_EQ(pointer_touch_target->states(1), WebTouchPoint::StateReleased);
  ASSERT_EQ(pointer_touch_target->touch_length(), 2U);
}

TEST_F(SyntheticPointerActionTest, PointerTouchActionIndexInvalid) {
  CreateSyntheticPointerActionTarget<MockSyntheticPointerTouchActionTarget>();

  // Users sent a wrong index for the touch action.
  SyntheticPointerActionParams params0 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  params0.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params0.set_index(-1);
  params0.set_position(gfx::PointF(54, 89));
  action_param_list_->push_back(params0);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(0, num_success_);
  EXPECT_EQ(1, num_failure_);

  action_param_list_.reset(new std::vector<SyntheticPointerActionParams>());
  params0.set_index(0);
  action_param_list_->push_back(params0);
  ForwardSyntheticPointerAction();

  MockSyntheticPointerTouchActionTarget* pointer_touch_target =
      static_cast<MockSyntheticPointerTouchActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(1, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchStart);
  EXPECT_EQ(pointer_touch_target->indexes(0), 0);
  EXPECT_EQ(index_map_[0], 0);
  EXPECT_EQ(pointer_touch_target->positions(0), gfx::PointF(54, 89));
  EXPECT_EQ(pointer_touch_target->states(0), WebTouchPoint::StatePressed);
  ASSERT_EQ(pointer_touch_target->touch_length(), 1U);
}

TEST_F(SyntheticPointerActionTest, PointerTouchActionSourceTypeInvalid) {
  CreateSyntheticPointerActionTarget<MockSyntheticPointerTouchActionTarget>();

  // Users' gesture source type does not match with the touch action.
  SyntheticPointerActionParams params0 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  params0.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params0.set_index(0);
  params0.set_position(gfx::PointF(54, 89));
  action_param_list_->push_back(params0);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(0, num_success_);
  EXPECT_EQ(1, num_failure_);

  action_param_list_.reset(new std::vector<SyntheticPointerActionParams>());
  params0.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  action_param_list_->push_back(params0);
  ForwardSyntheticPointerAction();

  MockSyntheticPointerTouchActionTarget* pointer_touch_target =
      static_cast<MockSyntheticPointerTouchActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(1, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchStart);
  EXPECT_EQ(pointer_touch_target->indexes(0), 0);
  EXPECT_EQ(index_map_[0], 0);
  EXPECT_EQ(pointer_touch_target->positions(0), gfx::PointF(54, 89));
  EXPECT_EQ(pointer_touch_target->states(0), WebTouchPoint::StatePressed);
  ASSERT_EQ(pointer_touch_target->touch_length(), 1U);
}

TEST_F(SyntheticPointerActionTest, PointerTouchActionTypeInvalid) {
  CreateSyntheticPointerActionTarget<MockSyntheticPointerTouchActionTarget>();

  // Cannot send a touch move or touch release without sending a touch press
  // first.
  SyntheticPointerActionParams params0 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  params0.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params0.set_index(0);
  params0.set_position(gfx::PointF(54, 89));
  action_param_list_->push_back(params0);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(0, num_success_);
  EXPECT_EQ(1, num_failure_);

  action_param_list_.reset(new std::vector<SyntheticPointerActionParams>());
  params0.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  action_param_list_->push_back(params0);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(0, num_success_);
  EXPECT_EQ(2, num_failure_);

  // Send a touch press for one finger.
  action_param_list_.reset(new std::vector<SyntheticPointerActionParams>());
  params0.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  action_param_list_->push_back(params0);
  ForwardSyntheticPointerAction();

  MockSyntheticPointerTouchActionTarget* pointer_touch_target =
      static_cast<MockSyntheticPointerTouchActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(2, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchStart);
  EXPECT_EQ(pointer_touch_target->indexes(0), 0);
  EXPECT_EQ(index_map_[0], 0);
  EXPECT_EQ(pointer_touch_target->positions(0), gfx::PointF(54, 89));
  EXPECT_EQ(pointer_touch_target->states(0), WebTouchPoint::StatePressed);
  ASSERT_EQ(pointer_touch_target->touch_length(), 1U);

  // Cannot send a touch press again without releasing the finger.
  action_param_list_.reset(new std::vector<SyntheticPointerActionParams>());
  params0.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params0.set_index(0);
  params0.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  action_param_list_->push_back(params0);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(3, num_failure_);
}

TEST_F(SyntheticPointerActionTest, PointerMouseAction) {
  CreateSyntheticPointerActionTarget<MockSyntheticPointerMouseActionTarget>();

  // Send a mouse move.
  SyntheticPointerActionParams params = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.set_index(0);
  params.set_position(gfx::PointF(189, 62));
  action_param_list_->push_back(params);
  ForwardSyntheticPointerAction();

  MockSyntheticPointerMouseActionTarget* pointer_mouse_target =
      static_cast<MockSyntheticPointerMouseActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_mouse_target->type(), WebInputEvent::MouseMove);
  EXPECT_EQ(pointer_mouse_target->position(), params.position());
  EXPECT_EQ(pointer_mouse_target->clickCount(), 0);
  EXPECT_EQ(pointer_mouse_target->button(), WebMouseEvent::Button::NoButton);

  // Send a mouse down.
  action_param_list_.reset(new std::vector<SyntheticPointerActionParams>());
  params.set_position(gfx::PointF(189, 62));
  params.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  action_param_list_->push_back(params);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_mouse_target->type(), WebInputEvent::MouseDown);
  EXPECT_EQ(pointer_mouse_target->position(), params.position());
  EXPECT_EQ(pointer_mouse_target->clickCount(), 1);
  EXPECT_EQ(pointer_mouse_target->button(), WebMouseEvent::Button::Left);

  // Send a mouse drag.
  action_param_list_.reset(new std::vector<SyntheticPointerActionParams>());
  params.set_position(gfx::PointF(326, 298));
  params.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  action_param_list_->push_back(params);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(3, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_mouse_target->type(), WebInputEvent::MouseMove);
  EXPECT_EQ(pointer_mouse_target->position(), params.position());
  EXPECT_EQ(pointer_mouse_target->clickCount(), 1);
  EXPECT_EQ(pointer_mouse_target->button(), WebMouseEvent::Button::Left);

  // Send a mouse up.
  action_param_list_.reset(new std::vector<SyntheticPointerActionParams>());
  params.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  action_param_list_->push_back(params);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(4, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_mouse_target->type(), WebInputEvent::MouseUp);
  EXPECT_EQ(pointer_mouse_target->clickCount(), 1);
  EXPECT_EQ(pointer_mouse_target->button(), WebMouseEvent::Button::Left);
}

TEST_F(SyntheticPointerActionTest, PointerMouseActionSourceTypeInvalid) {
  CreateSyntheticPointerActionTarget<MockSyntheticPointerMouseActionTarget>();

  // Users' gesture source type does not match with the mouse action.
  SyntheticPointerActionParams params = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.set_index(0);
  params.set_position(gfx::PointF(54, 89));
  action_param_list_->push_back(params);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(0, num_success_);
  EXPECT_EQ(1, num_failure_);

  action_param_list_.reset(new std::vector<SyntheticPointerActionParams>());
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  action_param_list_->push_back(params);
  ForwardSyntheticPointerAction();

  MockSyntheticPointerMouseActionTarget* pointer_mouse_target =
      static_cast<MockSyntheticPointerMouseActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(1, num_failure_);
  EXPECT_EQ(pointer_mouse_target->type(), WebInputEvent::MouseDown);
  EXPECT_EQ(pointer_mouse_target->position(), params.position());
  EXPECT_EQ(pointer_mouse_target->clickCount(), 1);
  EXPECT_EQ(pointer_mouse_target->button(), WebMouseEvent::Button::Left);
}

TEST_F(SyntheticPointerActionTest, PointerMouseActionTypeInvalid) {
  CreateSyntheticPointerActionTarget<MockSyntheticPointerMouseActionTarget>();

  // Send a mouse move.
  SyntheticPointerActionParams params = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.set_index(0);
  params.set_position(gfx::PointF(189, 62));
  action_param_list_->push_back(params);
  ForwardSyntheticPointerAction();

  MockSyntheticPointerMouseActionTarget* pointer_mouse_target =
      static_cast<MockSyntheticPointerMouseActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_mouse_target->type(), WebInputEvent::MouseMove);
  EXPECT_EQ(pointer_mouse_target->position(), params.position());
  EXPECT_EQ(pointer_mouse_target->clickCount(), 0);
  EXPECT_EQ(pointer_mouse_target->button(), WebMouseEvent::Button::NoButton);

  // Cannot send a mouse up without sending a mouse down first.
  action_param_list_.reset(new std::vector<SyntheticPointerActionParams>());
  params.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  action_param_list_->push_back(params);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(1, num_failure_);

  // Send a mouse down for one finger.
  action_param_list_.reset(new std::vector<SyntheticPointerActionParams>());
  params.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  action_param_list_->push_back(params);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(1, num_failure_);
  EXPECT_EQ(pointer_mouse_target->type(), WebInputEvent::MouseDown);
  EXPECT_EQ(pointer_mouse_target->position(), params.position());
  EXPECT_EQ(pointer_mouse_target->clickCount(), 1);
  EXPECT_EQ(pointer_mouse_target->button(), WebMouseEvent::Button::Left);

  // Cannot send a mouse down again without releasing the mouse button.
  action_param_list_.reset(new std::vector<SyntheticPointerActionParams>());
  params.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  action_param_list_->push_back(params);
  ForwardSyntheticPointerAction();

  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(2, num_failure_);
}

}  // namespace

}  // namespace content
