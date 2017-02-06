// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_gesture_controller.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_pinch_gesture.h"
#include "content/browser/renderer_host/input/synthetic_pointer_action.h"
#include "content/browser/renderer_host/input/synthetic_smooth_drag_gesture.h"
#include "content/browser/renderer_host/input/synthetic_smooth_move_gesture.h"
#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture.h"
#include "content/browser/renderer_host/input/synthetic_tap_gesture.h"
#include "content/browser/renderer_host/input/synthetic_touch_pointer.h"
#include "content/browser/renderer_host/input/synthetic_touchpad_pinch_gesture.h"
#include "content/browser/renderer_host/input/synthetic_touchscreen_pinch_gesture.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input/synthetic_smooth_drag_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input/synthetic_tap_gesture_params.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"

using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

namespace {

const int kFlushInputRateInMs = 16;
const int kPointerAssumedStoppedTimeMs = 43;
const float kTouchSlopInDips = 7.0f;
const float kMinScalingSpanInDips = 27.5f;
const int kTouchPointersLength = 16;

enum TouchGestureType { TOUCH_SCROLL, TOUCH_DRAG };

class MockSyntheticGesture : public SyntheticGesture {
 public:
  MockSyntheticGesture(bool* finished, int num_steps)
      : finished_(finished),
        num_steps_(num_steps),
        step_count_(0) {
    *finished_ = false;
  }
  ~MockSyntheticGesture() override {}

  Result ForwardInputEvents(const base::TimeTicks& timestamp,
                            SyntheticGestureTarget* target) override {
    step_count_++;
    if (step_count_ == num_steps_) {
      *finished_ = true;
      return SyntheticGesture::GESTURE_FINISHED;
    } else if (step_count_ > num_steps_) {
      *finished_ = true;
      // Return arbitrary failure.
      return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;
    }

    return SyntheticGesture::GESTURE_RUNNING;
  }

 protected:
  bool* finished_;
  int num_steps_;
  int step_count_;
};

class MockSyntheticGestureTarget : public SyntheticGestureTarget {
 public:
  MockSyntheticGestureTarget()
      : flush_requested_(false),
        pointer_assumed_stopped_time_ms_(kPointerAssumedStoppedTimeMs) {}
  ~MockSyntheticGestureTarget() override {}

  // SyntheticGestureTarget:
  void DispatchInputEventToPlatform(const WebInputEvent& event) override {}

  void SetNeedsFlush() override { flush_requested_ = true; }

  SyntheticGestureParams::GestureSourceType
  GetDefaultSyntheticGestureSourceType() const override {
    return SyntheticGestureParams::TOUCH_INPUT;
  }

  base::TimeDelta PointerAssumedStoppedTime() const override {
    return base::TimeDelta::FromMilliseconds(pointer_assumed_stopped_time_ms_);
  }

  void set_pointer_assumed_stopped_time_ms(int time_ms) {
    pointer_assumed_stopped_time_ms_ = time_ms;
  }

  float GetTouchSlopInDips() const override { return kTouchSlopInDips; }

  float GetMinScalingSpanInDips() const override {
    return kMinScalingSpanInDips;
  }

  bool flush_requested() const { return flush_requested_; }
  void ClearFlushRequest() { flush_requested_ = false; }

 private:
  bool flush_requested_;

  int pointer_assumed_stopped_time_ms_;
};

class MockMoveGestureTarget : public MockSyntheticGestureTarget {
 public:
  MockMoveGestureTarget() : total_abs_move_distance_length_(0) {}
  ~MockMoveGestureTarget() override {}

  gfx::Vector2dF start_to_end_distance() const {
    return start_to_end_distance_;
  }
  float total_abs_move_distance_length() const {
    return total_abs_move_distance_length_;
  }

 protected:
  gfx::Vector2dF start_to_end_distance_;
  float total_abs_move_distance_length_;
};

class MockScrollMouseTarget : public MockMoveGestureTarget {
 public:
  MockScrollMouseTarget() {}
  ~MockScrollMouseTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    ASSERT_EQ(event.type, WebInputEvent::MouseWheel);
    const WebMouseWheelEvent& mouse_wheel_event =
        static_cast<const WebMouseWheelEvent&>(event);
    gfx::Vector2dF delta(mouse_wheel_event.deltaX, mouse_wheel_event.deltaY);
    start_to_end_distance_ += delta;
    total_abs_move_distance_length_ += delta.Length();
  }
};

class MockMoveTouchTarget : public MockMoveGestureTarget {
 public:
  MockMoveTouchTarget() : started_(false) {}
  ~MockMoveTouchTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    ASSERT_TRUE(WebInputEvent::isTouchEventType(event.type));
    const WebTouchEvent& touch_event = static_cast<const WebTouchEvent&>(event);
    ASSERT_EQ(touch_event.touchesLength, 1U);

    if (!started_) {
      ASSERT_EQ(touch_event.type, WebInputEvent::TouchStart);
      start_.SetPoint(touch_event.touches[0].position.x,
                      touch_event.touches[0].position.y);
      last_touch_point_ = gfx::PointF(start_);
      started_ = true;
    } else {
      ASSERT_NE(touch_event.type, WebInputEvent::TouchStart);
      ASSERT_NE(touch_event.type, WebInputEvent::TouchCancel);

      gfx::PointF touch_point(touch_event.touches[0].position.x,
                              touch_event.touches[0].position.y);
      gfx::Vector2dF delta = touch_point - last_touch_point_;
      total_abs_move_distance_length_ += delta.Length();

      if (touch_event.type == WebInputEvent::TouchEnd)
        start_to_end_distance_ = touch_point - gfx::PointF(start_);

      last_touch_point_ = touch_point;
    }
  }

 protected:
  gfx::Point start_;
  gfx::PointF last_touch_point_;
  bool started_;
};

class MockDragMouseTarget : public MockMoveGestureTarget {
 public:
  MockDragMouseTarget() : started_(false) {}
  ~MockDragMouseTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    ASSERT_TRUE(WebInputEvent::isMouseEventType(event.type));
    const WebMouseEvent& mouse_event = static_cast<const WebMouseEvent&>(event);
    if (!started_) {
      EXPECT_EQ(mouse_event.button, WebMouseEvent::ButtonLeft);
      EXPECT_EQ(mouse_event.clickCount, 1);
      EXPECT_EQ(mouse_event.type, WebInputEvent::MouseDown);
      start_.SetPoint(mouse_event.x, mouse_event.y);
      last_mouse_point_ = start_;
      started_ = true;
    } else {
      EXPECT_EQ(mouse_event.button, WebMouseEvent::ButtonLeft);
      ASSERT_NE(mouse_event.type, WebInputEvent::MouseDown);

      gfx::PointF mouse_point(mouse_event.x, mouse_event.y);
      gfx::Vector2dF delta = mouse_point - last_mouse_point_;
      total_abs_move_distance_length_ += delta.Length();
      if (mouse_event.type == WebInputEvent::MouseUp)
        start_to_end_distance_ = mouse_point - start_;
      last_mouse_point_ = mouse_point;
    }
  }

 private:
  bool started_;
  gfx::PointF start_, last_mouse_point_;
};

class MockSyntheticTouchscreenPinchTouchTarget
    : public MockSyntheticGestureTarget {
 public:
  enum ZoomDirection {
    ZOOM_DIRECTION_UNKNOWN,
    ZOOM_IN,
    ZOOM_OUT
  };

  MockSyntheticTouchscreenPinchTouchTarget()
      : initial_pointer_distance_(0),
        last_pointer_distance_(0),
        zoom_direction_(ZOOM_DIRECTION_UNKNOWN),
        started_(false) {}
  ~MockSyntheticTouchscreenPinchTouchTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    ASSERT_TRUE(WebInputEvent::isTouchEventType(event.type));
    const WebTouchEvent& touch_event = static_cast<const WebTouchEvent&>(event);
    ASSERT_EQ(touch_event.touchesLength, 2U);

    if (!started_) {
      ASSERT_EQ(touch_event.type, WebInputEvent::TouchStart);

      start_0_ = gfx::PointF(touch_event.touches[0].position);
      start_1_ = gfx::PointF(touch_event.touches[1].position);
      last_pointer_distance_ = (start_0_ - start_1_).Length();
      initial_pointer_distance_ = last_pointer_distance_;
      EXPECT_GE(initial_pointer_distance_, GetMinScalingSpanInDips());

      started_ = true;
    } else {
      ASSERT_NE(touch_event.type, WebInputEvent::TouchStart);
      ASSERT_NE(touch_event.type, WebInputEvent::TouchCancel);

      gfx::PointF current_0 = gfx::PointF(touch_event.touches[0].position);
      gfx::PointF current_1 = gfx::PointF(touch_event.touches[1].position);

      float pointer_distance = (current_0 - current_1).Length();

      if (last_pointer_distance_ != pointer_distance) {
        if (zoom_direction_ == ZOOM_DIRECTION_UNKNOWN)
          zoom_direction_ =
              ComputeZoomDirection(last_pointer_distance_, pointer_distance);
        else
          EXPECT_EQ(
              zoom_direction_,
              ComputeZoomDirection(last_pointer_distance_, pointer_distance));
      }

      last_pointer_distance_ = pointer_distance;
    }
  }

  SyntheticGestureParams::GestureSourceType
  GetDefaultSyntheticGestureSourceType() const override {
    return SyntheticGestureParams::TOUCH_INPUT;
  }

  ZoomDirection zoom_direction() const { return zoom_direction_; }

  float ComputeScaleFactor() const {
    switch (zoom_direction_) {
      case ZOOM_IN:
        return last_pointer_distance_ /
               (initial_pointer_distance_ + 2 * GetTouchSlopInDips());
      case ZOOM_OUT:
        return last_pointer_distance_ /
               (initial_pointer_distance_ - 2 * GetTouchSlopInDips());
      case ZOOM_DIRECTION_UNKNOWN:
        return 1.0f;
      default:
        NOTREACHED();
        return 0.0f;
    }
  }

 private:
  ZoomDirection ComputeZoomDirection(float last_pointer_distance,
                                     float current_pointer_distance) {
    DCHECK_NE(last_pointer_distance, current_pointer_distance);
    return last_pointer_distance < current_pointer_distance ? ZOOM_IN
                                                            : ZOOM_OUT;
  }

  float initial_pointer_distance_;
  float last_pointer_distance_;
  ZoomDirection zoom_direction_;
  gfx::PointF start_0_;
  gfx::PointF start_1_;
  bool started_;
};

class MockSyntheticTouchpadPinchTouchTarget
    : public MockSyntheticGestureTarget {
 public:
  enum ZoomDirection { ZOOM_DIRECTION_UNKNOWN, ZOOM_IN, ZOOM_OUT };

  MockSyntheticTouchpadPinchTouchTarget()
      : zoom_direction_(ZOOM_DIRECTION_UNKNOWN),
        started_(false),
        ended_(false),
        scale_factor_(1.0f) {}
  ~MockSyntheticTouchpadPinchTouchTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    EXPECT_TRUE(WebInputEvent::isGestureEventType(event.type));
    const blink::WebGestureEvent& gesture_event =
        static_cast<const blink::WebGestureEvent&>(event);

    if (gesture_event.type == WebInputEvent::GesturePinchBegin) {
      EXPECT_FALSE(started_);
      EXPECT_FALSE(ended_);
      started_ = true;
    } else if (gesture_event.type == WebInputEvent::GesturePinchEnd) {
      EXPECT_TRUE(started_);
      EXPECT_FALSE(ended_);
      ended_ = true;
    } else {
      EXPECT_EQ(WebInputEvent::GesturePinchUpdate, gesture_event.type);
      EXPECT_TRUE(started_);
      EXPECT_FALSE(ended_);
      const float scale = gesture_event.data.pinchUpdate.scale;
      if (scale != 1.0f) {
        if (zoom_direction_ == ZOOM_DIRECTION_UNKNOWN) {
          zoom_direction_ = scale > 1.0f ? ZOOM_IN : ZOOM_OUT;
        } else if (zoom_direction_ == ZOOM_IN) {
          EXPECT_GT(scale, 1.0f);
        } else {
          EXPECT_EQ(ZOOM_OUT, zoom_direction_);
          EXPECT_LT(scale, 1.0f);
        }

        scale_factor_ *= scale;
      }
    }
  }

  SyntheticGestureParams::GestureSourceType
  GetDefaultSyntheticGestureSourceType() const override {
    return SyntheticGestureParams::MOUSE_INPUT;
  }

  ZoomDirection zoom_direction() const { return zoom_direction_; }

  float scale_factor() const { return scale_factor_; }

 private:
  ZoomDirection zoom_direction_;
  bool started_;
  bool ended_;
  float scale_factor_;
};

class MockSyntheticTapGestureTarget : public MockSyntheticGestureTarget {
 public:
  MockSyntheticTapGestureTarget() : state_(NOT_STARTED) {}
  ~MockSyntheticTapGestureTarget() override {}

  bool GestureFinished() const { return state_ == FINISHED; }
  gfx::PointF position() const { return position_; }
  base::TimeDelta GetDuration() const { return stop_time_ - start_time_; }

 protected:
  enum GestureState {
    NOT_STARTED,
    STARTED,
    FINISHED
  };

  gfx::PointF position_;
  base::TimeDelta start_time_;
  base::TimeDelta stop_time_;
  GestureState state_;
};

class MockSyntheticTapTouchTarget : public MockSyntheticTapGestureTarget {
 public:
  MockSyntheticTapTouchTarget() {}
  ~MockSyntheticTapTouchTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    ASSERT_TRUE(WebInputEvent::isTouchEventType(event.type));
    const WebTouchEvent& touch_event = static_cast<const WebTouchEvent&>(event);
    ASSERT_EQ(touch_event.touchesLength, 1U);

    switch (state_) {
      case NOT_STARTED:
        EXPECT_EQ(touch_event.type, WebInputEvent::TouchStart);
        position_ = gfx::PointF(touch_event.touches[0].position);
        start_time_ = base::TimeDelta::FromMilliseconds(
            static_cast<int64_t>(touch_event.timeStampSeconds * 1000));
        state_ = STARTED;
        break;
      case STARTED:
        EXPECT_EQ(touch_event.type, WebInputEvent::TouchEnd);
        EXPECT_EQ(position_, gfx::PointF(touch_event.touches[0].position));
        stop_time_ = base::TimeDelta::FromMilliseconds(
            static_cast<int64_t>(touch_event.timeStampSeconds * 1000));
        state_ = FINISHED;
        break;
      case FINISHED:
        EXPECT_FALSE(true);
        break;
    }
  }
};

class MockSyntheticTapMouseTarget : public MockSyntheticTapGestureTarget {
 public:
  MockSyntheticTapMouseTarget() {}
  ~MockSyntheticTapMouseTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    ASSERT_TRUE(WebInputEvent::isMouseEventType(event.type));
    const WebMouseEvent& mouse_event = static_cast<const WebMouseEvent&>(event);

    switch (state_) {
      case NOT_STARTED:
        EXPECT_EQ(mouse_event.type, WebInputEvent::MouseDown);
        EXPECT_EQ(mouse_event.button, WebMouseEvent::ButtonLeft);
        EXPECT_EQ(mouse_event.clickCount, 1);
        position_ = gfx::PointF(mouse_event.x, mouse_event.y);
        start_time_ = base::TimeDelta::FromMilliseconds(
            static_cast<int64_t>(mouse_event.timeStampSeconds * 1000));
        state_ = STARTED;
        break;
      case STARTED:
        EXPECT_EQ(mouse_event.type, WebInputEvent::MouseUp);
        EXPECT_EQ(mouse_event.button, WebMouseEvent::ButtonLeft);
        EXPECT_EQ(mouse_event.clickCount, 1);
        EXPECT_EQ(position_, gfx::PointF(mouse_event.x, mouse_event.y));
        stop_time_ = base::TimeDelta::FromMilliseconds(
            static_cast<int64_t>(mouse_event.timeStampSeconds * 1000));
        state_ = FINISHED;
        break;
      case FINISHED:
        EXPECT_FALSE(true);
        break;
    }
  }
};

class MockSyntheticPointerActionTarget : public MockSyntheticGestureTarget {
 public:
  MockSyntheticPointerActionTarget() {}
  ~MockSyntheticPointerActionTarget() override {}

  gfx::PointF positions(int index) const { return positions_[index]; }
  int indexes(int index) const { return indexes_[index]; }
  WebTouchPoint::State states(int index) { return states_[index]; }
  unsigned touch_length() const { return touch_length_; }
  WebInputEvent::Type type() const { return type_; }

 protected:
  gfx::PointF positions_[kTouchPointersLength];
  unsigned touch_length_;
  int indexes_[kTouchPointersLength];
  WebTouchPoint::State states_[kTouchPointersLength];
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
};

class SyntheticGestureControllerTestBase {
 public:
  SyntheticGestureControllerTestBase() {}
  ~SyntheticGestureControllerTestBase() {}

 protected:
  template<typename MockGestureTarget>
  void CreateControllerAndTarget() {
    target_ = new MockGestureTarget();
    controller_.reset(new SyntheticGestureController(
        std::unique_ptr<SyntheticGestureTarget>(target_)));
  }

  void QueueSyntheticGesture(std::unique_ptr<SyntheticGesture> gesture) {
    controller_->QueueSyntheticGesture(
        std::move(gesture),
        base::Bind(
            &SyntheticGestureControllerTestBase::OnSyntheticGestureCompleted,
            base::Unretained(this)));
  }

  void FlushInputUntilComplete() {
    while (target_->flush_requested()) {
      while (target_->flush_requested()) {
        target_->ClearFlushRequest();
        time_ += base::TimeDelta::FromMilliseconds(kFlushInputRateInMs);
        controller_->Flush(time_);
      }
      controller_->OnDidFlushInput();
    }
  }

  void OnSyntheticGestureCompleted(SyntheticGesture::Result result) {
    DCHECK_NE(result, SyntheticGesture::GESTURE_RUNNING);
    if (result == SyntheticGesture::GESTURE_FINISHED)
      num_success_++;
    else
      num_failure_++;
  }

  base::TimeDelta GetTotalTime() const { return time_ - start_time_; }

  MockSyntheticGestureTarget* target_;
  std::unique_ptr<SyntheticGestureController> controller_;
  base::TimeTicks start_time_;
  base::TimeTicks time_;
  int num_success_;
  int num_failure_;
};

class SyntheticGestureControllerTest
    : public SyntheticGestureControllerTestBase,
      public testing::Test {
 protected:
  void SetUp() override {
    start_time_ = base::TimeTicks::Now();
    time_ = start_time_;
    num_success_ = 0;
    num_failure_ = 0;
  }

  void TearDown() override {
    controller_.reset();
    target_ = nullptr;
    time_ = base::TimeTicks();
  }
};

class SyntheticGestureControllerTestWithParam
    : public SyntheticGestureControllerTestBase,
      public testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    start_time_ = base::TimeTicks::Now();
    time_ = start_time_;
    num_success_ = 0;
    num_failure_ = 0;
  }

  void TearDown() override {
    controller_.reset();
    target_ = nullptr;
    time_ = base::TimeTicks();
  }
};

TEST_F(SyntheticGestureControllerTest, SingleGesture) {
  CreateControllerAndTarget<MockSyntheticGestureTarget>();

  bool finished = false;
  std::unique_ptr<MockSyntheticGesture> gesture(
      new MockSyntheticGesture(&finished, 3));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  EXPECT_TRUE(finished);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
}

TEST_F(SyntheticGestureControllerTest, GestureFailed) {
  CreateControllerAndTarget<MockSyntheticGestureTarget>();

  bool finished = false;
  std::unique_ptr<MockSyntheticGesture> gesture(
      new MockSyntheticGesture(&finished, 0));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  EXPECT_TRUE(finished);
  EXPECT_EQ(1, num_failure_);
  EXPECT_EQ(0, num_success_);
}

TEST_F(SyntheticGestureControllerTest, SuccessiveGestures) {
  CreateControllerAndTarget<MockSyntheticGestureTarget>();

  bool finished_1 = false;
  std::unique_ptr<MockSyntheticGesture> gesture_1(
      new MockSyntheticGesture(&finished_1, 2));
  bool finished_2 = false;
  std::unique_ptr<MockSyntheticGesture> gesture_2(
      new MockSyntheticGesture(&finished_2, 4));

  // Queue first gesture and wait for it to finish
  QueueSyntheticGesture(std::move(gesture_1));
  FlushInputUntilComplete();

  EXPECT_TRUE(finished_1);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);

  // Queue second gesture.
  QueueSyntheticGesture(std::move(gesture_2));
  FlushInputUntilComplete();

  EXPECT_TRUE(finished_2);
  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
}

TEST_F(SyntheticGestureControllerTest, TwoGesturesInFlight) {
  CreateControllerAndTarget<MockSyntheticGestureTarget>();

  bool finished_1 = false;
  std::unique_ptr<MockSyntheticGesture> gesture_1(
      new MockSyntheticGesture(&finished_1, 2));
  bool finished_2 = false;
  std::unique_ptr<MockSyntheticGesture> gesture_2(
      new MockSyntheticGesture(&finished_2, 4));

  QueueSyntheticGesture(std::move(gesture_1));
  QueueSyntheticGesture(std::move(gesture_2));
  FlushInputUntilComplete();

  EXPECT_TRUE(finished_1);
  EXPECT_TRUE(finished_2);

  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
}

TEST_F(SyntheticGestureControllerTest, GestureCompletedOnDidFlushInput) {
  CreateControllerAndTarget<MockSyntheticGestureTarget>();

  bool finished_1, finished_2;
  std::unique_ptr<MockSyntheticGesture> gesture_1(
      new MockSyntheticGesture(&finished_1, 2));
  std::unique_ptr<MockSyntheticGesture> gesture_2(
      new MockSyntheticGesture(&finished_2, 4));

  QueueSyntheticGesture(std::move(gesture_1));
  QueueSyntheticGesture(std::move(gesture_2));

  while (target_->flush_requested()) {
    target_->ClearFlushRequest();
    time_ += base::TimeDelta::FromMilliseconds(kFlushInputRateInMs);
    controller_->Flush(time_);
  }
  EXPECT_EQ(0, num_success_);
  controller_->OnDidFlushInput();
  EXPECT_EQ(1, num_success_);

  while (target_->flush_requested()) {
    target_->ClearFlushRequest();
    time_ += base::TimeDelta::FromMilliseconds(kFlushInputRateInMs);
    controller_->Flush(time_);
  }
  EXPECT_EQ(1, num_success_);
  controller_->OnDidFlushInput();
  EXPECT_EQ(2, num_success_);
}

gfx::Vector2d AddTouchSlopToVector(const gfx::Vector2dF& vector,
                                   SyntheticGestureTarget* target) {
  const int kTouchSlop = target->GetTouchSlopInDips();

  int x = vector.x();
  if (x > 0)
    x += kTouchSlop;
  else if (x < 0)
    x -= kTouchSlop;

  int y = vector.y();
  if (y > 0)
    y += kTouchSlop;
  else if (y < 0)
    y -= kTouchSlop;

  return gfx::Vector2d(x, y);
}

TEST_P(SyntheticGestureControllerTestWithParam,
       SingleMoveGestureTouchVertical) {
  CreateControllerAndTarget<MockMoveTouchTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::TOUCH_INPUT;
  if (GetParam() == TOUCH_DRAG) {
    params.add_slop = false;
  }
  params.start_point.SetPoint(89, 32);
  params.distances.push_back(gfx::Vector2d(0, 123));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  if (GetParam() == TOUCH_SCROLL) {
    EXPECT_EQ(AddTouchSlopToVector(params.distances[0], target_),
              scroll_target->start_to_end_distance());
  } else {
    EXPECT_EQ(params.distances[0], scroll_target->start_to_end_distance());
  }
}

TEST_P(SyntheticGestureControllerTestWithParam,
       SingleScrollGestureTouchHorizontal) {
  CreateControllerAndTarget<MockMoveTouchTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::TOUCH_INPUT;
  if (GetParam() == TOUCH_DRAG) {
    params.add_slop = false;
  }
  params.start_point.SetPoint(12, -23);
  params.distances.push_back(gfx::Vector2d(-234, 0));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  if (GetParam() == TOUCH_SCROLL) {
    EXPECT_EQ(AddTouchSlopToVector(params.distances[0], target_),
              scroll_target->start_to_end_distance());
  } else {
    EXPECT_EQ(params.distances[0], scroll_target->start_to_end_distance());
  }
}

void CheckIsWithinRangeSingle(float scroll_distance,
                              int target_distance,
                              SyntheticGestureTarget* target) {
  if (target_distance > 0) {
    EXPECT_LE(target_distance, scroll_distance);
    EXPECT_LE(scroll_distance, target_distance + target->GetTouchSlopInDips());
  } else {
    EXPECT_GE(target_distance, scroll_distance);
    EXPECT_GE(scroll_distance, target_distance - target->GetTouchSlopInDips());
  }
}

void CheckSingleScrollDistanceIsWithinRange(
    const gfx::Vector2dF& scroll_distance,
    const gfx::Vector2dF& target_distance,
    SyntheticGestureTarget* target) {
  CheckIsWithinRangeSingle(scroll_distance.x(), target_distance.x(), target);
  CheckIsWithinRangeSingle(scroll_distance.y(), target_distance.y(), target);
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureTouchDiagonal) {
  CreateControllerAndTarget<MockMoveTouchTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::TOUCH_INPUT;
  params.start_point.SetPoint(0, 7);
  params.distances.push_back(gfx::Vector2d(413, -83));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  CheckSingleScrollDistanceIsWithinRange(
      scroll_target->start_to_end_distance(), params.distances[0], target_);
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureTouchLongStop) {
  CreateControllerAndTarget<MockMoveTouchTarget>();

  // Create a smooth scroll with a short distance and set the pointer assumed
  // stopped time high, so that the stopping should dominate the time the
  // gesture is active.
  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::TOUCH_INPUT;
  params.start_point.SetPoint(-98, -23);
  params.distances.push_back(gfx::Vector2d(21, -12));
  params.prevent_fling = true;
  target_->set_pointer_assumed_stopped_time_ms(543);

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  CheckSingleScrollDistanceIsWithinRange(
      scroll_target->start_to_end_distance(), params.distances[0], target_);
  EXPECT_GE(GetTotalTime(), target_->PointerAssumedStoppedTime());
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureTouchFling) {
  CreateControllerAndTarget<MockMoveTouchTarget>();

  // Create a smooth scroll with a short distance and set the pointer assumed
  // stopped time high. Disable 'prevent_fling' and check that the gesture
  // finishes without waiting before it stops.
  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::TOUCH_INPUT;
  params.start_point.SetPoint(-89, 78);
  params.distances.push_back(gfx::Vector2d(-43, 19));
  params.prevent_fling = false;

  target_->set_pointer_assumed_stopped_time_ms(543);

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  CheckSingleScrollDistanceIsWithinRange(
      scroll_target->start_to_end_distance(), params.distances[0], target_);
  EXPECT_LE(GetTotalTime(), target_->PointerAssumedStoppedTime());
}

TEST_P(SyntheticGestureControllerTestWithParam,
       SingleScrollGestureTouchZeroDistance) {
  CreateControllerAndTarget<MockMoveTouchTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::TOUCH_INPUT;
  if (GetParam() == TOUCH_DRAG) {
    params.add_slop = false;
  }
  params.start_point.SetPoint(-32, 43);
  params.distances.push_back(gfx::Vector2d(0, 0));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(gfx::Vector2dF(0, 0), scroll_target->start_to_end_distance());
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureMouseVertical) {
  CreateControllerAndTarget<MockScrollMouseTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::MOUSE_WHEEL_INPUT;
  params.start_point.SetPoint(432, 89);
  params.distances.push_back(gfx::Vector2d(0, -234));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(params.distances[0], scroll_target->start_to_end_distance());
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureMouseHorizontal) {
  CreateControllerAndTarget<MockScrollMouseTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::MOUSE_WHEEL_INPUT;
  params.start_point.SetPoint(90, 12);
  params.distances.push_back(gfx::Vector2d(345, 0));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(params.distances[0], scroll_target->start_to_end_distance());
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureMouseDiagonal) {
  CreateControllerAndTarget<MockScrollMouseTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::MOUSE_WHEEL_INPUT;
  params.start_point.SetPoint(90, 12);
  params.distances.push_back(gfx::Vector2d(-194, 303));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(params.distances[0], scroll_target->start_to_end_distance());
}

TEST_F(SyntheticGestureControllerTest, MultiScrollGestureMouse) {
  CreateControllerAndTarget<MockScrollMouseTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::MOUSE_WHEEL_INPUT;
  params.start_point.SetPoint(90, 12);
  params.distances.push_back(gfx::Vector2d(-129, 212));
  params.distances.push_back(gfx::Vector2d(8, -9));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(params.distances[0] + params.distances[1],
            scroll_target->start_to_end_distance());
}

TEST_F(SyntheticGestureControllerTest, MultiScrollGestureMouseHorizontal) {
  CreateControllerAndTarget<MockScrollMouseTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::MOUSE_WHEEL_INPUT;
  params.start_point.SetPoint(90, 12);
  params.distances.push_back(gfx::Vector2d(-129, 0));
  params.distances.push_back(gfx::Vector2d(79, 0));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  // This check only works for horizontal or vertical scrolls because of
  // floating point precision issues with diagonal scrolls.
  EXPECT_FLOAT_EQ(params.distances[0].Length() + params.distances[1].Length(),
                  scroll_target->total_abs_move_distance_length());
  EXPECT_EQ(params.distances[0] + params.distances[1],
            scroll_target->start_to_end_distance());
}

void CheckIsWithinRangeMulti(float scroll_distance,
                             int target_distance,
                             SyntheticGestureTarget* target) {
  if (target_distance > 0) {
    EXPECT_GE(scroll_distance, target_distance - target->GetTouchSlopInDips());
    EXPECT_LE(scroll_distance, target_distance + target->GetTouchSlopInDips());
  } else {
    EXPECT_LE(scroll_distance, target_distance + target->GetTouchSlopInDips());
    EXPECT_GE(scroll_distance, target_distance - target->GetTouchSlopInDips());
  }
}

void CheckMultiScrollDistanceIsWithinRange(
    const gfx::Vector2dF& scroll_distance,
    const gfx::Vector2dF& target_distance,
    SyntheticGestureTarget* target) {
  CheckIsWithinRangeMulti(scroll_distance.x(), target_distance.x(), target);
  CheckIsWithinRangeMulti(scroll_distance.y(), target_distance.y(), target);
}

TEST_F(SyntheticGestureControllerTest, MultiScrollGestureTouch) {
  CreateControllerAndTarget<MockMoveTouchTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::TOUCH_INPUT;
  params.start_point.SetPoint(8, -13);
  params.distances.push_back(gfx::Vector2d(234, 133));
  params.distances.push_back(gfx::Vector2d(-9, 78));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  CheckMultiScrollDistanceIsWithinRange(
      scroll_target->start_to_end_distance(),
      params.distances[0] + params.distances[1],
      target_);
}

TEST_P(SyntheticGestureControllerTestWithParam,
       MultiScrollGestureTouchVertical) {
  CreateControllerAndTarget<MockMoveTouchTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::TOUCH_INPUT;
  if (GetParam() == TOUCH_DRAG) {
    params.add_slop = false;
  }
  params.start_point.SetPoint(234, -13);
  params.distances.push_back(gfx::Vector2d(0, 133));
  params.distances.push_back(gfx::Vector2d(0, 78));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* scroll_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  if (GetParam() == TOUCH_SCROLL) {
    EXPECT_FLOAT_EQ(params.distances[0].Length() +
                        params.distances[1].Length() +
                        target_->GetTouchSlopInDips(),
                    scroll_target->total_abs_move_distance_length());
  EXPECT_EQ(AddTouchSlopToVector(params.distances[0] + params.distances[1],
                                 target_),
            scroll_target->start_to_end_distance());
  } else {
    EXPECT_FLOAT_EQ(params.distances[0].Length() + params.distances[1].Length(),
                    scroll_target->total_abs_move_distance_length());
    EXPECT_EQ(params.distances[0] + params.distances[1],
              scroll_target->start_to_end_distance());
  }
}

INSTANTIATE_TEST_CASE_P(Single,
                        SyntheticGestureControllerTestWithParam,
                        testing::Values(TOUCH_SCROLL, TOUCH_DRAG));

TEST_F(SyntheticGestureControllerTest, SingleDragGestureMouseDiagonal) {
  CreateControllerAndTarget<MockDragMouseTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::MOUSE_DRAG_INPUT;
  params.start_point.SetPoint(0, 7);
  params.distances.push_back(gfx::Vector2d(413, -83));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* drag_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(drag_target->start_to_end_distance(), params.distances[0]);
}

TEST_F(SyntheticGestureControllerTest, SingleDragGestureMouseZeroDistance) {
  CreateControllerAndTarget<MockDragMouseTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::MOUSE_DRAG_INPUT;
  params.start_point.SetPoint(-32, 43);
  params.distances.push_back(gfx::Vector2d(0, 0));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* drag_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(gfx::Vector2dF(0, 0), drag_target->start_to_end_distance());
}

TEST_F(SyntheticGestureControllerTest, MultiDragGestureMouse) {
  CreateControllerAndTarget<MockDragMouseTarget>();

  SyntheticSmoothMoveGestureParams params;
  params.input_type = SyntheticSmoothMoveGestureParams::MOUSE_DRAG_INPUT;
  params.start_point.SetPoint(8, -13);
  params.distances.push_back(gfx::Vector2d(234, 133));
  params.distances.push_back(gfx::Vector2d(-9, 78));

  std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
      new SyntheticSmoothMoveGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockMoveGestureTarget* drag_target =
      static_cast<MockMoveGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(drag_target->start_to_end_distance(),
            params.distances[0] + params.distances[1]);
}

TEST_F(SyntheticGestureControllerTest,
       SyntheticSmoothDragTestUsingSingleMouseDrag) {
  CreateControllerAndTarget<MockDragMouseTarget>();

  SyntheticSmoothDragGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.distances.push_back(gfx::Vector2d(234, 133));
  params.speed_in_pixels_s = 800;

  std::unique_ptr<SyntheticSmoothDragGesture> gesture(
      new SyntheticSmoothDragGesture(params));
  const base::TimeTicks timestamp;
  gesture->ForwardInputEvents(timestamp, target_);
}

TEST_F(SyntheticGestureControllerTest,
       SyntheticSmoothDragTestUsingSingleTouchDrag) {
  CreateControllerAndTarget<MockMoveTouchTarget>();

  SyntheticSmoothDragGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.start_point.SetPoint(89, 32);
  params.distances.push_back(gfx::Vector2d(0, 123));
  params.speed_in_pixels_s = 800;

  std::unique_ptr<SyntheticSmoothDragGesture> gesture(
      new SyntheticSmoothDragGesture(params));
  const base::TimeTicks timestamp;
  gesture->ForwardInputEvents(timestamp, target_);
}

TEST_F(SyntheticGestureControllerTest,
       SyntheticSmoothScrollTestUsingSingleTouchScroll) {
  CreateControllerAndTarget<MockMoveTouchTarget>();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;

  std::unique_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  const base::TimeTicks timestamp;
  gesture->ForwardInputEvents(timestamp, target_);
}

TEST_F(SyntheticGestureControllerTest,
       SyntheticSmoothScrollTestUsingSingleMouseScroll) {
  CreateControllerAndTarget<MockScrollMouseTarget>();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.anchor.SetPoint(432, 89);
  params.distances.push_back(gfx::Vector2d(0, -234));
  params.speed_in_pixels_s = 800;

  std::unique_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  const base::TimeTicks timestamp;
  gesture->ForwardInputEvents(timestamp, target_);
}

TEST_F(SyntheticGestureControllerTest,
       TouchscreenTouchpadPinchGestureTouchZoomIn) {
  CreateControllerAndTarget<MockSyntheticTouchscreenPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.scale_factor = 2.3f;
  params.anchor.SetPoint(54, 89);

  std::unique_ptr<SyntheticTouchscreenPinchGesture> gesture(
      new SyntheticTouchscreenPinchGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockSyntheticTouchscreenPinchTouchTarget* pinch_target =
      static_cast<MockSyntheticTouchscreenPinchTouchTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pinch_target->zoom_direction(),
            MockSyntheticTouchscreenPinchTouchTarget::ZOOM_IN);
  EXPECT_FLOAT_EQ(params.scale_factor, pinch_target->ComputeScaleFactor());
}

TEST_F(SyntheticGestureControllerTest,
       TouchscreenTouchpadPinchGestureTouchZoomOut) {
  CreateControllerAndTarget<MockSyntheticTouchscreenPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.scale_factor = 0.4f;
  params.anchor.SetPoint(-12, 93);

  std::unique_ptr<SyntheticTouchscreenPinchGesture> gesture(
      new SyntheticTouchscreenPinchGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockSyntheticTouchscreenPinchTouchTarget* pinch_target =
      static_cast<MockSyntheticTouchscreenPinchTouchTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pinch_target->zoom_direction(),
            MockSyntheticTouchscreenPinchTouchTarget::ZOOM_OUT);
  EXPECT_FLOAT_EQ(params.scale_factor, pinch_target->ComputeScaleFactor());
}

TEST_F(SyntheticGestureControllerTest,
       TouchscreenTouchpadPinchGestureTouchNoScaling) {
  CreateControllerAndTarget<MockSyntheticTouchscreenPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.scale_factor = 1.0f;

  std::unique_ptr<SyntheticTouchscreenPinchGesture> gesture(
      new SyntheticTouchscreenPinchGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockSyntheticTouchscreenPinchTouchTarget* pinch_target =
      static_cast<MockSyntheticTouchscreenPinchTouchTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pinch_target->zoom_direction(),
            MockSyntheticTouchscreenPinchTouchTarget::ZOOM_DIRECTION_UNKNOWN);
  EXPECT_EQ(params.scale_factor, pinch_target->ComputeScaleFactor());
}

TEST_F(SyntheticGestureControllerTest, TouchpadPinchGestureTouchZoomIn) {
  CreateControllerAndTarget<MockSyntheticTouchpadPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.scale_factor = 2.3f;
  params.anchor.SetPoint(54, 89);

  std::unique_ptr<SyntheticTouchpadPinchGesture> gesture(
      new SyntheticTouchpadPinchGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockSyntheticTouchpadPinchTouchTarget* pinch_target =
      static_cast<MockSyntheticTouchpadPinchTouchTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pinch_target->zoom_direction(),
            MockSyntheticTouchpadPinchTouchTarget::ZOOM_IN);
  EXPECT_FLOAT_EQ(params.scale_factor, pinch_target->scale_factor());
}

TEST_F(SyntheticGestureControllerTest, TouchpadPinchGestureTouchZoomOut) {
  CreateControllerAndTarget<MockSyntheticTouchpadPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.scale_factor = 0.4f;
  params.anchor.SetPoint(-12, 93);

  std::unique_ptr<SyntheticTouchpadPinchGesture> gesture(
      new SyntheticTouchpadPinchGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockSyntheticTouchpadPinchTouchTarget* pinch_target =
      static_cast<MockSyntheticTouchpadPinchTouchTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pinch_target->zoom_direction(),
            MockSyntheticTouchpadPinchTouchTarget::ZOOM_OUT);
  EXPECT_FLOAT_EQ(params.scale_factor, pinch_target->scale_factor());
}

TEST_F(SyntheticGestureControllerTest, TouchpadPinchGestureTouchNoScaling) {
  CreateControllerAndTarget<MockSyntheticTouchpadPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.scale_factor = 1.0f;

  std::unique_ptr<SyntheticTouchpadPinchGesture> gesture(
      new SyntheticTouchpadPinchGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockSyntheticTouchpadPinchTouchTarget* pinch_target =
      static_cast<MockSyntheticTouchpadPinchTouchTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pinch_target->zoom_direction(),
            MockSyntheticTouchpadPinchTouchTarget::ZOOM_DIRECTION_UNKNOWN);
  EXPECT_EQ(params.scale_factor, pinch_target->scale_factor());
}

// Ensure that if SyntheticPinchGesture is instantiated with TOUCH_INPUT it
// correctly creates a touchscreen gesture.
TEST_F(SyntheticGestureControllerTest, PinchGestureExplicitTouch) {
  CreateControllerAndTarget<MockSyntheticTouchscreenPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.scale_factor = 2.3f;
  params.anchor.SetPoint(54, 89);

  std::unique_ptr<SyntheticPinchGesture> gesture(
      new SyntheticPinchGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  // Gesture target will fail expectations if the wrong underlying
  // SyntheticPinch*Gesture was instantiated.
}

// Ensure that if SyntheticPinchGesture is instantiated with MOUSE_INPUT it
// correctly creates a touchpad gesture.
TEST_F(SyntheticGestureControllerTest, PinchGestureExplicitMouse) {
  CreateControllerAndTarget<MockSyntheticTouchpadPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.scale_factor = 2.3f;
  params.anchor.SetPoint(54, 89);

  std::unique_ptr<SyntheticPinchGesture> gesture(
      new SyntheticPinchGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  // Gesture target will fail expectations if the wrong underlying
  // SyntheticPinch*Gesture was instantiated.
}

// Ensure that if SyntheticPinchGesture is instantiated with DEFAULT_INPUT it
// correctly creates a touchscreen gesture for a touchscreen controller.
TEST_F(SyntheticGestureControllerTest, PinchGestureDefaultTouch) {
  CreateControllerAndTarget<MockSyntheticTouchscreenPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::DEFAULT_INPUT;
  params.scale_factor = 2.3f;
  params.anchor.SetPoint(54, 89);

  std::unique_ptr<SyntheticPinchGesture> gesture(
      new SyntheticPinchGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  // Gesture target will fail expectations if the wrong underlying
  // SyntheticPinch*Gesture was instantiated.
}

// Ensure that if SyntheticPinchGesture is instantiated with DEFAULT_INPUT it
// correctly creates a touchpad gesture for a touchpad controller.
TEST_F(SyntheticGestureControllerTest, PinchGestureDefaultMouse) {
  CreateControllerAndTarget<MockSyntheticTouchpadPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::DEFAULT_INPUT;
  params.scale_factor = 2.3f;
  params.anchor.SetPoint(54, 89);

  std::unique_ptr<SyntheticPinchGesture> gesture(
      new SyntheticPinchGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  // Gesture target will fail expectations if the wrong underlying
  // SyntheticPinch*Gesture was instantiated.
}

TEST_F(SyntheticGestureControllerTest, TapGestureTouch) {
  CreateControllerAndTarget<MockSyntheticTapTouchTarget>();

  SyntheticTapGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.duration_ms = 123;
  params.position.SetPoint(87, -124);

  std::unique_ptr<SyntheticTapGesture> gesture(new SyntheticTapGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockSyntheticTapTouchTarget* tap_target =
      static_cast<MockSyntheticTapTouchTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(tap_target->GestureFinished());
  EXPECT_EQ(tap_target->position(), params.position);
  EXPECT_EQ(tap_target->GetDuration().InMilliseconds(), params.duration_ms);
  EXPECT_GE(GetTotalTime(),
            base::TimeDelta::FromMilliseconds(params.duration_ms));
}

TEST_F(SyntheticGestureControllerTest, TapGestureMouse) {
  CreateControllerAndTarget<MockSyntheticTapMouseTarget>();

  SyntheticTapGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.duration_ms = 79;
  params.position.SetPoint(98, 123);

  std::unique_ptr<SyntheticTapGesture> gesture(new SyntheticTapGesture(params));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockSyntheticTapMouseTarget* tap_target =
      static_cast<MockSyntheticTapMouseTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(tap_target->GestureFinished());
  EXPECT_EQ(tap_target->position(), params.position);
  EXPECT_EQ(tap_target->GetDuration().InMilliseconds(), params.duration_ms);
  EXPECT_GE(GetTotalTime(),
            base::TimeDelta::FromMilliseconds(params.duration_ms));
}

TEST_F(SyntheticGestureControllerTest, PointerTouchAction) {
  CreateControllerAndTarget<MockSyntheticPointerTouchActionTarget>();

  SyntheticPointerActionParams params = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.set_index(0);
  params.set_position(gfx::PointF(54, 89));
  SyntheticTouchPointer synthetic_pointer;

  std::unique_ptr<SyntheticPointerAction> gesture(
      new SyntheticPointerAction(params, &synthetic_pointer));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  MockSyntheticPointerTouchActionTarget* pointer_touch_target =
      static_cast<MockSyntheticPointerTouchActionTarget*>(target_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchStart);
  EXPECT_EQ(pointer_touch_target->positions(0), params.position());
  EXPECT_EQ(pointer_touch_target->states(0), WebTouchPoint::StatePressed);
  ASSERT_EQ(pointer_touch_target->touch_length(), 1U);

  params.set_index(1);
  params.set_position(gfx::PointF(79, 132));
  gesture.reset(new SyntheticPointerAction(params, &synthetic_pointer));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  pointer_touch_target =
      static_cast<MockSyntheticPointerTouchActionTarget*>(target_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchStart);
  EXPECT_EQ(pointer_touch_target->indexes(1), params.index());
  EXPECT_EQ(pointer_touch_target->positions(1), params.position());
  EXPECT_EQ(pointer_touch_target->states(1), WebTouchPoint::StatePressed);
  ASSERT_EQ(pointer_touch_target->touch_length(), 2U);

  params.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  params.set_position(gfx::PointF(133, 156));
  gesture.reset(new SyntheticPointerAction(params, &synthetic_pointer));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  pointer_touch_target =
      static_cast<MockSyntheticPointerTouchActionTarget*>(target_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchMove);
  EXPECT_EQ(pointer_touch_target->positions(1), params.position());
  EXPECT_EQ(pointer_touch_target->states(1), WebTouchPoint::StateMoved);
  ASSERT_EQ(pointer_touch_target->touch_length(), 2U);

  params.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  gesture.reset(new SyntheticPointerAction(params, &synthetic_pointer));
  QueueSyntheticGesture(std::move(gesture));
  FlushInputUntilComplete();

  pointer_touch_target =
      static_cast<MockSyntheticPointerTouchActionTarget*>(target_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchEnd);
  EXPECT_EQ(pointer_touch_target->states(1), WebTouchPoint::StateReleased);
}

}  // namespace

}  // namespace content
