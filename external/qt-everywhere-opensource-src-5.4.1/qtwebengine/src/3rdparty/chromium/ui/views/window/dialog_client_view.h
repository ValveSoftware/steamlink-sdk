// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_DIALOG_CLIENT_VIEW_H_
#define UI_VIEWS_WINDOW_DIALOG_CLIENT_VIEW_H_

#include "ui/base/ui_base_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/window/client_view.h"

namespace views {

class DialogDelegate;
class LabelButton;
class Widget;

// DialogClientView provides adornments for a dialog's content view, including
// custom-labeled [OK] and [Cancel] buttons with [Enter] and [Esc] accelerators.
// The view also displays the delegate's extra view alongside the buttons and
// the delegate's footnote view below the buttons. The view appears like below.
// NOTE: The contents view is not inset on the top or side client view edges.
//   +------------------------------+
//   |        Contents View         |
//   +------------------------------+
//   | [Extra View]   [OK] [Cancel] |
//   | [      Footnote View       ] |
//   +------------------------------+
class VIEWS_EXPORT DialogClientView : public ClientView,
                                      public ButtonListener,
                                      public FocusChangeListener {
 public:
  DialogClientView(Widget* widget, View* contents_view);
  virtual ~DialogClientView();

  // Accept or Cancel the dialog.
  void AcceptWindow();
  void CancelWindow();

  // Accessors in case the user wishes to adjust these buttons.
  LabelButton* ok_button() const { return ok_button_; }
  LabelButton* cancel_button() const { return cancel_button_; }

  // Update the dialog buttons to match the dialog's delegate.
  void UpdateDialogButtons();

  // ClientView implementation:
  virtual bool CanClose() OVERRIDE;
  virtual DialogClientView* AsDialogClientView() OVERRIDE;
  virtual const DialogClientView* AsDialogClientView() const OVERRIDE;

  // FocusChangeListener implementation:
  virtual void OnWillChangeFocus(View* focused_before,
                                 View* focused_now) OVERRIDE;
  virtual void OnDidChangeFocus(View* focused_before,
                                View* focused_now) OVERRIDE;

  // View implementation:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE;
  virtual void NativeViewHierarchyChanged() OVERRIDE;
  virtual void OnNativeThemeChanged(const ui::NativeTheme* theme) OVERRIDE;

  // ButtonListener implementation:
  virtual void ButtonPressed(Button* sender, const ui::Event& event) OVERRIDE;

 protected:
  // For testing.
  DialogClientView(View* contents_view);

  // Returns the DialogDelegate for the window. Virtual for testing.
  virtual DialogDelegate* GetDialogDelegate() const;

  // Create and add the extra view, if supplied by the delegate.
  void CreateExtraView();

  // Creates and adds the footnote view, if supplied by the delegate.
  void CreateFootnoteView();

  // View implementation.
  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE;
  virtual void ChildVisibilityChanged(View* child) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(DialogClientViewTest, FocusManager);

  bool has_dialog_buttons() const { return ok_button_ || cancel_button_; }

  // Create a dialog button of the appropriate type.
  LabelButton* CreateDialogButton(ui::DialogButton type);

  // Update |button|'s text and enabled state according to the delegate's state.
  void UpdateButton(LabelButton* button, ui::DialogButton type);

  // Returns the height of the row containing the buttons and the extra view.
  int GetButtonsAndExtraViewRowHeight() const;

  // Returns the insets for the buttons and extra view.
  gfx::Insets GetButtonRowInsets() const;

  // Closes the widget.
  void Close();

  // The dialog buttons.
  LabelButton* ok_button_;
  LabelButton* cancel_button_;

  // The button that is currently default; may be NULL.
  LabelButton* default_button_;

  // Observe |focus_manager_| to update the default button with focus changes.
  FocusManager* focus_manager_;

  // The extra view shown in the row of buttons; may be NULL.
  View* extra_view_;

  // The footnote view shown below the buttons; may be NULL.
  View* footnote_view_;

  // True if we've notified the delegate the window is closing and the delegate
  // allosed the close. In some situations it's possible to get two closes (see
  // http://crbug.com/71940). This is used to avoid notifying the delegate
  // twice, which can have bad consequences.
  bool notified_delegate_;

  DISALLOW_COPY_AND_ASSIGN(DialogClientView);
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_DIALOG_CLIENT_VIEW_H_
