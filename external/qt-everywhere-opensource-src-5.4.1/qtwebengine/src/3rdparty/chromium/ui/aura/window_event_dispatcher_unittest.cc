// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_event_dispatcher.h"

#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/event_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_cursor_client.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_utils.h"
#include "ui/events/gestures/gesture_configuration.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/test_event_handler.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/transform.h"

namespace aura {
namespace {

// A delegate that always returns a non-client component for hit tests.
class NonClientDelegate : public test::TestWindowDelegate {
 public:
  NonClientDelegate()
      : non_client_count_(0),
        mouse_event_count_(0),
        mouse_event_flags_(0x0) {
  }
  virtual ~NonClientDelegate() {}

  int non_client_count() const { return non_client_count_; }
  gfx::Point non_client_location() const { return non_client_location_; }
  int mouse_event_count() const { return mouse_event_count_; }
  gfx::Point mouse_event_location() const { return mouse_event_location_; }
  int mouse_event_flags() const { return mouse_event_flags_; }

  virtual int GetNonClientComponent(const gfx::Point& location) const OVERRIDE {
    NonClientDelegate* self = const_cast<NonClientDelegate*>(this);
    self->non_client_count_++;
    self->non_client_location_ = location;
    return HTTOPLEFT;
  }
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    mouse_event_count_++;
    mouse_event_location_ = event->location();
    mouse_event_flags_ = event->flags();
    event->SetHandled();
  }

 private:
  int non_client_count_;
  gfx::Point non_client_location_;
  int mouse_event_count_;
  gfx::Point mouse_event_location_;
  int mouse_event_flags_;

  DISALLOW_COPY_AND_ASSIGN(NonClientDelegate);
};

// A simple event handler that consumes key events.
class ConsumeKeyHandler : public ui::test::TestEventHandler {
 public:
  ConsumeKeyHandler() {}
  virtual ~ConsumeKeyHandler() {}

  // Overridden from ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE {
    ui::test::TestEventHandler::OnKeyEvent(event);
    event->StopPropagation();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ConsumeKeyHandler);
};

bool IsFocusedWindow(aura::Window* window) {
  return client::GetFocusClient(window)->GetFocusedWindow() == window;
}

}  // namespace

typedef test::AuraTestBase WindowEventDispatcherTest;

TEST_F(WindowEventDispatcherTest, OnHostMouseEvent) {
  // Create two non-overlapping windows so we don't have to worry about which
  // is on top.
  scoped_ptr<NonClientDelegate> delegate1(new NonClientDelegate());
  scoped_ptr<NonClientDelegate> delegate2(new NonClientDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  gfx::Rect bounds1(100, 200, kWindowWidth, kWindowHeight);
  gfx::Rect bounds2(300, 400, kWindowWidth, kWindowHeight);
  scoped_ptr<aura::Window> window1(CreateTestWindowWithDelegate(
      delegate1.get(), -1234, bounds1, root_window()));
  scoped_ptr<aura::Window> window2(CreateTestWindowWithDelegate(
      delegate2.get(), -5678, bounds2, root_window()));

  // Send a mouse event to window1.
  gfx::Point point(101, 201);
  ui::MouseEvent event1(
      ui::ET_MOUSE_PRESSED, point, point, ui::EF_LEFT_MOUSE_BUTTON,
      ui::EF_LEFT_MOUSE_BUTTON);
  DispatchEventUsingWindowDispatcher(&event1);

  // Event was tested for non-client area for the target window.
  EXPECT_EQ(1, delegate1->non_client_count());
  EXPECT_EQ(0, delegate2->non_client_count());
  // The non-client component test was in local coordinates.
  EXPECT_EQ(gfx::Point(1, 1), delegate1->non_client_location());
  // Mouse event was received by target window.
  EXPECT_EQ(1, delegate1->mouse_event_count());
  EXPECT_EQ(0, delegate2->mouse_event_count());
  // Event was in local coordinates.
  EXPECT_EQ(gfx::Point(1, 1), delegate1->mouse_event_location());
  // Non-client flag was set.
  EXPECT_TRUE(delegate1->mouse_event_flags() & ui::EF_IS_NON_CLIENT);
}

TEST_F(WindowEventDispatcherTest, RepostEvent) {
  // Test RepostEvent in RootWindow. It only works for Mouse Press.
  EXPECT_FALSE(Env::GetInstance()->IsMouseButtonDown());
  gfx::Point point(10, 10);
  ui::MouseEvent event(
      ui::ET_MOUSE_PRESSED, point, point, ui::EF_LEFT_MOUSE_BUTTON,
      ui::EF_LEFT_MOUSE_BUTTON);
  host()->dispatcher()->RepostEvent(event);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(Env::GetInstance()->IsMouseButtonDown());
}

// Check that we correctly track the state of the mouse buttons in response to
// button press and release events.
TEST_F(WindowEventDispatcherTest, MouseButtonState) {
  EXPECT_FALSE(Env::GetInstance()->IsMouseButtonDown());

  gfx::Point location;
  scoped_ptr<ui::MouseEvent> event;

  // Press the left button.
  event.reset(new ui::MouseEvent(
      ui::ET_MOUSE_PRESSED,
      location,
      location,
      ui::EF_LEFT_MOUSE_BUTTON,
      ui::EF_LEFT_MOUSE_BUTTON));
  DispatchEventUsingWindowDispatcher(event.get());
  EXPECT_TRUE(Env::GetInstance()->IsMouseButtonDown());

  // Additionally press the right.
  event.reset(new ui::MouseEvent(
      ui::ET_MOUSE_PRESSED,
      location,
      location,
      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON,
      ui::EF_RIGHT_MOUSE_BUTTON));
  DispatchEventUsingWindowDispatcher(event.get());
  EXPECT_TRUE(Env::GetInstance()->IsMouseButtonDown());

  // Release the left button.
  event.reset(new ui::MouseEvent(
      ui::ET_MOUSE_RELEASED,
      location,
      location,
      ui::EF_RIGHT_MOUSE_BUTTON,
      ui::EF_LEFT_MOUSE_BUTTON));
  DispatchEventUsingWindowDispatcher(event.get());
  EXPECT_TRUE(Env::GetInstance()->IsMouseButtonDown());

  // Release the right button.  We should ignore the Shift-is-down flag.
  event.reset(new ui::MouseEvent(
      ui::ET_MOUSE_RELEASED,
      location,
      location,
      ui::EF_SHIFT_DOWN,
      ui::EF_RIGHT_MOUSE_BUTTON));
  DispatchEventUsingWindowDispatcher(event.get());
  EXPECT_FALSE(Env::GetInstance()->IsMouseButtonDown());

  // Press the middle button.
  event.reset(new ui::MouseEvent(
      ui::ET_MOUSE_PRESSED,
      location,
      location,
      ui::EF_MIDDLE_MOUSE_BUTTON,
      ui::EF_MIDDLE_MOUSE_BUTTON));
  DispatchEventUsingWindowDispatcher(event.get());
  EXPECT_TRUE(Env::GetInstance()->IsMouseButtonDown());
}

TEST_F(WindowEventDispatcherTest, TranslatedEvent) {
  scoped_ptr<Window> w1(test::CreateTestWindowWithDelegate(NULL, 1,
      gfx::Rect(50, 50, 100, 100), root_window()));

  gfx::Point origin(100, 100);
  ui::MouseEvent root(ui::ET_MOUSE_PRESSED, origin, origin, 0, 0);

  EXPECT_EQ("100,100", root.location().ToString());
  EXPECT_EQ("100,100", root.root_location().ToString());

  ui::MouseEvent translated_event(
      root, static_cast<Window*>(root_window()), w1.get(),
      ui::ET_MOUSE_ENTERED, root.flags());
  EXPECT_EQ("50,50", translated_event.location().ToString());
  EXPECT_EQ("100,100", translated_event.root_location().ToString());
}

namespace {

class TestEventClient : public client::EventClient {
 public:
  static const int kNonLockWindowId = 100;
  static const int kLockWindowId = 200;

  explicit TestEventClient(Window* root_window)
      : root_window_(root_window),
        lock_(false) {
    client::SetEventClient(root_window_, this);
    Window* lock_window =
        test::CreateTestWindowWithBounds(root_window_->bounds(), root_window_);
    lock_window->set_id(kLockWindowId);
    Window* non_lock_window =
        test::CreateTestWindowWithBounds(root_window_->bounds(), root_window_);
    non_lock_window->set_id(kNonLockWindowId);
  }
  virtual ~TestEventClient() {
    client::SetEventClient(root_window_, NULL);
  }

  // Starts/stops locking. Locking prevents windows other than those inside
  // the lock container from receiving events, getting focus etc.
  void Lock() {
    lock_ = true;
  }
  void Unlock() {
    lock_ = false;
  }

  Window* GetLockWindow() {
    return const_cast<Window*>(
        static_cast<const TestEventClient*>(this)->GetLockWindow());
  }
  const Window* GetLockWindow() const {
    return root_window_->GetChildById(kLockWindowId);
  }
  Window* GetNonLockWindow() {
    return root_window_->GetChildById(kNonLockWindowId);
  }

 private:
  // Overridden from client::EventClient:
  virtual bool CanProcessEventsWithinSubtree(
      const Window* window) const OVERRIDE {
    return lock_ ?
        window->Contains(GetLockWindow()) || GetLockWindow()->Contains(window) :
        true;
  }

  virtual ui::EventTarget* GetToplevelEventTarget() OVERRIDE {
    return NULL;
  }

  Window* root_window_;
  bool lock_;

  DISALLOW_COPY_AND_ASSIGN(TestEventClient);
};

}  // namespace

TEST_F(WindowEventDispatcherTest, CanProcessEventsWithinSubtree) {
  TestEventClient client(root_window());
  test::TestWindowDelegate d;

  ui::test::TestEventHandler nonlock_ef;
  ui::test::TestEventHandler lock_ef;
  client.GetNonLockWindow()->AddPreTargetHandler(&nonlock_ef);
  client.GetLockWindow()->AddPreTargetHandler(&lock_ef);

  Window* w1 = test::CreateTestWindowWithBounds(gfx::Rect(10, 10, 20, 20),
                                                client.GetNonLockWindow());
  w1->set_id(1);
  Window* w2 = test::CreateTestWindowWithBounds(gfx::Rect(30, 30, 20, 20),
                                                client.GetNonLockWindow());
  w2->set_id(2);
  scoped_ptr<Window> w3(
      test::CreateTestWindowWithDelegate(&d, 3, gfx::Rect(30, 30, 20, 20),
                                         client.GetLockWindow()));

  w1->Focus();
  EXPECT_TRUE(IsFocusedWindow(w1));

  client.Lock();

  // Since we're locked, the attempt to focus w2 will be ignored.
  w2->Focus();
  EXPECT_TRUE(IsFocusedWindow(w1));
  EXPECT_FALSE(IsFocusedWindow(w2));

  {
    // Attempting to send a key event to w1 (not in the lock container) should
    // cause focus to be reset.
    test::EventGenerator generator(root_window());
    generator.PressKey(ui::VKEY_SPACE, 0);
    EXPECT_EQ(NULL, client::GetFocusClient(w1)->GetFocusedWindow());
    EXPECT_FALSE(IsFocusedWindow(w1));
  }

  {
    // Events sent to a window not in the lock container will not be processed.
    // i.e. never sent to the non-lock container's event filter.
    test::EventGenerator generator(root_window(), w1);
    generator.ClickLeftButton();
    EXPECT_EQ(0, nonlock_ef.num_mouse_events());

    // Events sent to a window in the lock container will be processed.
    test::EventGenerator generator3(root_window(), w3.get());
    generator3.PressLeftButton();
    EXPECT_EQ(1, lock_ef.num_mouse_events());
  }

  // Prevent w3 from being deleted by the hierarchy since its delegate is owned
  // by this scope.
  w3->parent()->RemoveChild(w3.get());
}

TEST_F(WindowEventDispatcherTest, IgnoreUnknownKeys) {
  ConsumeKeyHandler handler;
  root_window()->AddPreTargetHandler(&handler);

  ui::KeyEvent unknown_event(ui::ET_KEY_PRESSED, ui::VKEY_UNKNOWN, 0, false);
  DispatchEventUsingWindowDispatcher(&unknown_event);
  EXPECT_FALSE(unknown_event.handled());
  EXPECT_EQ(0, handler.num_key_events());

  handler.Reset();
  ui::KeyEvent known_event(ui::ET_KEY_PRESSED, ui::VKEY_A, 0, false);
  DispatchEventUsingWindowDispatcher(&known_event);
  EXPECT_TRUE(known_event.handled());
  EXPECT_EQ(1, handler.num_key_events());

  handler.Reset();
  ui::KeyEvent ime_event(ui::ET_KEY_PRESSED, ui::VKEY_UNKNOWN,
                         ui::EF_IME_FABRICATED_KEY, false);
  DispatchEventUsingWindowDispatcher(&ime_event);
  EXPECT_TRUE(ime_event.handled());
  EXPECT_EQ(1, handler.num_key_events());

  handler.Reset();
  ui::KeyEvent unknown_key_with_char_event(ui::ET_KEY_PRESSED, ui::VKEY_UNKNOWN,
                                           0, false);
  unknown_key_with_char_event.set_character(0x00e4 /* "ä" */);
  DispatchEventUsingWindowDispatcher(&unknown_key_with_char_event);
  EXPECT_TRUE(unknown_key_with_char_event.handled());
  EXPECT_EQ(1, handler.num_key_events());
}

TEST_F(WindowEventDispatcherTest, NoDelegateWindowReceivesKeyEvents) {
  scoped_ptr<Window> w1(CreateNormalWindow(1, root_window(), NULL));
  w1->Show();
  w1->Focus();

  ui::test::TestEventHandler handler;
  w1->AddPreTargetHandler(&handler);
  ui::KeyEvent key_press(ui::ET_KEY_PRESSED, ui::VKEY_A, 0, false);
  DispatchEventUsingWindowDispatcher(&key_press);
  EXPECT_TRUE(key_press.handled());
  EXPECT_EQ(1, handler.num_key_events());

  w1->RemovePreTargetHandler(&handler);
}

// Tests that touch-events that are beyond the bounds of the root-window do get
// propagated to the event filters correctly with the root as the target.
TEST_F(WindowEventDispatcherTest, TouchEventsOutsideBounds) {
  ui::test::TestEventHandler handler;
  root_window()->AddPreTargetHandler(&handler);

  gfx::Point position = root_window()->bounds().origin();
  position.Offset(-10, -10);
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, position, 0, ui::EventTimeForNow());
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_EQ(1, handler.num_touch_events());

  position = root_window()->bounds().origin();
  position.Offset(root_window()->bounds().width() + 10,
                  root_window()->bounds().height() + 10);
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, position, 0, ui::EventTimeForNow());
  DispatchEventUsingWindowDispatcher(&release);
  EXPECT_EQ(2, handler.num_touch_events());
}

// Tests that scroll events are dispatched correctly.
TEST_F(WindowEventDispatcherTest, ScrollEventDispatch) {
  base::TimeDelta now = ui::EventTimeForNow();
  ui::test::TestEventHandler handler;
  root_window()->AddPreTargetHandler(&handler);

  test::TestWindowDelegate delegate;
  scoped_ptr<Window> w1(CreateNormalWindow(1, root_window(), &delegate));
  w1->SetBounds(gfx::Rect(20, 20, 40, 40));

  // A scroll event on the root-window itself is dispatched.
  ui::ScrollEvent scroll1(ui::ET_SCROLL,
                          gfx::Point(10, 10),
                          now,
                          0,
                          0, -10,
                          0, -10,
                          2);
  DispatchEventUsingWindowDispatcher(&scroll1);
  EXPECT_EQ(1, handler.num_scroll_events());

  // Scroll event on a window should be dispatched properly.
  ui::ScrollEvent scroll2(ui::ET_SCROLL,
                          gfx::Point(25, 30),
                          now,
                          0,
                          -10, 0,
                          -10, 0,
                          2);
  DispatchEventUsingWindowDispatcher(&scroll2);
  EXPECT_EQ(2, handler.num_scroll_events());
  root_window()->RemovePreTargetHandler(&handler);
}

namespace {

// FilterFilter that tracks the types of events it's seen.
class EventFilterRecorder : public ui::EventHandler {
 public:
  typedef std::vector<ui::EventType> Events;
  typedef std::vector<gfx::Point> EventLocations;
  typedef std::vector<int> EventFlags;

  EventFilterRecorder()
      : wait_until_event_(ui::ET_UNKNOWN) {
  }

  const Events& events() const { return events_; }

  const EventLocations& mouse_locations() const { return mouse_locations_; }
  gfx::Point mouse_location(int i) const { return mouse_locations_[i]; }
  const EventLocations& touch_locations() const { return touch_locations_; }
  const EventFlags& mouse_event_flags() const { return mouse_event_flags_; }

  void WaitUntilReceivedEvent(ui::EventType type) {
    wait_until_event_ = type;
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  Events GetAndResetEvents() {
    Events events = events_;
    Reset();
    return events;
  }

  void Reset() {
    events_.clear();
    mouse_locations_.clear();
    touch_locations_.clear();
    mouse_event_flags_.clear();
  }

  // ui::EventHandler overrides:
  virtual void OnEvent(ui::Event* event) OVERRIDE {
    ui::EventHandler::OnEvent(event);
    events_.push_back(event->type());
    if (wait_until_event_ == event->type() && run_loop_) {
      run_loop_->Quit();
      wait_until_event_ = ui::ET_UNKNOWN;
    }
  }

  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    mouse_locations_.push_back(event->location());
    mouse_event_flags_.push_back(event->flags());
  }

  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE {
    touch_locations_.push_back(event->location());
  }

  bool HasReceivedEvent(ui::EventType type) {
    return std::find(events_.begin(), events_.end(), type) != events_.end();
  }

 private:
  scoped_ptr<base::RunLoop> run_loop_;
  ui::EventType wait_until_event_;

  Events events_;
  EventLocations mouse_locations_;
  EventLocations touch_locations_;
  EventFlags mouse_event_flags_;

  DISALLOW_COPY_AND_ASSIGN(EventFilterRecorder);
};

// Converts an EventType to a string.
std::string EventTypeToString(ui::EventType type) {
  switch (type) {
    case ui::ET_TOUCH_RELEASED:
      return "TOUCH_RELEASED";

    case ui::ET_TOUCH_CANCELLED:
      return "TOUCH_CANCELLED";

    case ui::ET_TOUCH_PRESSED:
      return "TOUCH_PRESSED";

    case ui::ET_TOUCH_MOVED:
      return "TOUCH_MOVED";

    case ui::ET_MOUSE_PRESSED:
      return "MOUSE_PRESSED";

    case ui::ET_MOUSE_DRAGGED:
      return "MOUSE_DRAGGED";

    case ui::ET_MOUSE_RELEASED:
      return "MOUSE_RELEASED";

    case ui::ET_MOUSE_MOVED:
      return "MOUSE_MOVED";

    case ui::ET_MOUSE_ENTERED:
      return "MOUSE_ENTERED";

    case ui::ET_MOUSE_EXITED:
      return "MOUSE_EXITED";

    case ui::ET_GESTURE_SCROLL_BEGIN:
      return "GESTURE_SCROLL_BEGIN";

    case ui::ET_GESTURE_SCROLL_END:
      return "GESTURE_SCROLL_END";

    case ui::ET_GESTURE_SCROLL_UPDATE:
      return "GESTURE_SCROLL_UPDATE";

    case ui::ET_GESTURE_PINCH_BEGIN:
      return "GESTURE_PINCH_BEGIN";

    case ui::ET_GESTURE_PINCH_END:
      return "GESTURE_PINCH_END";

    case ui::ET_GESTURE_PINCH_UPDATE:
      return "GESTURE_PINCH_UPDATE";

    case ui::ET_GESTURE_TAP:
      return "GESTURE_TAP";

    case ui::ET_GESTURE_TAP_DOWN:
      return "GESTURE_TAP_DOWN";

    case ui::ET_GESTURE_TAP_CANCEL:
      return "GESTURE_TAP_CANCEL";

    case ui::ET_GESTURE_SHOW_PRESS:
      return "GESTURE_SHOW_PRESS";

    case ui::ET_GESTURE_BEGIN:
      return "GESTURE_BEGIN";

    case ui::ET_GESTURE_END:
      return "GESTURE_END";

    default:
      // We should explicitly require each event type.
      NOTREACHED() << "Received unexpected event: " << type;
      break;
  }
  return "";
}

std::string EventTypesToString(const EventFilterRecorder::Events& events) {
  std::string result;
  for (size_t i = 0; i < events.size(); ++i) {
    if (i != 0)
      result += " ";
    result += EventTypeToString(events[i]);
  }
  return result;
}

}  // namespace

// Verifies a repost mouse event targets the window with capture (if there is
// one).
TEST_F(WindowEventDispatcherTest, RepostTargetsCaptureWindow) {
  // Set capture on |window| generate a mouse event (that is reposted) and not
  // over |window| and verify |window| gets it (|window| gets it because it has
  // capture).
  EXPECT_FALSE(Env::GetInstance()->IsMouseButtonDown());
  EventFilterRecorder recorder;
  scoped_ptr<Window> window(CreateNormalWindow(1, root_window(), NULL));
  window->SetBounds(gfx::Rect(20, 20, 40, 30));
  window->AddPreTargetHandler(&recorder);
  window->SetCapture();
  const ui::MouseEvent press_event(
      ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  host()->dispatcher()->RepostEvent(press_event);
  RunAllPendingInMessageLoop();  // Necessitated by RepostEvent().
  // Mouse moves/enters may be generated. We only care about a pressed.
  EXPECT_TRUE(EventTypesToString(recorder.events()).find("MOUSE_PRESSED") !=
              std::string::npos) << EventTypesToString(recorder.events());
}

TEST_F(WindowEventDispatcherTest, MouseMovesHeld) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);

  test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 100, 100), root_window()));

  ui::MouseEvent mouse_move_event(ui::ET_MOUSE_MOVED, gfx::Point(0, 0),
                                  gfx::Point(0, 0), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse_move_event);
  // Discard MOUSE_ENTER.
  recorder.Reset();

  host()->dispatcher()->HoldPointerMoves();

  // Check that we don't immediately dispatch the MOUSE_DRAGGED event.
  ui::MouseEvent mouse_dragged_event(ui::ET_MOUSE_DRAGGED, gfx::Point(0, 0),
                                     gfx::Point(0, 0), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse_dragged_event);
  EXPECT_TRUE(recorder.events().empty());

  // Check that we do dispatch the held MOUSE_DRAGGED event before another type
  // of event.
  ui::MouseEvent mouse_pressed_event(ui::ET_MOUSE_PRESSED, gfx::Point(0, 0),
                                     gfx::Point(0, 0), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse_pressed_event);
  EXPECT_EQ("MOUSE_DRAGGED MOUSE_PRESSED",
            EventTypesToString(recorder.events()));
  recorder.Reset();

  // Check that we coalesce held MOUSE_DRAGGED events.
  ui::MouseEvent mouse_dragged_event2(ui::ET_MOUSE_DRAGGED, gfx::Point(10, 10),
                                      gfx::Point(10, 10), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse_dragged_event);
  DispatchEventUsingWindowDispatcher(&mouse_dragged_event2);
  EXPECT_TRUE(recorder.events().empty());
  DispatchEventUsingWindowDispatcher(&mouse_pressed_event);
  EXPECT_EQ("MOUSE_DRAGGED MOUSE_PRESSED",
            EventTypesToString(recorder.events()));
  recorder.Reset();

  // Check that on ReleasePointerMoves, held events are not dispatched
  // immediately, but posted instead.
  DispatchEventUsingWindowDispatcher(&mouse_dragged_event);
  host()->dispatcher()->ReleasePointerMoves();
  EXPECT_TRUE(recorder.events().empty());
  RunAllPendingInMessageLoop();
  EXPECT_EQ("MOUSE_DRAGGED", EventTypesToString(recorder.events()));
  recorder.Reset();

  // However if another message comes in before the dispatch of the posted
  // event, check that the posted event is dispatched before this new event.
  host()->dispatcher()->HoldPointerMoves();
  DispatchEventUsingWindowDispatcher(&mouse_dragged_event);
  host()->dispatcher()->ReleasePointerMoves();
  DispatchEventUsingWindowDispatcher(&mouse_pressed_event);
  EXPECT_EQ("MOUSE_DRAGGED MOUSE_PRESSED",
            EventTypesToString(recorder.events()));
  recorder.Reset();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.events().empty());

  // Check that if the other message is another MOUSE_DRAGGED, we still coalesce
  // them.
  host()->dispatcher()->HoldPointerMoves();
  DispatchEventUsingWindowDispatcher(&mouse_dragged_event);
  host()->dispatcher()->ReleasePointerMoves();
  DispatchEventUsingWindowDispatcher(&mouse_dragged_event2);
  EXPECT_EQ("MOUSE_DRAGGED", EventTypesToString(recorder.events()));
  recorder.Reset();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.events().empty());

  // Check that synthetic mouse move event has a right location when issued
  // while holding pointer moves.
  ui::MouseEvent mouse_dragged_event3(ui::ET_MOUSE_DRAGGED, gfx::Point(28, 28),
                                      gfx::Point(28, 28), 0, 0);
  host()->dispatcher()->HoldPointerMoves();
  DispatchEventUsingWindowDispatcher(&mouse_dragged_event);
  DispatchEventUsingWindowDispatcher(&mouse_dragged_event2);
  window->SetBounds(gfx::Rect(15, 15, 80, 80));
  DispatchEventUsingWindowDispatcher(&mouse_dragged_event3);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.events().empty());
  host()->dispatcher()->ReleasePointerMoves();
  RunAllPendingInMessageLoop();
  EXPECT_EQ("MOUSE_MOVED", EventTypesToString(recorder.events()));
  EXPECT_EQ(gfx::Point(13, 13), recorder.mouse_location(0));
  recorder.Reset();
  root_window()->RemovePreTargetHandler(&recorder);
}

TEST_F(WindowEventDispatcherTest, TouchMovesHeld) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);

  test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(50, 50, 100, 100), root_window()));

  const gfx::Point touch_location(60, 60);
  // Starting the touch and throwing out the first few events, since the system
  // is going to generate synthetic mouse events that are not relevant to the
  // test.
  ui::TouchEvent touch_pressed_event(
      ui::ET_TOUCH_PRESSED, touch_location, 0, ui::EventTimeForNow());
  DispatchEventUsingWindowDispatcher(&touch_pressed_event);
  recorder.WaitUntilReceivedEvent(ui::ET_GESTURE_SHOW_PRESS);
  recorder.Reset();

  host()->dispatcher()->HoldPointerMoves();

  // Check that we don't immediately dispatch the TOUCH_MOVED event.
  ui::TouchEvent touch_moved_event(
      ui::ET_TOUCH_MOVED, touch_location, 0, ui::EventTimeForNow());
  ui::TouchEvent touch_moved_event2 = touch_moved_event;
  ui::TouchEvent touch_moved_event3 = touch_moved_event;

  DispatchEventUsingWindowDispatcher(&touch_moved_event);
  EXPECT_TRUE(recorder.events().empty());

  // Check that on ReleasePointerMoves, held events are not dispatched
  // immediately, but posted instead.
  DispatchEventUsingWindowDispatcher(&touch_moved_event2);
  host()->dispatcher()->ReleasePointerMoves();
  EXPECT_TRUE(recorder.events().empty());

  RunAllPendingInMessageLoop();
  EXPECT_EQ("TOUCH_MOVED", EventTypesToString(recorder.events()));
  recorder.Reset();

  // If another touch event occurs then the held touch should be dispatched
  // immediately before it.
  ui::TouchEvent touch_released_event(
      ui::ET_TOUCH_RELEASED, touch_location, 0, ui::EventTimeForNow());
  recorder.Reset();
  host()->dispatcher()->HoldPointerMoves();
  DispatchEventUsingWindowDispatcher(&touch_moved_event3);
  DispatchEventUsingWindowDispatcher(&touch_released_event);
  EXPECT_EQ("TOUCH_MOVED TOUCH_RELEASED GESTURE_TAP GESTURE_END",
            EventTypesToString(recorder.events()));
  recorder.Reset();
  host()->dispatcher()->ReleasePointerMoves();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.events().empty());
}

// Verifies that a direct call to ProcessedTouchEvent() with a
// TOUCH_PRESSED event does not cause a crash.
TEST_F(WindowEventDispatcherTest, CallToProcessedTouchEvent) {
  test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(50, 50, 100, 100), root_window()));

  ui::TouchEvent touch(
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), 1, ui::EventTimeForNow());
  host()->dispatcher()->ProcessedTouchEvent(
      &touch, window.get(), ui::ER_UNHANDLED);
}

// This event handler requests the dispatcher to start holding pointer-move
// events when it receives the first scroll-update gesture.
class HoldPointerOnScrollHandler : public ui::test::TestEventHandler {
 public:
  HoldPointerOnScrollHandler(WindowEventDispatcher* dispatcher,
                             EventFilterRecorder* filter)
      : dispatcher_(dispatcher),
        filter_(filter),
        holding_moves_(false) {}
  virtual ~HoldPointerOnScrollHandler() {}

 private:
  // ui::test::TestEventHandler:
  virtual void OnGestureEvent(ui::GestureEvent* gesture) OVERRIDE {
    if (!holding_moves_ && gesture->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
      holding_moves_ = true;
      dispatcher_->HoldPointerMoves();
      filter_->Reset();
    } else if (gesture->type() == ui::ET_GESTURE_SCROLL_END) {
      dispatcher_->ReleasePointerMoves();
      holding_moves_ = false;
    }
  }

  WindowEventDispatcher* dispatcher_;
  EventFilterRecorder* filter_;
  bool holding_moves_;

  DISALLOW_COPY_AND_ASSIGN(HoldPointerOnScrollHandler);
};

// Tests that touch-move events don't contribute to an in-progress scroll
// gesture if touch-move events are being held by the dispatcher.
TEST_F(WindowEventDispatcherTest, TouchMovesHeldOnScroll) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);
  test::TestWindowDelegate delegate;
  HoldPointerOnScrollHandler handler(host()->dispatcher(), &recorder);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(50, 50, 100, 100), root_window()));
  window->AddPreTargetHandler(&handler);

  test::EventGenerator generator(root_window());
  generator.GestureScrollSequence(
      gfx::Point(60, 60), gfx::Point(10, 60),
      base::TimeDelta::FromMilliseconds(100), 25);

  // |handler| will have reset |filter| and started holding the touch-move
  // events when scrolling started. At the end of the scroll (i.e. upon
  // touch-release), the held touch-move event will have been dispatched first,
  // along with the subsequent events (i.e. touch-release, scroll-end, and
  // gesture-end).
  const EventFilterRecorder::Events& events = recorder.events();
  EXPECT_EQ(
      "TOUCH_MOVED GESTURE_SCROLL_UPDATE TOUCH_RELEASED "
      "GESTURE_SCROLL_END GESTURE_END",
      EventTypesToString(events));
  ASSERT_EQ(2u, recorder.touch_locations().size());
  EXPECT_EQ(gfx::Point(-40, 10).ToString(),
            recorder.touch_locations()[0].ToString());
  EXPECT_EQ(gfx::Point(-40, 10).ToString(),
            recorder.touch_locations()[1].ToString());
}

// Tests that a 'held' touch-event does contribute to gesture event when it is
// dispatched.
TEST_F(WindowEventDispatcherTest, HeldTouchMoveContributesToGesture) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);

  const gfx::Point location(20, 20);
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, location, 0, ui::EventTimeForNow());
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::ET_TOUCH_PRESSED));
  recorder.Reset();

  host()->dispatcher()->HoldPointerMoves();

  ui::TouchEvent move(ui::ET_TOUCH_MOVED,
                      location + gfx::Vector2d(100, 100),
                      0,
                      ui::EventTimeForNow());
  DispatchEventUsingWindowDispatcher(&move);
  EXPECT_FALSE(recorder.HasReceivedEvent(ui::ET_TOUCH_MOVED));
  EXPECT_FALSE(recorder.HasReceivedEvent(ui::ET_GESTURE_SCROLL_BEGIN));
  recorder.Reset();

  host()->dispatcher()->ReleasePointerMoves();
  EXPECT_FALSE(recorder.HasReceivedEvent(ui::ET_TOUCH_MOVED));
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::ET_TOUCH_MOVED));
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::ET_GESTURE_SCROLL_BEGIN));
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::ET_GESTURE_SCROLL_UPDATE));

  root_window()->RemovePreTargetHandler(&recorder);
}

// Tests that synthetic mouse events are ignored when mouse
// events are disabled.
TEST_F(WindowEventDispatcherTest, DispatchSyntheticMouseEvents) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);

  test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1234, gfx::Rect(5, 5, 100, 100), root_window()));
  window->Show();
  window->SetCapture();

  test::TestCursorClient cursor_client(root_window());

  // Dispatch a non-synthetic mouse event when mouse events are enabled.
  ui::MouseEvent mouse1(ui::ET_MOUSE_MOVED, gfx::Point(10, 10),
                        gfx::Point(10, 10), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse1);
  EXPECT_FALSE(recorder.events().empty());
  recorder.Reset();

  // Dispatch a synthetic mouse event when mouse events are enabled.
  ui::MouseEvent mouse2(ui::ET_MOUSE_MOVED, gfx::Point(10, 10),
                        gfx::Point(10, 10), ui::EF_IS_SYNTHESIZED, 0);
  DispatchEventUsingWindowDispatcher(&mouse2);
  EXPECT_FALSE(recorder.events().empty());
  recorder.Reset();

  // Dispatch a synthetic mouse event when mouse events are disabled.
  cursor_client.DisableMouseEvents();
  DispatchEventUsingWindowDispatcher(&mouse2);
  EXPECT_TRUE(recorder.events().empty());
  root_window()->RemovePreTargetHandler(&recorder);
}

// Tests that a mouse-move event is not synthesized when a mouse-button is down.
TEST_F(WindowEventDispatcherTest, DoNotSynthesizeWhileButtonDown) {
  EventFilterRecorder recorder;
  test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1234, gfx::Rect(5, 5, 100, 100), root_window()));
  window->Show();

  window->AddPreTargetHandler(&recorder);
  // Dispatch a non-synthetic mouse event when mouse events are enabled.
  ui::MouseEvent mouse1(ui::ET_MOUSE_PRESSED, gfx::Point(10, 10),
                        gfx::Point(10, 10), ui::EF_LEFT_MOUSE_BUTTON,
                        ui::EF_LEFT_MOUSE_BUTTON);
  DispatchEventUsingWindowDispatcher(&mouse1);
  ASSERT_EQ(1u, recorder.events().size());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, recorder.events()[0]);
  window->RemovePreTargetHandler(&recorder);
  recorder.Reset();

  // Move |window| away from underneath the cursor.
  root_window()->AddPreTargetHandler(&recorder);
  window->SetBounds(gfx::Rect(30, 30, 100, 100));
  EXPECT_TRUE(recorder.events().empty());
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.events().empty());
  root_window()->RemovePreTargetHandler(&recorder);
}

// Tests synthetic mouse events generated when window bounds changes such that
// the cursor previously outside the window becomes inside, or vice versa.
// Do not synthesize events if the window ignores events or is invisible.
TEST_F(WindowEventDispatcherTest, SynthesizeMouseEventsOnWindowBoundsChanged) {
  test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1234, gfx::Rect(5, 5, 100, 100), root_window()));
  window->Show();
  window->SetCapture();

  EventFilterRecorder recorder;
  window->AddPreTargetHandler(&recorder);

  // Dispatch a non-synthetic mouse event to place cursor inside window bounds.
  ui::MouseEvent mouse(ui::ET_MOUSE_MOVED, gfx::Point(10, 10),
                       gfx::Point(10, 10), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse);
  EXPECT_FALSE(recorder.events().empty());
  recorder.Reset();

  // Update the window bounds so that cursor is now outside the window.
  // This should trigger a synthetic MOVED event.
  gfx::Rect bounds1(20, 20, 100, 100);
  window->SetBounds(bounds1);
  RunAllPendingInMessageLoop();
  ASSERT_FALSE(recorder.events().empty());
  ASSERT_FALSE(recorder.mouse_event_flags().empty());
  EXPECT_EQ(ui::ET_MOUSE_MOVED, recorder.events().back());
  EXPECT_EQ(ui::EF_IS_SYNTHESIZED, recorder.mouse_event_flags().back());
  recorder.Reset();

  // Set window to ignore events.
  window->set_ignore_events(true);

  // Update the window bounds so that cursor is back inside the window.
  // This should not trigger a synthetic event.
  gfx::Rect bounds2(5, 5, 100, 100);
  window->SetBounds(bounds2);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.events().empty());
  recorder.Reset();

  // Set window to accept events but invisible.
  window->set_ignore_events(false);
  window->Hide();
  recorder.Reset();

  // Update the window bounds so that cursor is outside the window.
  // This should not trigger a synthetic event.
  window->SetBounds(bounds1);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.events().empty());
}

// Tests that a mouse exit is dispatched to the last known cursor location
// when the cursor becomes invisible.
TEST_F(WindowEventDispatcherTest, DispatchMouseExitWhenCursorHidden) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);

  test::TestWindowDelegate delegate;
  gfx::Point window_origin(7, 18);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1234, gfx::Rect(window_origin, gfx::Size(100, 100)),
      root_window()));
  window->Show();

  // Dispatch a mouse move event into the window.
  gfx::Point mouse_location(gfx::Point(15, 25));
  ui::MouseEvent mouse1(ui::ET_MOUSE_MOVED, mouse_location,
                        mouse_location, 0, 0);
  EXPECT_TRUE(recorder.events().empty());
  DispatchEventUsingWindowDispatcher(&mouse1);
  EXPECT_FALSE(recorder.events().empty());
  recorder.Reset();

  // Hide the cursor and verify a mouse exit was dispatched.
  host()->OnCursorVisibilityChanged(false);
  EXPECT_FALSE(recorder.events().empty());
  EXPECT_EQ("MOUSE_EXITED", EventTypesToString(recorder.events()));

  // Verify the mouse exit was dispatched at the correct location
  // (in the correct coordinate space).
  int translated_x = mouse_location.x() - window_origin.x();
  int translated_y = mouse_location.y() - window_origin.y();
  gfx::Point translated_point(translated_x, translated_y);
  EXPECT_EQ(recorder.mouse_location(0).ToString(), translated_point.ToString());
  root_window()->RemovePreTargetHandler(&recorder);
}

// Tests that a synthetic mouse exit is dispatched to the last known cursor
// location after mouse events are disabled on the cursor client.
TEST_F(WindowEventDispatcherTest,
       DispatchSyntheticMouseExitAfterMouseEventsDisabled) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);

  test::TestWindowDelegate delegate;
  gfx::Point window_origin(7, 18);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1234, gfx::Rect(window_origin, gfx::Size(100, 100)),
      root_window()));
  window->Show();

  // Dispatch a mouse move event into the window.
  gfx::Point mouse_location(gfx::Point(15, 25));
  ui::MouseEvent mouse1(ui::ET_MOUSE_MOVED, mouse_location,
                        mouse_location, 0, 0);
  EXPECT_TRUE(recorder.events().empty());
  DispatchEventUsingWindowDispatcher(&mouse1);
  EXPECT_FALSE(recorder.events().empty());
  recorder.Reset();

  test::TestCursorClient cursor_client(root_window());
  cursor_client.DisableMouseEvents();

  gfx::Point mouse_exit_location(gfx::Point(150, 150));
  ui::MouseEvent mouse2(ui::ET_MOUSE_EXITED, gfx::Point(150, 150),
                        gfx::Point(150, 150), ui::EF_IS_SYNTHESIZED, 0);
  DispatchEventUsingWindowDispatcher(&mouse2);

  EXPECT_FALSE(recorder.events().empty());
  // We get the mouse exited event twice in our filter. Once during the
  // predispatch phase and during the actual dispatch.
  EXPECT_EQ("MOUSE_EXITED MOUSE_EXITED", EventTypesToString(recorder.events()));

  // Verify the mouse exit was dispatched at the correct location
  // (in the correct coordinate space).
  int translated_x = mouse_exit_location.x() - window_origin.x();
  int translated_y = mouse_exit_location.y() - window_origin.y();
  gfx::Point translated_point(translated_x, translated_y);
  EXPECT_EQ(recorder.mouse_location(0).ToString(), translated_point.ToString());
  root_window()->RemovePreTargetHandler(&recorder);
}

class DeletingEventFilter : public ui::EventHandler {
 public:
  DeletingEventFilter()
      : delete_during_pre_handle_(false) {}
  virtual ~DeletingEventFilter() {}

  void Reset(bool delete_during_pre_handle) {
    delete_during_pre_handle_ = delete_during_pre_handle;
  }

 private:
  // Overridden from ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE {
    if (delete_during_pre_handle_)
      delete event->target();
  }

  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    if (delete_during_pre_handle_)
      delete event->target();
  }

  bool delete_during_pre_handle_;

  DISALLOW_COPY_AND_ASSIGN(DeletingEventFilter);
};

class DeletingWindowDelegate : public test::TestWindowDelegate {
 public:
  DeletingWindowDelegate()
      : window_(NULL),
        delete_during_handle_(false),
        got_event_(false) {}
  virtual ~DeletingWindowDelegate() {}

  void Reset(Window* window, bool delete_during_handle) {
    window_ = window;
    delete_during_handle_ = delete_during_handle;
    got_event_ = false;
  }
  bool got_event() const { return got_event_; }

 private:
  // Overridden from WindowDelegate:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE {
    if (delete_during_handle_)
      delete window_;
    got_event_ = true;
  }

  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    if (delete_during_handle_)
      delete window_;
    got_event_ = true;
  }

  Window* window_;
  bool delete_during_handle_;
  bool got_event_;

  DISALLOW_COPY_AND_ASSIGN(DeletingWindowDelegate);
};

TEST_F(WindowEventDispatcherTest, DeleteWindowDuringDispatch) {
  // Verifies that we can delete a window during each phase of event handling.
  // Deleting the window should not cause a crash, only prevent further
  // processing from occurring.
  scoped_ptr<Window> w1(CreateNormalWindow(1, root_window(), NULL));
  DeletingWindowDelegate d11;
  Window* w11 = CreateNormalWindow(11, w1.get(), &d11);
  WindowTracker tracker;
  DeletingEventFilter w1_filter;
  w1->AddPreTargetHandler(&w1_filter);
  client::GetFocusClient(w1.get())->FocusWindow(w11);

  test::EventGenerator generator(root_window(), w11);

  // First up, no one deletes anything.
  tracker.Add(w11);
  d11.Reset(w11, false);

  generator.PressLeftButton();
  EXPECT_TRUE(tracker.Contains(w11));
  EXPECT_TRUE(d11.got_event());
  generator.ReleaseLeftButton();

  // Delegate deletes w11. This will prevent the post-handle step from applying.
  w1_filter.Reset(false);
  d11.Reset(w11, true);
  generator.PressKey(ui::VKEY_A, 0);
  EXPECT_FALSE(tracker.Contains(w11));
  EXPECT_TRUE(d11.got_event());

  // Pre-handle step deletes w11. This will prevent the delegate and the post-
  // handle steps from applying.
  w11 = CreateNormalWindow(11, w1.get(), &d11);
  w1_filter.Reset(true);
  d11.Reset(w11, false);
  generator.PressLeftButton();
  EXPECT_FALSE(tracker.Contains(w11));
  EXPECT_FALSE(d11.got_event());
}

namespace {

// A window delegate that detaches the parent of the target's parent window when
// it receives a tap event.
class DetachesParentOnTapDelegate : public test::TestWindowDelegate {
 public:
  DetachesParentOnTapDelegate() {}
  virtual ~DetachesParentOnTapDelegate() {}

 private:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
      event->SetHandled();
      return;
    }

    if (event->type() == ui::ET_GESTURE_TAP) {
      Window* parent = static_cast<Window*>(event->target())->parent();
      parent->parent()->RemoveChild(parent);
      event->SetHandled();
    }
  }

  DISALLOW_COPY_AND_ASSIGN(DetachesParentOnTapDelegate);
};

}  // namespace

// Tests that the gesture recognizer is reset for all child windows when a
// window hides. No expectations, just checks that the test does not crash.
TEST_F(WindowEventDispatcherTest,
       GestureRecognizerResetsTargetWhenParentHides) {
  scoped_ptr<Window> w1(CreateNormalWindow(1, root_window(), NULL));
  DetachesParentOnTapDelegate delegate;
  scoped_ptr<Window> parent(CreateNormalWindow(22, w1.get(), NULL));
  Window* child = CreateNormalWindow(11, parent.get(), &delegate);
  test::EventGenerator generator(root_window(), child);
  generator.GestureTapAt(gfx::Point(40, 40));
}

namespace {

// A window delegate that processes nested gestures on tap.
class NestedGestureDelegate : public test::TestWindowDelegate {
 public:
  NestedGestureDelegate(test::EventGenerator* generator,
                        const gfx::Point tap_location)
      : generator_(generator),
        tap_location_(tap_location),
        gesture_end_count_(0) {}
  virtual ~NestedGestureDelegate() {}

  int gesture_end_count() const { return gesture_end_count_; }

 private:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    switch (event->type()) {
      case ui::ET_GESTURE_TAP_DOWN:
        event->SetHandled();
        break;
      case ui::ET_GESTURE_TAP:
        if (generator_)
          generator_->GestureTapAt(tap_location_);
        event->SetHandled();
        break;
      case ui::ET_GESTURE_END:
        ++gesture_end_count_;
        break;
      default:
        break;
    }
  }

  test::EventGenerator* generator_;
  const gfx::Point tap_location_;
  int gesture_end_count_;
  DISALLOW_COPY_AND_ASSIGN(NestedGestureDelegate);
};

}  // namespace

// Tests that gesture end is delivered after nested gesture processing.
TEST_F(WindowEventDispatcherTest, GestureEndDeliveredAfterNestedGestures) {
  NestedGestureDelegate d1(NULL, gfx::Point());
  scoped_ptr<Window> w1(CreateNormalWindow(1, root_window(), &d1));
  w1->SetBounds(gfx::Rect(0, 0, 100, 100));

  test::EventGenerator nested_generator(root_window(), w1.get());
  NestedGestureDelegate d2(&nested_generator, w1->bounds().CenterPoint());
  scoped_ptr<Window> w2(CreateNormalWindow(1, root_window(), &d2));
  w2->SetBounds(gfx::Rect(100, 0, 100, 100));

  // Tap on w2 which triggers nested gestures for w1.
  test::EventGenerator generator(root_window(), w2.get());
  generator.GestureTapAt(w2->bounds().CenterPoint());

  // Both windows should get their gesture end events.
  EXPECT_EQ(1, d1.gesture_end_count());
  EXPECT_EQ(1, d2.gesture_end_count());
}

// Tests whether we can repost the Tap down gesture event.
TEST_F(WindowEventDispatcherTest, RepostTapdownGestureTest) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);

  test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 100, 100), root_window()));

  ui::GestureEventDetails details(ui::ET_GESTURE_TAP_DOWN, 0.0f, 0.0f);
  gfx::Point point(10, 10);
  ui::GestureEvent event(ui::ET_GESTURE_TAP_DOWN,
                         point.x(),
                         point.y(),
                         0,
                         ui::EventTimeForNow(),
                         details,
                         0);
  host()->dispatcher()->RepostEvent(event);
  RunAllPendingInMessageLoop();
  // TODO(rbyers): Currently disabled - crbug.com/170987
  EXPECT_FALSE(EventTypesToString(recorder.events()).find("GESTURE_TAP_DOWN") !=
              std::string::npos);
  recorder.Reset();
  root_window()->RemovePreTargetHandler(&recorder);
}

// This class inherits from the EventFilterRecorder class which provides a
// facility to record events. This class additionally provides a facility to
// repost the ET_GESTURE_TAP_DOWN gesture to the target window and records
// events after that.
class RepostGestureEventRecorder : public EventFilterRecorder {
 public:
  RepostGestureEventRecorder(aura::Window* repost_source,
                             aura::Window* repost_target)
      : repost_source_(repost_source),
        repost_target_(repost_target),
        reposted_(false),
        done_cleanup_(false) {}

  virtual ~RepostGestureEventRecorder() {}

  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE {
    if (reposted_ && event->type() == ui::ET_TOUCH_PRESSED) {
      done_cleanup_ = true;
      Reset();
    }
    EventFilterRecorder::OnTouchEvent(event);
  }

  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    EXPECT_EQ(done_cleanup_ ? repost_target_ : repost_source_, event->target());
    if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
      if (!reposted_) {
        EXPECT_NE(repost_target_, event->target());
        reposted_ = true;
        repost_target_->GetHost()->dispatcher()->RepostEvent(*event);
        // Ensure that the reposted gesture event above goes to the
        // repost_target_;
        repost_source_->GetRootWindow()->RemoveChild(repost_source_);
        return;
      }
    }
    EventFilterRecorder::OnGestureEvent(event);
  }

  // Ignore mouse events as they don't fire at all times. This causes
  // the GestureRepostEventOrder test to fail randomly.
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {}

 private:
  aura::Window* repost_source_;
  aura::Window* repost_target_;
  // set to true if we reposted the ET_GESTURE_TAP_DOWN event.
  bool reposted_;
  // set true if we're done cleaning up after hiding repost_source_;
  bool done_cleanup_;
  DISALLOW_COPY_AND_ASSIGN(RepostGestureEventRecorder);
};

// Tests whether events which are generated after the reposted gesture event
// are received after that. In this case the scroll sequence events should
// be received after the reposted gesture event.
TEST_F(WindowEventDispatcherTest, GestureRepostEventOrder) {
  // Expected events at the end for the repost_target window defined below.
  const char kExpectedTargetEvents[] =
    // TODO)(rbyers): Gesture event reposting is disabled - crbug.com/279039.
    // "GESTURE_BEGIN GESTURE_TAP_DOWN "
    "TOUCH_PRESSED GESTURE_BEGIN GESTURE_TAP_DOWN TOUCH_MOVED "
    "GESTURE_TAP_CANCEL GESTURE_SCROLL_BEGIN GESTURE_SCROLL_UPDATE TOUCH_MOVED "
    "GESTURE_SCROLL_UPDATE TOUCH_MOVED GESTURE_SCROLL_UPDATE TOUCH_RELEASED "
    "GESTURE_SCROLL_END GESTURE_END";
  // We create two windows.
  // The first window (repost_source) is the one to which the initial tap
  // gesture is sent. It reposts this event to the second window
  // (repost_target).
  // We then generate the scroll sequence for repost_target and look for two
  // ET_GESTURE_TAP_DOWN events in the event list at the end.
  test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> repost_target(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 100, 100), root_window()));

  scoped_ptr<aura::Window> repost_source(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 50, 50), root_window()));

  RepostGestureEventRecorder repost_event_recorder(repost_source.get(),
                                                   repost_target.get());
  root_window()->AddPreTargetHandler(&repost_event_recorder);

  // Generate a tap down gesture for the repost_source. This will be reposted
  // to repost_target.
  test::EventGenerator repost_generator(root_window(), repost_source.get());
  repost_generator.GestureTapAt(gfx::Point(40, 40));
  RunAllPendingInMessageLoop();

  test::EventGenerator scroll_generator(root_window(), repost_target.get());
  scroll_generator.GestureScrollSequence(
      gfx::Point(80, 80),
      gfx::Point(100, 100),
      base::TimeDelta::FromMilliseconds(100),
      3);
  RunAllPendingInMessageLoop();

  int tap_down_count = 0;
  for (size_t i = 0; i < repost_event_recorder.events().size(); ++i) {
    if (repost_event_recorder.events()[i] == ui::ET_GESTURE_TAP_DOWN)
      ++tap_down_count;
  }

  // We expect two tap down events. One from the repost and the other one from
  // the scroll sequence posted above.
  // TODO(rbyers): Currently disabled - crbug.com/170987
  EXPECT_EQ(1, tap_down_count);

  EXPECT_EQ(kExpectedTargetEvents,
            EventTypesToString(repost_event_recorder.events()));
  root_window()->RemovePreTargetHandler(&repost_event_recorder);
}

class OnMouseExitDeletingEventFilter : public EventFilterRecorder {
 public:
  OnMouseExitDeletingEventFilter() : window_to_delete_(NULL) {}
  virtual ~OnMouseExitDeletingEventFilter() {}

  void set_window_to_delete(Window* window_to_delete) {
    window_to_delete_ = window_to_delete;
  }

 private:
  // Overridden from ui::EventHandler:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    EventFilterRecorder::OnMouseEvent(event);
    if (window_to_delete_) {
      delete window_to_delete_;
      window_to_delete_ = NULL;
    }
  }

  Window* window_to_delete_;

  DISALLOW_COPY_AND_ASSIGN(OnMouseExitDeletingEventFilter);
};

// Tests that RootWindow drops mouse-moved event that is supposed to be sent to
// a child, but the child is destroyed because of the synthesized mouse-exit
// event generated on the previous mouse_moved_handler_.
TEST_F(WindowEventDispatcherTest, DeleteWindowDuringMouseMovedDispatch) {
  // Create window 1 and set its event filter. Window 1 will take ownership of
  // the event filter.
  scoped_ptr<Window> w1(CreateNormalWindow(1, root_window(), NULL));
  OnMouseExitDeletingEventFilter w1_filter;
  w1->AddPreTargetHandler(&w1_filter);
  w1->SetBounds(gfx::Rect(20, 20, 60, 60));
  EXPECT_EQ(NULL, host()->dispatcher()->mouse_moved_handler());

  test::EventGenerator generator(root_window(), w1.get());

  // Move mouse over window 1 to set it as the |mouse_moved_handler_| for the
  // root window.
  generator.MoveMouseTo(51, 51);
  EXPECT_EQ(w1.get(), host()->dispatcher()->mouse_moved_handler());

  // Create window 2 under the mouse cursor and stack it above window 1.
  Window* w2 = CreateNormalWindow(2, root_window(), NULL);
  w2->SetBounds(gfx::Rect(30, 30, 40, 40));
  root_window()->StackChildAbove(w2, w1.get());

  // Set window 2 as the window that is to be deleted when a mouse-exited event
  // happens on window 1.
  w1_filter.set_window_to_delete(w2);

  // Move mosue over window 2. This should generate a mouse-exited event for
  // window 1 resulting in deletion of window 2. The original mouse-moved event
  // that was targeted to window 2 should be dropped since window 2 is
  // destroyed. This test passes if no crash happens.
  generator.MoveMouseTo(52, 52);
  EXPECT_EQ(NULL, host()->dispatcher()->mouse_moved_handler());

  // Check events received by window 1.
  EXPECT_EQ("MOUSE_ENTERED MOUSE_MOVED MOUSE_EXITED",
            EventTypesToString(w1_filter.events()));
}

namespace {

// Used to track if OnWindowDestroying() is invoked and if there is a valid
// RootWindow at such time.
class ValidRootDuringDestructionWindowObserver : public aura::WindowObserver {
 public:
  ValidRootDuringDestructionWindowObserver(bool* got_destroying,
                                           bool* has_valid_root)
      : got_destroying_(got_destroying),
        has_valid_root_(has_valid_root) {
  }

  // WindowObserver:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    *got_destroying_ = true;
    *has_valid_root_ = (window->GetRootWindow() != NULL);
  }

 private:
  bool* got_destroying_;
  bool* has_valid_root_;

  DISALLOW_COPY_AND_ASSIGN(ValidRootDuringDestructionWindowObserver);
};

}  // namespace

// Verifies GetRootWindow() from ~Window returns a valid root.
TEST_F(WindowEventDispatcherTest, ValidRootDuringDestruction) {
  bool got_destroying = false;
  bool has_valid_root = false;
  ValidRootDuringDestructionWindowObserver observer(&got_destroying,
                                                    &has_valid_root);
  {
    scoped_ptr<WindowTreeHost> host(
        WindowTreeHost::Create(gfx::Rect(0, 0, 100, 100)));
    host->InitHost();
    // Owned by WindowEventDispatcher.
    Window* w1 = CreateNormalWindow(1, host->window(), NULL);
    w1->AddObserver(&observer);
  }
  EXPECT_TRUE(got_destroying);
  EXPECT_TRUE(has_valid_root);
}

namespace {

// See description above DontResetHeldEvent for details.
class DontResetHeldEventWindowDelegate : public test::TestWindowDelegate {
 public:
  explicit DontResetHeldEventWindowDelegate(aura::Window* root)
      : root_(root),
        mouse_event_count_(0) {}
  virtual ~DontResetHeldEventWindowDelegate() {}

  int mouse_event_count() const { return mouse_event_count_; }

  // TestWindowDelegate:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    if ((event->flags() & ui::EF_SHIFT_DOWN) != 0 &&
        mouse_event_count_++ == 0) {
      ui::MouseEvent mouse_event(ui::ET_MOUSE_PRESSED,
                                 gfx::Point(10, 10), gfx::Point(10, 10),
                                 ui::EF_SHIFT_DOWN, 0);
      root_->GetHost()->dispatcher()->RepostEvent(mouse_event);
    }
  }

 private:
  Window* root_;
  int mouse_event_count_;

  DISALLOW_COPY_AND_ASSIGN(DontResetHeldEventWindowDelegate);
};

}  // namespace

// Verifies RootWindow doesn't reset |RootWindow::held_repostable_event_| after
// dispatching. This is done by using DontResetHeldEventWindowDelegate, which
// tracks the number of events with ui::EF_SHIFT_DOWN set (all reposted events
// have EF_SHIFT_DOWN). When the first event is seen RepostEvent() is used to
// schedule another reposted event.
TEST_F(WindowEventDispatcherTest, DontResetHeldEvent) {
  DontResetHeldEventWindowDelegate delegate(root_window());
  scoped_ptr<Window> w1(CreateNormalWindow(1, root_window(), &delegate));
  w1->SetBounds(gfx::Rect(0, 0, 40, 40));
  ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED,
                         gfx::Point(10, 10), gfx::Point(10, 10),
                         ui::EF_SHIFT_DOWN, 0);
  root_window()->GetHost()->dispatcher()->RepostEvent(pressed);
  ui::MouseEvent pressed2(ui::ET_MOUSE_PRESSED,
                          gfx::Point(10, 10), gfx::Point(10, 10), 0, 0);
  // Dispatch an event to flush event scheduled by way of RepostEvent().
  DispatchEventUsingWindowDispatcher(&pressed2);
  // Delegate should have seen reposted event (identified by way of
  // EF_SHIFT_DOWN). Dispatch another event to flush the second
  // RepostedEvent().
  EXPECT_EQ(1, delegate.mouse_event_count());
  DispatchEventUsingWindowDispatcher(&pressed2);
  EXPECT_EQ(2, delegate.mouse_event_count());
}

namespace {

// See description above DeleteHostFromHeldMouseEvent for details.
class DeleteHostFromHeldMouseEventDelegate
    : public test::TestWindowDelegate {
 public:
  explicit DeleteHostFromHeldMouseEventDelegate(WindowTreeHost* host)
      : host_(host),
        got_mouse_event_(false),
        got_destroy_(false) {
  }
  virtual ~DeleteHostFromHeldMouseEventDelegate() {}

  bool got_mouse_event() const { return got_mouse_event_; }
  bool got_destroy() const { return got_destroy_; }

  // TestWindowDelegate:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    if ((event->flags() & ui::EF_SHIFT_DOWN) != 0) {
      got_mouse_event_ = true;
      delete host_;
    }
  }
  virtual void OnWindowDestroyed(Window* window) OVERRIDE {
    got_destroy_ = true;
  }

 private:
  WindowTreeHost* host_;
  bool got_mouse_event_;
  bool got_destroy_;

  DISALLOW_COPY_AND_ASSIGN(DeleteHostFromHeldMouseEventDelegate);
};

}  // namespace

// Verifies if a WindowTreeHost is deleted from dispatching a held mouse event
// we don't crash.
TEST_F(WindowEventDispatcherTest, DeleteHostFromHeldMouseEvent) {
  // Should be deleted by |delegate|.
  WindowTreeHost* h2 = WindowTreeHost::Create(gfx::Rect(0, 0, 100, 100));
  h2->InitHost();
  DeleteHostFromHeldMouseEventDelegate delegate(h2);
  // Owned by |h2|.
  Window* w1 = CreateNormalWindow(1, h2->window(), &delegate);
  w1->SetBounds(gfx::Rect(0, 0, 40, 40));
  ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED,
                         gfx::Point(10, 10), gfx::Point(10, 10),
                         ui::EF_SHIFT_DOWN, 0);
  h2->dispatcher()->RepostEvent(pressed);
  // RunAllPendingInMessageLoop() to make sure the |pressed| is run.
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(delegate.got_mouse_event());
  EXPECT_TRUE(delegate.got_destroy());
}

TEST_F(WindowEventDispatcherTest, WindowHideCancelsActiveTouches) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);

  test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 100, 100), root_window()));

  gfx::Point position1 = root_window()->bounds().origin();
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, position1, 0, ui::EventTimeForNow());
  DispatchEventUsingWindowDispatcher(&press);

  EXPECT_EQ("TOUCH_PRESSED GESTURE_BEGIN GESTURE_TAP_DOWN",
            EventTypesToString(recorder.GetAndResetEvents()));

  window->Hide();

  EXPECT_EQ(ui::ET_TOUCH_CANCELLED, recorder.events()[0]);
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::ET_GESTURE_TAP_CANCEL));
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::ET_GESTURE_END));
  EXPECT_EQ(3U, recorder.events().size());
  root_window()->RemovePreTargetHandler(&recorder);
}

TEST_F(WindowEventDispatcherTest, WindowHideCancelsActiveGestures) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);

  test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 100, 100), root_window()));

  gfx::Point position1 = root_window()->bounds().origin();
  gfx::Point position2 = root_window()->bounds().CenterPoint();
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, position1, 0, ui::EventTimeForNow());
  DispatchEventUsingWindowDispatcher(&press);

  ui::TouchEvent move(
      ui::ET_TOUCH_MOVED, position2, 0, ui::EventTimeForNow());
  DispatchEventUsingWindowDispatcher(&move);

  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, position1, 1, ui::EventTimeForNow());
  DispatchEventUsingWindowDispatcher(&press2);

  // TODO(tdresser): once the unified Gesture Recognizer has stuck, remove the
  // special casing here. See crbug.com/332418 for details.
  std::string expected =
      "TOUCH_PRESSED GESTURE_BEGIN GESTURE_TAP_DOWN TOUCH_MOVED "
      "GESTURE_TAP_CANCEL GESTURE_SCROLL_BEGIN GESTURE_SCROLL_UPDATE "
      "TOUCH_PRESSED GESTURE_BEGIN GESTURE_PINCH_BEGIN";

  std::string expected_ugr =
      "TOUCH_PRESSED GESTURE_BEGIN GESTURE_TAP_DOWN TOUCH_MOVED "
      "GESTURE_TAP_CANCEL GESTURE_SCROLL_BEGIN GESTURE_SCROLL_UPDATE "
      "TOUCH_PRESSED GESTURE_BEGIN";

  std::string events_string = EventTypesToString(recorder.GetAndResetEvents());
  EXPECT_TRUE((expected == events_string) || (expected_ugr == events_string));

  window->Hide();

  expected =
      "TOUCH_CANCELLED GESTURE_PINCH_END GESTURE_END TOUCH_CANCELLED "
      "GESTURE_SCROLL_END GESTURE_END";
  expected_ugr =
      "TOUCH_CANCELLED GESTURE_SCROLL_END GESTURE_END GESTURE_END "
      "TOUCH_CANCELLED";

  events_string = EventTypesToString(recorder.GetAndResetEvents());
  EXPECT_TRUE((expected == events_string) || (expected_ugr == events_string));

  root_window()->RemovePreTargetHandler(&recorder);
}

// Places two windows side by side. Presses down on one window, and starts a
// scroll. Sets capture on the other window and ensures that the "ending" events
// aren't sent to the window which gained capture.
TEST_F(WindowEventDispatcherTest, EndingEventDoesntRetarget) {
  EventFilterRecorder recorder1;
  EventFilterRecorder recorder2;
  scoped_ptr<Window> window1(CreateNormalWindow(1, root_window(), NULL));
  window1->SetBounds(gfx::Rect(0, 0, 40, 40));

  scoped_ptr<Window> window2(CreateNormalWindow(2, root_window(), NULL));
  window2->SetBounds(gfx::Rect(40, 0, 40, 40));

  window1->AddPreTargetHandler(&recorder1);
  window2->AddPreTargetHandler(&recorder2);

  gfx::Point position = window1->bounds().origin();
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, position, 0, ui::EventTimeForNow());
  DispatchEventUsingWindowDispatcher(&press);

  gfx::Point position2 = window1->bounds().CenterPoint();
  ui::TouchEvent move(
      ui::ET_TOUCH_MOVED, position2, 0, ui::EventTimeForNow());
  DispatchEventUsingWindowDispatcher(&move);

  window2->SetCapture();

  EXPECT_EQ("TOUCH_PRESSED GESTURE_BEGIN GESTURE_TAP_DOWN TOUCH_MOVED "
            "GESTURE_TAP_CANCEL GESTURE_SCROLL_BEGIN GESTURE_SCROLL_UPDATE "
            "TOUCH_CANCELLED GESTURE_SCROLL_END GESTURE_END",
            EventTypesToString(recorder1.events()));

  EXPECT_TRUE(recorder2.events().empty());
}

namespace {

// This class creates and manages a window which is destroyed as soon as
// capture is lost. This is the case for the drag and drop capture window.
class CaptureWindowTracker : public test::TestWindowDelegate {
 public:
  CaptureWindowTracker() {}
  virtual ~CaptureWindowTracker() {}

  void CreateCaptureWindow(aura::Window* root_window) {
    capture_window_.reset(test::CreateTestWindowWithDelegate(
        this, -1234, gfx::Rect(20, 20, 20, 20), root_window));
    capture_window_->SetCapture();
  }

  void reset() {
    capture_window_.reset();
  }

  virtual void OnCaptureLost() OVERRIDE {
    capture_window_.reset();
  }

  virtual void OnWindowDestroyed(Window* window) OVERRIDE {
    TestWindowDelegate::OnWindowDestroyed(window);
    capture_window_.reset();
  }

  aura::Window* capture_window() { return capture_window_.get(); }

 private:
  scoped_ptr<aura::Window> capture_window_;

  DISALLOW_COPY_AND_ASSIGN(CaptureWindowTracker);
};

}

// Verifies handling loss of capture by the capture window being hidden.
TEST_F(WindowEventDispatcherTest, CaptureWindowHidden) {
  CaptureWindowTracker capture_window_tracker;
  capture_window_tracker.CreateCaptureWindow(root_window());
  capture_window_tracker.capture_window()->Hide();
  EXPECT_EQ(NULL, capture_window_tracker.capture_window());
}

// Verifies handling loss of capture by the capture window being destroyed.
TEST_F(WindowEventDispatcherTest, CaptureWindowDestroyed) {
  CaptureWindowTracker capture_window_tracker;
  capture_window_tracker.CreateCaptureWindow(root_window());
  capture_window_tracker.reset();
  EXPECT_EQ(NULL, capture_window_tracker.capture_window());
}

class ExitMessageLoopOnMousePress : public ui::test::TestEventHandler {
 public:
  ExitMessageLoopOnMousePress() {}
  virtual ~ExitMessageLoopOnMousePress() {}

 protected:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    ui::test::TestEventHandler::OnMouseEvent(event);
    if (event->type() == ui::ET_MOUSE_PRESSED)
      base::MessageLoopForUI::current()->Quit();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExitMessageLoopOnMousePress);
};

class WindowEventDispatcherTestWithMessageLoop
    : public WindowEventDispatcherTest {
 public:
  WindowEventDispatcherTestWithMessageLoop() {}
  virtual ~WindowEventDispatcherTestWithMessageLoop() {}

  void RunTest() {
    // Reset any event the window may have received when bringing up the window
    // (e.g. mouse-move events if the mouse cursor is over the window).
    handler_.Reset();

    // Start a nested message-loop, post an event to be dispatched, and then
    // terminate the message-loop. When the message-loop unwinds and gets back,
    // the reposted event should not have fired.
    scoped_ptr<ui::MouseEvent> mouse(new ui::MouseEvent(ui::ET_MOUSE_PRESSED,
                                                        gfx::Point(10, 10),
                                                        gfx::Point(10, 10),
                                                        ui::EF_NONE,
                                                        ui::EF_NONE));
    message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&WindowEventDispatcherTestWithMessageLoop::RepostEventHelper,
                   host()->dispatcher(),
                   base::Passed(&mouse)));
    message_loop()->PostTask(FROM_HERE, message_loop()->QuitClosure());

    base::MessageLoop::ScopedNestableTaskAllower allow(message_loop());
    base::RunLoop loop;
    loop.Run();
    EXPECT_EQ(0, handler_.num_mouse_events());

    // Let the current message-loop run. The event-handler will terminate the
    // message-loop when it receives the reposted event.
  }

  base::MessageLoop* message_loop() {
    return base::MessageLoopForUI::current();
  }

 protected:
  virtual void SetUp() OVERRIDE {
    WindowEventDispatcherTest::SetUp();
    window_.reset(CreateNormalWindow(1, root_window(), NULL));
    window_->AddPreTargetHandler(&handler_);
  }

  virtual void TearDown() OVERRIDE {
    window_.reset();
    WindowEventDispatcherTest::TearDown();
  }

 private:
  // Used to avoid a copying |event| when binding to a closure.
  static void RepostEventHelper(WindowEventDispatcher* dispatcher,
                                scoped_ptr<ui::MouseEvent> event) {
    dispatcher->RepostEvent(*event);
  }

  scoped_ptr<Window> window_;
  ExitMessageLoopOnMousePress handler_;

  DISALLOW_COPY_AND_ASSIGN(WindowEventDispatcherTestWithMessageLoop);
};

TEST_F(WindowEventDispatcherTestWithMessageLoop, EventRepostedInNonNestedLoop) {
  CHECK(!message_loop()->is_running());
  // Perform the test in a callback, so that it runs after the message-loop
  // starts.
  message_loop()->PostTask(
      FROM_HERE, base::Bind(
          &WindowEventDispatcherTestWithMessageLoop::RunTest,
          base::Unretained(this)));
  message_loop()->Run();
}

class WindowEventDispatcherTestInHighDPI : public WindowEventDispatcherTest {
 public:
  WindowEventDispatcherTestInHighDPI() {}
  virtual ~WindowEventDispatcherTestInHighDPI() {}

 protected:
  virtual void SetUp() OVERRIDE {
    WindowEventDispatcherTest::SetUp();
    test_screen()->SetDeviceScaleFactor(2.f);
  }
};

TEST_F(WindowEventDispatcherTestInHighDPI, EventLocationTransform) {
  test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> child(test::CreateTestWindowWithDelegate(&delegate,
      1234, gfx::Rect(20, 20, 100, 100), root_window()));
  child->Show();

  ui::test::TestEventHandler handler_child;
  ui::test::TestEventHandler handler_root;
  root_window()->AddPreTargetHandler(&handler_root);
  child->AddPreTargetHandler(&handler_child);

  {
    ui::MouseEvent move(ui::ET_MOUSE_MOVED,
                        gfx::Point(30, 30), gfx::Point(30, 30),
                        ui::EF_NONE, ui::EF_NONE);
    DispatchEventUsingWindowDispatcher(&move);
    EXPECT_EQ(0, handler_child.num_mouse_events());
    EXPECT_EQ(1, handler_root.num_mouse_events());
  }

  {
    ui::MouseEvent move(ui::ET_MOUSE_MOVED,
                        gfx::Point(50, 50), gfx::Point(50, 50),
                        ui::EF_NONE, ui::EF_NONE);
    DispatchEventUsingWindowDispatcher(&move);
    // The child receives an ENTER, and a MOVED event.
    EXPECT_EQ(2, handler_child.num_mouse_events());
    // The root receives both the ENTER and the MOVED events dispatched to
    // |child|, as well as an EXIT event.
    EXPECT_EQ(3, handler_root.num_mouse_events());
  }

  child->RemovePreTargetHandler(&handler_child);
  root_window()->RemovePreTargetHandler(&handler_root);
}

TEST_F(WindowEventDispatcherTestInHighDPI, TouchMovesHeldOnScroll) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);
  test::TestWindowDelegate delegate;
  HoldPointerOnScrollHandler handler(host()->dispatcher(), &recorder);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(50, 50, 100, 100), root_window()));
  window->AddPreTargetHandler(&handler);

  test::EventGenerator generator(root_window());
  generator.GestureScrollSequence(
      gfx::Point(120, 120), gfx::Point(20, 120),
      base::TimeDelta::FromMilliseconds(100), 25);

  // |handler| will have reset |filter| and started holding the touch-move
  // events when scrolling started. At the end of the scroll (i.e. upon
  // touch-release), the held touch-move event will have been dispatched first,
  // along with the subsequent events (i.e. touch-release, scroll-end, and
  // gesture-end).
  const EventFilterRecorder::Events& events = recorder.events();
  EXPECT_EQ(
      "TOUCH_MOVED GESTURE_SCROLL_UPDATE TOUCH_RELEASED "
      "GESTURE_SCROLL_END GESTURE_END",
      EventTypesToString(events));
  ASSERT_EQ(2u, recorder.touch_locations().size());
  EXPECT_EQ(gfx::Point(-40, 10).ToString(),
            recorder.touch_locations()[0].ToString());
  EXPECT_EQ(gfx::Point(-40, 10).ToString(),
            recorder.touch_locations()[1].ToString());
}

class SelfDestructDelegate : public test::TestWindowDelegate {
 public:
  SelfDestructDelegate() {}
  virtual ~SelfDestructDelegate() {}

  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    window_.reset();
  }

  void set_window(scoped_ptr<aura::Window> window) {
    window_ = window.Pass();
  }
  bool has_window() const { return !!window_.get(); }

 private:
  scoped_ptr<aura::Window> window_;
  DISALLOW_COPY_AND_ASSIGN(SelfDestructDelegate);
};

TEST_F(WindowEventDispatcherTest, SynthesizedLocatedEvent) {
  test::EventGenerator generator(root_window());
  generator.MoveMouseTo(10, 10);
  EXPECT_EQ("10,10",
            Env::GetInstance()->last_mouse_location().ToString());

  // Synthesized event should not update the mouse location.
  ui::MouseEvent mouseev(ui::ET_MOUSE_MOVED, gfx::Point(), gfx::Point(),
                         ui::EF_IS_SYNTHESIZED, 0);
  generator.Dispatch(&mouseev);
  EXPECT_EQ("10,10",
            Env::GetInstance()->last_mouse_location().ToString());

  generator.MoveMouseTo(0, 0);
  EXPECT_EQ("0,0",
            Env::GetInstance()->last_mouse_location().ToString());

  // Make sure the location gets updated when a syntheiszed enter
  // event destroyed the window.
  SelfDestructDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(50, 50, 100, 100), root_window()));
  delegate.set_window(window.Pass());
  EXPECT_TRUE(delegate.has_window());

  generator.MoveMouseTo(100, 100);
  EXPECT_FALSE(delegate.has_window());
  EXPECT_EQ("100,100",
            Env::GetInstance()->last_mouse_location().ToString());
}

class StaticFocusClient : public client::FocusClient {
 public:
  explicit StaticFocusClient(Window* focused)
      : focused_(focused) {}
  virtual ~StaticFocusClient() {}

 private:
  // client::FocusClient:
  virtual void AddObserver(client::FocusChangeObserver* observer) OVERRIDE {}
  virtual void RemoveObserver(client::FocusChangeObserver* observer) OVERRIDE {}
  virtual void FocusWindow(Window* window) OVERRIDE {}
  virtual void ResetFocusWithinActiveWindow(Window* window) OVERRIDE {}
  virtual Window* GetFocusedWindow() OVERRIDE { return focused_; }

  Window* focused_;

  DISALLOW_COPY_AND_ASSIGN(StaticFocusClient);
};

// Tests that host-cancel-mode event can be dispatched to a dispatcher safely
// when the focused window does not live in the dispatcher's tree.
TEST_F(WindowEventDispatcherTest, HostCancelModeWithFocusedWindowOutside) {
  test::TestWindowDelegate delegate;
  scoped_ptr<Window> focused(CreateTestWindowWithDelegate(&delegate, 123,
      gfx::Rect(20, 30, 100, 50), NULL));
  StaticFocusClient focus_client(focused.get());
  client::SetFocusClient(root_window(), &focus_client);
  EXPECT_FALSE(root_window()->Contains(focused.get()));
  EXPECT_EQ(focused.get(),
            client::GetFocusClient(root_window())->GetFocusedWindow());
  host()->dispatcher()->DispatchCancelModeEvent();
  EXPECT_EQ(focused.get(),
            client::GetFocusClient(root_window())->GetFocusedWindow());
}

// Dispatches a mouse-move event to |target| when it receives a mouse-move
// event.
class DispatchEventHandler : public ui::EventHandler {
 public:
  explicit DispatchEventHandler(Window* target)
      : target_(target),
        dispatched_(false) {}
  virtual ~DispatchEventHandler() {}

  bool dispatched() const { return dispatched_; }
 private:
  // ui::EventHandler:
  virtual void OnMouseEvent(ui::MouseEvent* mouse) OVERRIDE {
    if (mouse->type() == ui::ET_MOUSE_MOVED) {
      ui::MouseEvent move(ui::ET_MOUSE_MOVED, target_->bounds().CenterPoint(),
          target_->bounds().CenterPoint(), ui::EF_NONE, ui::EF_NONE);
      ui::EventDispatchDetails details =
          target_->GetHost()->dispatcher()->OnEventFromSource(&move);
      ASSERT_FALSE(details.dispatcher_destroyed);
      EXPECT_FALSE(details.target_destroyed);
      EXPECT_EQ(target_, move.target());
      dispatched_ = true;
    }
    ui::EventHandler::OnMouseEvent(mouse);
  }

  Window* target_;
  bool dispatched_;

  DISALLOW_COPY_AND_ASSIGN(DispatchEventHandler);
};

// Moves |window| to |root_window| when it receives a mouse-move event.
class MoveWindowHandler : public ui::EventHandler {
 public:
  MoveWindowHandler(Window* window, Window* root_window)
      : window_to_move_(window),
        root_window_to_move_to_(root_window) {}
  virtual ~MoveWindowHandler() {}

 private:
  // ui::EventHandler:
  virtual void OnMouseEvent(ui::MouseEvent* mouse) OVERRIDE {
    if (mouse->type() == ui::ET_MOUSE_MOVED) {
      root_window_to_move_to_->AddChild(window_to_move_);
    }
    ui::EventHandler::OnMouseEvent(mouse);
  }

  Window* window_to_move_;
  Window* root_window_to_move_to_;

  DISALLOW_COPY_AND_ASSIGN(MoveWindowHandler);
};

// Tests that nested event dispatch works correctly if the target of the older
// event being dispatched is moved to a different dispatcher in response to an
// event in the inner loop.
TEST_F(WindowEventDispatcherTest, NestedEventDispatchTargetMoved) {
  scoped_ptr<WindowTreeHost> second_host(
      WindowTreeHost::Create(gfx::Rect(20, 30, 100, 50)));
  second_host->InitHost();
  Window* second_root = second_host->window();

  // Create two windows parented to |root_window()|.
  test::TestWindowDelegate delegate;
  scoped_ptr<Window> first(CreateTestWindowWithDelegate(&delegate, 123,
      gfx::Rect(20, 10, 10, 20), root_window()));
  scoped_ptr<Window> second(CreateTestWindowWithDelegate(&delegate, 234,
      gfx::Rect(40, 10, 50, 20), root_window()));

  // Setup a handler on |first| so that it dispatches an event to |second| when
  // |first| receives an event.
  DispatchEventHandler dispatch_event(second.get());
  first->AddPreTargetHandler(&dispatch_event);

  // Setup a handler on |second| so that it moves |first| into |second_root|
  // when |second| receives an event.
  MoveWindowHandler move_window(first.get(), second_root);
  second->AddPreTargetHandler(&move_window);

  // Some sanity checks: |first| is inside |root_window()|'s tree.
  EXPECT_EQ(root_window(), first->GetRootWindow());
  // The two root windows are different.
  EXPECT_NE(root_window(), second_root);

  // Dispatch an event to |first|.
  ui::MouseEvent move(ui::ET_MOUSE_MOVED, first->bounds().CenterPoint(),
                      first->bounds().CenterPoint(), ui::EF_NONE, ui::EF_NONE);
  ui::EventDispatchDetails details =
      host()->dispatcher()->OnEventFromSource(&move);
  ASSERT_FALSE(details.dispatcher_destroyed);
  EXPECT_TRUE(details.target_destroyed);
  EXPECT_EQ(first.get(), move.target());
  EXPECT_TRUE(dispatch_event.dispatched());
  EXPECT_EQ(second_root, first->GetRootWindow());

  first->RemovePreTargetHandler(&dispatch_event);
  second->RemovePreTargetHandler(&move_window);
}

class AlwaysMouseDownInputStateLookup : public InputStateLookup {
 public:
  AlwaysMouseDownInputStateLookup() {}
  virtual ~AlwaysMouseDownInputStateLookup() {}

 private:
  // InputStateLookup:
  virtual bool IsMouseButtonDown() const OVERRIDE { return true; }

  DISALLOW_COPY_AND_ASSIGN(AlwaysMouseDownInputStateLookup);
};

TEST_F(WindowEventDispatcherTest,
       CursorVisibilityChangedWhileCaptureWindowInAnotherDispatcher) {
  test::EventCountDelegate delegate;
  scoped_ptr<Window> window(CreateTestWindowWithDelegate(&delegate, 123,
      gfx::Rect(20, 10, 10, 20), root_window()));
  window->Show();

  scoped_ptr<WindowTreeHost> second_host(
      WindowTreeHost::Create(gfx::Rect(20, 30, 100, 50)));
  second_host->InitHost();
  WindowEventDispatcher* second_dispatcher = second_host->dispatcher();

  // Install an InputStateLookup on the Env that always claims that a
  // mouse-button is down.
  test::EnvTestHelper(Env::GetInstance()).SetInputStateLookup(
      scoped_ptr<InputStateLookup>(new AlwaysMouseDownInputStateLookup()));

  window->SetCapture();

  // Because the mouse button is down, setting the capture on |window| will set
  // it as the mouse-move handler for |root_window()|.
  EXPECT_EQ(window.get(), host()->dispatcher()->mouse_moved_handler());

  // This does not set |window| as the mouse-move handler for the second
  // dispatcher.
  EXPECT_EQ(NULL, second_dispatcher->mouse_moved_handler());

  // However, some capture-client updates the capture in each root-window on a
  // capture. Emulate that here. Because of this, the second dispatcher also has
  // |window| as the mouse-move handler.
  client::CaptureDelegate* second_capture_delegate = second_dispatcher;
  second_capture_delegate->UpdateCapture(NULL, window.get());
  EXPECT_EQ(window.get(), second_dispatcher->mouse_moved_handler());

  // Reset the mouse-event counts for |window|.
  delegate.GetMouseMotionCountsAndReset();

  // Notify both hosts that the cursor is now hidden. This should send a single
  // mouse-exit event to |window|.
  host()->OnCursorVisibilityChanged(false);
  second_host->OnCursorVisibilityChanged(false);
  EXPECT_EQ("0 0 1", delegate.GetMouseMotionCountsAndReset());
}

}  // namespace aura
