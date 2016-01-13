// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/native/native_view_host_aura.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/view_constants_aura.h"
#include "ui/views/widget/widget.h"

namespace views {

class NativeViewHostAuraTest : public ViewsTestBase {
 public:
  NativeViewHostAuraTest() {
  }

  NativeViewHostAura* native_host() {
    return static_cast<NativeViewHostAura*>(host_->native_wrapper_.get());
  }

  Widget* toplevel() {
    return toplevel_.get();
  }

  NativeViewHost* host() {
    return host_.get();
  }

  Widget* child() {
    return child_.get();
  }

  void CreateHost() {
    // Create the top level widget.
    toplevel_.reset(new Widget);
    Widget::InitParams toplevel_params =
        CreateParams(Widget::InitParams::TYPE_WINDOW);
    toplevel_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    toplevel_->Init(toplevel_params);

    // And the child widget.
    View* test_view = new View;
    child_.reset(new Widget);
    Widget::InitParams child_params(Widget::InitParams::TYPE_CONTROL);
    child_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    child_params.parent = toplevel_->GetNativeView();
    child_->Init(child_params);
    child_->SetContentsView(test_view);

    // Owned by |toplevel|.
    host_.reset(new NativeViewHost);
    toplevel_->GetRootView()->AddChildView(host_.get());
    host_->Attach(child_->GetNativeView());
  }

  void DestroyHost() {
    host_.reset();
  }

 private:
  scoped_ptr<Widget> toplevel_;
  scoped_ptr<NativeViewHost> host_;
  scoped_ptr<Widget> child_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewHostAuraTest);
};

// Verifies NativeViewHostAura stops observing native view on destruction.
TEST_F(NativeViewHostAuraTest, StopObservingNativeViewOnDestruct) {
  CreateHost();
  aura::Window* child_win = child()->GetNativeView();
  NativeViewHostAura* aura_host = native_host();

  EXPECT_TRUE(child_win->HasObserver(aura_host));
  DestroyHost();
  EXPECT_FALSE(child_win->HasObserver(aura_host));
}

// Tests that the kHostViewKey is correctly set and cleared.
TEST_F(NativeViewHostAuraTest, HostViewPropertyKey) {
  // Create the NativeViewHost and attach a NativeView.
  CreateHost();
  aura::Window* child_win = child()->GetNativeView();
  EXPECT_EQ(host(), child_win->GetProperty(views::kHostViewKey));

  host()->Detach();
  EXPECT_FALSE(child_win->GetProperty(views::kHostViewKey));

  host()->Attach(child_win);
  EXPECT_EQ(host(), child_win->GetProperty(views::kHostViewKey));

  DestroyHost();
  EXPECT_FALSE(child_win->GetProperty(views::kHostViewKey));
}

// Tests that the NativeViewHost reports the cursor set on its native view.
TEST_F(NativeViewHostAuraTest, CursorForNativeView) {
  CreateHost();

  toplevel()->SetCursor(ui::kCursorHand);
  child()->SetCursor(ui::kCursorWait);
  ui::MouseEvent move_event(ui::ET_MOUSE_MOVED, gfx::Point(0, 0),
                            gfx::Point(0, 0), 0, 0);

  EXPECT_EQ(ui::kCursorWait, host()->GetCursor(move_event).native_type());

  DestroyHost();
}

}  // namespace views
