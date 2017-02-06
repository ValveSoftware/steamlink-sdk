// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/input/input_router_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input_messages.h"
#include "content/common/resize_params.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"

#if defined(OS_ANDROID)
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#endif

#if defined(USE_AURA) || defined(OS_MACOSX)
#include "content/browser/compositor/test/no_transport_image_transport_factory.h"
#endif

#if defined(USE_AURA)
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/renderer_host/ui_events_helper.h"
#include "ui/aura/test/test_screen.h"
#include "ui/events/event.h"
#endif

using base::TimeDelta;
using blink::WebGestureDevice;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

std::string GetInputMessageTypes(MockRenderProcessHost* process) {
  std::string result;
  for (size_t i = 0; i < process->sink().message_count(); ++i) {
    const IPC::Message* message = process->sink().GetMessageAt(i);
    EXPECT_EQ(InputMsg_HandleInputEvent::ID, message->type());
    InputMsg_HandleInputEvent::Param params;
    EXPECT_TRUE(InputMsg_HandleInputEvent::Read(message, &params));
    const WebInputEvent* event = std::get<0>(params);
    if (i != 0)
      result += " ";
    result += WebInputEventTraits::GetName(event->type);
  }
  process->sink().ClearMessages();
  return result;
}

// MockInputRouter -------------------------------------------------------------

class MockInputRouter : public InputRouter {
 public:
  explicit MockInputRouter(InputRouterClient* client)
      : send_event_called_(false),
        sent_mouse_event_(false),
        sent_wheel_event_(false),
        sent_keyboard_event_(false),
        sent_gesture_event_(false),
        send_touch_event_not_cancelled_(false),
        message_received_(false),
        client_(client) {
  }
  ~MockInputRouter() override {}

  // InputRouter
  bool SendInput(std::unique_ptr<IPC::Message> message) override {
    send_event_called_ = true;
    return true;
  }
  void SendMouseEvent(const MouseEventWithLatencyInfo& mouse_event) override {
    sent_mouse_event_ = true;
  }
  void SendWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) override {
    sent_wheel_event_ = true;
  }
  void SendKeyboardEvent(
      const NativeWebKeyboardEventWithLatencyInfo& key_event) override {
    sent_keyboard_event_ = true;
  }
  void SendGestureEvent(
      const GestureEventWithLatencyInfo& gesture_event) override {
    sent_gesture_event_ = true;
  }
  void SendTouchEvent(const TouchEventWithLatencyInfo& touch_event) override {
    send_touch_event_not_cancelled_ =
        client_->FilterInputEvent(touch_event.event, touch_event.latency) ==
        INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
  }
  const NativeWebKeyboardEvent* GetLastKeyboardEvent() const override {
    NOTREACHED();
    return NULL;
  }
  void NotifySiteIsMobileOptimized(bool is_mobile_optimized) override {}
  void RequestNotificationWhenFlushed() override {}
  bool HasPendingEvents() const override { return false; }
  void SetDeviceScaleFactor(float device_scale_factor) override {}
  void SetFrameTreeNodeId(int frameTreeNodeId) override {}

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& message) override {
    message_received_ = true;
    return false;
  }

  bool send_event_called_;
  bool sent_mouse_event_;
  bool sent_wheel_event_;
  bool sent_keyboard_event_;
  bool sent_gesture_event_;
  bool send_touch_event_not_cancelled_;
  bool message_received_;

 private:
  InputRouterClient* client_;

  DISALLOW_COPY_AND_ASSIGN(MockInputRouter);
};

// MockRenderWidgetHost ----------------------------------------------------

class MockRenderWidgetHost : public RenderWidgetHostImpl {
 public:
  MockRenderWidgetHost(RenderWidgetHostDelegate* delegate,
                       RenderProcessHost* process,
                       int routing_id)
      : RenderWidgetHostImpl(
            delegate,
            process,
            routing_id,
            false),
        new_content_rendering_timeout_fired_(false) {
    acked_touch_event_type_ = blink::WebInputEvent::Undefined;
  }

  // Allow poking at a few private members.
  using RenderWidgetHostImpl::GetResizeParams;
  using RenderWidgetHostImpl::OnUpdateRect;
  using RenderWidgetHostImpl::RendererExited;
  using RenderWidgetHostImpl::SetInitialRenderSizeParams;
  using RenderWidgetHostImpl::old_resize_params_;
  using RenderWidgetHostImpl::is_hidden_;
  using RenderWidgetHostImpl::resize_ack_pending_;
  using RenderWidgetHostImpl::input_router_;

  void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                       InputEventAckState ack_result) override {
    // Sniff touch acks.
    acked_touch_event_type_ = event.event.type;
    RenderWidgetHostImpl::OnTouchEventAck(event, ack_result);
  }

  bool new_content_rendering_timeout_fired() const {
    return new_content_rendering_timeout_fired_;
  }

  void DisableGestureDebounce() {
    input_router_.reset(new InputRouterImpl(
        process_, this, this, routing_id_, InputRouterImpl::Config()));
  }

  WebInputEvent::Type acked_touch_event_type() const {
    return acked_touch_event_type_;
  }

  void SetupForInputRouterTest() {
    input_router_.reset(new MockInputRouter(this));
  }

  MockInputRouter* mock_input_router() {
    return static_cast<MockInputRouter*>(input_router_.get());
  }

 protected:
  void NotifyNewContentRenderingTimeoutForTesting() override {
    new_content_rendering_timeout_fired_ = true;
  }

  bool new_content_rendering_timeout_fired_;
  WebInputEvent::Type acked_touch_event_type_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRenderWidgetHost);
};

namespace  {

// RenderWidgetHostProcess -----------------------------------------------------

class RenderWidgetHostProcess : public MockRenderProcessHost {
 public:
  explicit RenderWidgetHostProcess(BrowserContext* browser_context)
      : MockRenderProcessHost(browser_context) {
  }
  ~RenderWidgetHostProcess() override {}

  bool HasConnection() const override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostProcess);
};

// TestView --------------------------------------------------------------------

// This test view allows us to specify the size, and keep track of acked
// touch-events.
class TestView : public TestRenderWidgetHostView {
 public:
  explicit TestView(RenderWidgetHostImpl* rwh)
      : TestRenderWidgetHostView(rwh),
        unhandled_wheel_event_count_(0),
        acked_event_count_(0),
        gesture_event_type_(-1),
        use_fake_physical_backing_size_(false),
        ack_result_(INPUT_EVENT_ACK_STATE_UNKNOWN) {
  }

  // Sets the bounds returned by GetViewBounds.
  void set_bounds(const gfx::Rect& bounds) {
    bounds_ = bounds;
  }

  const WebTouchEvent& acked_event() const { return acked_event_; }
  int acked_event_count() const { return acked_event_count_; }
  void ClearAckedEvent() {
    acked_event_.type = blink::WebInputEvent::Undefined;
    acked_event_count_ = 0;
  }

  const WebMouseWheelEvent& unhandled_wheel_event() const {
    return unhandled_wheel_event_;
  }
  int unhandled_wheel_event_count() const {
    return unhandled_wheel_event_count_;
  }
  int gesture_event_type() const { return gesture_event_type_; }
  InputEventAckState ack_result() const { return ack_result_; }

  void SetMockPhysicalBackingSize(const gfx::Size& mock_physical_backing_size) {
    use_fake_physical_backing_size_ = true;
    mock_physical_backing_size_ = mock_physical_backing_size;
  }
  void ClearMockPhysicalBackingSize() {
    use_fake_physical_backing_size_ = false;
  }
  void SetScreenInfo(const blink::WebScreenInfo& screen_info) {
    screen_info_ = screen_info;
  }

  // RenderWidgetHostView override.
  gfx::Rect GetViewBounds() const override { return bounds_; }
  void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                              InputEventAckState ack_result) override {
    acked_event_ = touch.event;
    ++acked_event_count_;
  }
  void WheelEventAck(const WebMouseWheelEvent& event,
                     InputEventAckState ack_result) override {
    if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED)
      return;
    unhandled_wheel_event_count_++;
    unhandled_wheel_event_ = event;
  }
  void GestureEventAck(const WebGestureEvent& event,
                       InputEventAckState ack_result) override {
    gesture_event_type_ = event.type;
    ack_result_ = ack_result;
  }
  gfx::Size GetPhysicalBackingSize() const override {
    if (use_fake_physical_backing_size_)
      return mock_physical_backing_size_;
    return TestRenderWidgetHostView::GetPhysicalBackingSize();
  }
  void GetScreenInfo(blink::WebScreenInfo* screen_info) override {
    *screen_info = screen_info_;
  }
#if defined(USE_AURA)
  ~TestView() override {
    // Simulate the mouse exit event dispatched when an aura window is
    // destroyed. (MakeWebMouseEventFromAuraEvent translates ET_MOUSE_EXITED
    // into WebInputEvent::MouseMove.)
    WebMouseEvent event =
        SyntheticWebMouseEventBuilder::Build(WebInputEvent::MouseMove);
    event.timeStampSeconds =
        (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
    rwh_->input_router()->SendMouseEvent(
        MouseEventWithLatencyInfo(event, ui::LatencyInfo()));
  }
#endif

 protected:
  WebMouseWheelEvent unhandled_wheel_event_;
  int unhandled_wheel_event_count_;
  WebTouchEvent acked_event_;
  int acked_event_count_;
  int gesture_event_type_;
  gfx::Rect bounds_;
  bool use_fake_physical_backing_size_;
  gfx::Size mock_physical_backing_size_;
  InputEventAckState ack_result_;
  blink::WebScreenInfo screen_info_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestView);
};

// MockRenderWidgetHostDelegate --------------------------------------------

class MockRenderWidgetHostDelegate : public RenderWidgetHostDelegate {
 public:
  MockRenderWidgetHostDelegate()
      : prehandle_keyboard_event_(false),
        prehandle_keyboard_event_is_shortcut_(false),
        prehandle_keyboard_event_called_(false),
        prehandle_keyboard_event_type_(WebInputEvent::Undefined),
        unhandled_keyboard_event_called_(false),
        unhandled_keyboard_event_type_(WebInputEvent::Undefined),
        handle_wheel_event_(false),
        handle_wheel_event_called_(false),
        unresponsive_timer_fired_(false) {}
  ~MockRenderWidgetHostDelegate() override {}

  // Tests that make sure we ignore keyboard event acknowledgments to events we
  // didn't send work by making sure we didn't call UnhandledKeyboardEvent().
  bool unhandled_keyboard_event_called() const {
    return unhandled_keyboard_event_called_;
  }

  WebInputEvent::Type unhandled_keyboard_event_type() const {
    return unhandled_keyboard_event_type_;
  }

  bool prehandle_keyboard_event_called() const {
    return prehandle_keyboard_event_called_;
  }

  WebInputEvent::Type prehandle_keyboard_event_type() const {
    return prehandle_keyboard_event_type_;
  }

  void set_prehandle_keyboard_event(bool handle) {
    prehandle_keyboard_event_ = handle;
  }

  void set_handle_wheel_event(bool handle) {
    handle_wheel_event_ = handle;
  }

  void set_prehandle_keyboard_event_is_shortcut(bool is_shortcut) {
    prehandle_keyboard_event_is_shortcut_ = is_shortcut;
  }

  bool handle_wheel_event_called() const { return handle_wheel_event_called_; }

  bool unresponsive_timer_fired() const { return unresponsive_timer_fired_; }

 protected:
  bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                              bool* is_keyboard_shortcut) override {
    prehandle_keyboard_event_type_ = event.type;
    prehandle_keyboard_event_called_ = true;
    *is_keyboard_shortcut = prehandle_keyboard_event_is_shortcut_;
    return prehandle_keyboard_event_;
  }

  void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) override {
    unhandled_keyboard_event_type_ = event.type;
    unhandled_keyboard_event_called_ = true;
  }

  bool HandleWheelEvent(const blink::WebMouseWheelEvent& event) override {
    handle_wheel_event_called_ = true;
    return handle_wheel_event_;
  }

  void RendererUnresponsive(RenderWidgetHostImpl* render_widget_host,
                            RendererUnresponsiveType type) override {
    unresponsive_timer_fired_ = true;
  }

  void Cut() override {}
  void Copy() override {}
  void Paste() override {}
  void SelectAll() override {}

 private:
  bool prehandle_keyboard_event_;
  bool prehandle_keyboard_event_is_shortcut_;
  bool prehandle_keyboard_event_called_;
  WebInputEvent::Type prehandle_keyboard_event_type_;

  bool unhandled_keyboard_event_called_;
  WebInputEvent::Type unhandled_keyboard_event_type_;

  bool handle_wheel_event_;
  bool handle_wheel_event_called_;

  bool unresponsive_timer_fired_;
};

// RenderWidgetHostTest --------------------------------------------------------

class RenderWidgetHostTest : public testing::Test {
 public:
  RenderWidgetHostTest()
      : process_(NULL),
        handle_key_press_event_(false),
        handle_mouse_event_(false),
        simulated_event_time_delta_seconds_(0) {
    last_simulated_event_time_seconds_ =
        (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  }
  ~RenderWidgetHostTest() override {}

  bool KeyPressEventCallback(const NativeWebKeyboardEvent& /* event */) {
    return handle_key_press_event_;
  }
  bool MouseEventCallback(const blink::WebMouseEvent& /* event */) {
    return handle_mouse_event_;
  }

 protected:
  // testing::Test
  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kValidateInputEventStream);

    browser_context_.reset(new TestBrowserContext());
    delegate_.reset(new MockRenderWidgetHostDelegate());
    process_ = new RenderWidgetHostProcess(browser_context_.get());
#if defined(USE_AURA) || defined(OS_MACOSX)
    ImageTransportFactory::InitializeForUnitTests(
        std::unique_ptr<ImageTransportFactory>(
            new NoTransportImageTransportFactory));
#endif
#if defined(USE_AURA)
    screen_.reset(aura::TestScreen::Create(gfx::Size()));
    display::Screen::SetScreenInstance(screen_.get());
#endif
    host_.reset(new MockRenderWidgetHost(delegate_.get(), process_,
                                         process_->GetNextRoutingID()));
    view_.reset(new TestView(host_.get()));
    ConfigureView(view_.get());
    host_->SetView(view_.get());
    SetInitialRenderSizeParams();
    host_->Init();
    host_->DisableGestureDebounce();
  }

  void TearDown() override {
    view_.reset();
    host_.reset();
    delegate_.reset();
    process_ = NULL;
    browser_context_.reset();

#if defined(USE_AURA)
    display::Screen::SetScreenInstance(nullptr);
    screen_.reset();
#endif
#if defined(USE_AURA) || defined(OS_MACOSX)
    ImageTransportFactory::Terminate();
#endif

    // Process all pending tasks to avoid leaks.
    base::RunLoop().RunUntilIdle();
  }

  void SetInitialRenderSizeParams() {
    ResizeParams render_size_params;
    host_->GetResizeParams(&render_size_params);
    host_->SetInitialRenderSizeParams(render_size_params);
  }

  virtual void ConfigureView(TestView* view) {
  }

  int64_t GetLatencyComponentId() { return host_->GetLatencyComponentId(); }

  void SendInputEventACK(WebInputEvent::Type type,
                         InputEventAckState ack_result) {
    DCHECK(!WebInputEvent::isTouchEventType(type));
    InputEventAck ack(type, ack_result);
    host_->OnMessageReceived(InputHostMsg_HandleInputEvent_ACK(0, ack));
  }

  double GetNextSimulatedEventTimeSeconds() {
    last_simulated_event_time_seconds_ += simulated_event_time_delta_seconds_;
    return last_simulated_event_time_seconds_;
  }

  void SimulateKeyboardEvent(WebInputEvent::Type type) {
    SimulateKeyboardEvent(type, 0);
  }

  void SimulateKeyboardEvent(WebInputEvent::Type type, int modifiers) {
    WebKeyboardEvent event = SyntheticWebKeyboardEventBuilder::Build(type);
    event.modifiers = modifiers;
    NativeWebKeyboardEvent native_event;
    memcpy(&native_event, &event, sizeof(event));
    host_->ForwardKeyboardEvent(native_event);
  }

  void SimulateMouseEvent(WebInputEvent::Type type) {
    host_->ForwardMouseEvent(SyntheticWebMouseEventBuilder::Build(type));
  }

  void SimulateMouseEventWithLatencyInfo(WebInputEvent::Type type,
                                         const ui::LatencyInfo& ui_latency) {
    host_->ForwardMouseEventWithLatencyInfo(
        SyntheticWebMouseEventBuilder::Build(type),
        ui_latency);
  }

  void SimulateWheelEvent(float dX, float dY, int modifiers, bool precise) {
    host_->ForwardWheelEvent(SyntheticWebMouseWheelEventBuilder::Build(
        0, 0, dX, dY, modifiers, precise));
  }

  void SimulateWheelEventWithLatencyInfo(float dX,
                                         float dY,
                                         int modifiers,
                                         bool precise,
                                         const ui::LatencyInfo& ui_latency) {
    host_->ForwardWheelEventWithLatencyInfo(
        SyntheticWebMouseWheelEventBuilder::Build(0, 0, dX, dY, modifiers,
                                                  precise),
        ui_latency);
  }

  void SimulateMouseMove(int x, int y, int modifiers) {
    SimulateMouseEvent(WebInputEvent::MouseMove, x, y, modifiers, false);
  }

  void SimulateMouseEvent(
      WebInputEvent::Type type, int x, int y, int modifiers, bool pressed) {
    WebMouseEvent event =
        SyntheticWebMouseEventBuilder::Build(type, x, y, modifiers);
    if (pressed)
      event.button = WebMouseEvent::ButtonLeft;
    event.timeStampSeconds = GetNextSimulatedEventTimeSeconds();
    host_->ForwardMouseEvent(event);
  }

  // Inject simple synthetic WebGestureEvent instances.
  void SimulateGestureEvent(WebInputEvent::Type type,
                            WebGestureDevice sourceDevice) {
    host_->ForwardGestureEvent(
        SyntheticWebGestureEventBuilder::Build(type, sourceDevice));
  }

  void SimulateGestureEventWithLatencyInfo(WebInputEvent::Type type,
                                           WebGestureDevice sourceDevice,
                                           const ui::LatencyInfo& ui_latency) {
    host_->ForwardGestureEventWithLatencyInfo(
        SyntheticWebGestureEventBuilder::Build(type, sourceDevice),
        ui_latency);
  }

  // Set the timestamp for the touch-event.
  void SetTouchTimestamp(base::TimeTicks timestamp) {
    touch_event_.SetTimestamp(timestamp);
  }

  // Sends a touch event (irrespective of whether the page has a touch-event
  // handler or not).
  uint32_t SendTouchEvent() {
    uint32_t touch_event_id = touch_event_.uniqueTouchEventId;
    host_->ForwardTouchEventWithLatencyInfo(touch_event_, ui::LatencyInfo());

    touch_event_.ResetPoints();
    return touch_event_id;
  }

  int PressTouchPoint(int x, int y) {
    return touch_event_.PressPoint(x, y);
  }

  void MoveTouchPoint(int index, int x, int y) {
    touch_event_.MovePoint(index, x, y);
  }

  void ReleaseTouchPoint(int index) {
    touch_event_.ReleasePoint(index);
  }

  const WebInputEvent* GetInputEventFromMessage(const IPC::Message& message) {
    base::PickleIterator iter(message);
    const char* data;
    int data_length;
    if (!iter.ReadData(&data, &data_length))
      return NULL;
    return reinterpret_cast<const WebInputEvent*>(data);
  }

  base::MessageLoopForUI message_loop_;

  std::unique_ptr<TestBrowserContext> browser_context_;
  RenderWidgetHostProcess* process_;  // Deleted automatically by the widget.
  std::unique_ptr<MockRenderWidgetHostDelegate> delegate_;
  std::unique_ptr<MockRenderWidgetHost> host_;
  std::unique_ptr<TestView> view_;
  std::unique_ptr<display::Screen> screen_;
  bool handle_key_press_event_;
  bool handle_mouse_event_;
  double last_simulated_event_time_seconds_;
  double simulated_event_time_delta_seconds_;

 private:
  SyntheticWebTouchEvent touch_event_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostTest);
};

#if GTEST_HAS_PARAM_TEST
// RenderWidgetHostWithSourceTest ----------------------------------------------

// This is for tests that are to be run for all source devices.
class RenderWidgetHostWithSourceTest
    : public RenderWidgetHostTest,
      public testing::WithParamInterface<WebGestureDevice> {};
#endif  // GTEST_HAS_PARAM_TEST

}  // namespace

// -----------------------------------------------------------------------------

TEST_F(RenderWidgetHostTest, Resize) {
  // The initial bounds is the empty rect, so setting it to the same thing
  // shouldn't send the resize message.
  view_->set_bounds(gfx::Rect());
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // No resize ack if the physical backing gets set, but the view bounds are
  // zero.
  view_->SetMockPhysicalBackingSize(gfx::Size(200, 200));
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);

  // Setting the view bounds to nonzero should send out the notification.
  // but should not expect ack for empty physical backing size.
  gfx::Rect original_size(0, 0, 100, 100);
  process_->sink().ClearMessages();
  view_->set_bounds(original_size);
  view_->SetMockPhysicalBackingSize(gfx::Size());
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(original_size.size(), host_->old_resize_params_->new_size);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Setting the bounds and physical backing size to nonzero should send out
  // the notification and expect an ack.
  process_->sink().ClearMessages();
  view_->ClearMockPhysicalBackingSize();
  host_->WasResized();
  EXPECT_TRUE(host_->resize_ack_pending_);
  EXPECT_EQ(original_size.size(), host_->old_resize_params_->new_size);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));
  ViewHostMsg_UpdateRect_Params params;
  params.flags = ViewHostMsg_UpdateRect_Flags::IS_RESIZE_ACK;
  params.view_size = original_size.size();
  host_->OnUpdateRect(params);
  EXPECT_FALSE(host_->resize_ack_pending_);

  // Send out a update that's not a resize ack after setting resize ack pending
  // flag. This should not clean the resize ack pending flag.
  process_->sink().ClearMessages();
  gfx::Rect second_size(0, 0, 110, 110);
  EXPECT_FALSE(host_->resize_ack_pending_);
  view_->set_bounds(second_size);
  host_->WasResized();
  EXPECT_TRUE(host_->resize_ack_pending_);
  params.flags = 0;
  params.view_size = gfx::Size(100, 100);
  host_->OnUpdateRect(params);
  EXPECT_TRUE(host_->resize_ack_pending_);
  EXPECT_EQ(second_size.size(), host_->old_resize_params_->new_size);

  // Sending out a new notification should NOT send out a new IPC message since
  // a resize ACK is pending.
  gfx::Rect third_size(0, 0, 120, 120);
  process_->sink().ClearMessages();
  view_->set_bounds(third_size);
  host_->WasResized();
  EXPECT_TRUE(host_->resize_ack_pending_);
  EXPECT_EQ(second_size.size(), host_->old_resize_params_->new_size);
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Send a update that's a resize ack, but for the original_size we sent. Since
  // this isn't the second_size, the message handler should immediately send
  // a new resize message for the new size to the renderer.
  process_->sink().ClearMessages();
  params.flags = ViewHostMsg_UpdateRect_Flags::IS_RESIZE_ACK;
  params.view_size = original_size.size();
  host_->OnUpdateRect(params);
  EXPECT_TRUE(host_->resize_ack_pending_);
  EXPECT_EQ(third_size.size(), host_->old_resize_params_->new_size);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Send the resize ack for the latest size.
  process_->sink().ClearMessages();
  params.flags = ViewHostMsg_UpdateRect_Flags::IS_RESIZE_ACK;
  params.view_size = third_size.size();
  host_->OnUpdateRect(params);
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(third_size.size(), host_->old_resize_params_->new_size);
  EXPECT_FALSE(process_->sink().GetFirstMessageMatching(ViewMsg_Resize::ID));

  // Now clearing the bounds should send out a notification but we shouldn't
  // expect a resize ack (since the renderer won't ack empty sizes). The message
  // should contain the new size (0x0) and not the previous one that we skipped
  process_->sink().ClearMessages();
  view_->set_bounds(gfx::Rect());
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(), host_->old_resize_params_->new_size);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Send a rect that has no area but has either width or height set.
  process_->sink().ClearMessages();
  view_->set_bounds(gfx::Rect(0, 0, 0, 30));
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(0, 30), host_->old_resize_params_->new_size);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Set the same size again. It should not be sent again.
  process_->sink().ClearMessages();
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(0, 30), host_->old_resize_params_->new_size);
  EXPECT_FALSE(process_->sink().GetFirstMessageMatching(ViewMsg_Resize::ID));

  // A different size should be sent again, however.
  view_->set_bounds(gfx::Rect(0, 0, 0, 31));
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(0, 31), host_->old_resize_params_->new_size);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));
}

// Test that a resize event is sent if WasResized() is called after a
// WebScreenInfo change.
TEST_F(RenderWidgetHostTest, ResizeScreenInfo) {
  blink::WebScreenInfo screen_info;
  screen_info.deviceScaleFactor = 1.f;
  screen_info.rect = blink::WebRect(0, 0, 800, 600);
  screen_info.availableRect = blink::WebRect(0, 0, 800, 600);
  screen_info.orientationAngle = 0;
  screen_info.orientationType = blink::WebScreenOrientationPortraitPrimary;

  view_->SetScreenInfo(screen_info);
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));
  process_->sink().ClearMessages();

  screen_info.orientationAngle = 180;
  screen_info.orientationType = blink::WebScreenOrientationLandscapePrimary;

  view_->SetScreenInfo(screen_info);
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));
  process_->sink().ClearMessages();

  screen_info.deviceScaleFactor = 2.f;

  view_->SetScreenInfo(screen_info);
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));
  process_->sink().ClearMessages();

  // No screen change.
  view_->SetScreenInfo(screen_info);
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));
}

// Test for crbug.com/25097.  If a renderer crashes between a resize and the
// corresponding update message, we must be sure to clear the resize ack logic.
TEST_F(RenderWidgetHostTest, ResizeThenCrash) {
  // Clear the first Resize message that carried screen info.
  process_->sink().ClearMessages();

  // Setting the bounds to a "real" rect should send out the notification.
  gfx::Rect original_size(0, 0, 100, 100);
  view_->set_bounds(original_size);
  host_->WasResized();
  EXPECT_TRUE(host_->resize_ack_pending_);
  EXPECT_EQ(original_size.size(), host_->old_resize_params_->new_size);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Simulate a renderer crash before the update message.  Ensure all the
  // resize ack logic is cleared.  Must clear the view first so it doesn't get
  // deleted.
  host_->SetView(NULL);
  host_->RendererExited(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(), host_->old_resize_params_->new_size);

  // Reset the view so we can exit the test cleanly.
  host_->SetView(view_.get());
}

// Unable to include render_widget_host_view_mac.h and compile.
#if !defined(OS_MACOSX)
// Tests setting background transparency.
TEST_F(RenderWidgetHostTest, Background) {
  std::unique_ptr<RenderWidgetHostViewBase> view;
#if defined(USE_AURA)
  view.reset(new RenderWidgetHostViewAura(host_.get(), false));
  // TODO(derat): Call this on all platforms: http://crbug.com/102450.
  view->InitAsChild(NULL);
#elif defined(OS_ANDROID)
  view.reset(new RenderWidgetHostViewAndroid(host_.get(), NULL));
#endif
  host_->SetView(view.get());

  EXPECT_TRUE(view->GetBackgroundOpaque());
  view->SetBackgroundColor(SK_ColorTRANSPARENT);
  EXPECT_FALSE(view->GetBackgroundOpaque());

  const IPC::Message* set_background =
      process_->sink().GetUniqueMessageMatching(
          ViewMsg_SetBackgroundOpaque::ID);
  ASSERT_TRUE(set_background);
  std::tuple<bool> sent_background;
  ViewMsg_SetBackgroundOpaque::Read(set_background, &sent_background);
  EXPECT_FALSE(std::get<0>(sent_background));

#if defined(USE_AURA)
  // See the comment above |InitAsChild(NULL)|.
  host_->SetView(NULL);
  static_cast<RenderWidgetHostViewBase*>(view.release())->Destroy();
#endif
}
#endif

// Test that we don't paint when we're hidden, but we still send the ACK. Most
// of the rest of the painting is tested in the GetBackingStore* ones.
TEST_F(RenderWidgetHostTest, HiddenPaint) {
  BrowserThreadImpl ui_thread(BrowserThread::UI, base::MessageLoop::current());
  // Hide the widget, it should have sent out a message to the renderer.
  EXPECT_FALSE(host_->is_hidden_);
  host_->WasHidden();
  EXPECT_TRUE(host_->is_hidden_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_WasHidden::ID));

  // Send it an update as from the renderer.
  process_->sink().ClearMessages();
  ViewHostMsg_UpdateRect_Params params;
  params.view_size = gfx::Size(100, 100);
  host_->OnUpdateRect(params);

  // Now unhide.
  process_->sink().ClearMessages();
  host_->WasShown(ui::LatencyInfo());
  EXPECT_FALSE(host_->is_hidden_);

  // It should have sent out a restored message with a request to paint.
  const IPC::Message* restored = process_->sink().GetUniqueMessageMatching(
      ViewMsg_WasShown::ID);
  ASSERT_TRUE(restored);
  std::tuple<bool, ui::LatencyInfo> needs_repaint;
  ViewMsg_WasShown::Read(restored, &needs_repaint);
  EXPECT_TRUE(std::get<0>(needs_repaint));
}

TEST_F(RenderWidgetHostTest, IgnoreKeyEventsHandledByRenderer) {
  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::RawKeyDown,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(delegate_->unhandled_keyboard_event_called());
}

TEST_F(RenderWidgetHostTest, PreHandleRawKeyDownEvent) {
  // Simulate the situation that the browser handled the key down event during
  // pre-handle phrase.
  delegate_->set_prehandle_keyboard_event(true);
  process_->sink().ClearMessages();

  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  EXPECT_TRUE(delegate_->prehandle_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::RawKeyDown,
            delegate_->prehandle_keyboard_event_type());

  // Make sure the RawKeyDown event is not sent to the renderer.
  EXPECT_EQ(0U, process_->sink().message_count());

  // The browser won't pre-handle a Char event.
  delegate_->set_prehandle_keyboard_event(false);

  // Forward the Char event.
  SimulateKeyboardEvent(WebInputEvent::Char);

  // Make sure the Char event is suppressed.
  EXPECT_EQ(0U, process_->sink().message_count());

  // Forward the KeyUp event.
  SimulateKeyboardEvent(WebInputEvent::KeyUp);

  // Make sure only KeyUp was sent to the renderer.
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(InputMsg_HandleInputEvent::ID,
            process_->sink().GetMessageAt(0)->type());
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::KeyUp,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_TRUE(delegate_->unhandled_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::KeyUp, delegate_->unhandled_keyboard_event_type());
}

TEST_F(RenderWidgetHostTest, RawKeyDownShortcutEvent) {
  // Simulate the situation that the browser marks the key down as a keyboard
  // shortcut, but doesn't consume it in the pre-handle phase.
  delegate_->set_prehandle_keyboard_event_is_shortcut(true);
  process_->sink().ClearMessages();

  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  EXPECT_TRUE(delegate_->prehandle_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::RawKeyDown,
            delegate_->prehandle_keyboard_event_type());

  // Make sure the RawKeyDown event is sent to the renderer.
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ("RawKeyDown", GetInputMessageTypes(process_));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::RawKeyDown,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(WebInputEvent::RawKeyDown,
            delegate_->unhandled_keyboard_event_type());

  // The browser won't pre-handle a Char event.
  delegate_->set_prehandle_keyboard_event_is_shortcut(false);

  // Forward the Char event.
  SimulateKeyboardEvent(WebInputEvent::Char);

  // The Char event is not suppressed; the renderer will ignore it
  // if the preceding RawKeyDown shortcut goes unhandled.
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ("Char", GetInputMessageTypes(process_));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::Char, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(WebInputEvent::Char, delegate_->unhandled_keyboard_event_type());

  // Forward the KeyUp event.
  SimulateKeyboardEvent(WebInputEvent::KeyUp);

  // Make sure only KeyUp was sent to the renderer.
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ("KeyUp", GetInputMessageTypes(process_));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::KeyUp, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(WebInputEvent::KeyUp, delegate_->unhandled_keyboard_event_type());
}

TEST_F(RenderWidgetHostTest, UnhandledWheelEvent) {
  SimulateWheelEvent(-5, 0, 0, true);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(delegate_->handle_wheel_event_called());
  EXPECT_EQ(1, view_->unhandled_wheel_event_count());
  EXPECT_EQ(-5, view_->unhandled_wheel_event().deltaX);
}

TEST_F(RenderWidgetHostTest, HandleWheelEvent) {
  // Indicate that we're going to handle this wheel event
  delegate_->set_handle_wheel_event(true);

  SimulateWheelEvent(-5, 0, 0, true);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // ensure the wheel event handler was invoked
  EXPECT_TRUE(delegate_->handle_wheel_event_called());

  // and that it suppressed the unhandled wheel event handler.
  EXPECT_EQ(0, view_->unhandled_wheel_event_count());
}

TEST_F(RenderWidgetHostTest, UnhandledGestureEvent) {
  SimulateGestureEvent(WebInputEvent::GestureTwoFingerTap,
                       blink::WebGestureDeviceTouchscreen);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::GestureTwoFingerTap,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(WebInputEvent::GestureTwoFingerTap, view_->gesture_event_type());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, view_->ack_result());
}

// Test that the hang monitor timer expires properly if a new timer is started
// while one is in progress (see crbug.com/11007).
TEST_F(RenderWidgetHostTest, DontPostponeHangMonitorTimeout) {
  // Start with a short timeout.
  host_->StartHangMonitorTimeout(
      TimeDelta::FromMilliseconds(10),
      RenderWidgetHostDelegate::RENDERER_UNRESPONSIVE_UNKNOWN);

  // Immediately try to add a long 30 second timeout.
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());
  host_->StartHangMonitorTimeout(
      TimeDelta::FromSeconds(30),
      RenderWidgetHostDelegate::RENDERER_UNRESPONSIVE_UNKNOWN);

  // Wait long enough for first timeout and see if it fired.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

// Test that the hang monitor timer expires properly if it is started, stopped,
// and then started again.
TEST_F(RenderWidgetHostTest, StopAndStartHangMonitorTimeout) {
  // Start with a short timeout, then stop it.
  host_->StartHangMonitorTimeout(
      TimeDelta::FromMilliseconds(10),
      RenderWidgetHostDelegate::RENDERER_UNRESPONSIVE_UNKNOWN);
  host_->StopHangMonitorTimeout();

  // Start it again to ensure it still works.
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());
  host_->StartHangMonitorTimeout(
      TimeDelta::FromMilliseconds(10),
      RenderWidgetHostDelegate::RENDERER_UNRESPONSIVE_UNKNOWN);

  // Wait long enough for first timeout and see if it fired.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      TimeDelta::FromMilliseconds(40));
  base::MessageLoop::current()->Run();
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

// Test that the hang monitor timer expires properly if it is started, then
// updated to a shorter duration.
TEST_F(RenderWidgetHostTest, ShorterDelayHangMonitorTimeout) {
  // Start with a timeout.
  host_->StartHangMonitorTimeout(
      TimeDelta::FromMilliseconds(100),
      RenderWidgetHostDelegate::RENDERER_UNRESPONSIVE_UNKNOWN);

  // Start it again with shorter delay.
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());
  host_->StartHangMonitorTimeout(
      TimeDelta::FromMilliseconds(20),
      RenderWidgetHostDelegate::RENDERER_UNRESPONSIVE_UNKNOWN);

  // Wait long enough for the second timeout and see if it fired.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      TimeDelta::FromMilliseconds(25));
  base::MessageLoop::current()->Run();
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

// Test that the hang monitor timer is effectively disabled when the widget is
// hidden.
TEST_F(RenderWidgetHostTest, HangMonitorTimeoutDisabledForInputWhenHidden) {
  host_->set_hung_renderer_delay(base::TimeDelta::FromMicroseconds(1));
  SimulateMouseEvent(WebInputEvent::MouseMove, 10, 10, 0, false);

  // Hiding the widget should deactivate the timeout.
  host_->WasHidden();

  // The timeout should not fire.
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      TimeDelta::FromMicroseconds(2));
  base::MessageLoop::current()->Run();
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());

  // The timeout should never reactivate while hidden.
  SimulateMouseEvent(WebInputEvent::MouseMove, 10, 10, 0, false);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      TimeDelta::FromMicroseconds(2));
  base::MessageLoop::current()->Run();
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());

  // Showing the widget should restore the timeout, as the events have
  // not yet been ack'ed.
  host_->WasShown(ui::LatencyInfo());
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      TimeDelta::FromMicroseconds(2));
  base::MessageLoop::current()->Run();
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

// Test that the hang monitor catches two input events but only one ack.
// This can happen if the second input event causes the renderer to hang.
// This test will catch a regression of crbug.com/111185.
TEST_F(RenderWidgetHostTest, MultipleInputEvents) {
  // Configure the host to wait 10ms before considering
  // the renderer hung.
  host_->set_hung_renderer_delay(base::TimeDelta::FromMicroseconds(10));

  // Send two events but only one ack.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  SendInputEventACK(WebInputEvent::RawKeyDown,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  // Wait long enough for first timeout and see if it fired.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      TimeDelta::FromMicroseconds(20));
  base::MessageLoop::current()->Run();
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

// Test that the rendering timeout for newly loaded content fires
// when enough time passes without receiving a new compositor frame.
TEST_F(RenderWidgetHostTest, NewContentRenderingTimeout) {
  host_->set_new_content_rendering_delay_for_testing(
      base::TimeDelta::FromMicroseconds(10));

  // Test immediate start and stop, ensuring that the timeout doesn't fire.
  host_->StartNewContentRenderingTimeout();
  host_->OnFirstPaintAfterLoad();
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      TimeDelta::FromMicroseconds(20));
  base::MessageLoop::current()->Run();

  EXPECT_FALSE(host_->new_content_rendering_timeout_fired());

  // Test that the timer doesn't fire if it receives a stop before
  // a start.
  host_->OnFirstPaintAfterLoad();
  host_->StartNewContentRenderingTimeout();
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      TimeDelta::FromMicroseconds(20));
  base::MessageLoop::current()->Run();

  EXPECT_FALSE(host_->new_content_rendering_timeout_fired());

  // Test with a long delay to ensure that it does fire this time.
  host_->StartNewContentRenderingTimeout();
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      TimeDelta::FromMicroseconds(20));
  base::MessageLoop::current()->Run();
  EXPECT_TRUE(host_->new_content_rendering_timeout_fired());
}

TEST_F(RenderWidgetHostTest, TouchEmulator) {
  simulated_event_time_delta_seconds_ = 0.1;
  // Immediately ack all touches instead of sending them to the renderer.
  host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, false));
  host_->SetTouchEventEmulationEnabled(
      true, ui::GestureProviderConfigType::GENERIC_MOBILE);
  process_->sink().ClearMessages();
  view_->set_bounds(gfx::Rect(0, 0, 400, 200));
  view_->Show();

  SimulateMouseEvent(WebInputEvent::MouseMove, 10, 10, 0, false);
  EXPECT_EQ(0U, process_->sink().message_count());

  // Mouse press becomes touch start which in turn becomes tap.
  SimulateMouseEvent(WebInputEvent::MouseDown, 10, 10, 0, true);
  EXPECT_EQ(WebInputEvent::TouchStart, host_->acked_touch_event_type());
  EXPECT_EQ("GestureTapDown", GetInputMessageTypes(process_));

  // Mouse drag generates touch move, cancels tap and starts scroll.
  SimulateMouseEvent(WebInputEvent::MouseMove, 10, 30, 0, true);
  EXPECT_EQ(WebInputEvent::TouchMove, host_->acked_touch_event_type());
  EXPECT_EQ(
      "GestureTapCancel GestureScrollBegin GestureScrollUpdate",
      GetInputMessageTypes(process_));
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, process_->sink().message_count());

  // Mouse drag with shift becomes pinch.
  SimulateMouseEvent(
      WebInputEvent::MouseMove, 10, 40, WebInputEvent::ShiftKey, true);
  EXPECT_EQ(WebInputEvent::TouchMove, host_->acked_touch_event_type());
  EXPECT_EQ("GesturePinchBegin",
            GetInputMessageTypes(process_));
  EXPECT_EQ(0U, process_->sink().message_count());

  SimulateMouseEvent(
      WebInputEvent::MouseMove, 10, 50, WebInputEvent::ShiftKey, true);
  EXPECT_EQ(WebInputEvent::TouchMove, host_->acked_touch_event_type());
  EXPECT_EQ("GesturePinchUpdate",
            GetInputMessageTypes(process_));
  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, process_->sink().message_count());

  // Mouse drag without shift becomes scroll again.
  SimulateMouseEvent(WebInputEvent::MouseMove, 10, 60, 0, true);
  EXPECT_EQ(WebInputEvent::TouchMove, host_->acked_touch_event_type());
  EXPECT_EQ("GesturePinchEnd GestureScrollUpdate",
            GetInputMessageTypes(process_));
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, process_->sink().message_count());

  SimulateMouseEvent(WebInputEvent::MouseMove, 10, 70, 0, true);
  EXPECT_EQ(WebInputEvent::TouchMove, host_->acked_touch_event_type());
  EXPECT_EQ("GestureScrollUpdate",
            GetInputMessageTypes(process_));
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, process_->sink().message_count());

  SimulateMouseEvent(WebInputEvent::MouseUp, 10, 70, 0, true);
  EXPECT_EQ(WebInputEvent::TouchEnd, host_->acked_touch_event_type());
  EXPECT_EQ("GestureScrollEnd", GetInputMessageTypes(process_));
  EXPECT_EQ(0U, process_->sink().message_count());

  // Mouse move does nothing.
  SimulateMouseEvent(WebInputEvent::MouseMove, 10, 80, 0, false);
  EXPECT_EQ(0U, process_->sink().message_count());

  // Another mouse down continues scroll.
  SimulateMouseEvent(WebInputEvent::MouseDown, 10, 80, 0, true);
  EXPECT_EQ(WebInputEvent::TouchStart, host_->acked_touch_event_type());
  EXPECT_EQ("GestureTapDown", GetInputMessageTypes(process_));
  EXPECT_EQ(0U, process_->sink().message_count());

  SimulateMouseEvent(WebInputEvent::MouseMove, 10, 100, 0, true);
  EXPECT_EQ(WebInputEvent::TouchMove, host_->acked_touch_event_type());
  EXPECT_EQ(
      "GestureTapCancel GestureScrollBegin GestureScrollUpdate",
      GetInputMessageTypes(process_));
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, process_->sink().message_count());

  // Another pinch.
  SimulateMouseEvent(
      WebInputEvent::MouseMove, 10, 110, WebInputEvent::ShiftKey, true);
  EXPECT_EQ(WebInputEvent::TouchMove, host_->acked_touch_event_type());
  EXPECT_EQ("GesturePinchBegin",
            GetInputMessageTypes(process_));
  EXPECT_EQ(0U, process_->sink().message_count());

  SimulateMouseEvent(
      WebInputEvent::MouseMove, 10, 120, WebInputEvent::ShiftKey, true);
  EXPECT_EQ(WebInputEvent::TouchMove, host_->acked_touch_event_type());
  EXPECT_EQ("GesturePinchUpdate",
            GetInputMessageTypes(process_));
  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, process_->sink().message_count());

  // Turn off emulation during a pinch.
  host_->SetTouchEventEmulationEnabled(
      false, ui::GestureProviderConfigType::GENERIC_MOBILE);
  EXPECT_EQ(WebInputEvent::TouchCancel, host_->acked_touch_event_type());
  EXPECT_EQ("GesturePinchEnd GestureScrollEnd",
            GetInputMessageTypes(process_));
  EXPECT_EQ(0U, process_->sink().message_count());

  // Mouse event should pass untouched.
  SimulateMouseEvent(
      WebInputEvent::MouseMove, 10, 10, WebInputEvent::ShiftKey, true);
  EXPECT_EQ("MouseMove", GetInputMessageTypes(process_));
  SendInputEventACK(WebInputEvent::MouseMove,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, process_->sink().message_count());

  // Turn on emulation.
  host_->SetTouchEventEmulationEnabled(
      true, ui::GestureProviderConfigType::GENERIC_MOBILE);
  EXPECT_EQ(0U, process_->sink().message_count());

  // Another touch.
  SimulateMouseEvent(WebInputEvent::MouseDown, 10, 10, 0, true);
  EXPECT_EQ(WebInputEvent::TouchStart, host_->acked_touch_event_type());
  EXPECT_EQ("GestureTapDown", GetInputMessageTypes(process_));
  EXPECT_EQ(0U, process_->sink().message_count());

  // Scroll.
  SimulateMouseEvent(WebInputEvent::MouseMove, 10, 30, 0, true);
  EXPECT_EQ(WebInputEvent::TouchMove, host_->acked_touch_event_type());
  EXPECT_EQ(
      "GestureTapCancel GestureScrollBegin GestureScrollUpdate",
      GetInputMessageTypes(process_));
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  // Turn off emulation during a scroll.
  host_->SetTouchEventEmulationEnabled(
      false, ui::GestureProviderConfigType::GENERIC_MOBILE);
  EXPECT_EQ(WebInputEvent::TouchCancel, host_->acked_touch_event_type());

  EXPECT_EQ("GestureScrollEnd", GetInputMessageTypes(process_));
  EXPECT_EQ(0U, process_->sink().message_count());
}

#define TEST_InputRouterRoutes_NOARGS(INPUTMSG) \
  TEST_F(RenderWidgetHostTest, InputRouterRoutes##INPUTMSG) { \
    host_->SetupForInputRouterTest(); \
    host_->INPUTMSG(); \
    EXPECT_TRUE(host_->mock_input_router()->send_event_called_); \
  }

TEST_InputRouterRoutes_NOARGS(Focus);
TEST_InputRouterRoutes_NOARGS(Blur);
TEST_InputRouterRoutes_NOARGS(LostCapture);

#undef TEST_InputRouterRoutes_NOARGS

#define TEST_InputRouterRoutes_NOARGS_FromRFH(INPUTMSG) \
  TEST_F(RenderWidgetHostTest, InputRouterRoutes##INPUTMSG) { \
    host_->SetupForInputRouterTest(); \
    host_->Send(new INPUTMSG(host_->GetRoutingID())); \
    EXPECT_TRUE(host_->mock_input_router()->send_event_called_); \
  }

TEST_InputRouterRoutes_NOARGS_FromRFH(InputMsg_Undo);
TEST_InputRouterRoutes_NOARGS_FromRFH(InputMsg_Redo);
TEST_InputRouterRoutes_NOARGS_FromRFH(InputMsg_Cut);
TEST_InputRouterRoutes_NOARGS_FromRFH(InputMsg_Copy);
#if defined(OS_MACOSX)
TEST_InputRouterRoutes_NOARGS_FromRFH(InputMsg_CopyToFindPboard);
#endif
TEST_InputRouterRoutes_NOARGS_FromRFH(InputMsg_Paste);
TEST_InputRouterRoutes_NOARGS_FromRFH(InputMsg_PasteAndMatchStyle);
TEST_InputRouterRoutes_NOARGS_FromRFH(InputMsg_Delete);
TEST_InputRouterRoutes_NOARGS_FromRFH(InputMsg_SelectAll);
TEST_InputRouterRoutes_NOARGS_FromRFH(InputMsg_Unselect);

#undef TEST_InputRouterRoutes_NOARGS_FromRFH

TEST_F(RenderWidgetHostTest, InputRouterRoutesReplace) {
  host_->SetupForInputRouterTest();
  host_->Send(new InputMsg_Replace(host_->GetRoutingID(), base::string16()));
  EXPECT_TRUE(host_->mock_input_router()->send_event_called_);
}

TEST_F(RenderWidgetHostTest, InputRouterRoutesReplaceMisspelling) {
  host_->SetupForInputRouterTest();
  host_->Send(new InputMsg_ReplaceMisspelling(host_->GetRoutingID(),
                                              base::string16()));
  EXPECT_TRUE(host_->mock_input_router()->send_event_called_);
}

TEST_F(RenderWidgetHostTest, IgnoreInputEvent) {
  host_->SetupForInputRouterTest();

  host_->SetIgnoreInputEvents(true);

  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);

  SimulateMouseEvent(WebInputEvent::MouseMove);
  EXPECT_FALSE(host_->mock_input_router()->sent_mouse_event_);

  SimulateWheelEvent(0, 100, 0, true);
  EXPECT_FALSE(host_->mock_input_router()->sent_wheel_event_);

  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchpad);
  EXPECT_FALSE(host_->mock_input_router()->sent_gesture_event_);

  PressTouchPoint(100, 100);
  SendTouchEvent();
  EXPECT_FALSE(host_->mock_input_router()->send_touch_event_not_cancelled_);
}

TEST_F(RenderWidgetHostTest, KeyboardListenerIgnoresEvent) {
  host_->SetupForInputRouterTest();
  host_->AddKeyPressEventCallback(
      base::Bind(&RenderWidgetHostTest::KeyPressEventCallback,
                 base::Unretained(this)));
  handle_key_press_event_ = false;
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  EXPECT_TRUE(host_->mock_input_router()->sent_keyboard_event_);
}

TEST_F(RenderWidgetHostTest, KeyboardListenerSuppressFollowingEvents) {
  host_->SetupForInputRouterTest();

  host_->AddKeyPressEventCallback(
      base::Bind(&RenderWidgetHostTest::KeyPressEventCallback,
                 base::Unretained(this)));

  // The callback handles the first event
  handle_key_press_event_ = true;
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);

  // Following Char events should be suppressed
  handle_key_press_event_ = false;
  SimulateKeyboardEvent(WebInputEvent::Char);
  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);
  SimulateKeyboardEvent(WebInputEvent::Char);
  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);

  // Sending RawKeyDown event should stop suppression
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
  EXPECT_TRUE(host_->mock_input_router()->sent_keyboard_event_);

  host_->mock_input_router()->sent_keyboard_event_ = false;
  SimulateKeyboardEvent(WebInputEvent::Char);
  EXPECT_TRUE(host_->mock_input_router()->sent_keyboard_event_);
}

TEST_F(RenderWidgetHostTest, MouseEventCallbackCanHandleEvent) {
  host_->SetupForInputRouterTest();

  host_->AddMouseEventCallback(
      base::Bind(&RenderWidgetHostTest::MouseEventCallback,
                 base::Unretained(this)));

  handle_mouse_event_ = true;
  SimulateMouseEvent(WebInputEvent::MouseDown);

  EXPECT_FALSE(host_->mock_input_router()->sent_mouse_event_);

  handle_mouse_event_ = false;
  SimulateMouseEvent(WebInputEvent::MouseDown);

  EXPECT_TRUE(host_->mock_input_router()->sent_mouse_event_);
}

TEST_F(RenderWidgetHostTest, InputRouterReceivesHandleInputEvent_ACK) {
  host_->SetupForInputRouterTest();

  SendInputEventACK(WebInputEvent::RawKeyDown,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  EXPECT_TRUE(host_->mock_input_router()->message_received_);
}

TEST_F(RenderWidgetHostTest, InputRouterReceivesMoveCaret_ACK) {
  host_->SetupForInputRouterTest();

  host_->OnMessageReceived(InputHostMsg_MoveCaret_ACK(0));

  EXPECT_TRUE(host_->mock_input_router()->message_received_);
}

TEST_F(RenderWidgetHostTest, InputRouterReceivesSelectRange_ACK) {
  host_->SetupForInputRouterTest();

  host_->OnMessageReceived(InputHostMsg_SelectRange_ACK(0));

  EXPECT_TRUE(host_->mock_input_router()->message_received_);
}

TEST_F(RenderWidgetHostTest, InputRouterReceivesHasTouchEventHandlers) {
  host_->SetupForInputRouterTest();

  host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));

  EXPECT_TRUE(host_->mock_input_router()->message_received_);
}

void CheckLatencyInfoComponentInMessage(RenderWidgetHostProcess* process,
                                        int64_t component_id,
                                        WebInputEvent::Type expected_type) {
  EXPECT_EQ(process->sink().message_count(), 1U);

  const IPC::Message* message = process->sink().GetMessageAt(0);
  EXPECT_EQ(InputMsg_HandleInputEvent::ID, message->type());
  InputMsg_HandleInputEvent::Param params;
  EXPECT_TRUE(InputMsg_HandleInputEvent::Read(message, &params));

  const WebInputEvent* event = std::get<0>(params);
  ui::LatencyInfo latency_info = std::get<1>(params);

  EXPECT_TRUE(event->type == expected_type);
  EXPECT_TRUE(latency_info.FindLatency(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, component_id, NULL));

  process->sink().ClearMessages();
}

// Tests that after input event passes through RWHI through ForwardXXXEvent()
// or ForwardXXXEventWithLatencyInfo(), LatencyInfo component
// ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT will always present in the
// event's LatencyInfo.
TEST_F(RenderWidgetHostTest, InputEventRWHLatencyComponent) {
  host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  process_->sink().ClearMessages();

  // Tests RWHI::ForwardWheelEvent().
  SimulateWheelEvent(-5, 0, 0, true);
  CheckLatencyInfoComponentInMessage(
      process_, GetLatencyComponentId(), WebInputEvent::MouseWheel);
  SendInputEventACK(WebInputEvent::MouseWheel, INPUT_EVENT_ACK_STATE_CONSUMED);

  // Tests RWHI::ForwardWheelEventWithLatencyInfo().
  SimulateWheelEventWithLatencyInfo(-5, 0, 0, true, ui::LatencyInfo());
  CheckLatencyInfoComponentInMessage(
      process_, GetLatencyComponentId(), WebInputEvent::MouseWheel);
  SendInputEventACK(WebInputEvent::MouseWheel, INPUT_EVENT_ACK_STATE_CONSUMED);

  // Tests RWHI::ForwardMouseEvent().
  SimulateMouseEvent(WebInputEvent::MouseMove);
  CheckLatencyInfoComponentInMessage(
      process_, GetLatencyComponentId(), WebInputEvent::MouseMove);
  SendInputEventACK(WebInputEvent::MouseMove, INPUT_EVENT_ACK_STATE_CONSUMED);

  // Tests RWHI::ForwardMouseEventWithLatencyInfo().
  SimulateMouseEventWithLatencyInfo(WebInputEvent::MouseMove,
                                    ui::LatencyInfo());
  CheckLatencyInfoComponentInMessage(
      process_, GetLatencyComponentId(), WebInputEvent::MouseMove);
  SendInputEventACK(WebInputEvent::MouseMove, INPUT_EVENT_ACK_STATE_CONSUMED);

  // Tests RWHI::ForwardGestureEvent().
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  CheckLatencyInfoComponentInMessage(
      process_, GetLatencyComponentId(), WebInputEvent::GestureScrollBegin);

  // Tests RWHI::ForwardGestureEventWithLatencyInfo().
  SimulateGestureEventWithLatencyInfo(WebInputEvent::GestureScrollUpdate,
                                      blink::WebGestureDeviceTouchscreen,
                                      ui::LatencyInfo());
  CheckLatencyInfoComponentInMessage(
      process_, GetLatencyComponentId(), WebInputEvent::GestureScrollUpdate);
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  // Tests RWHI::ForwardTouchEventWithLatencyInfo().
  PressTouchPoint(0, 1);
  uint32_t touch_event_id = SendTouchEvent();
  InputEventAck ack(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_event_id);
  host_->OnMessageReceived(InputHostMsg_HandleInputEvent_ACK(0, ack));
  CheckLatencyInfoComponentInMessage(
      process_, GetLatencyComponentId(), WebInputEvent::TouchStart);
}

TEST_F(RenderWidgetHostTest, RendererExitedResetsInputRouter) {
  // RendererExited will delete the view.
  host_->SetView(new TestView(host_.get()));
  host_->RendererExited(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);

  // Make sure the input router is in a fresh state.
  ASSERT_FALSE(host_->input_router()->HasPendingEvents());
}

// Regression test for http://crbug.com/401859 and http://crbug.com/522795.
TEST_F(RenderWidgetHostTest, RendererExitedResetsIsHidden) {
  // RendererExited will delete the view.
  host_->SetView(new TestView(host_.get()));
  host_->WasShown(ui::LatencyInfo());

  ASSERT_FALSE(host_->is_hidden());
  host_->RendererExited(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
  ASSERT_TRUE(host_->is_hidden());

  // Make sure the input router is in a fresh state.
  ASSERT_FALSE(host_->input_router()->HasPendingEvents());
}

TEST_F(RenderWidgetHostTest, ResizeParams) {
  gfx::Rect bounds(0, 0, 100, 100);
  gfx::Size physical_backing_size(40, 50);
  view_->set_bounds(bounds);
  view_->SetMockPhysicalBackingSize(physical_backing_size);

  ResizeParams resize_params;
  host_->GetResizeParams(&resize_params);
  EXPECT_EQ(bounds.size(), resize_params.new_size);
  EXPECT_EQ(physical_backing_size, resize_params.physical_backing_size);
}

class RenderWidgetHostInitialSizeTest : public RenderWidgetHostTest {
 public:
  RenderWidgetHostInitialSizeTest()
      : RenderWidgetHostTest(), initial_size_(200, 100) {}

  void ConfigureView(TestView* view) override {
    view->set_bounds(gfx::Rect(initial_size_));
  }

 protected:
  gfx::Size initial_size_;
};

TEST_F(RenderWidgetHostInitialSizeTest, InitialSize) {
  // Having an initial size set means that the size information had been sent
  // with the reqiest to new up the RenderView and so subsequent WasResized
  // calls should not result in new IPC (unless the size has actually changed).
  host_->WasResized();
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));
  EXPECT_EQ(initial_size_, host_->old_resize_params_->new_size);
  EXPECT_TRUE(host_->resize_ack_pending_);
}

// Tests that event dispatch after the delegate has been detached doesn't cause
// a crash. See crbug.com/563237.
TEST_F(RenderWidgetHostTest, EventDispatchPostDetach) {
  host_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  process_->sink().ClearMessages();

  host_->DetachDelegate();

  // Tests RWHI::ForwardGestureEventWithLatencyInfo().
  SimulateGestureEventWithLatencyInfo(WebInputEvent::GestureScrollUpdate,
                                      blink::WebGestureDeviceTouchscreen,
                                      ui::LatencyInfo());


  // Tests RWHI::ForwardWheelEventWithLatencyInfo().
  SimulateWheelEventWithLatencyInfo(-5, 0, 0, true, ui::LatencyInfo());

  ASSERT_FALSE(host_->input_router()->HasPendingEvents());
}

}  // namespace content
