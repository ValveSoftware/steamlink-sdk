// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/nested_accelerator_controller.h"

#include "base/bind.h"
#include "base/event_types.h"
#include "base/message_loop/message_loop.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/scoped_event_dispatcher.h"
#include "ui/wm/core/nested_accelerator_delegate.h"
#include "ui/wm/public/dispatcher_client.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include "ui/events/test/events_test_utils_x11.h"
#endif  // USE_X11

namespace wm {
namespace test {

namespace {

class MockDispatcher : public ui::PlatformEventDispatcher {
 public:
  MockDispatcher() : num_key_events_dispatched_(0) {}

  int num_key_events_dispatched() { return num_key_events_dispatched_; }

 private:
  // ui::PlatformEventDispatcher:
  virtual bool CanDispatchEvent(const ui::PlatformEvent& event) OVERRIDE {
    return true;
  }
  virtual uint32_t DispatchEvent(const ui::PlatformEvent& event) OVERRIDE {
    if (ui::EventTypeFromNative(event) == ui::ET_KEY_RELEASED)
      num_key_events_dispatched_++;
    return ui::POST_DISPATCH_NONE;
  }

  int num_key_events_dispatched_;

  DISALLOW_COPY_AND_ASSIGN(MockDispatcher);
};

class TestTarget : public ui::AcceleratorTarget {
 public:
  TestTarget() : accelerator_pressed_count_(0) {}
  virtual ~TestTarget() {}

  int accelerator_pressed_count() const { return accelerator_pressed_count_; }

  // Overridden from ui::AcceleratorTarget:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE {
    accelerator_pressed_count_++;
    return true;
  }
  virtual bool CanHandleAccelerators() const OVERRIDE { return true; }

 private:
  int accelerator_pressed_count_;

  DISALLOW_COPY_AND_ASSIGN(TestTarget);
};

void DispatchKeyReleaseA(aura::Window* root_window) {
// Sending both keydown and keyup is necessary here because the accelerator
// manager only checks a keyup event following a keydown event. See
// ShouldHandle() in ui/base/accelerators/accelerator_manager.cc for details.
#if defined(OS_WIN)
  MSG native_event_down = {NULL, WM_KEYDOWN, ui::VKEY_A, 0};
  aura::WindowTreeHost* host = root_window->GetHost();
  host->PostNativeEvent(native_event_down);
  MSG native_event_up = {NULL, WM_KEYUP, ui::VKEY_A, 0};
  host->PostNativeEvent(native_event_up);
#elif defined(USE_X11)
  ui::ScopedXI2Event native_event;
  native_event.InitKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_A, 0);
  aura::WindowTreeHost* host = root_window->GetHost();
  host->PostNativeEvent(native_event);
  native_event.InitKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_A, 0);
  host->PostNativeEvent(native_event);
#endif
  // Make sure the inner message-loop terminates after dispatching the events.
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::MessageLoop::current()->QuitClosure());
}

class MockNestedAcceleratorDelegate : public NestedAcceleratorDelegate {
 public:
  MockNestedAcceleratorDelegate()
      : accelerator_manager_(new ui::AcceleratorManager) {}
  virtual ~MockNestedAcceleratorDelegate() {}

  // NestedAcceleratorDelegate:
  virtual Result ProcessAccelerator(
      const ui::Accelerator& accelerator) OVERRIDE {
    return accelerator_manager_->Process(accelerator) ?
        RESULT_PROCESSED : RESULT_NOT_PROCESSED;
  }

  void Register(const ui::Accelerator& accelerator,
                ui::AcceleratorTarget* target) {
    accelerator_manager_->Register(
        accelerator, ui::AcceleratorManager::kNormalPriority, target);
  }

 private:
  scoped_ptr<ui::AcceleratorManager> accelerator_manager_;

  DISALLOW_COPY_AND_ASSIGN(MockNestedAcceleratorDelegate);
};

class NestedAcceleratorTest : public aura::test::AuraTestBase {
 public:
  NestedAcceleratorTest() {}
  virtual ~NestedAcceleratorTest() {}

  virtual void SetUp() OVERRIDE {
    AuraTestBase::SetUp();
    delegate_ = new MockNestedAcceleratorDelegate();
    nested_accelerator_controller_.reset(
        new NestedAcceleratorController(delegate_));
    aura::client::SetDispatcherClient(root_window(),
                                      nested_accelerator_controller_.get());
  }

  virtual void TearDown() OVERRIDE {
    aura::client::SetDispatcherClient(root_window(), NULL);
    AuraTestBase::TearDown();
    delegate_ = NULL;
    nested_accelerator_controller_.reset();
  }

  MockNestedAcceleratorDelegate* delegate() { return delegate_; }

 private:
  scoped_ptr<NestedAcceleratorController> nested_accelerator_controller_;
  MockNestedAcceleratorDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(NestedAcceleratorTest);
};

}  // namespace

// Aura window above lock screen in z order.
TEST_F(NestedAcceleratorTest, AssociatedWindowAboveLockScreen) {
  // TODO(oshima|sadrul): remove when Win implements PES.
  if (!ui::PlatformEventSource::GetInstance())
    return;
  MockDispatcher inner_dispatcher;
  scoped_ptr<aura::Window> mock_lock_container(
      CreateNormalWindow(0, root_window(), NULL));
  aura::test::CreateTestWindowWithId(1, mock_lock_container.get());

  scoped_ptr<aura::Window> associated_window(
      CreateNormalWindow(2, root_window(), NULL));
  EXPECT_TRUE(aura::test::WindowIsAbove(associated_window.get(),
                                        mock_lock_container.get()));

  DispatchKeyReleaseA(root_window());
  scoped_ptr<ui::ScopedEventDispatcher> override_dispatcher =
      ui::PlatformEventSource::GetInstance()->OverrideDispatcher(
          &inner_dispatcher);
  aura::client::DispatcherRunLoop run_loop(
      aura::client::GetDispatcherClient(root_window()), NULL);
  run_loop.Run();
  EXPECT_EQ(1, inner_dispatcher.num_key_events_dispatched());
}

// Test that the nested dispatcher handles accelerators.
TEST_F(NestedAcceleratorTest, AcceleratorsHandled) {
  // TODO(oshima|sadrul): remove when Win implements PES.
  if (!ui::PlatformEventSource::GetInstance())
    return;
  MockDispatcher inner_dispatcher;
  ui::Accelerator accelerator(ui::VKEY_A, ui::EF_NONE);
  accelerator.set_type(ui::ET_KEY_RELEASED);
  TestTarget target;
  delegate()->Register(accelerator, &target);

  DispatchKeyReleaseA(root_window());
  scoped_ptr<ui::ScopedEventDispatcher> override_dispatcher =
      ui::PlatformEventSource::GetInstance()->OverrideDispatcher(
          &inner_dispatcher);
  aura::client::DispatcherRunLoop run_loop(
      aura::client::GetDispatcherClient(root_window()), NULL);
  run_loop.Run();
  EXPECT_EQ(0, inner_dispatcher.num_key_events_dispatched());
  EXPECT_EQ(1, target.accelerator_pressed_count());
}

}  //  namespace test
}  //  namespace wm
