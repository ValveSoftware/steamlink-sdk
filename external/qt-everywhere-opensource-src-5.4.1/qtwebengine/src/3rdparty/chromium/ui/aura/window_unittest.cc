// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window.h"

#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/visibility_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/test/window_test_api.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_property.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/test_layers.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gestures/gesture_configuration.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vector2d.h"

DECLARE_WINDOW_PROPERTY_TYPE(const char*)
DECLARE_WINDOW_PROPERTY_TYPE(int)

namespace aura {
namespace test {

class WindowTest : public AuraTestBase {
 public:
  WindowTest() : max_separation_(0) {
  }

  virtual void SetUp() OVERRIDE {
    AuraTestBase::SetUp();
    // TODO: there needs to be an easier way to do this.
    max_separation_ = ui::GestureConfiguration::
        max_separation_for_gesture_touches_in_pixels();
    ui::GestureConfiguration::
        set_max_separation_for_gesture_touches_in_pixels(0);
  }

  virtual void TearDown() OVERRIDE {
    AuraTestBase::TearDown();
    ui::GestureConfiguration::
        set_max_separation_for_gesture_touches_in_pixels(max_separation_);
  }

 private:
  int max_separation_;

  DISALLOW_COPY_AND_ASSIGN(WindowTest);
};

namespace {

// Used for verifying destruction methods are invoked.
class DestroyTrackingDelegateImpl : public TestWindowDelegate {
 public:
  DestroyTrackingDelegateImpl()
      : destroying_count_(0),
        destroyed_count_(0),
        in_destroying_(false) {}

  void clear_destroying_count() { destroying_count_ = 0; }
  int destroying_count() const { return destroying_count_; }

  void clear_destroyed_count() { destroyed_count_ = 0; }
  int destroyed_count() const { return destroyed_count_; }

  bool in_destroying() const { return in_destroying_; }

  virtual void OnWindowDestroying(Window* window) OVERRIDE {
    EXPECT_FALSE(in_destroying_);
    in_destroying_ = true;
    destroying_count_++;
  }

  virtual void OnWindowDestroyed(Window* window) OVERRIDE {
    EXPECT_TRUE(in_destroying_);
    in_destroying_ = false;
    destroyed_count_++;
  }

 private:
  int destroying_count_;
  int destroyed_count_;
  bool in_destroying_;

  DISALLOW_COPY_AND_ASSIGN(DestroyTrackingDelegateImpl);
};

// Used to verify that when OnWindowDestroying is invoked the parent is also
// is in the process of being destroyed.
class ChildWindowDelegateImpl : public DestroyTrackingDelegateImpl {
 public:
  explicit ChildWindowDelegateImpl(
      DestroyTrackingDelegateImpl* parent_delegate)
      : parent_delegate_(parent_delegate) {
  }

  virtual void OnWindowDestroying(Window* window) OVERRIDE {
    EXPECT_TRUE(parent_delegate_->in_destroying());
    DestroyTrackingDelegateImpl::OnWindowDestroying(window);
  }

 private:
  DestroyTrackingDelegateImpl* parent_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ChildWindowDelegateImpl);
};

// Used to verify that a Window is removed from its parent when
// OnWindowDestroyed is called.
class DestroyOrphanDelegate : public TestWindowDelegate {
 public:
  DestroyOrphanDelegate() : window_(NULL) {
  }

  void set_window(Window* window) { window_ = window; }

  virtual void OnWindowDestroyed(Window* window) OVERRIDE {
    EXPECT_FALSE(window_->parent());
  }

 private:
  Window* window_;
  DISALLOW_COPY_AND_ASSIGN(DestroyOrphanDelegate);
};

// Used in verifying mouse capture.
class CaptureWindowDelegateImpl : public TestWindowDelegate {
 public:
  CaptureWindowDelegateImpl() {
    ResetCounts();
  }

  void ResetCounts() {
    capture_changed_event_count_ = 0;
    capture_lost_count_ = 0;
    mouse_event_count_ = 0;
    touch_event_count_ = 0;
    gesture_event_count_ = 0;
  }

  int capture_changed_event_count() const {
    return capture_changed_event_count_;
  }
  int capture_lost_count() const { return capture_lost_count_; }
  int mouse_event_count() const { return mouse_event_count_; }
  int touch_event_count() const { return touch_event_count_; }
  int gesture_event_count() const { return gesture_event_count_; }

  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    if (event->type() == ui::ET_MOUSE_CAPTURE_CHANGED)
      capture_changed_event_count_++;
    mouse_event_count_++;
  }
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE {
    touch_event_count_++;
  }
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    gesture_event_count_++;
  }
  virtual void OnCaptureLost() OVERRIDE {
    capture_lost_count_++;
  }

 private:
  int capture_changed_event_count_;
  int capture_lost_count_;
  int mouse_event_count_;
  int touch_event_count_;
  int gesture_event_count_;

  DISALLOW_COPY_AND_ASSIGN(CaptureWindowDelegateImpl);
};

// Keeps track of the location of the gesture.
class GestureTrackPositionDelegate : public TestWindowDelegate {
 public:
  GestureTrackPositionDelegate() {}

  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    position_ = event->location();
    event->StopPropagation();
  }

  const gfx::Point& position() const { return position_; }

 private:
  gfx::Point position_;

  DISALLOW_COPY_AND_ASSIGN(GestureTrackPositionDelegate);
};

base::TimeDelta getTime() {
  return ui::EventTimeForNow();
}

class SelfEventHandlingWindowDelegate : public TestWindowDelegate {
 public:
  SelfEventHandlingWindowDelegate() {}

  virtual bool ShouldDescendIntoChildForEventHandling(
      Window* child,
      const gfx::Point& location) OVERRIDE {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SelfEventHandlingWindowDelegate);
};

// The delegate deletes itself when the window is being destroyed.
class DestroyWindowDelegate : public TestWindowDelegate {
 public:
  DestroyWindowDelegate() {}

 private:
  virtual ~DestroyWindowDelegate() {}

  // Overridden from WindowDelegate.
  virtual void OnWindowDestroyed(Window* window) OVERRIDE {
    delete this;
  }

  DISALLOW_COPY_AND_ASSIGN(DestroyWindowDelegate);
};

}  // namespace

TEST_F(WindowTest, GetChildById) {
  scoped_ptr<Window> w1(CreateTestWindowWithId(1, root_window()));
  scoped_ptr<Window> w11(CreateTestWindowWithId(11, w1.get()));
  scoped_ptr<Window> w111(CreateTestWindowWithId(111, w11.get()));
  scoped_ptr<Window> w12(CreateTestWindowWithId(12, w1.get()));

  EXPECT_EQ(NULL, w1->GetChildById(57));
  EXPECT_EQ(w12.get(), w1->GetChildById(12));
  EXPECT_EQ(w111.get(), w1->GetChildById(111));
}

// Make sure that Window::Contains correctly handles children, grandchildren,
// and not containing NULL or parents.
TEST_F(WindowTest, Contains) {
  Window parent(NULL);
  parent.Init(aura::WINDOW_LAYER_NOT_DRAWN);
  Window child1(NULL);
  child1.Init(aura::WINDOW_LAYER_NOT_DRAWN);
  Window child2(NULL);
  child2.Init(aura::WINDOW_LAYER_NOT_DRAWN);

  parent.AddChild(&child1);
  child1.AddChild(&child2);

  EXPECT_TRUE(parent.Contains(&parent));
  EXPECT_TRUE(parent.Contains(&child1));
  EXPECT_TRUE(parent.Contains(&child2));

  EXPECT_FALSE(parent.Contains(NULL));
  EXPECT_FALSE(child1.Contains(&parent));
  EXPECT_FALSE(child2.Contains(&child1));
}

TEST_F(WindowTest, ContainsPointInRoot) {
  scoped_ptr<Window> w(
      CreateTestWindow(SK_ColorWHITE, 1, gfx::Rect(10, 10, 5, 5),
                       root_window()));
  EXPECT_FALSE(w->ContainsPointInRoot(gfx::Point(9, 9)));
  EXPECT_TRUE(w->ContainsPointInRoot(gfx::Point(10, 10)));
  EXPECT_TRUE(w->ContainsPointInRoot(gfx::Point(14, 14)));
  EXPECT_FALSE(w->ContainsPointInRoot(gfx::Point(15, 15)));
  EXPECT_FALSE(w->ContainsPointInRoot(gfx::Point(20, 20)));
}

TEST_F(WindowTest, ContainsPoint) {
  scoped_ptr<Window> w(
      CreateTestWindow(SK_ColorWHITE, 1, gfx::Rect(10, 10, 5, 5),
                       root_window()));
  EXPECT_TRUE(w->ContainsPoint(gfx::Point(0, 0)));
  EXPECT_TRUE(w->ContainsPoint(gfx::Point(4, 4)));
  EXPECT_FALSE(w->ContainsPoint(gfx::Point(5, 5)));
  EXPECT_FALSE(w->ContainsPoint(gfx::Point(10, 10)));
}

TEST_F(WindowTest, ConvertPointToWindow) {
  // Window::ConvertPointToWindow is mostly identical to
  // Layer::ConvertPointToLayer, except NULL values for |source| are permitted,
  // in which case the function just returns.
  scoped_ptr<Window> w1(CreateTestWindowWithId(1, root_window()));
  gfx::Point reference_point(100, 100);
  gfx::Point test_point = reference_point;
  Window::ConvertPointToTarget(NULL, w1.get(), &test_point);
  EXPECT_EQ(reference_point, test_point);
}

TEST_F(WindowTest, MoveCursorTo) {
  scoped_ptr<Window> w1(
      CreateTestWindow(SK_ColorWHITE, 1, gfx::Rect(10, 10, 500, 500),
                       root_window()));
  scoped_ptr<Window> w11(
      CreateTestWindow(SK_ColorGREEN, 11, gfx::Rect(5, 5, 100, 100), w1.get()));
  scoped_ptr<Window> w111(
      CreateTestWindow(SK_ColorCYAN, 111, gfx::Rect(5, 5, 75, 75), w11.get()));
  scoped_ptr<Window> w1111(
      CreateTestWindow(SK_ColorRED, 1111, gfx::Rect(5, 5, 50, 50), w111.get()));

  Window* root = root_window();
  root->MoveCursorTo(gfx::Point(10, 10));
  EXPECT_EQ("10,10",
      gfx::Screen::GetScreenFor(root)->GetCursorScreenPoint().ToString());
  w1->MoveCursorTo(gfx::Point(10, 10));
  EXPECT_EQ("20,20",
      gfx::Screen::GetScreenFor(root)->GetCursorScreenPoint().ToString());
  w11->MoveCursorTo(gfx::Point(10, 10));
  EXPECT_EQ("25,25",
      gfx::Screen::GetScreenFor(root)->GetCursorScreenPoint().ToString());
  w111->MoveCursorTo(gfx::Point(10, 10));
  EXPECT_EQ("30,30",
      gfx::Screen::GetScreenFor(root)->GetCursorScreenPoint().ToString());
  w1111->MoveCursorTo(gfx::Point(10, 10));
  EXPECT_EQ("35,35",
      gfx::Screen::GetScreenFor(root)->GetCursorScreenPoint().ToString());
}

TEST_F(WindowTest, ContainsMouse) {
  scoped_ptr<Window> w(
      CreateTestWindow(SK_ColorWHITE, 1, gfx::Rect(10, 10, 500, 500),
                       root_window()));
  w->Show();
  WindowTestApi w_test_api(w.get());
  Window* root = root_window();
  root->MoveCursorTo(gfx::Point(10, 10));
  EXPECT_TRUE(w_test_api.ContainsMouse());
  root->MoveCursorTo(gfx::Point(9, 10));
  EXPECT_FALSE(w_test_api.ContainsMouse());
}

// Test Window::ConvertPointToWindow() with transform to root_window.
#if defined(USE_OZONE)
// TODO(rjkroege): Add cursor support in ozone: http://crbug.com/252315.
TEST_F(WindowTest, DISABLED_MoveCursorToWithTransformRootWindow) {
#else
TEST_F(WindowTest, MoveCursorToWithTransformRootWindow) {
#endif
  gfx::Transform transform;
  transform.Translate(100.0, 100.0);
  transform.Rotate(90.0);
  transform.Scale(2.0, 5.0);
  host()->SetRootTransform(transform);
  host()->MoveCursorTo(gfx::Point(10, 10));
#if !defined(OS_WIN)
  // TODO(yoshiki): fix this to build on Windows. See crbug.com/133413.OD
  EXPECT_EQ("50,120", QueryLatestMousePositionRequestInHost(host()).ToString());
#endif
  EXPECT_EQ("10,10", gfx::Screen::GetScreenFor(
      root_window())->GetCursorScreenPoint().ToString());
}

// Tests Window::ConvertPointToWindow() with transform to non-root windows.
TEST_F(WindowTest, MoveCursorToWithTransformWindow) {
  scoped_ptr<Window> w1(
      CreateTestWindow(SK_ColorWHITE, 1, gfx::Rect(10, 10, 500, 500),
                       root_window()));

  gfx::Transform transform1;
  transform1.Scale(2, 2);
  w1->SetTransform(transform1);
  w1->MoveCursorTo(gfx::Point(10, 10));
  EXPECT_EQ("30,30",
      gfx::Screen::GetScreenFor(w1.get())->GetCursorScreenPoint().ToString());

  gfx::Transform transform2;
  transform2.Translate(-10, 20);
  w1->SetTransform(transform2);
  w1->MoveCursorTo(gfx::Point(10, 10));
  EXPECT_EQ("10,40",
      gfx::Screen::GetScreenFor(w1.get())->GetCursorScreenPoint().ToString());

  gfx::Transform transform3;
  transform3.Rotate(90.0);
  w1->SetTransform(transform3);
  w1->MoveCursorTo(gfx::Point(5, 5));
  EXPECT_EQ("5,15",
      gfx::Screen::GetScreenFor(w1.get())->GetCursorScreenPoint().ToString());

  gfx::Transform transform4;
  transform4.Translate(100.0, 100.0);
  transform4.Rotate(90.0);
  transform4.Scale(2.0, 5.0);
  w1->SetTransform(transform4);
  w1->MoveCursorTo(gfx::Point(10, 10));
  EXPECT_EQ("60,130",
      gfx::Screen::GetScreenFor(w1.get())->GetCursorScreenPoint().ToString());
}

// Test Window::ConvertPointToWindow() with complex transforms to both root and
// non-root windows.
// Test Window::ConvertPointToWindow() with transform to root_window.
#if defined(USE_OZONE)
// TODO(rjkroege): Add cursor support in ozone: http://crbug.com/252315.
TEST_F(WindowTest, DISABLED_MoveCursorToWithComplexTransform) {
#else
TEST_F(WindowTest, MoveCursorToWithComplexTransform) {
#endif
  scoped_ptr<Window> w1(
      CreateTestWindow(SK_ColorWHITE, 1, gfx::Rect(10, 10, 500, 500),
                       root_window()));
  scoped_ptr<Window> w11(
      CreateTestWindow(SK_ColorGREEN, 11, gfx::Rect(5, 5, 100, 100), w1.get()));
  scoped_ptr<Window> w111(
      CreateTestWindow(SK_ColorCYAN, 111, gfx::Rect(5, 5, 75, 75), w11.get()));
  scoped_ptr<Window> w1111(
      CreateTestWindow(SK_ColorRED, 1111, gfx::Rect(5, 5, 50, 50), w111.get()));

  Window* root = root_window();

  // The root window expects transforms that produce integer rects.
  gfx::Transform root_transform;
  root_transform.Translate(60.0, 70.0);
  root_transform.Rotate(-90.0);
  root_transform.Translate(-50.0, -50.0);
  root_transform.Scale(2.0, 3.0);

  gfx::Transform transform;
  transform.Translate(10.0, 20.0);
  transform.Rotate(10.0);
  transform.Scale(0.3f, 0.5f);
  host()->SetRootTransform(root_transform);
  w1->SetTransform(transform);
  w11->SetTransform(transform);
  w111->SetTransform(transform);
  w1111->SetTransform(transform);

  w1111->MoveCursorTo(gfx::Point(10, 10));

#if !defined(OS_WIN)
  // TODO(yoshiki): fix this to build on Windows. See crbug.com/133413.
  EXPECT_EQ("169,80", QueryLatestMousePositionRequestInHost(host()).ToString());
#endif
  EXPECT_EQ("20,53",
      gfx::Screen::GetScreenFor(root)->GetCursorScreenPoint().ToString());
}

TEST_F(WindowTest, GetEventHandlerForPoint) {
  scoped_ptr<Window> w1(
      CreateTestWindow(SK_ColorWHITE, 1, gfx::Rect(10, 10, 500, 500),
                       root_window()));
  scoped_ptr<Window> w11(
      CreateTestWindow(SK_ColorGREEN, 11, gfx::Rect(5, 5, 100, 100), w1.get()));
  scoped_ptr<Window> w111(
      CreateTestWindow(SK_ColorCYAN, 111, gfx::Rect(5, 5, 75, 75), w11.get()));
  scoped_ptr<Window> w1111(
      CreateTestWindow(SK_ColorRED, 1111, gfx::Rect(5, 5, 50, 50), w111.get()));
  scoped_ptr<Window> w12(
      CreateTestWindow(SK_ColorMAGENTA, 12, gfx::Rect(10, 420, 25, 25),
                       w1.get()));
  scoped_ptr<Window> w121(
      CreateTestWindow(SK_ColorYELLOW, 121, gfx::Rect(5, 5, 5, 5), w12.get()));
  scoped_ptr<Window> w13(
      CreateTestWindow(SK_ColorGRAY, 13, gfx::Rect(5, 470, 50, 50), w1.get()));

  Window* root = root_window();
  w1->parent()->SetBounds(gfx::Rect(500, 500));
  EXPECT_EQ(NULL, root->GetEventHandlerForPoint(gfx::Point(5, 5)));
  EXPECT_EQ(w1.get(), root->GetEventHandlerForPoint(gfx::Point(11, 11)));
  EXPECT_EQ(w11.get(), root->GetEventHandlerForPoint(gfx::Point(16, 16)));
  EXPECT_EQ(w111.get(), root->GetEventHandlerForPoint(gfx::Point(21, 21)));
  EXPECT_EQ(w1111.get(), root->GetEventHandlerForPoint(gfx::Point(26, 26)));
  EXPECT_EQ(w12.get(), root->GetEventHandlerForPoint(gfx::Point(21, 431)));
  EXPECT_EQ(w121.get(), root->GetEventHandlerForPoint(gfx::Point(26, 436)));
  EXPECT_EQ(w13.get(), root->GetEventHandlerForPoint(gfx::Point(26, 481)));
}

TEST_F(WindowTest, GetEventHandlerForPointWithOverride) {
  // If our child is flush to our top-left corner he gets events just inside the
  // window edges.
  scoped_ptr<Window> parent(
      CreateTestWindow(SK_ColorWHITE, 1, gfx::Rect(10, 20, 400, 500),
                       root_window()));
  scoped_ptr<Window> child(
      CreateTestWindow(SK_ColorRED, 2, gfx::Rect(0, 0, 60, 70), parent.get()));
  EXPECT_EQ(child.get(), parent->GetEventHandlerForPoint(gfx::Point(0, 0)));
  EXPECT_EQ(child.get(), parent->GetEventHandlerForPoint(gfx::Point(1, 1)));

  // We can override the hit test bounds of the parent to make the parent grab
  // events along that edge.
  parent->set_hit_test_bounds_override_inner(gfx::Insets(1, 1, 1, 1));
  EXPECT_EQ(parent.get(), parent->GetEventHandlerForPoint(gfx::Point(0, 0)));
  EXPECT_EQ(child.get(),  parent->GetEventHandlerForPoint(gfx::Point(1, 1)));
}

TEST_F(WindowTest, GetEventHandlerForPointWithOverrideDescendingOrder) {
  scoped_ptr<SelfEventHandlingWindowDelegate> parent_delegate(
      new SelfEventHandlingWindowDelegate);
  scoped_ptr<Window> parent(CreateTestWindowWithDelegate(
      parent_delegate.get(), 1, gfx::Rect(10, 20, 400, 500), root_window()));
  scoped_ptr<Window> child(
      CreateTestWindow(SK_ColorRED, 2, gfx::Rect(0, 0, 390, 480),
                       parent.get()));

  // We can override ShouldDescendIntoChildForEventHandling to make the parent
  // grab all events.
  EXPECT_EQ(parent.get(), parent->GetEventHandlerForPoint(gfx::Point(0, 0)));
  EXPECT_EQ(parent.get(), parent->GetEventHandlerForPoint(gfx::Point(50, 50)));
}

TEST_F(WindowTest, GetTopWindowContainingPoint) {
  Window* root = root_window();
  root->SetBounds(gfx::Rect(0, 0, 300, 300));

  scoped_ptr<Window> w1(
      CreateTestWindow(SK_ColorWHITE, 1, gfx::Rect(10, 10, 100, 100),
                       root_window()));
  scoped_ptr<Window> w11(
      CreateTestWindow(SK_ColorGREEN, 11, gfx::Rect(0, 0, 120, 120), w1.get()));

  scoped_ptr<Window> w2(
      CreateTestWindow(SK_ColorRED, 2, gfx::Rect(5, 5, 55, 55),
                       root_window()));

  scoped_ptr<Window> w3(
      CreateTestWindowWithDelegate(
          NULL, 3, gfx::Rect(200, 200, 100, 100), root_window()));
  scoped_ptr<Window> w31(
      CreateTestWindow(SK_ColorCYAN, 31, gfx::Rect(0, 0, 50, 50), w3.get()));
  scoped_ptr<Window> w311(
      CreateTestWindow(SK_ColorBLUE, 311, gfx::Rect(0, 0, 10, 10), w31.get()));

  EXPECT_EQ(NULL, root->GetTopWindowContainingPoint(gfx::Point(0, 0)));
  EXPECT_EQ(w2.get(), root->GetTopWindowContainingPoint(gfx::Point(5, 5)));
  EXPECT_EQ(w2.get(), root->GetTopWindowContainingPoint(gfx::Point(10, 10)));
  EXPECT_EQ(w2.get(), root->GetTopWindowContainingPoint(gfx::Point(59, 59)));
  EXPECT_EQ(w1.get(), root->GetTopWindowContainingPoint(gfx::Point(60, 60)));
  EXPECT_EQ(w1.get(), root->GetTopWindowContainingPoint(gfx::Point(109, 109)));
  EXPECT_EQ(NULL, root->GetTopWindowContainingPoint(gfx::Point(110, 110)));
  EXPECT_EQ(w31.get(), root->GetTopWindowContainingPoint(gfx::Point(200, 200)));
  EXPECT_EQ(w31.get(), root->GetTopWindowContainingPoint(gfx::Point(220, 220)));
  EXPECT_EQ(NULL, root->GetTopWindowContainingPoint(gfx::Point(260, 260)));
}

TEST_F(WindowTest, GetToplevelWindow) {
  const gfx::Rect kBounds(0, 0, 10, 10);
  TestWindowDelegate delegate;

  scoped_ptr<Window> w1(CreateTestWindowWithId(1, root_window()));
  scoped_ptr<Window> w11(
      CreateTestWindowWithDelegate(&delegate, 11, kBounds, w1.get()));
  scoped_ptr<Window> w111(CreateTestWindowWithId(111, w11.get()));
  scoped_ptr<Window> w1111(
      CreateTestWindowWithDelegate(&delegate, 1111, kBounds, w111.get()));

  EXPECT_TRUE(root_window()->GetToplevelWindow() == NULL);
  EXPECT_TRUE(w1->GetToplevelWindow() == NULL);
  EXPECT_EQ(w11.get(), w11->GetToplevelWindow());
  EXPECT_EQ(w11.get(), w111->GetToplevelWindow());
  EXPECT_EQ(w11.get(), w1111->GetToplevelWindow());
}

class AddedToRootWindowObserver : public WindowObserver {
 public:
  AddedToRootWindowObserver() : called_(false) {}

  virtual void OnWindowAddedToRootWindow(Window* window) OVERRIDE {
    called_ = true;
  }

  bool called() const { return called_; }

 private:
  bool called_;

  DISALLOW_COPY_AND_ASSIGN(AddedToRootWindowObserver);
};

TEST_F(WindowTest, WindowAddedToRootWindowShouldNotifyChildAndNotParent) {
  AddedToRootWindowObserver parent_observer;
  AddedToRootWindowObserver child_observer;
  scoped_ptr<Window> parent_window(CreateTestWindowWithId(1, root_window()));
  scoped_ptr<Window> child_window(new Window(NULL));
  child_window->Init(aura::WINDOW_LAYER_TEXTURED);
  child_window->Show();

  parent_window->AddObserver(&parent_observer);
  child_window->AddObserver(&child_observer);

  parent_window->AddChild(child_window.get());

  EXPECT_FALSE(parent_observer.called());
  EXPECT_TRUE(child_observer.called());

  parent_window->RemoveObserver(&parent_observer);
  child_window->RemoveObserver(&child_observer);
}

// Various destruction assertions.
TEST_F(WindowTest, DestroyTest) {
  DestroyTrackingDelegateImpl parent_delegate;
  ChildWindowDelegateImpl child_delegate(&parent_delegate);
  {
    scoped_ptr<Window> parent(
        CreateTestWindowWithDelegate(&parent_delegate, 0, gfx::Rect(),
                                     root_window()));
    CreateTestWindowWithDelegate(&child_delegate, 0, gfx::Rect(), parent.get());
  }
  // Both the parent and child should have been destroyed.
  EXPECT_EQ(1, parent_delegate.destroying_count());
  EXPECT_EQ(1, parent_delegate.destroyed_count());
  EXPECT_EQ(1, child_delegate.destroying_count());
  EXPECT_EQ(1, child_delegate.destroyed_count());
}

// Tests that a window is orphaned before OnWindowDestroyed is called.
TEST_F(WindowTest, OrphanedBeforeOnDestroyed) {
  TestWindowDelegate parent_delegate;
  DestroyOrphanDelegate child_delegate;
  {
    scoped_ptr<Window> parent(
        CreateTestWindowWithDelegate(&parent_delegate, 0, gfx::Rect(),
                                     root_window()));
    scoped_ptr<Window> child(CreateTestWindowWithDelegate(&child_delegate, 0,
          gfx::Rect(), parent.get()));
    child_delegate.set_window(child.get());
  }
}

// Make sure StackChildAtTop moves both the window and layer to the front.
TEST_F(WindowTest, StackChildAtTop) {
  Window parent(NULL);
  parent.Init(aura::WINDOW_LAYER_NOT_DRAWN);
  Window child1(NULL);
  child1.Init(aura::WINDOW_LAYER_NOT_DRAWN);
  Window child2(NULL);
  child2.Init(aura::WINDOW_LAYER_NOT_DRAWN);

  parent.AddChild(&child1);
  parent.AddChild(&child2);
  ASSERT_EQ(2u, parent.children().size());
  EXPECT_EQ(&child1, parent.children()[0]);
  EXPECT_EQ(&child2, parent.children()[1]);
  ASSERT_EQ(2u, parent.layer()->children().size());
  EXPECT_EQ(child1.layer(), parent.layer()->children()[0]);
  EXPECT_EQ(child2.layer(), parent.layer()->children()[1]);

  parent.StackChildAtTop(&child1);
  ASSERT_EQ(2u, parent.children().size());
  EXPECT_EQ(&child1, parent.children()[1]);
  EXPECT_EQ(&child2, parent.children()[0]);
  ASSERT_EQ(2u, parent.layer()->children().size());
  EXPECT_EQ(child1.layer(), parent.layer()->children()[1]);
  EXPECT_EQ(child2.layer(), parent.layer()->children()[0]);
}

// Make sure StackChildBelow works.
TEST_F(WindowTest, StackChildBelow) {
  Window parent(NULL);
  parent.Init(aura::WINDOW_LAYER_NOT_DRAWN);
  Window child1(NULL);
  child1.Init(aura::WINDOW_LAYER_NOT_DRAWN);
  child1.set_id(1);
  Window child2(NULL);
  child2.Init(aura::WINDOW_LAYER_NOT_DRAWN);
  child2.set_id(2);
  Window child3(NULL);
  child3.Init(aura::WINDOW_LAYER_NOT_DRAWN);
  child3.set_id(3);

  parent.AddChild(&child1);
  parent.AddChild(&child2);
  parent.AddChild(&child3);
  EXPECT_EQ("1 2 3", ChildWindowIDsAsString(&parent));

  parent.StackChildBelow(&child1, &child2);
  EXPECT_EQ("1 2 3", ChildWindowIDsAsString(&parent));

  parent.StackChildBelow(&child2, &child1);
  EXPECT_EQ("2 1 3", ChildWindowIDsAsString(&parent));

  parent.StackChildBelow(&child3, &child2);
  EXPECT_EQ("3 2 1", ChildWindowIDsAsString(&parent));

  parent.StackChildBelow(&child3, &child1);
  EXPECT_EQ("2 3 1", ChildWindowIDsAsString(&parent));
}

// Various assertions for StackChildAbove.
TEST_F(WindowTest, StackChildAbove) {
  Window parent(NULL);
  parent.Init(aura::WINDOW_LAYER_NOT_DRAWN);
  Window child1(NULL);
  child1.Init(aura::WINDOW_LAYER_NOT_DRAWN);
  Window child2(NULL);
  child2.Init(aura::WINDOW_LAYER_NOT_DRAWN);
  Window child3(NULL);
  child3.Init(aura::WINDOW_LAYER_NOT_DRAWN);

  parent.AddChild(&child1);
  parent.AddChild(&child2);

  // Move 1 in front of 2.
  parent.StackChildAbove(&child1, &child2);
  ASSERT_EQ(2u, parent.children().size());
  EXPECT_EQ(&child2, parent.children()[0]);
  EXPECT_EQ(&child1, parent.children()[1]);
  ASSERT_EQ(2u, parent.layer()->children().size());
  EXPECT_EQ(child2.layer(), parent.layer()->children()[0]);
  EXPECT_EQ(child1.layer(), parent.layer()->children()[1]);

  // Add 3, resulting in order [2, 1, 3], then move 2 in front of 1, resulting
  // in [1, 2, 3].
  parent.AddChild(&child3);
  parent.StackChildAbove(&child2, &child1);
  ASSERT_EQ(3u, parent.children().size());
  EXPECT_EQ(&child1, parent.children()[0]);
  EXPECT_EQ(&child2, parent.children()[1]);
  EXPECT_EQ(&child3, parent.children()[2]);
  ASSERT_EQ(3u, parent.layer()->children().size());
  EXPECT_EQ(child1.layer(), parent.layer()->children()[0]);
  EXPECT_EQ(child2.layer(), parent.layer()->children()[1]);
  EXPECT_EQ(child3.layer(), parent.layer()->children()[2]);

  // Move 1 in front of 3, resulting in [2, 3, 1].
  parent.StackChildAbove(&child1, &child3);
  ASSERT_EQ(3u, parent.children().size());
  EXPECT_EQ(&child2, parent.children()[0]);
  EXPECT_EQ(&child3, parent.children()[1]);
  EXPECT_EQ(&child1, parent.children()[2]);
  ASSERT_EQ(3u, parent.layer()->children().size());
  EXPECT_EQ(child2.layer(), parent.layer()->children()[0]);
  EXPECT_EQ(child3.layer(), parent.layer()->children()[1]);
  EXPECT_EQ(child1.layer(), parent.layer()->children()[2]);

  // Moving 1 in front of 2 should lower it, resulting in [2, 1, 3].
  parent.StackChildAbove(&child1, &child2);
  ASSERT_EQ(3u, parent.children().size());
  EXPECT_EQ(&child2, parent.children()[0]);
  EXPECT_EQ(&child1, parent.children()[1]);
  EXPECT_EQ(&child3, parent.children()[2]);
  ASSERT_EQ(3u, parent.layer()->children().size());
  EXPECT_EQ(child2.layer(), parent.layer()->children()[0]);
  EXPECT_EQ(child1.layer(), parent.layer()->children()[1]);
  EXPECT_EQ(child3.layer(), parent.layer()->children()[2]);
}

// Various capture assertions.
TEST_F(WindowTest, CaptureTests) {
  CaptureWindowDelegateImpl delegate;
  scoped_ptr<Window> window(CreateTestWindowWithDelegate(
      &delegate, 0, gfx::Rect(0, 0, 20, 20), root_window()));
  EXPECT_FALSE(window->HasCapture());

  delegate.ResetCounts();

  // Do a capture.
  window->SetCapture();
  EXPECT_TRUE(window->HasCapture());
  EXPECT_EQ(0, delegate.capture_lost_count());
  EXPECT_EQ(0, delegate.capture_changed_event_count());
  EventGenerator generator(root_window(), gfx::Point(50, 50));
  generator.PressLeftButton();
  EXPECT_EQ(1, delegate.mouse_event_count());
  generator.ReleaseLeftButton();

  EXPECT_EQ(2, delegate.mouse_event_count());
  delegate.ResetCounts();

  ui::TouchEvent touchev(
      ui::ET_TOUCH_PRESSED, gfx::Point(50, 50), 0, getTime());
  DispatchEventUsingWindowDispatcher(&touchev);
  EXPECT_EQ(1, delegate.touch_event_count());
  delegate.ResetCounts();

  window->ReleaseCapture();
  EXPECT_FALSE(window->HasCapture());
  EXPECT_EQ(1, delegate.capture_lost_count());
  EXPECT_EQ(1, delegate.capture_changed_event_count());
  EXPECT_EQ(1, delegate.mouse_event_count());
  EXPECT_EQ(0, delegate.touch_event_count());

  generator.PressLeftButton();
  EXPECT_EQ(1, delegate.mouse_event_count());

  ui::TouchEvent touchev2(
      ui::ET_TOUCH_PRESSED, gfx::Point(250, 250), 1, getTime());
  DispatchEventUsingWindowDispatcher(&touchev2);
  EXPECT_EQ(0, delegate.touch_event_count());

  // Removing the capture window from parent should reset the capture window
  // in the root window.
  window->SetCapture();
  EXPECT_EQ(window.get(), aura::client::GetCaptureWindow(root_window()));
  window->parent()->RemoveChild(window.get());
  EXPECT_FALSE(window->HasCapture());
  EXPECT_EQ(NULL, aura::client::GetCaptureWindow(root_window()));
}

TEST_F(WindowTest, TouchCaptureCancelsOtherTouches) {
  CaptureWindowDelegateImpl delegate1;
  scoped_ptr<Window> w1(CreateTestWindowWithDelegate(
      &delegate1, 0, gfx::Rect(0, 0, 50, 50), root_window()));
  CaptureWindowDelegateImpl delegate2;
  scoped_ptr<Window> w2(CreateTestWindowWithDelegate(
      &delegate2, 0, gfx::Rect(50, 50, 50, 50), root_window()));

  // Press on w1.
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), 0, getTime());
  DispatchEventUsingWindowDispatcher(&press);
  // We will get both GESTURE_BEGIN and GESTURE_TAP_DOWN.
  EXPECT_EQ(2, delegate1.gesture_event_count());
  delegate1.ResetCounts();

  // Capturing to w2 should cause the touch to be canceled.
  w2->SetCapture();
  EXPECT_EQ(1, delegate1.touch_event_count());
  EXPECT_EQ(0, delegate2.touch_event_count());
  delegate1.ResetCounts();
  delegate2.ResetCounts();

  // Events now go to w2.
  ui::TouchEvent move(ui::ET_TOUCH_MOVED, gfx::Point(10, 20), 0, getTime());
  DispatchEventUsingWindowDispatcher(&move);
  EXPECT_EQ(0, delegate1.gesture_event_count());
  EXPECT_EQ(0, delegate1.touch_event_count());
  EXPECT_EQ(0, delegate2.gesture_event_count());
  EXPECT_EQ(1, delegate2.touch_event_count());

  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(10, 20), 0, getTime());
  DispatchEventUsingWindowDispatcher(&release);
  EXPECT_EQ(0, delegate1.gesture_event_count());
  EXPECT_EQ(0, delegate2.gesture_event_count());

  // A new press is captured by w2.
  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), 0, getTime());
  DispatchEventUsingWindowDispatcher(&press2);
  EXPECT_EQ(0, delegate1.gesture_event_count());
  // We will get both GESTURE_BEGIN and GESTURE_TAP_DOWN.
  EXPECT_EQ(2, delegate2.gesture_event_count());
  delegate1.ResetCounts();
  delegate2.ResetCounts();

  // And releasing capture changes nothing.
  w2->ReleaseCapture();
  EXPECT_EQ(0, delegate1.gesture_event_count());
  EXPECT_EQ(0, delegate1.touch_event_count());
  EXPECT_EQ(0, delegate2.gesture_event_count());
  EXPECT_EQ(0, delegate2.touch_event_count());
}

TEST_F(WindowTest, TouchCaptureDoesntCancelCapturedTouches) {
  CaptureWindowDelegateImpl delegate;
  scoped_ptr<Window> window(CreateTestWindowWithDelegate(
      &delegate, 0, gfx::Rect(0, 0, 50, 50), root_window()));

  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), 0, getTime());
  DispatchEventUsingWindowDispatcher(&press);

  // We will get both GESTURE_BEGIN and GESTURE_TAP_DOWN.
  EXPECT_EQ(2, delegate.gesture_event_count());
  EXPECT_EQ(1, delegate.touch_event_count());
  delegate.ResetCounts();

  window->SetCapture();
  EXPECT_EQ(0, delegate.gesture_event_count());
  EXPECT_EQ(0, delegate.touch_event_count());
  delegate.ResetCounts();

  // On move We will get TOUCH_MOVED, GESTURE_TAP_CANCEL,
  // GESTURE_SCROLL_START and GESTURE_SCROLL_UPDATE.
  ui::TouchEvent move(ui::ET_TOUCH_MOVED, gfx::Point(10, 20), 0, getTime());
  DispatchEventUsingWindowDispatcher(&move);
  EXPECT_EQ(1, delegate.touch_event_count());
  EXPECT_EQ(3, delegate.gesture_event_count());
  delegate.ResetCounts();

  // Release capture shouldn't change anything.
  window->ReleaseCapture();
  EXPECT_EQ(0, delegate.touch_event_count());
  EXPECT_EQ(0, delegate.gesture_event_count());
  delegate.ResetCounts();

  // On move we still get TOUCH_MOVED and GESTURE_SCROLL_UPDATE.
  ui::TouchEvent move2(ui::ET_TOUCH_MOVED, gfx::Point(10, 30), 0, getTime());
  DispatchEventUsingWindowDispatcher(&move2);
  EXPECT_EQ(1, delegate.touch_event_count());
  EXPECT_EQ(1, delegate.gesture_event_count());
  delegate.ResetCounts();

  // And on release we get TOUCH_RELEASED, GESTURE_SCROLL_END, GESTURE_END
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(10, 20), 0, getTime());
  DispatchEventUsingWindowDispatcher(&release);
  EXPECT_EQ(1, delegate.touch_event_count());
  EXPECT_EQ(2, delegate.gesture_event_count());
}


// Assertions around SetCapture() and touch/gestures.
TEST_F(WindowTest, TransferCaptureTouchEvents) {
  // Touch on |w1|.
  CaptureWindowDelegateImpl d1;
  scoped_ptr<Window> w1(CreateTestWindowWithDelegate(
      &d1, 0, gfx::Rect(0, 0, 20, 20), root_window()));
  ui::TouchEvent p1(ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), 0, getTime());
  DispatchEventUsingWindowDispatcher(&p1);
  // We will get both GESTURE_BEGIN and GESTURE_TAP_DOWN.
  EXPECT_EQ(1, d1.touch_event_count());
  EXPECT_EQ(2, d1.gesture_event_count());
  d1.ResetCounts();

  // Touch on |w2| with a different id.
  CaptureWindowDelegateImpl d2;
  scoped_ptr<Window> w2(CreateTestWindowWithDelegate(
      &d2, 0, gfx::Rect(40, 0, 40, 20), root_window()));
  ui::TouchEvent p2(ui::ET_TOUCH_PRESSED, gfx::Point(41, 10), 1, getTime());
  DispatchEventUsingWindowDispatcher(&p2);
  EXPECT_EQ(0, d1.touch_event_count());
  EXPECT_EQ(0, d1.gesture_event_count());
  // We will get both GESTURE_BEGIN and GESTURE_TAP_DOWN for new target window.
  EXPECT_EQ(1, d2.touch_event_count());
  EXPECT_EQ(2, d2.gesture_event_count());
  d1.ResetCounts();
  d2.ResetCounts();

  // Set capture on |w2|, this should send a cancel (TAP_CANCEL, END) to |w1|
  // but not |w2|.
  w2->SetCapture();
  EXPECT_EQ(1, d1.touch_event_count());
  EXPECT_EQ(2, d1.gesture_event_count());
  EXPECT_EQ(0, d2.touch_event_count());
  EXPECT_EQ(0, d2.gesture_event_count());
  d1.ResetCounts();
  d2.ResetCounts();

  CaptureWindowDelegateImpl d3;
  scoped_ptr<Window> w3(CreateTestWindowWithDelegate(
      &d3, 0, gfx::Rect(0, 0, 100, 101), root_window()));
  // Set capture on w3. No new events should be received.
  // Note this difference in behavior between the first and second capture
  // is confusing and error prone.  http://crbug.com/236930
  w3->SetCapture();
  EXPECT_EQ(0, d1.touch_event_count());
  EXPECT_EQ(0, d1.gesture_event_count());
  EXPECT_EQ(0, d2.touch_event_count());
  EXPECT_EQ(0, d2.gesture_event_count());
  EXPECT_EQ(0, d3.touch_event_count());
  EXPECT_EQ(0, d3.gesture_event_count());

  // Move touch id originally associated with |w2|. Since capture was transfered
  // from 2 to 3 only |w3| should get the event.
  ui::TouchEvent m3(ui::ET_TOUCH_MOVED, gfx::Point(110, 105), 1, getTime());
  DispatchEventUsingWindowDispatcher(&m3);
  EXPECT_EQ(0, d1.touch_event_count());
  EXPECT_EQ(0, d1.gesture_event_count());
  EXPECT_EQ(0, d2.touch_event_count());
  EXPECT_EQ(0, d2.gesture_event_count());
  // |w3| gets a TOUCH_MOVE, TAP_CANCEL and two scroll related events.
  EXPECT_EQ(1, d3.touch_event_count());
  EXPECT_EQ(3, d3.gesture_event_count());
  d1.ResetCounts();
  d2.ResetCounts();
  d3.ResetCounts();

  // When we release capture, no touches are canceled.
  w3->ReleaseCapture();
  EXPECT_EQ(0, d1.touch_event_count());
  EXPECT_EQ(0, d1.gesture_event_count());
  EXPECT_EQ(0, d2.touch_event_count());
  EXPECT_EQ(0, d2.gesture_event_count());
  EXPECT_EQ(0, d3.touch_event_count());
  EXPECT_EQ(0, d3.gesture_event_count());

  // And when we move the touch again, |w3| still gets the events.
  ui::TouchEvent m4(ui::ET_TOUCH_MOVED, gfx::Point(120, 105), 1, getTime());
  DispatchEventUsingWindowDispatcher(&m4);
  EXPECT_EQ(0, d1.touch_event_count());
  EXPECT_EQ(0, d1.gesture_event_count());
  EXPECT_EQ(0, d2.touch_event_count());
  EXPECT_EQ(0, d2.gesture_event_count());
  EXPECT_EQ(1, d3.touch_event_count());
  EXPECT_EQ(1, d3.gesture_event_count());
  d1.ResetCounts();
  d2.ResetCounts();
  d3.ResetCounts();
}

// Changes capture while capture is already ongoing.
TEST_F(WindowTest, ChangeCaptureWhileMouseDown) {
  CaptureWindowDelegateImpl delegate;
  scoped_ptr<Window> window(CreateTestWindowWithDelegate(
      &delegate, 0, gfx::Rect(0, 0, 20, 20), root_window()));
  CaptureWindowDelegateImpl delegate2;
  scoped_ptr<Window> w2(CreateTestWindowWithDelegate(
      &delegate2, 0, gfx::Rect(20, 20, 20, 20), root_window()));

  // Execute the scheduled draws so that mouse events are not
  // aggregated.
  RunAllPendingInMessageLoop();

  EXPECT_FALSE(window->HasCapture());

  // Do a capture.
  delegate.ResetCounts();
  window->SetCapture();
  EXPECT_TRUE(window->HasCapture());
  EXPECT_EQ(0, delegate.capture_lost_count());
  EXPECT_EQ(0, delegate.capture_changed_event_count());
  EventGenerator generator(root_window(), gfx::Point(50, 50));
  generator.PressLeftButton();
  EXPECT_EQ(0, delegate.capture_lost_count());
  EXPECT_EQ(0, delegate.capture_changed_event_count());
  EXPECT_EQ(1, delegate.mouse_event_count());

  // Set capture to |w2|, should implicitly unset capture for |window|.
  delegate.ResetCounts();
  delegate2.ResetCounts();
  w2->SetCapture();

  generator.MoveMouseTo(gfx::Point(40, 40), 2);
  EXPECT_EQ(1, delegate.capture_lost_count());
  EXPECT_EQ(1, delegate.capture_changed_event_count());
  EXPECT_EQ(1, delegate.mouse_event_count());
  EXPECT_EQ(2, delegate2.mouse_event_count());
}

// Verifies capture is reset when a window is destroyed.
TEST_F(WindowTest, ReleaseCaptureOnDestroy) {
  CaptureWindowDelegateImpl delegate;
  scoped_ptr<Window> window(CreateTestWindowWithDelegate(
      &delegate, 0, gfx::Rect(0, 0, 20, 20), root_window()));
  EXPECT_FALSE(window->HasCapture());

  // Do a capture.
  window->SetCapture();
  EXPECT_TRUE(window->HasCapture());

  // Destroy the window.
  window.reset();

  // Make sure the root window doesn't reference the window anymore.
  EXPECT_EQ(NULL, host()->dispatcher()->mouse_pressed_handler());
  EXPECT_EQ(NULL, aura::client::GetCaptureWindow(root_window()));
}

TEST_F(WindowTest, GetBoundsInRootWindow) {
  scoped_ptr<Window> viewport(CreateTestWindowWithBounds(
      gfx::Rect(0, 0, 300, 300), root_window()));
  scoped_ptr<Window> child(CreateTestWindowWithBounds(
      gfx::Rect(0, 0, 100, 100), viewport.get()));
  // Sanity check.
  EXPECT_EQ("0,0 100x100", child->GetBoundsInRootWindow().ToString());

  // The |child| window's screen bounds should move along with the |viewport|.
  viewport->SetBounds(gfx::Rect(-100, -100, 300, 300));
  EXPECT_EQ("-100,-100 100x100", child->GetBoundsInRootWindow().ToString());

  // The |child| window is moved to the 0,0 in screen coordinates.
  // |GetBoundsInRootWindow()| should return 0,0.
  child->SetBounds(gfx::Rect(100, 100, 100, 100));
  EXPECT_EQ("0,0 100x100", child->GetBoundsInRootWindow().ToString());
}

class MouseEnterExitWindowDelegate : public TestWindowDelegate {
 public:
  MouseEnterExitWindowDelegate() : entered_(false), exited_(false) {}

  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    switch (event->type()) {
      case ui::ET_MOUSE_ENTERED:
        EXPECT_TRUE(event->flags() & ui::EF_IS_SYNTHESIZED);
        entered_ = true;
        break;
      case ui::ET_MOUSE_EXITED:
        EXPECT_TRUE(event->flags() & ui::EF_IS_SYNTHESIZED);
        exited_ = true;
        break;
      default:
        break;
    }
  }

  bool entered() const { return entered_; }
  bool exited() const { return exited_; }

  // Clear the entered / exited states.
  void ResetExpectations() {
    entered_ = false;
    exited_ = false;
  }

 private:
  bool entered_;
  bool exited_;

  DISALLOW_COPY_AND_ASSIGN(MouseEnterExitWindowDelegate);
};


// Verifies that the WindowDelegate receives MouseExit and MouseEnter events for
// mouse transitions from window to window.
TEST_F(WindowTest, MouseEnterExit) {
  MouseEnterExitWindowDelegate d1;
  scoped_ptr<Window> w1(
      CreateTestWindowWithDelegate(&d1, 1, gfx::Rect(10, 10, 50, 50),
                                   root_window()));
  MouseEnterExitWindowDelegate d2;
  scoped_ptr<Window> w2(
      CreateTestWindowWithDelegate(&d2, 2, gfx::Rect(70, 70, 50, 50),
                                   root_window()));

  test::EventGenerator generator(root_window());
  generator.MoveMouseToCenterOf(w1.get());
  EXPECT_TRUE(d1.entered());
  EXPECT_FALSE(d1.exited());
  EXPECT_FALSE(d2.entered());
  EXPECT_FALSE(d2.exited());

  generator.MoveMouseToCenterOf(w2.get());
  EXPECT_TRUE(d1.entered());
  EXPECT_TRUE(d1.exited());
  EXPECT_TRUE(d2.entered());
  EXPECT_FALSE(d2.exited());
}

// Verifies that the WindowDelegate receives MouseExit from ET_MOUSE_EXITED.
TEST_F(WindowTest, WindowTreeHostExit) {
  MouseEnterExitWindowDelegate d1;
  scoped_ptr<Window> w1(
      CreateTestWindowWithDelegate(&d1, 1, gfx::Rect(10, 10, 50, 50),
                                   root_window()));

  test::EventGenerator generator(root_window());
  generator.MoveMouseToCenterOf(w1.get());
  EXPECT_TRUE(d1.entered());
  EXPECT_FALSE(d1.exited());
  d1.ResetExpectations();

  ui::MouseEvent exit_event(
      ui::ET_MOUSE_EXITED, gfx::Point(), gfx::Point(), 0, 0);
  DispatchEventUsingWindowDispatcher(&exit_event);
  EXPECT_FALSE(d1.entered());
  EXPECT_TRUE(d1.exited());
}

// Verifies that the WindowDelegate receives MouseExit and MouseEnter events for
// mouse transitions from window to window, even if the entered window sets
// and releases capture.
TEST_F(WindowTest, MouseEnterExitWithClick) {
  MouseEnterExitWindowDelegate d1;
  scoped_ptr<Window> w1(
      CreateTestWindowWithDelegate(&d1, 1, gfx::Rect(10, 10, 50, 50),
                                   root_window()));
  MouseEnterExitWindowDelegate d2;
  scoped_ptr<Window> w2(
      CreateTestWindowWithDelegate(&d2, 2, gfx::Rect(70, 70, 50, 50),
                                   root_window()));

  test::EventGenerator generator(root_window());
  generator.MoveMouseToCenterOf(w1.get());
  EXPECT_TRUE(d1.entered());
  EXPECT_FALSE(d1.exited());
  EXPECT_FALSE(d2.entered());
  EXPECT_FALSE(d2.exited());

  // Emmulate what Views does on a click by grabbing and releasing capture.
  generator.PressLeftButton();
  w1->SetCapture();
  w1->ReleaseCapture();
  generator.ReleaseLeftButton();

  generator.MoveMouseToCenterOf(w2.get());
  EXPECT_TRUE(d1.entered());
  EXPECT_TRUE(d1.exited());
  EXPECT_TRUE(d2.entered());
  EXPECT_FALSE(d2.exited());
}

TEST_F(WindowTest, MouseEnterExitWhenDeleteWithCapture) {
  MouseEnterExitWindowDelegate delegate;
  scoped_ptr<Window> window(
      CreateTestWindowWithDelegate(&delegate, 1, gfx::Rect(10, 10, 50, 50),
                                   root_window()));

  test::EventGenerator generator(root_window());
  generator.MoveMouseToCenterOf(window.get());
  EXPECT_TRUE(delegate.entered());
  EXPECT_FALSE(delegate.exited());

  // Emmulate what Views does on a click by grabbing and releasing capture.
  generator.PressLeftButton();
  window->SetCapture();

  delegate.ResetExpectations();
  generator.MoveMouseTo(0, 0);
  EXPECT_FALSE(delegate.entered());
  EXPECT_FALSE(delegate.exited());

  delegate.ResetExpectations();
  window.reset();
  EXPECT_FALSE(delegate.entered());
  EXPECT_FALSE(delegate.exited());
}

// Verifies that enter / exits are sent if windows appear and are deleted
// under the current mouse position..
TEST_F(WindowTest, MouseEnterExitWithDelete) {
  MouseEnterExitWindowDelegate d1;
  scoped_ptr<Window> w1(
      CreateTestWindowWithDelegate(&d1, 1, gfx::Rect(10, 10, 50, 50),
                                   root_window()));

  test::EventGenerator generator(root_window());
  generator.MoveMouseToCenterOf(w1.get());
  EXPECT_TRUE(d1.entered());
  EXPECT_FALSE(d1.exited());
  d1.ResetExpectations();

  MouseEnterExitWindowDelegate d2;
  {
    scoped_ptr<Window> w2(
        CreateTestWindowWithDelegate(&d2, 2, gfx::Rect(10, 10, 50, 50),
                                     root_window()));
    // Enters / exits can be sent asynchronously.
    RunAllPendingInMessageLoop();
    EXPECT_FALSE(d1.entered());
    EXPECT_TRUE(d1.exited());
    EXPECT_TRUE(d2.entered());
    EXPECT_FALSE(d2.exited());
    d1.ResetExpectations();
    d2.ResetExpectations();
  }
  // Enters / exits can be sent asynchronously.
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(d2.exited());
  EXPECT_TRUE(d1.entered());
}

// Verifies that enter / exits are sent if windows appear and are hidden
// under the current mouse position..
TEST_F(WindowTest, MouseEnterExitWithHide) {
  MouseEnterExitWindowDelegate d1;
  scoped_ptr<Window> w1(
      CreateTestWindowWithDelegate(&d1, 1, gfx::Rect(10, 10, 50, 50),
                                   root_window()));

  test::EventGenerator generator(root_window());
  generator.MoveMouseToCenterOf(w1.get());
  EXPECT_TRUE(d1.entered());
  EXPECT_FALSE(d1.exited());

  MouseEnterExitWindowDelegate d2;
  scoped_ptr<Window> w2(
      CreateTestWindowWithDelegate(&d2, 2, gfx::Rect(10, 10, 50, 50),
                                   root_window()));
  // Enters / exits can be send asynchronously.
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(d1.entered());
  EXPECT_TRUE(d1.exited());
  EXPECT_TRUE(d2.entered());
  EXPECT_FALSE(d2.exited());

  d1.ResetExpectations();
  w2->Hide();
  // Enters / exits can be send asynchronously.
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(d2.exited());
  EXPECT_TRUE(d1.entered());
}

TEST_F(WindowTest, MouseEnterExitWithParentHide) {
  MouseEnterExitWindowDelegate d1;
  scoped_ptr<Window> w1(
      CreateTestWindowWithDelegate(&d1, 1, gfx::Rect(10, 10, 50, 50),
                                   root_window()));
  MouseEnterExitWindowDelegate d2;
  Window* w2 = CreateTestWindowWithDelegate(&d2, 2, gfx::Rect(10, 10, 50, 50),
                                            w1.get());
  test::EventGenerator generator(root_window());
  generator.MoveMouseToCenterOf(w2);
  // Enters / exits can be send asynchronously.
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(d2.entered());
  EXPECT_FALSE(d2.exited());

  d2.ResetExpectations();
  w1->Hide();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(d2.entered());
  EXPECT_TRUE(d2.exited());

  w1.reset();
}

TEST_F(WindowTest, MouseEnterExitWithParentDelete) {
  MouseEnterExitWindowDelegate d1;
  scoped_ptr<Window> w1(
      CreateTestWindowWithDelegate(&d1, 1, gfx::Rect(10, 10, 50, 50),
                                   root_window()));
  MouseEnterExitWindowDelegate d2;
  Window* w2 = CreateTestWindowWithDelegate(&d2, 2, gfx::Rect(10, 10, 50, 50),
                                            w1.get());
  test::EventGenerator generator(root_window());
  generator.MoveMouseToCenterOf(w2);

  // Enters / exits can be send asynchronously.
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(d2.entered());
  EXPECT_FALSE(d2.exited());

  d2.ResetExpectations();
  w1.reset();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(d2.entered());
  EXPECT_TRUE(d2.exited());
}

// Creates a window with a delegate (w111) that can handle events at a lower
// z-index than a window without a delegate (w12). w12 is sized to fill the
// entire bounds of the container. This test verifies that
// GetEventHandlerForPoint() skips w12 even though its bounds contain the event,
// because it has no children that can handle the event and it has no delegate
// allowing it to handle the event itself.
TEST_F(WindowTest, GetEventHandlerForPoint_NoDelegate) {
  TestWindowDelegate d111;
  scoped_ptr<Window> w1(CreateTestWindowWithDelegate(NULL, 1,
      gfx::Rect(0, 0, 500, 500), root_window()));
  scoped_ptr<Window> w11(CreateTestWindowWithDelegate(NULL, 11,
      gfx::Rect(0, 0, 500, 500), w1.get()));
  scoped_ptr<Window> w111(CreateTestWindowWithDelegate(&d111, 111,
      gfx::Rect(50, 50, 450, 450), w11.get()));
  scoped_ptr<Window> w12(CreateTestWindowWithDelegate(NULL, 12,
      gfx::Rect(0, 0, 500, 500), w1.get()));

  gfx::Point target_point = w111->bounds().CenterPoint();
  EXPECT_EQ(w111.get(), w1->GetEventHandlerForPoint(target_point));
}

class VisibilityWindowDelegate : public TestWindowDelegate {
 public:
  VisibilityWindowDelegate()
      : shown_(0),
        hidden_(0) {
  }

  int shown() const { return shown_; }
  int hidden() const { return hidden_; }
  void Clear() {
    shown_ = 0;
    hidden_ = 0;
  }

  virtual void OnWindowTargetVisibilityChanged(bool visible) OVERRIDE {
    if (visible)
      shown_++;
    else
      hidden_++;
  }

 private:
  int shown_;
  int hidden_;

  DISALLOW_COPY_AND_ASSIGN(VisibilityWindowDelegate);
};

// Verifies show/hide propagate correctly to children and the layer.
TEST_F(WindowTest, Visibility) {
  VisibilityWindowDelegate d;
  VisibilityWindowDelegate d2;
  scoped_ptr<Window> w1(CreateTestWindowWithDelegate(&d, 1, gfx::Rect(),
                                                     root_window()));
  scoped_ptr<Window> w2(
      CreateTestWindowWithDelegate(&d2, 2, gfx::Rect(),  w1.get()));
  scoped_ptr<Window> w3(CreateTestWindowWithId(3, w2.get()));

  // Create shows all the windows.
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_TRUE(w3->IsVisible());
  EXPECT_EQ(1, d.shown());

  d.Clear();
  w1->Hide();
  EXPECT_FALSE(w1->IsVisible());
  EXPECT_FALSE(w2->IsVisible());
  EXPECT_FALSE(w3->IsVisible());
  EXPECT_EQ(1, d.hidden());
  EXPECT_EQ(0, d.shown());

  w2->Show();
  EXPECT_FALSE(w1->IsVisible());
  EXPECT_FALSE(w2->IsVisible());
  EXPECT_FALSE(w3->IsVisible());

  w3->Hide();
  EXPECT_FALSE(w1->IsVisible());
  EXPECT_FALSE(w2->IsVisible());
  EXPECT_FALSE(w3->IsVisible());

  d.Clear();
  w1->Show();
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_FALSE(w3->IsVisible());
  EXPECT_EQ(0, d.hidden());
  EXPECT_EQ(1, d.shown());

  w3->Show();
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_TRUE(w3->IsVisible());

  // Verify that if an ancestor isn't visible and we change the visibility of a
  // child window that OnChildWindowVisibilityChanged() is still invoked.
  w1->Hide();
  d2.Clear();
  w2->Hide();
  EXPECT_EQ(1, d2.hidden());
  EXPECT_EQ(0, d2.shown());
  d2.Clear();
  w2->Show();
  EXPECT_EQ(0, d2.hidden());
  EXPECT_EQ(1, d2.shown());
}

TEST_F(WindowTest, IgnoreEventsTest) {
  TestWindowDelegate d11;
  TestWindowDelegate d12;
  TestWindowDelegate d111;
  TestWindowDelegate d121;
  scoped_ptr<Window> w1(CreateTestWindowWithDelegate(NULL, 1,
      gfx::Rect(0, 0, 500, 500), root_window()));
  scoped_ptr<Window> w11(CreateTestWindowWithDelegate(&d11, 11,
      gfx::Rect(0, 0, 500, 500), w1.get()));
  scoped_ptr<Window> w111(CreateTestWindowWithDelegate(&d111, 111,
      gfx::Rect(50, 50, 450, 450), w11.get()));
  scoped_ptr<Window> w12(CreateTestWindowWithDelegate(&d12, 12,
      gfx::Rect(0, 0, 500, 500), w1.get()));
  scoped_ptr<Window> w121(CreateTestWindowWithDelegate(&d121, 121,
      gfx::Rect(150, 150, 50, 50), w12.get()));

  EXPECT_EQ(w12.get(), w1->GetEventHandlerForPoint(gfx::Point(10, 10)));
  w12->set_ignore_events(true);
  EXPECT_EQ(w11.get(), w1->GetEventHandlerForPoint(gfx::Point(10, 10)));
  w12->set_ignore_events(false);

  EXPECT_EQ(w121.get(), w1->GetEventHandlerForPoint(gfx::Point(160, 160)));
  w121->set_ignore_events(true);
  EXPECT_EQ(w12.get(), w1->GetEventHandlerForPoint(gfx::Point(160, 160)));
  w12->set_ignore_events(true);
  EXPECT_EQ(w111.get(), w1->GetEventHandlerForPoint(gfx::Point(160, 160)));
  w111->set_ignore_events(true);
  EXPECT_EQ(w11.get(), w1->GetEventHandlerForPoint(gfx::Point(160, 160)));
}

// Tests transformation on the root window.
TEST_F(WindowTest, Transform) {
  gfx::Size size = host()->GetBounds().size();
  EXPECT_EQ(gfx::Rect(size),
            gfx::Screen::GetScreenFor(root_window())->GetDisplayNearestPoint(
                gfx::Point()).bounds());

  // Rotate it clock-wise 90 degrees.
  gfx::Transform transform;
  transform.Translate(size.height(), 0);
  transform.Rotate(90.0);
  host()->SetRootTransform(transform);

  // The size should be the transformed size.
  gfx::Size transformed_size(size.height(), size.width());
  EXPECT_EQ(transformed_size.ToString(),
            root_window()->bounds().size().ToString());
  EXPECT_EQ(
      gfx::Rect(transformed_size).ToString(),
      gfx::Screen::GetScreenFor(root_window())->GetDisplayNearestPoint(
          gfx::Point()).bounds().ToString());

  // Host size shouldn't change.
  EXPECT_EQ(size.ToString(), host()->GetBounds().size().ToString());
}

TEST_F(WindowTest, TransformGesture) {
  gfx::Size size = host()->GetBounds().size();

  scoped_ptr<GestureTrackPositionDelegate> delegate(
      new GestureTrackPositionDelegate);
  scoped_ptr<Window> window(CreateTestWindowWithDelegate(delegate.get(), -1234,
      gfx::Rect(0, 0, 20, 20), root_window()));

  // Rotate the root-window clock-wise 90 degrees.
  gfx::Transform transform;
  transform.Translate(size.height(), 0.0);
  transform.Rotate(90.0);
  host()->SetRootTransform(transform);

  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(size.height() - 10, 10), 0, getTime());
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_EQ(gfx::Point(10, 10).ToString(), delegate->position().ToString());
}

namespace {
DEFINE_WINDOW_PROPERTY_KEY(int, kIntKey, -2);
DEFINE_WINDOW_PROPERTY_KEY(const char*, kStringKey, "squeamish");
}

TEST_F(WindowTest, Property) {
  scoped_ptr<Window> w(CreateTestWindowWithId(0, root_window()));

  static const char native_prop_key[] = "fnord";

  // Non-existent properties should return the default values.
  EXPECT_EQ(-2, w->GetProperty(kIntKey));
  EXPECT_EQ(std::string("squeamish"), w->GetProperty(kStringKey));
  EXPECT_EQ(NULL, w->GetNativeWindowProperty(native_prop_key));

  // A set property value should be returned again (even if it's the default
  // value).
  w->SetProperty(kIntKey, INT_MAX);
  EXPECT_EQ(INT_MAX, w->GetProperty(kIntKey));
  w->SetProperty(kIntKey, -2);
  EXPECT_EQ(-2, w->GetProperty(kIntKey));
  w->SetProperty(kIntKey, INT_MIN);
  EXPECT_EQ(INT_MIN, w->GetProperty(kIntKey));

  w->SetProperty(kStringKey, static_cast<const char*>(NULL));
  EXPECT_EQ(NULL, w->GetProperty(kStringKey));
  w->SetProperty(kStringKey, "squeamish");
  EXPECT_EQ(std::string("squeamish"), w->GetProperty(kStringKey));
  w->SetProperty(kStringKey, "ossifrage");
  EXPECT_EQ(std::string("ossifrage"), w->GetProperty(kStringKey));

  w->SetNativeWindowProperty(native_prop_key, &*w);
  EXPECT_EQ(&*w, w->GetNativeWindowProperty(native_prop_key));
  w->SetNativeWindowProperty(native_prop_key, NULL);
  EXPECT_EQ(NULL, w->GetNativeWindowProperty(native_prop_key));

  // ClearProperty should restore the default value.
  w->ClearProperty(kIntKey);
  EXPECT_EQ(-2, w->GetProperty(kIntKey));
  w->ClearProperty(kStringKey);
  EXPECT_EQ(std::string("squeamish"), w->GetProperty(kStringKey));
}

namespace {

class TestProperty {
 public:
  TestProperty() {}
  virtual ~TestProperty() {
    last_deleted_ = this;
  }
  static TestProperty* last_deleted() { return last_deleted_; }

 private:
  static TestProperty* last_deleted_;
  DISALLOW_COPY_AND_ASSIGN(TestProperty);
};

TestProperty* TestProperty::last_deleted_ = NULL;

DEFINE_OWNED_WINDOW_PROPERTY_KEY(TestProperty, kOwnedKey, NULL);

}  // namespace

TEST_F(WindowTest, OwnedProperty) {
  scoped_ptr<Window> w(CreateTestWindowWithId(0, root_window()));
  EXPECT_EQ(NULL, w->GetProperty(kOwnedKey));
  TestProperty* p1 = new TestProperty();
  w->SetProperty(kOwnedKey, p1);
  EXPECT_EQ(p1, w->GetProperty(kOwnedKey));
  EXPECT_EQ(NULL, TestProperty::last_deleted());

  TestProperty* p2 = new TestProperty();
  w->SetProperty(kOwnedKey, p2);
  EXPECT_EQ(p2, w->GetProperty(kOwnedKey));
  EXPECT_EQ(p1, TestProperty::last_deleted());

  w->ClearProperty(kOwnedKey);
  EXPECT_EQ(NULL, w->GetProperty(kOwnedKey));
  EXPECT_EQ(p2, TestProperty::last_deleted());

  TestProperty* p3 = new TestProperty();
  w->SetProperty(kOwnedKey, p3);
  EXPECT_EQ(p3, w->GetProperty(kOwnedKey));
  EXPECT_EQ(p2, TestProperty::last_deleted());
  w.reset();
  EXPECT_EQ(p3, TestProperty::last_deleted());
}

TEST_F(WindowTest, SetBoundsInternalShouldCheckTargetBounds) {
  // We cannot short-circuit animations in this test.
  ui::ScopedAnimationDurationScaleMode normal_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  scoped_ptr<Window> w1(
      CreateTestWindowWithBounds(gfx::Rect(0, 0, 100, 100), root_window()));

  EXPECT_FALSE(!w1->layer());
  w1->layer()->GetAnimator()->set_disable_timer_for_test(true);
  ui::LayerAnimator* animator = w1->layer()->GetAnimator();

  EXPECT_EQ("0,0 100x100", w1->bounds().ToString());
  EXPECT_EQ("0,0 100x100", w1->layer()->GetTargetBounds().ToString());

  // Animate to a different position.
  {
    ui::ScopedLayerAnimationSettings settings(w1->layer()->GetAnimator());
    w1->SetBounds(gfx::Rect(100, 100, 100, 100));
  }

  EXPECT_EQ("0,0 100x100", w1->bounds().ToString());
  EXPECT_EQ("100,100 100x100", w1->layer()->GetTargetBounds().ToString());

  // Animate back to the first position. The animation hasn't started yet, so
  // the current bounds are still (0, 0, 100, 100), but the target bounds are
  // (100, 100, 100, 100). If we step the animator ahead, we should find that
  // we're at (0, 0, 100, 100). That is, the second animation should be applied.
  {
    ui::ScopedLayerAnimationSettings settings(w1->layer()->GetAnimator());
    w1->SetBounds(gfx::Rect(0, 0, 100, 100));
  }

  EXPECT_EQ("0,0 100x100", w1->bounds().ToString());
  EXPECT_EQ("0,0 100x100", w1->layer()->GetTargetBounds().ToString());

  // Confirm that the target bounds are reached.
  base::TimeTicks start_time =
      w1->layer()->GetAnimator()->last_step_time();

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_EQ("0,0 100x100", w1->bounds().ToString());
}


typedef std::pair<const void*, intptr_t> PropertyChangeInfo;

class WindowObserverTest : public WindowTest,
                           public WindowObserver {
 public:
  struct VisibilityInfo {
    bool window_visible;
    bool visible_param;
  };

  WindowObserverTest()
      : added_count_(0),
        removed_count_(0),
        destroyed_count_(0),
        old_property_value_(-3) {
  }

  virtual ~WindowObserverTest() {}

  const VisibilityInfo* GetVisibilityInfo() const {
    return visibility_info_.get();
  }

  void ResetVisibilityInfo() {
    visibility_info_.reset();
  }

  // Returns a description of the WindowObserver methods that have been invoked.
  std::string WindowObserverCountStateAndClear() {
    std::string result(
        base::StringPrintf("added=%d removed=%d",
        added_count_, removed_count_));
    added_count_ = removed_count_ = 0;
    return result;
  }

  int DestroyedCountAndClear() {
    int result = destroyed_count_;
    destroyed_count_ = 0;
    return result;
  }

  // Return a tuple of the arguments passed in OnPropertyChanged callback.
  PropertyChangeInfo PropertyChangeInfoAndClear() {
    PropertyChangeInfo result(property_key_, old_property_value_);
    property_key_ = NULL;
    old_property_value_ = -3;
    return result;
  }

 private:
  virtual void OnWindowAdded(Window* new_window) OVERRIDE {
    added_count_++;
  }

  virtual void OnWillRemoveWindow(Window* window) OVERRIDE {
    removed_count_++;
  }

  virtual void OnWindowVisibilityChanged(Window* window,
                                         bool visible) OVERRIDE {
    visibility_info_.reset(new VisibilityInfo);
    visibility_info_->window_visible = window->IsVisible();
    visibility_info_->visible_param = visible;
  }

  virtual void OnWindowDestroyed(Window* window) OVERRIDE {
    EXPECT_FALSE(window->parent());
    destroyed_count_++;
  }

  virtual void OnWindowPropertyChanged(Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE {
    property_key_ = key;
    old_property_value_ = old;
  }

  int added_count_;
  int removed_count_;
  int destroyed_count_;
  scoped_ptr<VisibilityInfo> visibility_info_;
  const void* property_key_;
  intptr_t old_property_value_;

  DISALLOW_COPY_AND_ASSIGN(WindowObserverTest);
};

// Various assertions for WindowObserver.
TEST_F(WindowObserverTest, WindowObserver) {
  scoped_ptr<Window> w1(CreateTestWindowWithId(1, root_window()));
  w1->AddObserver(this);

  // Create a new window as a child of w1, our observer should be notified.
  scoped_ptr<Window> w2(CreateTestWindowWithId(2, w1.get()));
  EXPECT_EQ("added=1 removed=0", WindowObserverCountStateAndClear());

  // Delete w2, which should result in the remove notification.
  w2.reset();
  EXPECT_EQ("added=0 removed=1", WindowObserverCountStateAndClear());

  // Create a window that isn't parented to w1, we shouldn't get any
  // notification.
  scoped_ptr<Window> w3(CreateTestWindowWithId(3, root_window()));
  EXPECT_EQ("added=0 removed=0", WindowObserverCountStateAndClear());

  // Similarly destroying w3 shouldn't notify us either.
  w3.reset();
  EXPECT_EQ("added=0 removed=0", WindowObserverCountStateAndClear());
  w1->RemoveObserver(this);
}

// Test if OnWindowVisibilityChagned is invoked with expected
// parameters.
TEST_F(WindowObserverTest, WindowVisibility) {
  scoped_ptr<Window> w1(CreateTestWindowWithId(1, root_window()));
  scoped_ptr<Window> w2(CreateTestWindowWithId(1, w1.get()));
  w2->AddObserver(this);

  // Hide should make the window invisible and the passed visible
  // parameter is false.
  w2->Hide();
  EXPECT_FALSE(!GetVisibilityInfo());
  EXPECT_FALSE(!GetVisibilityInfo());
  if (!GetVisibilityInfo())
    return;
  EXPECT_FALSE(GetVisibilityInfo()->window_visible);
  EXPECT_FALSE(GetVisibilityInfo()->visible_param);

  // If parent isn't visible, showing window won't make the window visible, but
  // passed visible value must be true.
  w1->Hide();
  ResetVisibilityInfo();
  EXPECT_TRUE(!GetVisibilityInfo());
  w2->Show();
  EXPECT_FALSE(!GetVisibilityInfo());
  if (!GetVisibilityInfo())
    return;
  EXPECT_FALSE(GetVisibilityInfo()->window_visible);
  EXPECT_TRUE(GetVisibilityInfo()->visible_param);

  // If parent is visible, showing window will make the window
  // visible and the passed visible value is true.
  w1->Show();
  w2->Hide();
  ResetVisibilityInfo();
  w2->Show();
  EXPECT_FALSE(!GetVisibilityInfo());
  if (!GetVisibilityInfo())
    return;
  EXPECT_TRUE(GetVisibilityInfo()->window_visible);
  EXPECT_TRUE(GetVisibilityInfo()->visible_param);
}

// Test if OnWindowDestroyed is invoked as expected.
TEST_F(WindowObserverTest, WindowDestroyed) {
  // Delete a window should fire a destroyed notification.
  scoped_ptr<Window> w1(CreateTestWindowWithId(1, root_window()));
  w1->AddObserver(this);
  w1.reset();
  EXPECT_EQ(1, DestroyedCountAndClear());

  // Observe on child and delete parent window should fire a notification.
  scoped_ptr<Window> parent(CreateTestWindowWithId(1, root_window()));
  Window* child = CreateTestWindowWithId(1, parent.get());  // owned by parent
  child->AddObserver(this);
  parent.reset();
  EXPECT_EQ(1, DestroyedCountAndClear());
}

TEST_F(WindowObserverTest, PropertyChanged) {
  // Setting property should fire a property change notification.
  scoped_ptr<Window> w1(CreateTestWindowWithId(1, root_window()));
  w1->AddObserver(this);

  static const WindowProperty<int> prop = {-2};
  static const char native_prop_key[] = "fnord";

  w1->SetProperty(&prop, 1);
  EXPECT_EQ(PropertyChangeInfo(&prop, -2), PropertyChangeInfoAndClear());
  w1->SetProperty(&prop, -2);
  EXPECT_EQ(PropertyChangeInfo(&prop, 1), PropertyChangeInfoAndClear());
  w1->SetProperty(&prop, 3);
  EXPECT_EQ(PropertyChangeInfo(&prop, -2), PropertyChangeInfoAndClear());
  w1->ClearProperty(&prop);
  EXPECT_EQ(PropertyChangeInfo(&prop, 3), PropertyChangeInfoAndClear());

  w1->SetNativeWindowProperty(native_prop_key, &*w1);
  EXPECT_EQ(PropertyChangeInfo(native_prop_key, 0),
            PropertyChangeInfoAndClear());
  w1->SetNativeWindowProperty(native_prop_key, NULL);
  EXPECT_EQ(PropertyChangeInfo(native_prop_key,
                               reinterpret_cast<intptr_t>(&*w1)),
            PropertyChangeInfoAndClear());

  // Sanity check to see if |PropertyChangeInfoAndClear| really clears.
  EXPECT_EQ(PropertyChangeInfo(
      reinterpret_cast<const void*>(NULL), -3), PropertyChangeInfoAndClear());
}

TEST_F(WindowTest, AcquireLayer) {
  scoped_ptr<Window> window1(CreateTestWindowWithId(1, root_window()));
  scoped_ptr<Window> window2(CreateTestWindowWithId(2, root_window()));
  ui::Layer* parent = window1->parent()->layer();
  EXPECT_EQ(2U, parent->children().size());

  WindowTestApi window1_test_api(window1.get());
  WindowTestApi window2_test_api(window2.get());

  EXPECT_TRUE(window1_test_api.OwnsLayer());
  EXPECT_TRUE(window2_test_api.OwnsLayer());

  // After acquisition, window1 should not own its layer, but it should still
  // be available to the window.
  scoped_ptr<ui::Layer> window1_layer(window1->AcquireLayer());
  EXPECT_FALSE(window1_test_api.OwnsLayer());
  EXPECT_TRUE(window1_layer.get() == window1->layer());

  // The acquired layer's owner should be set NULL and re-acquring
  // should return NULL.
  EXPECT_FALSE(window1_layer->owner());
  scoped_ptr<ui::Layer> window1_layer_reacquired(window1->AcquireLayer());
  EXPECT_FALSE(window1_layer_reacquired.get());

  // Upon destruction, window1's layer should still be valid, and in the layer
  // hierarchy, but window2's should be gone, and no longer in the hierarchy.
  window1.reset();
  window2.reset();

  // This should be set by the window's destructor.
  EXPECT_TRUE(window1_layer->delegate() == NULL);
  EXPECT_EQ(1U, parent->children().size());
}

// Make sure that properties which should persist from the old layer to the new
// layer actually do.
TEST_F(WindowTest, RecreateLayer) {
  // Set properties to non default values.
  Window w(new ColorTestWindowDelegate(SK_ColorWHITE));
  w.set_id(1);
  w.Init(aura::WINDOW_LAYER_SOLID_COLOR);
  w.SetBounds(gfx::Rect(0, 0, 100, 100));

  ui::Layer* layer = w.layer();
  layer->SetVisible(false);
  layer->SetMasksToBounds(true);

  ui::Layer child_layer;
  layer->Add(&child_layer);

  scoped_ptr<ui::Layer> old_layer(w.RecreateLayer());
  layer = w.layer();
  EXPECT_EQ(ui::LAYER_SOLID_COLOR, layer->type());
  EXPECT_FALSE(layer->visible());
  EXPECT_EQ(1u, layer->children().size());
  EXPECT_TRUE(layer->GetMasksToBounds());
  EXPECT_EQ("0,0 100x100", w.bounds().ToString());
  EXPECT_EQ("0,0 100x100", layer->bounds().ToString());
}

// Verify that RecreateLayer() stacks the old layer above the newly creatd
// layer.
TEST_F(WindowTest, RecreateLayerZOrder) {
  scoped_ptr<Window> w(
      CreateTestWindow(SK_ColorWHITE, 1, gfx::Rect(0, 0, 100, 100),
                       root_window()));
  scoped_ptr<ui::Layer> old_layer(w->RecreateLayer());

  const std::vector<ui::Layer*>& child_layers =
      root_window()->layer()->children();
  ASSERT_EQ(2u, child_layers.size());
  EXPECT_EQ(w->layer(), child_layers[0]);
  EXPECT_EQ(old_layer.get(), child_layers[1]);
}

// Ensure that acquiring a layer then recreating a layer does not crash
// and that RecreateLayer returns null.
TEST_F(WindowTest, AcquireThenRecreateLayer) {
  scoped_ptr<Window> w(
      CreateTestWindow(SK_ColorWHITE, 1, gfx::Rect(0, 0, 100, 100),
                       root_window()));
  scoped_ptr<ui::Layer> acquired_layer(w->AcquireLayer());
  scoped_ptr<ui::Layer> doubly_acquired_layer(w->RecreateLayer());
  EXPECT_EQ(NULL, doubly_acquired_layer.get());

  // Destroy window before layer gets destroyed.
  w.reset();
}

TEST_F(WindowTest, StackWindowAtBottomBelowWindowWhoseLayerHasNoDelegate) {
  scoped_ptr<Window> window1(CreateTestWindowWithId(1, root_window()));
  window1->layer()->set_name("1");
  scoped_ptr<Window> window2(CreateTestWindowWithId(2, root_window()));
  window2->layer()->set_name("2");
  scoped_ptr<Window> window3(CreateTestWindowWithId(3, root_window()));
  window3->layer()->set_name("3");

  EXPECT_EQ("1 2 3", ChildWindowIDsAsString(root_window()));
  EXPECT_EQ("1 2 3",
            ui::test::ChildLayerNamesAsString(*root_window()->layer()));
  window1->layer()->set_delegate(NULL);
  root_window()->StackChildAtBottom(window3.get());

  // Window 3 should have moved to the bottom.
  EXPECT_EQ("3 1 2", ChildWindowIDsAsString(root_window()));
  EXPECT_EQ("3 1 2",
            ui::test::ChildLayerNamesAsString(*root_window()->layer()));
}

class TestVisibilityClient : public client::VisibilityClient {
 public:
  explicit TestVisibilityClient(Window* root_window)
      : ignore_visibility_changes_(false) {
    client::SetVisibilityClient(root_window, this);
  }
  virtual ~TestVisibilityClient() {
  }

  void set_ignore_visibility_changes(bool ignore_visibility_changes) {
    ignore_visibility_changes_ = ignore_visibility_changes;
  }

  // Overridden from client::VisibilityClient:
  virtual void UpdateLayerVisibility(aura::Window* window,
                                     bool visible) OVERRIDE {
    if (!ignore_visibility_changes_)
      window->layer()->SetVisible(visible);
  }

 private:
  bool ignore_visibility_changes_;
  DISALLOW_COPY_AND_ASSIGN(TestVisibilityClient);
};

TEST_F(WindowTest, VisibilityClientIsVisible) {
  TestVisibilityClient client(root_window());

  scoped_ptr<Window> window(CreateTestWindowWithId(1, root_window()));
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->layer()->visible());

  window->Hide();
  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(window->layer()->visible());
  window->Show();

  client.set_ignore_visibility_changes(true);
  window->Hide();
  EXPECT_FALSE(window->IsVisible());
  EXPECT_TRUE(window->layer()->visible());
}

// Tests mouse events on window change.
TEST_F(WindowTest, MouseEventsOnWindowChange) {
  gfx::Size size = host()->GetBounds().size();

  EventGenerator generator(root_window());
  generator.MoveMouseTo(50, 50);

  EventCountDelegate d1;
  scoped_ptr<Window> w1(CreateTestWindowWithDelegate(&d1, 1,
      gfx::Rect(0, 0, 100, 100), root_window()));
  RunAllPendingInMessageLoop();
  // The format of result is "Enter/Mouse/Leave".
  EXPECT_EQ("1 1 0", d1.GetMouseMotionCountsAndReset());

  // Adding new window.
  EventCountDelegate d11;
  scoped_ptr<Window> w11(CreateTestWindowWithDelegate(
      &d11, 1, gfx::Rect(0, 0, 100, 100), w1.get()));
  RunAllPendingInMessageLoop();
  EXPECT_EQ("0 0 1", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("1 1 0", d11.GetMouseMotionCountsAndReset());

  // Move bounds.
  w11->SetBounds(gfx::Rect(0, 0, 10, 10));
  RunAllPendingInMessageLoop();
  EXPECT_EQ("1 1 0", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0 1", d11.GetMouseMotionCountsAndReset());

  w11->SetBounds(gfx::Rect(0, 0, 60, 60));
  RunAllPendingInMessageLoop();
  EXPECT_EQ("0 0 1", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("1 1 0", d11.GetMouseMotionCountsAndReset());

  // Detach, then re-attach.
  w1->RemoveChild(w11.get());
  RunAllPendingInMessageLoop();
  EXPECT_EQ("1 1 0", d1.GetMouseMotionCountsAndReset());
  // Window is detached, so no event is set.
  EXPECT_EQ("0 0 1", d11.GetMouseMotionCountsAndReset());

  w1->AddChild(w11.get());
  RunAllPendingInMessageLoop();
  EXPECT_EQ("0 0 1", d1.GetMouseMotionCountsAndReset());
  // Window is detached, so no event is set.
  EXPECT_EQ("1 1 0", d11.GetMouseMotionCountsAndReset());

  // Visibility Change
  w11->Hide();
  RunAllPendingInMessageLoop();
  EXPECT_EQ("1 1 0", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0 1", d11.GetMouseMotionCountsAndReset());

  w11->Show();
  RunAllPendingInMessageLoop();
  EXPECT_EQ("0 0 1", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("1 1 0", d11.GetMouseMotionCountsAndReset());

  // Transform: move d11 by 100 100.
  gfx::Transform transform;
  transform.Translate(100, 100);
  w11->SetTransform(transform);
  RunAllPendingInMessageLoop();
  EXPECT_EQ("1 1 0", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0 1", d11.GetMouseMotionCountsAndReset());

  w11->SetTransform(gfx::Transform());
  RunAllPendingInMessageLoop();
  EXPECT_EQ("0 0 1", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("1 1 0", d11.GetMouseMotionCountsAndReset());

  // Closing a window.
  w11.reset();
  RunAllPendingInMessageLoop();
  EXPECT_EQ("1 1 0", d1.GetMouseMotionCountsAndReset());

  // Make sure we don't synthesize events if the mouse
  // is outside of the root window.
  generator.MoveMouseTo(-10, -10);
  EXPECT_EQ("0 0 1", d1.GetMouseMotionCountsAndReset());

  // Adding new windows.
  w11.reset(CreateTestWindowWithDelegate(
      &d11, 1, gfx::Rect(0, 0, 100, 100), w1.get()));
  RunAllPendingInMessageLoop();
  EXPECT_EQ("0 0 0", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0 1", d11.GetMouseMotionCountsAndReset());

  // Closing windows
  w11.reset();
  RunAllPendingInMessageLoop();
  EXPECT_EQ("0 0 0", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0 0", d11.GetMouseMotionCountsAndReset());
}

class RootWindowAttachmentObserver : public WindowObserver {
 public:
  RootWindowAttachmentObserver() : added_count_(0), removed_count_(0) {}
  virtual ~RootWindowAttachmentObserver() {}

  int added_count() const { return added_count_; }
  int removed_count() const { return removed_count_; }

  void Clear() {
    added_count_ = 0;
    removed_count_ = 0;
  }

  // Overridden from WindowObserver:
  virtual void OnWindowAddedToRootWindow(Window* window) OVERRIDE {
    ++added_count_;
  }
  virtual void OnWindowRemovingFromRootWindow(Window* window,
                                              Window* new_root) OVERRIDE {
    ++removed_count_;
  }

 private:
  int added_count_;
  int removed_count_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowAttachmentObserver);
};

TEST_F(WindowTest, RootWindowAttachment) {
  RootWindowAttachmentObserver observer;

  // Test a direct add/remove from the RootWindow.
  scoped_ptr<Window> w1(new Window(NULL));
  w1->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  w1->AddObserver(&observer);

  ParentWindow(w1.get());
  EXPECT_EQ(1, observer.added_count());
  EXPECT_EQ(0, observer.removed_count());

  w1.reset();
  EXPECT_EQ(1, observer.added_count());
  EXPECT_EQ(1, observer.removed_count());

  observer.Clear();

  // Test an indirect add/remove from the RootWindow.
  w1.reset(new Window(NULL));
  w1->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  Window* w11 = new Window(NULL);
  w11->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  w11->AddObserver(&observer);
  w1->AddChild(w11);
  EXPECT_EQ(0, observer.added_count());
  EXPECT_EQ(0, observer.removed_count());

  ParentWindow(w1.get());
  EXPECT_EQ(1, observer.added_count());
  EXPECT_EQ(0, observer.removed_count());

  w1.reset();  // Deletes w11.
  w11 = NULL;
  EXPECT_EQ(1, observer.added_count());
  EXPECT_EQ(1, observer.removed_count());

  observer.Clear();

  // Test an indirect add/remove with nested observers.
  w1.reset(new Window(NULL));
  w1->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  w11 = new Window(NULL);
  w11->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  w11->AddObserver(&observer);
  w1->AddChild(w11);
  Window* w111 = new Window(NULL);
  w111->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  w111->AddObserver(&observer);
  w11->AddChild(w111);

  EXPECT_EQ(0, observer.added_count());
  EXPECT_EQ(0, observer.removed_count());

  ParentWindow(w1.get());
  EXPECT_EQ(2, observer.added_count());
  EXPECT_EQ(0, observer.removed_count());

  w1.reset();  // Deletes w11 and w111.
  w11 = NULL;
  w111 = NULL;
  EXPECT_EQ(2, observer.added_count());
  EXPECT_EQ(2, observer.removed_count());
}

class BoundsChangedWindowObserver : public WindowObserver {
 public:
  BoundsChangedWindowObserver() : root_set_(false) {}

  virtual void OnWindowBoundsChanged(Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE {
    root_set_ = window->GetRootWindow() != NULL;
  }

  bool root_set() const { return root_set_; }

 private:
  bool root_set_;

  DISALLOW_COPY_AND_ASSIGN(BoundsChangedWindowObserver);
};

TEST_F(WindowTest, RootWindowSetWhenReparenting) {
  Window parent1(NULL);
  parent1.Init(aura::WINDOW_LAYER_NOT_DRAWN);
  Window parent2(NULL);
  parent2.Init(aura::WINDOW_LAYER_NOT_DRAWN);
  ParentWindow(&parent1);
  ParentWindow(&parent2);
  parent1.SetBounds(gfx::Rect(10, 10, 300, 300));
  parent2.SetBounds(gfx::Rect(20, 20, 300, 300));

  BoundsChangedWindowObserver observer;
  Window child(NULL);
  child.Init(aura::WINDOW_LAYER_NOT_DRAWN);
  child.SetBounds(gfx::Rect(5, 5, 100, 100));
  parent1.AddChild(&child);

  // We need animations to start in order to observe the bounds changes.
  ui::ScopedAnimationDurationScaleMode animation_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::ScopedLayerAnimationSettings settings1(child.layer()->GetAnimator());
  settings1.SetTransitionDuration(base::TimeDelta::FromMilliseconds(100));
  gfx::Rect new_bounds(gfx::Rect(35, 35, 50, 50));
  child.SetBounds(new_bounds);

  child.AddObserver(&observer);

  // Reparenting the |child| will cause it to get moved. During this move
  // the window should still have root window set.
  parent2.AddChild(&child);
  EXPECT_TRUE(observer.root_set());

  // Animations should stop and the bounds should be as set before the |child|
  // got reparented.
  EXPECT_EQ(new_bounds.ToString(), child.GetTargetBounds().ToString());
  EXPECT_EQ(new_bounds.ToString(), child.bounds().ToString());
  EXPECT_EQ("55,55 50x50", child.GetBoundsInRootWindow().ToString());
}

TEST_F(WindowTest, OwnedByParentFalse) {
  // By default, a window is owned by its parent. If this is set to false, the
  // window will not be destroyed when its parent is.

  scoped_ptr<Window> w1(new Window(NULL));
  w1->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  scoped_ptr<Window> w2(new Window(NULL));
  w2->set_owned_by_parent(false);
  w2->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  w1->AddChild(w2.get());

  w1.reset();

  // We should be able to deref w2 still, but its parent should now be NULL.
  EXPECT_EQ(NULL, w2->parent());
}

namespace {

// Used By DeleteWindowFromOnWindowDestroyed. Destroys a Window from
// OnWindowDestroyed().
class OwningWindowDelegate : public TestWindowDelegate {
 public:
  OwningWindowDelegate() {}

  void SetOwnedWindow(Window* window) {
    owned_window_.reset(window);
  }

  virtual void OnWindowDestroyed(Window* window) OVERRIDE {
    owned_window_.reset(NULL);
  }

 private:
  scoped_ptr<Window> owned_window_;

  DISALLOW_COPY_AND_ASSIGN(OwningWindowDelegate);
};

}  // namespace

// Creates a window with two child windows. When the first child window is
// destroyed (WindowDelegate::OnWindowDestroyed) it deletes the second child.
// This synthesizes BrowserView and the status bubble. Both are children of the
// same parent and destroying BrowserView triggers it destroying the status
// bubble.
TEST_F(WindowTest, DeleteWindowFromOnWindowDestroyed) {
  scoped_ptr<Window> parent(new Window(NULL));
  parent->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  OwningWindowDelegate delegate;
  Window* c1 = new Window(&delegate);
  c1->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  parent->AddChild(c1);
  Window* c2 = new Window(NULL);
  c2->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  parent->AddChild(c2);
  delegate.SetOwnedWindow(c2);
  parent.reset();
}

namespace {

// Used by DelegateNotifiedAsBoundsChange to verify OnBoundsChanged() is
// invoked.
class BoundsChangeDelegate : public TestWindowDelegate {
 public:
  BoundsChangeDelegate() : bounds_changed_(false) {}

  void clear_bounds_changed() { bounds_changed_ = false; }
  bool bounds_changed() const {
    return bounds_changed_;
  }

  // Window
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) OVERRIDE {
    bounds_changed_ = true;
  }

 private:
  // Was OnBoundsChanged() invoked?
  bool bounds_changed_;

  DISALLOW_COPY_AND_ASSIGN(BoundsChangeDelegate);
};

}  // namespace

// Verifies the delegate is notified when the actual bounds of the layer
// change.
TEST_F(WindowTest, DelegateNotifiedAsBoundsChange) {
  BoundsChangeDelegate delegate;

  // We cannot short-circuit animations in this test.
  ui::ScopedAnimationDurationScaleMode normal_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  scoped_ptr<Window> window(
      CreateTestWindowWithDelegate(&delegate, 1,
                                   gfx::Rect(0, 0, 100, 100), root_window()));
  window->layer()->GetAnimator()->set_disable_timer_for_test(true);

  delegate.clear_bounds_changed();

  // Animate to a different position.
  {
    ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
    window->SetBounds(gfx::Rect(100, 100, 100, 100));
  }

  // Bounds shouldn't immediately have changed.
  EXPECT_EQ("0,0 100x100", window->bounds().ToString());
  EXPECT_FALSE(delegate.bounds_changed());

  // Animate to the end, which should notify of the change.
  base::TimeTicks start_time =
      window->layer()->GetAnimator()->last_step_time();
  ui::LayerAnimator* animator = window->layer()->GetAnimator();
  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));
  EXPECT_TRUE(delegate.bounds_changed());
  EXPECT_NE("0,0 100x100", window->bounds().ToString());
}

// Verifies the delegate is notified when the actual bounds of the layer
// change even when the window is not the layer's delegate
TEST_F(WindowTest, DelegateNotifiedAsBoundsChangeInHiddenLayer) {
  BoundsChangeDelegate delegate;

  // We cannot short-circuit animations in this test.
  ui::ScopedAnimationDurationScaleMode normal_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  scoped_ptr<Window> window(
      CreateTestWindowWithDelegate(&delegate, 1,
                                   gfx::Rect(0, 0, 100, 100), root_window()));
  window->layer()->GetAnimator()->set_disable_timer_for_test(true);

  delegate.clear_bounds_changed();

  // Suppress paint on the window since it is hidden (should reset the layer's
  // delegate to NULL)
  window->SuppressPaint();
  EXPECT_EQ(NULL, window->layer()->delegate());

  // Animate to a different position.
  {
    ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
    window->SetBounds(gfx::Rect(100, 100, 110, 100));
  }

  // Layer delegate is NULL but we should still get bounds changed notification.
  EXPECT_EQ("100,100 110x100", window->GetTargetBounds().ToString());
  EXPECT_TRUE(delegate.bounds_changed());

  delegate.clear_bounds_changed();

  // Animate to the end: will *not* notify of the change since we are hidden.
  base::TimeTicks start_time =
      window->layer()->GetAnimator()->last_step_time();
  ui::LayerAnimator* animator = window->layer()->GetAnimator();
  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  // No bounds changed notification at the end of animation since layer
  // delegate is NULL.
  EXPECT_FALSE(delegate.bounds_changed());
  EXPECT_NE("0,0 100x100", window->layer()->bounds().ToString());
}

namespace {

// Used by AddChildNotifications to track notification counts.
class AddChildNotificationsObserver : public WindowObserver {
 public:
  AddChildNotificationsObserver() : added_count_(0), removed_count_(0) {}

  std::string CountStringAndReset() {
    std::string result = base::IntToString(added_count_) + " " +
        base::IntToString(removed_count_);
    added_count_ = removed_count_ = 0;
    return result;
  }

  // WindowObserver overrides:
  virtual void OnWindowAddedToRootWindow(Window* window) OVERRIDE {
    added_count_++;
  }
  virtual void OnWindowRemovingFromRootWindow(Window* window,
                                              Window* new_root) OVERRIDE {
    removed_count_++;
  }

 private:
  int added_count_;
  int removed_count_;

  DISALLOW_COPY_AND_ASSIGN(AddChildNotificationsObserver);
};

}  // namespace

// Assertions around when root window notifications are sent.
TEST_F(WindowTest, AddChildNotifications) {
  AddChildNotificationsObserver observer;
  scoped_ptr<Window> w1(CreateTestWindowWithId(1, root_window()));
  scoped_ptr<Window> w2(CreateTestWindowWithId(1, root_window()));
  w2->AddObserver(&observer);
  w2->Focus();
  EXPECT_TRUE(w2->HasFocus());

  // Move |w2| to be a child of |w1|.
  w1->AddChild(w2.get());
  // Sine we moved in the same root, observer shouldn't be notified.
  EXPECT_EQ("0 0", observer.CountStringAndReset());
  // |w2| should still have focus after moving.
  EXPECT_TRUE(w2->HasFocus());
}

// Tests that a delegate that destroys itself when the window is destroyed does
// not break.
TEST_F(WindowTest, DelegateDestroysSelfOnWindowDestroy) {
  scoped_ptr<Window> w1(CreateTestWindowWithDelegate(
      new DestroyWindowDelegate(),
      0,
      gfx::Rect(10, 20, 30, 40),
      root_window()));
}

class HierarchyObserver : public WindowObserver {
 public:
  HierarchyObserver(Window* target) : target_(target) {
    target_->AddObserver(this);
  }
  virtual ~HierarchyObserver() {
    target_->RemoveObserver(this);
  }

  void ValidateState(
      int index,
      const WindowObserver::HierarchyChangeParams& params) const {
    ParamsMatch(params_[index], params);
  }

  void Reset() {
    params_.clear();
  }

 private:
  // Overridden from WindowObserver:
  virtual void OnWindowHierarchyChanging(
      const HierarchyChangeParams& params) OVERRIDE {
    params_.push_back(params);
  }
  virtual void OnWindowHierarchyChanged(
      const HierarchyChangeParams& params) OVERRIDE {
    params_.push_back(params);
  }

  void ParamsMatch(const WindowObserver::HierarchyChangeParams& p1,
                   const WindowObserver::HierarchyChangeParams& p2) const {
    EXPECT_EQ(p1.phase, p2.phase);
    EXPECT_EQ(p1.target, p2.target);
    EXPECT_EQ(p1.new_parent, p2.new_parent);
    EXPECT_EQ(p1.old_parent, p2.old_parent);
    EXPECT_EQ(p1.receiver, p2.receiver);
  }

  Window* target_;
  std::vector<WindowObserver::HierarchyChangeParams> params_;

  DISALLOW_COPY_AND_ASSIGN(HierarchyObserver);
};

// Tests hierarchy change notifications.
TEST_F(WindowTest, OnWindowHierarchyChange) {
  {
    // Simple add & remove.
    HierarchyObserver oroot(root_window());

    scoped_ptr<Window> w1(CreateTestWindowWithId(1, NULL));
    HierarchyObserver o1(w1.get());

    // Add.
    root_window()->AddChild(w1.get());

    WindowObserver::HierarchyChangeParams params;
    params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGING;
    params.target = w1.get();
    params.old_parent = NULL;
    params.new_parent = root_window();
    params.receiver = w1.get();
    o1.ValidateState(0, params);

    params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGED;
    params.receiver = w1.get();
    o1.ValidateState(1, params);

    params.receiver = root_window();
    oroot.ValidateState(0, params);

    // Remove.
    o1.Reset();
    oroot.Reset();

    root_window()->RemoveChild(w1.get());

    params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGING;
    params.old_parent = root_window();
    params.new_parent = NULL;
    params.receiver = w1.get();

    o1.ValidateState(0, params);

    params.receiver = root_window();
    oroot.ValidateState(0, params);

    params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGED;
    params.receiver = w1.get();
    o1.ValidateState(1, params);
  }

  {
    // Add & remove of hierarchy. Tests notification order per documentation in
    // WindowObserver.
    HierarchyObserver o(root_window());
    scoped_ptr<Window> w1(CreateTestWindowWithId(1, NULL));
    Window* w11 = CreateTestWindowWithId(11, w1.get());
    w1->AddObserver(&o);
    w11->AddObserver(&o);

    // Add.
    root_window()->AddChild(w1.get());

    // Dispatched to target first.
    int index = 0;
    WindowObserver::HierarchyChangeParams params;
    params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGING;
    params.target = w1.get();
    params.old_parent = NULL;
    params.new_parent = root_window();
    params.receiver = w1.get();
    o.ValidateState(index++, params);

    // Dispatched to target's children.
    params.receiver = w11;
    o.ValidateState(index++, params);

    params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGED;

    // Now process the "changed" phase.
    params.receiver = w1.get();
    o.ValidateState(index++, params);
    params.receiver = w11;
    o.ValidateState(index++, params);
    params.receiver = root_window();
    o.ValidateState(index++, params);

    // Remove.
    root_window()->RemoveChild(w1.get());
    params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGING;
    params.old_parent = root_window();
    params.new_parent = NULL;
    params.receiver = w1.get();
    o.ValidateState(index++, params);
    params.receiver = w11;
    o.ValidateState(index++, params);
    params.receiver = root_window();
    o.ValidateState(index++, params);
    params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGED;
    params.receiver = w1.get();
    o.ValidateState(index++, params);
    params.receiver = w11;
    o.ValidateState(index++, params);

    w1.reset();
  }

  {
    // Reparent. Tests notification order per documentation in WindowObserver.
    scoped_ptr<Window> w1(CreateTestWindowWithId(1, root_window()));
    Window* w11 = CreateTestWindowWithId(11, w1.get());
    Window* w111 = CreateTestWindowWithId(111, w11);
    scoped_ptr<Window> w2(CreateTestWindowWithId(2, root_window()));

    HierarchyObserver o(root_window());
    w1->AddObserver(&o);
    w11->AddObserver(&o);
    w111->AddObserver(&o);
    w2->AddObserver(&o);

    w2->AddChild(w11);

    // Dispatched to target first.
    int index = 0;
    WindowObserver::HierarchyChangeParams params;
    params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGING;
    params.target = w11;
    params.old_parent = w1.get();
    params.new_parent = w2.get();
    params.receiver = w11;
    o.ValidateState(index++, params);

    // Then to target's children.
    params.receiver = w111;
    o.ValidateState(index++, params);

    // Then to target's old parent chain.
    params.receiver = w1.get();
    o.ValidateState(index++, params);
    params.receiver = root_window();
    o.ValidateState(index++, params);

    // "Changed" phase.
    params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGED;
    params.receiver = w11;
    o.ValidateState(index++, params);
    params.receiver = w111;
    o.ValidateState(index++, params);
    params.receiver = w2.get();
    o.ValidateState(index++, params);
    params.receiver = root_window();
    o.ValidateState(index++, params);

    w1.reset();
    w2.reset();
  }

}

// Verifies SchedulePaint() on a layerless window results in damaging the right
// thing.
TEST_F(WindowTest, LayerlessWindowSchedulePaint) {
  Window root(NULL);
  root.Init(aura::WINDOW_LAYER_NOT_DRAWN);
  root.SetBounds(gfx::Rect(0, 0, 100, 100));

  Window* layerless_window = new Window(NULL);  // Owned by |root|.
  layerless_window->Init(WINDOW_LAYER_NONE);
  layerless_window->SetBounds(gfx::Rect(10, 11, 12, 13));
  root.AddChild(layerless_window);

  root.layer()->SendDamagedRects();
  layerless_window->SchedulePaintInRect(gfx::Rect(1, 2, 100, 4));
  // Note the the region is clipped by the parent hence 100 going to 11.
  EXPECT_EQ("11,13 11x4",
            gfx::SkIRectToRect(root.layer()->damaged_region().getBounds()).
            ToString());

  Window* layerless_window2 = new Window(NULL);  // Owned by |layerless_window|.
  layerless_window2->Init(WINDOW_LAYER_NONE);
  layerless_window2->SetBounds(gfx::Rect(1, 2, 3, 4));
  layerless_window->AddChild(layerless_window2);

  root.layer()->SendDamagedRects();
  layerless_window2->SchedulePaintInRect(gfx::Rect(1, 2, 100, 4));
  // Note the the region is clipped by the |layerless_window| hence 100 going to
  // 2.
  EXPECT_EQ("12,15 2x2",
            gfx::SkIRectToRect(root.layer()->damaged_region().getBounds()).
            ToString());
}

// Verifies bounds of layerless windows are correctly updated when adding
// removing.
TEST_F(WindowTest, NestedLayerlessWindowsBoundsOnAddRemove) {
  // Creates the following structure (all children owned by root):
  // root
  //   w1ll      1,2
  //     w11ll   3,4
  //       w111  5,6
  //     w12     7,8
  //       w121  9,10
  //
  // ll: layer less, eg no layer
  Window root(NULL);
  root.Init(WINDOW_LAYER_NOT_DRAWN);
  root.SetBounds(gfx::Rect(0, 0, 100, 100));

  Window* w1ll = new Window(NULL);
  w1ll->Init(WINDOW_LAYER_NONE);
  w1ll->SetBounds(gfx::Rect(1, 2, 100, 100));

  Window* w11ll = new Window(NULL);
  w11ll->Init(WINDOW_LAYER_NONE);
  w11ll->SetBounds(gfx::Rect(3, 4, 100, 100));
  w1ll->AddChild(w11ll);

  Window* w111 = new Window(NULL);
  w111->Init(WINDOW_LAYER_NOT_DRAWN);
  w111->SetBounds(gfx::Rect(5, 6, 100, 100));
  w11ll->AddChild(w111);

  Window* w12 = new Window(NULL);
  w12->Init(WINDOW_LAYER_NOT_DRAWN);
  w12->SetBounds(gfx::Rect(7, 8, 100, 100));
  w1ll->AddChild(w12);

  Window* w121 = new Window(NULL);
  w121->Init(WINDOW_LAYER_NOT_DRAWN);
  w121->SetBounds(gfx::Rect(9, 10, 100, 100));
  w12->AddChild(w121);

  root.AddChild(w1ll);

  // All layers should be parented to the root.
  EXPECT_EQ(root.layer(), w111->layer()->parent());
  EXPECT_EQ(root.layer(), w12->layer()->parent());
  EXPECT_EQ(w12->layer(), w121->layer()->parent());

  // Ensure bounds are what we expect.
  EXPECT_EQ("1,2 100x100", w1ll->bounds().ToString());
  EXPECT_EQ("3,4 100x100", w11ll->bounds().ToString());
  EXPECT_EQ("5,6 100x100", w111->bounds().ToString());
  EXPECT_EQ("7,8 100x100", w12->bounds().ToString());
  EXPECT_EQ("9,10 100x100", w121->bounds().ToString());

  // Bounds of layers are relative to the nearest ancestor with a layer.
  EXPECT_EQ("8,10 100x100", w12->layer()->bounds().ToString());
  EXPECT_EQ("9,12 100x100", w111->layer()->bounds().ToString());
  EXPECT_EQ("9,10 100x100", w121->layer()->bounds().ToString());

  // Remove and repeat.
  root.RemoveChild(w1ll);

  EXPECT_TRUE(w111->layer()->parent() == NULL);
  EXPECT_TRUE(w12->layer()->parent() == NULL);

  // Verify bounds haven't changed again.
  EXPECT_EQ("1,2 100x100", w1ll->bounds().ToString());
  EXPECT_EQ("3,4 100x100", w11ll->bounds().ToString());
  EXPECT_EQ("5,6 100x100", w111->bounds().ToString());
  EXPECT_EQ("7,8 100x100", w12->bounds().ToString());
  EXPECT_EQ("9,10 100x100", w121->bounds().ToString());

  // Bounds of layers should now match that of windows.
  EXPECT_EQ("7,8 100x100", w12->layer()->bounds().ToString());
  EXPECT_EQ("5,6 100x100", w111->layer()->bounds().ToString());
  EXPECT_EQ("9,10 100x100", w121->layer()->bounds().ToString());

  delete w1ll;
}

// Verifies bounds of layerless windows are correctly updated when bounds
// of ancestor changes.
TEST_F(WindowTest, NestedLayerlessWindowsBoundsOnSetBounds) {
  // Creates the following structure (all children owned by root):
  // root
  //   w1ll      1,2
  //     w11ll   3,4
  //       w111  5,6
  //     w12     7,8
  //       w121  9,10
  //
  // ll: layer less, eg no layer
  Window root(NULL);
  root.Init(WINDOW_LAYER_NOT_DRAWN);
  root.SetBounds(gfx::Rect(0, 0, 100, 100));

  Window* w1ll = new Window(NULL);
  w1ll->Init(WINDOW_LAYER_NONE);
  w1ll->SetBounds(gfx::Rect(1, 2, 100, 100));

  Window* w11ll = new Window(NULL);
  w11ll->Init(WINDOW_LAYER_NONE);
  w11ll->SetBounds(gfx::Rect(3, 4, 100, 100));
  w1ll->AddChild(w11ll);

  Window* w111 = new Window(NULL);
  w111->Init(WINDOW_LAYER_NOT_DRAWN);
  w111->SetBounds(gfx::Rect(5, 6, 100, 100));
  w11ll->AddChild(w111);

  Window* w12 = new Window(NULL);
  w12->Init(WINDOW_LAYER_NOT_DRAWN);
  w12->SetBounds(gfx::Rect(7, 8, 100, 100));
  w1ll->AddChild(w12);

  Window* w121 = new Window(NULL);
  w121->Init(WINDOW_LAYER_NOT_DRAWN);
  w121->SetBounds(gfx::Rect(9, 10, 100, 100));
  w12->AddChild(w121);

  root.AddChild(w1ll);

  w111->SetBounds(gfx::Rect(7, 8, 11, 12));
  EXPECT_EQ("7,8 11x12", w111->bounds().ToString());
  EXPECT_EQ("7,8 11x12", w111->GetTargetBounds().ToString());
  EXPECT_EQ("11,14 11x12", w111->layer()->bounds().ToString());

  // Set back.
  w111->SetBounds(gfx::Rect(5, 6, 100, 100));
  EXPECT_EQ("5,6 100x100", w111->bounds().ToString());
  EXPECT_EQ("5,6 100x100", w111->GetTargetBounds().ToString());
  EXPECT_EQ("9,12 100x100", w111->layer()->bounds().ToString());

  // Setting the bounds of a layerless window needs to adjust the bounds of
  // layered children.
  w11ll->SetBounds(gfx::Rect(5, 6, 100, 100));
  EXPECT_EQ("5,6 100x100", w11ll->bounds().ToString());
  EXPECT_EQ("5,6 100x100", w11ll->GetTargetBounds().ToString());
  EXPECT_EQ("5,6 100x100", w111->bounds().ToString());
  EXPECT_EQ("5,6 100x100", w111->GetTargetBounds().ToString());
  EXPECT_EQ("11,14 100x100", w111->layer()->bounds().ToString());

  root.RemoveChild(w1ll);

  w111->SetBounds(gfx::Rect(7, 8, 11, 12));
  EXPECT_EQ("7,8 11x12", w111->bounds().ToString());
  EXPECT_EQ("7,8 11x12", w111->GetTargetBounds().ToString());
  EXPECT_EQ("7,8 11x12", w111->layer()->bounds().ToString());

  delete w1ll;
}

namespace {

// Tracks the number of times paint is invoked along with what the clip and
// translate was.
class PaintWindowDelegate : public TestWindowDelegate {
 public:
  PaintWindowDelegate() : paint_count_(0) {}
  virtual ~PaintWindowDelegate() {}

  const gfx::Rect& most_recent_paint_clip_bounds() const {
    return most_recent_paint_clip_bounds_;
  }

  const gfx::Vector2d& most_recent_paint_matrix_offset() const {
    return most_recent_paint_matrix_offset_;
  }

  void clear_paint_count() { paint_count_ = 0; }
  int paint_count() const { return paint_count_; }

  // TestWindowDelegate::
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    paint_count_++;
    canvas->GetClipBounds(&most_recent_paint_clip_bounds_);
    const SkMatrix& matrix = canvas->sk_canvas()->getTotalMatrix();
    most_recent_paint_matrix_offset_ = gfx::Vector2d(
        SkScalarFloorToInt(matrix.getTranslateX()),
        SkScalarFloorToInt(matrix.getTranslateY()));
  }

 private:
  int paint_count_;
  gfx::Rect most_recent_paint_clip_bounds_;
  gfx::Vector2d most_recent_paint_matrix_offset_;

  DISALLOW_COPY_AND_ASSIGN(PaintWindowDelegate);
};

}  // namespace

// Assertions around layerless children being painted when non-layerless window
// is painted.
TEST_F(WindowTest, PaintLayerless) {
  // Creates the following structure (all children owned by root):
  // root
  //   w1ll      1,2 40x50
  //     w11ll   3,4 11x12
  //       w111  5,6
  //
  // ll: layer less, eg no layer
  PaintWindowDelegate w1ll_delegate;
  PaintWindowDelegate w11ll_delegate;
  PaintWindowDelegate w111_delegate;

  Window root(NULL);
  root.Init(WINDOW_LAYER_NOT_DRAWN);
  root.SetBounds(gfx::Rect(0, 0, 100, 100));

  Window* w1ll = new Window(&w1ll_delegate);
  w1ll->Init(WINDOW_LAYER_NONE);
  w1ll->SetBounds(gfx::Rect(1, 2, 40, 50));
  w1ll->Show();
  root.AddChild(w1ll);

  Window* w11ll = new Window(&w11ll_delegate);
  w11ll->Init(WINDOW_LAYER_NONE);
  w11ll->SetBounds(gfx::Rect(3, 4, 11, 12));
  w11ll->Show();
  w1ll->AddChild(w11ll);

  Window* w111 = new Window(&w111_delegate);
  w111->Init(WINDOW_LAYER_NOT_DRAWN);
  w111->SetBounds(gfx::Rect(5, 6, 100, 100));
  w111->Show();
  w11ll->AddChild(w111);

  EXPECT_EQ(0, w1ll_delegate.paint_count());
  EXPECT_EQ(0, w11ll_delegate.paint_count());
  EXPECT_EQ(0, w111_delegate.paint_count());

  // Paint the root, this should trigger painting of the two layerless
  // descendants but not the layered descendant.
  gfx::Canvas canvas(gfx::Size(200, 200), 1.0f, true);
  static_cast<ui::LayerDelegate&>(root).OnPaintLayer(&canvas);

  // NOTE: SkCanvas::getClipBounds() extends the clip 1 pixel to the left and up
  // and 2 pixels down and to the right.
  EXPECT_EQ(1, w1ll_delegate.paint_count());
  EXPECT_EQ("-1,-1 42x52",
            w1ll_delegate.most_recent_paint_clip_bounds().ToString());
  EXPECT_EQ("[1 2]",
            w1ll_delegate.most_recent_paint_matrix_offset().ToString());
  EXPECT_EQ(1, w11ll_delegate.paint_count());
  EXPECT_EQ("-1,-1 13x14",
            w11ll_delegate.most_recent_paint_clip_bounds().ToString());
  EXPECT_EQ("[4 6]",
            w11ll_delegate.most_recent_paint_matrix_offset().ToString());
  EXPECT_EQ(0, w111_delegate.paint_count());
}

namespace {

std::string ConvertPointToTargetString(const Window* source,
                                       const Window* target) {
  gfx::Point location;
  Window::ConvertPointToTarget(source, target, &location);
  return location.ToString();
}

}  // namespace

// Assertions around Window::ConvertPointToTarget() with layerless windows.
TEST_F(WindowTest, ConvertPointToTargetLayerless) {
  // Creates the following structure (all children owned by root):
  // root
  //   w1ll      1,2
  //     w11ll   3,4
  //       w111  5,6
  //     w12     7,8
  //       w121  9,10
  //
  // ll: layer less, eg no layer
  Window root(NULL);
  root.Init(WINDOW_LAYER_NOT_DRAWN);
  root.SetBounds(gfx::Rect(0, 0, 100, 100));

  Window* w1ll = new Window(NULL);
  w1ll->Init(WINDOW_LAYER_NONE);
  w1ll->SetBounds(gfx::Rect(1, 2, 100, 100));

  Window* w11ll = new Window(NULL);
  w11ll->Init(WINDOW_LAYER_NONE);
  w11ll->SetBounds(gfx::Rect(3, 4, 100, 100));
  w1ll->AddChild(w11ll);

  Window* w111 = new Window(NULL);
  w111->Init(WINDOW_LAYER_NOT_DRAWN);
  w111->SetBounds(gfx::Rect(5, 6, 100, 100));
  w11ll->AddChild(w111);

  Window* w12 = new Window(NULL);
  w12->Init(WINDOW_LAYER_NOT_DRAWN);
  w12->SetBounds(gfx::Rect(7, 8, 100, 100));
  w1ll->AddChild(w12);

  Window* w121 = new Window(NULL);
  w121->Init(WINDOW_LAYER_NOT_DRAWN);
  w121->SetBounds(gfx::Rect(9, 10, 100, 100));
  w12->AddChild(w121);

  root.AddChild(w1ll);

  // w111->w11ll
  EXPECT_EQ("5,6", ConvertPointToTargetString(w111, w11ll));

  // w111->w1ll
  EXPECT_EQ("8,10", ConvertPointToTargetString(w111, w1ll));

  // w111->root
  EXPECT_EQ("9,12", ConvertPointToTargetString(w111, &root));

  // w111->w12
  EXPECT_EQ("1,2", ConvertPointToTargetString(w111, w12));

  // w111->w121
  EXPECT_EQ("-8,-8", ConvertPointToTargetString(w111, w121));

  // w11ll->w111
  EXPECT_EQ("-5,-6", ConvertPointToTargetString(w11ll, w111));

  // w11ll->w11ll
  EXPECT_EQ("3,4", ConvertPointToTargetString(w11ll, w1ll));

  // w11ll->root
  EXPECT_EQ("4,6", ConvertPointToTargetString(w11ll, &root));

  // w11ll->w12
  EXPECT_EQ("-4,-4", ConvertPointToTargetString(w11ll, w12));
}

#if !defined(NDEBUG)
// Verifies PrintWindowHierarchy() doesn't crash with a layerless window.
TEST_F(WindowTest, PrintWindowHierarchyNotCrashLayerless) {
  Window root(NULL);
  root.Init(WINDOW_LAYER_NONE);
  root.SetBounds(gfx::Rect(0, 0, 100, 100));
  root.PrintWindowHierarchy(0);
}
#endif

namespace {

// See AddWindowsFromString() for details.
aura::Window* CreateWindowFromDescription(const std::string& description,
                                          WindowDelegate* delegate) {
  WindowLayerType window_type = WINDOW_LAYER_NOT_DRAWN;
  std::vector<std::string> tokens;
  Tokenize(description, ":", &tokens);
  DCHECK(!tokens.empty());
  std::string name(tokens[0]);
  tokens.erase(tokens.begin());
  if (!tokens.empty()) {
    if (tokens[0] == "ll") {
      window_type = WINDOW_LAYER_NONE;
      tokens.erase(tokens.begin());
    }
    DCHECK(tokens.empty()) << "unknown tokens for creating window "
                           << description;
  }
  Window* window = new Window(delegate);
  window->Init(window_type);
  window->SetName(name);
  // Window name is only propagated to layer in debug builds.
  if (window->layer())
    window->layer()->set_name(name);
  return window;
}

// Creates and adds a tree of windows to |parent|. |description| consists
// of the following pieces:
//   X: Identifies a new window. Consists of a name and optionally ":ll" to
//      specify  WINDOW_LAYER_NONE, eg "w1:ll".
//   []: optionally used to specify the children of the window. Contains any
//       number of window identifiers and their corresponding children.
// For example: "[ a [ a1 a2:ll ] b c [ c1 ] ]" creates the tree:
//   a
//     a1
//     a2 -> WINDOW_LAYER_NONE.
//   b
//   c
//     c1
// NOTE: you must have a space after every token.
std::string::size_type AddWindowsFromString(aura::Window* parent,
                                            const std::string& description,
                                            std::string::size_type start_pos,
                                            WindowDelegate* delegate) {
  DCHECK(parent);
  std::string::size_type end_pos = description.find(' ', start_pos);
  while (end_pos != std::string::npos) {
    const std::string::size_type part_length = end_pos - start_pos;
    const std::string window_description =
        description.substr(start_pos, part_length);
    if (window_description == "[") {
      start_pos = AddWindowsFromString(parent->children().back(),
                                       description,
                                       end_pos + 1,
                                       delegate);
      end_pos = description.find(' ', start_pos);
      if (end_pos == std::string::npos && start_pos != end_pos)
        end_pos = description.length();
    } else if (window_description == "]") {
      ++end_pos;
      break;
    } else {
      Window* window =
          CreateWindowFromDescription(window_description, delegate);
      parent->AddChild(window);
      start_pos = ++end_pos;
      end_pos = description.find(' ', start_pos);
    }
  }
  return end_pos;
}

// Used by BuildRootWindowTreeDescription().
std::string BuildWindowTreeDescription(const aura::Window& window) {
  std::string result;
  result += window.name();
  if (window.children().empty())
    return result;

  result += " [ ";
  for (size_t i = 0; i < window.children().size(); ++i) {
    if (i != 0)
      result += " ";
    result += BuildWindowTreeDescription(*(window.children()[i]));
  }
  result += " ]";
  return result;
}

// Creates a string from |window|. See AddWindowsFromString() for details of the
// returned string. This does *not* include the layer type in the description,
// on the name.
std::string BuildRootWindowTreeDescription(const aura::Window& window) {
  std::string result;
  for (size_t i = 0; i < window.children().size(); ++i) {
    if (i != 0)
      result += " ";
    result += BuildWindowTreeDescription(*(window.children()[i]));
  }
  return result;
}

// Used by BuildRootWindowTreeDescription().
std::string BuildLayerTreeDescription(const ui::Layer& layer) {
  std::string result;
  result += layer.name();
  if (layer.children().empty())
    return result;

  result += " [ ";
  for (size_t i = 0; i < layer.children().size(); ++i) {
    if (i != 0)
      result += " ";
    result += BuildLayerTreeDescription(*(layer.children()[i]));
  }
  result += " ]";
  return result;
}

// Builds a string for all the children of |layer|. The returned string is in
// the same format as AddWindowsFromString() but only includes the name of the
// layers.
std::string BuildRootLayerTreeDescription(const ui::Layer& layer) {
  std::string result;
  for (size_t i = 0; i < layer.children().size(); ++i) {
    if (i != 0)
      result += " ";
    result += BuildLayerTreeDescription(*(layer.children()[i]));
  }
  return result;
}

// Returns the first window whose name matches |name| in |parent|.
aura::Window* FindWindowByName(aura::Window* parent,
                               const std::string& name) {
  if (parent->name() == name)
    return parent;
  for (size_t i = 0; i < parent->children().size(); ++i) {
    aura::Window* child = FindWindowByName(parent->children()[i], name);
    if (child)
      return child;
  }
  return NULL;
}

}  // namespace

// Direction to stack.
enum StackType {
  STACK_ABOVE,
  STACK_BELOW,
  STACK_AT_BOTTOM,
  STACK_AT_TOP,
};

// Permutations of StackChildAt with various data.
TEST_F(WindowTest, StackChildAtLayerless) {
  struct TestData {
    // Describes the window tree to create. See AddWindowsFromString() for
    // details.
    const std::string initial_description;

    // Identifies the window to move.
    const std::string source_window;

    // Window to move |source_window| relative to. Not used for STACK_AT_BOTTOM
    // or STACK_AT_TOP.
    const std::string target_window;

    StackType stack_type;

    // Expected window and layer results.
    const std::string expected_description;
    const std::string expected_layer_description;
  } data[] = {
    // 1 at top.
    {
      "1:ll [ 11 12 ] 2:ll [ 21 ]",
      "1",
      "",
      STACK_AT_TOP,
      "2 [ 21 ] 1 [ 11 12 ]",
      "21 11 12",
    },

    // 1 at bottom.
    {
      "1:ll [ 11 12 ] 2:ll [ 21 ]",
      "1",
      "",
      STACK_AT_BOTTOM,
      "1 [ 11 12 ] 2 [ 21 ]",
      "11 12 21",
    },

    // 2 at bottom.
    {
      "1:ll [ 11 12 ] 2:ll [ 21 ]",
      "2",
      "",
      STACK_AT_BOTTOM,
      "2 [ 21 ] 1 [ 11 12 ]",
      "21 11 12",
    },

    // 3 below 2.
    {
      "1:ll [ 11 12 ] 2:ll [ 21 ] 3:ll",
      "3",
      "2",
      STACK_BELOW,
      "1 [ 11 12 ] 3 2 [ 21 ]",
      "11 12 21",
    },

    // 2 below 1.
    {
      "1:ll [ 11 12 ] 2:ll [ 21 ]",
      "2",
      "1",
      STACK_BELOW,
      "2 [ 21 ] 1 [ 11 12 ]",
      "21 11 12",
    },

    // 1 above 3.
    {
      "1:ll [ 11 12 ] 2:ll [ 21 ] 3:ll",
      "1",
      "3",
      STACK_ABOVE,
      "2 [ 21 ] 3 1 [ 11 12 ]",
      "21 11 12",
    },

    // 1 above 2.
    {
      "1:ll [ 11 12 ] 2:ll [ 21 ]",
      "1",
      "2",
      STACK_ABOVE,
      "2 [ 21 ] 1 [ 11 12 ]",
      "21 11 12",
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    test::TestWindowDelegate delegate;
    Window root(NULL);
    root.Init(WINDOW_LAYER_NOT_DRAWN);
    root.SetBounds(gfx::Rect(0, 0, 100, 100));
    AddWindowsFromString(
        &root,
        data[i].initial_description,
        static_cast<std::string::size_type>(0), &delegate);
    aura::Window* source = FindWindowByName(&root, data[i].source_window);
    ASSERT_TRUE(source != NULL) << "unable to find source window "
                                << data[i].source_window << " at " << i;
    aura::Window* target = FindWindowByName(&root, data[i].target_window);
    switch (data[i].stack_type) {
      case STACK_ABOVE:
        ASSERT_TRUE(target != NULL) << "unable to find target window "
                                    << data[i].target_window << " at " << i;
        source->parent()->StackChildAbove(source, target);
        break;
      case STACK_BELOW:
        ASSERT_TRUE(target != NULL) << "unable to find target window "
                                    << data[i].target_window << " at " << i;
        source->parent()->StackChildBelow(source, target);
        break;
      case STACK_AT_BOTTOM:
        source->parent()->StackChildAtBottom(source);
        break;
      case STACK_AT_TOP:
        source->parent()->StackChildAtTop(source);
        break;
    }
    EXPECT_EQ(data[i].expected_layer_description,
              BuildRootLayerTreeDescription(*root.layer()))
        << "layer tree doesn't match at " << i;
    EXPECT_EQ(data[i].expected_description,
              BuildRootWindowTreeDescription(root))
        << "window tree doesn't match at " << i;
  }
}

namespace {

class TestLayerAnimationObserver : public ui::LayerAnimationObserver {
 public:
  TestLayerAnimationObserver()
      : animation_completed_(false),
        animation_aborted_(false) {}
  virtual ~TestLayerAnimationObserver() {}

  bool animation_completed() const { return animation_completed_; }
  bool animation_aborted() const { return animation_aborted_; }

  void Reset() {
    animation_completed_ = false;
    animation_aborted_ = false;
  }

 private:
  // ui::LayerAnimationObserver:
  virtual void OnLayerAnimationEnded(
      ui::LayerAnimationSequence* sequence) OVERRIDE {
    animation_completed_ = true;
  }

  virtual void OnLayerAnimationAborted(
      ui::LayerAnimationSequence* sequence) OVERRIDE {
    animation_aborted_ = true;
  }

  virtual void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) OVERRIDE {
  }

  bool animation_completed_;
  bool animation_aborted_;

  DISALLOW_COPY_AND_ASSIGN(TestLayerAnimationObserver);
};

}

TEST_F(WindowTest, WindowDestroyCompletesAnimations) {
  ui::ScopedAnimationDurationScaleMode normal_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  scoped_refptr<ui::LayerAnimator> animator =
      ui::LayerAnimator::CreateImplicitAnimator();
  TestLayerAnimationObserver observer;
  animator->AddObserver(&observer);
  // Make sure destroying a Window completes the animation.
  {
    scoped_ptr<Window> window(CreateTestWindowWithId(1, root_window()));
    window->layer()->SetAnimator(animator);

    gfx::Transform transform;
    transform.Scale(0.5f, 0.5f);
    window->SetTransform(transform);

    EXPECT_TRUE(animator->is_animating());
    EXPECT_FALSE(observer.animation_completed());
  }
  EXPECT_TRUE(animator);
  EXPECT_FALSE(animator->is_animating());
  EXPECT_TRUE(observer.animation_completed());
  EXPECT_FALSE(observer.animation_aborted());
  animator->RemoveObserver(&observer);
  observer.Reset();

  animator = ui::LayerAnimator::CreateImplicitAnimator();
  animator->AddObserver(&observer);
  ui::Layer layer;
  layer.SetAnimator(animator);
  {
    scoped_ptr<Window> window(CreateTestWindowWithId(1, root_window()));
    window->layer()->Add(&layer);

    gfx::Transform transform;
    transform.Scale(0.5f, 0.5f);
    layer.SetTransform(transform);

    EXPECT_TRUE(animator->is_animating());
    EXPECT_FALSE(observer.animation_completed());
  }

  EXPECT_TRUE(animator);
  EXPECT_FALSE(animator->is_animating());
  EXPECT_TRUE(observer.animation_completed());
  EXPECT_FALSE(observer.animation_aborted());
  animator->RemoveObserver(&observer);
}

}  // namespace test
}  // namespace aura
