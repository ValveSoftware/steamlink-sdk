// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_DIALOG_DELEGATE_H_
#define UI_VIEWS_WINDOW_DIALOG_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

class DialogClientView;

///////////////////////////////////////////////////////////////////////////////
//
// DialogDelegate
//
//  DialogDelegate is an interface implemented by objects that wish to show a
//  dialog box Window. The window that is displayed uses this interface to
//  determine how it should be displayed and notify the delegate object of
//  certain events.
//
///////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT DialogDelegate : public ui::DialogModel,
                                    public WidgetDelegate {
 public:
  DialogDelegate();
  virtual ~DialogDelegate();

  // Create a dialog widget with the specified |context| or |parent|.
  static Widget* CreateDialogWidget(WidgetDelegate* delegate,
                                    gfx::NativeView context,
                                    gfx::NativeView parent);

  // Override this function to display an extra view adjacent to the buttons.
  // Overrides may construct the view; this will only be called once per dialog.
  virtual View* CreateExtraView();

  // Override this function to display an extra view in the titlebar.
  // Overrides may construct the view; this will only be called once per dialog.
  // Note: this only works for new style dialogs.
  virtual View* CreateTitlebarExtraView();

  // Override this function to display a footnote view below the buttons.
  // Overrides may construct the view; this will only be called once per dialog.
  virtual View* CreateFootnoteView();

  // For Dialog boxes, if there is a "Cancel" button or no dialog button at all,
  // this is called when the user presses the "Cancel" button or the Esc key.
  // It can also be called on a close action if |Close| has not been
  // overridden. This function should return true if the window can be closed
  // after it returns, or false if it must remain open.
  virtual bool Cancel();

  // For Dialog boxes, this is called when the user presses the "OK" button,
  // or the Enter key. It can also be called on a close action if |Close|
  // has not been overridden. This function should return true if the window
  // can be closed after it returns, or false if it must remain open.
  // If |window_closing| is true, it means that this handler is
  // being called because the window is being closed (e.g.  by Window::Close)
  // and there is no Cancel handler, so Accept is being called instead.
  virtual bool Accept(bool window_closing);
  virtual bool Accept();

  // Called when the user closes the window without selecting an option,
  // e.g. by pressing the close button on the window or using a window manager
  // gesture. By default, this calls Accept() if the only button in the dialog
  // is Accept, Cancel() otherwise. This function should return true if the
  // window can be closed after it returns, or false if it must remain open.
  virtual bool Close();

  // Overridden from ui::DialogModel:
  virtual base::string16 GetDialogLabel() const OVERRIDE;
  virtual base::string16 GetDialogTitle() const OVERRIDE;
  virtual int GetDialogButtons() const OVERRIDE;
  virtual int GetDefaultDialogButton() const OVERRIDE;
  virtual bool ShouldDefaultButtonBeBlue() const OVERRIDE;
  virtual base::string16 GetDialogButtonLabel(
      ui::DialogButton button) const OVERRIDE;
  virtual bool IsDialogButtonEnabled(ui::DialogButton button) const OVERRIDE;

  // Overridden from WidgetDelegate:
  virtual View* GetInitiallyFocusedView() OVERRIDE;
  virtual DialogDelegate* AsDialogDelegate() OVERRIDE;
  virtual ClientView* CreateClientView(Widget* widget) OVERRIDE;
  virtual NonClientFrameView* CreateNonClientFrameView(Widget* widget) OVERRIDE;

  // Create a frame view using the new dialog style.
  static NonClientFrameView* CreateDialogFrameView(Widget* widget);

  // Returns whether this particular dialog should use the new dialog style.
  virtual bool UseNewStyleForThisDialog() const;

  // Called when the window has been closed.
  virtual void OnClosed() {}

  // A helper for accessing the DialogClientView object contained by this
  // delegate's Window.
  const DialogClientView* GetDialogClientView() const;
  DialogClientView* GetDialogClientView();

 protected:
  // Overridden from WidgetDelegate:
  virtual ui::AXRole GetAccessibleWindowRole() const OVERRIDE;

 private:
  // A flag indicating whether this dialog supports the new style.
  bool supports_new_style_;
};

// A DialogDelegate implementation that is-a View. Used to override GetWidget()
// to call View's GetWidget() for the common case where a DialogDelegate
// implementation is-a View. Note that DialogDelegateView is not owned by
// view's hierarchy and is expected to be deleted on DeleteDelegate call.
class VIEWS_EXPORT DialogDelegateView : public DialogDelegate,
                                        public View {
 public:
  DialogDelegateView();
  virtual ~DialogDelegateView();

  // Overridden from DialogDelegate:
  virtual void DeleteDelegate() OVERRIDE;
  virtual Widget* GetWidget() OVERRIDE;
  virtual const Widget* GetWidget() const OVERRIDE;
  virtual View* GetContentsView() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DialogDelegateView);
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_DIALOG_DELEGATE_H_
