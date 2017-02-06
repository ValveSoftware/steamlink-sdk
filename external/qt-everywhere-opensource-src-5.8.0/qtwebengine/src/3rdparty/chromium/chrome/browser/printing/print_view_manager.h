// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_H_
#define CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_H_

#include "base/macros.h"
#include "chrome/browser/printing/print_view_manager_base.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class RenderProcessHost;
}

namespace printing {

// Manages the print commands for a WebContents.
class PrintViewManager : public PrintViewManagerBase,
                         public content::WebContentsUserData<PrintViewManager> {
 public:
  ~PrintViewManager() override;

#if defined(ENABLE_BASIC_PRINTING)
  // Same as PrintNow(), but for the case where a user prints with the system
  // dialog from print preview.
  // |dialog_shown_callback| is called when the print dialog is shown.
  bool PrintForSystemDialogNow(const base::Closure& dialog_shown_callback);

  // Same as PrintNow(), but for the case where a user press "ctrl+shift+p" to
  // show the native system dialog. This can happen from both initiator and
  // preview dialog.
  bool BasicPrint();
#endif  // ENABLE_BASIC_PRINTING

  // Initiate print preview of the current document by first notifying the
  // renderer. Since this happens asynchronous, the print preview dialog
  // creation will not be completed on the return of this function. Returns
  // false if print preview is impossible at the moment.
  bool PrintPreviewNow(bool selection_only);

  // Notify PrintViewManager that print preview is starting in the renderer for
  // a particular WebNode.
  void PrintPreviewForWebNode();

  // Notify PrintViewManager that print preview has finished. Unfreeze the
  // renderer in the case of scripted print preview.
  void PrintPreviewDone();

  // content::WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // content::WebContentsObserver implementation.
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;

  // content::WebContentsObserver implementation.
  // Terminates or cancels the print job if one was pending.
  void RenderProcessGone(base::TerminationStatus status) override;

 private:
  explicit PrintViewManager(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PrintViewManager>;

  enum PrintPreviewState {
    NOT_PREVIEWING,
    USER_INITIATED_PREVIEW,
    SCRIPTED_PREVIEW,
  };

  // IPC Message handlers.
  void OnDidShowPrintDialog();
  void OnSetupScriptedPrintPreview(IPC::Message* reply_msg);
  void OnShowScriptedPrintPreview(bool source_is_modifiable);
  void OnScriptedPrintPreviewReply(IPC::Message* reply_msg);

  base::Closure on_print_dialog_shown_callback_;

  // Current state of print preview for this view.
  PrintPreviewState print_preview_state_;

  // Keeps track of the pending callback during scripted print preview.
  content::RenderProcessHost* scripted_print_preview_rph_;

  DISALLOW_COPY_AND_ASSIGN(PrintViewManager);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_H_
