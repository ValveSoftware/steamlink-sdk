// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {

class TestDialogClientView : public DialogClientView {
 public:
  TestDialogClientView(View* contents_view,
                       DialogDelegate* dialog_delegate)
      : DialogClientView(contents_view),
        dialog_(dialog_delegate) {}
  virtual ~TestDialogClientView() {}

  // DialogClientView implementation.
  virtual DialogDelegate* GetDialogDelegate() const OVERRIDE { return dialog_; }

  View* GetContentsView() { return contents_view(); }

  void CreateExtraViews() {
    CreateExtraView();
    CreateFootnoteView();
  }

 private:
  DialogDelegate* dialog_;

  DISALLOW_COPY_AND_ASSIGN(TestDialogClientView);
};

class DialogClientViewTest : public ViewsTestBase,
                             public DialogDelegateView {
 public:
  DialogClientViewTest()
      : dialog_buttons_(ui::DIALOG_BUTTON_NONE),
        extra_view_(NULL),
        footnote_view_(NULL) {}
  virtual ~DialogClientViewTest() {}

  // testing::Test implementation.
  virtual void SetUp() OVERRIDE {
    dialog_buttons_ = ui::DIALOG_BUTTON_NONE;
    contents_.reset(new StaticSizedView(gfx::Size(100, 200)));
    client_view_.reset(new TestDialogClientView(contents_.get(), this));

    ViewsTestBase::SetUp();
  }

  // DialogDelegateView implementation.
  virtual View* GetContentsView() OVERRIDE { return contents_.get(); }
  virtual View* CreateExtraView() OVERRIDE { return extra_view_; }
  virtual View* CreateFootnoteView() OVERRIDE { return footnote_view_; }
  virtual int GetDialogButtons() const OVERRIDE { return dialog_buttons_; }

 protected:
  gfx::Rect GetUpdatedClientBounds() {
    client_view_->SizeToPreferredSize();
    client_view_->Layout();
    return client_view_->bounds();
  }

  // Makes sure that the content view is sized correctly. Width must be at least
  // the requested amount, but height should always match exactly.
  void CheckContentsIsSetToPreferredSize() {
    const gfx::Rect client_bounds = GetUpdatedClientBounds();
    const gfx::Size preferred_size = contents_->GetPreferredSize();
    EXPECT_EQ(preferred_size.height(), contents_->bounds().height());
    EXPECT_LE(preferred_size.width(), contents_->bounds().width());
    EXPECT_EQ(contents_->bounds().origin(), client_bounds.origin());
    EXPECT_EQ(contents_->bounds().right(), client_bounds.right());
  }

  // Sets the buttons to show in the dialog and refreshes the dialog.
  void SetDialogButtons(int dialog_buttons) {
    dialog_buttons_ = dialog_buttons;
    client_view_->UpdateDialogButtons();
  }

  // Sets the extra view.
  void SetExtraView(View* view) {
    DCHECK(!extra_view_);
    extra_view_ = view;
    client_view_->CreateExtraViews();
  }

  // Sets the footnote view.
  void SetFootnoteView(View* view) {
    DCHECK(!footnote_view_);
    footnote_view_ = view;
    client_view_->CreateExtraViews();
  }

  TestDialogClientView* client_view() { return client_view_.get(); }

 private:
  // The contents of the dialog.
  scoped_ptr<View> contents_;
  // The DialogClientView that's being tested.
  scoped_ptr<TestDialogClientView> client_view_;
  // The bitmask of buttons to show in the dialog.
  int dialog_buttons_;
  View* extra_view_;  // weak
  View* footnote_view_;  // weak

  DISALLOW_COPY_AND_ASSIGN(DialogClientViewTest);
};

TEST_F(DialogClientViewTest, UpdateButtons) {
  // This dialog should start with no buttons.
  EXPECT_EQ(GetDialogButtons(), ui::DIALOG_BUTTON_NONE);
  EXPECT_EQ(NULL, client_view()->ok_button());
  EXPECT_EQ(NULL, client_view()->cancel_button());
  const int height_without_buttons = GetUpdatedClientBounds().height();

  // Update to use both buttons.
  SetDialogButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  EXPECT_TRUE(client_view()->ok_button()->is_default());
  EXPECT_FALSE(client_view()->cancel_button()->is_default());
  const int height_with_buttons = GetUpdatedClientBounds().height();
  EXPECT_GT(height_with_buttons, height_without_buttons);

  // Remove the dialog buttons.
  SetDialogButtons(ui::DIALOG_BUTTON_NONE);
  EXPECT_EQ(NULL, client_view()->ok_button());
  EXPECT_EQ(NULL, client_view()->cancel_button());
  EXPECT_EQ(GetUpdatedClientBounds().height(), height_without_buttons);

  // Reset with just an ok button.
  SetDialogButtons(ui::DIALOG_BUTTON_OK);
  EXPECT_TRUE(client_view()->ok_button()->is_default());
  EXPECT_EQ(NULL, client_view()->cancel_button());
  EXPECT_EQ(GetUpdatedClientBounds().height(), height_with_buttons);

  // Reset with just a cancel button.
  SetDialogButtons(ui::DIALOG_BUTTON_CANCEL);
  EXPECT_EQ(NULL, client_view()->ok_button());
  EXPECT_TRUE(client_view()->cancel_button()->is_default());
  EXPECT_EQ(GetUpdatedClientBounds().height(), height_with_buttons);
}

TEST_F(DialogClientViewTest, RemoveAndUpdateButtons) {
  // Removing buttons from another context should clear the local pointer.
  SetDialogButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  delete client_view()->ok_button();
  EXPECT_EQ(NULL, client_view()->ok_button());
  delete client_view()->cancel_button();
  EXPECT_EQ(NULL, client_view()->cancel_button());

  // Updating should restore the requested buttons properly.
  SetDialogButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  EXPECT_TRUE(client_view()->ok_button()->is_default());
  EXPECT_FALSE(client_view()->cancel_button()->is_default());
}

// Test that the contents view gets its preferred size in the basic dialog
// configuration.
TEST_F(DialogClientViewTest, ContentsSize) {
  CheckContentsIsSetToPreferredSize();
  EXPECT_EQ(GetContentsView()->bounds().bottom(),
            client_view()->bounds().bottom());
}

// Test the effect of the button strip on layout.
TEST_F(DialogClientViewTest, LayoutWithButtons) {
  SetDialogButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  CheckContentsIsSetToPreferredSize();
  EXPECT_LT(GetContentsView()->bounds().bottom(),
            client_view()->bounds().bottom());
  gfx::Size no_extra_view_size = client_view()->bounds().size();

  View* extra_view = new StaticSizedView(gfx::Size(200, 200));
  SetExtraView(extra_view);
  CheckContentsIsSetToPreferredSize();
  EXPECT_GT(client_view()->bounds().height(), no_extra_view_size.height());
  int width_of_extra_view = extra_view->bounds().width();

  // Visibility of extra view is respected.
  extra_view->SetVisible(false);
  CheckContentsIsSetToPreferredSize();
  EXPECT_EQ(no_extra_view_size.height(), client_view()->bounds().height());
  EXPECT_EQ(no_extra_view_size.width(), client_view()->bounds().width());

  // Try with a reduced-size dialog.
  extra_view->SetVisible(true);
  client_view()->SetBoundsRect(gfx::Rect(gfx::Point(0, 0), no_extra_view_size));
  client_view()->Layout();
  DCHECK_GT(width_of_extra_view, extra_view->bounds().width());
}

// Test the effect of the footnote view on layout.
TEST_F(DialogClientViewTest, LayoutWithFootnote) {
  CheckContentsIsSetToPreferredSize();
  gfx::Size no_footnote_size = client_view()->bounds().size();

  View* footnote_view = new StaticSizedView(gfx::Size(200, 200));
  SetFootnoteView(footnote_view);
  CheckContentsIsSetToPreferredSize();
  EXPECT_GT(client_view()->bounds().height(), no_footnote_size.height());
  EXPECT_EQ(200, footnote_view->bounds().height());
  gfx::Size with_footnote_size = client_view()->bounds().size();
  EXPECT_EQ(with_footnote_size.width(), footnote_view->bounds().width());

  SetDialogButtons(ui::DIALOG_BUTTON_CANCEL);
  CheckContentsIsSetToPreferredSize();
  EXPECT_LE(with_footnote_size.height(), client_view()->bounds().height());
  EXPECT_LE(with_footnote_size.width(), client_view()->bounds().width());
  gfx::Size with_footnote_and_button_size = client_view()->bounds().size();

  SetDialogButtons(ui::DIALOG_BUTTON_NONE);
  footnote_view->SetVisible(false);
  CheckContentsIsSetToPreferredSize();
  EXPECT_EQ(no_footnote_size.height(), client_view()->bounds().height());
  EXPECT_EQ(no_footnote_size.width(), client_view()->bounds().width());
}

// Test that GetHeightForWidth is respected for the footnote view.
TEST_F(DialogClientViewTest, LayoutWithFootnoteHeightForWidth) {
  CheckContentsIsSetToPreferredSize();
  gfx::Size no_footnote_size = client_view()->bounds().size();

  View* footnote_view = new ProportionallySizedView(3);
  SetFootnoteView(footnote_view);
  CheckContentsIsSetToPreferredSize();
  EXPECT_GT(client_view()->bounds().height(), no_footnote_size.height());
  EXPECT_EQ(footnote_view->bounds().width() * 3,
            footnote_view->bounds().height());
}

// Test that the DialogClientView's FocusManager is properly updated when the
// DialogClientView belongs to a non top level widget and the widget is
// reparented. The DialogClientView belongs to a non top level widget in the
// case of constrained windows. The constrained window's widget is reparented
// when a browser tab is dragged to a different browser window.
TEST_F(DialogClientViewTest, FocusManager) {
  scoped_ptr<Widget> toplevel1(new Widget);
  Widget::InitParams toplevel1_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  toplevel1_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  toplevel1->Init(toplevel1_params);

  scoped_ptr<Widget> toplevel2(new Widget);
  Widget::InitParams toplevel2_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  toplevel2_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  toplevel2->Init(toplevel2_params);

  Widget* dialog = new Widget;
  Widget::InitParams dialog_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  dialog_params.child = true;
  dialog_params.delegate = new DialogDelegateView();
  dialog_params.parent = toplevel1->GetNativeView();
  dialog->Init(dialog_params);

  // Test that the FocusManager has been properly set when the DialogClientView
  // was parented to |dialog|.
  DialogClientView* client_view =
      static_cast<DialogClientView*>(dialog->client_view());
  EXPECT_EQ(toplevel1->GetFocusManager(), client_view->focus_manager_);

  // Test that the FocusManager is properly updated when the DialogClientView's
  // top level widget is changed.
  Widget::ReparentNativeView(dialog->GetNativeView(), NULL);
  EXPECT_EQ(NULL, client_view->focus_manager_);
  Widget::ReparentNativeView(dialog->GetNativeView(),
                             toplevel2->GetNativeView());
  EXPECT_EQ(toplevel2->GetFocusManager(), client_view->focus_manager_);
  Widget::ReparentNativeView(dialog->GetNativeView(),
                             toplevel1->GetNativeView());
  EXPECT_NE(toplevel1->GetFocusManager(), toplevel2->GetFocusManager());
  EXPECT_EQ(toplevel1->GetFocusManager(), client_view->focus_manager_);

  // Test that the FocusManager is properly cleared when the DialogClientView is
  // removed from |dialog| during the widget's destruction.
  client_view->set_owned_by_client();
  scoped_ptr<DialogClientView> owned_client_view(client_view);
  toplevel1->CloseNow();
  toplevel2->CloseNow();
  EXPECT_EQ(NULL, owned_client_view->focus_manager_);
}

}  // namespace views
