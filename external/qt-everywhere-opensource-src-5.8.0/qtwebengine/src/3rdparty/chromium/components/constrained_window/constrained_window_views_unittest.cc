// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/constrained_window/constrained_window_views.h"

#include <memory>

#include "base/macros.h"
#include "components/web_modal/test_web_contents_modal_dialog_host.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/border.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

using views::Widget;

namespace constrained_window {
namespace {

class DialogContents : public views::DialogDelegateView {
 public:
  DialogContents() {}
  ~DialogContents() override {}

  void set_preferred_size(const gfx::Size& preferred_size) {
    preferred_size_ = preferred_size;
  }

  // Overriden from DialogDelegateView:
  views::View* GetContentsView() override { return this; }
  gfx::Size GetPreferredSize() const override { return preferred_size_; }
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }

 private:
  gfx::Size preferred_size_;

  DISALLOW_COPY_AND_ASSIGN(DialogContents);
};

class ConstrainedWindowViewsTest : public views::ViewsTestBase {
 public:
  ConstrainedWindowViewsTest() : contents_(nullptr), dialog_(nullptr) {}
  ~ConstrainedWindowViewsTest() override {}

  void SetUp() override {
    views::ViewsTestBase::SetUp();
    contents_ = new DialogContents;
    dialog_ = views::DialogDelegate::CreateDialogWidget(
        contents_, GetContext(), nullptr);
    dialog_host_.reset(new web_modal::TestWebContentsModalDialogHost(
        dialog_->GetNativeView()));
    dialog_host_->set_max_dialog_size(gfx::Size(5000, 5000));

    // Make sure the dialog size is dominated by the preferred size of the
    // contents.
    gfx::Size preferred_size = dialog()->GetRootView()->GetPreferredSize();
    preferred_size.Enlarge(500, 500);
    contents()->set_preferred_size(preferred_size);
  }

  void TearDown() override {
    contents_ = nullptr;
    dialog_host_.reset();
    dialog_->CloseNow();
    ViewsTestBase::TearDown();
  }

  gfx::Size GetDialogSize() {
    return dialog()->GetRootView()->GetBoundsInScreen().size();
  }

  DialogContents* contents() { return contents_; }
  web_modal::TestWebContentsModalDialogHost* dialog_host() {
    return dialog_host_.get();
  }
  Widget* dialog() { return dialog_; }

 private:
  DialogContents* contents_;
  std::unique_ptr<web_modal::TestWebContentsModalDialogHost> dialog_host_;
  Widget* dialog_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowViewsTest);
};

}  // namespace

// Make sure a dialog that increases its preferred size grows on the next
// position update.
TEST_F(ConstrainedWindowViewsTest, GrowModalDialogSize) {
  UpdateWidgetModalDialogPosition(dialog(), dialog_host());
  gfx::Size expected_size = GetDialogSize();
  gfx::Size preferred_size = contents()->GetPreferredSize();
  expected_size.Enlarge(50, 50);
  preferred_size.Enlarge(50, 50);
  contents()->set_preferred_size(preferred_size);
  UpdateWidgetModalDialogPosition(dialog(), dialog_host());
  EXPECT_EQ(expected_size.ToString(), GetDialogSize().ToString());
}

// Make sure a dialog that reduces its preferred size shrinks on the next
// position update.
TEST_F(ConstrainedWindowViewsTest, ShrinkModalDialogSize) {
  UpdateWidgetModalDialogPosition(dialog(), dialog_host());
  gfx::Size expected_size = GetDialogSize();
  gfx::Size preferred_size = contents()->GetPreferredSize();
  expected_size.Enlarge(-50, -50);
  preferred_size.Enlarge(-50, -50);
  contents()->set_preferred_size(preferred_size);
  UpdateWidgetModalDialogPosition(dialog(), dialog_host());
  EXPECT_EQ(expected_size.ToString(), GetDialogSize().ToString());
}

// Make sure browser modal dialogs are not affected by restrictions on web
// content modal dialog maximum sizes.
TEST_F(ConstrainedWindowViewsTest, MaximumBrowserDialogSize) {
  UpdateWidgetModalDialogPosition(dialog(), dialog_host());
  gfx::Size dialog_size = GetDialogSize();
  gfx::Size max_dialog_size = dialog_size;
  max_dialog_size.Enlarge(-50, -50);
  dialog_host()->set_max_dialog_size(max_dialog_size);
  UpdateWidgetModalDialogPosition(dialog(), dialog_host());
  EXPECT_EQ(dialog_size.ToString(), GetDialogSize().ToString());
}

// Web content modal dialogs should not get a size larger than what the dialog
// host gives as the maximum size.
TEST_F(ConstrainedWindowViewsTest, MaximumWebContentsDialogSize) {
  UpdateWebContentsModalDialogPosition(dialog(), dialog_host());
  gfx::Size full_dialog_size = GetDialogSize();
  gfx::Size max_dialog_size = full_dialog_size;
  max_dialog_size.Enlarge(-50, -50);
  dialog_host()->set_max_dialog_size(max_dialog_size);
  UpdateWebContentsModalDialogPosition(dialog(), dialog_host());
  // The top border of the dialog is intentionally drawn outside the area
  // specified by the dialog host, so add it to the size the dialog is expected
  // to occupy.
  gfx::Size expected_size = max_dialog_size;
  views::Border* border = dialog()->non_client_view()->frame_view()->border();
  if (border)
    expected_size.Enlarge(0, border->GetInsets().top());
  EXPECT_EQ(expected_size.ToString(), GetDialogSize().ToString());

  // Increasing the maximum dialog size should bring the dialog back to its
  // original size.
  max_dialog_size.Enlarge(100, 100);
  dialog_host()->set_max_dialog_size(max_dialog_size);
  UpdateWebContentsModalDialogPosition(dialog(), dialog_host());
  EXPECT_EQ(full_dialog_size.ToString(), GetDialogSize().ToString());
}

}  // namespace constrained_window
