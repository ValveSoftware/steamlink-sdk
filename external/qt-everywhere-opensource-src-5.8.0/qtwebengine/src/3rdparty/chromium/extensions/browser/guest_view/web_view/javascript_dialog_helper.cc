// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/web_view/javascript_dialog_helper.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_types.h"

namespace extensions {

namespace {

std::string JavaScriptMessageTypeToString(
    content::JavaScriptMessageType message_type) {
  switch (message_type) {
    case content::JAVASCRIPT_MESSAGE_TYPE_ALERT:
      return "alert";
    case content::JAVASCRIPT_MESSAGE_TYPE_CONFIRM:
      return "confirm";
    case content::JAVASCRIPT_MESSAGE_TYPE_PROMPT:
      return "prompt";
    default:
      NOTREACHED() << "Unknown JavaScript Message Type.";
      return "unknown";
  }
}

}  // namespace

JavaScriptDialogHelper::JavaScriptDialogHelper(WebViewGuest* guest)
    : web_view_guest_(guest) {
}

JavaScriptDialogHelper::~JavaScriptDialogHelper() {
}

void JavaScriptDialogHelper::RunJavaScriptDialog(
    content::WebContents* web_contents,
    const GURL& origin_url,
    content::JavaScriptMessageType javascript_message_type,
    const base::string16& message_text,
    const base::string16& default_prompt_text,
    const DialogClosedCallback& callback,
    bool* did_suppress_message) {
  base::DictionaryValue request_info;
  request_info.Set(
      webview::kDefaultPromptText,
      new base::StringValue(base::UTF16ToUTF8(default_prompt_text)));
  request_info.Set(webview::kMessageText,
                   new base::StringValue(base::UTF16ToUTF8(message_text)));
  request_info.Set(webview::kMessageType,
                   new base::StringValue(
                       JavaScriptMessageTypeToString(javascript_message_type)));
  request_info.Set(guest_view::kUrl, new base::StringValue(origin_url.spec()));
  WebViewPermissionHelper* web_view_permission_helper =
      WebViewPermissionHelper::FromWebContents(web_contents);
  web_view_permission_helper->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_JAVASCRIPT_DIALOG,
      request_info,
      base::Bind(&JavaScriptDialogHelper::OnPermissionResponse,
                 base::Unretained(this),
                 callback),
      false /* allowed_by_default */);
}

void JavaScriptDialogHelper::RunBeforeUnloadDialog(
    content::WebContents* web_contents,
    bool is_reload,
    const DialogClosedCallback& callback) {
  // This is called if the guest has a beforeunload event handler.
  // This callback allows navigation to proceed.
  callback.Run(true, base::string16());
}

bool JavaScriptDialogHelper::HandleJavaScriptDialog(
    content::WebContents* web_contents,
    bool accept,
    const base::string16* prompt_override) {
  return false;
}

void JavaScriptDialogHelper::CancelActiveAndPendingDialogs(
    content::WebContents* web_contents) {
}

void JavaScriptDialogHelper::ResetDialogState(
    content::WebContents* web_contents) {
}

void JavaScriptDialogHelper::OnPermissionResponse(
    const DialogClosedCallback& callback,
    bool allow,
    const std::string& user_input) {
  callback.Run(allow && web_view_guest_->attached(),
               base::UTF8ToUTF16(user_input));
}

}  // namespace extensions
