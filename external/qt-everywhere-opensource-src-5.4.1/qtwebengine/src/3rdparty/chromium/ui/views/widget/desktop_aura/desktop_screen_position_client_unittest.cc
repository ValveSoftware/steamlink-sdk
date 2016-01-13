// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {

typedef ViewsTestBase DesktopScreenPositionClientTest;

// Verifies setting the bounds of a dialog parented to a Widget with a
// DesktopNativeWidgetAura is positioned correctly.
TEST_F(DesktopScreenPositionClientTest, PositionDialog) {
  Widget parent_widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(10, 11, 200, 200);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.native_widget = new DesktopNativeWidgetAura(&parent_widget);
  parent_widget.Init(params);

  // Owned by |dialog|.
  DialogDelegateView* dialog_delegate_view = new DialogDelegateView;
  // Owned by |parent_widget|.
  Widget* dialog = DialogDelegate::CreateDialogWidget(
      dialog_delegate_view, NULL, parent_widget.GetNativeView());
  dialog->SetBounds(gfx::Rect(11, 12, 200, 200));
  EXPECT_EQ("11,12", dialog->GetWindowBoundsInScreen().origin().ToString());
}

// Verifies that setting the bounds of a control parented to something other
// than the root window is positioned correctly.
TEST_F(DesktopScreenPositionClientTest, PositionControlWithNonRootParent) {
  Widget widget1;
  Widget widget2;
  Widget widget3;
  gfx::Point origin = gfx::Point(16, 16);

  // Use a custom frame type.  By default we will choose a native frame when
  // aero glass is enabled, and this complicates the logic surrounding origin
  // computation, making it difficult to compute the expected origin location.
  widget1.set_frame_type(Widget::FRAME_TYPE_FORCE_CUSTOM);
  widget2.set_frame_type(Widget::FRAME_TYPE_FORCE_CUSTOM);
  widget3.set_frame_type(Widget::FRAME_TYPE_FORCE_CUSTOM);

  // Create 3 windows.  A root window, an arbitrary window parented to the root
  // but NOT positioned at (0,0) relative to the root, and then a third window
  // parented to the second, also not positioned at (0,0).
  Widget::InitParams params1 =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  params1.bounds = gfx::Rect(origin, gfx::Size(700, 600));
  params1.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params1.native_widget = new DesktopNativeWidgetAura(&widget1);
  widget1.Init(params1);

  Widget::InitParams params2 =
    CreateParams(Widget::InitParams::TYPE_WINDOW);
  params2.bounds = gfx::Rect(origin, gfx::Size(600, 500));
  params2.parent = widget1.GetNativeView();
  params2.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params2.child = true;
  widget2.Init(params2);

  Widget::InitParams params3 =
      CreateParams(Widget::InitParams::TYPE_CONTROL);
  params3.parent = widget2.GetNativeView();
  params3.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params3.child = true;
  params3.bounds = gfx::Rect(origin, gfx::Size(500, 400));
  widget3.Init(params3);

  // The origin of the 3rd window should be the sum of all parent origins.
  gfx::Point expected_origin(origin.x() * 3, origin.y() * 3);
  gfx::Rect expected_bounds(expected_origin, gfx::Size(500, 400));
  gfx::Rect actual_bounds(widget3.GetWindowBoundsInScreen());
  EXPECT_EQ(expected_bounds.ToString(), actual_bounds.ToString());
}

}  // namespace views
