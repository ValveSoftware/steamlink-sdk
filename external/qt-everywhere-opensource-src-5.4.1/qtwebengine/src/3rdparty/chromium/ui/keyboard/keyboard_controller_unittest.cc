// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_controller.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_focus_manager.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/keyboard/keyboard_controller_proxy.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/wm/core/default_activation_client.h"

namespace keyboard {
namespace {

// Steps a layer animation until it is completed. Animations must be enabled.
void RunAnimationForLayer(ui::Layer* layer) {
  // Animations must be enabled for stepping to work.
  ASSERT_NE(ui::ScopedAnimationDurationScaleMode::duration_scale_mode(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  ui::LayerAnimatorTestController controller(layer->GetAnimator());
  // Multiple steps are required to complete complex animations.
  // TODO(vollick): This should not be necessary. crbug.com/154017
  while (controller.animator()->is_animating()) {
    controller.StartThreadedAnimationsIfNeeded();
    base::TimeTicks step_time = controller.animator()->last_step_time();
    controller.animator()->Step(step_time +
                                base::TimeDelta::FromMilliseconds(1000));
  }
}

// An event handler that focuses a window when it is clicked/touched on. This is
// used to match the focus manger behaviour in ash and views.
class TestFocusController : public ui::EventHandler {
 public:
  explicit TestFocusController(aura::Window* root)
      : root_(root) {
    root_->AddPreTargetHandler(this);
  }

  virtual ~TestFocusController() {
    root_->RemovePreTargetHandler(this);
  }

 private:
  // Overridden from ui::EventHandler:
  virtual void OnEvent(ui::Event* event) OVERRIDE {
    aura::Window* target = static_cast<aura::Window*>(event->target());
    if (event->type() == ui::ET_MOUSE_PRESSED ||
        event->type() == ui::ET_TOUCH_PRESSED) {
      aura::client::GetFocusClient(target)->FocusWindow(target);
    }
  }

  aura::Window* root_;
  DISALLOW_COPY_AND_ASSIGN(TestFocusController);
};

class TestKeyboardControllerProxy : public KeyboardControllerProxy {
 public:
  TestKeyboardControllerProxy()
      : input_method_(
            ui::CreateInputMethod(NULL, gfx::kNullAcceleratedWidget)) {}

  virtual ~TestKeyboardControllerProxy() {
    // Destroy the window before the delegate.
    window_.reset();
  }

  // Overridden from KeyboardControllerProxy:
  virtual bool HasKeyboardWindow() const OVERRIDE { return window_; }
  virtual aura::Window* GetKeyboardWindow() OVERRIDE {
    if (!window_) {
      window_.reset(new aura::Window(&delegate_));
      window_->Init(aura::WINDOW_LAYER_NOT_DRAWN);
      window_->set_owned_by_parent(false);
    }
    return window_.get();
  }
  virtual content::BrowserContext* GetBrowserContext() OVERRIDE { return NULL; }
  virtual ui::InputMethod* GetInputMethod() OVERRIDE {
    return input_method_.get();
  }
  virtual void RequestAudioInput(content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) OVERRIDE { return; }
  virtual void LoadSystemKeyboard() OVERRIDE {};
  virtual void ReloadKeyboardIfNeeded() OVERRIDE {};

 private:
  scoped_ptr<aura::Window> window_;
  aura::test::TestWindowDelegate delegate_;
  scoped_ptr<ui::InputMethod> input_method_;

  DISALLOW_COPY_AND_ASSIGN(TestKeyboardControllerProxy);
};

// Keeps a count of all the events a window receives.
class EventObserver : public ui::EventHandler {
 public:
  EventObserver() {}
  virtual ~EventObserver() {}

  int GetEventCount(ui::EventType type) {
    return event_counts_[type];
  }

 private:
  // Overridden from ui::EventHandler:
  virtual void OnEvent(ui::Event* event) OVERRIDE {
    ui::EventHandler::OnEvent(event);
    event_counts_[event->type()]++;
  }

  std::map<ui::EventType, int> event_counts_;
  DISALLOW_COPY_AND_ASSIGN(EventObserver);
};

class KeyboardContainerObserver : public aura::WindowObserver {
 public:
  explicit KeyboardContainerObserver(aura::Window* window) : window_(window) {
    window_->AddObserver(this);
  }
  virtual ~KeyboardContainerObserver() {
    window_->RemoveObserver(this);
  }

 private:
  virtual void OnWindowVisibilityChanged(aura::Window* window,
                                         bool visible) OVERRIDE {
    if (!visible)
      base::MessageLoop::current()->Quit();
  }

  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardContainerObserver);
};

}  // namespace

class KeyboardControllerTest : public testing::Test {
 public:
  KeyboardControllerTest() {}
  virtual ~KeyboardControllerTest() {}

  virtual void SetUp() OVERRIDE {
    // The ContextFactory must exist before any Compositors are created.
    bool enable_pixel_output = false;
    ui::ContextFactory* context_factory =
        ui::InitializeContextFactoryForTests(enable_pixel_output);

    aura_test_helper_.reset(new aura::test::AuraTestHelper(&message_loop_));
    aura_test_helper_->SetUp(context_factory);
    new wm::DefaultActivationClient(aura_test_helper_->root_window());
    ui::SetUpInputMethodFactoryForTesting();
    if (::switches::IsTextInputFocusManagerEnabled())
      ui::TextInputFocusManager::GetInstance()->FocusTextInputClient(NULL);
    focus_controller_.reset(new TestFocusController(root_window()));
    proxy_ = new TestKeyboardControllerProxy();
    controller_.reset(new KeyboardController(proxy_));
  }

  virtual void TearDown() OVERRIDE {
    controller_.reset();
    focus_controller_.reset();
    if (::switches::IsTextInputFocusManagerEnabled())
      ui::TextInputFocusManager::GetInstance()->FocusTextInputClient(NULL);
    aura_test_helper_->TearDown();
    ui::TerminateContextFactoryForTests();
  }

  aura::Window* root_window() { return aura_test_helper_->root_window(); }
  KeyboardControllerProxy* proxy() { return proxy_; }
  KeyboardController* controller() { return controller_.get(); }

  void ShowKeyboard() {
    test_text_input_client_.reset(
        new ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_TEXT));
    SetFocus(test_text_input_client_.get());
  }

 protected:
  void SetFocus(ui::TextInputClient* client) {
    ui::InputMethod* input_method = proxy()->GetInputMethod();
    if (::switches::IsTextInputFocusManagerEnabled()) {
      ui::TextInputFocusManager::GetInstance()->FocusTextInputClient(client);
      input_method->OnTextInputTypeChanged(client);
    } else {
      input_method->SetFocusedTextInputClient(client);
    }
    if (client && client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE) {
      input_method->ShowImeIfNeeded();
      if (proxy_->GetKeyboardWindow()->bounds().height() == 0) {
        // Set initial bounds for test keyboard window.
        proxy_->GetKeyboardWindow()->SetBounds(
            KeyboardBoundsFromWindowBounds(
                controller()->GetContainerWindow()->bounds(), 100));
      }
    }
  }

  bool WillHideKeyboard() {
    return controller_->WillHideKeyboard();
  }

  base::MessageLoopForUI message_loop_;
  scoped_ptr<aura::test::AuraTestHelper> aura_test_helper_;
  scoped_ptr<TestFocusController> focus_controller_;

 private:
  KeyboardControllerProxy* proxy_;
  scoped_ptr<KeyboardController> controller_;
  scoped_ptr<ui::TextInputClient> test_text_input_client_;
  DISALLOW_COPY_AND_ASSIGN(KeyboardControllerTest);
};

TEST_F(KeyboardControllerTest, KeyboardSize) {
  aura::Window* container(controller()->GetContainerWindow());
  aura::Window* keyboard(proxy()->GetKeyboardWindow());
  container->SetBounds(gfx::Rect(0, 0, 200, 100));

  container->AddChild(keyboard);
  const gfx::Rect& before_bounds = keyboard->bounds();
  // The initial keyboard should be positioned at the bottom of container and
  // has 0 height.
  ASSERT_EQ(gfx::Rect(0, 100, 200, 0), before_bounds);

  gfx::Rect new_bounds(
      before_bounds.x(), before_bounds.y() - 50,
      before_bounds.width(), 50);

  keyboard->SetBounds(new_bounds);
  ASSERT_EQ(new_bounds, keyboard->bounds());

  // Mock a screen rotation.
  container->SetBounds(gfx::Rect(0, 0, 100, 200));
  // The above call should resize keyboard to new width while keeping the old
  // height.
  ASSERT_EQ(gfx::Rect(0, 150, 100, 50), keyboard->bounds());
}

// Tests that tapping/clicking inside the keyboard does not give it focus.
TEST_F(KeyboardControllerTest, ClickDoesNotFocusKeyboard) {
  const gfx::Rect& root_bounds = root_window()->bounds();
  aura::test::EventCountDelegate delegate;
  scoped_ptr<aura::Window> window(new aura::Window(&delegate));
  window->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  window->SetBounds(root_bounds);
  root_window()->AddChild(window.get());
  window->Show();
  window->Focus();

  aura::Window* keyboard_container(controller()->GetContainerWindow());
  keyboard_container->SetBounds(root_bounds);

  root_window()->AddChild(keyboard_container);
  keyboard_container->Show();

  ShowKeyboard();

  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(keyboard_container->IsVisible());
  EXPECT_TRUE(window->HasFocus());
  EXPECT_FALSE(keyboard_container->HasFocus());

  // Click on the keyboard. Make sure the keyboard receives the event, but does
  // not get focus.
  EventObserver observer;
  keyboard_container->AddPreTargetHandler(&observer);

  aura::test::EventGenerator generator(root_window());
  generator.MoveMouseTo(proxy()->GetKeyboardWindow()->bounds().CenterPoint());
  generator.ClickLeftButton();
  EXPECT_TRUE(window->HasFocus());
  EXPECT_FALSE(keyboard_container->HasFocus());
  EXPECT_EQ("0 0", delegate.GetMouseButtonCountsAndReset());
  EXPECT_EQ(1, observer.GetEventCount(ui::ET_MOUSE_PRESSED));
  EXPECT_EQ(1, observer.GetEventCount(ui::ET_MOUSE_RELEASED));

  // Click outside of the keyboard. It should reach the window behind.
  generator.MoveMouseTo(gfx::Point());
  generator.ClickLeftButton();
  EXPECT_EQ("1 1", delegate.GetMouseButtonCountsAndReset());
  keyboard_container->RemovePreTargetHandler(&observer);
}

TEST_F(KeyboardControllerTest, EventHitTestingInContainer) {
  const gfx::Rect& root_bounds = root_window()->bounds();
  aura::test::EventCountDelegate delegate;
  scoped_ptr<aura::Window> window(new aura::Window(&delegate));
  window->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  window->SetBounds(root_bounds);
  root_window()->AddChild(window.get());
  window->Show();
  window->Focus();

  aura::Window* keyboard_container(controller()->GetContainerWindow());
  keyboard_container->SetBounds(root_bounds);

  root_window()->AddChild(keyboard_container);
  keyboard_container->Show();

  ShowKeyboard();

  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(keyboard_container->IsVisible());
  EXPECT_TRUE(window->HasFocus());
  EXPECT_FALSE(keyboard_container->HasFocus());

  // Make sure hit testing works correctly while the keyboard is visible.
  aura::Window* keyboard_window = proxy()->GetKeyboardWindow();
  ui::EventTarget* root = root_window();
  ui::EventTargeter* targeter = root->GetEventTargeter();
  gfx::Point location = keyboard_window->bounds().CenterPoint();
  ui::MouseEvent mouse1(ui::ET_MOUSE_MOVED, location, location, ui::EF_NONE,
                        ui::EF_NONE);
  EXPECT_EQ(keyboard_window, targeter->FindTargetForEvent(root, &mouse1));

  location.set_y(keyboard_window->bounds().y() - 5);
  ui::MouseEvent mouse2(ui::ET_MOUSE_MOVED, location, location, ui::EF_NONE,
                        ui::EF_NONE);
  EXPECT_EQ(window.get(), targeter->FindTargetForEvent(root, &mouse2));
}

TEST_F(KeyboardControllerTest, KeyboardWindowCreation) {
  const gfx::Rect& root_bounds = root_window()->bounds();
  aura::test::EventCountDelegate delegate;
  scoped_ptr<aura::Window> window(new aura::Window(&delegate));
  window->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  window->SetBounds(root_bounds);
  root_window()->AddChild(window.get());
  window->Show();
  window->Focus();

  aura::Window* keyboard_container(controller()->GetContainerWindow());
  keyboard_container->SetBounds(root_bounds);

  root_window()->AddChild(keyboard_container);
  keyboard_container->Show();

  EXPECT_FALSE(proxy()->HasKeyboardWindow());

  ui::EventTarget* root = root_window();
  ui::EventTargeter* targeter = root->GetEventTargeter();
  gfx::Point location(root_window()->bounds().width() / 2,
                      root_window()->bounds().height() - 10);
  ui::MouseEvent mouse(
      ui::ET_MOUSE_MOVED, location, location, ui::EF_NONE, ui::EF_NONE);
  EXPECT_EQ(window.get(), targeter->FindTargetForEvent(root, &mouse));
  EXPECT_FALSE(proxy()->HasKeyboardWindow());

  EXPECT_EQ(
      controller()->GetContainerWindow(),
      controller()->GetContainerWindow()->GetEventHandlerForPoint(location));
  EXPECT_FALSE(proxy()->HasKeyboardWindow());
}

TEST_F(KeyboardControllerTest, VisibilityChangeWithTextInputTypeChange) {
  const gfx::Rect& root_bounds = root_window()->bounds();

  ui::DummyTextInputClient input_client_0(ui::TEXT_INPUT_TYPE_TEXT);
  ui::DummyTextInputClient input_client_1(ui::TEXT_INPUT_TYPE_TEXT);
  ui::DummyTextInputClient input_client_2(ui::TEXT_INPUT_TYPE_TEXT);
  ui::DummyTextInputClient no_input_client_0(ui::TEXT_INPUT_TYPE_NONE);
  ui::DummyTextInputClient no_input_client_1(ui::TEXT_INPUT_TYPE_NONE);

  aura::Window* keyboard_container(controller()->GetContainerWindow());
  scoped_ptr<KeyboardContainerObserver> keyboard_container_observer(
      new KeyboardContainerObserver(keyboard_container));
  keyboard_container->SetBounds(root_bounds);
  root_window()->AddChild(keyboard_container);

  SetFocus(&input_client_0);

  EXPECT_TRUE(keyboard_container->IsVisible());

  SetFocus(&no_input_client_0);
  // Keyboard should not immediately hide itself. It is delayed to avoid layout
  // flicker when the focus of input field quickly change.
  EXPECT_TRUE(keyboard_container->IsVisible());
  EXPECT_TRUE(WillHideKeyboard());
  // Wait for hide keyboard to finish.
  base::MessageLoop::current()->Run();
  EXPECT_FALSE(keyboard_container->IsVisible());

  SetFocus(&input_client_1);
  EXPECT_TRUE(keyboard_container->IsVisible());

  // Schedule to hide keyboard.
  SetFocus(&no_input_client_1);
  EXPECT_TRUE(WillHideKeyboard());
  // Cancel keyboard hide.
  SetFocus(&input_client_2);

  EXPECT_FALSE(WillHideKeyboard());
  EXPECT_TRUE(keyboard_container->IsVisible());
}

TEST_F(KeyboardControllerTest, AlwaysVisibleWhenLocked) {
  const gfx::Rect& root_bounds = root_window()->bounds();

  ui::DummyTextInputClient input_client_0(ui::TEXT_INPUT_TYPE_TEXT);
  ui::DummyTextInputClient input_client_1(ui::TEXT_INPUT_TYPE_TEXT);
  ui::DummyTextInputClient no_input_client_0(ui::TEXT_INPUT_TYPE_NONE);
  ui::DummyTextInputClient no_input_client_1(ui::TEXT_INPUT_TYPE_NONE);

  aura::Window* keyboard_container(controller()->GetContainerWindow());
  scoped_ptr<KeyboardContainerObserver> keyboard_container_observer(
      new KeyboardContainerObserver(keyboard_container));
  keyboard_container->SetBounds(root_bounds);
  root_window()->AddChild(keyboard_container);

  SetFocus(&input_client_0);

  EXPECT_TRUE(keyboard_container->IsVisible());

  // Lock keyboard.
  controller()->set_lock_keyboard(true);

  SetFocus(&no_input_client_0);
  // Keyboard should not try to hide itself as it is locked.
  EXPECT_TRUE(keyboard_container->IsVisible());
  EXPECT_FALSE(WillHideKeyboard());

  SetFocus(&input_client_1);
  EXPECT_TRUE(keyboard_container->IsVisible());

  // Unlock keyboard.
  controller()->set_lock_keyboard(false);

  // Keyboard should hide when focus on no input client.
  SetFocus(&no_input_client_1);
  EXPECT_TRUE(WillHideKeyboard());

  // Wait for hide keyboard to finish.
  base::MessageLoop::current()->Run();
  EXPECT_FALSE(keyboard_container->IsVisible());
}

class KeyboardControllerAnimationTest : public KeyboardControllerTest,
                                        public KeyboardControllerObserver {
 public:
  KeyboardControllerAnimationTest() {}
  virtual ~KeyboardControllerAnimationTest() {}

  virtual void SetUp() OVERRIDE {
    // We cannot short-circuit animations for this test.
    ui::ScopedAnimationDurationScaleMode normal_duration_mode(
        ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

    KeyboardControllerTest::SetUp();

    const gfx::Rect& root_bounds = root_window()->bounds();
    keyboard_container()->SetBounds(root_bounds);
    root_window()->AddChild(keyboard_container());
    controller()->AddObserver(this);
  }

  virtual void TearDown() OVERRIDE {
    controller()->RemoveObserver(this);
    KeyboardControllerTest::TearDown();
  }

 protected:
  // KeyboardControllerObserver overrides
  virtual void OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) OVERRIDE {
    notified_bounds_ = new_bounds;
  }

  const gfx::Rect& notified_bounds() { return notified_bounds_; }

  aura::Window* keyboard_container() {
    return controller()->GetContainerWindow();
  }

  aura::Window* keyboard_window() {
    return proxy()->GetKeyboardWindow();
  }

 private:
  gfx::Rect notified_bounds_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardControllerAnimationTest);
};

// Tests virtual keyboard has correct show and hide animation.
TEST_F(KeyboardControllerAnimationTest, ContainerAnimation) {
  ui::Layer* layer = keyboard_container()->layer();
  ShowKeyboard();

  // Keyboard container and window should immediately become visible before
  // animation starts.
  EXPECT_TRUE(keyboard_container()->IsVisible());
  EXPECT_TRUE(keyboard_window()->IsVisible());
  float show_start_opacity = layer->opacity();
  gfx::Transform transform;
  transform.Translate(0, keyboard_window()->bounds().height());
  EXPECT_EQ(transform, layer->transform());
  EXPECT_EQ(gfx::Rect(), notified_bounds());

  RunAnimationForLayer(layer);
  EXPECT_TRUE(keyboard_container()->IsVisible());
  EXPECT_TRUE(keyboard_window()->IsVisible());
  float show_end_opacity = layer->opacity();
  EXPECT_LT(show_start_opacity, show_end_opacity);
  EXPECT_EQ(gfx::Transform(), layer->transform());
  // KeyboardController should notify the bounds of keyboard window to its
  // observers after show animation finished.
  EXPECT_EQ(keyboard_window()->bounds(), notified_bounds());

  // Directly hide keyboard without delay.
  controller()->HideKeyboard(KeyboardController::HIDE_REASON_AUTOMATIC);
  EXPECT_TRUE(keyboard_container()->IsVisible());
  EXPECT_TRUE(keyboard_container()->layer()->visible());
  EXPECT_TRUE(keyboard_window()->IsVisible());
  float hide_start_opacity = layer->opacity();
  // KeyboardController should notify the bounds of keyboard window to its
  // observers before hide animation starts.
  EXPECT_EQ(gfx::Rect(), notified_bounds());

  RunAnimationForLayer(layer);
  EXPECT_FALSE(keyboard_container()->IsVisible());
  EXPECT_FALSE(keyboard_container()->layer()->visible());
  EXPECT_FALSE(keyboard_window()->IsVisible());
  float hide_end_opacity = layer->opacity();
  EXPECT_GT(hide_start_opacity, hide_end_opacity);
  EXPECT_EQ(transform, layer->transform());
  EXPECT_EQ(gfx::Rect(), notified_bounds());
}

// Show keyboard during keyboard hide animation should abort the hide animation
// and the keyboard should animate in.
// Test for crbug.com/333284.
TEST_F(KeyboardControllerAnimationTest, ContainerShowWhileHide) {
  ui::Layer* layer = keyboard_container()->layer();
  ShowKeyboard();
  RunAnimationForLayer(layer);

  controller()->HideKeyboard(KeyboardController::HIDE_REASON_AUTOMATIC);
  // Before hide animation finishes, show keyboard again.
  ShowKeyboard();
  RunAnimationForLayer(layer);
  EXPECT_TRUE(keyboard_container()->IsVisible());
  EXPECT_TRUE(keyboard_window()->IsVisible());
  EXPECT_EQ(1.0, layer->opacity());
  EXPECT_EQ(gfx::Transform(), layer->transform());
}

class KeyboardControllerUsabilityTest : public KeyboardControllerTest {
 public:
  KeyboardControllerUsabilityTest() {}
  virtual ~KeyboardControllerUsabilityTest() {}

  virtual void SetUp() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kKeyboardUsabilityExperiment);
    KeyboardControllerTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardControllerUsabilityTest);
};

TEST_F(KeyboardControllerUsabilityTest, KeyboardAlwaysVisibleInUsabilityTest) {
  const gfx::Rect& root_bounds = root_window()->bounds();

  ui::DummyTextInputClient input_client(ui::TEXT_INPUT_TYPE_TEXT);
  ui::DummyTextInputClient no_input_client(ui::TEXT_INPUT_TYPE_NONE);

  aura::Window* keyboard_container(controller()->GetContainerWindow());
  keyboard_container->SetBounds(root_bounds);
  root_window()->AddChild(keyboard_container);

  SetFocus(&input_client);
  EXPECT_TRUE(keyboard_container->IsVisible());

  SetFocus(&no_input_client);
  // Keyboard should not hide itself after lost focus.
  EXPECT_TRUE(keyboard_container->IsVisible());
  EXPECT_FALSE(WillHideKeyboard());
}

}  // namespace keyboard
