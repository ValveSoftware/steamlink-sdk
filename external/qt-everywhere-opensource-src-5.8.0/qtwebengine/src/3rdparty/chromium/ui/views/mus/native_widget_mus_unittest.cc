// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/native_widget_mus.h"

#include "base/callback.h"
#include "base/macros.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/tests/window_tree_client_private.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/cpp/window_tree_client.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
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
#include "ui/views/test/focus_manager_test.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/public/activation_client.h"

using mus::mojom::EventResult;

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

  void AckCallback(mus::mojom::EventResult result) {
    ack_callback_count_++;
    EXPECT_EQ(mus::mojom::EventResult::HANDLED, result);
  }

  // Returns a mouse pressed event inside the widget. Tests that place views
  // within the widget that respond to the event must be constructed within the
  // widget coordinate space such that they respond correctly.
  std::unique_ptr<ui::MouseEvent> CreateMouseEvent() {
    return base::WrapUnique(new ui::MouseEvent(
        ui::ET_MOUSE_PRESSED, gfx::Point(50, 50), gfx::Point(50, 50),
        base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  }

  // Simulates an input event to the NativeWidget.
  void OnWindowInputEvent(
      NativeWidgetMus* native_widget,
      const ui::Event& event,
      std::unique_ptr<base::Callback<void(mus::mojom::EventResult)>>*
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

// Tests that a window with an icon sets the mus::Window icon property.
TEST_F(NativeWidgetMusTest, AppIcon) {
  // Create a Widget with a bitmap as the icon.
  SkBitmap source_bitmap = MakeBitmap(SK_ColorRED);
  std::unique_ptr<Widget> widget(
      CreateWidget(new TestWidgetDelegate(source_bitmap)));

  // The mus::Window has the icon property.
  mus::Window* window =
      static_cast<NativeWidgetMus*>(widget->native_widget_private())->window();
  EXPECT_TRUE(window->HasSharedProperty(
      mus::mojom::WindowManager::kWindowAppIcon_Property));

  // The icon is the expected icon.
  SkBitmap icon = window->GetSharedProperty<SkBitmap>(
      mus::mojom::WindowManager::kWindowAppIcon_Property);
  EXPECT_TRUE(gfx::BitmapsAreEqual(source_bitmap, icon));
}

// Tests that a window without an icon does not set the mus::Window icon
// property.
TEST_F(NativeWidgetMusTest, NoAppIcon) {
  // Create a Widget without a special icon.
  std::unique_ptr<Widget> widget(CreateWidget(nullptr));

  // The mus::Window does not have an icon property.
  mus::Window* window =
      static_cast<NativeWidgetMus*>(widget->native_widget_private())->window();
  EXPECT_FALSE(window->HasSharedProperty(
      mus::mojom::WindowManager::kWindowAppIcon_Property));
}

// Tests that changing the icon on a Widget updates the mus::Window icon
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
  mus::Window* window =
      static_cast<NativeWidgetMus*>(widget->native_widget_private())->window();
  SkBitmap icon = window->GetSharedProperty<SkBitmap>(
      mus::mojom::WindowManager::kWindowAppIcon_Property);
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
// mus::Window.
TEST_F(NativeWidgetMusTest, GetName) {
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.name = "MyWidget";
  widget.Init(params);
  mus::Window* window =
      static_cast<NativeWidgetMus*>(widget.native_widget_private())->window();
  EXPECT_EQ("MyWidget", window->GetName());
}

// Tests that a Widget with a hit test mask propagates the mask to the
// mus::Window.
TEST_F(NativeWidgetMusTest, HitTestMask) {
  gfx::Rect mask(5, 5, 10, 10);
  std::unique_ptr<Widget> widget(
      CreateWidget(new WidgetDelegateWithHitTestMask(mask)));

  // The window has the mask.
  mus::Window* window =
      static_cast<NativeWidgetMus*>(widget->native_widget_private())->window();
  ASSERT_TRUE(window->hit_test_mask());
  EXPECT_EQ(mask.ToString(), window->hit_test_mask()->ToString());
}

// Verifies changing the visibility of a child mus::Window doesn't change the
// visibility of the parent.
TEST_F(NativeWidgetMusTest, ChildVisibilityDoesntEffectParent) {
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(params);
  widget.Show();
  mus::Window* window =
      static_cast<NativeWidgetMus*>(widget.native_widget_private())->window();
  ASSERT_TRUE(window->visible());

  // Create a child window, make it visible and parent it to the Widget's
  // window.
  mus::Window* child_window = window->window_tree()->NewWindow();
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
  mus::WindowTreeClientPrivate test_api(native_widget->window());
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
  mus::Window* mus_window =
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

// Ensure that manually setting NativeWidgetMus's mus::Window bounds also
// updates its WindowTreeHost bounds.
TEST_F(NativeWidgetMusTest, SetMusWindowBounds) {
  std::unique_ptr<Widget> widget(CreateWidget(nullptr));
  widget->Show();
  View* content = new View;
  widget->GetContentsView()->AddChildView(content);
  NativeWidgetMus* native_widget =
      static_cast<NativeWidgetMus*>(widget->native_widget_private());
  mus::Window* mus_window = native_widget->window();

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

}  // namespace views
