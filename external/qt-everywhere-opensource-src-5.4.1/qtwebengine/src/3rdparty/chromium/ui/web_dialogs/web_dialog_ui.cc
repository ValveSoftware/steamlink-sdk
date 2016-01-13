// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/web_dialogs/web_dialog_ui.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/values.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/bindings_policy.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

using content::RenderViewHost;
using content::WebUIMessageHandler;

namespace ui {

namespace {

const char kWebDialogDelegateUserDataKey[] = "WebDialogDelegateUserData";

class WebDialogDelegateUserData : public base::SupportsUserData::Data {
 public:
  explicit WebDialogDelegateUserData(WebDialogDelegate* delegate)
      : delegate_(delegate) {}
  virtual ~WebDialogDelegateUserData() {}
  WebDialogDelegate* delegate() { return delegate_; }

 private:
  WebDialogDelegate* delegate_;  // unowned
};

}  // namespace

WebDialogUI::WebDialogUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
}

WebDialogUI::~WebDialogUI() {
  // Don't unregister our user data. During the teardown of the WebContents,
  // this will be deleted, but the WebContents will already be destroyed.
  //
  // This object is owned indirectly by the WebContents. WebUIs can change, so
  // it's scary if this WebUI is changed out and replaced with something else,
  // since the user data will still point to the old delegate. But the delegate
  // is itself the owner of the WebContents for a dialog so will be in scope,
  // and the HTML dialogs won't swap WebUIs anyway since they don't navigate.
}

void WebDialogUI::CloseDialog(const base::ListValue* args) {
  OnDialogClosed(args);
}

// static
void WebDialogUI::SetDelegate(content::WebContents* web_contents,
                              WebDialogDelegate* delegate) {
  web_contents->SetUserData(&kWebDialogDelegateUserDataKey,
                            new WebDialogDelegateUserData(delegate));
}

////////////////////////////////////////////////////////////////////////////////
// Private:

WebDialogDelegate* WebDialogUI::GetDelegate(
    content::WebContents* web_contents) {
  WebDialogDelegateUserData* user_data =
      static_cast<WebDialogDelegateUserData*>(
          web_contents->GetUserData(&kWebDialogDelegateUserDataKey));

  return user_data ? user_data->delegate() : NULL;
}


void WebDialogUI::RenderViewCreated(RenderViewHost* render_view_host) {
  // Hook up the javascript function calls, also known as chrome.send("foo")
  // calls in the HTML, to the actual C++ functions.
  web_ui()->RegisterMessageCallback("dialogClose",
      base::Bind(&WebDialogUI::OnDialogClosed, base::Unretained(this)));

  // Pass the arguments to the renderer supplied by the delegate.
  std::string dialog_args;
  std::vector<WebUIMessageHandler*> handlers;
  WebDialogDelegate* delegate = GetDelegate(web_ui()->GetWebContents());
  if (delegate) {
    dialog_args = delegate->GetDialogArgs();
    delegate->GetWebUIMessageHandlers(&handlers);
  }

  if (0 != (web_ui()->GetBindings() & content::BINDINGS_POLICY_WEB_UI))
    render_view_host->SetWebUIProperty("dialogArguments", dialog_args);
  for (std::vector<WebUIMessageHandler*>::iterator it = handlers.begin();
       it != handlers.end(); ++it) {
    web_ui()->AddMessageHandler(*it);
  }

  if (delegate)
    delegate->OnDialogShown(web_ui(), render_view_host);
}

void WebDialogUI::OnDialogClosed(const base::ListValue* args) {
  WebDialogDelegate* delegate = GetDelegate(web_ui()->GetWebContents());
  if (delegate) {
    std::string json_retval;
    if (args && !args->empty() && !args->GetString(0, &json_retval))
      NOTREACHED() << "Could not read JSON argument";

    delegate->OnDialogCloseFromWebUI(json_retval);
  }
}

ExternalWebDialogUI::ExternalWebDialogUI(content::WebUI* web_ui)
    : WebDialogUI(web_ui) {
  // Non-file based UI needs to not have access to the Web UI bindings
  // for security reasons. The code hosting the dialog should provide
  // dialog specific functionality through other bindings and methods
  // that are scoped in duration to the dialogs existence.
  web_ui->SetBindings(web_ui->GetBindings() & ~content::BINDINGS_POLICY_WEB_UI);
}

ExternalWebDialogUI::~ExternalWebDialogUI() {
}

}  // namespace ui
