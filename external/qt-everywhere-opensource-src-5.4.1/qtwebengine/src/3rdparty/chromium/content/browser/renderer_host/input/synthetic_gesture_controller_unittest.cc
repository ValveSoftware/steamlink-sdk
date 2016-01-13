// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_controller.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_pinch_gesture.h"
#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture.h"
#include "content/browser/renderer_host/input/synthetic_tap_gesture.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input/synthetic_tap_gesture_params.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/point.h"
#include "ui/gfx/point_f.h"
#include "ui/gfx/vector2d.h"
#include "ui/gfx/vector2d_f.h"

using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;

namespace content {

namespace {

const int kFlushInputRateInMs = 16;
const int kPointerAssumedStoppedTimeMs = 43;
const float kTouchSlopInDips = 7.0f;
const float kMinScalingSpanInDips = 27.5f;

class MockSyntheticGesture : public SyntheticGesture {
 public:
  MockSyntheticGesture(bool* finished, int num_steps)
      : finished_(finished),
        num_steps_(num_steps),
        step_count_(0) {
    *finished_ = false;
  }
  virtual ~MockSyntheticGesture() {}

  virtual Result ForwardInputEvents(const base::TimeTicks& timestamp,
                                    SyntheticGestureTarget* target) OVERRIDE {
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
  virtual ~MockSyntheticGestureTarget() {}

  // SyntheticGestureTarget:
  virtual void DispatchInputEventToPlatform(
      const WebInputEvent& event) OVERRIDE {}

  virtual void SetNeedsFlush() OVERRIDE {
    flush_requested_ = true;
  }

  virtual SyntheticGestureParams::GestureSourceType
  GetDefaultSyntheticGestureSourceType() const OVERRIDE {
    return SyntheticGestureParams::TOUCH_INPUT;
  }

  virtual base::TimeDelta PointerAssumedStoppedTime() const OVERRIDE {
    return base::TimeDelta::FromMilliseconds(pointer_assumed_stopped_time_ms_);
  }

  void set_pointer_assumed_stopped_time_ms(int time_ms) {
    pointer_assumed_stopped_time_ms_ = time_ms;
  }

  virtual float GetTouchSlopInDips() const OVERRIDE {
    return kTouchSlopInDips;
  }

  virtual float GetMinScalingSpanInDips() const OVERRIDE {
    return kMinScalingSpanInDips;
  }

  bool flush_requested() const { return flush_requested_; }
  void ClearFlushRequest() { flush_requested_ = false; }

 private:
  bool flush_requested_;

  int pointer_assumed_stopped_time_ms_;
};

class MockScrollGestureTarget : public MockSyntheticGestureTarget {
 public:
  MockScrollGestureTarget() : total_abs_scroll_distance_length_(0) {}
  virtual ~MockScrollGestureTarget() {}

  gfx::Vector2dF start_to_end_distance() const {
    return start_to_end_distance_;
  }
  float total_abs_scroll_distance_length() const {
    return total_abs_scroll_distance_length_;
  }

 protected:
  gfx::Vector2dF start_to_end_distance_;
  float total_abs_scroll_distance_length_;
};

class MockScrollMouseTarget : public MockScrollGestureTarget {
 public:
  MockScrollMouseTarget() {}
  virtual ~MockScrollMouseTarget() {}

  virtual void DispatchInputEventToPlatform(
      const WebInputEvent& event) OVERRIDE {
    ASSERT_EQ(event.type, WebInputEvent::MouseWheel);
    const WebMouseWheelEvent& mouse_wheel_event =
        static_cast<const WebMouseWheelEvent&>(event);
    gfx::Vector2dF delta(mouse_wheel_event.deltaX, mouse_wheel_event.deltaY);
    start_to_end_distance_ += delta;
    total_abs_scroll_distance_length_ += delta.Length();
  }
};

class MockScrollTouchTarget : public MockScrollGestureTarget {
 public:
  MockScrollTouchTarget() : started_(false) {}
  virtual ~MockScrollTouchTarget() {}

  virtual void DispatchInputEventToPlatform(
      const WebInputEvent& event) OVERRIDE {
    ASSERT_TRUE(WebInputEvent::isTouchEventType(event.type));
    const WebTouchEvent& touch_event = static_cast<const WebTouchEvent&>(event);
    ASSERT_EQ(touch_event.touchesLength, 1U);

    if (!started_) {
      ASSERT_EQ(touch_event.type, WebInputEvent::TouchStart);
      start_.SetPoint(touch_event.touches[0].position.x,
                      touch_event.touches[0].position.y);
      last_touch_point_ = start_;
      started_ = true;
    } else {
      ASSERT_NE(touch_event.type, WebInputEvent::TouchStart);
      ASSERT_NE(touch_event.type, WebInputEvent::TouchCancel);

      gfx::PointF touch_point(touch_event.touches[0].position.x,
                              touch_event.touches[0].position.y);
      gfx::Vector2dF delta = touch_point - last_touch_point_;
      total_abs_scroll_distance_length_ += delta.Length();

      if (touch_event.type == WebInputEvent::TouchEnd)
        start_to_end_distance_ = touch_point - start_;

      last_touch_point_ = touch_point;
    }
  }

 protected:
  gfx::Point start_;
  gfx::PointF last_touch_point_;
  bool started_;
};

class MockSyntheticPinchTouchTarget : public MockSyntheticGestureTarget {
 public:
  enum ZoomDirection {
    ZOOM_DIRECTION_UNKNOWN,
    ZOOM_IN,
    ZOOM_OUT
  };

  MockSyntheticPinchTouchTarget()
      : initial_pointer_distance_(0),
        last_pointer_distance_(0),
        zoom_direction_(ZOOM_DIRECTION_UNKNOWN),
        started_(false) {}
  virtual ~MockSyntheticPinchTouchTarget() {}

  virtual void DispatchInputEventToPlatform(
      const WebInputEvent& event) OVERRIDE {
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

class MockSyntheticTapGestureTarget : public MockSyntheticGestureTarget {
 public:
  MockSyntheticTapGestureTarget() : state_(NOT_STARTED) {}
  virtual ~MockSyntheticTapGestureTarget() {}

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
  virtual ~MockSyntheticTapTouchTarget() {}

  virtual void DispatchInputEventToPlatform(
        const WebInputEvent& event) OVERRIDE {
    ASSERT_TRUE(WebInputEvent::isTouchEventType(event.type));
    const WebTouchEvent& touch_event = static_cast<const WebTouchEvent&>(event);
    ASSERT_EQ(touch_event.touchesLength, 1U);

    switch (state_) {
      case NOT_STARTED:
        EXPECT_EQ(touch_event.type, WebInputEvent::TouchStart);
        position_ = gfx::PointF(touch_event.touches[0].position);
        start_time_ = base::TimeDelta::FromMilliseconds(
            static_cast<int64>(touch_event.timeStampSeconds * 1000));
        state_ = STARTED;
        break;
      case STARTED:
        EXPECT_EQ(touch_event.type, WebInputEvent::TouchEnd);
        EXPECT_EQ(position_, gfx::PointF(touch_event.touches[0].position));
        stop_time_ = base::TimeDelta::FromMilliseconds(
            static_cast<int64>(touch_event.timeStampSeconds * 1000));
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
  virtual ~MockSyntheticTapMouseTarget() {}

  virtual void DispatchInputEventToPlatform(
        const WebInputEvent& event) OVERRIDE {
    ASSERT_TRUE(WebInputEvent::isMouseEventType(event.type));
    const WebMouseEvent& mouse_event = static_cast<const WebMouseEvent&>(event);

    switch (state_) {
      case NOT_STARTED:
        EXPECT_EQ(mouse_event.type, WebInputEvent::MouseDown);
        EXPECT_EQ(mouse_event.button, WebMouseEvent::ButtonLeft);
        EXPECT_EQ(mouse_event.clickCount, 1);
        position_ = gfx::PointF(mouse_event.x, mouse_event.y);
        start_time_ = base::TimeDelta::FromMilliseconds(
            static_cast<int64>(mouse_event.timeStampSeconds * 1000));
        state_ = STARTED;
        break;
      case STARTED:
        EXPECT_EQ(mouse_event.type, WebInputEvent::MouseUp);
        EXPECT_EQ(mouse_event.button, WebMouseEvent::ButtonLeft);
        EXPECT_EQ(mouse_event.clickCount, 1);
        EXPECT_EQ(position_, gfx::PointF(mouse_event.x, mouse_event.y));
        stop_time_ = base::TimeDelta::FromMilliseconds(
            static_cast<int64>(mouse_event.timeStampSeconds * 1000));
        state_ = FINISHED;
        break;
      case FINISHED:
        EXPECT_FALSE(true);
        break;
    }
  }
};

class SyntheticGestureControllerTest : public testing::Test {
 public:
  SyntheticGestureControllerTest() {}
  virtual ~SyntheticGestureControllerTest() {}

 protected:
  template<typename MockGestureTarget>
  void CreateControllerAndTarget() {
    target_ = new MockGestureTarget();
    controller_.reset(new SyntheticGestureController(
        scoped_ptr<SyntheticGestureTarget>(target_)));
  }

  virtual void SetUp() OVERRIDE {
    start_time_ = base::TimeTicks::Now();
    time_ = start_time_;
    num_success_ = 0;
    num_failure_ = 0;
  }

  virtual void TearDown() OVERRIDE {
    controller_.reset();
    target_ = NULL;
    time_ = base::TimeTicks();
  }

  void QueueSyntheticGesture(scoped_ptr<SyntheticGesture> gesture) {
    controller_->QueueSyntheticGesture(gesture.Pass(),
        base::Bind(&SyntheticGestureControllerTest::OnSyntheticGestureCompleted,
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
  scoped_ptr<SyntheticGestureController> controller_;
  base::TimeTicks start_time_;
  base::TimeTicks time_;
  int num_success_;
  int num_failure_;
};

TEST_F(SyntheticGestureControllerTest, SingleGesture) {
  CreateControllerAndTarget<MockSyntheticGestureTarget>();

  bool finished;
  scoped_ptr<MockSyntheticGesture> gesture(
      new MockSyntheticGesture(&finished, 3));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  EXPECT_TRUE(finished);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
}

TEST_F(SyntheticGestureControllerTest, GestureFailed) {
  CreateControllerAndTarget<MockSyntheticGestureTarget>();

  bool finished;
  scoped_ptr<MockSyntheticGesture> gesture(
      new MockSyntheticGesture(&finished, 0));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  EXPECT_TRUE(finished);
  EXPECT_EQ(1, num_failure_);
  EXPECT_EQ(0, num_success_);
}

TEST_F(SyntheticGestureControllerTest, SuccessiveGestures) {
  CreateControllerAndTarget<MockSyntheticGestureTarget>();

  bool finished_1, finished_2;
  scoped_ptr<MockSyntheticGesture> gesture_1(
      new MockSyntheticGesture(&finished_1, 2));
  scoped_ptr<MockSyntheticGesture> gesture_2(
      new MockSyntheticGesture(&finished_2, 4));

  // Queue first gesture and wait for it to finish
  QueueSyntheticGesture(gesture_1.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  EXPECT_TRUE(finished_1);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);

  // Queue second gesture.
  QueueSyntheticGesture(gesture_2.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  EXPECT_TRUE(finished_2);
  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
}

TEST_F(SyntheticGestureControllerTest, TwoGesturesInFlight) {
  CreateControllerAndTarget<MockSyntheticGestureTarget>();

  bool finished_1, finished_2;
  scoped_ptr<MockSyntheticGesture> gesture_1(
      new MockSyntheticGesture(&finished_1, 2));
  scoped_ptr<MockSyntheticGesture> gesture_2(
      new MockSyntheticGesture(&finished_2, 4));

  QueueSyntheticGesture(gesture_1.PassAs<SyntheticGesture>());
  QueueSyntheticGesture(gesture_2.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  EXPECT_TRUE(finished_1);
  EXPECT_TRUE(finished_2);

  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
}

TEST_F(SyntheticGestureControllerTest, GestureCompletedOnDidFlushInput) {
  CreateControllerAndTarget<MockSyntheticGestureTarget>();

  bool finished_1, finished_2;
  scoped_ptr<MockSyntheticGesture> gesture_1(
      new MockSyntheticGesture(&finished_1, 2));
  scoped_ptr<MockSyntheticGesture> gesture_2(
      new MockSyntheticGesture(&finished_2, 4));

  QueueSyntheticGesture(gesture_1.PassAs<SyntheticGesture>());
  QueueSyntheticGesture(gesture_2.PassAs<SyntheticGesture>());

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

gfx::Vector2d AddTouchSlopToVector(const gfx::Vector2d& vector,
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

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureTouchVertical) {
  CreateControllerAndTarget<MockScrollTouchTarget>();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.anchor.SetPoint(89, 32);
  params.distances.push_back(gfx::Vector2d(0, 123));

  scoped_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockScrollGestureTarget* scroll_target =
      static_cast<MockScrollGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  // TODO(dominikg): Remove adjustment when crbug.com/332418 is fixed.
  EXPECT_EQ(AddTouchSlopToVector(params.distances[0], target_),
            scroll_target->start_to_end_distance() - gfx::Vector2dF(0, 0.001f));
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureTouchHorizontal) {
  CreateControllerAndTarget<MockScrollTouchTarget>();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.anchor.SetPoint(12, -23);
  params.distances.push_back(gfx::Vector2d(-234, 0));

  scoped_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockScrollGestureTarget* scroll_target =
      static_cast<MockScrollGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  // TODO(dominikg): Use vector comparison when crbug.com/332418 is fixed.
  //EXPECT_EQ(AddTouchSlopToVector(params.distances[0], target_),
  //          scroll_target->start_to_end_distance());
  EXPECT_EQ(AddTouchSlopToVector(params.distances[0], target_).x(),
            scroll_target->start_to_end_distance().x());
  EXPECT_LT(AddTouchSlopToVector(params.distances[0], target_).y(),
            scroll_target->start_to_end_distance().y());
  EXPECT_GE(AddTouchSlopToVector(params.distances[0], target_).y(),
            scroll_target->start_to_end_distance().y() - 0.001f);
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
    const gfx::Vector2d& target_distance,
    SyntheticGestureTarget* target) {
  CheckIsWithinRangeSingle(scroll_distance.x(), target_distance.x(), target);
  CheckIsWithinRangeSingle(scroll_distance.y(), target_distance.y(), target);
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureTouchDiagonal) {
  CreateControllerAndTarget<MockScrollTouchTarget>();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.anchor.SetPoint(0, 7);
  params.distances.push_back(gfx::Vector2d(413, -83));

  scoped_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockScrollGestureTarget* scroll_target =
      static_cast<MockScrollGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  CheckSingleScrollDistanceIsWithinRange(
      scroll_target->start_to_end_distance(), params.distances[0], target_);
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureTouchLongStop) {
  CreateControllerAndTarget<MockScrollTouchTarget>();

  // Create a smooth scroll with a short distance and set the pointer assumed
  // stopped time high, so that the stopping should dominate the time the
  // gesture is active.
  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.anchor.SetPoint(-98, -23);
  params.distances.push_back(gfx::Vector2d(21, -12));
  params.prevent_fling = true;

  target_->set_pointer_assumed_stopped_time_ms(543);

  scoped_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockScrollGestureTarget* scroll_target =
      static_cast<MockScrollGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  CheckSingleScrollDistanceIsWithinRange(
      scroll_target->start_to_end_distance(), params.distances[0], target_);
  EXPECT_GE(GetTotalTime(), target_->PointerAssumedStoppedTime());
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureTouchFling) {
  CreateControllerAndTarget<MockScrollTouchTarget>();

  // Create a smooth scroll with a short distance and set the pointer assumed
  // stopped time high. Disable 'prevent_fling' and check that the gesture
  // finishes without waiting before it stops.
  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.anchor.SetPoint(-89, 78);
  params.distances.push_back(gfx::Vector2d(-43, 19));
  params.prevent_fling = false;

  target_->set_pointer_assumed_stopped_time_ms(543);

  scoped_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockScrollGestureTarget* scroll_target =
      static_cast<MockScrollGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  CheckSingleScrollDistanceIsWithinRange(
      scroll_target->start_to_end_distance(), params.distances[0], target_);
  EXPECT_LE(GetTotalTime(), target_->PointerAssumedStoppedTime());
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureTouchZeroDistance) {
  CreateControllerAndTarget<MockScrollTouchTarget>();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.anchor.SetPoint(-32, 43);
  params.distances.push_back(gfx::Vector2d(0, 0));

  scoped_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockScrollGestureTarget* scroll_target =
      static_cast<MockScrollGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(gfx::Vector2dF(0, 0), scroll_target->start_to_end_distance());
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureMouseVertical) {
  CreateControllerAndTarget<MockScrollMouseTarget>();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.anchor.SetPoint(432, 89);
  params.distances.push_back(gfx::Vector2d(0, -234));

  scoped_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockScrollGestureTarget* scroll_target =
      static_cast<MockScrollGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(params.distances[0], scroll_target->start_to_end_distance());
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureMouseHorizontal) {
  CreateControllerAndTarget<MockScrollMouseTarget>();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.anchor.SetPoint(90, 12);
  params.distances.push_back(gfx::Vector2d(345, 0));

  scoped_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockScrollGestureTarget* scroll_target =
      static_cast<MockScrollGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(params.distances[0], scroll_target->start_to_end_distance());
}

TEST_F(SyntheticGestureControllerTest, SingleScrollGestureMouseDiagonal) {
  CreateControllerAndTarget<MockScrollMouseTarget>();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.anchor.SetPoint(90, 12);
  params.distances.push_back(gfx::Vector2d(-194, 303));

  scoped_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockScrollGestureTarget* scroll_target =
      static_cast<MockScrollGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(params.distances[0], scroll_target->start_to_end_distance());
}

TEST_F(SyntheticGestureControllerTest, MultiScrollGestureMouse) {
  CreateControllerAndTarget<MockScrollMouseTarget>();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.anchor.SetPoint(90, 12);
  params.distances.push_back(gfx::Vector2d(-129, 212));
  params.distances.push_back(gfx::Vector2d(8, -9));

  scoped_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockScrollGestureTarget* scroll_target =
      static_cast<MockScrollGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(params.distances[0] + params.distances[1],
            scroll_target->start_to_end_distance());
}

TEST_F(SyntheticGestureControllerTest, MultiScrollGestureMouseHorizontal) {
  CreateControllerAndTarget<MockScrollMouseTarget>();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.anchor.SetPoint(90, 12);
  params.distances.push_back(gfx::Vector2d(-129, 0));
  params.distances.push_back(gfx::Vector2d(79, 0));

  scoped_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockScrollGestureTarget* scroll_target =
      static_cast<MockScrollGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  // This check only works for horizontal or vertical scrolls because of
  // floating point precision issues with diagonal scrolls.
  EXPECT_FLOAT_EQ(params.distances[0].Length() + params.distances[1].Length(),
                  scroll_target->total_abs_scroll_distance_length());
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
    const gfx::Vector2d& target_distance,
    SyntheticGestureTarget* target) {
  CheckIsWithinRangeMulti(scroll_distance.x(), target_distance.x(), target);
  CheckIsWithinRangeMulti(scroll_distance.y(), target_distance.y(), target);
}

TEST_F(SyntheticGestureControllerTest, MultiScrollGestureTouch) {
  CreateControllerAndTarget<MockScrollTouchTarget>();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.anchor.SetPoint(8, -13);
  params.distances.push_back(gfx::Vector2d(234, 133));
  params.distances.push_back(gfx::Vector2d(-9, 78));

  scoped_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockScrollGestureTarget* scroll_target =
      static_cast<MockScrollGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  CheckMultiScrollDistanceIsWithinRange(
      scroll_target->start_to_end_distance(),
      params.distances[0] + params.distances[1],
      target_);
}

TEST_F(SyntheticGestureControllerTest, MultiScrollGestureTouchVertical) {
  CreateControllerAndTarget<MockScrollTouchTarget>();

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.anchor.SetPoint(234, -13);
  params.distances.push_back(gfx::Vector2d(0, 133));
  params.distances.push_back(gfx::Vector2d(0, 78));

  scoped_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockScrollGestureTarget* scroll_target =
      static_cast<MockScrollGestureTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  // TODO(dominikg): Remove adjustment when crbug.com/332418 is fixed.
  EXPECT_FLOAT_EQ(
      params.distances[0].Length() + params.distances[1].Length() +
          target_->GetTouchSlopInDips(),
      scroll_target->total_abs_scroll_distance_length() - 0.001f);
  EXPECT_EQ(AddTouchSlopToVector(params.distances[0] + params.distances[1],
                                 target_),
            scroll_target->start_to_end_distance() - gfx::Vector2dF(0, 0.001f));
}

TEST_F(SyntheticGestureControllerTest, PinchGestureTouchZoomIn) {
  CreateControllerAndTarget<MockSyntheticPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.scale_factor = 2.3f;
  params.anchor.SetPoint(54, 89);

  scoped_ptr<SyntheticPinchGesture> gesture(new SyntheticPinchGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockSyntheticPinchTouchTarget* pinch_target =
      static_cast<MockSyntheticPinchTouchTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pinch_target->zoom_direction(),
            MockSyntheticPinchTouchTarget::ZOOM_IN);
  EXPECT_FLOAT_EQ(params.scale_factor, pinch_target->ComputeScaleFactor());
}

TEST_F(SyntheticGestureControllerTest, PinchGestureTouchZoomOut) {
  CreateControllerAndTarget<MockSyntheticPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.scale_factor = 0.4f;
  params.anchor.SetPoint(-12, 93);

  scoped_ptr<SyntheticPinchGesture> gesture(new SyntheticPinchGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockSyntheticPinchTouchTarget* pinch_target =
      static_cast<MockSyntheticPinchTouchTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pinch_target->zoom_direction(),
            MockSyntheticPinchTouchTarget::ZOOM_OUT);
  EXPECT_FLOAT_EQ(params.scale_factor, pinch_target->ComputeScaleFactor());
}

TEST_F(SyntheticGestureControllerTest, PinchGestureTouchNoScaling) {
  CreateControllerAndTarget<MockSyntheticPinchTouchTarget>();

  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.scale_factor = 1.0f;

  scoped_ptr<SyntheticPinchGesture> gesture(new SyntheticPinchGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
  FlushInputUntilComplete();

  MockSyntheticPinchTouchTarget* pinch_target =
      static_cast<MockSyntheticPinchTouchTarget*>(target_);
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pinch_target->zoom_direction(),
            MockSyntheticPinchTouchTarget::ZOOM_DIRECTION_UNKNOWN);
  EXPECT_EQ(params.scale_factor, pinch_target->ComputeScaleFactor());
}

TEST_F(SyntheticGestureControllerTest, TapGestureTouch) {
  CreateControllerAndTarget<MockSyntheticTapTouchTarget>();

  SyntheticTapGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.duration_ms = 123;
  params.position.SetPoint(87, -124);

  scoped_ptr<SyntheticTapGesture> gesture(new SyntheticTapGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
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

  scoped_ptr<SyntheticTapGesture> gesture(new SyntheticTapGesture(params));
  QueueSyntheticGesture(gesture.PassAs<SyntheticGesture>());
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

}  // namespace

}  // namespace content
