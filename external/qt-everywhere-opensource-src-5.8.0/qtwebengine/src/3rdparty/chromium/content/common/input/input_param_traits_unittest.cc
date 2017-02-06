// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/input_param_traits.h"

#include <stddef.h>
#include <utility>

#include "content/common/input/input_event.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input/synthetic_pointer_action_params.h"
#include "content/common/input/synthetic_smooth_drag_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input_messages.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {
namespace {

typedef ScopedVector<InputEvent> InputEvents;

class InputParamTraitsTest : public testing::Test {
 protected:
  static void Compare(const InputEvent* a, const InputEvent* b) {
    EXPECT_EQ(!!a->web_event, !!b->web_event);
    if (a->web_event && b->web_event) {
      const size_t a_size = a->web_event->size;
      ASSERT_EQ(a_size, b->web_event->size);
      EXPECT_EQ(0, memcmp(a->web_event.get(), b->web_event.get(), a_size));
    }
    EXPECT_EQ(a->latency_info.latency_components().size(),
              b->latency_info.latency_components().size());
  }

  static void Compare(const InputEvents* a, const InputEvents* b) {
    for (size_t i = 0; i < a->size(); ++i)
      Compare((*a)[i], (*b)[i]);
  }

  static void Compare(const SyntheticSmoothScrollGestureParams* a,
                      const SyntheticSmoothScrollGestureParams* b) {
    EXPECT_EQ(a->gesture_source_type, b->gesture_source_type);
    EXPECT_EQ(a->anchor, b->anchor);
    EXPECT_EQ(a->distances.size(), b->distances.size());
    for (size_t i = 0; i < a->distances.size(); i++)
      EXPECT_EQ(a->distances[i], b->distances[i]);
    EXPECT_EQ(a->prevent_fling, b->prevent_fling);
    EXPECT_EQ(a->speed_in_pixels_s, b->speed_in_pixels_s);
  }

  static void Compare(const SyntheticSmoothDragGestureParams* a,
                      const SyntheticSmoothDragGestureParams* b) {
    EXPECT_EQ(a->gesture_source_type, b->gesture_source_type);
    EXPECT_EQ(a->start_point, b->start_point);
    EXPECT_EQ(a->distances.size(), b->distances.size());
    for (size_t i = 0; i < a->distances.size(); i++)
      EXPECT_EQ(a->distances[i], b->distances[i]);
    EXPECT_EQ(a->speed_in_pixels_s, b->speed_in_pixels_s);
  }

  static void Compare(const SyntheticPinchGestureParams* a,
                      const SyntheticPinchGestureParams* b) {
    EXPECT_EQ(a->gesture_source_type, b->gesture_source_type);
    EXPECT_EQ(a->scale_factor, b->scale_factor);
    EXPECT_EQ(a->anchor, b->anchor);
    EXPECT_EQ(a->relative_pointer_speed_in_pixels_s,
              b->relative_pointer_speed_in_pixels_s);
  }

  static void Compare(const SyntheticTapGestureParams* a,
                      const SyntheticTapGestureParams* b) {
    EXPECT_EQ(a->gesture_source_type, b->gesture_source_type);
    EXPECT_EQ(a->position, b->position);
    EXPECT_EQ(a->duration_ms, b->duration_ms);
  }

  static void Compare(const SyntheticPointerActionParams* a,
                      const SyntheticPointerActionParams* b) {
    EXPECT_EQ(a->gesture_source_type, b->gesture_source_type);
    EXPECT_EQ(a->pointer_action_type(), b->pointer_action_type());
    if (a->pointer_action_type() ==
            SyntheticPointerActionParams::PointerActionType::PRESS ||
        a->pointer_action_type() ==
            SyntheticPointerActionParams::PointerActionType::MOVE) {
      EXPECT_EQ(a->position(), b->position());
    }
    if (a->pointer_action_type() !=
        SyntheticPointerActionParams::PointerActionType::PROCESS) {
      EXPECT_EQ(a->index(), b->index());
    }
  }

  static void Compare(const SyntheticGesturePacket* a,
                      const SyntheticGesturePacket* b) {
    ASSERT_EQ(!!a, !!b);
    if (!a) return;
    ASSERT_EQ(!!a->gesture_params(), !!b->gesture_params());
    if (!a->gesture_params()) return;
    ASSERT_EQ(a->gesture_params()->GetGestureType(),
              b->gesture_params()->GetGestureType());
    switch (a->gesture_params()->GetGestureType()) {
      case SyntheticGestureParams::SMOOTH_SCROLL_GESTURE:
        Compare(SyntheticSmoothScrollGestureParams::Cast(a->gesture_params()),
                SyntheticSmoothScrollGestureParams::Cast(b->gesture_params()));
        break;
      case SyntheticGestureParams::SMOOTH_DRAG_GESTURE:
        Compare(SyntheticSmoothDragGestureParams::Cast(a->gesture_params()),
                SyntheticSmoothDragGestureParams::Cast(b->gesture_params()));
        break;
      case SyntheticGestureParams::PINCH_GESTURE:
        Compare(SyntheticPinchGestureParams::Cast(a->gesture_params()),
                SyntheticPinchGestureParams::Cast(b->gesture_params()));
        break;
      case SyntheticGestureParams::TAP_GESTURE:
        Compare(SyntheticTapGestureParams::Cast(a->gesture_params()),
                SyntheticTapGestureParams::Cast(b->gesture_params()));
        break;
      case SyntheticGestureParams::POINTER_ACTION:
        Compare(SyntheticPointerActionParams::Cast(a->gesture_params()),
                SyntheticPointerActionParams::Cast(b->gesture_params()));
        break;
    }
  }

  static void Verify(const InputEvents& events_in) {
    IPC::Message msg;
    IPC::ParamTraits<InputEvents>::Write(&msg, events_in);

    InputEvents events_out;
    base::PickleIterator iter(msg);
    EXPECT_TRUE(IPC::ParamTraits<InputEvents>::Read(&msg, &iter, &events_out));

    Compare(&events_in, &events_out);

    // Perform a sanity check that logging doesn't explode.
    std::string events_in_string;
    IPC::ParamTraits<InputEvents>::Log(events_in, &events_in_string);
    std::string events_out_string;
    IPC::ParamTraits<InputEvents>::Log(events_out, &events_out_string);
    ASSERT_FALSE(events_in_string.empty());
    EXPECT_EQ(events_in_string, events_out_string);
  }

  static void Verify(const SyntheticGesturePacket& packet_in) {
    IPC::Message msg;
    IPC::ParamTraits<SyntheticGesturePacket>::Write(&msg, packet_in);

    SyntheticGesturePacket packet_out;
    base::PickleIterator iter(msg);
    EXPECT_TRUE(IPC::ParamTraits<SyntheticGesturePacket>::Read(&msg, &iter,
                                                               &packet_out));

    Compare(&packet_in, &packet_out);

    // Perform a sanity check that logging doesn't explode.
    std::string packet_in_string;
    IPC::ParamTraits<SyntheticGesturePacket>::Log(packet_in, &packet_in_string);
    std::string packet_out_string;
    IPC::ParamTraits<SyntheticGesturePacket>::Log(packet_out,
                                                  &packet_out_string);
    ASSERT_FALSE(packet_in_string.empty());
    EXPECT_EQ(packet_in_string, packet_out_string);
  }
};

TEST_F(InputParamTraitsTest, UninitializedEvents) {
  InputEvent event;

  IPC::Message msg;
  IPC::WriteParam(&msg, event);

  InputEvent event_out;
  base::PickleIterator iter(msg);
  EXPECT_FALSE(IPC::ReadParam(&msg, &iter, &event_out));
}

TEST_F(InputParamTraitsTest, InitializedEvents) {
  InputEvents events;

  ui::LatencyInfo latency;

  blink::WebKeyboardEvent key_event;
  key_event.type = blink::WebInputEvent::RawKeyDown;
  key_event.nativeKeyCode = 5;
  events.push_back(new InputEvent(key_event, latency));

  blink::WebMouseWheelEvent wheel_event;
  wheel_event.type = blink::WebInputEvent::MouseWheel;
  wheel_event.deltaX = 10;
  latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, 1, 1);
  events.push_back(new InputEvent(wheel_event, latency));

  blink::WebMouseEvent mouse_event;
  mouse_event.type = blink::WebInputEvent::MouseDown;
  mouse_event.x = 10;
  latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT, 2, 2);
  events.push_back(new InputEvent(mouse_event, latency));

  blink::WebGestureEvent gesture_event;
  gesture_event.type = blink::WebInputEvent::GestureScrollBegin;
  gesture_event.x = -1;
  events.push_back(new InputEvent(gesture_event, latency));

  blink::WebTouchEvent touch_event;
  touch_event.type = blink::WebInputEvent::TouchStart;
  touch_event.touchesLength = 1;
  touch_event.touches[0].radiusX = 1;
  events.push_back(new InputEvent(touch_event, latency));

  Verify(events);
}

TEST_F(InputParamTraitsTest, InvalidSyntheticGestureParams) {
  IPC::Message msg;
  // Write invalid value for SyntheticGestureParams::GestureType.
  WriteParam(&msg, -3);

  SyntheticGesturePacket packet_out;
  base::PickleIterator iter(msg);
  ASSERT_FALSE(
      IPC::ParamTraits<SyntheticGesturePacket>::Read(&msg, &iter, &packet_out));
}

TEST_F(InputParamTraitsTest, SyntheticSmoothScrollGestureParams) {
  std::unique_ptr<SyntheticSmoothScrollGestureParams> gesture_params(
      new SyntheticSmoothScrollGestureParams);
  gesture_params->gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  gesture_params->anchor.SetPoint(234, 345);
  gesture_params->distances.push_back(gfx::Vector2d(123, -789));
  gesture_params->distances.push_back(gfx::Vector2d(-78, 43));
  gesture_params->prevent_fling = false;
  gesture_params->speed_in_pixels_s = 456;
  ASSERT_EQ(SyntheticGestureParams::SMOOTH_SCROLL_GESTURE,
            gesture_params->GetGestureType());
  SyntheticGesturePacket packet_in;
  packet_in.set_gesture_params(std::move(gesture_params));

  Verify(packet_in);
}

TEST_F(InputParamTraitsTest, SyntheticPinchGestureParams) {
  std::unique_ptr<SyntheticPinchGestureParams> gesture_params(
      new SyntheticPinchGestureParams);
  gesture_params->gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  gesture_params->scale_factor = 2.3f;
  gesture_params->anchor.SetPoint(234, 345);
  gesture_params->relative_pointer_speed_in_pixels_s = 456;
  ASSERT_EQ(SyntheticGestureParams::PINCH_GESTURE,
            gesture_params->GetGestureType());
  SyntheticGesturePacket packet_in;
  packet_in.set_gesture_params(std::move(gesture_params));

  Verify(packet_in);
}

TEST_F(InputParamTraitsTest, SyntheticTapGestureParams) {
  std::unique_ptr<SyntheticTapGestureParams> gesture_params(
      new SyntheticTapGestureParams);
  gesture_params->gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  gesture_params->position.SetPoint(798, 233);
  gesture_params->duration_ms = 13;
  ASSERT_EQ(SyntheticGestureParams::TAP_GESTURE,
            gesture_params->GetGestureType());
  SyntheticGesturePacket packet_in;
  packet_in.set_gesture_params(std::move(gesture_params));

  Verify(packet_in);
}

TEST_F(InputParamTraitsTest, SyntheticPointerActionParamsMove) {
  std::unique_ptr<SyntheticPointerActionParams> gesture_params(
      new SyntheticPointerActionParams(
          SyntheticPointerActionParams::PointerActionType::MOVE));
  gesture_params->gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  gesture_params->set_position(gfx::PointF(356, 287));
  gesture_params->set_index(0);
  ASSERT_EQ(SyntheticGestureParams::POINTER_ACTION,
            gesture_params->GetGestureType());
  SyntheticGesturePacket packet_in;
  packet_in.set_gesture_params(std::move(gesture_params));
  Verify(packet_in);
}

TEST_F(InputParamTraitsTest, SyntheticPointerActionParamsRelease) {
  std::unique_ptr<SyntheticPointerActionParams> gesture_params(
      new SyntheticPointerActionParams(
          SyntheticPointerActionParams::PointerActionType::RELEASE));
  gesture_params->gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  gesture_params->set_index(0);
  ASSERT_EQ(SyntheticGestureParams::POINTER_ACTION,
            gesture_params->GetGestureType());
  SyntheticGesturePacket packet_in;
  packet_in.set_gesture_params(std::move(gesture_params));
  Verify(packet_in);
}

TEST_F(InputParamTraitsTest, SyntheticPointerActionParamsProcess) {
  std::unique_ptr<SyntheticPointerActionParams> gesture_params(
      new SyntheticPointerActionParams(
          SyntheticPointerActionParams::PointerActionType::PROCESS));
  gesture_params->gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  ASSERT_EQ(SyntheticGestureParams::POINTER_ACTION,
            gesture_params->GetGestureType());
  SyntheticGesturePacket packet_in;
  packet_in.set_gesture_params(std::move(gesture_params));
  Verify(packet_in);
}

}  // namespace
}  // namespace content
