// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_JAVASCRIPT_DIALOG_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_JAVASCRIPT_DIALOG_MANAGER_H_

#include <string>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/public/common/javascript_message_type.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace content {

class WebContents;

// An interface consisting of methods that can be called to produce and manage
// JavaScript dialogs.
class CONTENT_EXPORT JavaScriptDialogManager {
 public:
  typedef base::Callback<void(bool /* success */,
                              const base::string16& /* user_input */)>
                                  DialogClosedCallback;

  // Displays a JavaScript dialog. |did_suppress_message| will not be nil; if
  // |true| is returned in it, the caller will handle faking the reply.
  virtual void RunJavaScriptDialog(
      WebContents* web_contents,
      const GURL& origin_url,
      JavaScriptMessageType javascript_message_type,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      const DialogClosedCallback& callback,
      bool* did_suppress_message) = 0;

  // Displays a dialog asking the user if they want to leave a page.
  virtual void RunBeforeUnloadDialog(WebContents* web_contents,
                                     bool is_reload,
                                     const DialogClosedCallback& callback) = 0;

  // Accepts or dismisses the active JavaScript dialog, which must be owned
  // by the given |web_contents|. If |prompt_override| is not null, the prompt
  // text of the dialog should be set before accepting. Returns true if the
  // dialog was handled.
  virtual bool HandleJavaScriptDialog(WebContents* web_contents,
                                      bool accept,
                                      const base::string16* prompt_override);

  // Cancels all active and pending dialogs for the given WebContents.
  virtual void CancelActiveAndPendingDialogs(WebContents* web_contents) = 0;

  // Reset any saved state tied to the given WebContents.
  virtual void ResetDialogState(WebContents* web_contents) = 0;

  virtual ~JavaScriptDialogManager() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_JAVASCRIPT_DIALOG_MANAGER_H_
