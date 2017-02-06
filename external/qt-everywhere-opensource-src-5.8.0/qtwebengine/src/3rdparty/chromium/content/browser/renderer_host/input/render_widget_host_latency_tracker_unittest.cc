// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/histogram_tester.h"
#include "content/browser/renderer_host/input/render_widget_host_latency_tracker.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using blink::WebInputEvent;
using testing::ElementsAre;

namespace content {
namespace {

void AddFakeComponents(const RenderWidgetHostLatencyTracker& tracker,
                       ui::LatencyInfo* latency) {
  latency->AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_UI_COMPONENT, 0, 0, base::TimeTicks::Now(), 1);
  latency->AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 0, 0,
      base::TimeTicks::Now(), 1);
  latency->AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, 0, 0,
      base::TimeTicks::Now(), 1);
  latency->AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
      tracker.latency_component_id(), 0, base::TimeTicks::Now(), 1);
  latency->AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT, 0, 0,
      base::TimeTicks::Now(), 1);
  latency->AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_BROWSER_RECEIVED_RENDERER_SWAP_COMPONENT, 0, 0,
      base::TimeTicks::Now(), 1);
}

void AddRenderingScheduledComponent(ui::LatencyInfo* latency, bool main) {
  if (main) {
    latency->AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT, 0, 0,
        base::TimeTicks::Now(), 1);

  } else {
  latency->AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT, 0, 0,
      base::TimeTicks::Now(), 1);
  }
}

}  // namespace

class RenderWidgetHostLatencyTrackerTest : public testing::Test {
 public:
  RenderWidgetHostLatencyTrackerTest() {
    tracker_.Initialize(kTestRoutingId, kTestProcessId);
    ResetHistograms();
  }

  ::testing::AssertionResult HistogramSizeEq(const char* histogram_name,
                                             int size) {
    uint64_t histogram_size =
        histogram_tester_->GetAllSamples(histogram_name).size();
    if (static_cast<uint64_t>(size) == histogram_size) {
      return ::testing::AssertionSuccess();
    } else {
      return ::testing::AssertionFailure() << histogram_name << " expected "
                                           << size << " entries, but had "
                                           << histogram_size;
    }
  }

  RenderWidgetHostLatencyTracker* tracker() { return &tracker_; }
  void ResetHistograms() {
    histogram_tester_.reset(new base::HistogramTester());
  }

  const base::HistogramTester& histogram_tester() {
    return *histogram_tester_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostLatencyTrackerTest);
  const int kTestRoutingId = 3;
  const int kTestProcessId = 1;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  RenderWidgetHostLatencyTracker tracker_;
};

TEST_F(RenderWidgetHostLatencyTrackerTest, TestHistograms) {
  for (bool rendering_on_main : { false, true }) {
    ResetHistograms();
    {
      auto scroll = SyntheticWebGestureEventBuilder::BuildScrollUpdate(
          5.f, -5.f, 0, blink::WebGestureDeviceTouchscreen);
      scroll.timeStampSeconds =
          (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
      ui::LatencyInfo scroll_latency;
      AddFakeComponents(*tracker(), &scroll_latency);
      AddRenderingScheduledComponent(&scroll_latency, rendering_on_main);
      tracker()->OnInputEvent(scroll, &scroll_latency);
      EXPECT_TRUE(scroll_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
          tracker()->latency_component_id(), nullptr));
      EXPECT_TRUE(scroll_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0, nullptr));
      EXPECT_EQ(1U, scroll_latency.input_coordinates_size());
      tracker()->OnInputEventAck(scroll, &scroll_latency,
                                 INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    }

    {
      auto wheel = SyntheticWebMouseWheelEventBuilder::Build(
          blink::WebMouseWheelEvent::PhaseChanged);
      wheel.timeStampSeconds =
          (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
      ui::LatencyInfo wheel_latency;
      AddFakeComponents(*tracker(), &wheel_latency);
      AddRenderingScheduledComponent(&wheel_latency, rendering_on_main);
      tracker()->OnInputEvent(wheel, &wheel_latency);
      EXPECT_TRUE(wheel_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
          tracker()->latency_component_id(), nullptr));
      EXPECT_TRUE(wheel_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0, nullptr));
      EXPECT_EQ(1U, wheel_latency.input_coordinates_size());
      tracker()->OnInputEventAck(wheel, &wheel_latency,
                                 INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    }

    {
      SyntheticWebTouchEvent touch;
      touch.PressPoint(0, 0);
      touch.PressPoint(1, 1);
      ui::LatencyInfo touch_latency;
      AddFakeComponents(*tracker(), &touch_latency);
      AddRenderingScheduledComponent(&touch_latency, rendering_on_main);
      tracker()->OnInputEvent(touch, &touch_latency);
      EXPECT_TRUE(touch_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
          tracker()->latency_component_id(), nullptr));
      EXPECT_TRUE(touch_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0, nullptr));
      EXPECT_EQ(2U, touch_latency.input_coordinates_size());
      tracker()->OnInputEventAck(touch, &touch_latency,
                                 INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
      tracker()->OnFrameSwapped(touch_latency);
    }

    EXPECT_TRUE(HistogramSizeEq("Event.Latency.Browser.WheelUI", 1));
    EXPECT_TRUE(HistogramSizeEq("Event.Latency.Browser.TouchUI", 1));
    EXPECT_TRUE(HistogramSizeEq("Event.Latency.Browser.WheelAcked", 1));
    EXPECT_TRUE(HistogramSizeEq("Event.Latency.Browser.TouchAcked", 1));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.TouchToFirstScrollUpdateSwapBegin", 1));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.TouchToScrollUpdateSwapBegin", 1));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollUpdate.TouchToHandled_Main",
                        rendering_on_main ? 1 : 0));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollUpdate.TouchToHandled_Impl",
                        rendering_on_main ? 0 : 1));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollUpdate.HandledToRendererSwap_Main",
                        rendering_on_main ? 1 : 0));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollUpdate.HandledToRendererSwap_Impl",
                        rendering_on_main ? 0 : 1));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.RendererSwapToBrowserNotified", 1));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.BrowserNotifiedToBeforeGpuSwap", 1));
    EXPECT_TRUE(HistogramSizeEq("Event.Latency.ScrollUpdate.GpuSwap", 1));
  }
}

TEST_F(RenderWidgetHostLatencyTrackerTest,
       LatencyTerminatedOnAckIfRenderingNotScheduled) {
  {
    auto scroll = SyntheticWebGestureEventBuilder::BuildScrollBegin(
        5.f, -5.f, blink::WebGestureDeviceTouchscreen);
    ui::LatencyInfo scroll_latency;
    AddFakeComponents(*tracker(), &scroll_latency);
    // Don't include the rendering schedule component, since we're testing the
    // case where rendering isn't scheduled.
    tracker()->OnInputEvent(scroll, &scroll_latency);
    tracker()->OnInputEventAck(scroll, &scroll_latency,
                               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_TRUE(scroll_latency.FindLatency(
        ui::INPUT_EVENT_LATENCY_TERMINATED_GESTURE_COMPONENT, 0, nullptr));
    EXPECT_TRUE(scroll_latency.terminated());
  }

  {
    auto wheel = SyntheticWebMouseWheelEventBuilder::Build(
        blink::WebMouseWheelEvent::PhaseChanged);
    ui::LatencyInfo wheel_latency;
    AddFakeComponents(*tracker(), &wheel_latency);
    tracker()->OnInputEvent(wheel, &wheel_latency);
    tracker()->OnInputEventAck(wheel, &wheel_latency,
                               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_TRUE(wheel_latency.FindLatency(
        ui::INPUT_EVENT_LATENCY_TERMINATED_MOUSE_WHEEL_COMPONENT, 0, nullptr));
    EXPECT_TRUE(wheel_latency.terminated());
  }

  {
    SyntheticWebTouchEvent touch;
    touch.PressPoint(0, 0);
    ui::LatencyInfo touch_latency;
    AddFakeComponents(*tracker(), &touch_latency);
    tracker()->OnInputEvent(touch, &touch_latency);
    tracker()->OnInputEventAck(touch, &touch_latency,
                               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_TRUE(touch_latency.FindLatency(
        ui::INPUT_EVENT_LATENCY_TERMINATED_TOUCH_COMPONENT, 0, nullptr));
    EXPECT_TRUE(touch_latency.terminated());
    tracker()->OnFrameSwapped(touch_latency);
  }

  {
    auto mouse_move = SyntheticWebMouseEventBuilder::Build(
        blink::WebMouseEvent::MouseMove);
    ui::LatencyInfo mouse_latency;
    AddFakeComponents(*tracker(), &mouse_latency);
    tracker()->OnInputEvent(mouse_move, &mouse_latency);
    tracker()->OnInputEventAck(mouse_move, &mouse_latency,
                               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_TRUE(mouse_latency.FindLatency(
        ui::INPUT_EVENT_LATENCY_TERMINATED_MOUSE_COMPONENT, 0, nullptr));
    EXPECT_TRUE(mouse_latency.terminated());
  }

  {
    auto key_event = SyntheticWebKeyboardEventBuilder::Build(
        blink::WebKeyboardEvent::Char);
    ui::LatencyInfo key_latency;
    AddFakeComponents(*tracker(), &key_latency);
    tracker()->OnInputEvent(key_event, &key_latency);
    tracker()->OnInputEventAck(key_event, &key_latency,
                               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_TRUE(key_latency.FindLatency(
        ui::INPUT_EVENT_LATENCY_TERMINATED_KEYBOARD_COMPONENT, 0, nullptr));
    EXPECT_TRUE(key_latency.terminated());
  }

  EXPECT_TRUE(HistogramSizeEq("Event.Latency.Browser.WheelUI", 1));
  EXPECT_TRUE(HistogramSizeEq("Event.Latency.Browser.TouchUI", 1));
  EXPECT_TRUE(HistogramSizeEq("Event.Latency.Browser.WheelAcked", 1));
  EXPECT_TRUE(HistogramSizeEq("Event.Latency.Browser.TouchAcked", 1));
  EXPECT_TRUE(
      HistogramSizeEq("Event.Latency.TouchToFirstScrollUpdateSwapBegin", 1));
  EXPECT_TRUE(HistogramSizeEq("Event.Latency.TouchToScrollUpdateSwapBegin", 1));
  EXPECT_TRUE(
      HistogramSizeEq("Event.Latency.ScrollUpdate.TouchToHandled_Main", 0));
  EXPECT_TRUE(
      HistogramSizeEq("Event.Latency.ScrollUpdate.TouchToHandled_Impl", 0));
  EXPECT_TRUE(HistogramSizeEq(
      "Event.Latency.ScrollUpdate.HandledToRendererSwap_Main", 0));
  EXPECT_TRUE(HistogramSizeEq(
      "Event.Latency.ScrollUpdate.HandledToRendererSwap_Impl", 0));
  EXPECT_TRUE(HistogramSizeEq(
      "Event.Latency.ScrollUpdate.RendererSwapToBrowserNotified", 0));
  EXPECT_TRUE(HistogramSizeEq(
      "Event.Latency.ScrollUpdate.BrowserNotifiedToBeforeGpuSwap", 0));
  EXPECT_TRUE(HistogramSizeEq("Event.Latency.ScrollUpdate.GpuSwap", 0));
}

TEST_F(RenderWidgetHostLatencyTrackerTest, InputCoordinatesPopulated) {
  {
    auto event =
        SyntheticWebMouseWheelEventBuilder::Build(0, 0, -5, 0, 0, true);
    event.x = 100;
    event.y = 200;
    ui::LatencyInfo latency_info;
    tracker()->OnInputEvent(event, &latency_info);
    EXPECT_EQ(1u, latency_info.input_coordinates_size());
    EXPECT_EQ(100, latency_info.input_coordinates()[0].x());
    EXPECT_EQ(200, latency_info.input_coordinates()[0].y());
  }

  {
    auto event = SyntheticWebMouseEventBuilder::Build(WebInputEvent::MouseMove);
    event.x = 300;
    event.y = 400;
    ui::LatencyInfo latency_info;
    tracker()->OnInputEvent(event, &latency_info);
    EXPECT_EQ(1u, latency_info.input_coordinates_size());
    EXPECT_EQ(300, latency_info.input_coordinates()[0].x());
    EXPECT_EQ(400, latency_info.input_coordinates()[0].y());
  }

  {
    auto event = SyntheticWebGestureEventBuilder::Build(
        WebInputEvent::GestureScrollBegin, blink::WebGestureDeviceTouchscreen);
    event.x = 500;
    event.y = 600;
    ui::LatencyInfo latency_info;
    tracker()->OnInputEvent(event, &latency_info);
    EXPECT_EQ(1u, latency_info.input_coordinates_size());
    EXPECT_EQ(500, latency_info.input_coordinates()[0].x());
    EXPECT_EQ(600, latency_info.input_coordinates()[0].y());
  }

  {
    SyntheticWebTouchEvent event;
    event.PressPoint(700, 800);
    event.PressPoint(900, 1000);
    event.PressPoint(1100, 1200);  // LatencyInfo only holds two coordinates.
    ui::LatencyInfo latency_info;
    tracker()->OnInputEvent(event, &latency_info);
    EXPECT_EQ(2u, latency_info.input_coordinates_size());
    EXPECT_EQ(700, latency_info.input_coordinates()[0].x());
    EXPECT_EQ(800, latency_info.input_coordinates()[0].y());
    EXPECT_EQ(900, latency_info.input_coordinates()[1].x());
    EXPECT_EQ(1000, latency_info.input_coordinates()[1].y());
  }

  {
    NativeWebKeyboardEvent event;
    event.type = blink::WebKeyboardEvent::KeyDown;
    ui::LatencyInfo latency_info;
    tracker()->OnInputEvent(event, &latency_info);
    EXPECT_EQ(0u, latency_info.input_coordinates_size());
  }
}

TEST_F(RenderWidgetHostLatencyTrackerTest, ScrollLatency) {
  auto scroll_begin = SyntheticWebGestureEventBuilder::BuildScrollBegin(
      5, -5, blink::WebGestureDeviceTouchscreen);
  ui::LatencyInfo scroll_latency;
  scroll_latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0,
                                  0);
  tracker()->OnInputEvent(scroll_begin, &scroll_latency);
  EXPECT_TRUE(
      scroll_latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                 tracker()->latency_component_id(), nullptr));
  EXPECT_EQ(2U, scroll_latency.latency_components().size());

  // The first GestureScrollUpdate should be provided with
  // INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT.
  auto first_scroll_update = SyntheticWebGestureEventBuilder::BuildScrollUpdate(
      5.f, -5.f, 0, blink::WebGestureDeviceTouchscreen);
  scroll_latency = ui::LatencyInfo();
  scroll_latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0,
                                  0);
  tracker()->OnInputEvent(first_scroll_update, &scroll_latency);
  EXPECT_TRUE(
      scroll_latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                 tracker()->latency_component_id(), nullptr));
  EXPECT_TRUE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
      tracker()->latency_component_id(), nullptr));
  EXPECT_FALSE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
      tracker()->latency_component_id(), nullptr));
  EXPECT_EQ(3U, scroll_latency.latency_components().size());

  // Subsequent GestureScrollUpdates should be provided with
  // INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT.
  auto scroll_update = SyntheticWebGestureEventBuilder::BuildScrollUpdate(
      -5.f, 5.f, 0, blink::WebGestureDeviceTouchscreen);
  scroll_latency = ui::LatencyInfo();
  scroll_latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0,
                                  0);
  tracker()->OnInputEvent(scroll_update, &scroll_latency);
  EXPECT_TRUE(
      scroll_latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                 tracker()->latency_component_id(), nullptr));
  EXPECT_FALSE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
      tracker()->latency_component_id(), nullptr));
  EXPECT_TRUE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
      tracker()->latency_component_id(), nullptr));
  EXPECT_EQ(3U, scroll_latency.latency_components().size());
}

TEST_F(RenderWidgetHostLatencyTrackerTest, TouchBlockingAndQueueingTime) {
  // These numbers are sensitive to where the histogram buckets are.
  int touchstart_timestamps_ms[] = {11, 25, 35};
  int touchmove_timestamps_ms[] = {1, 5, 12};
  int touchend_timestamps_ms[] = {3, 8, 12};

  for (InputEventAckState blocking :
       {INPUT_EVENT_ACK_STATE_NOT_CONSUMED, INPUT_EVENT_ACK_STATE_CONSUMED}) {
    SyntheticWebTouchEvent event;
    {
      // Touch start.
      event.PressPoint(1, 1);

      ui::LatencyInfo latency;
      tracker()->OnInputEvent(event, &latency);

      ui::LatencyInfo fake_latency;
      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
          tracker()->latency_component_id(), 0,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchstart_timestamps_ms[0]),
          1);

      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT, 0, 0,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchstart_timestamps_ms[1]),
          1);

      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT, 0, 0,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchstart_timestamps_ms[2]),
          1);

      // Call ComputeInputLatencyHistograms directly to avoid OnInputEventAck
      // overwriting components.
      tracker()->ComputeInputLatencyHistograms(
          event.type, tracker()->latency_component_id(), fake_latency,
          blocking);

      tracker()->OnInputEventAck(event, &latency,
                                 INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    }

    {
      // Touch move.
      ui::LatencyInfo latency;
      event.MovePoint(0, 20, 20);
      tracker()->OnInputEvent(event, &latency);

      EXPECT_TRUE(latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0, nullptr));
      EXPECT_TRUE(
          latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                   tracker()->latency_component_id(), nullptr));

      EXPECT_EQ(2U, latency.latency_components().size());

      ui::LatencyInfo fake_latency;
      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
          tracker()->latency_component_id(), 0,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchmove_timestamps_ms[0]),
          1);

      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT, 0, 0,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchmove_timestamps_ms[1]),
          1);

      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT, 0, 0,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchmove_timestamps_ms[2]),
          1);

      // Call ComputeInputLatencyHistograms directly to avoid OnInputEventAck
      // overwriting components.
      tracker()->ComputeInputLatencyHistograms(
          event.type, tracker()->latency_component_id(), fake_latency,
          blocking);
    }

    {
      // Touch end.
      ui::LatencyInfo latency;
      event.ReleasePoint(0);
      tracker()->OnInputEvent(event, &latency);

      EXPECT_TRUE(latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0, nullptr));
      EXPECT_TRUE(
          latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                   tracker()->latency_component_id(), nullptr));

      EXPECT_EQ(2U, latency.latency_components().size());

      ui::LatencyInfo fake_latency;
      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
          tracker()->latency_component_id(), 0,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchend_timestamps_ms[0]),
          1);

      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT, 0, 0,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchend_timestamps_ms[1]),
          1);

      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT, 0, 0,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchend_timestamps_ms[2]),
          1);

      // Call ComputeInputLatencyHistograms directly to avoid OnInputEventAck
      // overwriting components.
      tracker()->ComputeInputLatencyHistograms(
          event.type, tracker()->latency_component_id(), fake_latency,
          blocking);
    }
  }

  // Touch start.
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Event.Latency.QueueingTime.TouchStartDefaultPrevented"),
      ElementsAre(Bucket(
          touchstart_timestamps_ms[1] - touchstart_timestamps_ms[0], 1)));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Event.Latency.QueueingTime.TouchStartDefaultAllowed"),
      ElementsAre(Bucket(
          touchstart_timestamps_ms[1] - touchstart_timestamps_ms[0], 1)));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Event.Latency.BlockingTime.TouchStartDefaultPrevented"),
      ElementsAre(Bucket(
          touchstart_timestamps_ms[2] - touchstart_timestamps_ms[1], 1)));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Event.Latency.BlockingTime.TouchStartDefaultAllowed"),
      ElementsAre(Bucket(
          touchstart_timestamps_ms[2] - touchstart_timestamps_ms[1], 1)));

  // Touch move.
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.QueueingTime.TouchMoveDefaultPrevented"),
              ElementsAre(Bucket(
                  touchmove_timestamps_ms[1] - touchmove_timestamps_ms[0], 1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.QueueingTime.TouchMoveDefaultAllowed"),
              ElementsAre(Bucket(
                  touchmove_timestamps_ms[1] - touchmove_timestamps_ms[0], 1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.BlockingTime.TouchMoveDefaultPrevented"),
              ElementsAre(Bucket(
                  touchmove_timestamps_ms[2] - touchmove_timestamps_ms[1], 1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.BlockingTime.TouchMoveDefaultAllowed"),
              ElementsAre(Bucket(
                  touchmove_timestamps_ms[2] - touchmove_timestamps_ms[1], 1)));

  // Touch end.
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.QueueingTime.TouchEndDefaultPrevented"),
              ElementsAre(Bucket(
                  touchend_timestamps_ms[1] - touchend_timestamps_ms[0], 1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.QueueingTime.TouchEndDefaultAllowed"),
              ElementsAre(Bucket(
                  touchend_timestamps_ms[1] - touchend_timestamps_ms[0], 1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.BlockingTime.TouchEndDefaultPrevented"),
              ElementsAre(Bucket(
                  touchend_timestamps_ms[2] - touchend_timestamps_ms[1], 1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.BlockingTime.TouchEndDefaultAllowed"),
              ElementsAre(Bucket(
                  touchend_timestamps_ms[2] - touchend_timestamps_ms[1], 1)));
}

}  // namespace content
