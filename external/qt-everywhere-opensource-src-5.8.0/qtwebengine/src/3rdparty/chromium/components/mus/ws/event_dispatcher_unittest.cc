// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/event_dispatcher.h"

#include <stddef.h>
#include <stdint.h>

#include <queue>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/mus/common/event_matcher_util.h"
#include "components/mus/ws/accelerator.h"
#include "components/mus/ws/event_dispatcher_delegate.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_surface_manager_test_api.h"
#include "components/mus/ws/test_server_window_delegate.h"
#include "components/mus/ws/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"

namespace mus {
namespace ws {
namespace test {
namespace {

// Client ids used to indicate the client area and non-client area.
const ClientSpecificId kClientAreaId = 11;
const ClientSpecificId kNonclientAreaId = 111;

// Identifies a generated event.
struct DispatchedEventDetails {
  DispatchedEventDetails()
      : window(nullptr), client_id(kInvalidClientId), accelerator(nullptr) {}

  bool IsNonclientArea() const { return client_id == kNonclientAreaId; }
  bool IsClientArea() const { return client_id == kClientAreaId; }

  ServerWindow* window;
  ClientSpecificId client_id;
  std::unique_ptr<ui::Event> event;
  Accelerator* accelerator;
};

class TestEventDispatcherDelegate : public EventDispatcherDelegate {
 public:
  // Delegate interface used by this class to release capture on event
  // dispatcher.
  class Delegate {
   public:
    virtual void ReleaseCapture() = 0;
  };

  explicit TestEventDispatcherDelegate(Delegate* delegate)
      : delegate_(delegate),
        focused_window_(nullptr),
        lost_capture_window_(nullptr),
        last_accelerator_(0) {}
  ~TestEventDispatcherDelegate() override {}

  ui::Event* last_event_target_not_found() {
    return last_event_target_not_found_.get();
  }

  uint32_t GetAndClearLastAccelerator() {
    uint32_t return_value = last_accelerator_;
    last_accelerator_ = 0;
    return return_value;
  }

  void set_root(ServerWindow* root) { root_ = root; }

  // Returns the last dispatched event, or null if there are no more.
  std::unique_ptr<DispatchedEventDetails>
  GetAndAdvanceDispatchedEventDetails() {
    if (dispatched_event_queue_.empty())
      return nullptr;

    std::unique_ptr<DispatchedEventDetails> details =
        std::move(dispatched_event_queue_.front());
    dispatched_event_queue_.pop();
    return details;
  }

  ServerWindow* GetAndClearLastFocusedWindow() {
    ServerWindow* result = focused_window_;
    focused_window_ = nullptr;
    return result;
  }

  bool has_queued_events() const { return !dispatched_event_queue_.empty(); }
  ServerWindow* lost_capture_window() { return lost_capture_window_; }

  // EventDispatcherDelegate:
  void SetFocusedWindowFromEventDispatcher(ServerWindow* window) override {
    focused_window_ = window;
  }

 private:
  // EventDispatcherDelegate:
  void OnAccelerator(uint32_t accelerator, const ui::Event& event) override {
    EXPECT_EQ(0u, last_accelerator_);
    last_accelerator_ = accelerator;
  }
  ServerWindow* GetFocusedWindowForEventDispatcher() override {
    return focused_window_;
  }
  void SetNativeCapture(ServerWindow* window) override {}
  void ReleaseNativeCapture() override {
    if (delegate_)
      delegate_->ReleaseCapture();
  }
  void OnServerWindowCaptureLost(ServerWindow* window) override {
    lost_capture_window_ = window;
  }
  void OnMouseCursorLocationChanged(const gfx::Point& point) override {}
  void DispatchInputEventToWindow(ServerWindow* target,
                                  ClientSpecificId client_id,
                                  const ui::Event& event,
                                  Accelerator* accelerator) override {
    std::unique_ptr<DispatchedEventDetails> details(new DispatchedEventDetails);
    details->window = target;
    details->client_id = client_id;
    details->event = ui::Event::Clone(event);
    details->accelerator = accelerator;
    dispatched_event_queue_.push(std::move(details));
  }
  ClientSpecificId GetEventTargetClientId(const ServerWindow* window,
                                          bool in_nonclient_area) override {
    return in_nonclient_area ? kNonclientAreaId : kClientAreaId;
  }
  ServerWindow* GetRootWindowContaining(const gfx::Point& location) override {
    return root_;
  }
  void OnEventTargetNotFound(const ui::Event& event) override {
    last_event_target_not_found_ = ui::Event::Clone(event);
  }

  Delegate* delegate_;
  ServerWindow* focused_window_;
  ServerWindow* lost_capture_window_;
  uint32_t last_accelerator_;
  std::queue<std::unique_ptr<DispatchedEventDetails>> dispatched_event_queue_;
  ServerWindow* root_ = nullptr;
  std::unique_ptr<ui::Event> last_event_target_not_found_;

  DISALLOW_COPY_AND_ASSIGN(TestEventDispatcherDelegate);
};

// Used by RunMouseEventTests(). Can identify up to two generated events. The
// first ServerWindow and two points identify the first event, the second
// ServerWindow and points identify the second event. If only one event is
// generated set the second window to null.
struct MouseEventTest {
  ui::MouseEvent input_event;
  ServerWindow* expected_target_window1;
  gfx::Point expected_root_location1;
  gfx::Point expected_location1;
  ServerWindow* expected_target_window2;
  gfx::Point expected_root_location2;
  gfx::Point expected_location2;
};

// Verifies |details| matches the supplied ServerWindow and points.
void ExpectDispatchedEventDetailsMatches(const DispatchedEventDetails* details,
                                         ServerWindow* target,
                                         const gfx::Point& root_location,
                                         const gfx::Point& location) {
  if (!target) {
    ASSERT_FALSE(details);
    return;
  }

  ASSERT_EQ(target, details->window);
  ASSERT_TRUE(details->event);
  ASSERT_TRUE(details->event->IsLocatedEvent());
  ASSERT_TRUE(details->IsClientArea());
  ASSERT_EQ(root_location, details->event->AsLocatedEvent()->root_location());
  ASSERT_EQ(location, details->event->AsLocatedEvent()->location());
}

void RunMouseEventTests(EventDispatcher* dispatcher,
                        TestEventDispatcherDelegate* dispatcher_delegate,
                        MouseEventTest* tests,
                        size_t test_count) {
  for (size_t i = 0; i < test_count; ++i) {
    const MouseEventTest& test = tests[i];
    ASSERT_FALSE(dispatcher_delegate->has_queued_events())
        << " unexpected queued events before running " << i;
    if (test.input_event.IsMouseWheelEvent())
      dispatcher->ProcessEvent(test.input_event);
    else
      dispatcher->ProcessEvent(ui::PointerEvent(test.input_event));

    std::unique_ptr<DispatchedEventDetails> details =
        dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
    ASSERT_NO_FATAL_FAILURE(ExpectDispatchedEventDetailsMatches(
        details.get(), test.expected_target_window1,
        test.expected_root_location1, test.expected_location1))
        << " details don't match " << i;
    details = dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
    ASSERT_NO_FATAL_FAILURE(ExpectDispatchedEventDetailsMatches(
        details.get(), test.expected_target_window2,
        test.expected_root_location2, test.expected_location2))
        << " details2 don't match " << i;
    ASSERT_FALSE(dispatcher_delegate->has_queued_events())
        << " unexpected queued events after running " << i;
  }
}

}  // namespace

// Test fixture for EventDispatcher with friend access to verify the internal
// state. Setup creates a TestServerWindowDelegate, a visible root ServerWindow,
// a TestEventDispatcher and the EventDispatcher for testing.
class EventDispatcherTest : public testing::Test,
                            public TestEventDispatcherDelegate::Delegate {
 public:
  EventDispatcherTest() {}
  ~EventDispatcherTest() override {}

  ServerWindow* root_window() { return root_window_.get(); }
  TestEventDispatcherDelegate* test_event_dispatcher_delegate() {
    return test_event_dispatcher_delegate_.get();
  }
  EventDispatcher* event_dispatcher() { return event_dispatcher_.get(); }

  bool AreAnyPointersDown() const;
  // Deletes everything created during SetUp()
  void ClearSetup();
  std::unique_ptr<ServerWindow> CreateChildWindowWithParent(
      const WindowId& id,
      ServerWindow* parent);
  // Creates a window which is a child of |root_window_|.
  std::unique_ptr<ServerWindow> CreateChildWindow(const WindowId& id);
  bool IsMouseButtonDown() const;
  bool IsWindowPointerTarget(const ServerWindow* window) const;
  int NumberPointerTargetsForWindow(ServerWindow* window) const;
  ServerWindow* GetActiveSystemModalWindow() const;

 protected:
  // testing::Test:
  void SetUp() override;

 private:
  // TestEventDispatcherDelegate::Delegate:
  void ReleaseCapture() override {
    event_dispatcher_->SetCaptureWindow(nullptr, kInvalidClientId);
  }

  std::unique_ptr<TestServerWindowDelegate> window_delegate_;
  std::unique_ptr<ServerWindow> root_window_;
  std::unique_ptr<TestEventDispatcherDelegate> test_event_dispatcher_delegate_;
  std::unique_ptr<EventDispatcher> event_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(EventDispatcherTest);
};

bool EventDispatcherTest::AreAnyPointersDown() const {
  return EventDispatcherTestApi(event_dispatcher_.get()).AreAnyPointersDown();
}

void EventDispatcherTest::ClearSetup() {
  window_delegate_.reset();
  root_window_.reset();
  test_event_dispatcher_delegate_.reset();
  event_dispatcher_.reset();
}

std::unique_ptr<ServerWindow> EventDispatcherTest::CreateChildWindowWithParent(
    const WindowId& id,
    ServerWindow* parent) {
  std::unique_ptr<ServerWindow> child(
      new ServerWindow(window_delegate_.get(), id));
  parent->Add(child.get());
  child->SetVisible(true);
  EnableHitTest(child.get());
  return child;
}

std::unique_ptr<ServerWindow> EventDispatcherTest::CreateChildWindow(
    const WindowId& id) {
  return CreateChildWindowWithParent(id, root_window_.get());
}

bool EventDispatcherTest::IsMouseButtonDown() const {
  return EventDispatcherTestApi(event_dispatcher_.get()).is_mouse_button_down();
}

bool EventDispatcherTest::IsWindowPointerTarget(
    const ServerWindow* window) const {
  return EventDispatcherTestApi(event_dispatcher_.get())
      .IsWindowPointerTarget(window);
}

int EventDispatcherTest::NumberPointerTargetsForWindow(
    ServerWindow* window) const {
  return EventDispatcherTestApi(event_dispatcher_.get())
      .NumberPointerTargetsForWindow(window);
}

ServerWindow* EventDispatcherTest::GetActiveSystemModalWindow() const {
  ModalWindowController* mwc =
      EventDispatcherTestApi(event_dispatcher_.get()).modal_window_controller();
  return ModalWindowControllerTestApi(mwc).GetActiveSystemModalWindow();
}

void EventDispatcherTest::SetUp() {
  testing::Test::SetUp();

  window_delegate_.reset(new TestServerWindowDelegate());
  root_window_.reset(new ServerWindow(window_delegate_.get(), WindowId(1, 2)));
  window_delegate_->set_root_window(root_window_.get());
  root_window_->SetVisible(true);

  test_event_dispatcher_delegate_.reset(new TestEventDispatcherDelegate(this));
  event_dispatcher_.reset(
      new EventDispatcher(test_event_dispatcher_delegate_.get()));
  test_event_dispatcher_delegate_->set_root(root_window_.get());
}

TEST_F(EventDispatcherTest, ProcessEvent) {
  std::unique_ptr<ServerWindow> child = CreateChildWindow(WindowId(1, 3));

  root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  child->SetBounds(gfx::Rect(10, 10, 20, 20));

  // Send event that is over child.
  const ui::PointerEvent ui_event(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, gfx::Point(20, 25), gfx::Point(20, 25),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  event_dispatcher()->ProcessEvent(ui_event);

  std::unique_ptr<DispatchedEventDetails> details =
      test_event_dispatcher_delegate()->GetAndAdvanceDispatchedEventDetails();
  ASSERT_TRUE(details);
  ASSERT_EQ(child.get(), details->window);

  ASSERT_TRUE(details->event);
  ASSERT_TRUE(details->event->IsPointerEvent());

  ui::PointerEvent* dispatched_event = details->event->AsPointerEvent();
  EXPECT_EQ(gfx::Point(20, 25), dispatched_event->root_location());
  EXPECT_EQ(gfx::Point(10, 15), dispatched_event->location());
}

TEST_F(EventDispatcherTest, ProcessEventNoTarget) {
  // Send event without a target.
  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE);
  event_dispatcher()->ProcessEvent(key);

  // Event wasn't dispatched to a target.
  std::unique_ptr<DispatchedEventDetails> details =
      test_event_dispatcher_delegate()->GetAndAdvanceDispatchedEventDetails();
  EXPECT_FALSE(details);

  // Delegate was informed that there wasn't a target.
  ui::Event* event_out =
      test_event_dispatcher_delegate()->last_event_target_not_found();
  ASSERT_TRUE(event_out);
  EXPECT_TRUE(event_out->IsKeyEvent());
  EXPECT_EQ(ui::VKEY_A, event_out->AsKeyEvent()->key_code());
}

TEST_F(EventDispatcherTest, AcceleratorBasic) {
  ClearSetup();
  TestEventDispatcherDelegate event_dispatcher_delegate(nullptr);
  EventDispatcher dispatcher(&event_dispatcher_delegate);

  uint32_t accelerator_1 = 1;
  mojom::EventMatcherPtr matcher = mus::CreateKeyMatcher(
      ui::mojom::KeyboardCode::W, ui::mojom::kEventFlagControlDown);
  EXPECT_TRUE(dispatcher.AddAccelerator(accelerator_1, std::move(matcher)));

  uint32_t accelerator_2 = 2;
  matcher = mus::CreateKeyMatcher(ui::mojom::KeyboardCode::N,
                                  ui::mojom::kEventFlagNone);
  EXPECT_TRUE(dispatcher.AddAccelerator(accelerator_2, std::move(matcher)));

  // Attempting to add a new accelerator with the same id should fail.
  matcher = mus::CreateKeyMatcher(ui::mojom::KeyboardCode::T,
                                  ui::mojom::kEventFlagNone);
  EXPECT_FALSE(dispatcher.AddAccelerator(accelerator_2, std::move(matcher)));

  // Adding the accelerator with the same id should succeed once the existing
  // accelerator is removed.
  dispatcher.RemoveAccelerator(accelerator_2);
  matcher = mus::CreateKeyMatcher(ui::mojom::KeyboardCode::T,
                                  ui::mojom::kEventFlagNone);
  EXPECT_TRUE(dispatcher.AddAccelerator(accelerator_2, std::move(matcher)));

  // Attempting to add an accelerator with the same matcher should fail.
  uint32_t accelerator_3 = 3;
  matcher = mus::CreateKeyMatcher(ui::mojom::KeyboardCode::T,
                                  ui::mojom::kEventFlagNone);
  EXPECT_FALSE(dispatcher.AddAccelerator(accelerator_3, std::move(matcher)));

  matcher = mus::CreateKeyMatcher(ui::mojom::KeyboardCode::T,
                                  ui::mojom::kEventFlagControlDown);
  EXPECT_TRUE(dispatcher.AddAccelerator(accelerator_3, std::move(matcher)));
}

TEST_F(EventDispatcherTest, EventMatching) {
  TestEventDispatcherDelegate* event_dispatcher_delegate =
      test_event_dispatcher_delegate();
  EventDispatcher* dispatcher = event_dispatcher();

  mojom::EventMatcherPtr matcher = mus::CreateKeyMatcher(
      ui::mojom::KeyboardCode::W, ui::mojom::kEventFlagControlDown);
  uint32_t accelerator_1 = 1;
  dispatcher->AddAccelerator(accelerator_1, std::move(matcher));

  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_CONTROL_DOWN);
  dispatcher->ProcessEvent(key);
  EXPECT_EQ(accelerator_1,
            event_dispatcher_delegate->GetAndClearLastAccelerator());

  // EF_NUM_LOCK_ON should be ignored since CreateKeyMatcher defaults to
  // ignoring.
  key = ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_W,
                     ui::EF_CONTROL_DOWN | ui::EF_NUM_LOCK_ON);
  dispatcher->ProcessEvent(key);
  EXPECT_EQ(accelerator_1,
            event_dispatcher_delegate->GetAndClearLastAccelerator());

  key = ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_NONE);
  dispatcher->ProcessEvent(key);
  EXPECT_EQ(0u, event_dispatcher_delegate->GetAndClearLastAccelerator());

  uint32_t accelerator_2 = 2;
  matcher = mus::CreateKeyMatcher(ui::mojom::KeyboardCode::W,
                                  ui::mojom::kEventFlagNone);
  dispatcher->AddAccelerator(accelerator_2, std::move(matcher));
  dispatcher->ProcessEvent(key);
  EXPECT_EQ(accelerator_2,
            event_dispatcher_delegate->GetAndClearLastAccelerator());

  dispatcher->RemoveAccelerator(accelerator_2);
  dispatcher->ProcessEvent(key);
  EXPECT_EQ(0u, event_dispatcher_delegate->GetAndClearLastAccelerator());
}

// Tests that a post-target accelerator is not triggered by ProcessEvent.
TEST_F(EventDispatcherTest, PostTargetAccelerator) {
  TestEventDispatcherDelegate* event_dispatcher_delegate =
      test_event_dispatcher_delegate();
  EventDispatcher* dispatcher = event_dispatcher();

  mojom::EventMatcherPtr matcher = mus::CreateKeyMatcher(
      ui::mojom::KeyboardCode::W, ui::mojom::kEventFlagControlDown);
  matcher->accelerator_phase = ui::mojom::AcceleratorPhase::POST_TARGET;
  uint32_t accelerator_1 = 1;
  dispatcher->AddAccelerator(accelerator_1, std::move(matcher));

  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_CONTROL_DOWN);
  // The post-target accelerator should be fired if there is no focused window.
  dispatcher->ProcessEvent(key);
  EXPECT_EQ(accelerator_1,
            event_dispatcher_delegate->GetAndClearLastAccelerator());
  std::unique_ptr<DispatchedEventDetails> details =
      event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_FALSE(details);

  // Set focused window for EventDispatcher dispatches key events.
  std::unique_ptr<ServerWindow> child = CreateChildWindow(WindowId(1, 3));
  event_dispatcher_delegate->SetFocusedWindowFromEventDispatcher(child.get());

  // With a focused window the event should be dispatched.
  dispatcher->ProcessEvent(key);
  EXPECT_EQ(0u, event_dispatcher_delegate->GetAndClearLastAccelerator());
  details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_TRUE(details);
  EXPECT_TRUE(details->accelerator);

  base::WeakPtr<Accelerator> accelerator_weak_ptr =
      details->accelerator->GetWeakPtr();
  dispatcher->RemoveAccelerator(accelerator_1);
  EXPECT_FALSE(accelerator_weak_ptr);

  // Post deletion there should be no accelerator
  dispatcher->ProcessEvent(key);
  EXPECT_EQ(0u, event_dispatcher_delegate->GetAndClearLastAccelerator());
  details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_TRUE(details);
  EXPECT_FALSE(details->accelerator);
}

TEST_F(EventDispatcherTest, Capture) {
  ServerWindow* root = root_window();
  std::unique_ptr<ServerWindow> child = CreateChildWindow(WindowId(1, 3));

  root->SetBounds(gfx::Rect(0, 0, 100, 100));
  child->SetBounds(gfx::Rect(10, 10, 20, 20));

  MouseEventTest tests[] = {
      // Send a mouse down event over child.
      {ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(20, 25),
                      gfx::Point(20, 25), base::TimeTicks(),
                      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON),
       child.get(), gfx::Point(20, 25), gfx::Point(10, 15), nullptr,
       gfx::Point(), gfx::Point()},

      // Capture should be activated. Let's send a mouse move outside the bounds
      // of the child.
      {ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(50, 50),
                      gfx::Point(50, 50), base::TimeTicks(),
                      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON),
       child.get(), gfx::Point(50, 50), gfx::Point(40, 40), nullptr,
       gfx::Point(), gfx::Point()},
      // Release the mouse and verify that the mouse up event goes to the child.
      {ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(50, 50),
                      gfx::Point(50, 50), base::TimeTicks(),
                      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON),
       child.get(), gfx::Point(50, 50), gfx::Point(40, 40), nullptr,
       gfx::Point(), gfx::Point()},

      // A mouse move at (50, 50) should now go to the root window. As the
      // move crosses between |child| and |root| |child| gets an exit, and
      // |root| the move.
      {ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(50, 50),
                      gfx::Point(50, 50), base::TimeTicks(),
                      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON),
       child.get(), gfx::Point(50, 50), gfx::Point(40, 40), root,
       gfx::Point(50, 50), gfx::Point(50, 50)},

  };
  RunMouseEventTests(event_dispatcher(), test_event_dispatcher_delegate(),
                     tests, arraysize(tests));
}

TEST_F(EventDispatcherTest, CaptureMultipleMouseButtons) {
  std::unique_ptr<ServerWindow> child = CreateChildWindow(WindowId(1, 3));

  root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  child->SetBounds(gfx::Rect(10, 10, 20, 20));

  MouseEventTest tests[] = {
      // Send a mouse down event over child with a left mouse button
      {ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(20, 25),
                      gfx::Point(20, 25), base::TimeTicks(),
                      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON),
       child.get(), gfx::Point(20, 25), gfx::Point(10, 15), nullptr,
       gfx::Point(), gfx::Point()},

      // Capture should be activated. Let's send a mouse move outside the bounds
      // of the child and press the right mouse button too.
      {ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(50, 50),
                      gfx::Point(50, 50), base::TimeTicks(),
                      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON, 0),
       child.get(), gfx::Point(50, 50), gfx::Point(40, 40), nullptr,
       gfx::Point(), gfx::Point()},

      // Release the left mouse button and verify that the mouse up event goes
      // to the child.
      {ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(50, 50),
                      gfx::Point(50, 50), base::TimeTicks(),
                      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON,
                      ui::EF_RIGHT_MOUSE_BUTTON),
       child.get(), gfx::Point(50, 50), gfx::Point(40, 40), nullptr,
       gfx::Point(), gfx::Point()},

      // A mouse move at (50, 50) should still go to the child.
      {ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(50, 50),
                      gfx::Point(50, 50), base::TimeTicks(),
                      ui::EF_LEFT_MOUSE_BUTTON, 0),
       child.get(), gfx::Point(50, 50), gfx::Point(40, 40), nullptr,
       gfx::Point(), gfx::Point()},

  };
  RunMouseEventTests(event_dispatcher(), test_event_dispatcher_delegate(),
                     tests, arraysize(tests));
}

TEST_F(EventDispatcherTest, ClientAreaGoesToOwner) {
  std::unique_ptr<ServerWindow> child = CreateChildWindow(WindowId(1, 3));

  root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  child->SetBounds(gfx::Rect(10, 10, 20, 20));

  child->SetClientArea(gfx::Insets(5, 5, 5, 5), std::vector<gfx::Rect>());

  TestEventDispatcherDelegate* event_dispatcher_delegate =
      test_event_dispatcher_delegate();
  EventDispatcher* dispatcher = event_dispatcher();

  // Start move loop by sending mouse event over non-client area.
  const ui::PointerEvent press_event(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, gfx::Point(12, 12), gfx::Point(12, 12),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  dispatcher->ProcessEvent(press_event);

  // Events should target child and be in the non-client area.
  std::unique_ptr<DispatchedEventDetails> details =
      event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_FALSE(event_dispatcher_delegate->has_queued_events());
  ASSERT_TRUE(details);
  ASSERT_EQ(child.get(), details->window);
  EXPECT_TRUE(details->IsNonclientArea());

  // Move the mouse 5,6 pixels and target is the same.
  const ui::PointerEvent move_event(
      ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(17, 18), gfx::Point(17, 18),
                     base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  dispatcher->ProcessEvent(move_event);

  // Still same target.
  details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_FALSE(event_dispatcher_delegate->has_queued_events());
  ASSERT_EQ(child.get(), details->window);
  EXPECT_TRUE(details->IsNonclientArea());

  // Release the mouse.
  const ui::PointerEvent release_event(ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, gfx::Point(17, 18), gfx::Point(17, 18),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  dispatcher->ProcessEvent(release_event);

  // The event should not have been dispatched to the delegate.
  details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_FALSE(event_dispatcher_delegate->has_queued_events());
  ASSERT_EQ(child.get(), details->window);
  EXPECT_TRUE(details->IsNonclientArea());

  // Press in the client area and verify target/client area. The non-client area
  // should get an exit first.
  const ui::PointerEvent press_event2(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, gfx::Point(21, 22), gfx::Point(21, 22),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  dispatcher->ProcessEvent(press_event2);
  details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_TRUE(event_dispatcher_delegate->has_queued_events());
  ASSERT_EQ(child.get(), details->window);
  EXPECT_TRUE(details->IsNonclientArea());
  EXPECT_EQ(ui::ET_POINTER_EXITED, details->event->type());

  details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_FALSE(event_dispatcher_delegate->has_queued_events());
  ASSERT_EQ(child.get(), details->window);
  EXPECT_TRUE(details->IsClientArea());
  EXPECT_EQ(ui::ET_POINTER_DOWN, details->event->type());
}

TEST_F(EventDispatcherTest, AdditionalClientArea) {
  std::unique_ptr<ServerWindow> child = CreateChildWindow(WindowId(1, 3));

  root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  child->SetBounds(gfx::Rect(10, 10, 20, 20));

  std::vector<gfx::Rect> additional_client_areas;
  additional_client_areas.push_back(gfx::Rect(18, 0, 2, 2));
  child->SetClientArea(gfx::Insets(5, 5, 5, 5), additional_client_areas);

  TestEventDispatcherDelegate* event_dispatcher_delegate =
      test_event_dispatcher_delegate();
  // Press in the additional client area, it should go to the child.
  const ui::PointerEvent press_event(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, gfx::Point(28, 11), gfx::Point(28, 11),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  event_dispatcher()->ProcessEvent(press_event);

  // Events should target child and be in the client area.
  std::unique_ptr<DispatchedEventDetails> details =
      event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_FALSE(event_dispatcher_delegate->has_queued_events());
  ASSERT_EQ(child.get(), details->window);
  EXPECT_TRUE(details->IsClientArea());
}

TEST_F(EventDispatcherTest, HitTestMask) {
  std::unique_ptr<ServerWindow> child = CreateChildWindow(WindowId(1, 3));

  root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  child->SetBounds(gfx::Rect(10, 10, 20, 20));
  child->SetHitTestMask(gfx::Rect(2, 2, 16, 16));

  // Move in the masked area.
  const ui::PointerEvent move1(ui::MouseEvent(
      ui::ET_MOUSE_MOVED, gfx::Point(11, 11), gfx::Point(11, 11),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  event_dispatcher()->ProcessEvent(move1);

  // Event went through the child window and hit the root.
  std::unique_ptr<DispatchedEventDetails> details1 =
      test_event_dispatcher_delegate()->GetAndAdvanceDispatchedEventDetails();
  EXPECT_EQ(root_window(), details1->window);
  EXPECT_TRUE(details1->IsClientArea());

  child->ClearHitTestMask();

  // Move right in the same part of the window.
  const ui::PointerEvent move2(ui::MouseEvent(
      ui::ET_MOUSE_MOVED, gfx::Point(11, 12), gfx::Point(11, 12),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  event_dispatcher()->ProcessEvent(move2);

  // Mouse exits the root.
  std::unique_ptr<DispatchedEventDetails> details2 =
      test_event_dispatcher_delegate()->GetAndAdvanceDispatchedEventDetails();
  EXPECT_EQ(ui::ET_POINTER_EXITED, details2->event->type());

  // Mouse hits the child.
  std::unique_ptr<DispatchedEventDetails> details3 =
      test_event_dispatcher_delegate()->GetAndAdvanceDispatchedEventDetails();
  EXPECT_EQ(child.get(), details3->window);
  EXPECT_TRUE(details3->IsClientArea());
}

TEST_F(EventDispatcherTest, DontFocusOnSecondDown) {
  std::unique_ptr<ServerWindow> child1 = CreateChildWindow(WindowId(1, 3));
  std::unique_ptr<ServerWindow> child2 = CreateChildWindow(WindowId(1, 4));

  root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  child1->SetBounds(gfx::Rect(10, 10, 20, 20));
  child2->SetBounds(gfx::Rect(50, 51, 11, 12));

  TestEventDispatcherDelegate* event_dispatcher_delegate =
      test_event_dispatcher_delegate();
  EventDispatcher* dispatcher = event_dispatcher();

  // Press on child1. First press event should change focus.
  const ui::PointerEvent press_event(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, gfx::Point(12, 12), gfx::Point(12, 12),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  dispatcher->ProcessEvent(press_event);
  std::unique_ptr<DispatchedEventDetails> details =
      event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_FALSE(event_dispatcher_delegate->has_queued_events());
  EXPECT_EQ(child1.get(), details->window);
  EXPECT_EQ(child1.get(),
            event_dispatcher_delegate->GetAndClearLastFocusedWindow());

  // Press (with a different pointer id) on child2. Event should go to child2,
  // but focus should not change.
  const ui::PointerEvent touch_event(ui::TouchEvent(
      ui::ET_TOUCH_PRESSED, gfx::Point(53, 54), 2, base::TimeTicks()));
  dispatcher->ProcessEvent(touch_event);
  details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_FALSE(event_dispatcher_delegate->has_queued_events());
  EXPECT_EQ(child2.get(), details->window);
  EXPECT_EQ(nullptr, event_dispatcher_delegate->GetAndClearLastFocusedWindow());
}

TEST_F(EventDispatcherTest, TwoPointersActive) {
  std::unique_ptr<ServerWindow> child1 = CreateChildWindow(WindowId(1, 3));
  std::unique_ptr<ServerWindow> child2 = CreateChildWindow(WindowId(1, 4));

  root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  child1->SetBounds(gfx::Rect(10, 10, 20, 20));
  child2->SetBounds(gfx::Rect(50, 51, 11, 12));

  TestEventDispatcherDelegate* event_dispatcher_delegate =
      test_event_dispatcher_delegate();
  EventDispatcher* dispatcher = event_dispatcher();

  // Press on child1.
  const ui::PointerEvent touch_event1(ui::TouchEvent(
      ui::ET_TOUCH_PRESSED, gfx::Point(12, 13), 1, base::TimeTicks()));
  dispatcher->ProcessEvent(touch_event1);
  std::unique_ptr<DispatchedEventDetails> details =
      event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_EQ(child1.get(), details->window);

  // Drag over child2, child1 should get the drag.
  const ui::PointerEvent drag_event1(ui::TouchEvent(
      ui::ET_TOUCH_MOVED, gfx::Point(53, 54), 1, base::TimeTicks()));
  dispatcher->ProcessEvent(drag_event1);
  details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_EQ(child1.get(), details->window);

  // Press on child2 with a different touch id.
  const ui::PointerEvent touch_event2(ui::TouchEvent(
      ui::ET_TOUCH_PRESSED, gfx::Point(54, 55), 2, base::TimeTicks()));
  dispatcher->ProcessEvent(touch_event2);
  details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_EQ(child2.get(), details->window);

  // Drag over child1 with id 2, child2 should continue to get the drag.
  const ui::PointerEvent drag_event2(ui::TouchEvent(
      ui::ET_TOUCH_MOVED, gfx::Point(13, 14), 2, base::TimeTicks()));
  dispatcher->ProcessEvent(drag_event2);
  details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_EQ(child2.get(), details->window);

  // Drag again with id 1, child1 should continue to get it.
  dispatcher->ProcessEvent(drag_event1);
  details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_EQ(child1.get(), details->window);

  // Release touch id 1, and click on 2. 2 should get it.
  const ui::PointerEvent touch_release(ui::TouchEvent(
      ui::ET_TOUCH_RELEASED, gfx::Point(54, 55), 1, base::TimeTicks()));
  dispatcher->ProcessEvent(touch_release);
  details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_EQ(child1.get(), details->window);
  const ui::PointerEvent touch_event3(ui::TouchEvent(
      ui::ET_TOUCH_PRESSED, gfx::Point(54, 55), 2, base::TimeTicks()));
  dispatcher->ProcessEvent(touch_event3);
  details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_EQ(child2.get(), details->window);
}

TEST_F(EventDispatcherTest, DestroyWindowWhileGettingEvents) {
  std::unique_ptr<ServerWindow> child = CreateChildWindow(WindowId(1, 3));

  root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  child->SetBounds(gfx::Rect(10, 10, 20, 20));

  TestEventDispatcherDelegate* event_dispatcher_delegate =
      test_event_dispatcher_delegate();
  EventDispatcher* dispatcher = event_dispatcher();

  // Press on child.
  const ui::PointerEvent touch_event1(ui::TouchEvent(
      ui::ET_TOUCH_PRESSED, gfx::Point(12, 13), 1, base::TimeTicks()));
  dispatcher->ProcessEvent(touch_event1);
  std::unique_ptr<DispatchedEventDetails> details =
      event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_FALSE(event_dispatcher_delegate->has_queued_events());
  EXPECT_EQ(child.get(), details->window);

  // Delete child, and continue the drag. Event should not be dispatched.
  child.reset();

  const ui::PointerEvent drag_event1(ui::TouchEvent(
      ui::ET_TOUCH_MOVED, gfx::Point(53, 54), 1, base::TimeTicks()));
  dispatcher->ProcessEvent(drag_event1);
  details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_EQ(nullptr, details.get());
}

TEST_F(EventDispatcherTest, MouseInExtendedHitTestRegion) {
  ServerWindow* root = root_window();
  std::unique_ptr<ServerWindow> child = CreateChildWindow(WindowId(1, 3));

  root->SetBounds(gfx::Rect(0, 0, 100, 100));
  child->SetBounds(gfx::Rect(10, 10, 20, 20));

  TestEventDispatcherDelegate* event_dispatcher_delegate =
      test_event_dispatcher_delegate();
  EventDispatcher* dispatcher = event_dispatcher();

  // Send event that is not over child.
  const ui::PointerEvent ui_event(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, gfx::Point(8, 9), gfx::Point(8, 9),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  dispatcher->ProcessEvent(ui_event);
  std::unique_ptr<DispatchedEventDetails> details =
      event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  ASSERT_EQ(root, details->window);

  // Release the mouse.
  const ui::PointerEvent release_event(ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, gfx::Point(8, 9), gfx::Point(8, 9),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  dispatcher->ProcessEvent(release_event);
  details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_FALSE(event_dispatcher_delegate->has_queued_events());
  ASSERT_EQ(root, details->window);
  EXPECT_TRUE(details->IsClientArea());

  // Change the extended hit test region and send event in extended hit test
  // region. Should result in exit for root, followed by press for child.
  child->set_extended_hit_test_region(gfx::Insets(5, 5, 5, 5));
  dispatcher->ProcessEvent(ui_event);
  details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_EQ(root, details->window);
  EXPECT_EQ(ui::ET_POINTER_EXITED, details->event->type());
  details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  ASSERT_TRUE(details);

  EXPECT_FALSE(event_dispatcher_delegate->has_queued_events());
  EXPECT_TRUE(details->IsNonclientArea());
  ASSERT_EQ(child.get(), details->window);
  EXPECT_EQ(ui::ET_POINTER_DOWN, details->event->type());
  ASSERT_TRUE(details->event.get());
  ASSERT_TRUE(details->event->IsPointerEvent());
  EXPECT_EQ(gfx::Point(-2, -1), details->event->AsPointerEvent()->location());
}

TEST_F(EventDispatcherTest, WheelWhileDown) {
  std::unique_ptr<ServerWindow> child1 = CreateChildWindow(WindowId(1, 3));
  std::unique_ptr<ServerWindow> child2 = CreateChildWindow(WindowId(1, 4));

  root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  child1->SetBounds(gfx::Rect(10, 10, 20, 20));
  child2->SetBounds(gfx::Rect(50, 51, 11, 12));

  MouseEventTest tests[] = {
      // Send a mouse down event over child1.
      {ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(15, 15),
                      gfx::Point(15, 15), base::TimeTicks(),
                      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON),
       child1.get(), gfx::Point(15, 15), gfx::Point(5, 5), nullptr,
       gfx::Point(), gfx::Point()},
      // Send mouse wheel over child2, should go to child1 as it has capture.
      {ui::MouseWheelEvent(gfx::Vector2d(1, 0), gfx::Point(53, 54),
                           gfx::Point(53, 54), base::TimeTicks(), ui::EF_NONE,
                           ui::EF_NONE),
       child1.get(), gfx::Point(53, 54), gfx::Point(43, 44), nullptr,
       gfx::Point(), gfx::Point()},
  };
  RunMouseEventTests(event_dispatcher(), test_event_dispatcher_delegate(),
                     tests, arraysize(tests));
}

// Tests that when explicit capture has been set that all events go to the
// designated window, and that when capture is cleared, events find the
// appropriate target window.
TEST_F(EventDispatcherTest, SetExplicitCapture) {
  ServerWindow* root = root_window();
  std::unique_ptr<ServerWindow> child = CreateChildWindow(WindowId(1, 3));

  root->SetBounds(gfx::Rect(0, 0, 100, 100));
  child->SetBounds(gfx::Rect(10, 10, 20, 20));

  TestEventDispatcherDelegate* event_dispatcher_delegate =
      test_event_dispatcher_delegate();
  EventDispatcher* dispatcher = event_dispatcher();

  {
    // Send all pointer events to the child.
    dispatcher->SetCaptureWindow(child.get(), kClientAreaId);

    // The mouse press should go to the child even though its outside its
    // bounds.
    const ui::PointerEvent left_press_event(ui::MouseEvent(
        ui::ET_MOUSE_PRESSED, gfx::Point(5, 5), gfx::Point(5, 5),
        base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
    dispatcher->ProcessEvent(left_press_event);

    // Events should target child.
    std::unique_ptr<DispatchedEventDetails> details =
        event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();

    ASSERT_TRUE(details);
    ASSERT_EQ(child.get(), details->window);
    EXPECT_TRUE(details->IsClientArea());
    EXPECT_TRUE(IsMouseButtonDown());

    // The mouse down state should update while capture is set.
    const ui::PointerEvent right_press_event(ui::MouseEvent(
        ui::ET_MOUSE_PRESSED, gfx::Point(5, 5), gfx::Point(5, 5),
        base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON,
        ui::EF_RIGHT_MOUSE_BUTTON));
    dispatcher->ProcessEvent(right_press_event);
    details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
    EXPECT_TRUE(IsMouseButtonDown());

    // One button released should not clear mouse down
    const ui::PointerEvent left_release_event(ui::MouseEvent(
        ui::ET_MOUSE_RELEASED, gfx::Point(5, 5), gfx::Point(5, 5),
        base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON,
        ui::EF_LEFT_MOUSE_BUTTON));
    dispatcher->ProcessEvent(left_release_event);
    details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
    EXPECT_TRUE(IsMouseButtonDown());

    // Touch Event while mouse is down should not affect state.
    const ui::PointerEvent touch_event(ui::TouchEvent(
        ui::ET_TOUCH_PRESSED, gfx::Point(15, 15), 2, base::TimeTicks()));
    dispatcher->ProcessEvent(touch_event);
    details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
    EXPECT_TRUE(IsMouseButtonDown());

    // Move event should not affect down
    const ui::PointerEvent move_event(
        ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(15, 5), gfx::Point(15, 5),
                       base::TimeTicks(), ui::EF_RIGHT_MOUSE_BUTTON,
                       ui::EF_RIGHT_MOUSE_BUTTON));
    dispatcher->ProcessEvent(move_event);
    details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
    EXPECT_TRUE(IsMouseButtonDown());

    // All mouse buttons up should clear mouse down.
    const ui::PointerEvent right_release_event(
        ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(5, 5),
                       gfx::Point(5, 5), base::TimeTicks(),
                       ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON));
    dispatcher->ProcessEvent(right_release_event);
    details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
    EXPECT_FALSE(IsMouseButtonDown());
  }

  {
    // Releasing capture and sending the same event will go to the root.
    dispatcher->SetCaptureWindow(nullptr, kClientAreaId);
    const ui::PointerEvent press_event(ui::MouseEvent(
        ui::ET_MOUSE_PRESSED, gfx::Point(5, 5), gfx::Point(5, 5),
        base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
    dispatcher->ProcessEvent(press_event);

    // Events should target the root.
    std::unique_ptr<DispatchedEventDetails> details =
        event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();

    ASSERT_TRUE(details);
    ASSERT_EQ(root, details->window);
  }
}

// This test verifies that explicit capture overrides and resets implicit
// capture.
TEST_F(EventDispatcherTest, ExplicitCaptureOverridesImplicitCapture) {
  ServerWindow* root = root_window();
  std::unique_ptr<ServerWindow> child = CreateChildWindow(WindowId(1, 3));

  root->SetBounds(gfx::Rect(0, 0, 100, 100));
  child->SetBounds(gfx::Rect(10, 10, 20, 20));

  TestEventDispatcherDelegate* event_dispatcher_delegate =
      test_event_dispatcher_delegate();
  EventDispatcher* dispatcher = event_dispatcher();

  // Run some implicit capture tests.
  MouseEventTest tests[] = {
      // Send a mouse down event over child with a left mouse button
      {ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(20, 25),
                      gfx::Point(20, 25), base::TimeTicks(),
                      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON),
       child.get(), gfx::Point(20, 25), gfx::Point(10, 15)},
      // Capture should be activated. Let's send a mouse move outside the bounds
      // of the child and press the right mouse button too.
      {ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(50, 50),
                      gfx::Point(50, 50), base::TimeTicks(),
                      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON, 0),
       child.get(), gfx::Point(50, 50), gfx::Point(40, 40)},
      // Release the left mouse button and verify that the mouse up event goes
      // to the child.
      {ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(50, 50),
                      gfx::Point(50, 50), base::TimeTicks(),
                      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON,
                      ui::EF_RIGHT_MOUSE_BUTTON),
       child.get(), gfx::Point(50, 50), gfx::Point(40, 40)},
      // A mouse move at (50, 50) should still go to the child.
      {ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(50, 50),
                      gfx::Point(50, 50), base::TimeTicks(),
                      ui::EF_LEFT_MOUSE_BUTTON, 0),
       child.get(), gfx::Point(50, 50), gfx::Point(40, 40)},

  };
  RunMouseEventTests(dispatcher, event_dispatcher_delegate, tests,
                     arraysize(tests));

  // Add a second pointer target to the child.
  {
    const ui::PointerEvent touch_event(ui::TouchEvent(
        ui::ET_TOUCH_PRESSED, gfx::Point(12, 13), 1, base::TimeTicks()));
    dispatcher->ProcessEvent(touch_event);
  }

  std::unique_ptr<DispatchedEventDetails> details =
      event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_FALSE(event_dispatcher_delegate->has_queued_events());
  EXPECT_EQ(child.get(), details->window);

  // Verify that no window has explicit capture and hence we did indeed do
  // implicit capture.
  ASSERT_EQ(nullptr, dispatcher->capture_window());

  // Give the root window explicit capture and verify input events over the
  // child go to the root instead.
  dispatcher->SetCaptureWindow(root, kNonclientAreaId);

  // The implicit target should receive a cancel event for each pointer target.
  details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  ASSERT_TRUE(details);
  EXPECT_TRUE(event_dispatcher_delegate->has_queued_events());
  EXPECT_EQ(child.get(), details->window);
  EXPECT_EQ(ui::ET_POINTER_CANCELLED, details->event->type());

  details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  ASSERT_TRUE(details);
  EXPECT_FALSE(event_dispatcher_delegate->has_queued_events());
  EXPECT_EQ(child.get(), details->window);
  EXPECT_EQ(ui::ET_POINTER_EXITED, details->event->type());

  const ui::PointerEvent press_event(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, gfx::Point(15, 15), gfx::Point(15, 15),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  dispatcher->ProcessEvent(press_event);

  // Events should target the root.
  details = event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  ASSERT_TRUE(details);
  ASSERT_EQ(root, details->window);
  ASSERT_TRUE(details->IsNonclientArea());
}

// Tests that setting capture does delete active pointer targets for the capture
// window.
TEST_F(EventDispatcherTest, CaptureUpdatesActivePointerTargets) {
  ServerWindow* root = root_window();
  root->SetBounds(gfx::Rect(0, 0, 100, 100));

  EventDispatcher* dispatcher = event_dispatcher();
  {
    const ui::PointerEvent press_event(ui::MouseEvent(
        ui::ET_MOUSE_PRESSED, gfx::Point(5, 5), gfx::Point(5, 5),
        base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
    dispatcher->ProcessEvent(press_event);

    std::unique_ptr<DispatchedEventDetails> details =
        test_event_dispatcher_delegate()->GetAndAdvanceDispatchedEventDetails();
    ASSERT_TRUE(details);
    ASSERT_EQ(root, details->window);
  }
  {
    const ui::PointerEvent touch_event(ui::TouchEvent(
        ui::ET_TOUCH_PRESSED, gfx::Point(12, 13), 1, base::TimeTicks()));
    dispatcher->ProcessEvent(touch_event);
  }

  ASSERT_TRUE(AreAnyPointersDown());
  ASSERT_TRUE(IsWindowPointerTarget(root));
  EXPECT_EQ(2, NumberPointerTargetsForWindow(root));

  // Setting the capture should clear the implicit pointers for the specified
  // window.
  dispatcher->SetCaptureWindow(root, kNonclientAreaId);
  EXPECT_FALSE(AreAnyPointersDown());
  EXPECT_FALSE(IsWindowPointerTarget(root));
}

// Tests that when explicit capture is changed, that the previous window with
// capture is no longer being observed.
TEST_F(EventDispatcherTest, UpdatingCaptureStopsObservingPreviousCapture) {
  std::unique_ptr<ServerWindow> child1 = CreateChildWindow(WindowId(1, 3));
  std::unique_ptr<ServerWindow> child2 = CreateChildWindow(WindowId(1, 4));

  root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  child1->SetBounds(gfx::Rect(10, 10, 20, 20));
  child2->SetBounds(gfx::Rect(50, 51, 11, 12));

  EventDispatcher* dispatcher = event_dispatcher();
  ASSERT_FALSE(AreAnyPointersDown());
  ASSERT_FALSE(IsWindowPointerTarget(child1.get()));
  ASSERT_FALSE(IsWindowPointerTarget(child2.get()));
  dispatcher->SetCaptureWindow(child1.get(), kClientAreaId);
  dispatcher->SetCaptureWindow(child2.get(), kClientAreaId);
  EXPECT_EQ(child1.get(),
            test_event_dispatcher_delegate()->lost_capture_window());

  // If observing does not stop during the capture update this crashes.
  child1->AddObserver(dispatcher);
}

// Tests that destroying a window with explicit capture clears the capture
// state.
TEST_F(EventDispatcherTest, DestroyingCaptureWindowRemovesExplicitCapture) {
  std::unique_ptr<ServerWindow> child = CreateChildWindow(WindowId(1, 3));
  child->SetBounds(gfx::Rect(10, 10, 20, 20));

  EventDispatcher* dispatcher = event_dispatcher();
  dispatcher->SetCaptureWindow(child.get(), kClientAreaId);
  EXPECT_EQ(child.get(), dispatcher->capture_window());

  ServerWindow* lost_capture_window = child.get();
  child.reset();
  EXPECT_EQ(nullptr, dispatcher->capture_window());
  EXPECT_EQ(lost_capture_window,
            test_event_dispatcher_delegate()->lost_capture_window());
}

// Tests that when |client_id| is set for a window performing capture, that this
// preference is used regardless of whether an event targets the client region.
TEST_F(EventDispatcherTest, CaptureInNonClientAreaOverridesActualPoint) {
  ServerWindow* root = root_window();
  root->SetBounds(gfx::Rect(0, 0, 100, 100));

  root->SetClientArea(gfx::Insets(5, 5, 5, 5), std::vector<gfx::Rect>());
  EventDispatcher* dispatcher = event_dispatcher();
  dispatcher->SetCaptureWindow(root, kNonclientAreaId);

  TestEventDispatcherDelegate* event_dispatcher_delegate =
      test_event_dispatcher_delegate();
  // Press in the client area, it should be marked as non client.
  const ui::PointerEvent press_event(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, gfx::Point(6, 6), gfx::Point(6, 6),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  event_dispatcher()->ProcessEvent(press_event);

  // Events should target child and be in the client area.
  std::unique_ptr<DispatchedEventDetails> details =
      event_dispatcher_delegate->GetAndAdvanceDispatchedEventDetails();
  EXPECT_FALSE(event_dispatcher_delegate->has_queued_events());
  ASSERT_EQ(root, details->window);
  EXPECT_TRUE(details->IsNonclientArea());
}

TEST_F(EventDispatcherTest, ProcessPointerEvents) {
  std::unique_ptr<ServerWindow> child = CreateChildWindow(WindowId(1, 3));

  root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  child->SetBounds(gfx::Rect(10, 10, 20, 20));

  {
    const ui::PointerEvent pointer_event(ui::MouseEvent(
        ui::ET_MOUSE_PRESSED, gfx::Point(20, 25), gfx::Point(20, 25),
        base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
    event_dispatcher()->ProcessEvent(pointer_event);

    std::unique_ptr<DispatchedEventDetails> details =
        test_event_dispatcher_delegate()->GetAndAdvanceDispatchedEventDetails();
    ASSERT_TRUE(details);
    ASSERT_EQ(child.get(), details->window);

    ASSERT_TRUE(details->event);
    ASSERT_TRUE(details->event->IsPointerEvent());

    ui::PointerEvent* dispatched_event = details->event->AsPointerEvent();
    EXPECT_EQ(gfx::Point(20, 25), dispatched_event->root_location());
    EXPECT_EQ(gfx::Point(10, 15), dispatched_event->location());
  }

  {
    const int touch_id = 3;
    const ui::PointerEvent pointer_event(
        ui::TouchEvent(ui::ET_TOUCH_RELEASED, gfx::Point(25, 20), touch_id,
                       base::TimeTicks()));
    event_dispatcher()->ProcessEvent(pointer_event);

    std::unique_ptr<DispatchedEventDetails> details =
        test_event_dispatcher_delegate()->GetAndAdvanceDispatchedEventDetails();
    ASSERT_TRUE(details);
    ASSERT_EQ(child.get(), details->window);

    ASSERT_TRUE(details->event);
    ASSERT_TRUE(details->event->IsPointerEvent());

    ui::PointerEvent* dispatched_event = details->event->AsPointerEvent();
    EXPECT_EQ(gfx::Point(25, 20), dispatched_event->root_location());
    EXPECT_EQ(gfx::Point(15, 10), dispatched_event->location());
    EXPECT_EQ(touch_id, dispatched_event->pointer_id());
  }
}

TEST_F(EventDispatcherTest, ResetClearsPointerDown) {
  std::unique_ptr<ServerWindow> child = CreateChildWindow(WindowId(1, 3));

  root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  child->SetBounds(gfx::Rect(10, 10, 20, 20));

  // Send event that is over child.
  const ui::PointerEvent ui_event(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, gfx::Point(20, 25), gfx::Point(20, 25),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  event_dispatcher()->ProcessEvent(ui_event);

  std::unique_ptr<DispatchedEventDetails> details =
      test_event_dispatcher_delegate()->GetAndAdvanceDispatchedEventDetails();
  ASSERT_TRUE(details);
  ASSERT_EQ(child.get(), details->window);

  EXPECT_TRUE(AreAnyPointersDown());

  event_dispatcher()->Reset();
  EXPECT_FALSE(test_event_dispatcher_delegate()->has_queued_events());
  EXPECT_FALSE(AreAnyPointersDown());
}

TEST_F(EventDispatcherTest, ResetClearsCapture) {
  ServerWindow* root = root_window();
  root->SetBounds(gfx::Rect(0, 0, 100, 100));

  root->SetClientArea(gfx::Insets(5, 5, 5, 5), std::vector<gfx::Rect>());
  EventDispatcher* dispatcher = event_dispatcher();
  dispatcher->SetCaptureWindow(root, kNonclientAreaId);

  event_dispatcher()->Reset();
  EXPECT_FALSE(test_event_dispatcher_delegate()->has_queued_events());
  EXPECT_EQ(nullptr, event_dispatcher()->capture_window());
}

// Tests that events on a modal parent target the modal child.
TEST_F(EventDispatcherTest, ModalWindowEventOnModalParent) {
  std::unique_ptr<ServerWindow> w1 = CreateChildWindow(WindowId(1, 3));
  std::unique_ptr<ServerWindow> w2 = CreateChildWindow(WindowId(1, 5));

  root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  w1->SetBounds(gfx::Rect(10, 10, 30, 30));
  w2->SetBounds(gfx::Rect(50, 10, 10, 10));

  w1->AddTransientWindow(w2.get());
  w2->SetModal();

  // Send event that is over |w1|.
  const ui::PointerEvent mouse_pressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, gfx::Point(15, 15), gfx::Point(15, 15),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  event_dispatcher()->ProcessEvent(mouse_pressed);

  std::unique_ptr<DispatchedEventDetails> details =
      test_event_dispatcher_delegate()->GetAndAdvanceDispatchedEventDetails();
  ASSERT_TRUE(details);
  EXPECT_EQ(w2.get(), details->window);
  EXPECT_TRUE(details->IsNonclientArea());

  ASSERT_TRUE(details->event);
  ASSERT_TRUE(details->event->IsPointerEvent());

  ui::PointerEvent* dispatched_event = details->event->AsPointerEvent();
  EXPECT_EQ(gfx::Point(15, 15), dispatched_event->root_location());
  EXPECT_EQ(gfx::Point(-35, 5), dispatched_event->location());
}

// Tests that events on a modal child target the modal child itself.
TEST_F(EventDispatcherTest, ModalWindowEventOnModalChild) {
  std::unique_ptr<ServerWindow> w1 = CreateChildWindow(WindowId(1, 3));
  std::unique_ptr<ServerWindow> w2 = CreateChildWindow(WindowId(1, 5));

  root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  w1->SetBounds(gfx::Rect(10, 10, 30, 30));
  w2->SetBounds(gfx::Rect(50, 10, 10, 10));

  w1->AddTransientWindow(w2.get());
  w2->SetModal();

  // Send event that is over |w2|.
  const ui::PointerEvent mouse_pressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, gfx::Point(55, 15), gfx::Point(55, 15),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  event_dispatcher()->ProcessEvent(mouse_pressed);

  std::unique_ptr<DispatchedEventDetails> details =
      test_event_dispatcher_delegate()->GetAndAdvanceDispatchedEventDetails();
  ASSERT_TRUE(details);
  EXPECT_EQ(w2.get(), details->window);
  EXPECT_TRUE(details->IsClientArea());

  ASSERT_TRUE(details->event);
  ASSERT_TRUE(details->event->IsPointerEvent());

  ui::PointerEvent* dispatched_event = details->event->AsPointerEvent();
  EXPECT_EQ(gfx::Point(55, 15), dispatched_event->root_location());
  EXPECT_EQ(gfx::Point(5, 5), dispatched_event->location());
}

// Tests that events on an unrelated window are not affected by the modal
// window.
TEST_F(EventDispatcherTest, ModalWindowEventOnUnrelatedWindow) {
  std::unique_ptr<ServerWindow> w1 = CreateChildWindow(WindowId(1, 3));
  std::unique_ptr<ServerWindow> w2 = CreateChildWindow(WindowId(1, 5));
  std::unique_ptr<ServerWindow> w3 = CreateChildWindow(WindowId(1, 6));

  root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  w1->SetBounds(gfx::Rect(10, 10, 30, 30));
  w2->SetBounds(gfx::Rect(50, 10, 10, 10));
  w3->SetBounds(gfx::Rect(70, 10, 10, 10));

  w1->AddTransientWindow(w2.get());
  w2->SetModal();

  // Send event that is over |w3|.
  const ui::PointerEvent mouse_pressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, gfx::Point(75, 15), gfx::Point(75, 15),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  event_dispatcher()->ProcessEvent(mouse_pressed);

  std::unique_ptr<DispatchedEventDetails> details =
      test_event_dispatcher_delegate()->GetAndAdvanceDispatchedEventDetails();
  ASSERT_TRUE(details);
  EXPECT_EQ(w3.get(), details->window);
  EXPECT_TRUE(details->IsClientArea());

  ASSERT_TRUE(details->event);
  ASSERT_TRUE(details->event->IsPointerEvent());

  ui::PointerEvent* dispatched_event = details->event->AsPointerEvent();
  EXPECT_EQ(gfx::Point(75, 15), dispatched_event->root_location());
  EXPECT_EQ(gfx::Point(5, 5), dispatched_event->location());
}

// Tests that events events on a descendant of a modal parent target the modal
// child.
TEST_F(EventDispatcherTest, ModalWindowEventOnDescendantOfModalParent) {
  std::unique_ptr<ServerWindow> w1 = CreateChildWindow(WindowId(1, 3));
  std::unique_ptr<ServerWindow> w11 =
      CreateChildWindowWithParent(WindowId(1, 4), w1.get());
  std::unique_ptr<ServerWindow> w2 = CreateChildWindow(WindowId(1, 5));

  root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  w1->SetBounds(gfx::Rect(10, 10, 30, 30));
  w11->SetBounds(gfx::Rect(10, 10, 10, 10));
  w2->SetBounds(gfx::Rect(50, 10, 10, 10));

  w1->AddTransientWindow(w2.get());
  w2->SetModal();

  // Send event that is over |w11|.
  const ui::PointerEvent mouse_pressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, gfx::Point(25, 25), gfx::Point(25, 25),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  event_dispatcher()->ProcessEvent(mouse_pressed);

  std::unique_ptr<DispatchedEventDetails> details =
      test_event_dispatcher_delegate()->GetAndAdvanceDispatchedEventDetails();
  ASSERT_TRUE(details);
  EXPECT_EQ(w2.get(), details->window);
  EXPECT_TRUE(details->IsNonclientArea());

  ASSERT_TRUE(details->event);
  ASSERT_TRUE(details->event->IsPointerEvent());

  ui::PointerEvent* dispatched_event = details->event->AsPointerEvent();
  EXPECT_EQ(gfx::Point(25, 25), dispatched_event->root_location());
  EXPECT_EQ(gfx::Point(-25, 15), dispatched_event->location());
}

// Tests that events on a system modal window target the modal window itself.
TEST_F(EventDispatcherTest, ModalWindowEventOnSystemModal) {
  std::unique_ptr<ServerWindow> w1 = CreateChildWindow(WindowId(1, 3));

  root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  w1->SetBounds(gfx::Rect(10, 10, 30, 30));
  w1->SetModal();

  // Send event that is over |w1|.
  const ui::PointerEvent mouse_pressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, gfx::Point(15, 15), gfx::Point(15, 15),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  event_dispatcher()->ProcessEvent(mouse_pressed);

  std::unique_ptr<DispatchedEventDetails> details =
      test_event_dispatcher_delegate()->GetAndAdvanceDispatchedEventDetails();
  ASSERT_TRUE(details);
  EXPECT_EQ(w1.get(), details->window);
  EXPECT_TRUE(details->IsClientArea());

  ASSERT_TRUE(details->event);
  ASSERT_TRUE(details->event->IsPointerEvent());

  ui::PointerEvent* dispatched_event = details->event->AsPointerEvent();
  EXPECT_EQ(gfx::Point(15, 15), dispatched_event->root_location());
  EXPECT_EQ(gfx::Point(5, 5), dispatched_event->location());
}

// Tests that events outside of system modal window target the modal window.
TEST_F(EventDispatcherTest, ModalWindowEventOutsideSystemModal) {
  std::unique_ptr<ServerWindow> w1 = CreateChildWindow(WindowId(1, 3));

  root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  w1->SetBounds(gfx::Rect(10, 10, 30, 30));
  w1->SetModal();
  event_dispatcher()->AddSystemModalWindow(w1.get());

  // Send event that is over |w1|.
  const ui::PointerEvent mouse_pressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, gfx::Point(45, 15), gfx::Point(45, 15),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  event_dispatcher()->ProcessEvent(mouse_pressed);

  std::unique_ptr<DispatchedEventDetails> details =
      test_event_dispatcher_delegate()->GetAndAdvanceDispatchedEventDetails();
  ASSERT_TRUE(details);
  EXPECT_EQ(w1.get(), details->window);
  EXPECT_TRUE(details->IsNonclientArea());

  ASSERT_TRUE(details->event);
  ASSERT_TRUE(details->event->IsPointerEvent());

  ui::PointerEvent* dispatched_event = details->event->AsPointerEvent();
  EXPECT_EQ(gfx::Point(45, 15), dispatched_event->root_location());
  EXPECT_EQ(gfx::Point(35, 5), dispatched_event->location());
}

// Tests that setting capture to a descendant of a modal parent fails.
TEST_F(EventDispatcherTest, ModalWindowSetCaptureDescendantOfModalParent) {
  std::unique_ptr<ServerWindow> w1 = CreateChildWindow(WindowId(1, 3));
  std::unique_ptr<ServerWindow> w11 =
      CreateChildWindowWithParent(WindowId(1, 4), w1.get());
  std::unique_ptr<ServerWindow> w2 = CreateChildWindow(WindowId(1, 5));

  root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  w1->SetBounds(gfx::Rect(10, 10, 30, 30));
  w11->SetBounds(gfx::Rect(10, 10, 10, 10));
  w2->SetBounds(gfx::Rect(50, 10, 10, 10));

  w1->AddTransientWindow(w2.get());
  w2->SetModal();

  EXPECT_FALSE(event_dispatcher()->SetCaptureWindow(w11.get(), kClientAreaId));
  EXPECT_EQ(nullptr, event_dispatcher()->capture_window());
}

// Tests that setting capture to a window unrelated to a modal parent works.
TEST_F(EventDispatcherTest, ModalWindowSetCaptureUnrelatedWindow) {
  std::unique_ptr<ServerWindow> w1 = CreateChildWindow(WindowId(1, 3));
  std::unique_ptr<ServerWindow> w2 = CreateChildWindow(WindowId(1, 4));
  std::unique_ptr<ServerWindow> w3 = CreateChildWindow(WindowId(1, 5));

  root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  w1->SetBounds(gfx::Rect(10, 10, 30, 30));
  w2->SetBounds(gfx::Rect(50, 10, 10, 10));
  w3->SetBounds(gfx::Rect(70, 10, 10, 10));

  w1->AddTransientWindow(w2.get());
  w2->SetModal();

  EXPECT_TRUE(event_dispatcher()->SetCaptureWindow(w3.get(), kClientAreaId));
  EXPECT_EQ(w3.get(), event_dispatcher()->capture_window());
}

// Tests that setting capture fails when there is a system modal window.
TEST_F(EventDispatcherTest, ModalWindowSystemSetCapture) {
  std::unique_ptr<ServerWindow> w1 = CreateChildWindow(WindowId(1, 3));
  std::unique_ptr<ServerWindow> w2 = CreateChildWindow(WindowId(1, 4));

  root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  w1->SetBounds(gfx::Rect(10, 10, 30, 30));
  w2->SetBounds(gfx::Rect(50, 10, 10, 10));

  event_dispatcher()->AddSystemModalWindow(w2.get());

  EXPECT_FALSE(event_dispatcher()->SetCaptureWindow(w1.get(), kClientAreaId));
  EXPECT_EQ(nullptr, event_dispatcher()->capture_window());
}

// Tests having multiple system modal windows.
TEST_F(EventDispatcherTest, ModalWindowMultipleSystemModals) {
  std::unique_ptr<ServerWindow> w1 = CreateChildWindow(WindowId(1, 3));
  std::unique_ptr<ServerWindow> w2 = CreateChildWindow(WindowId(1, 4));
  std::unique_ptr<ServerWindow> w3 = CreateChildWindow(WindowId(1, 5));

  w2->SetVisible(false);

  // In the beginning, there should be no active system modal window.
  EXPECT_EQ(nullptr, GetActiveSystemModalWindow());

  // Add a visible system modal window. It should become the active one.
  event_dispatcher()->AddSystemModalWindow(w1.get());
  EXPECT_EQ(w1.get(), GetActiveSystemModalWindow());

  // Add an invisible system modal window. It should not change the active one.
  event_dispatcher()->AddSystemModalWindow(w2.get());
  EXPECT_EQ(w1.get(), GetActiveSystemModalWindow());

  // Add another visible system modal window. It should become the active one.
  event_dispatcher()->AddSystemModalWindow(w3.get());
  EXPECT_EQ(w3.get(), GetActiveSystemModalWindow());

  // Make an existing system modal window visible. It should become the active
  // one.
  w2->SetVisible(true);
  EXPECT_EQ(w2.get(), GetActiveSystemModalWindow());

  // Remove the active system modal window. Next one should become active.
  w2.reset();
  EXPECT_EQ(w3.get(), GetActiveSystemModalWindow());

  // Remove an inactive system modal window. It should not change the active
  // one.
  w1.reset();
  EXPECT_EQ(w3.get(), GetActiveSystemModalWindow());

  // Remove the last remaining system modal window. There should be no active
  // one anymore.
  w3.reset();
  EXPECT_EQ(nullptr, GetActiveSystemModalWindow());
}

TEST_F(EventDispatcherTest, CaptureNotResetOnParentChange) {
  std::unique_ptr<ServerWindow> w1 = CreateChildWindow(WindowId(1, 3));
  DisableHitTest(w1.get());
  std::unique_ptr<ServerWindow> w11 =
      CreateChildWindowWithParent(WindowId(1, 4), w1.get());
  std::unique_ptr<ServerWindow> w2 = CreateChildWindow(WindowId(1, 5));
  DisableHitTest(w2.get());

  root_window()->SetBounds(gfx::Rect(0, 0, 100, 100));
  w1->SetBounds(gfx::Rect(0, 0, 100, 100));
  w11->SetBounds(gfx::Rect(10, 10, 10, 10));
  w2->SetBounds(gfx::Rect(0, 0, 100, 100));

  // Send event that is over |w11|.
  const ui::PointerEvent mouse_pressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, gfx::Point(15, 15), gfx::Point(15, 15),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  event_dispatcher()->ProcessEvent(mouse_pressed);
  event_dispatcher()->SetCaptureWindow(w11.get(), kClientAreaId);

  std::unique_ptr<DispatchedEventDetails> details =
      test_event_dispatcher_delegate()->GetAndAdvanceDispatchedEventDetails();
  ASSERT_TRUE(details);
  EXPECT_EQ(w11.get(), details->window);
  EXPECT_TRUE(details->IsClientArea());

  // Move |w11| to |w2| and verify the mouse is still down, and |w11| has
  // capture.
  w2->Add(w11.get());
  EXPECT_TRUE(IsMouseButtonDown());
  EXPECT_EQ(w11.get(),
            EventDispatcherTestApi(event_dispatcher()).capture_window());
}

TEST_F(EventDispatcherTest, ChangeCaptureFromClientToNonclient) {
  std::unique_ptr<ServerWindow> child = CreateChildWindow(WindowId(1, 3));
  event_dispatcher()->SetCaptureWindow(child.get(), kNonclientAreaId);
  EXPECT_EQ(kNonclientAreaId,
            event_dispatcher()->capture_window_client_id());
  EXPECT_EQ(nullptr, test_event_dispatcher_delegate()->lost_capture_window());
  event_dispatcher()->SetCaptureWindow(child.get(), kClientAreaId);
  // Changing capture from client to non-client should notify the delegate.
  // The delegate can decide if it really wants to forward the event or not.
  EXPECT_EQ(child.get(),
            test_event_dispatcher_delegate()->lost_capture_window());
  EXPECT_EQ(child.get(), event_dispatcher()->capture_window());
  EXPECT_EQ(kClientAreaId, event_dispatcher()->capture_window_client_id());
}

}  // namespace test
}  // namespace ws
}  // namespace mus
