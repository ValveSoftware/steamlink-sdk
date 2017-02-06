// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_MODAL_APP_MODAL_DIALOG_H_
#define COMPONENTS_APP_MODAL_APP_MODAL_DIALOG_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "build/build_config.h"

namespace content {
class WebContents;
}

namespace app_modal {

class NativeAppModalDialog;

// A controller+model base class for modal dialogs.
class AppModalDialog {
 public:
  // A union of data necessary to determine the type of message box to
  // show.
  AppModalDialog(content::WebContents* web_contents,
                 const base::string16& title);
  virtual ~AppModalDialog();

  // Called by the AppModalDialogQueue to show this dialog.
  void ShowModalDialog();

  // Called by the AppModalDialogQueue to activate the dialog.
  void ActivateModalDialog();

  // Closes the dialog if it is showing.
  void CloseModalDialog();

  // Completes dialog handling, shows next modal dialog from the queue.
  // TODO(beng): Get rid of this method.
  void CompleteDialog();

  base::string16 title() const { return title_; }
  NativeAppModalDialog* native_dialog() const { return native_dialog_; }
  content::WebContents* web_contents() const { return web_contents_; }

  // Returns true if the dialog is still valid. As dialogs are created they are
  // added to the AppModalDialogQueue. When the current modal dialog finishes
  // and it's time to show the next dialog in the queue IsValid is invoked.
  // If IsValid returns false the dialog is deleted and not shown.
  bool IsValid();

  // Methods overridable by AppModalDialog subclasses:

  // Invalidates the dialog, therefore causing it to not be shown when its turn
  // to be shown comes around.
  virtual void Invalidate();

  // Used only for testing. Returns whether the dialog is a JavaScript modal
  // dialog.
  virtual bool IsJavaScriptModalDialog();

 protected:
  // Overridden by subclasses to create the feature-specific native dialog box.
  virtual NativeAppModalDialog* CreateNativeDialog() = 0;

 private:
  // Information about the message box is held in the following variables.
  base::string16 title_;

  // True if CompleteDialog was called.
  bool completed_;

  // False if the dialog should no longer be shown, e.g. because the underlying
  // tab navigated away while the dialog was queued.
  bool valid_;

  // The toolkit-specific implementation of the app modal dialog box.
  NativeAppModalDialog* native_dialog_;

  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(AppModalDialog);
};

// An interface to observe that a modal dialog is shown.
class AppModalDialogObserver {
 public:
  AppModalDialogObserver();
  virtual ~AppModalDialogObserver();

  // Called when the modal dialog is shown.
  virtual void Notify(AppModalDialog* dialog) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppModalDialogObserver);
};

}  // namespace app_modal

#endif  // COMPONENTS_APP_MODAL_APP_MODAL_DIALOG_H_
