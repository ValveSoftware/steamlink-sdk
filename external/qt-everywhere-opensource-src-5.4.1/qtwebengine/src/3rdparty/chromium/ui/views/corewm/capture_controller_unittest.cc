// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/capture_controller.h"

#include "base/logging.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

#if !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_screen_position_client.h"
#endif

namespace views {

class CaptureControllerTest : public aura::test::AuraTestBase {
 public:
  CaptureControllerTest() {}

  virtual void SetUp() OVERRIDE {
    AuraTestBase::SetUp();
    capture_controller_.reset(new wm::ScopedCaptureClient(root_window()));

    second_host_.reset(aura::WindowTreeHost::Create(gfx::Rect(0, 0, 800, 600)));
    second_host_->InitHost();
    second_host_->window()->Show();
    second_host_->SetBounds(gfx::Rect(800, 600));
    second_capture_controller_.reset(
        new wm::ScopedCaptureClient(second_host_->window()));

#if !defined(OS_CHROMEOS)
    desktop_position_client_.reset(
        new DesktopScreenPositionClient(root_window()));

    second_desktop_position_client_.reset(
        new DesktopScreenPositionClient(second_host_->window()));
#endif
  }

  virtual void TearDown() OVERRIDE {
    RunAllPendingInMessageLoop();

#if !defined(OS_CHROMEOS)
    second_desktop_position_client_.reset();
#endif
    second_capture_controller_.reset();

    // Kill any active compositors before we hit the compositor shutdown paths.
    second_host_.reset();

#if !defined(OS_CHROMEOS)
    desktop_position_client_.reset();
#endif
    capture_controller_.reset();

    AuraTestBase::TearDown();
  }

  aura::Window* GetCaptureWindow() {
    return capture_controller_->capture_client()->GetCaptureWindow();
  }

  aura::Window* GetSecondCaptureWindow() {
    return second_capture_controller_->capture_client()->GetCaptureWindow();
  }

  scoped_ptr<wm::ScopedCaptureClient> capture_controller_;
  scoped_ptr<aura::WindowTreeHost> second_host_;
  scoped_ptr<wm::ScopedCaptureClient> second_capture_controller_;
#if !defined(OS_CHROMEOS)
  scoped_ptr<aura::client::ScreenPositionClient> desktop_position_client_;
  scoped_ptr<aura::client::ScreenPositionClient>
      second_desktop_position_client_;
#endif

  DISALLOW_COPY_AND_ASSIGN(CaptureControllerTest);
};

// Makes sure that internal details that are set on mouse down (such as
// mouse_pressed_handler()) are cleared when another root window takes capture.
TEST_F(CaptureControllerTest, ResetMouseEventHandlerOnCapture) {
  // Create a window inside the WindowEventDispatcher.
  scoped_ptr<aura::Window> w1(CreateNormalWindow(1, root_window(), NULL));

  // Make a synthesized mouse down event. Ensure that the WindowEventDispatcher
  // will dispatch further mouse events to |w1|.
  ui::MouseEvent mouse_pressed_event(ui::ET_MOUSE_PRESSED, gfx::Point(5, 5),
                                     gfx::Point(5, 5), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse_pressed_event);
  EXPECT_EQ(w1.get(), host()->dispatcher()->mouse_pressed_handler());

  // Build a window in the second WindowEventDispatcher.
  scoped_ptr<aura::Window> w2(
      CreateNormalWindow(2, second_host_->window(), NULL));

  // The act of having the second window take capture should clear out mouse
  // pressed handler in the first WindowEventDispatcher.
  w2->SetCapture();
  EXPECT_EQ(NULL, host()->dispatcher()->mouse_pressed_handler());
}

// Makes sure that when one window gets capture, it forces the release on the
// other. This is needed has to be handled explicitly on Linux, and is a sanity
// check on Windows.
TEST_F(CaptureControllerTest, ResetOtherWindowCaptureOnCapture) {
  // Create a window inside the WindowEventDispatcher.
  scoped_ptr<aura::Window> w1(CreateNormalWindow(1, root_window(), NULL));
  w1->SetCapture();
  // Both capture clients should return the same capture window.
  EXPECT_EQ(w1.get(), GetCaptureWindow());
  EXPECT_EQ(w1.get(), GetSecondCaptureWindow());

  // Build a window in the second WindowEventDispatcher and give it capture.
  // Both capture clients should return the same capture window.
  scoped_ptr<aura::Window> w2(
      CreateNormalWindow(2, second_host_->window(), NULL));
  w2->SetCapture();
  EXPECT_EQ(w2.get(), GetCaptureWindow());
  EXPECT_EQ(w2.get(), GetSecondCaptureWindow());
}

// Verifies the touch target for the WindowEventDispatcher gets reset on
// releasing capture.
TEST_F(CaptureControllerTest, TouchTargetResetOnCaptureChange) {
  // Create a window inside the WindowEventDispatcher.
  scoped_ptr<aura::Window> w1(CreateNormalWindow(1, root_window(), NULL));
  aura::test::EventGenerator event_generator1(root_window());
  event_generator1.PressTouch();
  w1->SetCapture();
  // Both capture clients should return the same capture window.
  EXPECT_EQ(w1.get(), GetCaptureWindow());
  EXPECT_EQ(w1.get(), GetSecondCaptureWindow());

  // Build a window in the second WindowEventDispatcher and give it capture.
  // Both capture clients should return the same capture window.
  scoped_ptr<aura::Window> w2(
      CreateNormalWindow(2, second_host_->window(), NULL));
  w2->SetCapture();
  EXPECT_EQ(w2.get(), GetCaptureWindow());
  EXPECT_EQ(w2.get(), GetSecondCaptureWindow());

  // Release capture on the window. Releasing capture should reset the touch
  // target of the first WindowEventDispatcher (as it no longer contains the
  // capture target).
  w2->ReleaseCapture();
  EXPECT_EQ(static_cast<aura::Window*>(NULL), GetCaptureWindow());
  EXPECT_EQ(static_cast<aura::Window*>(NULL), GetSecondCaptureWindow());
  ui::TouchEvent touch_event(
      ui::ET_TOUCH_PRESSED, gfx::Point(), 0, 0, ui::EventTimeForNow(), 1.0f,
      1.0f, 1.0f, 1.0f);
  EXPECT_EQ(static_cast<ui::GestureConsumer*>(w2.get()),
            ui::GestureRecognizer::Get()->GetTouchLockedTarget(touch_event));
}

}  // namespace views
