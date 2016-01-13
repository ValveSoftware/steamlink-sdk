// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/input_method_event_filter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_focus_manager.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/events/test/test_event_handler.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/public/activation_client.h"

#if !defined(OS_WIN) && !defined(USE_X11)
// On platforms except Windows and X11, aura::test::EventGenerator::PressKey
// generates a key event without native_event(), which is not supported by
// ui::MockInputMethod.
#define TestInputMethodKeyEventPropagation \
DISABLED_TestInputMethodKeyEventPropagation
#endif

namespace wm {

class TestTextInputClient : public ui::DummyTextInputClient {
 public:
  explicit TestTextInputClient(aura::Window* window) : window_(window) {}

  virtual aura::Window* GetAttachedWindow() const OVERRIDE { return window_; }

 private:
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(TestTextInputClient);
};

class InputMethodEventFilterTest : public aura::test::AuraTestBase {
 public:
  InputMethodEventFilterTest() {}
  virtual ~InputMethodEventFilterTest() {}

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    aura::test::AuraTestBase::SetUp();

    root_window()->AddPreTargetHandler(&root_filter_);
    input_method_event_filter_.reset(
        new InputMethodEventFilter(host()->GetAcceleratedWidget()));
    input_method_event_filter_->SetInputMethodPropertyInRootWindow(
        root_window());
    root_filter_.AddHandler(input_method_event_filter_.get());
    root_filter_.AddHandler(&test_filter_);

    test_window_.reset(aura::test::CreateTestWindowWithDelegate(
        &test_window_delegate_, -1, gfx::Rect(), root_window()));
    test_input_client_.reset(new TestTextInputClient(test_window_.get()));

    input_method_event_filter_->input_method()->SetFocusedTextInputClient(
        test_input_client_.get());
  }

  virtual void TearDown() OVERRIDE {
    test_window_.reset();
    root_filter_.RemoveHandler(&test_filter_);
    root_filter_.RemoveHandler(input_method_event_filter_.get());
    root_window()->RemovePreTargetHandler(&root_filter_);

    input_method_event_filter_.reset();
    test_input_client_.reset();
    aura::test::AuraTestBase::TearDown();
  }

 protected:
  CompoundEventFilter root_filter_;
  ui::test::TestEventHandler test_filter_;
  scoped_ptr<InputMethodEventFilter> input_method_event_filter_;
  aura::test::TestWindowDelegate test_window_delegate_;
  scoped_ptr<aura::Window> test_window_;
  scoped_ptr<TestTextInputClient> test_input_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodEventFilterTest);
};

TEST_F(InputMethodEventFilterTest, TestInputMethodProperty) {
  // Tests if InputMethodEventFilter adds a window property on its
  // construction.
  EXPECT_TRUE(root_window()->GetProperty(
      aura::client::kRootWindowInputMethodKey));
}

// Tests if InputMethodEventFilter dispatches a ui::ET_TRANSLATED_KEY_* event to
// the root window.
TEST_F(InputMethodEventFilterTest, TestInputMethodKeyEventPropagation) {
  // Send a fake key event to the root window. InputMethodEventFilter, which is
  // automatically set up by AshTestBase, consumes it and sends a new
  // ui::ET_TRANSLATED_KEY_* event to the root window, which will be consumed by
  // the test event filter.
  aura::test::EventGenerator generator(root_window());
  EXPECT_EQ(0, test_filter_.num_key_events());
  generator.PressKey(ui::VKEY_SPACE, 0);
  EXPECT_EQ(1, test_filter_.num_key_events());
  generator.ReleaseKey(ui::VKEY_SPACE, 0);
  EXPECT_EQ(2, test_filter_.num_key_events());
}

TEST_F(InputMethodEventFilterTest, TestEventDispatching) {
  ui::KeyEvent evt(ui::ET_KEY_PRESSED,
                   ui::VKEY_PROCESSKEY,
                   ui::EF_IME_FABRICATED_KEY,
                   false);
  // Calls DispatchKeyEventPostIME() without a focused text input client.
  if (switches::IsTextInputFocusManagerEnabled())
    ui::TextInputFocusManager::GetInstance()->FocusTextInputClient(NULL);
  else
    input_method_event_filter_->input_method()->SetFocusedTextInputClient(NULL);
  input_method_event_filter_->input_method()->DispatchKeyEvent(evt);
  // Verifies 0 key event happened because InputMethodEventFilter::
  // DispatchKeyEventPostIME() returns false.
  EXPECT_EQ(0, test_filter_.num_key_events());

  // Calls DispatchKeyEventPostIME() with a focused text input client.
  if (switches::IsTextInputFocusManagerEnabled()) {
    ui::TextInputFocusManager::GetInstance()->FocusTextInputClient(
        test_input_client_.get());
  } else {
    input_method_event_filter_->input_method()->SetFocusedTextInputClient(
        test_input_client_.get());
  }
  input_method_event_filter_->input_method()->DispatchKeyEvent(evt);
  // Verifies 1 key event happened because InputMethodEventFilter::
  // DispatchKeyEventPostIME() returns true.
  EXPECT_EQ(1, test_filter_.num_key_events());
}

}  // namespace wm
