// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/mojo/traits_test_service.mojom.h"

namespace ui {

namespace {

class StructTraitsTest : public testing::Test, public mojom::TraitsTestService {
 public:
  StructTraitsTest() {}

 protected:
  mojom::TraitsTestServicePtr GetTraitsTestProxy() {
    return traits_test_bindings_.CreateInterfacePtrAndBind(this);
  }

 private:
  // TraitsTestService:
  void EchoEvent(std::unique_ptr<ui::Event> e,
                 const EchoEventCallback& callback) override {
    callback.Run(std::move(e));
  }

  void EchoLatencyComponent(
      const LatencyInfo::LatencyComponent& l,
      const EchoLatencyComponentCallback& callback) override {
    callback.Run(l);
  }

  void EchoLatencyComponentId(
      const std::pair<LatencyComponentType, int64_t>& id,
      const EchoLatencyComponentIdCallback& callback) override {
    callback.Run(id);
  }

  void EchoLatencyInfo(const LatencyInfo& info,
                       const EchoLatencyInfoCallback& callback) override {
    callback.Run(info);
  }

  base::MessageLoop loop_;
  mojo::BindingSet<TraitsTestService> traits_test_bindings_;
  DISALLOW_COPY_AND_ASSIGN(StructTraitsTest);
};

}  // namespace

TEST_F(StructTraitsTest, LatencyComponent) {
  const int64_t sequence_number = 13371337;
  const base::TimeTicks event_time = base::TimeTicks::Now();
  const uint32_t event_count = 1234;
  LatencyInfo::LatencyComponent input;
  input.sequence_number = sequence_number;
  input.event_time = event_time;
  input.event_count = event_count;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  LatencyInfo::LatencyComponent output;
  proxy->EchoLatencyComponent(input, &output);
  EXPECT_EQ(sequence_number, output.sequence_number);
  EXPECT_EQ(event_time, output.event_time);
  EXPECT_EQ(event_count, output.event_count);
}

TEST_F(StructTraitsTest, LatencyComponentId) {
  const LatencyComponentType type =
      INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT;
  const int64_t id = 1337;
  std::pair<LatencyComponentType, int64_t> input(type, id);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  std::pair<LatencyComponentType, int64_t> output;
  proxy->EchoLatencyComponentId(input, &output);
  EXPECT_EQ(type, output.first);
  EXPECT_EQ(id, output.second);
}

TEST_F(StructTraitsTest, LatencyInfo) {
  LatencyInfo latency;
  ASSERT_FALSE(latency.terminated());
  ASSERT_EQ(0u, latency.input_coordinates_size());
  latency.AddLatencyNumber(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 1234, 0);
  latency.AddLatencyNumber(INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, 1234, 100);
  latency.AddLatencyNumber(INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT,
                           1234, 0);
  EXPECT_TRUE(latency.AddInputCoordinate(gfx::PointF(100, 200)));
  EXPECT_TRUE(latency.AddInputCoordinate(gfx::PointF(101, 201)));
  // Up to 2 InputCoordinate is allowed.
  EXPECT_FALSE(latency.AddInputCoordinate(gfx::PointF(102, 202)));

  EXPECT_EQ(100, latency.trace_id());
  EXPECT_TRUE(latency.terminated());
  EXPECT_EQ(2u, latency.input_coordinates_size());

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  LatencyInfo output;
  proxy->EchoLatencyInfo(latency, &output);

  EXPECT_EQ(latency.trace_id(), output.trace_id());
  EXPECT_EQ(latency.terminated(), output.terminated());
  EXPECT_EQ(latency.input_coordinates_size(), output.input_coordinates_size());
  for (size_t i = 0; i < latency.input_coordinates_size(); i++) {
    EXPECT_EQ(latency.input_coordinates()[i].x(),
              output.input_coordinates()[i].x());
    EXPECT_EQ(latency.input_coordinates()[i].y(),
              output.input_coordinates()[i].y());
  }

  EXPECT_TRUE(output.FindLatency(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 1234,
                                 nullptr));

  LatencyInfo::LatencyComponent rwh_comp;
  EXPECT_TRUE(output.FindLatency(INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, 1234,
                                 &rwh_comp));
  EXPECT_EQ(100, rwh_comp.sequence_number);
  EXPECT_EQ(1u, rwh_comp.event_count);

  EXPECT_TRUE(output.FindLatency(
      INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 1234, nullptr));
}

TEST_F(StructTraitsTest, KeyEvent) {
  KeyEvent kTestData[] = {
      {ET_KEY_PRESSED, VKEY_RETURN, EF_CONTROL_DOWN},
      {ET_KEY_PRESSED, VKEY_MENU, EF_ALT_DOWN},
      {ET_KEY_RELEASED, VKEY_SHIFT, EF_SHIFT_DOWN},
      {ET_KEY_RELEASED, VKEY_MENU, EF_ALT_DOWN},
      {ET_KEY_PRESSED, VKEY_A, ui::DomCode::US_A, EF_NONE},
      {ET_KEY_PRESSED, VKEY_B, ui::DomCode::US_B,
       EF_CONTROL_DOWN | EF_ALT_DOWN},
      {'\x12', VKEY_2, EF_CONTROL_DOWN},
      {'Z', VKEY_Z, EF_CAPS_LOCK_ON},
      {'z', VKEY_Z, EF_NONE},
  };

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  for (size_t i = 0; i < arraysize(kTestData); i++) {
    std::unique_ptr<Event> output;
    proxy->EchoEvent(Event::Clone(kTestData[i]), &output);
    EXPECT_TRUE(output->IsKeyEvent());

    const KeyEvent* output_key_event = output->AsKeyEvent();
    EXPECT_EQ(kTestData[i].type(), output_key_event->type());
    EXPECT_EQ(kTestData[i].flags(), output_key_event->flags());
    EXPECT_EQ(kTestData[i].GetCharacter(), output_key_event->GetCharacter());
    EXPECT_EQ(kTestData[i].GetUnmodifiedText(),
              output_key_event->GetUnmodifiedText());
    EXPECT_EQ(kTestData[i].GetText(), output_key_event->GetText());
    EXPECT_EQ(kTestData[i].is_char(), output_key_event->is_char());
    EXPECT_EQ(kTestData[i].is_repeat(), output_key_event->is_repeat());
    EXPECT_EQ(kTestData[i].GetConflatedWindowsKeyCode(),
              output_key_event->GetConflatedWindowsKeyCode());
    EXPECT_EQ(kTestData[i].code(), output_key_event->code());
  }
}

TEST_F(StructTraitsTest, PointerEvent) {
  PointerEvent kTestData[] = {
      // Mouse pointer events:
      {ET_POINTER_DOWN, gfx::Point(10, 10), gfx::Point(20, 30), EF_NONE,
       PointerEvent::kMousePointerId,
       PointerDetails(EventPointerType::POINTER_TYPE_MOUSE), base::TimeTicks()},
      {ET_POINTER_MOVED, gfx::Point(1, 5), gfx::Point(5, 1),
       EF_LEFT_MOUSE_BUTTON, PointerEvent::kMousePointerId,
       PointerDetails(EventPointerType::POINTER_TYPE_MOUSE), base::TimeTicks()},
      {ET_POINTER_UP, gfx::Point(411, 130), gfx::Point(20, 30),
       EF_MIDDLE_MOUSE_BUTTON | EF_RIGHT_MOUSE_BUTTON,
       PointerEvent::kMousePointerId,
       PointerDetails(EventPointerType::POINTER_TYPE_MOUSE), base::TimeTicks()},
      {ET_POINTER_EXITED, gfx::Point(10, 10), gfx::Point(20, 30),
       EF_BACK_MOUSE_BUTTON, PointerEvent::kMousePointerId,
       PointerDetails(EventPointerType::POINTER_TYPE_MOUSE), base::TimeTicks()},

      // Touch pointer events:
      {ET_POINTER_DOWN, gfx::Point(10, 10), gfx::Point(20, 30), EF_NONE, 1,
       PointerDetails(EventPointerType::POINTER_TYPE_TOUCH, 1.0, 2.0, 3.0, 4.0,
                      5.0),
       base::TimeTicks()},
      {ET_POINTER_CANCELLED, gfx::Point(120, 120), gfx::Point(2, 3), EF_NONE, 2,
       PointerDetails(EventPointerType::POINTER_TYPE_TOUCH, 5.5, 4.5, 3.5, 2.5,
                      0.5),
       base::TimeTicks()},
  };

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  for (size_t i = 0; i < arraysize(kTestData); i++) {
    std::unique_ptr<Event> output;
    proxy->EchoEvent(Event::Clone(kTestData[i]), &output);
    EXPECT_TRUE(output->IsPointerEvent());

    const PointerEvent* output_ptr_event = output->AsPointerEvent();
    EXPECT_EQ(kTestData[i].type(), output_ptr_event->type());
    EXPECT_EQ(kTestData[i].flags(), output_ptr_event->flags());
    EXPECT_EQ(kTestData[i].location(), output_ptr_event->location());
    EXPECT_EQ(kTestData[i].root_location(), output_ptr_event->root_location());
    EXPECT_EQ(kTestData[i].pointer_id(), output_ptr_event->pointer_id());
    EXPECT_EQ(kTestData[i].pointer_details(),
              output_ptr_event->pointer_details());
  }
}

TEST_F(StructTraitsTest, MouseWheelEvent) {
  MouseWheelEvent kTestData[] = {
      {gfx::Vector2d(11, 15), gfx::Point(3, 4), gfx::Point(40, 30),
       base::TimeTicks(), EF_LEFT_MOUSE_BUTTON, EF_LEFT_MOUSE_BUTTON},
      {gfx::Vector2d(-5, 3), gfx::Point(40, 3), gfx::Point(4, 0),
       base::TimeTicks(), EF_MIDDLE_MOUSE_BUTTON | EF_RIGHT_MOUSE_BUTTON,
       EF_MIDDLE_MOUSE_BUTTON | EF_RIGHT_MOUSE_BUTTON},
      {gfx::Vector2d(1, 0), gfx::Point(3, 4), gfx::Point(40, 30),
       base::TimeTicks(), EF_NONE, EF_NONE},
  };

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  for (size_t i = 0; i < arraysize(kTestData); i++) {
    std::unique_ptr<Event> output;
    proxy->EchoEvent(Event::Clone(kTestData[i]), &output);
    EXPECT_TRUE(output->IsMouseWheelEvent());

    const MouseWheelEvent* output_wheel_event = output->AsMouseWheelEvent();
    EXPECT_EQ(kTestData[i].type(), output_wheel_event->type());
    EXPECT_EQ(kTestData[i].flags(), output_wheel_event->flags());
    EXPECT_EQ(kTestData[i].location(), output_wheel_event->location());
    EXPECT_EQ(kTestData[i].root_location(),
              output_wheel_event->root_location());
    EXPECT_EQ(kTestData[i].offset(), output_wheel_event->offset());
  }
}

}  // namespace ui
