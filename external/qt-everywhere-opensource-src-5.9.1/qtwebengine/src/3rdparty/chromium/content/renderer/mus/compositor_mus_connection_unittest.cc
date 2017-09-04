// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mus/compositor_mus_connection.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "content/common/input/input_event_ack.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/public/test/mock_render_thread.h"
#include "content/renderer/input/input_handler_manager.h"
#include "content/renderer/input/input_handler_manager_client.h"
#include "content/renderer/input/render_widget_input_handler.h"
#include "content/renderer/mus/render_widget_mus_connection.h"
#include "content/renderer/render_widget.h"
#include "content/test/fake_compositor_dependencies.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/ui/public/cpp/tests/test_window.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/scheduler/test/fake_renderer_scheduler.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/events/event_utils.h"
#include "ui/events/mojo/event.mojom.h"
#include "ui/events/mojo/event_constants.mojom.h"
#include "ui/events/mojo/keyboard_codes.mojom.h"

using ui::mojom::EventResult;

namespace content {

namespace {

// Wrapper for the callback provided to
// CompositorMusConnection:OnWindowInputEvent. This tracks whether the it was
// called, along with the result.
class TestCallback : public base::RefCounted<TestCallback> {
 public:
  TestCallback() : called_(false), result_(EventResult::UNHANDLED) {}

  bool called() { return called_; }
  EventResult result() { return result_; }

  void ResultCallback(EventResult result) {
    called_ = true;
    result_ = result;
  }

 private:
  friend class base::RefCounted<TestCallback>;

  ~TestCallback() {}

  bool called_;
  EventResult result_;

  DISALLOW_COPY_AND_ASSIGN(TestCallback);
};

// Allows for overriding the behaviour of HandleInputEvent, to simulate input
// handlers which consume events before they are sent to the renderer.
class TestInputHandlerManager : public InputHandlerManager {
 public:
  TestInputHandlerManager(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      InputHandlerManagerClient* client,
      blink::scheduler::RendererScheduler* renderer_scheduler)
      : InputHandlerManager(task_runner, client, nullptr, renderer_scheduler),
        override_result_(false),
        result_(InputEventAckState::INPUT_EVENT_ACK_STATE_UNKNOWN) {}
  ~TestInputHandlerManager() override {}

  // Stops overriding the behaviour of HandleInputEvent
  void ClearHandleInputEventOverride();

  // Overrides the behaviour of HandleInputEvent, returing |result|.
  void SetHandleInputEventResult(InputEventAckState result);

  // InputHandlerManager:
  void HandleInputEvent(int routing_id,
                        ui::ScopedWebInputEvent input_event,
                        const ui::LatencyInfo& latency_info,
                        const InputEventAckStateCallback& callback) override;

 private:
  // If true InputHandlerManager::HandleInputEvent is not called.
  bool override_result_;

  // The result to return in HandleInputEvent if |override_result_|.
  InputEventAckState result_;

  DISALLOW_COPY_AND_ASSIGN(TestInputHandlerManager);
};

void TestInputHandlerManager::ClearHandleInputEventOverride() {
  override_result_ = false;
}

void TestInputHandlerManager::SetHandleInputEventResult(
    InputEventAckState result) {
  override_result_ = true;
  result_ = result;
}

void TestInputHandlerManager::HandleInputEvent(
    int routing_id,
    ui::ScopedWebInputEvent input_event,
    const ui::LatencyInfo& latency_info,
    const InputEventAckStateCallback& callback) {
  if (override_result_) {
    callback.Run(result_, std::move(input_event), latency_info, nullptr);
    return;
  }
  InputHandlerManager::HandleInputEvent(routing_id, std::move(input_event),
                                        latency_info, callback);
}

// Empty implementation of InputHandlerManagerClient.
class TestInputHandlerManagerClient : public InputHandlerManagerClient {
 public:
  TestInputHandlerManagerClient() {}
  ~TestInputHandlerManagerClient() override{};

  // InputHandlerManagerClient:
  void SetInputHandlerManager(
      InputHandlerManager* input_handler_manager) override {}
  void RegisterRoutingID(int routing_id) override {}
  void UnregisterRoutingID(int routing_id) override {}
  void DidOverscroll(int routing_id,
                     const ui::DidOverscrollParams& params) override {}
  void DidStopFlinging(int routing_id) override {}
  void DispatchNonBlockingEventToMainThread(
      int routing_id,
      ui::ScopedWebInputEvent event,
      const ui::LatencyInfo& latency_info) override {}

  void NotifyInputEventHandled(int routing_id,
                               blink::WebInputEvent::Type type,
                               InputEventAckState ack_result) override {}
  void ProcessRafAlignedInput(int routing_id) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestInputHandlerManagerClient);
};

// Implementation of RenderWidget for testing, performs no initialization.
class TestRenderWidget : public RenderWidget {
 public:
  explicit TestRenderWidget(CompositorDependencies* compositor_deps)
      : RenderWidget(1,
                     compositor_deps,
                     blink::WebPopupTypeNone,
                     ScreenInfo(),
                     true,
                     false,
                     false) {}

 protected:
  ~TestRenderWidget() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestRenderWidget);
};

// Test override of RenderWidgetInputHandler to allow the control of
// HandleInputEvent. This will perform no actions on input until a
// RenderWidgetInputHandlerDelegate is set. Once set this will always ack
// received events.
class TestRenderWidgetInputHandler : public RenderWidgetInputHandler {
 public:
  TestRenderWidgetInputHandler(RenderWidget* render_widget);
  ~TestRenderWidgetInputHandler() override {}

  void set_delegate(RenderWidgetInputHandlerDelegate* delegate) {
    delegate_ = delegate;
  }
  void set_state(InputEventAckState state) { state_ = state; }

  // RenderWidgetInputHandler:
  void HandleInputEvent(const blink::WebInputEvent& input_event,
                        const ui::LatencyInfo& latency_info,
                        InputEventDispatchType dispatch_type) override;

 private:
  // The input delegate which receives event acks.
  RenderWidgetInputHandlerDelegate* delegate_;

  // The result of input handling to send to |delegate_| during the ack.
  InputEventAckState state_;

  DISALLOW_COPY_AND_ASSIGN(TestRenderWidgetInputHandler);
};

TestRenderWidgetInputHandler::TestRenderWidgetInputHandler(
    RenderWidget* render_widget)
    : RenderWidgetInputHandler(render_widget, render_widget),
      delegate_(nullptr),
      state_(InputEventAckState::INPUT_EVENT_ACK_STATE_UNKNOWN) {}

void TestRenderWidgetInputHandler::HandleInputEvent(
    const blink::WebInputEvent& input_event,
    const ui::LatencyInfo& latency_info,
    InputEventDispatchType dispatch_type) {
  if (delegate_) {
    std::unique_ptr<InputEventAck> ack(new InputEventAck(
        InputEventAckSource::COMPOSITOR_THREAD, input_event.type, state_));
    delegate_->OnInputEventAck(std::move(ack));
  }
}

}  // namespace

// Test suite for CompositorMusConnection, this does not setup a full renderer
// environment. This does not establish a connection to a mus server, nor does
// it initialize one.
class CompositorMusConnectionTest : public testing::Test {
 public:
  CompositorMusConnectionTest() {}
  ~CompositorMusConnectionTest() override {}

  // Returns a valid key event, so that it can be converted to a web event by
  // CompositorMusConnection.
  std::unique_ptr<ui::Event> GenerateKeyEvent();

  // Calls CompositorMusConnection::OnWindowInputEvent.
  void OnWindowInputEvent(
      ui::Window* window,
      const ui::Event& event,
      std::unique_ptr<base::Callback<void(EventResult)>>* ack_callback);

  // Confirms the state of pending tasks enqueued on each task runner, and runs
  // until idle.
  void VerifyAndRunQueues(bool main_task_runner_enqueued,
                          bool compositor_task_runner_enqueued);

  CompositorMusConnection* compositor_connection() {
    return compositor_connection_.get();
  }
  RenderWidgetMusConnection* connection() { return connection_; }
  TestInputHandlerManager* input_handler_manager() {
    return input_handler_manager_.get();
  }
  TestRenderWidgetInputHandler* render_widget_input_handler() {
    return render_widget_input_handler_.get();
  }

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

 private:
  // Mocks/Fakes of the testing environment.
  TestInputHandlerManagerClient input_handler_manager_client_;
  FakeCompositorDependencies compositor_dependencies_;
  blink::scheduler::FakeRendererScheduler renderer_scheduler_;
  MockRenderThread render_thread_;
  scoped_refptr<TestRenderWidget> render_widget_;
  mojo::InterfaceRequest<ui::mojom::WindowTreeClient> request_;

  // Not owned, RenderWidgetMusConnection tracks in static state. Cleared during
  // TearDown.
  RenderWidgetMusConnection* connection_;

  // Test versions of task runners, see VerifyAndRunQueues to use in testing.
  scoped_refptr<base::TestSimpleTaskRunner> main_task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> compositor_task_runner_;

  // Actual CompositorMusConnection for testing.
  scoped_refptr<CompositorMusConnection> compositor_connection_;

  // Test implementations, to control input given to |compositor_connection_|.
  std::unique_ptr<TestInputHandlerManager> input_handler_manager_;
  std::unique_ptr<TestRenderWidgetInputHandler> render_widget_input_handler_;

  DISALLOW_COPY_AND_ASSIGN(CompositorMusConnectionTest);
};

std::unique_ptr<ui::Event> CompositorMusConnectionTest::GenerateKeyEvent() {
  return std::unique_ptr<ui::Event>(new ui::KeyEvent(
      ui::ET_KEY_PRESSED, ui::KeyboardCode::VKEY_A, ui::EF_NONE));
}

void CompositorMusConnectionTest::OnWindowInputEvent(
    ui::Window* window,
    const ui::Event& event,
    std::unique_ptr<base::Callback<void(EventResult)>>* ack_callback) {
  compositor_connection_->OnWindowInputEvent(window, event, ack_callback);
}

void CompositorMusConnectionTest::VerifyAndRunQueues(
    bool main_task_runner_enqueued,
    bool compositor_task_runner_enqueued) {
  // Run through the enqueued actions.
  EXPECT_EQ(main_task_runner_enqueued, main_task_runner_->HasPendingTask());
  main_task_runner_->RunUntilIdle();

  EXPECT_EQ(compositor_task_runner_enqueued,
            compositor_task_runner_->HasPendingTask());
  compositor_task_runner_->RunUntilIdle();
}

void CompositorMusConnectionTest::SetUp() {
  testing::Test::SetUp();

  main_task_runner_ = new base::TestSimpleTaskRunner();
  compositor_task_runner_ = new base::TestSimpleTaskRunner();

  input_handler_manager_.reset(new TestInputHandlerManager(
      compositor_task_runner_, &input_handler_manager_client_,
      &renderer_scheduler_));

  const int routing_id = 42;
  compositor_connection_ = new CompositorMusConnection(
      routing_id, main_task_runner_, compositor_task_runner_,
      std::move(request_), input_handler_manager_.get());

  // CompositorMusConnection attempts to create connection to the non-existant
  // server. Clear that.
  compositor_task_runner_->ClearPendingTasks();

  render_widget_ = new TestRenderWidget(&compositor_dependencies_);
  render_widget_input_handler_.reset(
      new TestRenderWidgetInputHandler(render_widget_.get()));
  connection_ = RenderWidgetMusConnection::GetOrCreate(routing_id);
  connection_->SetInputHandler(render_widget_input_handler_.get());
}

void CompositorMusConnectionTest::TearDown() {
  // Clear static state.
  connection_->OnConnectionLost();
  testing::Test::TearDown();
}

// Tests that for events which the renderer will ack, yet not consume, that
// CompositorMusConnection consumes the ack during OnWindowInputEvent, and calls
// it with the correct state once processed.
TEST_F(CompositorMusConnectionTest, NotConsumed) {
  TestRenderWidgetInputHandler* input_handler = render_widget_input_handler();
  input_handler->set_delegate(connection());
  input_handler->set_state(
      InputEventAckState::INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  ui::TestWindow test_window;
  std::unique_ptr<ui::Event> event(GenerateKeyEvent());
  scoped_refptr<TestCallback> test_callback(new TestCallback);
  std::unique_ptr<base::Callback<void(EventResult)>> ack_callback(
      new base::Callback<void(EventResult)>(
          base::Bind(&TestCallback::ResultCallback, test_callback)));

  OnWindowInputEvent(&test_window, *event.get(), &ack_callback);
  // OnWindowInputEvent is expected to clear the callback if it plans on
  // handling the ack.
  EXPECT_FALSE(ack_callback.get());

  VerifyAndRunQueues(true, true);

  // The ack callback should have been called
  EXPECT_TRUE(test_callback->called());
  EXPECT_EQ(EventResult::UNHANDLED, test_callback->result());
}

// Tests that for events which the renderer will ack, and consume, that
// CompositorMusConnection consumes the ack during OnWindowInputEvent, and calls
// it with the correct state once processed.
TEST_F(CompositorMusConnectionTest, Consumed) {
  TestRenderWidgetInputHandler* input_handler = render_widget_input_handler();
  input_handler->set_delegate(connection());
  input_handler->set_state(InputEventAckState::INPUT_EVENT_ACK_STATE_CONSUMED);

  ui::TestWindow test_window;
  std::unique_ptr<ui::Event> event(GenerateKeyEvent());
  scoped_refptr<TestCallback> test_callback(new TestCallback);
  std::unique_ptr<base::Callback<void(EventResult)>> ack_callback(
      new base::Callback<void(EventResult)>(
          base::Bind(&TestCallback::ResultCallback, test_callback)));

  OnWindowInputEvent(&test_window, *event.get(), &ack_callback);
  // OnWindowInputEvent is expected to clear the callback if it plans on
  // handling the ack.
  EXPECT_FALSE(ack_callback.get());

  VerifyAndRunQueues(true, true);

  // The ack callback should have been called
  EXPECT_TRUE(test_callback->called());
  EXPECT_EQ(EventResult::HANDLED, test_callback->result());
}

// Tests that when the RenderWidgetInputHandler does not ack before a new event
// arrives, that only the most recent ack is fired.
TEST_F(CompositorMusConnectionTest, LostAck) {
  ui::TestWindow test_window;
  std::unique_ptr<ui::Event> event1(GenerateKeyEvent());
  scoped_refptr<TestCallback> test_callback1(new TestCallback);
  std::unique_ptr<base::Callback<void(EventResult)>> ack_callback1(
      new base::Callback<void(EventResult)>(
          base::Bind(&TestCallback::ResultCallback, test_callback1)));

  OnWindowInputEvent(&test_window, *event1.get(), &ack_callback1);
  EXPECT_FALSE(ack_callback1.get());
  // When simulating the timeout the ack is never enqueued
  VerifyAndRunQueues(true, false);

  // Setting a delegate will lead to the next event being acked. Having a
  // cleared queue simulates the input handler timing out on an event.
  TestRenderWidgetInputHandler* input_handler = render_widget_input_handler();
  input_handler->set_delegate(connection());
  input_handler->set_state(InputEventAckState::INPUT_EVENT_ACK_STATE_CONSUMED);

  std::unique_ptr<ui::Event> event2(GenerateKeyEvent());
  scoped_refptr<TestCallback> test_callback2(new TestCallback);
  std::unique_ptr<base::Callback<void(EventResult)>> ack_callback2(
      new base::Callback<void(EventResult)>(
          base::Bind(&TestCallback::ResultCallback, test_callback2)));
  OnWindowInputEvent(&test_window, *event2.get(), &ack_callback2);
  EXPECT_FALSE(ack_callback2.get());

  VerifyAndRunQueues(true, true);

  // Only the most recent ack was called.
  EXPECT_FALSE(test_callback1->called());
  EXPECT_TRUE(test_callback2->called());
  EXPECT_EQ(EventResult::HANDLED, test_callback2->result());
}

// Tests that when an input handler consumes the event, that
// CompositorMusConnection will consume the ack, but call as UNHANDLED.
TEST_F(CompositorMusConnectionTest, InputHandlerConsumes) {
  input_handler_manager()->SetHandleInputEventResult(
      InputEventAckState::INPUT_EVENT_ACK_STATE_CONSUMED);
  ui::TestWindow test_window;
  std::unique_ptr<ui::Event> event(GenerateKeyEvent());
  scoped_refptr<TestCallback> test_callback(new TestCallback);
  std::unique_ptr<base::Callback<void(EventResult)>> ack_callback(
      new base::Callback<void(EventResult)>(
          base::Bind(&TestCallback::ResultCallback, test_callback)));

  OnWindowInputEvent(&test_window, *event.get(), &ack_callback);

  EXPECT_FALSE(ack_callback.get());
  VerifyAndRunQueues(false, false);
  EXPECT_TRUE(test_callback->called());
  EXPECT_EQ(EventResult::UNHANDLED, test_callback->result());
}

// Tests that when the renderer will not ack an event, that
// CompositorMusConnection will consume the ack, but call as UNHANDLED.
TEST_F(CompositorMusConnectionTest, RendererWillNotSendAck) {
  ui::TestWindow test_window;
  ui::PointerEvent event(
      ui::ET_POINTER_DOWN, gfx::Point(), gfx::Point(), ui::EF_NONE, 0, 0,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_MOUSE),
      ui::EventTimeForNow());

  scoped_refptr<TestCallback> test_callback(new TestCallback);
  std::unique_ptr<base::Callback<void(EventResult)>> ack_callback(
      new base::Callback<void(EventResult)>(
          base::Bind(&TestCallback::ResultCallback, test_callback)));

  OnWindowInputEvent(&test_window, event, &ack_callback);
  EXPECT_FALSE(ack_callback.get());

  VerifyAndRunQueues(true, false);
  EXPECT_TRUE(test_callback->called());
  EXPECT_EQ(EventResult::UNHANDLED, test_callback->result());
}

// Tests that when a touch event id provided, that CompositorMusConnection
// consumes the ack.
TEST_F(CompositorMusConnectionTest, TouchEventConsumed) {
  TestRenderWidgetInputHandler* input_handler = render_widget_input_handler();
  input_handler->set_delegate(connection());
  input_handler->set_state(InputEventAckState::INPUT_EVENT_ACK_STATE_CONSUMED);

  ui::TestWindow test_window;
  ui::PointerEvent event(
      ui::ET_POINTER_DOWN, gfx::Point(), gfx::Point(), ui::EF_NONE, 0, 0,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH),
      ui::EventTimeForNow());

  scoped_refptr<TestCallback> test_callback(new TestCallback);
  std::unique_ptr<base::Callback<void(EventResult)>> ack_callback(
      new base::Callback<void(EventResult)>(
          base::Bind(&TestCallback::ResultCallback, test_callback)));

  OnWindowInputEvent(&test_window, event, &ack_callback);
  // OnWindowInputEvent is expected to clear the callback if it plans on
  // handling the ack.
  EXPECT_FALSE(ack_callback.get());

  VerifyAndRunQueues(true, true);

  // The ack callback should have been called
  EXPECT_TRUE(test_callback->called());
  EXPECT_EQ(EventResult::HANDLED, test_callback->result());
}

}  // namespace content
