// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEB_DIALOGS_WEB_DIALOG_UI_H_
#define UI_WEB_DIALOGS_WEB_DIALOG_UI_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/ui_base_types.h"
#include "ui/web_dialogs/web_dialogs_export.h"
#include "url/gurl.h"

namespace content {
class WebContents;
class WebUIMessageHandler;
struct ContextMenuParams;
}

namespace gfx {
class Size;
}

namespace ui {

class WebDialogDelegate;

// Displays file URL contents inside a modal web dialog.
//
// This application really should not use WebContents + WebUI. It should instead
// just embed a RenderView in a dialog and be done with it.
//
// Before loading a URL corresponding to this WebUI, the caller should set its
// delegate as user data on the WebContents by calling SetDelegate(). This WebUI
// will pick it up from there and call it back. This is a bit of a hack to allow
// the dialog to pass its delegate to the Web UI without having nasty accessors
// on the WebContents. The correct design using RVH directly would avoid all of
// this.
class WEB_DIALOGS_EXPORT WebDialogUI : public content::WebUIController {
 public:
  struct WebDialogParams {
    // The URL for the content that will be loaded in the dialog.
    GURL url;
    // Width of the dialog.
    int width;
    // Height of the dialog.
    int height;
    // The JSON input to pass to the dialog when showing it.
    std::string json_input;
  };

  // When created, the delegate should already be set as user data on the
  // WebContents.
  explicit WebDialogUI(content::WebUI* web_ui);
  ~WebDialogUI() override;

  // Close the dialog, passing the specified arguments to the close handler.
  void CloseDialog(const base::ListValue* args);

  // Sets the delegate on the WebContents.
  static void SetDelegate(content::WebContents* web_contents,
                          WebDialogDelegate* delegate);

 private:
  // WebUIController
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;

  // Gets the delegate for the WebContent set with SetDelegate.
  static WebDialogDelegate* GetDelegate(content::WebContents* web_contents);

  // JS message handler.
  void OnDialogClosed(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(WebDialogUI);
};

}  // namespace ui

#endif  // UI_WEB_DIALOGS_WEB_DIALOG_UI_H_
