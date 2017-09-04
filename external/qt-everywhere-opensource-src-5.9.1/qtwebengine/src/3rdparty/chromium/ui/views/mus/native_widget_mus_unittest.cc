// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "base/callback.h"
#include "base/macros.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/cpp/tests/window_tree_client_private.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_observer.h"
#include "services/ui/public/cpp/window_property.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/test/test_event_handler.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/test/focus_manager_test.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/public/activation_client.h"

using ui::mojom::EventResult;

namespace views {
namespace {

// A view that reports any mouse press as handled.
class HandleMousePressView : public View {
 public:
  HandleMousePressView() {}
  ~HandleMousePressView() override {}

  // View:
  bool OnMousePressed(const ui::MouseEvent& event) override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(HandleMousePressView);
};

// A view that deletes a widget on mouse press.
class DeleteWidgetView : public View {
 public:
  explicit DeleteWidgetView(std::unique_ptr<Widget>* widget_ptr)
      : widget_ptr_(widget_ptr) {}
  ~DeleteWidgetView() override {}

  // View:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    widget_ptr_->reset();
    return true;
  }

 private:
  std::unique_ptr<Widget>* widget_ptr_;
  DISALLOW_COPY_AND_ASSIGN(DeleteWidgetView);
};

// Returns a small colored bitmap.
SkBitmap MakeBitmap(SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(8, 8);
  bitmap.eraseColor(color);
  return bitmap;
}

// An observer that tracks widget activation changes.
class WidgetActivationObserver : public WidgetObserver {
 public:
  explicit WidgetActivationObserver(Widget* widget) : widget_(widget) {
    widget_->AddObserver(this);
  }

  ~WidgetActivationObserver() override {
    widget_->RemoveObserver(this);
  }

  const std::vector<bool>& changes() const { return changes_; }

  // WidgetObserver:
  void OnWidgetActivationChanged(Widget* widget, bool active) override {
    ASSERT_EQ(widget_, widget);
    changes_.push_back(active);
  }

 private:
  Widget* widget_;
  std::vector<bool> changes_;

  DISALLOW_COPY_AND_ASSIGN(WidgetActivationObserver);
};

// A WidgetDelegate that supplies an app icon.
class TestWidgetDelegate : public WidgetDelegateView {
 public:
  explicit TestWidgetDelegate(const SkBitmap& icon)
      : app_icon_(gfx::ImageSkia::CreateFrom1xBitmap(icon)) {}

  ~TestWidgetDelegate() override {}

  void SetIcon(const SkBitmap& icon) {
    app_icon_ = gfx::ImageSkia::CreateFrom1xBitmap(icon);
  }

  // views::WidgetDelegate:
  gfx::ImageSkia GetWindowAppIcon() override { return app_icon_; }

 private:
  gfx::ImageSkia app_icon_;

  DISALLOW_COPY_AND_ASSIGN(TestWidgetDelegate);
};

class WidgetDelegateWithHitTestMask : public WidgetDelegateView {
 public:
  explicit WidgetDelegateWithHitTestMask(const gfx::Rect& mask_rect)
      : mask_rect_(mask_rect) {}

  ~WidgetDelegateWithHitTestMask() override {}

  // views::WidgetDelegate:
  bool WidgetHasHitTestMask() const override { return true; }
  void GetWidgetHitTestMask(gfx::Path* mask) const override {
    mask->addRect(gfx::RectToSkRect(mask_rect_));
  }

 private:
  gfx::Rect mask_rect_;

  DISALLOW_COPY_AND_ASSIGN(WidgetDelegateWithHitTestMask);
};

}  // namespace

class NativeWidgetMusTest : public ViewsTestBase {
 public:
  NativeWidgetMusTest() {}
  ~NativeWidgetMusTest() override {}

  // Creates a test widget. Takes ownership of |delegate|.
  std::unique_ptr<Widget> CreateWidget(WidgetDelegate* delegate) {
    std::unique_ptr<Widget> widget(new Widget());
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.delegate = delegate;
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = initial_bounds();
    widget->Init(params);
    return widget;
  }

  int ack_callback_count() { return ack_callback_count_; }

  void AckCallback(ui::mojom::EventResult result) {
    ack_callback_count_++;
    EXPECT_EQ(ui::mojom::EventResult::HANDLED, result);
  }

  // Returns a mouse pressed event inside the widget. Tests that place views
  // within the widget that respond to the event must be constructed within the
  // widget coordinate space such that they respond correctly.
  std::unique_ptr<ui::MouseEvent> CreateMouseEvent() {
    return base::MakeUnique<ui::MouseEvent>(
        ui::ET_MOUSE_PRESSED, gfx::Point(50, 50), gfx::Point(50, 50),
        base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  }

  // Simulates an input event to the NativeWidget.
  void OnWindowInputEvent(
      NativeWidgetMus* native_widget,
      const ui::Event& event,
      std::unique_ptr<base::Callback<void(ui::mojom::EventResult)>>*
          ack_callback) {
    native_widget->OnWindowInputEvent(native_widget->window(), event,
                                      ack_callback);
  }

 protected:
  gfx::Rect initial_bounds() { return gfx::Rect(10, 20, 100, 200); }

 private:
  int ack_callback_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetMusTest);
};

// Tests communication of activation and focus between Widget and
// NativeWidgetMus.
TEST_F(NativeWidgetMusTest, OnActivationChanged) {
  std::unique_ptr<Widget> widget(CreateWidget(nullptr));
  widget->Show();

  // Track activation, focus and blur events.
  WidgetActivationObserver activation_observer(widget.get());
  TestWidgetFocusChangeListener focus_listener;
  WidgetFocusManager::GetInstance()->AddFocusChangeListener(&focus_listener);

  // Deactivate the Widget, which deactivates the NativeWidgetMus.
  widget->Deactivate();

  // The widget is blurred and deactivated.
  ASSERT_EQ(1u, focus_listener.focus_changes().size());
  EXPECT_EQ(nullptr, focus_listener.focus_changes()[0]);
  ASSERT_EQ(1u, activation_observer.changes().size());
  EXPECT_EQ(false, activation_observer.changes()[0]);

  // Re-activate the Widget, which actives the NativeWidgetMus.
  widget->Activate();

  // The widget is focused and activated.
  ASSERT_EQ(2u, focus_listener.focus_changes().size());
  EXPECT_EQ(widget->GetNativeView(), focus_listener.focus_changes()[1]);
  ASSERT_EQ(2u, activation_observer.changes().size());
  EXPECT_TRUE(activation_observer.changes()[1]);

  WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(&focus_listener);
}

// Tests that showing a non-activatable widget does not activate it.
// TODO(jamescook): Remove this test when widget_interactive_uittests.cc runs
// under mus.
TEST_F(NativeWidgetMusTest, ShowNonActivatableWidget) {
  Widget widget;
  WidgetActivationObserver activation_observer(&widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_BUBBLE);
  params.activatable = Widget::InitParams::ACTIVATABLE_NO;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(10, 20, 100, 200);
  widget.Init(params);
  widget.Show();

  // The widget is not currently active.
  EXPECT_FALSE(widget.IsActive());

  // The widget was never active.
  EXPECT_EQ(0u, activation_observer.changes().size());
}

// Tests that a window with an icon sets the ui::Window icon property.
TEST_F(NativeWidgetMusTest, AppIcon) {
  // Create a Widget with a bitmap as the icon.
  SkBitmap source_bitmap = MakeBitmap(SK_ColorRED);
  std::unique_ptr<Widget> widget(
      CreateWidget(new TestWidgetDelegate(source_bitmap)));

  // The ui::Window has the icon property.
  ui::Window* window =
      static_cast<NativeWidgetMus*>(widget->native_widget_private())->window();
  EXPECT_TRUE(window->HasSharedProperty(
      ui::mojom::WindowManager::kWindowAppIcon_Property));

  // The icon is the expected icon.
  SkBitmap icon = window->GetSharedProperty<SkBitmap>(
      ui::mojom::WindowManager::kWindowAppIcon_Property);
  EXPECT_TRUE(gfx::BitmapsAreEqual(source_bitmap, icon));
}

// Tests that a window without an icon does not set the ui::Window icon
// property.
TEST_F(NativeWidgetMusTest, NoAppIcon) {
  // Create a Widget without a special icon.
  std::unique_ptr<Widget> widget(CreateWidget(nullptr));

  // The ui::Window does not have an icon property.
  ui::Window* window =
      static_cast<NativeWidgetMus*>(widget->native_widget_private())->window();
  EXPECT_FALSE(window->HasSharedProperty(
      ui::mojom::WindowManager::kWindowAppIcon_Property));
}

// Tests that changing the icon on a Widget updates the ui::Window icon
// property.
TEST_F(NativeWidgetMusTest, ChangeAppIcon) {
  // Create a Widget with an icon.
  SkBitmap bitmap1 = MakeBitmap(SK_ColorRED);
  TestWidgetDelegate* delegate = new TestWidgetDelegate(bitmap1);
  std::unique_ptr<Widget> widget(CreateWidget(delegate));

  // Update the icon to a new image.
  SkBitmap bitmap2 = MakeBitmap(SK_ColorGREEN);
  delegate->SetIcon(bitmap2);
  widget->UpdateWindowIcon();

  // The window has the updated icon.
  ui::Window* window =
      static_cast<NativeWidgetMus*>(widget->native_widget_private())->window();
  SkBitmap icon = window->GetSharedProperty<SkBitmap>(
      ui::mojom::WindowManager::kWindowAppIcon_Property);
  EXPECT_TRUE(gfx::BitmapsAreEqual(bitmap2, icon));
}

TEST_F(NativeWidgetMusTest, ValidLayerTree) {
  std::unique_ptr<Widget> widget(CreateWidget(nullptr));
  View* content = new View;
  content->SetPaintToLayer(true);
  widget->GetContentsView()->AddChildView(content);
  EXPECT_TRUE(widget->GetNativeWindow()->layer()->Contains(content->layer()));
}

// Tests that the internal name is propagated from the Widget to the
// ui::Window.
TEST_F(NativeWidgetMusTest, GetName) {
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.name = "MyWidget";
  widget.Init(params);
  ui::Window* window =
      static_cast<NativeWidgetMus*>(widget.native_widget_private())->window();
  EXPECT_EQ("MyWidget", window->GetName());
}

// Tests that a Widget with a hit test mask propagates the mask to the
// ui::Window.
TEST_F(NativeWidgetMusTest, HitTestMask) {
  gfx::Rect mask(5, 5, 10, 10);
  std::unique_ptr<Widget> widget(
      CreateWidget(new WidgetDelegateWithHitTestMask(mask)));

  // The window has the mask.
  ui::Window* window =
      static_cast<NativeWidgetMus*>(widget->native_widget_private())->window();
  ASSERT_TRUE(window->hit_test_mask());
  EXPECT_EQ(mask.ToString(), window->hit_test_mask()->ToString());
}

// Verifies changing the visibility of a child ui::Window doesn't change the
// visibility of the parent.
TEST_F(NativeWidgetMusTest, ChildVisibilityDoesntEffectParent) {
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(params);
  widget.Show();
  ui::Window* window =
      static_cast<NativeWidgetMus*>(widget.native_widget_private())->window();
  ASSERT_TRUE(window->visible());

  // Create a child window, make it visible and parent it to the Widget's
  // window.
  ui::Window* child_window = window->window_tree()->NewWindow();
  child_window->SetVisible(true);
  window->AddChild(child_window);

  // Hide the child, this should not impact the visibility of the parent.
  child_window->SetVisible(false);
  EXPECT_TRUE(window->visible());
}

// Tests that child aura::Windows cannot be activated.
TEST_F(NativeWidgetMusTest, FocusChildAuraWindow) {
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(params);

  View* focusable = new View;
  focusable->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  widget.GetContentsView()->AddChildView(focusable);

  NativeViewHost* native_host = new NativeViewHost;
  widget.GetContentsView()->AddChildView(native_host);

  std::unique_ptr<aura::Window> window(new aura::Window(nullptr));
  window->Init(ui::LayerType::LAYER_SOLID_COLOR);
  native_host->SetBounds(5, 10, 20, 30);
  native_host->Attach(window.get());
  widget.Show();
  window->Show();
  widget.SetBounds(gfx::Rect(10, 20, 30, 40));

  // Sanity check that the |window| is a descendent of the Widget's window.
  ASSERT_TRUE(widget.GetNativeView()->Contains(window->parent()));

  // Focusing the child window should not activate it.
  window->Focus();
  EXPECT_TRUE(window->HasFocus());
  aura::Window* active_window =
      aura::client::GetActivationClient(window.get()->GetRootWindow())
          ->GetActiveWindow();
  EXPECT_NE(window.get(), active_window);
  EXPECT_EQ(widget.GetNativeView(), active_window);

  // Moving focus to a child View should move focus away from |window|, and to
  // the Widget's window instead.
  focusable->RequestFocus();
  EXPECT_FALSE(window->HasFocus());
  EXPECT_TRUE(widget.GetNativeView()->HasFocus());
  active_window =
      aura::client::GetActivationClient(window.get()->GetRootWindow())
          ->GetActiveWindow();
  EXPECT_EQ(widget.GetNativeView(), active_window);
}

TEST_F(NativeWidgetMusTest, WidgetReceivesEvent) {
  std::unique_ptr<Widget> widget(CreateWidget(nullptr));
  widget->Show();

  View* content = new HandleMousePressView;
  content->SetBounds(10, 20, 90, 180);
  widget->GetContentsView()->AddChildView(content);

  ui::test::TestEventHandler handler;
  content->AddPreTargetHandler(&handler);

  std::unique_ptr<ui::MouseEvent> mouse = CreateMouseEvent();
  NativeWidgetMus* native_widget =
      static_cast<NativeWidgetMus*>(widget->native_widget_private());
  ui::WindowTreeClientPrivate test_api(native_widget->window());
  test_api.CallOnWindowInputEvent(native_widget->window(), std::move(mouse));
  EXPECT_EQ(1, handler.num_mouse_events());
}

// Tests that an incoming UI event is acked with the handled status.
TEST_F(NativeWidgetMusTest, EventAcked) {
  std::unique_ptr<Widget> widget(CreateWidget(nullptr));
  widget->Show();

  View* content = new HandleMousePressView;
  content->SetBounds(10, 20, 90, 180);
  widget->GetContentsView()->AddChildView(content);

  // Dispatch an input event to the window and view.
  std::unique_ptr<ui::MouseEvent> event = CreateMouseEvent();
  std::unique_ptr<base::Callback<void(EventResult)>> ack_callback(
      new base::Callback<void(EventResult)>(base::Bind(
          &NativeWidgetMusTest::AckCallback, base::Unretained(this))));
  OnWindowInputEvent(
      static_cast<NativeWidgetMus*>(widget->native_widget_private()),
      *event,
      &ack_callback);

  // The test took ownership of the callback and called it.
  EXPECT_FALSE(ack_callback);
  EXPECT_EQ(1, ack_callback_count());
}

// Tests that a window that is deleted during event handling properly acks the
// event.
TEST_F(NativeWidgetMusTest, EventAckedWithWindowDestruction) {
  std::unique_ptr<Widget> widget(CreateWidget(nullptr));
  widget->Show();

  View* content = new DeleteWidgetView(&widget);
  content->SetBounds(10, 20, 90, 180);
  widget->GetContentsView()->AddChildView(content);

  // Dispatch an input event to the window and view.
  std::unique_ptr<ui::MouseEvent> event = CreateMouseEvent();
  std::unique_ptr<base::Callback<void(EventResult)>> ack_callback(
      new base::Callback<void(EventResult)>(base::Bind(
          &NativeWidgetMusTest::AckCallback, base::Unretained(this))));
  OnWindowInputEvent(
      static_cast<NativeWidgetMus*>(widget->native_widget_private()),
      *event,
      &ack_callback);

  // The widget was deleted.
  EXPECT_FALSE(widget.get());

  // The test took ownership of the callback and called it.
  EXPECT_FALSE(ack_callback);
  EXPECT_EQ(1, ack_callback_count());
}

TEST_F(NativeWidgetMusTest, SetAndReleaseCapture) {
  std::unique_ptr<Widget> widget(CreateWidget(nullptr));
  widget->Show();
  View* content = new View;
  widget->GetContentsView()->AddChildView(content);
  internal::NativeWidgetPrivate* widget_private =
      widget->native_widget_private();
  ui::Window* mus_window =
      static_cast<NativeWidgetMus*>(widget_private)->window();
  EXPECT_FALSE(widget_private->HasCapture());
  EXPECT_FALSE(mus_window->HasCapture());

  widget->SetCapture(content);
  EXPECT_TRUE(widget_private->HasCapture());
  EXPECT_TRUE(mus_window->HasCapture());

  widget->ReleaseCapture();
  EXPECT_FALSE(widget_private->HasCapture());
  EXPECT_FALSE(mus_window->HasCapture());
}

// Ensure that manually setting NativeWidgetMus's ui::Window bounds also
// updates its WindowTreeHost bounds.
TEST_F(NativeWidgetMusTest, SetMusWindowBounds) {
  std::unique_ptr<Widget> widget(CreateWidget(nullptr));
  widget->Show();
  View* content = new View;
  widget->GetContentsView()->AddChildView(content);
  NativeWidgetMus* native_widget =
      static_cast<NativeWidgetMus*>(widget->native_widget_private());
  ui::Window* mus_window = native_widget->window();

  gfx::Rect start_bounds = initial_bounds();
  gfx::Rect end_bounds = gfx::Rect(40, 50, 60, 70);
  EXPECT_NE(start_bounds, end_bounds);

  EXPECT_EQ(start_bounds, mus_window->bounds());
  EXPECT_EQ(start_bounds, native_widget->window_tree_host()->GetBounds());

  mus_window->SetBounds(end_bounds);

  EXPECT_EQ(end_bounds, mus_window->bounds());

  // Main check for this test: Setting |mus_window| bounds while bypassing
  // |native_widget| must update window_tree_host bounds.
  EXPECT_EQ(end_bounds, native_widget->window_tree_host()->GetBounds());
}

// Verifies visibility of the aura::Window and ui::Window are updated when the
// Widget is shown/hidden.
TEST_F(NativeWidgetMusTest, TargetVisibility) {
  std::unique_ptr<Widget> widget(CreateWidget(nullptr));
  NativeWidgetMus* native_widget =
      static_cast<NativeWidgetMus*>(widget->native_widget_private());
  ui::Window* mus_window = native_widget->window();
  EXPECT_FALSE(mus_window->visible());
  EXPECT_FALSE(widget->GetNativeView()->TargetVisibility());

  widget->Show();
  EXPECT_TRUE(mus_window->visible());
  EXPECT_TRUE(widget->GetNativeView()->TargetVisibility());
}

// Indirectly verifies Show() isn't invoked twice on the underlying
// aura::Window.
TEST_F(NativeWidgetMusTest, DontShowTwice) {
  std::unique_ptr<Widget> widget(CreateWidget(nullptr));
  widget->GetNativeView()->layer()->SetOpacity(0.0f);
  // aura::Window::Show() allows the opacity to be 0 as long as the window is
  // hidden. So, as long as this only invokes aura::Window::Show() once the
  // DCHECK in aura::Window::Show() won't fire.
  widget->Show();
}

namespace {

// See description of test for details.
class IsMaximizedObserver : public ui::WindowObserver {
 public:
  IsMaximizedObserver() {}
  ~IsMaximizedObserver() override {}

  void set_widget(Widget* widget) { widget_ = widget; }

  bool got_change() const { return got_change_; }

  // ui::WindowObserver:
  void OnWindowSharedPropertyChanged(
      ui::Window* window,
      const std::string& name,
      const std::vector<uint8_t>* old_data,
      const std::vector<uint8_t>* new_data) override {
    // Expect only one change for the show state.
    ASSERT_FALSE(got_change_);
    got_change_ = true;
    EXPECT_EQ(ui::mojom::WindowManager::kShowState_Property, name);
    EXPECT_TRUE(widget_->IsMaximized());
  }

 private:
  bool got_change_ = false;
  Widget* widget_ =  nullptr;

  DISALLOW_COPY_AND_ASSIGN(IsMaximizedObserver);
};

}  // namespace

// Verifies that asking for Widget::IsMaximized() from within
// OnWindowSharedPropertyChanged() returns the right thing.
TEST_F(NativeWidgetMusTest, IsMaximized) {
  ASSERT_TRUE(WindowManagerConnection::Exists());
  ui::Window* window = WindowManagerConnection::Get()->NewTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>());
  IsMaximizedObserver observer;
  // NOTE: the order here is important, we purposefully add the
  // ui::WindowObserver before creating NativeWidgetMus, which also adds its
  // own observer.
  window->AddObserver(&observer);

  std::unique_ptr<Widget> widget(new Widget());
  observer.set_widget(widget.get());
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = initial_bounds();
  params.native_widget = new NativeWidgetMus(
      widget.get(), window, ui::mojom::CompositorFrameSinkType::DEFAULT);
  widget->Init(params);
  window->SetSharedProperty<int32_t>(
      ui::mojom::WindowManager::kShowState_Property,
      static_cast<uint32_t>(ui::mojom::ShowState::MAXIMIZED));
  EXPECT_TRUE(widget->IsMaximized());
}

// This test is to ensure that when initializing a widget with InitParams.parent
// set to another widget's aura::Window, the ui::Window of the former widget is
// added as a child to the ui::Window of the latter widget.
TEST_F(NativeWidgetMusTest, InitNativeWidgetParentsUIWindow) {
  ASSERT_TRUE(WindowManagerConnection::Exists());

  ui::Window* parent_window = WindowManagerConnection::Get()->NewTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>());
  std::unique_ptr<Widget> parent_widget(new Widget());
  Widget::InitParams parent_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  parent_params.name = "Parent Widget";
  parent_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  parent_params.shadow_type = Widget::InitParams::SHADOW_TYPE_NONE;
  parent_params.opacity = Widget::InitParams::OPAQUE_WINDOW;
  parent_params.parent = nullptr;
  parent_params.bounds = initial_bounds();
  parent_params.native_widget =
      new NativeWidgetMus(parent_widget.get(), parent_window,
                          ui::mojom::CompositorFrameSinkType::DEFAULT);
  parent_widget->Init(parent_params);

  std::unique_ptr<Widget> child_widget(new Widget());
  ui::Window* child_window = parent_window->window_tree()->NewWindow();
  Widget::InitParams child_params = CreateParams(Widget::InitParams::TYPE_MENU);
  child_params.parent = parent_widget->GetNativeView();
  child_params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  child_params.name = "Child Widget";
  child_params.native_widget =
      new NativeWidgetMus(child_widget.get(), child_window,
                          ui::mojom::CompositorFrameSinkType::DEFAULT);
  child_widget->Init(child_params);

  EXPECT_EQ(child_window->parent(), parent_window);

  std::unique_ptr<Widget> not_child_widget(new Widget());
  ui::Window* not_child_window = parent_window->window_tree()->NewWindow();
  Widget::InitParams not_child_params =
      CreateParams(Widget::InitParams::TYPE_MENU);
  not_child_params.ownership =
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  not_child_params.name = "Not Child Widget";
  not_child_params.native_widget =
      new NativeWidgetMus(not_child_widget.get(), not_child_window,
                          ui::mojom::CompositorFrameSinkType::DEFAULT);
  not_child_widget->Init(not_child_params);

  EXPECT_NE(not_child_window->parent(), parent_window);
}

}  // namespace views
